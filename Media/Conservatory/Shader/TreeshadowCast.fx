//--------------------------------------------------------------------------------------
// Copyright 2011 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//--------------------------------------------------------------------------------------


#include "..\..\..\..\CPUT\CPUT\CPUTGlobalShaderInclude.fx"

// ********************************************************************************************************
struct VS_INPUT
{
    float3 Pos      : POSITION; // Projected position
    float2 UV0      : TEXCOORD0;
};
struct PS_INPUT
{
    float4 Pos      : SV_POSITION;
    float2 UV0      : TEXCOORD0;
};

// ********************************************************************************************************
cbuffer cbPerModelValues
{
    row_major float4x4 World : WORLD;
    row_major float4x4 WorldViewProjection : WORLDVIEWPROJECTION;
    row_major float4x4 InverseWorld : INVERSEWORLD;
              float4   LightDirection;
              float4   EyePosition;
    row_major float4x4 LightWorldViewProjection;
};

SamplerState SAMPLER0 : register( s0 );
Texture2D texture_DM : register( t1 );

// ********************************************************************************************************
PS_INPUT VSMain( VS_INPUT input )
{
    PS_INPUT output = (PS_INPUT)0;
    output.Pos = mul( float4( input.Pos, 1.0f), WorldViewProjection );
//    output.Pos.z -= 0.0003 * output.Pos.w;  
  output.UV0 = input.UV0;

    return output;
}

// ********************************************************************************************************
void PSMain( PS_INPUT input )
{

	float4 inputTexture = texture_DM.Sample( SAMPLER0, input.UV0 );

	clip(inputTexture.a - 0.01);

 //   return float4(1,0,0,1);
}
