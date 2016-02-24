//--------------------------------------------------------------------------------------
// Copyright 2013 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//--------------------------------------------------------------------------------------

#include "ChatHeads.h"

#include "CPUTMaterial.h"
#include "CPUTLight.h"
#include "CPUTTextureDX11.h"
#include "CPUTRenderTarget.h"
#ifdef _DEBUG
	#include <DXGIDebug.h>
#endif

#include "VTuneScopedTask.h"
#include "TheoraException.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "SystemMetrics.h"

#include <algorithm>
#include <string>

#define IM_ARRAYSIZE(_ARR)  ((int)(sizeof(_ARR)/sizeof(*_ARR)))

CPUTLight* GetLight(CPUTScene* pScene, LightType type);
CPUTCamera* GetCamera(CPUTScene* pScene);
__itt_domain* g_pDomain = __itt_domain_create(L"Chatheads.Sample");
const UINT SHADOW_WIDTH_HEIGHT = 2048;


void ChatHeads::Create()
{
	CreateBasicCPUTResources();
	InitIMGUI();
	bool bPreInit = mRSMgr.PreInit(); // need to do this to get the list of valid resolutions when using BGS

	// Exit app if a realsense session wasn't started
	if (!bPreInit)
	{
		const std::string rsBuildVersion = std::to_string(PXC_VERSION_MAJOR) + "." + std::to_string(PXC_VERSION_MINOR);
		CPUTOSServices::OpenMessageBox("Error", "RealSense session wasn't created successfully. Either RS Runtime is absent, or there's a runtime mismatch. ChatHeads.exe was built with " + rsBuildVersion);
		mpWindow->Destroy();
		PostQuitMessage(0);
	}
}


void ChatHeads::Shutdown()
{
	Log.Log(LOG_INFO, "Shutting Chat Heads down. Goodbye!");
	
	mNetLayer.Shutdown();

	mRSMgr.Shutdown();
	ReleaseRSResources();

	ReleaseMovieResources();

	mEncoder.Shutdown();
	for (DecodeTransform& dt : mDecoders) dt.Shutdown();

	ImGui_ImplDX11_Shutdown();
}


void ChatHeads::Update(double deltaSeconds)
{
	// Conditonally recreate remote texture/buffer resources if size has changed
	RecreateRemoteResourcesIfNeedBe();

	mNetLayer.WakeNetworkThread();


    if (mpWindow->DoesWindowHaveFocus())
    {
        if(mpCameraController)
			mpCameraController->Update((float)deltaSeconds);		
    }

	if (!mbPlayMovie)
	{
		if (mpScene)
			mpScene->Update((float)deltaSeconds);
	}

	ProcessUI();
}


// Handle keyboard events
//-----------------------------------------------------------------------------
CPUTEventHandledCode ChatHeads::HandleKeyboardEvent(CPUTKey key, CPUTKeyState state)
{
	// Imgui key state is set directly in WndProc in CPUTWindowWin.cpp

    CPUTEventHandledCode    handled = CPUT_EVENT_UNHANDLED;

    switch(key)
    {
    case KEY_F2:
        if (state == CPUT_KEY_UP) {
            mDisplayGUI = !mDisplayGUI;
        }
        handled = CPUT_EVENT_HANDLED;
        break;

    case KEY_ESCAPE:
        handled = CPUT_EVENT_HANDLED;
        mpWindow->Destroy();
        PostQuitMessage(0);
        break;
    default:
        break;
    }
    // pass it to the camera controller
    if(handled == CPUT_EVENT_UNHANDLED)
    {
        if (mpCameraController)
        handled = mpCameraController->HandleKeyboardEvent(key, state);
    }

    return handled;
}


// Handle mouse events
//-----------------------------------------------------------------------------
CPUTEventHandledCode ChatHeads::HandleMouseEvent(int x, int y, int wheel, CPUTMouseState state, CPUTEventID message) 
{
	// Imgui mouse state is set here (instead of WndProc)
	ImGuiIO& io = ImGui::GetIO();
	io.MousePos = ImVec2((float) x, (float) y);
	io.MouseDown[0] = (state == CPUT_MOUSE_LEFT_DOWN); // L
	io.MouseDown[1] = (state == CPUT_MOUSE_RIGHT_DOWN); // R
	io.MouseDown[2] = (state == CPUT_MOUSE_MIDDLE_DOWN); // M
	io.DeltaTime = 1.0f / 60.0f; // --todo-- Fix

    if( mpCameraController )
    {
        return mpCameraController->HandleMouseEvent(x, y, wheel, state, message);
    }
    return CPUT_EVENT_UNHANDLED;
}


void ChatHeads::ResizeWindow(UINT width, UINT height)
{
    // Before we can resize the swap chain, we must release any references to it.
    // We could have a "AssetLibrary::ReleaseSwapChainResources(), or similar.  But,
    // Generic "release all" works, is simpler to implement/maintain, and is not performance critical.

	CPUT_DX11::ResizeWindow( width, height );

	// Resize any application-specific render targets here
    if( mpCamera ) 
        mpCamera->SetAspectRatio(((float)width)/((float)height));

	RecreateChatheadSprites();
}


void ChatHeads::Render(double deltaSeconds)
{
	mFrameRate = (double)(1.0f / deltaSeconds);
    
	CPUTRenderParameters renderParams;
    int windowWidth, windowHeight;
    mpWindow->GetClientDimensions( &windowWidth, &windowHeight);
    renderParams.mWidth = windowWidth;
    renderParams.mHeight = windowHeight;
    renderParams.mRenderOnlyVisibleModels = false;
	renderParams.mpCamera = mpCamera;
	renderParams.mpShadowCamera = mpShadowCamera;
	renderParams.mpPerFrameConstants = mpPerFrameConstantBuffer;
	renderParams.mpPerModelConstants = mpPerModelConstantBuffer;
	renderParams.mpSkinningData = mpSkinningDataConstantBuffer;
  
    // Clear back buffer
    const float clearColor[] = { 0.0993f, 0.0993f, 0.0993f, 1.0f };
    mpContext->ClearRenderTargetView( mpBackBufferRTV,  clearColor );
    mpContext->ClearDepthStencilView( mpDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 0.0f, 0);
 
	if (mbPlayMovie)
	{
		ULONGLONG curTick = GetTickCount64();
		mpMovieMgr->update((curTick - mMovieTickCount) / 1000.0f);
		mMovieTickCount = curTick;

		TheoraVideoFrame *pFrame = mpMovieClip->getNextFrame();

		if (pFrame)
		{
			UpdateMovieTexture(pFrame->getBuffer(), renderParams);
			mpMovieClip->popFrame();
		}

		mpMovieSprite->DrawSprite(renderParams);
	}
	else
	{
		if (mpScene)
		{
			const int DEFAULT_MATERIAL = 0;
			const int SHADOW_MATERIAL = 1;

			//*******************************
			// Draw the shadow scene
			//*******************************
			renderParams.mWidth = SHADOW_WIDTH_HEIGHT;
			renderParams.mHeight = SHADOW_WIDTH_HEIGHT;
			UpdatePerFrameConstantBuffer(renderParams, deltaSeconds);
			mpShadowRenderTarget->SetRenderTarget(renderParams, 0, 0.0f, true);
			mpScene->Render(renderParams, SHADOW_MATERIAL);
			mpShadowRenderTarget->RestoreRenderTarget(renderParams);

			//*******************************
			// Draw the regular scene
			//*******************************
			renderParams.mWidth = windowWidth;
			renderParams.mHeight = windowHeight;
			renderParams.mpCamera = mpCamera;
			renderParams.mpShadowCamera = mpShadowCamera;
			UpdatePerFrameConstantBuffer(renderParams, deltaSeconds);
			mpScene->Render(renderParams, DEFAULT_MATERIAL);
		}
	}

	if (mRSMgr.InitSuccess())
		RenderChatheads(renderParams);	

	if (mDisplayGUI)
		ImGui::Render();
}


