#include "RealsenseMgr.h"

#include "pxc3dseg.h" // PXC3DSeg
#include "pxcsensemanager.h" // PXCSession, PXCSenseManager

#include "VTuneScopedTask.h"
#include "SetThreadName.h"
#include "CPUTOSServices.h" // CPUT logging

#include <algorithm>
#include <iostream>

using namespace CPUTFileSystem;
extern CPUTLog Log;
extern __itt_domain* g_pDomain;

#define RS_STATUS_CHECK(status, log) \
	if (##status < PXC_STATUS_NO_ERROR) { \
		##log.Log(LOG_ERROR, "RS status check fail");  \
		return false; \
	}

// Event handler to get 3D segmentation alerts. Currently logs them to the VS output area
class MyPXC3DSegEventHandler : public PXC3DSeg::AlertHandler
{
public:
	MyPXC3DSegEventHandler() {};
	~MyPXC3DSegEventHandler() {};
	void PXCAPI OnAlert(const PXC3DSeg::AlertData& data)
	{
		using namespace std;

		switch (data.label)
		{
			case PXC3DSeg::ALERT_USER_TOO_FAR:
			{
				Log.Log(LOG_INFO, "You are outside the ideal operating range, please move closer.\n");
				break;
			}
			case PXC3DSeg::ALERT_USER_TOO_CLOSE:
			{
				Log.Log(LOG_INFO, "You are outside the ideal operating range, please move back.\n");
				break;
			}
			case PXC3DSeg::ALERT_USER_IN_RANGE:
			{
				Log.Log(LOG_INFO, "You are now in the ideal operating range.\n");
				break;
			}
		}
	};
} g_handler;


// Utility function to get version of any RS module
PXCSession::ImplVersion GetVersion(PXCSession *session, PXCBase *module)
{
	PXCSession::ImplDesc mdesc = {};
	session->QueryModuleDesc(module, &mdesc);
	return mdesc.version;
}


/********************************************************* public functions *******************************************************************/
// Initialize the session, sense manager, 3D segmentation modality and populate the list of valid resolutions (for 3D segmentation)
// Return false if something went wrong
bool RealsenseMgr::PreInit()
{
	VTUNE_TASK(g_pDomain, "RS BasicInit");

	pxcStatus status;

	// print SDK info
	mpSession = PXCSession::CreateInstance();
	if (!mpSession)
	{
		Log.Log(LOG_ERROR, "Couldn't create PXCSession\n");
		return false;
	}

	PXCSession::ImplVersion ver;
	ver = mpSession->QueryVersion();
	mSDKVersionMajor = ver.major;
	mSDKVersionMinor = ver.minor;
	Log.Log(LOG_INFO, "RS SDK info: %d.%d\n", mSDKVersionMajor, mSDKVersionMinor);

	mpSenseMgr = mpSession->CreateSenseManager();
	if (!mpSenseMgr)
	{
		Log.Log(LOG_ERROR, "Sense manager instance was not created\n");
		return false;
	}
	Log.Log(LOG_INFO, "Sense manager instance was created\n");

	status = mpSenseMgr->Enable3DSeg();

	if (status < PXC_STATUS_NO_ERROR)
	{
		Log.Log(LOG_ERROR, "3D segmentation modality wasn't enabled\n");
		return false;
	}
	Log.Log(LOG_INFO, "3D segmentation modality was enabled\n");


	mp3DSeg = mpSenseMgr->Query3DSeg();
	if (!mp3DSeg) {
		Log.Log(LOG_ERROR, "Couldn't get the 3D segmentation instance\n");
		return false;
	}
	Log.Log(LOG_INFO, "Got the 3D segmentation instance\n");

	mp3DSeg->Subscribe(&g_handler);

	FindValidResolutions();

	return true;
}


