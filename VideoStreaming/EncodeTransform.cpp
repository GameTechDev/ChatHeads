/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");// you may not use this file except in compliance with the License.// You may obtain a copy of the License at//// http://www.apache.org/licenses/LICENSE-2.0//// Unless required by applicable law or agreed to in writing, software// distributed under the License is distributed on an "AS IS" BASIS,// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.// See the License for the specific language governing permissions and// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

#include "EncodeTransform.h"
#include "ColorConversion.h"
#include "VTuneScopedTask.h"

extern __itt_domain* g_pDomain;


EncodeTransform::EncodeTransform()
{
	mStreamHeight = 0;
	mStreamWidth = 0;
}


void EncodeTransform::Init(int width, int height)
{
	VTUNE_TASK(g_pDomain, "Encoder Init");

	mStreamHeight = height;
	mStreamWidth = width;
	mCompressedBuffer = new DWORD[(width*height) / 2];

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (SUCCEEDED(hr) || (FAILED(hr) && hr == RPC_E_CHANGED_MODE))
	{
		hr = FindEncoder(MFVideoFormat_H264);
		if (FAILED(hr))
		{
			std::cout << "failed to find/Create specified encoder mft" << std::endl;
		}

		// set the media output info
		hr = SetOutputMediaType();
		if (FAILED(hr))
		{
			std::cout << "failed to SetOutputMediaType " << std::endl;
		}

		// set the media input info
		hr = SetInputMediaType();
		if (FAILED(hr))
		{
			std::cout << "failed to SetInputMediaType" << std::endl;
		}

		// query media input stream info
		hr = QueryInputStreamInfo();
		if (FAILED(hr))
		{
			std::cout << "failed to QueryInputStreamInfo" << std::endl;
		}

		{  //used to be NUM_PIXELS_YUY2 * 4
			HRESULT hr = MFCreateMemoryBuffer((mStreamWidth*mStreamHeight) * 2, &mpInputBuffer);
			if (FAILED(hr))
			{
				std::cout << "Failed to MFCreateMemoryBuffer" << std::endl;
			}

			hr = MFCreateSample(&pSampleProcIn);
			if (FAILED(hr))
			{
				std::cout << "Failed to MFCreateSample" << std::endl;
			}
			hr = pSampleProcIn->AddBuffer(mpInputBuffer);
			if (FAILED(hr))
			{
				std::cout << "Failed to AddBuffer to sample" << std::endl;
			}
		}

		{   //used to be NUM_PIXELS_YUY2 * 4
			hr = MFCreateMemoryBuffer(mStreamWidth * mStreamWidth * 2, &mpEncodedBuffer);
			if (FAILED(hr))
			{
				std::cout << "Failed to MFCreateMemoryBuffer" << std::endl;
			}

			hr = MFCreateSample(&pSampleProcOut);
			if (FAILED(hr))
			{
				std::cout << "Failed to MFCreateSample" << std::endl;
			}

			hr = pSampleProcOut->AddBuffer(mpEncodedBuffer);
			if (FAILED(hr))
			{
				std::cout << "Failed to AddBuffer to sample" << std::endl;
			}
		}
	}
}


void EncodeTransform::Shutdown()
{
	SendStreamEndMessage();
	Release(&mpEncoder);

	Release(&mpInputBuffer);
	Release(&pSampleProcIn);

	Release(&mpEncodedBuffer);
	Release(&pSampleProcOut);

	if (mCompressedBuffer)
		delete[] mCompressedBuffer;
}


// Query stream limits to determine pipeline characteristics...I.E fix size stream and Stream ID
HRESULT EncodeTransform::QueryStreamCapabilities()
{
	// if the min and max input and oupt stream counts are the same, the MFT is a Fix stream size. So we cannot add or remove streams
	HRESULT hr = mpEncoder->GetStreamLimits(&mInputStreamMin, &mInputStreamMax, &mOutputStreamMin, &mOutputStreamMax);
	if (FAILED(hr))
	{
		std::cout << "Failed to GetStreamLimits for MFT" << std::endl;
	}

	hr = mpEncoder->GetStreamIDs(mInputStreamMin, &mInputStreamID, mOutputStreamMin, &mOutputStreamID);
	if (FAILED(hr))
	{
		std::cout << "Failed to GetStreamIDs for MFT" << hr << std::endl;
	}

	return hr;
}


