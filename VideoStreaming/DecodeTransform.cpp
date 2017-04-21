/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");// you may not use this file except in compliance with the License.// You may obtain a copy of the License at//// http://www.apache.org/licenses/LICENSE-2.0//// Unless required by applicable law or agreed to in writing, software// distributed under the License is distributed on an "AS IS" BASIS,// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.// See the License for the specific language governing permissions and// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

#include "DecodeTransform.h"
#include "VTuneScopedTask.h"

extern __itt_domain* g_pDomain;


void DecodeTransform::Shutdown()
{
	if (mpRGBABuffer)
	{
		delete mpRGBABuffer;
		mpRGBABuffer = NULL;
	}


	Release(&mpDecoder);
	mbInitSuccess = false;
}


// Finds and initializes decoder MFT
void DecodeTransform::Init(int width, int height)
{
	mStreamHeight = height;
	mStreamWidth = width;

	// Create the buffer to hold decoded RGBA data
	mpRGBABuffer = new byte[mStreamWidth* mStreamHeight * 4];

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	MFStartup(MF_VERSION);

	// Create H.264 decoder.
	HRESULT hr = CoCreateInstance(CLSID_CMSH264DecoderMFT, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&mpDecoder));
	if (FAILED(hr))
	{
		std::cout << "Failed to create H264 decoder MFT.\n";
		return;
	}

	// configure the decoder input media type
	hr = SetDecoderInputType();
	if (FAILED(hr))
		return;

	// configure the decoder output media type
	hr = SetDecoderOutputType();
	if (FAILED(hr))
		return;

	hr = mpDecoder->GetInputStatus(0, &mftStatus);
	if (FAILED(hr))
	{
		std::cout << "Failed to get input status from H.264 decoder MFT.\n";
		return;
	}
	if (MFT_INPUT_STATUS_ACCEPT_DATA != mftStatus)
	{
		printf("H.264 decoder MFT is not accepting data.\n");
		return;
	}

	mbInitSuccess = true;
}


/// Set the input attributes for the decoder MFT
HRESULT DecodeTransform::SetDecoderInputType()
{
	// Set the input media type for the decoder
	IMFMediaType* pMediaTypeIn = NULL;

	HRESULT hr = MFCreateMediaType(&pMediaTypeIn);
	if (FAILED(hr))
	{
		std::cout << "Failed to get IMFMediaType interface.\n";
	}

	hr = pMediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	if (FAILED(hr))
	{
		std::cout << "Failed to Set IMFMediaType major type attribute.\n";
	}

	hr = pMediaTypeIn->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	if (FAILED(hr))
	{
		std::cout << "Failed to Set IMFMediaType subtype type attribute.\n";
	}

	hr = MFSetAttributeRatio(pMediaTypeIn, MF_MT_FRAME_RATE, VIDEO_FPS, 1);
	if (FAILED(hr))
	{
		std::cout << "Failed to Set IMFMediaType subtype type attribute.\n";
	}

	hr = MFSetAttributeSize(pMediaTypeIn, MF_MT_FRAME_SIZE, mStreamWidth, mStreamHeight);
	if (FAILED(hr))
	{
		std::cout << "Failed to set frame size on H264 MFT in type.\n";
	}

	hr = mpDecoder->SetInputType(0, pMediaTypeIn, 0);
	if (FAILED(hr))
	{
		std::cout << "Failed to set input media type on H.264 decoder MFT.\n";
	}
	return hr;
}


