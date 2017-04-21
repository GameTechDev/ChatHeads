/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");// you may not use this file except in compliance with the License.// You may obtain a copy of the License at//// http://www.apache.org/licenses/LICENSE-2.0//// Unless required by applicable law or agreed to in writing, software// distributed under the License is distributed on an "AS IS" BASIS,// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.// See the License for the specific language governing permissions and// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __CPUT_SAMPLESTARTDX11_H__
#define __CPUT_SAMPLESTARTDX11_H__

#include <vector>

#include "CPUT_DX11.h"
#include <D3D11.h>
#include "CPUTBufferDX11.h"
#include "CPUTSprite.h"
#include "CPUTScene.h"
#include "CPUTParser.h"

#include "RealsenseMgr.h"
#undef _WINSOCKAPI_ // prevent redef in winsock2.h included in NetworkLayer.h
#include "NetworkLayer.h"
#include "TheoraPlayer.h"
#include "EncodeTransform.h"
#include "DecodeTransform.h"

// UI options to play with the sample
struct ChatHeadsOptions
{
public:
	enum SceneOptions
	{
		LeagueOfLegends,
		Hearthstone,
		CPUT3DScene
	};

	ChatHeadsOptions()
	{
		strcpy(IPAddr, "127.0.0.1");
	}

	const			int cDefaultAlphaThreshold = 12; // hacky way of encoding/decoding bg pixels: alpha/yuyv values smaller this ==> bg pixel

	bool			bIsServer = false;
	char			IPAddr[16];
	bool			bEnableBGS = true;
	SceneOptions	eScene = LeagueOfLegends;
	int				curResListIndex = 0;
	int				frameSkipInterval = 0;
	int				encodingThreshold = cDefaultAlphaThreshold; // 8 bit channel value
	int				decodingThreshold = cDefaultAlphaThreshold; // 8 bit channel value
	bool			bPauseBGS = false;
	float			chatHeadSize[2]; // wrt 100 units as full screen
	float			chatHeadPos[2]; // wrt 100 units & (0,0) being top left
	int 			nwSendInterval = 30; // ms
	bool			bVsync = true; // interval = 1 for swapchain->present
};


//-----------------------------------------------------------------------------
class ChatHeads : public CPUT_DX11
{
	// Remote player shared state between sample and network classes
	struct RemoteChathead
	{
		ImageBuffer			imgBuffer;
		bool				bWasUpdated;
		bool				bSizeChanged;
		int					newWidth;
		int					newHeight;
		volatile LONG		userLock;
	};

private:
	/********************************* CPUT default sample stuff  *******************************/
	double								mFrameRate = 0;
    CPUTCameraController				*mpCameraController = nullptr;
	CPUTScene							*mpScene = nullptr;
    CommandParser						mParsedCommandLine;
	CPUTRenderTargetDepth				*mpShadowRenderTarget = nullptr;
	bool								mDisplayGUI = true;

	/*********************************  Realsense stuff  *******************************/
	RealsenseMgr						mRSMgr;
	std::vector<CPUTSprite*>			mChatheadSprites;
	std::vector<CPUTRenderTargetColor*> mChatheadTextures;
	std::vector<RemoteChathead>			mRemoteChatheads;
	bool								mbChatheadResourcesCreated = false;

	/*********************************  Networking stuff  *******************************/
	NetworkLayer						mNetLayer;
	bool								mbInitNetwork = false;

	/*********************************  Encode/Decode stuff ***********************/
	EncodeTransform						 mEncoder; // used to encode local player's video feed
	DecodeTransform						 mDecoders[NetworkLayer::cMaxPlayers]; // used to decode each remote players' video feed (need multiple, since they keep state on last frame)

	/*********************************  Movie texure playback stuff ***********************/
	TheoraVideoManager					*mpMovieMgr = nullptr;
	TheoraVideoClip						*mpMovieClip = nullptr;
	CPUTSprite							*mpMovieSprite = nullptr;
	CPUTRenderTargetColor				*mpMovieRenderTarget = nullptr;
	CPUTMaterial						*mpMovieTexMaterial = nullptr;
	ULONGLONG							mMovieTickCount = 0;
	bool								mbPlayMovie = false;

	/*********************************  GUI stuff  ****************************************/
	ChatHeadsOptions					mOptions;

public:
	static void NetMsgCallback(NetworkMsg eMsg, void *pThis, void *pMsg);
	static bool GetUIListItem(void*, int, const char**);

    void CreateBasicCPUTResources();
	void ReleaseBasicCPUTResources();
    void LoadScene(std::string filename);

	virtual CPUTEventHandledCode HandleKeyboardEvent(CPUTKey key, CPUTKeyState state) override;
	virtual CPUTEventHandledCode HandleMouseEvent(int x, int y, int wheel, CPUTMouseState state, CPUTEventID message) override;    
	virtual void Create() override;
	virtual void Render(double deltaSeconds) override;
	virtual void Update(double deltaSeconds) override;
	virtual void ResizeWindow(UINT width, UINT height) override;
	virtual void Shutdown() override;

private:
	/*********************************  Realsense stuff  *******************************/
	void InitRealsenseAndEncoder();
	void CreateRSResources();
	void ReleaseRSResources();
	void ShutdownRealsense();
	void CreateChatheadSprites();
	void RecreateChatheadSprites();
	void DrawChatheadSprites(CPUTRenderParameters& rp);
	void CreateDefaultChatheadResources();	
	void RecreateRemoteResourcesIfNeedBe();
	void RenderChatheads(CPUTRenderParameters& renderParams);

	/*********************************  Networking stuff  *******************************/
	void UpdateRemoteChatheadBuffer(NetMsgVideoUpdate *pMsg);


	/*********************************  Movie texure playback stuff ***********************/
	void InitMovieTexturePlayback(std::string moviePath);
	void CreateMovieResources();
	void ReleaseMovieResources();
	void ReleaseMovieClip();
	void UpdateMovieTexture(unsigned char* pframe, CPUTRenderParameters& renderParams);

	/*********************************  UI ***********************/
	void InitIMGUI();
	void ProcessUI();
	void PersistentUI();
	void NetworkUI();
	void PostStartUI();
	void SystemMetricsUI();
	void SelectScene();
	static inline void ShowHelpMarker(const char *text);
};



#endif // __CPUT_SAMPLESTARTDX11_H__
