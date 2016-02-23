//--------------------------------------------------------------------------------------
// Copyright 2012 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURposE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//--------------------------------------------------------------------------------------

// ********************************************************************************************************
struct VS_INPUT
{
    float3 pos     : posITION;
    float2 uv      : TEXCOORD0;
};
struct PS_INPUT
{
    float4 pos     : SV_posITION;
    float2 uv      : TEXCOORD0;
};

cbuffer RgbNv16
{
	// .r = 4x Y texel width (= 1 / Output texture width * 2)
	// .g = 2x Y texel width (= 1 / Output texture width)
	float4 texOffsets;
};

//-----------------------------------------------------------------------------
// Yuv->RGB coefficients

// BT.601 (Y [16 .. 235], U/V [16 .. 240]) with linear, full-range RGB output.
// Input Yuv must be first subtracted by (0.0625, 0.5, 0.5).
static const float3x3 yuvCoef = {
	1.164f, 1.164f, 1.164f,
	0.000f, -0.392f, 2.017f,
	1.596f, -0.813f, 0.000f };

// BT.709 (Y [16 .. 235], U/V [16 .. 240]) with linear, full-range RGB output.
// Input Yuv must be first subtracted by (0.0625, 0.5, 0.5).
//static const float3x3 yuvCoef = {
//	1.164f,  1.164f, 1.164f,
//	0.000f, -0.213f, 2.112f,
//	1.793f, -0.533f, 0.000f};


// ********************************************************************************************************
Texture2D    TEXTURE0 : register( t0 );
SamplerState SAMPLER0 : register( s0 );

// ********************************************************************************************************
PS_INPUT VSMain( VS_INPUT input )
{
    PS_INPUT output = (PS_INPUT)0;
    output.pos  = float4( input.pos, 1.0f);
    output.uv   = input.uv;
    return output;
}
// ********************************************************************************************************
float4 PSMainYUVToRGB( PS_INPUT input ) : SV_Target
{
	float4 pix = TEXTURE0.Sample(SAMPLER0, input.uv*float2(1, 1) /* dont need to flip y*/);

	float2 texOffsets = float2(1 / 640, 1 / 320);
	// Unpack the Yuv components
	float3 yuv = float3(
	(fmod(input.uv.x, texOffsets.r) < texOffsets.g) ? pix.r : pix.b,
	pix.g,
	pix.a);

	// Do Yuv->RGB conversion
	yuv -= float3(0.0625f, 0.5f, 0.5f);
	yuv = mul(yuv, yuvCoef); // `yuv` now contains RGB
	yuv = saturate(yuv);

	// Return RGBA
	return float4(yuv, 1.0f);
	return pix;
	return pow(pix, 0.2f);
}

float4 PSMain(PS_INPUT input) : SV_Target
{
	float4 pix = TEXTURE0.Sample(SAMPLER0, input.uv*float2(1, 1) /* dont need to flip y*/);

	return pix;
	return pow(pix, 0.2f);
}