/**************************************************************  CPUT Scene specifics ***************************************************************/
CPUTLight* GetLight(CPUTScene* pScene, LightType type)
{
    for (unsigned int i = 0; i < pScene->GetNumAssetSets(); i++)
    {
        CPUTAssetSet* pSet = pScene->GetAssetSet(i);
        CPUTRenderNode* pRoot = pSet->GetRoot();
        CPUTRenderNode* pCurrent = pRoot;
        while (pCurrent)
        {
            if (pCurrent->GetNodeType() == CPUTRenderNode::CPUT_NODE_LIGHT)
            {
                CPUTLight* pLight = (CPUTLight*)pCurrent;
                CPUTLightParams* pParams = pLight->GetLightParameters();
                if (pParams->nLightType == type)
                {
                    pLight->AddRef();
                    SAFE_RELEASE(pRoot);
                    return pLight;
                }
            }
            CPUTRenderNode* pNext = pCurrent->GetChild();
            if (pNext == NULL)
            {
                pNext = pCurrent->GetSibling();

                if (pNext == NULL)
                {
                    pNext = pCurrent->GetParent();
                    if (pNext != NULL)
                    {
                        pNext = pNext->GetSibling();
                    }
                }
            }
            pCurrent = pNext;
        }
        SAFE_RELEASE(pRoot);
    }

    return NULL;
}


CPUTCamera* GetCamera(CPUTScene* pScene)
{
    for (unsigned int i = 0; i < pScene->GetNumAssetSets(); i++)
    {
        CPUTCamera* pCamera = pScene->GetAssetSet(i)->GetFirstCamera();
        if (pCamera)
            return pCamera;
    }
    return NULL;
}


void ChatHeads::LoadScene(std::string sceneFilename)
{
	CPUTAssetLibrary *pAssetLibrary = CPUTAssetLibrary::GetAssetLibrary();

	mpScene = CPUTScene::Create();

	std::string path = "no path";
	std::string filename = "no filename";
	if (sceneFilename.empty())
		return;


	size_t lastSlash = sceneFilename.find_last_of('\\');
	path = sceneFilename.substr(0, lastSlash + 1);
	filename = sceneFilename.substr(lastSlash + 1);
	pAssetLibrary->SetMediaDirectoryName(path);
	mpScene->LoadScene(sceneFilename);
	float3 sceneCenterPoint, halfVector;
	mpScene->GetBoundingBox(&sceneCenterPoint, &halfVector);
	float  length = halfVector.length();

	mpCamera = GetCamera(mpScene);

	if (mpCamera == NULL) {
		mpCamera = CPUTCamera::Create(CPUT_PERSPECTIVE);
		pAssetLibrary->AddCamera("SampleStart Camera", "", "", mpCamera);

		mpShadowCamera->SetPosition(sceneCenterPoint - float3(0.0, 0.0, 1.0) * length);
		mpShadowCamera->LookAt(sceneCenterPoint);

		float farplane = 2.0f * length;
		float nearplane = farplane / 1000.0f;
		mpCamera->SetFarPlaneDistance(farplane);
		mpCamera->SetNearPlaneDistance(nearplane);
	}
	mpCamera->Update();

	CPUTLight* pAmbient = GetLight(mpScene, CPUT_LIGHT_AMBIENT);
	if (pAmbient != NULL)
	{
		CPUTLightParams *pParams = pAmbient->GetLightParameters();
		mAmbientColor = float4(pParams->pColor[0], pParams->pColor[1], pParams->pColor[2], 1.0);
		pAmbient->Release();
	}
	CPUTLight* pDirectional = GetLight(mpScene, CPUT_LIGHT_DIRECTIONAL);
	if (pDirectional != NULL)
	{
		float3 position = pDirectional->GetPositionWS();
		float3 d = position - sceneCenterPoint;
		float l = d.length() + halfVector.length();
		mpShadowCamera->SetPosition(sceneCenterPoint - pDirectional->GetLookWS()*l);
		mpShadowCamera->LookAt(sceneCenterPoint);
		CPUTLightParams *pParams = pDirectional->GetLightParameters();
		mpShadowCamera->SetNearPlaneDistance(.10f);
		mpShadowCamera->SetFarPlaneDistance(2 * l);
		mLightColor = float4(pParams->pColor[0], pParams->pColor[1], pParams->pColor[2], 1.0);
		pDirectional->Release();
	}
	else
	{
		mpShadowCamera->SetPosition(sceneCenterPoint - float3(0.0, -1.0, 1.0) * length);
		mpShadowCamera->LookAt(sceneCenterPoint);
		mpShadowCamera->SetNearPlaneDistance(1.0f);
		mpShadowCamera->SetFarPlaneDistance(2.0f*length + 1.0f);
	}

	mpShadowCamera->SetAspectRatio(1.0f);
	mpShadowCamera->SetWidth(length * 2);
	mpShadowCamera->SetHeight(length * 2);
	mpShadowCamera->Update();

	pAssetLibrary->AddCamera("ShadowCamera", "", "", mpShadowCamera);

	mpCameraController->SetCamera(mpCamera);
	mpCameraController->SetLookSpeed(0.004f);
	mpCameraController->SetMoveSpeed(50.0f);
}


