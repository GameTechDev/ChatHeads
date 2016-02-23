
//--------------------------------------------------------------------------------------
// Copyright 2014 Intel Corporation
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
#define UV_LAYER_1


cbuffer cbPerModelValues
{
    row_major float4x4 World : WORLD;
    row_major float4x4 NormalMatrix : WORLD;
    row_major float4x4 WorldViewProjection : WORLDVIEWPROJECTION;
    row_major float4x4 InverseWorld : WORLDINVERSE;
    row_major float4x4 LightWorldViewProjection;
              float4   BoundingBoxCenterWorldSpace  < string UIWidget="None"; >;
              float4   BoundingBoxHalfWorldSpace    < string UIWidget="None"; >;
              float4   BoundingBoxCenterObjectSpace < string UIWidget="None"; >;
              float4   BoundingBoxHalfObjectSpace   < string UIWidget="None"; >;
};

// -------------------------------------
cbuffer cbPerFrameValues
{
    row_major float4x4  View;
    row_major float4x4  InverseView : ViewInverse	< string UIWidget="None"; >;
    row_major float4x4  Projection;
    row_major float4x4  ViewProjection;
              float4    AmbientColor < string UIWidget="None"; > = .20;
              float4    LightColor < string UIWidget="None"; >   = 1.0f;
              float4    LightDirection  : Direction < string UIName = "Light Direction";  string Object = "TargetLight"; string Space = "World"; int Ref_ID=0; > = {0,0,-1, 0};
              float4    EyePosition;
              float4    TotalTimeInSeconds < string UIWidget="None"; >;
};

// -------------------------------------
struct VS_INPUT
{
    float3 Position : POSITION; // Projected position
    float3 Normal   : NORMAL;
    float2 UV0      : TEXCOORD0;
#ifdef UV_LAYER_1
    float2 UV1      : TEXCOORD1;
#endif
#ifdef USE_NORMALMAP
    float3 Tangent  : TANGENT;
    float3 Binormal : BINORMAL;
#endif
};

// -------------------------------------
struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 Normal   : NORMAL;
#ifdef USE_NORMALMAP
    float3 Tangent  : TANGENT;
    float3 Binormal : BINORMAL;
#endif
    float2 UV0      : TEXCOORD0;
#ifdef UV_LAYER_1
    float2 UV1      : TEXCOORD1;
#endif
    float3 WorldPosition : TEXCOORD2; // Object space position 
    float3 Reflection : TEXCOORD3;
    float4 LightUV  : LIGHTSPACEPOSITION;
};

/* 
Requires Diffuse, specular, specularity, ambient 
Optional defines (for material file):
    UV_LAYER_1
    USE_NORMALMAP (allows normal function)
    USE_EMISSIVE  (allows emissive function)
*/
// -------------------------------------
#ifdef _CPUT
    SamplerState SAMPLER0 : register( s0 );
    Texture2D texture_ST : register( t0 );
    Texture2D texture_DM : register( t1 );
    Texture2D texture_SM : register( t2 );
    Texture2D texture_NM : register( t3 );
    Texture2D texture_AO : register( t4 );
#else
texture2D texture_ST < string Name = "texture_ST"; string UIName = "texture_ST"; string ResourceType = "2D";>;
sampler2D SAMPLER0 = sampler_state{ texture = (texture_ST); MinFilter = Linear; MagFilter = Linear; MipFilter = Linear; };
bool texture_STsRGB = true;
texture2D texture_DM < string Name = "texture_DM"; string UIName = "texture_DM"; string ResourceType = "2D";>;
sampler2D SAMPLER1 = sampler_state{ texture = (texture_DM); MinFilter = Linear; MagFilter = Linear; MipFilter = Linear; };
bool texture_DMsRGB = true;
texture2D texture_SM < string Name = "texture_SM"; string UIName = "texture_SM"; string ResourceType = "2D";>;
sampler2D SAMPLER2 = sampler_state{ texture = (texture_SM); MinFilter = Linear; MagFilter = Linear; MipFilter = Linear; };
bool texture_SMsRGB = true;
texture2D texture_NM < string Name = "texture_NM"; string UIName = "texture_NM"; string ResourceType = "2D";>;
sampler2D SAMPLER3 = sampler_state{ texture = (texture_NM); MinFilter = Linear; MagFilter = Linear; MipFilter = Linear; };
bool texture_NMsRGB = true;
texture2D texture_AO < string Name = "texture_AO"; string UIName = "texture_AO"; string ResourceType = "2D";>;
sampler2D SAMPLER4 = sampler_state{ texture = (texture_AO); MinFilter = Linear; MagFilter = Linear; MipFilter = Linear; };
bool texture_AOsRGB = true;
float4 Linear2SRGB(float4 color)
{
   return float4(pow(color.rgb,0.45454545),color.a);
}

float4 SRGB2Linear(float4 color, bool isSRGB)
{
	if(isSRGB)
	{
		return float4(pow(color.rgb,2.2),color.a);
	}
	else
	{
	 return color;
	}
}
#endif

// -------------------------------------
float4 DIFFUSE( PS_INPUT input )
{
    return 
#ifdef _CPUT
texture_DM.Sample( SAMPLER0, (((input.UV0)) *(2.000000)) )
#else
SRGB2Linear(tex2D( SAMPLER1, (((input.UV0)) *(2.000000)) ), texture_DMsRGB)
#endif
;
}

// -------------------------------------
float4 SPECULARTMP( PS_INPUT input )
{
    return 
#ifdef _CPUT
texture_SM.Sample( SAMPLER0, (((input.UV0)) *(20.000000)) )
#else
SRGB2Linear(tex2D( SAMPLER2, (((input.UV0)) *(20.000000)) ), texture_SMsRGB)
#endif
;
}

