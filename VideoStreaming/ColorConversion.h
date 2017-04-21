/////////////////////////////////////////////////////////////////////////////////////////////
// Copyright 2017 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");// you may not use this file except in compliance with the License.// You may obtain a copy of the License at//// http://www.apache.org/licenses/LICENSE-2.0//// Unless required by applicable law or agreed to in writing, software// distributed under the License is distributed on an "AS IS" BASIS,// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.// See the License for the specific language governing permissions and// limitations under the License.
/////////////////////////////////////////////////////////////////////////////////////////////

#ifndef _COLOR_CONVERSION_H_
#define _COLOR_CONVERSION_H_

#include <Windows.h>
#include <math.h>

class ColorConversion
{
public:
	// non fancy colors (BGRA)
	static const DWORD RGBBlue = 0xFF000000;
	static const DWORD RGBGreen = 0x00FF0000;
	static const DWORD RGBRed = 0x0000FF00;

	static const DWORD BGRBlue = 0xFF000000;
	static const DWORD BGRGreen = 0x00FF0000;
	static const DWORD BGRRed = 0x0000FF00;

	static DWORD RGBtoYUY2(DWORD& ARGB1, DWORD& ARGB2, bool bEncodeBackgroundPixels = false, int encodingThreshold = 0);
	static DWORDLONG YUY2toRGB(DWORD& YUV, bool bEncodeBackgroundPixels, int channelThreshold = 0);
	static void YUY2toRGBBuffer(byte *pYUY2Buffer, DWORD yuy2BufLength, byte *pRGBBuffer, int VIDEO_WIDTH, int VIDEO_HEIGHT, bool bEncodeBgPixels = false, int channelThreshold = 0);

	static BYTE Clip(int n);

	//static DWORD YUV422toBGRA(DWORD* YUV442);
	//static DWORD BGRAtoYUV422(DWORD* BGRA1, DWORD* BGRA2);

};
#endif // _COLOR_CONVERSION_H_