void ChatHeads::CreateBasicCPUTResources()
{
    CPUT_DX11::CreateResources();

    int width, height;
    mpWindow->GetClientDimensions(&width, &height);
		
    CPUTAssetLibrary *pAssetLibrary = CPUTAssetLibrary::GetAssetLibrary();

    std::string executableDirectory;
    std::string mediaDirectory;

    CPUTFileSystem::GetExecutableDirectory(&executableDirectory);
    CPUTFileSystem::ResolveAbsolutePathAndFilename(executableDirectory + "../../../../../Media/", &mediaDirectory);

    pAssetLibrary->SetMediaDirectoryName(mediaDirectory + "gui_assets/");
    pAssetLibrary->SetSystemDirectoryName(mediaDirectory + "System/");
    pAssetLibrary->SetFontDirectoryName(mediaDirectory + "gui_assets/Font/");
    pAssetLibrary->SetMediaDirectoryName(mediaDirectory);

    mpShadowRenderTarget = CPUTRenderTargetDepth::Create();
    mpShadowRenderTarget->CreateRenderTarget(std::string("$shadow_depth"), SHADOW_WIDTH_HEIGHT, SHADOW_WIDTH_HEIGHT, DXGI_FORMAT_D32_FLOAT);
    
    mpCameraController = CPUTCameraControllerFPS::Create();
    mpShadowCamera = CPUTCamera::Create(CPUT_ORTHOGRAPHIC);     
}


void ChatHeads::ReleaseBasicCPUTResources()
{
	// Note: these two are defined in the base.  We release them because we addref them.
	SAFE_RELEASE(mpCamera);
	SAFE_RELEASE(mpShadowCamera);

	SAFE_DELETE(mpCameraController);
	SAFE_DELETE(mpShadowRenderTarget);
	SAFE_DELETE(mpScene);
	CPUTAssetLibrary::GetAssetLibrary()->ReleaseAllLibraryLists();
	CPUT_DX11::ReleaseResources();
}


/**************************************************************  Realsense stuff  ***************************************************************/
void ChatHeads::InitRealsenseAndEncoder()
{
	if (mRSMgr.InitSuccess())
		return;

	mRSMgr.SetResolution(mOptions.curResListIndex); // call before Init

	if (mRSMgr.Init())	
		mEncoder.Init(mRSMgr.VideoWidth(), mRSMgr.VideoHeight());
	else
		CPUTOSServices::OpenMessageBox("Error", "Realsense initialization failed. Is your camera plugged in? If so, try another resolution. If that doesn't work, restart the RealsenseDCMF250 service in Task Manager.");

	// textures have to be created before the sprites, so the material can bind them as SRV
	if (!mbChatheadResourcesCreated)
	{
		CreateDefaultChatheadResources();
		CreateChatheadSprites();

		mbChatheadResourcesCreated = true;
	}
}


void ChatHeads::ReleaseRSResources()
{
	for (auto& sprite : mChatheadSprites)
		SAFE_DELETE(sprite);

	for (auto& rt : mChatheadTextures)
		SAFE_DELETE(rt);

	for (auto& rd : mRemoteChatheads)
		SAFE_DELETE_ARRAY(rd.imgBuffer.pBuffer);

	mChatheadSprites.clear();
	mChatheadTextures.clear();
	mRemoteChatheads.clear();

	mbChatheadResourcesCreated = false;
}


// Create render target textures for the maximum # of players.
// TODO: If the stream size changes, these might have to be recreated
void ChatHeads::CreateDefaultChatheadResources()
{
	using namespace std;

	int width = 320, height = 240;

	if (mRSMgr.InitSuccess())
	{
		width = mRSMgr.VideoWidth();
		height = mRSMgr.VideoHeight();
	}

	std::string textureName("$chathead_texture");

	// Create DX textures for ALL players, even if the size is incorrect.
	for (int ii = 0; ii < mNetLayer.cMaxPlayers; ii++)
	{
		// Create a texture to hold the video frame data (can't be bound as a render target even though the function name suggests so)
		CPUTRenderTargetColor* pChatheadTexture = CPUTRenderTargetColor::Create();
		pChatheadTexture->CreateRenderTarget(
			textureName + std::to_string(ii),
			width,
			height,
			//DXGI_FORMAT_YUY2, // segmented image comes as RGBA. yuy2 doesn't encode alpha
			DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, // LSB [B][G][R][A] MSB is the same as PXCImage::PixelFormat::PIXEL_FORMAT_RGB32, which is stored as
										// BGRA layout on a little-endian machine
			1,
			false,
			false,
			D3D11_USAGE_DYNAMIC);		// using dynamic here prevents the staging texture creation & extra gpu copy during Map/Unmap

		mChatheadTextures.push_back(pChatheadTexture);
	}

	// create buffers to hold remote player chathead data (before it's written to the DX texture)
	const int maxRemotePlayers = mNetLayer.cMaxPlayers - 1;
	for (int ii = 0; ii < maxRemotePlayers; ii++) {
		ImageBuffer buf;
		buf.width = width;
		buf.height = height;
		// TODO: This wont work for more than 2 players (1 being server-and-player) 
		// If it's the server, clients haven't even connected. 
		// If client, server needs to tell it about other players.
		// So creation needs to be triggered by n/w events/messages
		buf.pBuffer = new byte[width * height * RealsenseMgr::cBytesPerPixel];

		RemoteChathead rd;
		rd.imgBuffer = buf;
		rd.bWasUpdated = false;
		rd.bSizeChanged = false;
		rd.userLock = IL_UNLOCK;
				
		mRemoteChatheads.push_back(rd);
	}
}


// Check if remote data buffer(s) and texture resource(s) need to be resized
// Note: This HAS to execute on the main thread. Thank you DX11.
void ChatHeads::RecreateRemoteResourcesIfNeedBe()
{
	int rcIndex = 0;

	for (RemoteChathead& rc : mRemoteChatheads)
	{
		if (rc.bSizeChanged)
		{
			if (InterlockedExchange(&rc.userLock, IL_LOCK) == IL_UNLOCK)
			{
				SAFE_DELETE_ARRAY(rc.imgBuffer.pBuffer);

				rc.imgBuffer.pBuffer = new byte[rc.newWidth * rc.newHeight * RealsenseMgr::cBytesPerPixel];
				rc.imgBuffer.width = rc.newWidth;
				rc.imgBuffer.height = rc.newHeight;

				std::string textureName("$chathead_texture");
				
				const int textureIndex = rcIndex + 1; // local player texture is at index 0

				CPUTRenderTargetColor* pChatheadTexture = mChatheadTextures[textureIndex]; 

				pChatheadTexture->CreateRenderTarget(
					textureName + std::to_string(textureIndex),
					rc.newWidth,
					rc.newHeight,
					DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
					1,
					false,
					true, // recreate
					D3D11_USAGE_DYNAMIC);

				rc.bSizeChanged = false;

				if (mDecoders[rcIndex].mbInitSuccess)
					mDecoders[rcIndex].Shutdown();

				mDecoders[rcIndex].Init(rc.newWidth, rc.newHeight);

				InterlockedExchange(&rc.userLock, IL_UNLOCK);
			}
		}

		rcIndex++;
	}
}