// Initializes the Realsense camera with the selected profile (resolution, fps)
// Spawns the thread that takes care of updating the shared data buffer with the video stream data
// Note: Call PreInit() & SetResolution before Init()
bool RealsenseMgr::Init()
{
	VTUNE_TASK(g_pDomain, "RS Init");

	if (!mpSenseMgr){
		Log.Log(LOG_ERROR, "Sense manager initialization failed. Did you call PreInit()? \n");
		return false;
	}

	pxcStatus status;

	VideoResolution& vr = mResolutions[mActiveResolution];
	status = mpSenseMgr->EnableStream(PXCCapture::STREAM_TYPE_COLOR, vr.width, vr.height, static_cast<pxcF32>(cVideoFps));
	RS_STATUS_CHECK(status, Log);
	status = mpSenseMgr->EnableStream(PXCCapture::STREAM_TYPE_DEPTH);
	RS_STATUS_CHECK(status, Log);
	mpSenseMgr->EnableStream(PXCCapture::STREAM_TYPE_IR);
	RS_STATUS_CHECK(status, Log);

	status = mpSenseMgr->Init();
	if (status < PXC_STATUS_NO_ERROR)
	{
		Log.Log(LOG_ERROR, "Sense manager initialization failed\n");
		mbInitSuccess = false;
		return false;
	}
	mbInitSuccess = true;
	Log.Log(LOG_INFO, "Sense manager initialization succeeded\n");

	PXCSession::ImplVersion ver = GetVersion(mpSession, mpSenseMgr->QueryCaptureManager()->QueryCapture());
	mDCMVersionMajor = ver.major;
	mDCMVersionMinor = ver.minor;

	// Note: QueryDevice can be called only after sense mgr is initialized
	PXCCapture::Device *pDevice = mpSenseMgr->QueryCaptureManager()->QueryDevice();
	PXCCapture::Device::MirrorMode MirrorMode = pDevice->QueryMirrorMode();
	pDevice->SetMirrorMode(PXCCapture::Device::MIRROR_MODE_HORIZONTAL); // image is flipped horz by default..

	// get image stream width and height (ensure it's what we set earlier) to create the shared data buffer
	PXCSizeI32 size = mpSenseMgr->QueryCaptureManager()->QueryImageSize(PXCCapture::STREAM_TYPE_COLOR);
	mVideoWidth = size.width;
	mVideoHeight = size.height;
	mSharedData.segImage.width = mVideoWidth;
	mSharedData.segImage.height = mVideoHeight;
	mSharedData.segImage.pBuffer = new byte[size.width * size.height * cBytesPerPixel];

	// create userlock (atomic) and thread 
	mSharedData.hLock = IL_UNLOCK;
	mSharedData.bImageUpdated = false;
	// mutex is expensive, although it does let the thread sleep when it has to wait. going with cheap userlock instead
	DWORD threadID;
	mSharedData.hThread = CreateThread(NULL, 0, RealsenseMgr::StaticRSThreadFunc, (void*)this, 0, &threadID);
	SetThreadName(threadID, "RSThread");

	mSharedData.bStayAlive = true;

	return true;
}


void RealsenseMgr::Shutdown()
{
	VTUNE_TASK(g_pDomain, "RS Shutdown");

	if (!mbInitSuccess)
		return;
	
	mbInitSuccess = false;
	mSharedData.bStayAlive = false;
	WaitForSingleObject(mSharedData.hThread, INFINITE);
	CloseHandle(mSharedData.hThread);
	Log.Log(LOG_INFO, "Realsense Update thread has been shutdown");
	//CloseHandle(mSharedData.hMutex);

	if (mpSenseMgr) {
		mpSenseMgr->Release();
		mpSenseMgr = nullptr;
	}

	if (mpSession) {
		mpSession->Release();
		mpSession = nullptr;
	}
	// don't manually release the 3D segmentation module intsance; it's taken care of internally	
	
	// class is responsible for creation and deletion of the shared data buffer.	
	if (mSharedData.segImage.pBuffer)
		delete[] mSharedData.segImage.pBuffer;
}


void RealsenseMgr::PauseBGS(bool pause)
{
	mpSenseMgr->Pause3DSeg(pause);
}


#if PXC_VERSION_MAJOR >= RSSDK_BGS_FREQ_MAJ_VERSION
// Coarse control over the frequency at which BGS runs (0 = every frame, 1 = alternate frames, ..)
void RealsenseMgr::SetBGSFrameSkipInterval(int frameInterval)
{
	mp3DSeg->SetFrameSkipInterval(frameInterval);
}
#endif


DWORD WINAPI RealsenseMgr::StaticRSThreadFunc(void* t)
{
	return static_cast<RealsenseMgr*>(t)->RSThread();
}