// -------------------------------------
float4 SPECULARITY( PS_INPUT input )
{
    return ((( (
#ifdef _CPUT
texture_ST.Sample( SAMPLER0, (((input.UV0)) *(20.000000)) )
#else
SRGB2Linear(tex2D( SAMPLER0, (((input.UV0)) *(20.000000)) ), texture_STsRGB)
#endif
 ).r )) *(128.000000)) +(4.000000);
}

// -------------------------------------
float4 NORMALS( PS_INPUT input )
{
    return 
#ifdef _CPUT
texture_NM.Sample( SAMPLER0, (((input.UV0)) *(2.000000)) )
#else
SRGB2Linear(tex2D( SAMPLER3, (((input.UV0)) *(2.000000)) ), texture_NMsRGB)
#endif
 * 2.0 - 1.0;
}

// -------------------------------------
float4 SPECULAR( PS_INPUT input )
{
    return (SPECULARTMP(input)) *(50.000000);
}

// -------------------------------------
float4 AMBIENTOCC( PS_INPUT input )
{
    return 
#ifdef _CPUT
texture_AO.Sample( SAMPLER0, ((input.UV1)) )
#else
SRGB2Linear(tex2D( SAMPLER4, ((input.UV1)) ), texture_AOsRGB)
#endif
;
}

// -------------------------------------
float4 AMBIENT( PS_INPUT input )
{
    return ((DIFFUSE(input)) *(AMBIENTOCC(input))) *(5.000000);
}

// -------------------------------------

/*
// -------------------------------------
float4 CLIP( PS_INPUT input )
{
    return (ALPHA(input)) -(0.5);
}
*/
Texture2D    _Shadow;
SamplerComparisonState SHADOW_SAMPLER : register( s1 );
// -------------------------------------
float ComputeShadowAmount( PS_INPUT input )
{
#ifdef _CPUT
    float3  lightUV = input.LightUV.xyz / input.LightUV.w;
    lightUV.xy = lightUV.xy * 0.5f + 0.5f; // TODO: Move to matrix?
    lightUV.y  = 1.0f - lightUV.y;
    float  shadowAmount      = _Shadow.SampleCmp( SHADOW_SAMPLER, lightUV, lightUV.z );
    return shadowAmount;
#else
    return 1.0f;
#endif
}

// -------------------------------------
PS_INPUT VSMain( VS_INPUT input )
{
    PS_INPUT output = (PS_INPUT)0;

    output.Position      = mul( float4( input.Position, 1.0f), WorldViewProjection );
    output.WorldPosition = mul( float4( input.Position, 1.0f), World ).xyz;

    // TODO: transform the light into object space instead of the normal into world space
    output.Normal   = mul( input.Normal, (float3x3)World );
#ifdef USE_NORMALMAP
    output.Tangent  = mul( input.Tangent, (float3x3)World );
    output.Binormal = mul( input.Binormal, (float3x3)World );
#endif
    output.UV0 = input.UV0;
#ifdef UV_LAYER_1
    output.UV1 = input.UV1;
#endif
    output.LightUV = mul( float4( input.Position, 1.0f), LightWorldViewProjection );

    return output;
}

float3 ComputeNormal(PS_INPUT input)
{
    float3 normal;
#ifdef USE_NORMALMAP
        float3x3 tangentToWorld = float3x3(input.Tangent, input.Binormal, input.Normal);
        normal = normalize( mul( NORMALS(input), tangentToWorld ));
#else
        normal = normalize(input.Normal);
#endif
    return normal;
}


// -------------------------------------
float4 PSMain( PS_INPUT input ) : SV_Target
{
    float4 result = float4(0.0, 0.0, 0.0, 1.0);

#ifdef ENABLE_CLIP
    result.a = ALPHA(input).a;
    clip(result.a - CLIPTHRESHOLD(input).a);
#endif
    float  shadowAmount = ComputeShadowAmount(input);
    
    float3 normal = ComputeNormal(input);
    
    // Ambient-related computation
    float3 ambient = AmbientColor * AMBIENT(input);
    result.xyz +=  ambient;
    float3 lightDirection = -LightDirection.xyz;

    // Diffuse-related computation
    float4 diffuseTexture = DIFFUSE(input);
    float  nDotL         = saturate( dot( normal, lightDirection ) );    
    
    // Specular-related computation
    float3 eyeDirection  = normalize(input.WorldPosition - EyePosition);
    float3 reflection    = normalize(reflect( eyeDirection, normal ));
    if(nDotL > 0.0)
    {
        float shadowAmount = ComputeShadowAmount(input);
        float3 diffuseLight = LightColor.rgb * nDotL * shadowAmount * diffuseTexture.rgb;
        result = float4((diffuseLight + result.rgb), diffuseTexture.a); 
    
        float  rDotL         = saturate(dot( reflection, lightDirection ));
        float3 specular      = pow(rDotL,  SPECULARITY(input) );
        specular             = shadowAmount * specular;
        specular            *= SPECULAR(input);
        result.xyz += LightColor.rgb * specular;
    }   
#ifdef USE_EMISSIVE
    result.xyz += EMISSIVE(input).rgb;
#endif
    return result;
}

technique10 Simple10
{
    pass p0
    {
        SetVertexShader(
            CompileShader(
            vs_4_0,
            VSMain()));

        SetGeometryShader(NULL);

        SetPixelShader(
            CompileShader(
            ps_4_0,
            PSMain()));
    }
}