// Create sprites for the maximum number of players
void ChatHeads::CreateChatheadSprites()
{
	using namespace std;

	CPUTAssetLibrary *pAssetLibrary = CPUTAssetLibrary::GetAssetLibrary();
	float spriteWidth = 0.6f, spriteHeight = 0.6f, 
		  spriteGapX = 0.2f, spriteGapY = 0.0f, 
		  topLeftViewportX = -1.0f, topLeftViewportY = -1.0f;
	string matName("%chathead");

	for (int ii = 0; ii < mNetLayer.cMaxPlayers; ii++) 
	{
		// CPUT material files hardcode the texture string that needs to be bound, so we use multiple material files as the easiest workaround
		CPUTMaterial* pChatheadMaterial = pAssetLibrary->GetMaterial(matName + to_string(ii));

		// Change sprite size and position depending on scene shown
		switch (mOptions.eScene)
		{
		case ChatHeadsOptions::LeagueOfLegends:
			spriteWidth = spriteHeight = 0.18f; // make size similar to hero UI
			topLeftViewportX = -0.92f;			
			topLeftViewportY = -0.84f + ii * (spriteHeight + spriteGapY);	
			break;

		case ChatHeadsOptions::Hearthstone:
			spriteWidth = spriteHeight = 0.36f;
			topLeftViewportX = -1.0f;
			if (ii == 0) // localplayer -- place at the bottom
				topLeftViewportY = 0.3f;
			else
				topLeftViewportY = -0.9f;
			break;

		case ChatHeadsOptions::CPUT3DScene:
			topLeftViewportX = -1.0f + ii * (spriteWidth + spriteGapX);
			topLeftViewportY = -1.0f;			
			break;
		}

		mChatheadSprites.push_back(CPUTSprite::Create(topLeftViewportX, topLeftViewportY, spriteWidth, spriteHeight, pChatheadMaterial));

		if (ii == 0)
		{
			// update local player's pos/size since it's exposed in the UI
			mOptions.chatHeadPos[0] = (topLeftViewportX + 1) * 50; // (-1,1) --> (0,100)
			mOptions.chatHeadPos[1] = (topLeftViewportY + 1) * 50; // (-1,1) --> (0,100)
			mOptions.chatHeadSize[0] = spriteWidth * 50; // (0, 2) --> (0,100)
			mOptions.chatHeadSize[1] = spriteHeight * 50; // (0, 2) --> (0,100)
		}

		SAFE_RELEASE(pChatheadMaterial);
	}
}


void ChatHeads::RecreateChatheadSprites()
{
	// Instead of adding CPUT code to change sprite positions, just delete and recreate them. Lazy coder me.
	if (mbChatheadResourcesCreated)
	{
		for (auto& sprite : mChatheadSprites)
			SAFE_DELETE(sprite);

		mChatheadSprites.clear();

		CreateChatheadSprites();
	}
}


void ChatHeads::RenderChatheads(CPUTRenderParameters& renderParams)
{
	VTUNE_TASK(g_pDomain, "RenderChatheads");

	// --- update local players chathead & send over n/w ---- 
	{		
		VTUNE_TASK(g_pDomain, "LocalPlayer");
		
		if (mRSMgr.mSharedData.bImageUpdated)
		{
			if (InterlockedExchange(&mRSMgr.mSharedData.hLock, IL_LOCK) == IL_UNLOCK)
			{
				// lock resource to update on the CPU
				CPUTRenderTargetColor *mpLocalPlayerChatheadRT = mChatheadTextures[0];
				D3D11_MAPPED_SUBRESOURCE mappedResource = mpLocalPlayerChatheadRT->MapRenderTarget(renderParams, CPUT_MAP_WRITE_DISCARD, true);

				RSAppSharedData& data = mRSMgr.mSharedData;

				// copy the shared data to the mapped resource
				byte *pDst = (byte*)mappedResource.pData;
				byte *pSrc = data.segImage.pBuffer;

				const int height = data.segImage.height;
				const int width = data.segImage.width;
				const size_t numBytes = width * height * RealsenseMgr::cBytesPerPixel;
				const size_t dstRowPitch = mappedResource.RowPitch;
				const size_t srcRowPitch = data.segImage.width * RealsenseMgr::cBytesPerPixel;

				// src pitch can be different from the dst row pitch (latter can have padding)
				// if so, copy the image row by row
				if (srcRowPitch != dstRowPitch) {
					for (int j = 0; j < height; j++)
					{
						memcpy(pDst, pSrc, srcRowPitch);
						pDst += dstRowPitch;
						pSrc += srcRowPitch;
					}
				}
				else
					memcpy(pDst, pSrc, numBytes);

				mpLocalPlayerChatheadRT->UnmapRenderTarget(renderParams);


				{					
					// Encode only when you can send data out on the network
					if (mNetLayer.InitComplete() && mNetLayer.CanSendData() && mNetLayer.IsConnected())
					{
						EncoderOutput etn = mEncoder.EncodeData(reinterpret_cast<char*>(pSrc), numBytes);
						
						// Encoder returns false when more data is needed, in which case nothing is sent over the n/w
						if (etn.returnCode == S_OK)
						{
							const bool bBroadcast = mNetLayer.IsServer();
							NetMsgVideoUpdate msg;
							msg.header.playerId = mNetLayer.PlayerID();
							msg.header.width = mRSMgr.VideoWidth();
							msg.header.height = mRSMgr.VideoHeight();
							msg.header.timestamp = etn.timestamp;
							msg.header.duration = etn.duration;
							msg.pEncodedData = etn.pEncodedData;
							msg.sizeBytes = etn.numBytes;

							mNetLayer.SendVideoData(msg, bBroadcast);

							mEncoder.Unlock();
						}
					}
				}

				mRSMgr.mSharedData.bImageUpdated = false;
				InterlockedExchange(&mRSMgr.mSharedData.hLock, IL_UNLOCK);
			} // got lock

		} // image was updated
	}


	// --- update remote player chathead textures if the network callback isn't writing to the buffers ---
	// (if the network is updating the buffers, then, we simply show the last texture contents)
	if (mNetLayer.IsConnected())
	{	
		VTUNE_TASK(g_pDomain, "RemotePlayers");

		// TODO: this should be over # active players, not max
		const int maxRemotePlayers = mNetLayer.cMaxPlayers - 1;

		for (int ii = 0; ii < maxRemotePlayers; ii++)
		{
			RemoteChathead& rd = mRemoteChatheads[ii];

			if (rd.bWasUpdated)
			{
				volatile LONG& userLock = rd.userLock;

				// If the network thread isn't writing to the buffer and there was an update, update the DX textures
				if (InterlockedExchange(&userLock, IL_LOCK) == IL_UNLOCK)
				{
					ASSERT(ii < mNetLayer.cMaxPlayers, "remote player texture out of range");
					CPUTRenderTargetColor *mpRemotePlayerChatheadRT = mChatheadTextures[ii + 1]; // ii = 0 represents the local player's video texture
					// lock
					D3D11_MAPPED_SUBRESOURCE mappedResource = mpRemotePlayerChatheadRT->MapRenderTarget(renderParams, CPUT_MAP_WRITE_DISCARD, true);

					// copy the shared data to the mapped resource
					byte *pDst = (byte*)mappedResource.pData;
					byte *pSrc = rd.imgBuffer.pBuffer;

					const int height = rd.imgBuffer.height;
					const int width = rd.imgBuffer.width;
					const size_t numBytes = width * height * RealsenseMgr::cBytesPerPixel;
					const size_t dstRowPitch = mappedResource.RowPitch;
					const size_t srcRowPitch = width * RealsenseMgr::cBytesPerPixel;

					// src pitch can be different from the dst row pitch (latter can have padding)
					// if so, copy the image row by row
					if (srcRowPitch != dstRowPitch) {
						for (int j = 0; j < height; j++)
						{
							memcpy(pDst, pSrc, srcRowPitch);
							pDst += dstRowPitch;
							pSrc += srcRowPitch;
						}
					}
					else
						memcpy(pDst, pSrc, numBytes);

					// unlock
					mpRemotePlayerChatheadRT->UnmapRenderTarget(renderParams);
					rd.bWasUpdated = false;
					InterlockedExchange(&userLock, IL_UNLOCK);
				}
			}
		}
	}


	// draw all the player chatheads
	DrawChatheadSprites(renderParams);	
}


