#ifndef __RS_THREAD_H__
#define __RS_THREAD_H__

#include <windows.h> // HANDLE, LONG, DWORD, WINAPI
#include <string>
#include <vector>
#include "pxcversion.h"

#define IL_UNLOCK	0
#define IL_LOCK		1
#define RSSDK_BGS_FREQ_MAJ_VERSION 6

struct ImageBuffer
{
	int		width;
	int		height;
	byte	*pBuffer;
};

// All data shared between the main thread and the realsense thread
struct RSAppSharedData
{
	ImageBuffer		segImage;
	volatile LONG	hLock;
	volatile bool	bStayAlive;
	volatile bool	bDoSegmentation;
	bool			bImageUpdated;
	HANDLE			hThread;				
};


struct VideoResolution
{
	int			width;
	int			height;
	std::string desc;

public:
	// return true is first arg is "before" the second
	static bool compare(const VideoResolution& vr1, const VideoResolution& vr2)
	{
		if (vr1.width < vr2.width)
			return true;
		else if (vr1.width == vr2.width)
			return (vr1.height < vr2.height);

		return false;
	}
};


// forward decls (thanks C++)
class PXCSession;
class PXCSenseManager;
class PXC3DSeg;

// RealsenseMgr abstracts away all realsense related functionality.
// The main application needs to:
// i)   call PreInit to check if RS runtime exists and get the resolutions (profiles) supported by BGS
// ii)  call SetResolution to select a profile
// iii) call Init which brings up the RS camera; also spawns the thread that takes care of video updates
// iv)  lock/unlock mSharedData.hMutex to get segmented image data from mSharedData.segImage
// v)   call Shutdown when you want to end the RS session
class RealsenseMgr
{
public:
	static DWORD WINAPI StaticRSThreadFunc(void* t);

public:
	bool PreInit();
	bool Init();
	void Shutdown();
	void PauseBGS(bool pause);
#if PXC_VERSION_MAJOR >= RSSDK_BGS_FREQ_MAJ_VERSION
	void SetBGSFrameSkipInterval(int numFrames);
#endif
	// getters/setters
	inline void DoSegmentation(bool bDoBGS) { mSharedData.bDoSegmentation = bDoBGS; }
	inline std::vector<VideoResolution>& GetResolutions() { return mResolutions; };
	inline void SetResolution(int index) { mActiveResolution = index; }
	inline int VideoWidth() const { return mVideoWidth; }
	inline int VideoHeight() const { return mVideoHeight; }
	inline bool InitSuccess() const { return mbInitSuccess; }
	inline int SDKVersionMajor() const { return mSDKVersionMajor; }
	inline int SDKVersionMinor() const { return mSDKVersionMinor; }
	inline int DCMVersionMajor() const { return mDCMVersionMajor; }
	inline int DCMVersionMinor() const { return mDCMVersionMinor; }

private:
	DWORD RSThread();
	void FindValidResolutions();

public:
	static const UINT	cBytesPerPixel = 4;
	static const int	cVideoFps = 30;
	RSAppSharedData		mSharedData;

private:
	PXCSession			*mpSession = nullptr;
	PXCSenseManager		*mpSenseMgr = nullptr;
	PXC3DSeg			*mp3DSeg = nullptr;
	bool				mbInitSuccess = false;
	std::vector<VideoResolution> mResolutions;
	int					mActiveResolution = 0;
	int					mVideoWidth = 0;
	int					mVideoHeight = 0;
	int					mSDKVersionMajor = 0;
	int					mSDKVersionMinor = 0;
	int					mDCMVersionMajor = 0;
	int					mDCMVersionMinor = 0;
};

#endif __RS_THREAD_H__
