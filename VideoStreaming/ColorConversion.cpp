/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");// you may not use this file except in compliance with the License.// You may obtain a copy of the License at//// http://www.apache.org/licenses/LICENSE-2.0//// Unless required by applicable law or agreed to in writing, software// distributed under the License is distributed on an "AS IS" BASIS,// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.// See the License for the specific language governing permissions and// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

#include "ColorConversion.h"
#include "Includes.h"

//<summary>
///<para> Convert RGB-888 -> YUV-444 -> YUV-422 using integer.</para>
/////We take each pixel of the frame buffer in pairs and scale two DWORD down to a single DWORD represented as the YUV color space
///</summary>
DWORD ColorConversion::RGBtoYUY2(DWORD& ARGB1, DWORD& ARGB2, bool bEncodeBackgroundPixels, int encodingThreshold)
{
	//First convert RGB-888 to the YUV-444 colorspace
	byte A1 = (ARGB1 >> 24);
	byte A2 = (ARGB2 >> 24);
	
	byte R1 = (ARGB1 >> 16) & 0xFF;
	byte G1 = (ARGB1 >> 8) & 0xFF;
	byte B1 = (ARGB1 & 0xFF);

	byte R2 = (ARGB2 >> 16) & 0xFF;
	byte G2 = (ARGB2 >> 8) & 0xFF;
	byte B2 = (ARGB2 & 0xFF);

	// Detect transparent (i.e. background) pixels and set the encoded value to 0
	if (bEncodeBackgroundPixels)
	{
		if (A1 <= encodingThreshold || A2 <= encodingThreshold)
		{
			return 0;
		}
	}

	int Y1 = ((66 * R1 + 129 * G1 + 25 * B1 + 128) >> 8) + 16;
	int U1 = ((-38 * R1 - 74 * G1 + 112 * B1 + 128) >> 8) + 128;
	int V1 = ((112 * R1 - 94 * G1 - 18 * B1 + 128) >> 8) + 128;

	int Y2 = ((66 * R2 + 129 * G2 + 25 * B2 + 128) >> 8) + 16;
	int U2 = ((-38 * R2 - 74 * G2 + 112 * B2 + 128) >> 8) + 128;
	int V2 = ((112 * R2 - 94 * G2 - 18 * B2 + 128) >> 8) + 128;

	// Now convert the YUV-444 to the yuv-422 colorspace
	DWORD fccYUY2 = NULL;
	fccYUY2 |= (Y1) << 24;				// set the y1
	fccYUY2 |= ( ( ((U1 + U2) / 2) & 0xFF ) << 16);	// set the u1-2
	fccYUY2 |= ((Y2 & 0xFF)  << 8);				// set the y2
	fccYUY2 |= ((V1 + V2) / 2) & 0xFF;			// set the v1-2

	return fccYUY2;
}

///<summary>
///<para> Convert YUV-422 -> YUV-422 -> RGB-888 using floating point.</para>
/// YUV-422 is one pixel at 16 bytes, referred to as YUVY where Y1 and Y2 are average RGB data points per pixel and U, V are average between those pixels
/// Pixel 0: U0 Y0 V0  | Pixel 1: U0 Y1 V0  | Pixel 2: U2 Y2 V2  | Pixel 2: U2 Y3 V2 
///</summary>
DWORDLONG ColorConversion::YUY2toRGB(DWORD& YUY2, bool bEncodeBackgroundPixels, int decodingThreshold)
{
	if (bEncodeBackgroundPixels)
	{
		if (YUY2 == 0)
			return 0;
	}
	
	// Convert from YUV-422 to YUV-444
	int y1 = (YUY2 >> 24) & 0xFF;
	int  u = (YUY2 >> 16) & 0xFF;
	int y2 = (YUY2 >> 8) & 0xFF;
	int  v = (YUY2 & 0xFF);

	if (bEncodeBackgroundPixels)
	{
		if (y1 < decodingThreshold && y2 < decodingThreshold && u < decodingThreshold && v < decodingThreshold)
			return 0;
	}

	int C1 = y1 - 16;
	int C2 = y2 - 16;
	int D = u - 128;
	int E = v - 128;

	// Integer based
	byte R1 = Clip((298 * C1 + 409 * E + 128) >> 8);
	byte G1 = Clip((298 * C1 - 100 * D - 208 * E + 128) >> 8);
	byte B1 = Clip((298 * C1 + 516 * D + 128) >> 8);
	
	// Conversion for the second pixel
	byte R2 = Clip((298 * C2 + 409 * E + 128) >> 8);
	byte G2 = Clip((298 * C2 - 100 * D - 208 * E + 128) >> 8);
	byte B2 = Clip((298 * C2 + 516 * D + 128) >> 8);
		
	byte alpha = 0xFF; // non-transparent pixels are fully opaque

	if (R1 == 0 && R2 == 0 && G1 == 0 && G2 == 0 && B1 == 0 && B2 == 0)
		alpha = 0;

	DWORD BGRA1 = 0;
	//First pixel
	BGRA1 |= (alpha << 24); //A
	BGRA1 |= (R1 << 16);	//R
	BGRA1 |= (G1 << 8);		//G
	BGRA1 |= (B1);			//B

	DWORD BGRA2 = 0;
	//Second pixel
	BGRA2 |= (alpha << 24); //A
	BGRA2 |= (R2 << 16);   	//R
	BGRA2 |= (G2 << 8); 	//G
	BGRA2 |= (B2); 			//B

	DWORDLONG retVal = 0;
	retVal |= (BGRA1);
	retVal |= DWORDLONG(BGRA2) << 32;

	return retVal;
}

//<summary>
///<para> Clip overflow to within range.</para>
///</summary>
BYTE ColorConversion::Clip(int n)
{
	return n = n < 0 ? 0 : (n > 0xFF ? 0xFF : n);
}


void ColorConversion::YUY2toRGBBuffer(byte *pYUY2Buffer, DWORD yuy2BufLength, byte *pRGBBuffer, int VIDEO_WIDTH, int VIDEO_HEIGHT, bool bEncodeBgPixels, int channelThreshold)
{
	DWORD *pYUY2BufferAsDWORD = reinterpret_cast<DWORD*> (pYUY2Buffer);
	DWORDLONG *pRGBBufferAsDWORDLONG = reinterpret_cast<DWORDLONG*> (pRGBBuffer);
	const size_t numYUY2Pixels = VIDEO_WIDTH / 2 * VIDEO_HEIGHT;

	for (unsigned int i = 0; i < numYUY2Pixels; i++)
	{
		pRGBBufferAsDWORDLONG[i] = YUY2toRGB(pYUY2BufferAsDWORD[i], bEncodeBgPixels, channelThreshold);
	}	
}