void ChatHeads::DrawChatheadSprites(CPUTRenderParameters& rp)
{
	for (auto& sprite : mChatheadSprites)
		sprite->DrawSprite(rp);
}


/**************************************************************  Network stuff  ****************************************************************/
// Update remote player's texture buffer after decoding the video data
// Note: This executes on the networking thread (callback during message processing)
void ChatHeads::UpdateRemoteChatheadBuffer(NetMsgVideoUpdate *pMsg)
{
	std::string taskname("ReceiveVideoData : " + std::to_string(pMsg->header.playerId));
	VTUNE_TASK(g_pDomain, taskname.c_str());

	// Since this function is a callback from the network thread, it is possible for it to execute before realsense resource creation
	if (!mbChatheadResourcesCreated || (!mNetLayer.IsClientConnectedToServer() && !mNetLayer.IsServer()))
		return;

	const int playerId = pMsg->header.playerId;
	// Each of the remote players (including server if we're not the server) has its own texture buffer area
	// The index starts at zero (hence the minus 1), with server swapped with our network playerId
	int rpIndex = (playerId == 0) ? mNetLayer.PlayerID() - 1 : playerId - 1;
	
	ImageBuffer& ib = mRemoteChatheads[rpIndex].imgBuffer;
	if (ib.width != pMsg->header.width || ib.height != pMsg->header.height)
	{
		// If size is different, set a bool so that the main thread can recreate the buffers. Thanks DX11.
		mRemoteChatheads[rpIndex].bSizeChanged = true;
		mRemoteChatheads[rpIndex].newWidth = pMsg->header.width;
		mRemoteChatheads[rpIndex].newHeight = pMsg->header.height;

		// Don't update the buffer. Just skip the frame.
		return;
	}

	// If decoder wasn't initialized, do it now
	if (!mDecoders[rpIndex].mbInitSuccess)
		mDecoders[rpIndex].Init(pMsg->header.width, pMsg->header.height);

	
	DecoderOutput dtn = mDecoders[rpIndex].DecodeData(reinterpret_cast<byte*>(pMsg->pEncodedData),
															(DWORD)pMsg->sizeBytes,
															pMsg->header.timestamp,
															pMsg->header.duration);

	//Log.Log(LOG_INFO, "RCV: Message tiemstamp %lld, duration %lld, size %lu \n", pMsg->header.timestamp, pMsg->header.duration, pMsg->sizeBytes);

	if (dtn.returnCode == S_OK)
	{
		// User lock to ensure thread safety (we don't want to use a mutex here)
		volatile LONG& userLock = mRemoteChatheads[rpIndex].userLock;

		if (InterlockedExchange(&userLock, IL_LOCK) == IL_UNLOCK)
		{
			VTUNE_TASK(g_pDomain, "UpdateRemoteChatheadTexture");

			// TODO: need a map between client id and remote buffer index
			ASSERT(rpIndex >= 0, "Bad client ID received");

			ImageBuffer& buf = mRemoteChatheads[rpIndex].imgBuffer;
			memcpy(buf.pBuffer, dtn.pDecodedData, dtn.numBytes);
			mRemoteChatheads[rpIndex].bWasUpdated = true;

			InterlockedExchange(&userLock, IL_UNLOCK);
		}
		else
		{
			Log.Log(LOG_INFO, "Failed remote buffer update at %ld", GetTickCount64());
		}
	}
}


void ChatHeads::NetMsgCallback(NetworkMsg eMsg, void *pThis, void *pMsg)
{
	switch (eMsg)
	{
	case ID_GAME_MESSAGE_VIDEO_UPDATE:
		NetMsgVideoUpdate *pVUMsg = reinterpret_cast<NetMsgVideoUpdate*>(pMsg);
		static_cast<ChatHeads*>(pThis)->UpdateRemoteChatheadBuffer(pVUMsg);
		break;
	}
}