// Finds and returns the h264 MFT if available...otherwise fails with style.</para>
HRESULT EncodeTransform::FindEncoder(const GUID& subtype)
{
	HRESULT hr = S_OK;
	UINT32 count = 0;

	CLSID *ppCLSIDs = NULL;

	MFT_REGISTER_TYPE_INFO info = { 0 };

	info.guidMajorType = MFMediaType_Video;
	info.guidSubtype = subtype;

	hr = MFTEnum(
		MFT_CATEGORY_VIDEO_ENCODER,
		0,
		NULL,
		&info,
		NULL,
		&ppCLSIDs,
		&count
		);

	if (SUCCEEDED(hr) && count == 0)
	{
		hr = MF_E_TOPO_CODEC_NOT_FOUND;
	}

	if (SUCCEEDED(hr))
	{
		hr = CoCreateInstance(ppCLSIDs[0], NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&mpEncoder));
	}

	CoTaskMemFree(ppCLSIDs);
	return hr;
}


HRESULT EncodeTransform::SetInputMediaType() 
{
	if (!mpEncoder)
	{
		return MF_E_NOT_INITIALIZED;
	}

	IMFMediaType* pMediaTypeIn = NULL;
	HRESULT hr = MFCreateMediaType(&pMediaTypeIn);
	// Set the input media type.
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	}
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeIn->SetGUID(MF_MT_SUBTYPE, cVideoInputFormat);
	}
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeIn->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	}
	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeSize(pMediaTypeIn, MF_MT_FRAME_SIZE, mStreamWidth, mStreamHeight);
	}
	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeRatio(pMediaTypeIn, MF_MT_FRAME_RATE, VIDEO_FPS, 1);
	}
	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeRatio(pMediaTypeIn, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	}
	if (SUCCEEDED(hr))
	{
		hr = mpEncoder->SetInputType(0, pMediaTypeIn, 0); //
		if (hr == MF_E_TRANSFORM_TYPE_NOT_SET)
		{
			std::cout << "MF_E_TRANSFORM_TYPE_NOT_SET -> 0xC00D6D60L: You must set the output type first" << std::endl;
		}
		if (hr == MF_E_INVALIDMEDIATYPE)
		{
			std::cout << "MF_E_INVALIDMEDIATYPE -> 0xc00d36b4: the data specified for the media type is invalid, inconsistent, or not supported by this object" << std::endl;
		}

#if defined(CODECAPI_AVLowLatencyMode) // Win8 only
		hr = mpEncoder->QueryInterface(IID_PPV_ARGS(&mpCodecAPI));
		if (SUCCEEDED(hr))
		{
			VARIANT var;
			var.vt = VT_UI4;
			var.ulVal = eAVEncCommonRateControlMode_Quality;
			hr = mpCodecAPI->SetValue(&CODECAPI_AVEncCommonRateControlMode, &var);
			if (FAILED(hr)){printf("Failed to set rate control mode.\n");}

			var.vt = VT_BOOL;
			var.boolVal = VARIANT_TRUE;
			hr = mpCodecAPI->SetValue(&CODECAPI_AVLowLatencyMode, &var);
			if (FAILED(hr)){ printf("Failed to enable low latency mode.\n"); }

			// This property controls the quality level when the encoder is not using a constrained bit rate. The AVEncCommonRateControlMode property determines whether the bit rate is constrained.
			VARIANT quality;
			InitVariantFromUInt32(50, &quality);
			hr = mpCodecAPI->SetValue(&CODECAPI_AVEncCommonQuality, &quality);
			if (FAILED(hr)){ printf("Failed to adjust quality mode.\n"); }
		}
#endif
	}

	return hr;
}


HRESULT EncodeTransform::SetOutputMediaType()
{
	if (!mpEncoder)
	{
		return MF_E_NOT_INITIALIZED;
	}

	IMFMediaType* pMediaTypeOut = NULL;
	HRESULT hr = MFCreateMediaType(&pMediaTypeOut);
	// Set the output media type.
	if (SUCCEEDED(hr))
	{
		hr = MFCreateMediaType(&pMediaTypeOut);
	}
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	}
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeOut->SetGUID(MF_MT_SUBTYPE, cVideoEncodingFormat);
	}
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeOut->SetUINT32(MF_MT_AVG_BITRATE, VIDEO_BIT_RATE);
	}
	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeRatio(pMediaTypeOut, MF_MT_FRAME_RATE, VIDEO_FPS, 1);
	}
	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeSize(pMediaTypeOut, MF_MT_FRAME_SIZE, mStreamWidth, mStreamHeight);
	}
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	}
	if (SUCCEEDED(hr))
	{
		hr = pMediaTypeOut->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_Base);
	}

	if (SUCCEEDED(hr))
	{
		hr = MFSetAttributeRatio(pMediaTypeOut, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	}
	if (SUCCEEDED(hr))
	{
		hr = mpEncoder->SetOutputType(0, pMediaTypeOut, 0);
	}
	return hr;
}


