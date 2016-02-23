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