/*********************************  Movie texure playback stuff ***********************/
void ChatHeads::InitMovieTexturePlayback(std::string moviePath)
{
	if (!mpMovieMgr)
	{
		mpMovieMgr = new TheoraVideoManager();
		mpMovieMgr->setAudioInterfaceFactory(NULL);
	}

	ReleaseMovieClip(); // if selecting movies back to back, we want to release the last clip.

	try
	{
		mpMovieClip = mpMovieMgr->createVideoClip(moviePath, TH_RGBX);
	}
	catch (_TheoraGenericException* e)
	{
		Log.Log(LOG_ERROR, e->getErrorText().c_str());
		CPUTOSServices::OpenMessageBox("Error", e->getErrorText().c_str());
	}

	mMovieTickCount = GetTickCount64();

	if (mpMovieClip)
	{
		mpMovieClip->setAutoRestart(1);

		CreateMovieResources();
	}

	mbPlayMovie = true;
}


void ChatHeads::CreateMovieResources()
{
	CPUTAssetLibrary *pAssetLibrary = CPUTAssetLibrary::GetAssetLibrary();

	if (!mpMovieRenderTarget)
	{
		mpMovieRenderTarget = CPUTRenderTargetColor::Create();
		mpMovieRenderTarget->CreateRenderTarget(
			"$movie_texture",
			mpMovieClip->getWidth(),
			mpMovieClip->getHeight(),
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			1,
			false,
			false,
			D3D11_USAGE_DYNAMIC);		// using dynamic here prevents the staging texture creation & extra gpu copy during Map/Unmap

		mpMovieTexMaterial = pAssetLibrary->GetMaterial("%movietexture");

		mpMovieSprite = CPUTSprite::Create(-1.0f, -1.0f, 2.0f, 2.0f, mpMovieTexMaterial);

	}
	else
	{
		// recreate the render target alone (sprite/material bindings can stay as-is; doesn't need to be recreated since same rt name is used)
		mpMovieRenderTarget->CreateRenderTarget(
			"$movie_texture",
			mpMovieClip->getWidth(),
			mpMovieClip->getHeight(),
			DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			1,
			false,
			true, // recreate
			D3D11_USAGE_DYNAMIC);		// using dynamic here prevents the staging texture creation & extra gpu copy during Map/Unmap

	}
}


void ChatHeads::ReleaseMovieResources()
{
	if (mpMovieMgr && mpMovieClip)
	{
		mpMovieMgr->destroyVideoClip(mpMovieClip); // dll owns the clip	
		mpMovieClip = nullptr;
	}

	SAFE_DELETE(mpMovieMgr);
	SAFE_DELETE(mpMovieSprite);
	SAFE_DELETE(mpMovieRenderTarget);
	SAFE_RELEASE(mpMovieTexMaterial);
}


void ChatHeads::ReleaseMovieClip()
{
	if (mpMovieMgr && mpMovieClip)
	{
		mpMovieMgr->destroyVideoClip(mpMovieClip); // dll owns the clip	
		mpMovieClip = nullptr;
	}
}


void ChatHeads::UpdateMovieTexture(unsigned char* pframe, CPUTRenderParameters& renderParams)
{
	HRESULT hr = S_OK;

	D3D11_MAPPED_SUBRESOURCE mappedResource = mpMovieRenderTarget->MapRenderTarget(renderParams, CPUT_MAP_WRITE_DISCARD);

	unsigned char* pvData = (unsigned char*) mappedResource.pData;
	const int iSize = mpMovieClip->getWidth() * 4; // 4 bytes per pixel

	for (int i = 0; i < mpMovieClip->getHeight();)
	{
		memcpy(pvData, pframe, iSize);

		++i;
		pvData += mappedResource.RowPitch;
		pframe += iSize;
	}

	mpMovieRenderTarget->UnmapRenderTarget(renderParams);
}


/*********************************  IMGUI stuff ***********************/
void ChatHeads::InitIMGUI()
{
	ImGui_ImplDX11_Init(mpWindow->GetHWnd(), mpD3dDevice, mpContext);
	Log.Log(LOG_INFO, "Initializing Imgui");

	// System metrics init
	InitCPUQuery();
	InitCPUInfo();
}


void ChatHeads::ProcessUI()
{
	if (!mDisplayGUI)
		return;

	ImGui_ImplDX11_NewFrame();

	
	ImGui::Begin("Chat Heads Option Panel", false, ImGuiWindowFlags_AlwaysAutoResize);

	PersistentUI();

	// Options related to starting/connecting to a server
	if (!mbInitNetwork)
	{
		NetworkUI();
	}
	else
	{
		PostStartUI();
	}

	ImGui::End();
}


// Persistent options (always displayed)
void ChatHeads::PersistentUI()
{
	const char* listbox_items[] = { "League of Legends", "Hearthstone", "CPUT Scene" };
	ImGui::ListBox("Scenes", reinterpret_cast<int*>(&(mOptions.eScene)), listbox_items, IM_ARRAYSIZE(listbox_items), 3);

	if (ImGui::Button("Load Scene"))
		SelectScene();

	std::vector<VideoResolution>& resolutions = mRSMgr.GetResolutions();
	ImGui::ListBox("Resolutions", &(mOptions.curResListIndex), GetUIListItem, reinterpret_cast<void*> (&resolutions), (int)resolutions.size(), (int)resolutions.size());
}


// Options pertaining to connecting to a server/starting the server
void ChatHeads::NetworkUI()
{
	ImGui::Checkbox("Is Server", &mOptions.bIsServer);

	if (!mOptions.bIsServer)
		ImGui::InputText("IP Address", mOptions.IPAddr, IM_ARRAYSIZE(mOptions.IPAddr), ImGuiInputTextFlags_CharsDecimal);

	if (ImGui::Button("Start"))
	{
		// set window title (add server/client to string)
		{
			std::wstring title(L"Chat Heads -- ");
			if (mOptions.bIsServer)
				title.append(L"Server");
			else
				title.append(L"Client");

			mpWindow->SetWindowTitle(title);
		}

		// Initialize RS and media pipe
		InitRealsenseAndEncoder();

		mNetLayer.RegisterCallback(this, &ChatHeads::NetMsgCallback);
		mNetLayer.Setup(mOptions.bIsServer, mOptions.IPAddr);

		SelectScene();

		mbInitNetwork = true;
	}
}