// set the output attributes decoder MFT
HRESULT DecodeTransform::SetDecoderOutputType()
{
	IMFMediaType *pMediaTypeOut = NULL;
	HRESULT hr = MFCreateMediaType(&pMediaTypeOut);
	if (FAILED(hr))
	{
		std::cout << "Failed to create media type.\n";
	}

	hr = pMediaTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	if (FAILED(hr))
	{
		std::cout << "Failed to set MF_MT_MAJOR_TYPE.\n";
	}

	hr = pMediaTypeOut->SetGUID(MF_MT_SUBTYPE, VIDEO_INPUT_FORMAT);// what is the difference between NV12 and IYUV?
	if (FAILED(hr))
	{
		std::cout << "Failed to set MF_MT_SUBTYPE.\n";
	}

	hr = pMediaTypeOut->SetUINT32(MF_MT_AVG_BITRATE, VIDEO_BIT_RATE);
	if (FAILED(hr))
	{
		std::cout << "Failed to set MF_MT_AVG_BITRATE.\n";
	}

	hr = MFSetAttributeSize(pMediaTypeOut, MF_MT_FRAME_SIZE, mStreamWidth, mStreamHeight);
	if (FAILED(hr))
	{
		std::cout << "Failed to set frame size on H264 MFT out type.\n";
	}

	hr = MFSetAttributeRatio(pMediaTypeOut, MF_MT_FRAME_RATE, VIDEO_FPS, 1);
	if (FAILED(hr))
	{
		std::cout << "Failed to set frame rate on H264 MFT out type.\n";
	}

	hr = MFSetAttributeRatio(pMediaTypeOut, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	if (FAILED(hr))
	{
		std::cout << "Failed to set aspect ratio on H264 MFT out type.\n";
	}

	hr = pMediaTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	if (FAILED(hr))
	{
		std::cout << "Failed to set MF_MT_INTERLACE_MODE.\n";
	}

	hr = mpDecoder->SetOutputType(0, pMediaTypeOut, 0);
	if (FAILED(hr))
	{
		//MF_E_INVALIDMEDIATYPE
		std::cout << "Failed to set output media type on H.264 decoder MFT.\n";
	}
	return hr;
}


// reconstructs the sample from encoded data
DecoderOutput DecodeTransform::DecodeData(byte *pYUY2Buffer, DWORD bufferLength, LONGLONG& time, LONGLONG& duration)
{
	DecoderOutput dtn;
	dtn.numBytes = 0;
	dtn.pDecodedData = NULL;
	dtn.returnCode = S_FALSE;

	IMFSample *pSample = NULL;
	IMFMediaBuffer *pMBuffer = NULL;

	// Create a new memory buffer.
	HRESULT hr = MFCreateMemoryBuffer(bufferLength, &pMBuffer);

	// Lock the buffer and copy the video frame to the buffer.
	BYTE *pData = NULL;
	if (SUCCEEDED(hr))
		hr = pMBuffer->Lock(&pData, NULL, NULL);

	if (SUCCEEDED(hr))
		memcpy(pData, pYUY2Buffer, bufferLength);

	if (pYUY2Buffer)
	{
		pMBuffer->SetCurrentLength(bufferLength);
		pMBuffer->Unlock();
	}

	// Create a media sample and add the buffer to the sample.
	if (SUCCEEDED(hr))
		hr = MFCreateSample(&pSample);

	if (SUCCEEDED(hr))
		hr = pSample->AddBuffer(pMBuffer);

	// Set the time stamp and the duration.
	if (SUCCEEDED(hr))
		hr = pSample->SetSampleTime(time);

	if (SUCCEEDED(hr))
		hr = pSample->SetSampleDuration(duration);

	hr = ProcessSample(&pSample, time, duration, dtn);

	Release(&pSample);
	Release(&pMBuffer);

	return dtn;
}


// Process the incoming sample
HRESULT DecodeTransform::ProcessSample(IMFSample **ppSample, LONGLONG& time, LONGLONG& duration, DecoderOutput& oDtn)
{
	IMFMediaBuffer *buffer = nullptr;
	DWORD bufferSize;
	HRESULT hr = (*ppSample)->ConvertToContiguousBuffer(&buffer);
	if (SUCCEEDED(hr))
	{
		buffer->GetCurrentLength(&bufferSize);
	}
	if (ppSample)
	{
		hr = ProcessInput(ppSample);
		if (SUCCEEDED(hr))
		{
			//std::cout << "DECODER - Processed Input." << std::endl;

			while (hr != MF_E_TRANSFORM_NEED_MORE_INPUT)
			{
				hr = ProcessOutput(time, duration, oDtn);
			}

			if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
			{
			}
			if (hr == MF_E_NOTACCEPTING)
			{
			}
		}
		if (hr == MF_E_NOTACCEPTING)
		{
			//hr = decoder->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, NULL);

			while (hr != MF_E_TRANSFORM_NEED_MORE_INPUT)
			{
				hr = ProcessOutput(time, duration, oDtn);
			}

		}

	}

	return hr;
}


// Process the output sample for the decoder.
HRESULT DecodeTransform::ProcessInput(IMFSample **ppSample)
{
	VTUNE_TASK(g_pDomain, "ProcessInput");

	mInputCount += 1;
	HRESULT hr = mpDecoder->ProcessInput(0, *ppSample, 0);
	return hr;
}


// Process the output sample for the decoder
HRESULT DecodeTransform::ProcessOutput(LONGLONG& time, LONGLONG& duration, DecoderOutput& oDtn/*output*/)
{
	VTUNE_TASK(g_pDomain, "ProcessOutput");


	IMFMediaBuffer *pBuffer = NULL;
	DWORD mftOutFlags;
	MFT_OUTPUT_DATA_BUFFER outputDataBuffer;
	IMFSample *pMftOutSample = NULL;
	MFT_OUTPUT_STREAM_INFO streamInfo;

	memset(&outputDataBuffer, 0, sizeof outputDataBuffer);

	HRESULT hr = mpDecoder->GetOutputStatus(&mftOutFlags);
	if (FAILED(hr))
	{
	}

	hr = mpDecoder->GetOutputStreamInfo(0, &streamInfo);
	if (FAILED(hr))
	{
	}

	hr = MFCreateSample(&pMftOutSample);
	if (FAILED(hr))
	{
	}

	hr = MFCreateMemoryBuffer(streamInfo.cbSize, &pBuffer);
	if (FAILED(hr))
	{	
	}
	hr = pMftOutSample->AddBuffer(pBuffer);
	if (FAILED(hr))
	{
	}
	outputDataBuffer.dwStreamID = 0;
	outputDataBuffer.dwStatus = 0;
	outputDataBuffer.pEvents = NULL;
	outputDataBuffer.pSample = pMftOutSample;

	hr = mpDecoder->GetOutputStreamInfo(0, &streamInfo);
	if (SUCCEEDED(hr))
	{
		hr = mpDecoder->ProcessOutput(0, 1, &outputDataBuffer, 0);
	}
	if (SUCCEEDED(hr))
	{
		GetDecodedBuffer(outputDataBuffer.pSample, outputDataBuffer, time, duration, oDtn);
	}

	Release(&pBuffer);
	Release(&pMftOutSample);

	return hr;
}


// Write the decoded sample out to a file
HRESULT DecodeTransform::GetDecodedBuffer(IMFSample *pMftOutSample, MFT_OUTPUT_DATA_BUFFER& outputDataBuffer, LONGLONG& time, LONGLONG& duration, DecoderOutput& oDtn/*output*/)
{
	VTUNE_TASK(g_pDomain, "WriteToFile");

	// ToDo: These two lines are not right. Need to work out where to get timestamp and duration from the H264 decoder MFT.
	HRESULT hr = outputDataBuffer.pSample->SetSampleTime(time);
	if (SUCCEEDED(hr))
	{
		hr = outputDataBuffer.pSample->SetSampleDuration(duration);
	}

	if (SUCCEEDED(hr))
	{
		hr = pMftOutSample->ConvertToContiguousBuffer(&pDecodedBuffer);
	}
	
	if (SUCCEEDED(hr))
	{
		DWORD bufLength;
		hr = pDecodedBuffer->GetCurrentLength(&bufLength);
	}
	
	if (SUCCEEDED(hr))
	{
		byte *pEncodedYUVBuffer;
		DWORD buffCurrLen = 0;
		DWORD buffMaxLen = 0;
		pDecodedBuffer->GetCurrentLength(&buffCurrLen);
		pDecodedBuffer->Lock(&pEncodedYUVBuffer, &buffMaxLen, &buffCurrLen);
		ColorConversion::YUY2toRGBBuffer(pEncodedYUVBuffer, 
										buffCurrLen, 
										mpRGBABuffer,
										mStreamWidth,
										mStreamHeight,
										mbEncodeBackgroundPixels,
										mChannelThreshold);

		pDecodedBuffer->Unlock();		
		Release(&pDecodedBuffer);

		// to show yuy2 image (2 alternate clones) , uncomment below
		//dtn.pDecodedData = pEncodedYUVBuffer;
		//dtn.numBytes = VIDEO_WIDTH* VIDEO_HEIGHT * 2;
		//dtn.returnCode = hr; // will be S_OK..

		oDtn.pDecodedData = mpRGBABuffer;
		oDtn.numBytes = mStreamWidth * mStreamHeight * 4;
		oDtn.returnCode = hr; // will be S_OK..
		/*outputBuffer->write((char *)byteBuffer, bufLength);
		outputBuffer->flush();
		*/
	}
		
	return hr;
}


// Send request to MFT to allocate necessary resources for streaming
HRESULT DecodeTransform::SendStreamStartMessage()
{
	if (!mpDecoder)
	{
		return MF_E_NOT_INITIALIZED;
	}
	HRESULT hr = mpDecoder->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, NULL);
	if (FAILED(hr))
	{
	}
	return hr;
}


// Send request to MFT to de-allocate necessary resources for streaming
HRESULT DecodeTransform::SendStreamEndMessage()
{
	if (!mpDecoder)
	{
		return MF_E_NOT_INITIALIZED;
	}
	HRESULT hr = mpDecoder->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, NULL);
	if (FAILED(hr))
	{
	}
	return hr;
}