// Gets the buffer requirements and other information for the input stream of the MFT
HRESULT EncodeTransform::QueryInputStreamInfo()
{
	if (!mpEncoder)
	{
		return MF_E_NOT_INITIALIZED;
	}
	HRESULT  hr = mpEncoder->GetInputStreamInfo(mInputStreamID, &mInputStreamInfo);
	if (FAILED(hr))
	{
		std::cout << " Failed to queried input stream info" << std::endl;
	}
	return hr;
}


// Gets the buffer requirements and other information for the output stream of the MFT
HRESULT EncodeTransform::QueryOutputStreamInfo()
{
	if (!mpEncoder)
	{
		return MF_E_NOT_INITIALIZED;
	}
	HRESULT hr = mpEncoder->GetOutputStreamInfo(mOutputStreamID, &mOutputStreamInfo);

	// Flag to specify that the MFT provides the output sample for this stream (throught internal allocation or operating on the input stream)
	if (&mOutputStreamInfo != nullptr)
	{
		mOutputStreamInfo.dwFlags |= MFT_OUTPUT_STREAM_PROVIDES_SAMPLES;
	}
	if (SUCCEEDED(hr))
	{
		std::cout << "Failed to query output stream info" << std::endl;
	}
	return hr;
}


// Send request to MFT to allocate necessary resources for streaming
HRESULT EncodeTransform::SendStreamStartMessage()
{
	if (!mpEncoder)
	{
		return MF_E_NOT_INITIALIZED;
	}
	HRESULT hr = mpEncoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL);
	if (FAILED(hr))
	{
		std::cout << "Failed to send start stream request" << std::endl;
	}

	hr = mpEncoder->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, NULL);
	if (FAILED(hr))
	{
		std::cout << "Failed to send start stream request" << std::endl;
	}
	return hr;
}


HRESULT EncodeTransform::CommandDrain()
{
	if (!mpEncoder)
	{
		return MF_E_NOT_INITIALIZED;
	}
	HRESULT hr = mpEncoder->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, NULL);
	mInputComplete = true;
	if (FAILED(hr))
	{
		std::cout << "Failed to send end stream request" << std::endl;
	}
	return hr;
}

// Send request to MFT to de-allocate necessary resources for streaming
HRESULT EncodeTransform::SendStreamEndMessage()
{
	if (!mpEncoder)
	{
		return MF_E_NOT_INITIALIZED;
	}
	HRESULT hr = mpEncoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, NULL);
	if (FAILED(hr))
	{
		std::cout << "Failed to send end stream request" << std::endl;
	}
	return hr;
}


// Deliver an input stream to the MFT
HRESULT EncodeTransform::ProcessInput()
{
	IMFSample *pSample = NULL;
	HRESULT hr = mpEncoder->ProcessInput(mInputStreamID, pSample, NULL);
	if (SUCCEEDED(hr))
	{
		std::cout << "ENCODER - processed input" << std::endl;
	}
	return S_OK;
}

// Create a new sample for mft input from segemtation image buffer and pass in the timestamp
HRESULT EncodeTransform::AddSample(const LONGLONG& rtStart)
{
	VTUNE_TASK(g_pDomain, "AddSample");

	const LONG cYUY2WidthBytes = mStreamWidth * 2; // halfwidth * 4
	const DWORD cbBuffer = cYUY2WidthBytes * mStreamHeight;

	BYTE *pData = NULL;

	// Lock the buffer and copy the video frame to the buffer.
	HRESULT hr = mpInputBuffer->Lock(&pData, NULL, NULL);
	
	if (SUCCEEDED(hr))
	{
		hr = MFCopyImage(
			pData,						// Destination buffer.
			cYUY2WidthBytes,			// Destination stride.
			(BYTE*)mCompressedBuffer,	// First row in source image.
			cYUY2WidthBytes,			// Source stride.
			cYUY2WidthBytes,			// Image width in bytes.
			mStreamHeight				// Image height in pixels.
			);

		hr = mpInputBuffer->Unlock();
	}


	// Set the data length of the buffer.
	if (SUCCEEDED(hr))
	{
		hr = mpInputBuffer->SetCurrentLength(cbBuffer);
	}

	// Set the time stamp and the duration.
	if (SUCCEEDED(hr))
	{
		hr = pSampleProcIn->SetSampleTime(rtStart);
	}
	if (SUCCEEDED(hr))
	{
		hr = pSampleProcIn->SetSampleDuration(VIDEO_FRAME_DURATION);
	}

	hr = mpEncoder->ProcessInput(mInputStreamID, pSampleProcIn, NULL);

	return hr;
}