// Once server/client is up all systems are initialized, display these options
void ChatHeads::PostStartUI()
{
	// Change local chathead resolution
	if (ImGui::Button("Set Resolution"))
	{
		// Each time a new profile (resolution) is chosen, unfortunately have to restart the RS camera.			
		mRSMgr.Shutdown();
		mRSMgr.PreInit();
		InitRealsenseAndEncoder();

		// recreate DX texture for local chathead
		if (mbChatheadResourcesCreated && mRSMgr.InitSuccess())
		{
			mChatheadTextures[0]->CreateRenderTarget(
				"$chathead_texture0",
				mRSMgr.VideoWidth(),
				mRSMgr.VideoHeight(),
				DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, // LSB [B][G][R][A] MSB is the same as PXCImage::PixelFormat::PIXEL_FORMAT_RGB32, which is stored as
												 // BGRA layout on a little-endian machine
				1,
				false,
				true,						// recreate flag
				D3D11_USAGE_DYNAMIC);		// using dynamic here prevents the staging texture creation & extra gpu copy during Map/Unmap
		}
	}

	if (ImGui::CollapsingHeader("BGS/Media controls", 0, true, true))
	{
		if (mRSMgr.InitSuccess())
		{
			// these options make sense only if local bgs image exists
			ImGui::Checkbox("Show BGS Image", &mOptions.bEnableBGS);
			ImGui::SameLine(); ShowHelpMarker("Show background segmentated image (disabling this doesn't stop the BGS logic from running; it just shows the color stream instead. To compare perf w/ and w/o BGS running, use Pause BGS");
			mRSMgr.DoSegmentation(mOptions.bEnableBGS);
			mEncoder.mbEncodeBackgroundPixels = mOptions.bEnableBGS;
			for (DecodeTransform& d : mDecoders) { d.mbEncodeBackgroundPixels = mOptions.bEnableBGS; }

			ImGui::Checkbox("Pause BGS", &mOptions.bPauseBGS);
			ImGui::SameLine(); ShowHelpMarker("Pause background segmentation. This uses the RSSDK API to stop all algorithmic work for BGS. See the CPU utilization change as a result.");
			mRSMgr.PauseBGS(mOptions.bPauseBGS);

#if PXC_VERSION_MAJOR >= RSSDK_BGS_FREQ_MAJ_VERSION
			const char* bgsFreqOptions[] = { "Run every frame", "Run every alternate frame", "Run once in three frames", "Run once in four frames" };
			ImGui::Combo("BGS frequency", &mOptions.frameSkipInterval, bgsFreqOptions, IM_ARRAYSIZE(bgsFreqOptions));
			mRSMgr.SetBGSFrameSkipInterval(mOptions.frameSkipInterval);
			ImGui::SameLine(); ShowHelpMarker("Coarse control over the frequency at which BGS algorithm runs ");
#endif

			ImGui::SliderInt("Encoding Threshold", &mOptions.encodingThreshold, 0, 255);
			ImGui::SameLine(); ShowHelpMarker("Pre-encoding, RGBA pixels with alpha channel lesser than this represent the background (fully transparent). YUYV is set to 0 for background pixels. (RGBA->YUYV->Encode)");
			mEncoder.SetEncodingThreshold(mOptions.encodingThreshold);
		}

		// you can still be connected to other players w/o RS initialized..
		if (mNetLayer.IsConnected())
		{
			ImGui::SliderInt("Decoding Threshold", &mOptions.decodingThreshold, 0, 255);
			ImGui::SameLine(); ShowHelpMarker("Post-decoding, Y/U/Y/V channel values lesser than this represent alpha = 0, i.e. a background pixel. (Decode->YUYV->RGBA)");
			for (DecodeTransform& dt : mDecoders) dt.SetThreshold(mOptions.decodingThreshold);
		}
	}

	if (mRSMgr.InitSuccess() && ImGui::CollapsingHeader("Size/Pos controls", 0, true, true))
	{
		bool bSizeChanged = ImGui::DragFloat2("size", (float*)&mOptions.chatHeadSize, 1.0f, 0.1f, 100.0f, "%.0f");
		ImGui::SameLine(); ShowHelpMarker("Click+Drag to resize local chathead");
		bool bPosChanged = ImGui::DragFloat2("pos", (float*)&mOptions.chatHeadPos, 1.0f, 0.0f, 100.0f, "%.0f");
		ImGui::SameLine(); ShowHelpMarker("Click+Drag to move local chathead");

		if (bSizeChanged || bPosChanged)
		{
			if (mbChatheadResourcesCreated)
			{
				// Lazy way out, just recreating sprite since CPUT doesn't support vertex bufer modifications
				SAFE_DELETE(mChatheadSprites[0]);
				float vpX = mOptions.chatHeadPos[0] / 100.0f * 2 - 1; // (0,100) to (-1,1)
				float vpY = mOptions.chatHeadPos[1] / 100.0f * 2 - 1;
				float vpWidth = mOptions.chatHeadSize[0] / 100.0f * 2; // (0,100) to (0,2)
				float vpHeight = mOptions.chatHeadSize[1] / 100.0f * 2;
				CPUTMaterial* pChatheadMaterial = CPUTAssetLibrary::GetAssetLibrary()->GetMaterial("%chathead0");
				mChatheadSprites[0] = CPUTSprite::Create(vpX, vpY, vpWidth, vpHeight, pChatheadMaterial);
				SAFE_RELEASE(pChatheadMaterial);
			}
		}
	}

	if (mNetLayer.IsConnected() && ImGui::CollapsingHeader("Network control/info", 0, true, true))
	{
		ImGui::SliderInt("Network send interval (ms)", &mOptions.nwSendInterval, 1, 100);
		ImGui::SameLine(); ShowHelpMarker("Alpha channel values of the segmented image lesser than this represent the background (fully transparent)");
		mNetLayer.SendInterval(mOptions.nwSendInterval);

		{
			static ImVector<float> bytesSent; if (bytesSent.empty()) { bytesSent.resize(90); memset(bytesSent.Data, 0, bytesSent.Size*sizeof(float)); }
			static float totalBytesSent = 0.0f;
			static int sendIndex = 0;

			bytesSent[sendIndex] = static_cast<float> (mNetLayer.GetBytesSentInLastSecond());
			totalBytesSent += bytesSent[sendIndex];
			sendIndex = (sendIndex + 1) % bytesSent.Size;

			float avgBytesSent = 0;
			if (sendIndex == 0)
			{
				totalBytesSent = bytesSent[bytesSent.Size - 1]; // reset
				avgBytesSent = totalBytesSent;
			}
			else
				avgBytesSent = totalBytesSent / (float)sendIndex;

			std::string avgSentText = std::to_string(int(avgBytesSent / 1000)).append(" KB/s");

			ImGui::PlotHistogram("Sent: KB/s", bytesSent.Data, bytesSent.Size, sendIndex, avgSentText.c_str(), 0.0f, 1024 * 300 /*300 KB*/, ImVec2(0, 80));
		}

		{
			static ImVector<float> bytesRcvd; if (bytesRcvd.empty()) { bytesRcvd.resize(90); memset(bytesRcvd.Data, 0, bytesRcvd.Size*sizeof(float)); }
			static float totalBytesRcvd = 0.0f;
			static int rcvdIndex = 0;

			bytesRcvd[rcvdIndex] = static_cast<float> (mNetLayer.GetBytesRcvdInLastSecond());
			totalBytesRcvd += bytesRcvd[rcvdIndex];
			rcvdIndex = (rcvdIndex + 1) % bytesRcvd.Size;

			float avgBytesRcvd = 0;
			if (rcvdIndex == 0)
			{
				totalBytesRcvd = bytesRcvd[bytesRcvd.Size - 1];
				avgBytesRcvd = totalBytesRcvd;
			}
			else
				avgBytesRcvd = totalBytesRcvd / (float)rcvdIndex;
						
			std::string avgRcvdText = std::to_string(int(avgBytesRcvd / 1000)).append(" KB/s");
			ImGui::PlotHistogram("Rcvd: KB/s", bytesRcvd.Data, bytesRcvd.Size, rcvdIndex, avgRcvdText.c_str(), 0.0f, 1024 * 300 /*300 KB*/, ImVec2(0, 80));
		}
	}

	SystemMetricsUI();

	if (ImGui::CollapsingHeader("Misc/Info", 0, true, true))
	{
		if (ImGui::Checkbox("VSync", &mOptions.bVsync))
		{
			if (mOptions.bVsync)
				SetSyncInterval(1);
			else
				SetSyncInterval(0);
		}
		

		ImGui::Text("Frame rate: %f ", ImGui::GetIO().Framerate);		
		ImGui::Text("Chat Head resolution: %dx%d ", mRSMgr.VideoWidth(), mRSMgr.VideoHeight());
		ImGui::Text("RSSDK version %d.%d ", mRSMgr.SDKVersionMajor(), mRSMgr.SDKVersionMinor());
		ImGui::Text("DCM version %d.%d ", mRSMgr.DCMVersionMajor(), mRSMgr.DCMVersionMinor());
		ImGui::Text("IP Address: %s", mNetLayer.GetIPAddress());
		ImGui::Text("");
		ImGui::Text("Toggle F2 to show/hide this panel");
	}
}


