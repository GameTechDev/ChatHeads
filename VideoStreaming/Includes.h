#ifndef __VIDEO_STREAMING_INCLUDES__
#define __VIDEO_STREAMING_INCLUDES__

#include <mfapi.h>
#include <mfobjects.h>
#include <mftransform.h>
#include <codecapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <assert.h>
#include <stdio.h>
#include <tchar.h>
#include <evr.h>
#include <mfplay.h>
#include <mfreadwrite.h>
#include <wmcodecdsp.h>
#include <fstream>
#include <iostream>

const UINT32 VIDEO_FPS = 30;
// MFSample::GetSampleTime method Retrieves the presentation time of the sample.
const UINT64 VIDEO_FRAME_DURATION = 10 * 1000 * 1000 / VIDEO_FPS;
const UINT32 VIDEO_BIT_RATE = 800000; // bits per second
const GUID   VIDEO_INPUT_FORMAT = MFVideoFormat_YUY2;

template <class T> void Release(T **ppT)
{
	if (*ppT)
	{
		(*ppT)->Release();
		*ppT = NULL;
	}
}

#endif