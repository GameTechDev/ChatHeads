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