void ChatHeads::SystemMetricsUI()
{
	ImGui::Begin("Metrics");

	if (ImGui::CollapsingHeader("System Metrics", 0, true, false))
	{
		ImGui::Text("Num Logical Cores: %d", NumProcessors());
		ImGui::Text("Physical memory total : %ld MB", TotalPhysicalMemory() >> 20);
		ImGui::Text("Physical memory used  : %ld MB", PhysicalMemoryUsed() >> 20);
		ImGui::Text("Virtual memory total  : %ld MB", TotalVirtualMemory() >> 20);
		ImGui::Text("Virtual memory used   : %ld MB", VirtualMemoryUsed() >> 20);

		static ImVector<float> cpuUsed; if (cpuUsed.empty()) { cpuUsed.resize(90); memset(cpuUsed.Data, 0, cpuUsed.Size*sizeof(float)); }
		static int cuId = 0;
		static float totalCPUPercUsage = 0;
		float avgCPUPercUsage = 0;

		cpuUsed[cuId] = (float)CPUUsed();
		cuId = (cuId + 1) % cpuUsed.Size;
		totalCPUPercUsage += cpuUsed[cuId];		

		if (cuId == 0)
		{
			totalCPUPercUsage = cpuUsed[cpuUsed.Size - 1];
			avgCPUPercUsage = totalCPUPercUsage;
		}
		else
			avgCPUPercUsage = totalCPUPercUsage / (float)cuId;

		ImGui::PlotHistogram("CPU Used", cpuUsed.Data, cpuUsed.Size, cuId, std::to_string(int(avgCPUPercUsage)).append(" %").c_str(), 0.0f, 100.0f, ImVec2(0, 80));
	}


	if (ImGui::CollapsingHeader("Process Metrics", 0, true, true))
	{

		ImGui::Text("Physical memory used  : %lu MB", PhysicalMemoryUsedByProcess() >> 20);
		ImGui::Text("Virtual memory total  : %lu MB ", VirtualMemoryUsedByProcess() >> 20);

		static ImVector<float> cpuUsedProc; if (cpuUsedProc.empty()) { cpuUsedProc.resize(90); memset(cpuUsedProc.Data, 0, cpuUsedProc.Size*sizeof(float)); }
		static int cuId = 0;
		static float totalCPUPercUsage = 0;
		float avgCPUPercUsage = 0;
		
		cpuUsedProc[cuId] = (float)CPUUsedByProcess();
		totalCPUPercUsage += cpuUsedProc[cuId];
		cuId = (cuId + 1) % cpuUsedProc.Size;

		if (cuId == 0)
		{
			totalCPUPercUsage = cpuUsedProc[cpuUsedProc.Size - 1];
			avgCPUPercUsage = totalCPUPercUsage;
		}
		else
			avgCPUPercUsage = totalCPUPercUsage / (float)cuId;
		
		ImGui::PlotHistogram("CPU Used", cpuUsedProc.Data, cpuUsedProc.Size, cuId, std::to_string(int(avgCPUPercUsage)).append(" %").c_str(), 0.0f, 100.0f, ImVec2(0, 80));
	}

	ImGui::End();
}


void ChatHeads::SelectScene()
{
	CPUTAssetLibrary *pAssetLibrary = CPUTAssetLibrary::GetAssetLibrary();
	std::string mediaPath = pAssetLibrary->GetMediaDirectoryName();

	switch (mOptions.eScene)
	{
	case ChatHeadsOptions::LeagueOfLegends:		
		InitMovieTexturePlayback(mediaPath + "\\clips\\lol.ogg");
		break;

	case ChatHeadsOptions::Hearthstone:
		InitMovieTexturePlayback(mediaPath + "\\clips\\hearthstone.ogg");
		break;

	case ChatHeadsOptions::CPUT3DScene:
		if (!mpScene)	
			LoadScene(mediaPath + "\\Conservatory.scene");

		mbPlayMovie = false;
		break;
	}

	RecreateChatheadSprites();
}


bool ChatHeads::GetUIListItem(void *ptr , int index, const char **ppText)
{
	using namespace std;
	vector<VideoResolution> *pVec = reinterpret_cast<vector<VideoResolution>*> (ptr);

	if (index >= pVec->size())
		return false;

	*ppText = pVec->at(index).desc.c_str();

	return true;
}


void ChatHeads::ShowHelpMarker(const char* desc)
{
	ImGui::TextDisabled("(?)");
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip(desc);
}