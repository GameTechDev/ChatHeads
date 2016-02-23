###Chat Heads
Chat Heads uses RealSense to overlay background segmented (BGS) player images on a 3D scene or video playback in a multiplayer scenario. 
The goal is twofold: 
(i) to demonstrate a RealSense use-case that can improve the player/spectator e-sport experience 
(ii) serve as an example implementation to help understand the various pieces (realsense, network, media), their interactions and performance criteria

For more info, please checkout https://software.intel.com/en-us/articles/chat-heads-and-intel-realsense-sdk-background-segmentation-boosts-esport-experience

---
####Library dependencies:
- Realsense SDK
- Raknet for networking
- Microsoft Windows Media Foundation (WMF) framework for encoding/decoding
- Intel Instrumentation and Tracing Technology (ITT) for performance analysis
- Libtheoraplayer/libtheora/libvorbis/libogg and OpenAL for converting video into texture data and for ogg playback
- IMGui for UI
 
####Building the sample:
Install the RealSense SDK prior to building the sample (include path for header depends on RSSDK_DIR env variable)

####Running the sample:
Install the OpenAL runtime and RealSense DCM (depth camera manager) before running the sample.
There are no command line arguments. Just start the .exe and the UI should be self-explanatory.
The sample has been tested on Windows 8.1 and 10 systems with an embedded and external RealSense camera.

---
####Project structure:
ChatheadsNativePOC\ contains windowsMain.cpp, the sample (ChatHeads.cpp/h), RealSense wrapper (RealsenseMgr.cpp/h) and Raknet wrapper (NetworkLayer.cpp/h)

CPUTDX\ houses our in-house 3D game engine framework

VideoStreaming\ has the encoder and decoder logic, and uses Microsoft's WMF

Imgui\ is the UI implementation

Media\ contains the video clips, shader and other art assets

TheoraPlayer\ contains the header and library files for LibTheoraPlayer and associated libraries

Raknet\ contains the header and library files for Raknet

itt\ contains the header and library files for libittnotify


####Code browsing pointers:
RealSense stuff is in RealsenseMgr.h/cpp; Important functions include:

	bool PreInit();
	
	bool Init();
       	
    void Shutdown();
       	
	DWORD RSThreadUpdate();


Network wrapper over Raknet is in NetworkLayer.h/cpp
	
	static DWORD WINAPI NetworkThread(LPVOID lpParam);
       	
	bool SendVideoData(NetMsgVideoUpdate& msg, bool broadcast);
       	
	static void ProcessVideoUpdateMsg(NetworkLayer *pNet, RakNet::Packet *pPacket);

Sample code is in ChatHeads.h/cpp
	
	void RenderChatheads(CPUTRenderParameters& renderParams);


Media code is in EncodeTransform.h/cpp and DecodeTransform.h/cpp

####Feedback
For bugs reports, fixes & feedback, please send a pull request (or) email to raja.bala@intel.com
