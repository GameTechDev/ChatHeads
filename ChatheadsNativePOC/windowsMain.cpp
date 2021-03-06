/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");// you may not use this file except in compliance with the License.// You may obtain a copy of the License at//// http://www.apache.org/licenses/LICENSE-2.0//// Unless required by applicable law or agreed to in writing, software// distributed under the License is distributed on an "AS IS" BASIS,// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.// See the License for the specific language governing permissions and// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

#define _CRTDBG_MAP_ALLOC
#ifdef _DEBUG
#ifndef DBG_NEW
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )
#define new DBG_NEW
#endif
#endif  // _DEBUG
#include <stdlib.h>
#include <crtdbg.h>

#include "ChatHeads.h"

const std::string WINDOW_TITLE = "Chat Heads";

// Application entry point.  Execution begins here.
//-----------------------------------------------------------------------------
int main(int argc, char **argv)
{
#ifdef DEBUG
#ifdef CPUT_OS_WINDOWS
    // tell VS to report leaks at any exit of the program
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
    //http://msdn.microsoft.com/en-us/library/x98tx3cf%28v=vs.100%29.aspx
    //Add a watch for �{,,msvcr110d.dll}_crtBreakAlloc� to the watch window
    //Set the value of the watch to the memory allocation number reported by your sample at exit.
    //Note that the �msvcr110d.dll� is for MSVC2012.  Other versions of MSVC use different versions of this dll; you�ll need to specify the appropriate version.
#endif
#endif
    CPUTResult result = CPUT_SUCCESS;
    int returnCode = 0;

    // create an instance of Chat Heads
    ChatHeads *pSample = new ChatHeads();	
       
    // window and device parameters
    CPUTWindowCreationParams params;	
    params.startFullscreen = false;
	params.windowWidth = 1920; params.windowHeight = 1080;
	params.windowPositionX = 0; params.windowPositionY = 0;

	result = pSample->CPUTCreateWindowAndContext(WINDOW_TITLE, params);
    ASSERT(CPUTSUCCESS(result), "CPUT Error creating window and context.");

    pSample->Create();

    returnCode = pSample->CPUTMessageLoop();

    pSample->ReleaseBasicCPUTResources();
    pSample->DeviceShutdown();

    // cleanup resources
    SAFE_DELETE(pSample);

#if defined CPUT_FOR_DX11 && defined SUPER_DEBUG_DX
    typedef HRESULT(__stdcall *fPtrDXGIGetDebugInterface)(const IID&, void**);
    HMODULE hMod = GetModuleHandle(L"Dxgidebug.dll");
    fPtrDXGIGetDebugInterface DXGIGetDebugInterface = (fPtrDXGIGetDebugInterface)GetProcAddress(hMod, "DXGIGetDebugInterface"); 
 
    IDXGIDebug *pDebugInterface;
    DXGIGetDebugInterface(__uuidof(IDXGIDebug), (void**)&pDebugInterface); 

    pDebugInterface->ReportLiveObjects(DXGI_DEBUG_D3D11, DXGI_DEBUG_RLO_ALL);
#endif 
	_CrtDumpMemoryLeaks();

    return returnCode;
}