/********************************************************* private functions *******************************************************************/
// Realsense thread that locks a frame, gets segmented image, updates shared data buffer and releases locks
// (this is a non-static member function, so use the static wrapper below to start the thread)
DWORD RealsenseMgr::RSThread()
{
	while (mSharedData.bStayAlive)
	{
		// We don't use signals/mutex to synchronize with the realsense thread because AcquireFrame blocks until the color buffer is ready
		// So, the thread doesn't spin, and doesn't really need to be woken up by the app. It can "just run"!

		pxcStatus status;
		{
			VTUNE_TASK(g_pDomain, "AcquireFrame");
			status = mpSenseMgr->AcquireFrame(true);
		}

		if (status < PXC_STATUS_NO_ERROR)
			continue;

		// Going with a user lock instead of a mutex since getting the frame takes a while (~30 ms?). The loop doesn't really spin, and we can avoid system calls caused by using a mutex.		
		if (InterlockedExchange(&mSharedData.hLock, IL_LOCK) == IL_UNLOCK)
		{
			// just get color if bgs is disabled
			if (!mSharedData.bDoSegmentation)
			{
				mpSenseMgr->AcquireFrame(true);

				const PXCCapture::Sample *pSample = mpSenseMgr->QuerySample();

				PXCImage::ImageInfo info = pSample->color->QueryInfo();

				PXCImage::ImageData data;
				pSample->color->AcquireAccess(PXCImage::ACCESS_READ, PXCImage::PIXEL_FORMAT_RGB32, &data);

				// copy the image data to the shared buffer
				pxcU16 *pSrc = (pxcU16 *)data.planes[0];
				byte *pDst = (byte*)mSharedData.segImage.pBuffer;
				const size_t numBytes = info.width * info.height * cBytesPerPixel;
				memcpy(pDst, pSrc, numBytes);

				pSample->color->ReleaseAccess(&data);
			}
			else // do background segmentation
			{
				// get the segmented image (needs to be deallocated later)
				PXCImage *pImage = nullptr;
				{
					VTUNE_TASK(g_pDomain, "AcquireSegmentedImage");
					pImage = mp3DSeg->AcquireSegmentedImage();
				}
				if (!pImage)
					goto ReleaseFrame;

				{
					VTUNE_TASK(g_pDomain, "CopyDataToSharedBuffer");
					// lock image for read
					PXCImage::ImageData pImgData;
					status = pImage->AcquireAccess(PXCImage::ACCESS_READ,
						PXCImage::PixelFormat::PIXEL_FORMAT_RGB32 /*assuming 8 bits per channel*/,
						//PXCImage::PixelFormat::PIXEL_FORMAT_YUY2, // segmented image is only BGRA (YUY2 doesn't encode alpha)
						&pImgData);

					if (status < PXC_STATUS_NO_ERROR)
					{
						Log.Log(LOG_ERROR, "Couldn't acquire access to segmented image");
						InterlockedExchange(&mSharedData.hLock, IL_UNLOCK);
						goto ReleaseFrame;
					}

					// copy the image data to the shared buffer
					byte *pSrc = (byte*)pImgData.planes[0]; // color plane
					byte *pDst = (byte*)mSharedData.segImage.pBuffer;
					PXCImage::ImageInfo info = pImage->QueryInfo();
					const size_t numBytes = info.width * info.height * cBytesPerPixel;
					memcpy(pDst, pSrc, numBytes);

					// unlock image
					status = pImage->ReleaseAccess(&pImgData);
					if (status < PXC_STATUS_NO_ERROR)
					{
						Log.Log(LOG_ERROR, "Couldn't release access to segmented image");
						InterlockedExchange(&mSharedData.hLock, IL_UNLOCK);
						goto ReleaseFrame;
					}

					// release image copy (RSSDK owns this)
					pImage->Release();
				}
			}
			InterlockedExchange(&mSharedData.hLock, IL_UNLOCK);
			mSharedData.bImageUpdated = true; // at this point, other thread can lock if it has tested this bool
		}

		// release frame to resume streaming
	ReleaseFrame:
		mpSenseMgr->ReleaseFrame();
	}

	return 0;
}


// Fill mResolutions with valid resolutions when using 3D segmentation
void RealsenseMgr::FindValidResolutions()
{
	using namespace std;

	mResolutions.clear();

	PXCVideoModule::DataDesc videoProfile;

	for (int profileId = 0;; profileId++) {

		pxcStatus sts = mp3DSeg->QueryInstance<PXCVideoModule>()->QueryCaptureProfile(profileId, &videoProfile);;

		if (sts < PXC_STATUS_NO_ERROR) break;

		VideoResolution vr;
		vr.width = videoProfile.streams.color.sizeMax.width;
		vr.height = videoProfile.streams.color.sizeMax.height;
		vr.desc = std::to_string(vr.width) + "x" + std::to_string(vr.height);

		mResolutions.push_back(vr);
	}

	sort(mResolutions.begin(), mResolutions.end(), VideoResolution::compare);
}


