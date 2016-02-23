###Chat Heads
Intel RealSense Technology can be used to improve the e-sports experience for both players and spectators by allowing them to see each other on-screen during the game. Using background segmented video (BGS), players’ “floating heads” can be overlaid on top of a game using less screen real-estate than full widescreen video and graphics can be displayed behind them.

Chat Heads is a sample that will help you understand the various pieces in the implementation (using the Intel RealSense SDK for background segmentation, networking, video compression and decompression), the social interaction, and the performance of this RealSense use case. The code in this sample is written in C++ and uses DirectX*.

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
Install the [RealSense SDK R5 or greater] (https://software.intel.com/en-us/intel-realsense-sdk/download) prior to building the sample 

####Running the sample:
Install the OpenAL runtime and RealSense DCM (depth camera manager) before running the sample.
There are no command line arguments. Just start the .exe and the UI should be self-explanatory.

The sample has been tested on Windows 8.1 and 10 with both embedded and external (usb) RealSense cameras.

---
####Project structure:
**ChatheadsNativePOC\** contains windowsMain.cpp, the sample (ChatHeads.cpp/h), RealSense wrapper (RealsenseMgr.cpp/h) and Raknet wrapper (NetworkLayer.cpp/h)

**CPUTDX\** houses our in-house 3D game engine framework

**VideoStreaming\** has the encoder and decoder logic, and uses Microsoft's WMF

**Imgui\** is the UI implementation

**Media\** contains the video clips, shader and other art assets

**TheoraPlayer\** contains the header and library files for LibTheoraPlayer and associated libraries

**Raknet\** contains the header and library files for Raknet

**itt\** contains the header and library files for libittnotify


####Code browsing pointers:
Application code is in ChatHeads.h/cpp
	
	void RenderChatheads(CPUTRenderParameters& renderParams);
	void UpdateRemoteChatheadBuffer(NetMsgVideoUpdate *pMsg);
	
RealSense stuff is in RealsenseMgr.h/cpp; Important functions include:

	bool PreInit();
	bool Init();
     	void Shutdown();
	DWORD RSThreadUpdate();


Network wrapper over Raknet is in NetworkLayer.h/cpp
	
	static DWORD WINAPI NetworkThread(LPVOID lpParam);
	bool SendVideoData(NetMsgVideoUpdate& msg, bool broadcast);
	static void ProcessVideoUpdateMsg(NetworkLayer *pNet, RakNet::Packet *pPacket);

Media code is in EncodeTransform.h/cpp and DecodeTransform.h/cpp

####Feedback
For bugs reports, fixes & feedback, please send a pull request (or) email to raja.bala@intel.com
