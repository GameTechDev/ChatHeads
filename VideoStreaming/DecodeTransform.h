/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");// you may not use this file except in compliance with the License.// You may obtain a copy of the License at//// http://www.apache.org/licenses/LICENSE-2.0//// Unless required by applicable law or agreed to in writing, software// distributed under the License is distributed on an "AS IS" BASIS,// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.// See the License for the specific language governing permissions and// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _DECODE_TRANSFORM_H_
#define _DECODE_TRANSFORM_H_

#include "Includes.h"
#include "ColorConversion.h"

struct DecoderOutput
{
	byte		*pDecodedData;
	DWORD		numBytes;
	HRESULT		returnCode;
};

class DecodeTransform
{
public:
	bool mbEncodeBackgroundPixels = false;
	bool mbInitSuccess = false;

	DecoderOutput DecodeData(byte *pBuffer, DWORD bufferLength, LONGLONG& time, LONGLONG& duration);
	void Init(int width, int height);
	void Shutdown();
	void SetThreshold(int threshold) { mChannelThreshold = threshold; }

private:
	HRESULT SetDecoderInputType();
	HRESULT SetDecoderOutputType();
	HRESULT GetDecodedBuffer(IMFSample *pMftOutSample, MFT_OUTPUT_DATA_BUFFER& outputDataBuffer, LONGLONG& time, LONGLONG& duration, DecoderOutput& oDtn/*output*/);
	HRESULT ProcessInput(IMFSample **ppSample);
	HRESULT ProcessOutput(LONGLONG& time, LONGLONG& duration, DecoderOutput& oDtn/*output*/);
	HRESULT SendStreamStartMessage();
	HRESULT ProcessSample(IMFSample **ppSample, LONGLONG& time, LONGLONG& duration, DecoderOutput& oDtn/*output*/);
	HRESULT SendStreamEndMessage();

	IMFTransform *mpDecoder = NULL;
	LONGLONG mVideoTimeStamp = 0;
	DWORD mftStatus = 0;	
	//std::ofstream *outputBuffer;
	int mInputCount = 0;
	int mOutputCount = 0;
	IMFMediaBuffer *pDecodedBuffer = NULL;
	byte *mpRGBABuffer = NULL;
	int mStreamWidth = 0;
	int mStreamHeight = 0;
	int mChannelThreshold = 0;
};

#endif // _DECODE_TRANSFORM_H_