HRESULT EncodeTransform::ProcessOutput(EncoderOutput& etn)
{
	VTUNE_TASK(g_pDomain, "ProcessOutput");

	if (mpEncoder == NULL)
	{
		return MF_E_NOT_INITIALIZED;
	}

	MFT_OUTPUT_STREAM_INFO mftStreamInfo = { 0 };
	MFT_OUTPUT_DATA_BUFFER mftOutputData = { 0 };

	HRESULT hr = mpEncoder->GetOutputStreamInfo(mOutputStreamID, &mftStreamInfo);
	if (FAILED(hr))
	{
		std::cout << "Failed to GetOutputStreamInfo" << std::endl;
	}


	//Set the output sample
	DWORD dwStatus = 0;
	mftOutputData.dwStreamID = mOutputStreamID;
	mftOutputData.dwStatus = 0;
	mftOutputData.pEvents = NULL;
	mftOutputData.pSample = pSampleProcOut;

	//Generate the output sample
	hr = mpEncoder->ProcessOutput(0, 1, &mftOutputData, &dwStatus);
	if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
	{
		std::cout << "ENCODER - Needs more input before processing output." << std::endl;
	}
	if (hr == MF_E_NOTACCEPTING)
	{
		std::cout << "ENCODER - Not accepting input." << std::endl;
	}
	if (SUCCEEDED(hr))
	{
		byte *byteBuffer;
		LONGLONG duration = 0;
		LONGLONG time = 0;
		DWORD buffCurrLen = 0;
		DWORD buffMaxLen = 0;

		hr = pSampleProcOut->GetSampleDuration(&duration);
		hr = pSampleProcOut->GetSampleTime(&time);

		hr = mpEncodedBuffer->GetCurrentLength(&buffCurrLen);
		hr = mpEncodedBuffer->GetMaxLength(&buffMaxLen);
		hr = mpEncodedBuffer->Lock(&byteBuffer, &buffMaxLen, &buffCurrLen);

		etn.pEncodedData	= byteBuffer;
		etn.numBytes		= buffCurrLen;
		etn.duration		= duration;
		etn.timestamp		= time;
	}

	return hr;
}


EncoderOutput EncodeTransform::EncodeData(char *pRGBAData, size_t numBytes)
{	
	VTUNE_TASK(g_pDomain, "EncodeData");

	const size_t numRGBAPixels = numBytes >> 2; // 4 bytes per pixel (TODO:Cleanup)
	
	EncoderOutput etn;

	etn.pEncodedData = NULL;
	etn.numBytes = 0;
	etn.returnCode = S_FALSE;

	size_t yuy2PixelIndex = 0;
	// The first step is to compress to YUV by taking in the current and next pixel (for averaging) n1 and n2, n2 and n3, n3 and n4,....

	DWORD *pRGBADataAsDWORD = reinterpret_cast<DWORD*> (pRGBAData);

	for (size_t rgbaIndex = 0; rgbaIndex < numRGBAPixels; rgbaIndex = rgbaIndex + 2)
	{
		//Here we convert RGB to YUY2 and add to the encode buffer (omiting intermediate step, go to function for more detail)
		mCompressedBuffer[yuy2PixelIndex++] = ColorConversion::RGBtoYUY2(pRGBADataAsDWORD[rgbaIndex], 
																		pRGBADataAsDWORD[rgbaIndex + 1], 
																		mbEncodeBackgroundPixels,
																		mEncodingThreshold);
	}

	SendStreamEndMessage();

	// Add a sample with the frame timestamp
	 HRESULT hr = AddSample(mTimeStamp);

	if (SUCCEEDED(hr))
	{
		etn.returnCode = ProcessOutput(etn);
	}

	mTimeStamp += VIDEO_FRAME_DURATION;

	return etn;
}


// User's responsibility to call this function to unlock buffer after calling EncodeData to get the buffer ptr
HRESULT EncodeTransform::Unlock()
{
	HRESULT hr = mpEncodedBuffer->Unlock();

	return hr;
}