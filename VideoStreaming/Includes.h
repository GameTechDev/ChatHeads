/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");// you may not use this file except in compliance with the License.// You may obtain a copy of the License at//// http://www.apache.org/licenses/LICENSE-2.0//// Unless required by applicable law or agreed to in writing, software// distributed under the License is distributed on an "AS IS" BASIS,// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.// See the License for the specific language governing permissions and// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

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