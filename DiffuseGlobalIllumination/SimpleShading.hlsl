// Copyright (c) 2011 NVIDIA Corporation. All rights reserved.
//
// TO  THE MAXIMUM  EXTENT PERMITTED  BY APPLICABLE  LAW, THIS SOFTWARE  IS PROVIDED
// *AS IS*  AND NVIDIA AND  ITS SUPPLIERS DISCLAIM  ALL WARRANTIES,  EITHER  EXPRESS
// OR IMPLIED, INCLUDING, BUT NOT LIMITED  TO, NONINFRINGEMENT,IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL  NVIDIA 
// OR ITS SUPPLIERS BE  LIABLE  FOR  ANY  DIRECT, SPECIAL,  INCIDENTAL,  INDIRECT,  OR  
// CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT LIMITATION,  DAMAGES FOR LOSS 
// OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR ANY 
// OTHER PECUNIARY LOSS) ARISING OUT OF THE  USE OF OR INABILITY  TO USE THIS SOFTWARE, 
// EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
// Please direct any bugs or questions to SDKFeedback@nvidia.com

#include "Defines.h"

Texture2D   g_txDiffuse                         : register( t0 );
Texture2D   g_shadowMap                         : register( t1 );
Texture2D   g_txNormalMap                       : register( t39 );
Texture2D   g_txAlpha                           : register( t40 );

Texture2DArray   g_propagatedLPVRed             : register( t2 ); //used for the first level of the cascade, or for the hierarchy
Texture2DArray   g_propagatedLPVGreen           : register( t3 ); //used for the first level of the cascade, or for the hierarchy
Texture2DArray   g_propagatedLPVBlue            : register( t4 ); //used for the first level of the cascade, or for the hierarchy
Texture2DArray   g_propagatedLPV2Red            : register( t5 ); //used for the second level of the cascade
Texture2DArray   g_propagatedLPV2Green          : register( t6 ); //used for the second level of the cascade
Texture2DArray   g_propagatedLPV2Blue           : register( t7 ); //used for the second level of the cascade
Texture2DArray   g_propagatedLPV3Red            : register( t8 ); //used for the third level of the cascade
Texture2DArray   g_propagatedLPV3Green          : register( t9 ); //used for the third level of the cascade
Texture2DArray   g_propagatedLPV3Blue           : register( t10 ); //used for the third level of the cascade

Texture2DArray   g_LPVTex                       : register( t11 );

//auxiliary textures incase we are binding float textures instead of float4 textures
Texture2DArray   g_propagatedLPVRed_1           : register( t12 ); //used for the first level of the cascade, or for the hierarchy
Texture2DArray   g_propagatedLPVGreen_1         : register( t13 ); //used for the first level of the cascade, or for the hierarchy
Texture2DArray   g_propagatedLPVBlue_1          : register( t14 ); //used for the first level of the cascade, or for the hierarchy
Texture2DArray   g_propagatedLPV2Red_1          : register( t15 ); //used for the second level of the cascade
Texture2DArray   g_propagatedLPV2Green_1        : register( t16 ); //used for the second level of the cascade
Texture2DArray   g_propagatedLPV2Blue_1         : register( t17 ); //used for the second level of the cascade
Texture2DArray   g_propagatedLPV3Red_1          : register( t18 ); //used for the third level of the cascade
Texture2DArray   g_propagatedLPV3Green_1        : register( t19 ); //used for the third level of the cascade
Texture2DArray   g_propagatedLPV3Blue_1         : register( t20 ); //used for the third level of the cascade

Texture2DArray   g_propagatedLPVRed_2           : register( t21 ); //used for the first level of the cascade, or for the hierarchy
Texture2DArray   g_propagatedLPVGreen_2         : register( t22 ); //used for the first level of the cascade, or for the hierarchy
Texture2DArray   g_propagatedLPVBlue_2          : register( t23 ); //used for the first level of the cascade, or for the hierarchy
Texture2DArray   g_propagatedLPV2Red_2          : register( t24 ); //used for the second level of the cascade
Texture2DArray   g_propagatedLPV2Green_2        : register( t25 ); //used for the second level of the cascade
Texture2DArray   g_propagatedLPV2Blue_2         : register( t26 ); //used for the second level of the cascade
Texture2DArray   g_propagatedLPV3Red_2          : register( t27 ); //used for the third level of the cascade
Texture2DArray   g_propagatedLPV3Green_2        : register( t28 ); //used for the third level of the cascade
Texture2DArray   g_propagatedLPV3Blue_2         : register( t29 ); //used for the third level of the cascade

Texture2DArray   g_propagatedLPVRed_3           : register( t30 ); //used for the first level of the cascade, or for the hierarchy
Texture2DArray   g_propagatedLPVGreen_3         : register( t31 ); //used for the first level of the cascade, or for the hierarchy
Texture2DArray   g_propagatedLPVBlue_3          : register( t32 ); //used for the first level of the cascade, or for the hierarchy
Texture2DArray   g_propagatedLPV2Red_3          : register( t33 ); //used for the second level of the cascade
Texture2DArray   g_propagatedLPV2Green_3        : register( t34 ); //used for the second level of the cascade
Texture2DArray   g_propagatedLPV2Blue_3         : register( t35 ); //used for the second level of the cascade
Texture2DArray   g_propagatedLPV3Red_3          : register( t36 ); //used for the third level of the cascade
Texture2DArray   g_propagatedLPV3Green_3        : register( t37 ); //used for the third level of the cascade
Texture2DArray   g_propagatedLPV3Blue_3         : register( t38 ); //used for the third level of the cascade


Texture2D    g_txPrevDepthBuffer                : register( t6 );


SamplerState samLinear : register( s0 )
{ 
    Filter = MIN_MAG_LINEAR_MIP_LINEAR; 
};

SamplerComparisonState PCF_Sampler : register( s1 )
{
    ComparisonFunc = LESS;
    Filter = COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
    AddressU = Border;
    AddressV = Border;
    BorderColor = float4(MAX_LINEAR_DEPTH, 0, 0, 0);
};

SamplerState DepthSampler : register( s2 )
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};
SamplerState samAniso : register( s3 )
{ 
    Filter = MIN_MAG_LINEAR_MIP_LINEAR; 
};


cbuffer cbConstants : register( b0 )
{
    matrix  g_WorldViewProj            : packoffset( c0 );
    matrix  g_WorldViewIT            : packoffset( c4 );
    matrix  g_World                    : packoffset( c8 );
    matrix  g_WorldViewProjClip2Tex : packoffset( c12 );
}

cbuffer cbConstants2 : register( b1 )
{
    float4 g_lightWorldPos;

    float g_depthBiasFromGUI;
    int g_bUseSM;
    int g_minCascadeMethod;
    int g_numCascadeLevels;
}

cbuffer cbLPV_Light_index : register(b2)
{
    float3 g_LPVSpacePos;
    float padding3;
}

cbuffer cbConstants4 : register( b4 )
{
    matrix g_WorldViewProjSimple        : packoffset( c0 );
    float4 g_color                      : packoffset( c4 );
    float4 temp                         : packoffset( c5 ); 
    float4  g_sphereScale               : packoffset( c6 );
}


cbuffer cbConstantsRenderPS : register( b5 )
{
    float g_diffuseScale                : packoffset(c0.x);  //PS
    int g_useDiffuseInterreflection     : packoffset(c0.y);  //PS
    float g_directLight                 : packoffset(c0.z);  //PS
    float g_ambientLight                : packoffset(c0.w);  //PS

    int g_useFloat4s                    : packoffset(c1.x);  //PS
    int g_useFloats                     : packoffset(c1.y);  //PS
    float temp2                         : packoffset(c1.z);  //PS
    float temp3                         : packoffset(c1.w);  //PS

    float invSMSize                           : packoffset(c2.x);  //PS
    float g_NormalmapMultiplier               : packoffset(c2.y);  //PS
    int g_useDirectionalDerivativeClamping    : packoffset(c2.z);  //PS
    float g_directionalDampingAmount    : packoffset(c2.w);  //PS

    matrix g_WorldToLPVSpace            : packoffset( c3 );  //PS / VS
    matrix g_WorldToLPVSpace2           : packoffset( c7 );  //PS / VS
    matrix g_WorldToLPVSpace3           : packoffset( c11 );  //PS / VS

    matrix g_WorldToLPVSpaceRender      : packoffset( c15 );  //PS / VS
    matrix g_WorldToLPVSpaceRender2     : packoffset( c19 );  //PS / VS
    matrix g_WorldToLPVSpaceRender3     : packoffset( c23 );  //PS / VS
}

cbuffer cbConstantsMeshRenderOptions : register(b6)
{
    int g_useTexture;
    int g_useAlpha;
    float padding8;
    float padding9;
}

cbuffer cbConstantsRenderPSLPV : register( b7 )
{
    int g_numCols1          : packoffset(c0.x);    //the number of columns in the flattened 2D LPV
    int g_numRows1          : packoffset(c0.y);      //the number of columns in the flattened 2D LPV
    int LPV2DWidth1         : packoffset(c0.z);   //the total width of the flattened 2D LPV
    int LPV2DHeight1        : packoffset(c0.w);   //the total height of the flattened 2D LPV

    int LPV3DWidth1         : packoffset(c1.x);      //the width of the LPV in 3D
    int LPV3DHeight1        : packoffset(c1.y);     //the height of the LPV in 3D
    int LPV3DDepth1         : packoffset(c1.z); 
    float padding4          : packoffset(c1.w);
}

cbuffer cbConstantsRenderPSLPV2 : register( b8 )
{
    int g_numCols2          : packoffset(c0.x);    //the number of columns in the flattened 2D LPV
    int g_numRows2          : packoffset(c0.y);      //the number of columns in the flattened 2D LPV
    int LPV2DWidth2         : packoffset(c0.z);   //the total width of the flattened 2D LPV
    int LPV2DHeight2        : packoffset(c0.w);   //the total height of the flattened 2D LPV

    int LPV3DWidth2         : packoffset(c1.x);      //the width of the LPV in 3D
    int LPV3DHeight2        : packoffset(c1.y);     //the height of the LPV in 3D
    int LPV3DDepth2         : packoffset(c1.z); 
    float padding5          : packoffset(c1.w);
}

cbuffer cbPoissonDiskSamples : register( b9 )
{
    int g_numTaps                             : packoffset(c0.x);
    float g_FilterSize                        : packoffset(c0.y);
    float padding11                           : packoffset(c0.z);
    float padding12                           : packoffset(c0.w);
    float4 g_Poisson_samples[MAX_P_SAMPLES]   : packoffset(c1.x);
}




struct VS_INPUT { 
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 TextureUV : TEXCOORD0;
    float3 Tangent : TANGENT;
};

struct VS_OUTPUT { 
    float4 Pos : SV_POSITION;
    float3 Normal : NORMAL;
    float2 TextureUV : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float4 LightPos : TEXCOORD2;
    float3 LPVSpacePos : TEXCOORD3;
    float3 LPVSpacePos2 : TEXCOORD4;

    float3 LPVSpaceUnsnappedPos : TEXCOORD5;
    float3 LPVSpaceUnsnappedPos2 : TEXCOORD6;

    float3 tangent  : TANGENT;
    float3 binorm   : BINORMAL;
};

struct VS_OUTPUT_RSM { 
    float4 Pos : SV_POSITION;
    float3 Normal : NORMAL;
    float2 TextureUV : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
};

struct VS_OUTPUT_SM
{
    float4 Pos : SV_POSITION;
};

struct VS_OUTPUT_RSM_DP { 
    float4 Pos : SV_POSITION;
    float3 Normal : NORMAL;
    float2 TextureUV : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
    float4 depthMapuv : TEXCOORD2;

    float2 ScreenTex      : TEXCOORD3;
    float2 Depth          : TEXCOORD4;
};


struct PS_MRTOutput
{
    float4 Color    : SV_Target0;
    float4 Normal    : SV_Target1;
    float4 Albedo   : SV_Target2;
};

//--------------------------------------------------------
//helper routines for SH
//------------------------------------------------------- 

#define PI 3.14159265

void clampledCosineCoeff(float3 xyz, inout float4 c)
{
    c.x = PI * 0.282094792;
    c.y = ((2.0*PI)/3.0f) * -0.4886025119 * xyz.y;
    c.z = ((2.0*PI)/3.0f) *  0.4886025119 * xyz.z;
    c.w = ((2.0*PI)/3.0f) * -0.4886025119 * xyz.x;
}

float innerProductSH(float4 SH1, float4 SH2)
{
    return SH1.x*SH2.x + SH1.y*SH2.y + SH1.z*SH2.z + SH1.w*SH2.w;
}

float SHLength(float4 SH1, float4 SH2)
{
    return sqrt(innerProductSH( SH1, SH2));
}

float innerProductSH(float3 SH1, float3 SH2)
{
    return SH1.x*SH2.x + SH1.y*SH2.y + SH1.z*SH2.z;
}

void SHMul(inout float4 shVal, float mul)
{
    shVal.x *= mul;
    shVal.y *= mul;
    shVal.z *= mul;
    shVal.w *= mul;
}

float4 SHSub(float4 val1, float4 val2)
{
    return float4(val1.x-val2.x, val1.y-val2.y, val1.z-val2.z, val1.w-val2.w);
}

float4 SHNormalize(float4 SHval)
{
    float ip = SHLength(SHval,SHval);
    if(ip>0.000001)
        return float4(SHval.x/ip, SHval.y/ip, SHval.z/ip, SHval.w/ip );
    return float4(0.0f,0.0f,0.0f,0.0f);
}

float3 SHNormalize(float3 SHval)
{
    float ip = sqrt(innerProductSH(SHval,SHval));
    if(ip>0.000001)
        return float3(SHval.x/ip, SHval.y/ip, SHval.z/ip );
    return float3(0.0f,0.0f,0.0f);
}


//--------------------------------------------------------
//helper routines for PCF
//------------------------------------------------------- 

#define PCF_FILTER_STEP_COUNT 2

float poissonPCFmultitap(float2 uv, float mult, float z)
{
    float sum = 0;
    for(int i=0;i<g_numTaps;i++)
    {
        float2 offset = g_Poisson_samples[i].xy * mult;
        sum += g_shadowMap.SampleCmpLevelZero(PCF_Sampler, uv + offset, z-g_depthBiasFromGUI);
    }
    return sum /g_numTaps;
}


float regularPCFmultitapFilter(float2 uv, float2 stepUV, float z)
{
        float sum = 0;
        stepUV = stepUV / PCF_FILTER_STEP_COUNT; 
        for( float x = -PCF_FILTER_STEP_COUNT; x <= PCF_FILTER_STEP_COUNT; ++x )
            for( float y = -PCF_FILTER_STEP_COUNT; y <= PCF_FILTER_STEP_COUNT; ++y )
            {
                float2 offset = float2( x, y ) * stepUV;
                sum += g_shadowMap.SampleCmpLevelZero(PCF_Sampler, uv + offset, z-g_depthBiasFromGUI);
            }
        float numSamples = (PCF_FILTER_STEP_COUNT*2+1);
        return  ( sum / (numSamples*numSamples));
}

float lookupShadow(float2 uv, float z)
{
    //return g_shadowMap.SampleCmpLevelZero(PCF_Sampler, uv, z-g_depthBiasFromGUI);
    //return regularPCFmultitapFilter(uv, g_FilterSize * invSMSize, z);
    return poissonPCFmultitap(uv, g_FilterSize*invSMSize ,z);
}

//--------------------------------------------------------
//shaders
//------------------------------------------------------- 


VS_OUTPUT VS( VS_INPUT input )
{
    VS_OUTPUT f;
    f.Pos = mul( float4( input.Pos, 1 ), g_WorldViewProj ); 
    f.Normal = normalize( mul( input.Normal, (float3x3)g_World ) );
    f.TextureUV = input.TextureUV; 
    float3 worldPos = mul( float4( input.Pos, 1 ), g_World ); 
    f.worldPos = worldPos;
    f.LightPos = mul( float4( input.Pos, 1 ), g_WorldViewProjClip2Tex);
    f.LPVSpacePos = mul( float4(worldPos,1), g_WorldToLPVSpace );  
    f.LPVSpacePos2 = mul( float4(worldPos,1), g_WorldToLPVSpace2 ); 

    f.LPVSpaceUnsnappedPos = mul( float4(worldPos,1), g_WorldToLPVSpaceRender );  
    f.LPVSpaceUnsnappedPos2 = mul( float4(worldPos,1), g_WorldToLPVSpaceRender2 );  

    f.tangent = normalize(mul(input.Tangent.xyz, (float3x3)g_World ));
    f.binorm = cross(f.Normal,f.tangent);    

    return f;
}

//trilinear sample from a spatted 2D texture or a 2D texture array
void trilinearSample(inout float4 SHred, inout float4 SHgreen, inout float4 SHblue, float3 lookupCoords,
                    Texture2DArray LPVRed, Texture2DArray LPVGreen, Texture2DArray LPVBlue,
                    int LPV3DDepth) 
{
    
    int zL = floor(lookupCoords.z);
    int zH = min(zL+1, LPV3DDepth-1);
    float zHw = lookupCoords.z - zL;
    float zLw = 1.0f - zHw;
    float3 lookupCoordsLow = float3(lookupCoords.x, lookupCoords.y, zL);
    float3 lookupCoordsHigh = float3(lookupCoords.x, lookupCoords.y, zH);

    SHred = zLw*LPVRed.Sample(samLinear, lookupCoordsLow) + zHw*LPVRed.Sample(samLinear, lookupCoordsHigh );
    SHgreen = zLw*LPVGreen.Sample(samLinear, lookupCoordsLow) + zHw*LPVGreen.Sample(samLinear, lookupCoordsHigh );
    SHblue = zLw*LPVBlue.Sample(samLinear, lookupCoordsLow) + zHw*LPVBlue.Sample(samLinear, lookupCoordsHigh );
}

//trilinear sample from a spatted 2D texture or a 2D texture array
void trilinearSample(inout float4 SHred, inout float4 SHgreen, inout float4 SHblue, float3 lookupCoords,
                    Texture2DArray LPVRed_0,   Texture2DArray LPVRed_1,   Texture2DArray LPVRed_2,   Texture2DArray LPVRed_3,
                    Texture2DArray LPVGreen_0, Texture2DArray LPVGreen_1, Texture2DArray LPVGreen_2, Texture2DArray LPVGreen_3,
                    Texture2DArray LPVBlue_0,  Texture2DArray LPVBlue_1,  Texture2DArray LPVBlue_2,  Texture2DArray LPVBlue_3,
                    int LPV3DDepth)
{
    int zL = floor(lookupCoords.z);
    int zH = min(zL+1,LPV3DDepth-1);
    float zHw = lookupCoords.z - zL;
    float zLw = 1.0f - zHw;
    float3 lookupCoordsLow = float3(lookupCoords.x, lookupCoords.y, zL);
    float3 lookupCoordsHigh = float3(lookupCoords.x, lookupCoords.y, zH);

    SHred =   zLw*float4(LPVRed_0.Sample(samLinear, lookupCoordsLow).x,   LPVRed_1.Sample(samLinear, lookupCoordsLow).x,   LPVRed_2.Sample(samLinear, lookupCoordsLow).x,   LPVRed_3.Sample(samLinear, lookupCoordsLow).x)
            + zHw*float4(LPVRed_0.Sample(samLinear, lookupCoordsHigh).x,   LPVRed_1.Sample(samLinear, lookupCoordsHigh).x,   LPVRed_2.Sample(samLinear, lookupCoordsHigh).x,   LPVRed_3.Sample(samLinear, lookupCoordsHigh).x);
    SHgreen = zLw*float4(LPVGreen_0.Sample(samLinear, lookupCoordsLow).x, LPVGreen_1.Sample(samLinear, lookupCoordsLow).x, LPVGreen_2.Sample(samLinear, lookupCoordsLow).x, LPVGreen_3.Sample(samLinear, lookupCoordsLow).x)
            + zHw*float4(LPVGreen_0.Sample(samLinear, lookupCoordsHigh).x, LPVGreen_1.Sample(samLinear, lookupCoordsHigh).x, LPVGreen_2.Sample(samLinear, lookupCoordsHigh).x, LPVGreen_3.Sample(samLinear, lookupCoordsHigh).x);
    SHblue =  zLw*float4(LPVBlue_0.Sample(samLinear, lookupCoordsLow).x,  LPVBlue_1.Sample(samLinear, lookupCoordsLow).x,  LPVBlue_2.Sample(samLinear, lookupCoordsLow).x,  LPVBlue_3.Sample(samLinear, lookupCoordsLow).x)
            + zHw*float4(LPVBlue_0.Sample(samLinear, lookupCoordsHigh).x,  LPVBlue_1.Sample(samLinear, lookupCoordsHigh).x,  LPVBlue_2.Sample(samLinear, lookupCoordsHigh).x,  LPVBlue_3.Sample(samLinear, lookupCoordsHigh).x);

}


float3 lookupSHAux(float3 LPVSpacePos, int numCols, int numRows, int LPV2DWidth, int LPV2DHeight, int LPV3DWidth, int LPV3DHeight, int LPV3DDepth)
{
    float3 lookupCoords = float3(LPVSpacePos.x,LPVSpacePos.y,LPVSpacePos.z*LPV3DDepth) + float3(0.5f/LPV3DWidth,0.5f/LPV3DHeight, 0.5f);
    return lookupCoords;
}

void lookupSHSamples(float3 LPVSpacePos, float3 UnsnappedLPVSpacePos, inout float4 SHred, inout float4 SHgreen, inout float4 SHblue, inout float inside,
Texture2DArray LPVRed, Texture2DArray LPVGreen, Texture2DArray LPVBlue, 
int numCols, int numRows, int LPV2DWidth, int LPV2DHeight, int LPV3DWidth, int LPV3DHeight, int LPV3DDepth) 
{
    
    float blendRegion = 0.3f;
    float insideX = min( min(blendRegion, UnsnappedLPVSpacePos.x), min(blendRegion, 1.0f - UnsnappedLPVSpacePos.x));
    float insideY = min( min(blendRegion, UnsnappedLPVSpacePos.y), min(blendRegion, 1.0f - UnsnappedLPVSpacePos.y));
    float insideZ = min( min(blendRegion, UnsnappedLPVSpacePos.z), min(blendRegion, 1.0f - UnsnappedLPVSpacePos.z));
    inside = min(insideX, (insideY,insideZ));
    inside /= blendRegion;
    if(inside<=0)
    { 
        inside = 0;
        return;
    }
    if(inside>1) inside = 1;
    

    float3 lookupCoords = lookupSHAux(LPVSpacePos, numCols, numRows, LPV2DWidth, LPV2DHeight, LPV3DWidth, LPV3DHeight, LPV3DDepth);

    trilinearSample(SHred, SHgreen, SHblue, lookupCoords, LPVRed, LPVGreen, LPVBlue, LPV3DDepth);

}

void lookupSHSamples(float3 LPVSpacePos, float3 UnsnappedLPVSpacePos, inout float4 SHred, inout float4 SHgreen, inout float4 SHblue, inout float inside, 
Texture2DArray LPVRed_0,   Texture2DArray LPVRed_1,   Texture2DArray LPVRed_2,   Texture2DArray LPVRed_3,
Texture2DArray LPVGreen_0, Texture2DArray LPVGreen_1, Texture2DArray LPVGreen_2, Texture2DArray LPVGreen_3,
Texture2DArray LPVBlue_0,  Texture2DArray LPVBlue_1,  Texture2DArray LPVBlue_2,  Texture2DArray LPVBlue_3, 
int numCols, int numRows, int LPV2DWidth, int LPV2DHeight, int LPV3DWidth, int LPV3DHeight, int LPV3DDepth) 
{

    float blendRegion = 0.3f;
    float insideX = min( min(blendRegion, UnsnappedLPVSpacePos.x), min(blendRegion, 1.0f - UnsnappedLPVSpacePos.x));
    float insideY = min( min(blendRegion, UnsnappedLPVSpacePos.y), min(blendRegion, 1.0f - UnsnappedLPVSpacePos.y));
    float insideZ = min( min(blendRegion, UnsnappedLPVSpacePos.z), min(blendRegion, 1.0f - UnsnappedLPVSpacePos.z));
    inside = min(insideX, (insideY,insideZ));
    inside /= blendRegion;
    if(inside<=0)
    { 
        inside = 0;
        return;
    }
    if(inside>1) inside = 1; 

    float3 lookupCoords = lookupSHAux(LPVSpacePos, numCols, numRows, LPV2DWidth, LPV2DHeight, LPV3DWidth, LPV3DHeight, LPV3DDepth);

    trilinearSample(SHred, SHgreen, SHblue, lookupCoords,
                   LPVRed_0, LPVRed_1, LPVRed_2, LPVRed_3,
                   LPVGreen_0,  LPVGreen_1,  LPVGreen_2,  LPVGreen_3,
                   LPVBlue_0,   LPVBlue_1,   LPVBlue_2,   LPVBlue_3, LPV3DDepth);
}

float3 loadNormal(VS_OUTPUT f)
{    
    float3 Normal = normalize(f.Normal);
    float3 Tn = normalize(f.tangent);
    float3 Bn = normalize(f.binorm);
    float4 bumps = g_txNormalMap.Sample(samAniso, f.TextureUV);    
    bumps = float4(2*bumps.xyz - float3(1,1,1),0);
    float3x3 TangentToWorld = float3x3(Tn,Bn,Normal);
    float3 Nb = lerp(Normal,mul(bumps.xyz,TangentToWorld),g_NormalmapMultiplier);
    Nb = normalize(Nb);
    return Nb;
}

float4 SHCNormalize(in float4 res)
{
    // extract direction
    float l = dot(res.gba, res.gba);
    res.gba /= max(0.05f, sqrt(l));
    res.r = 1.0;
    return res;
}

float4 PS( VS_OUTPUT f ) : SV_Target
{
    float3 LightDir = g_lightWorldPos.xyz - f.worldPos.xyz;
    float LightDistSq = dot(LightDir, LightDir);
    LightDir = normalize(LightDir);
    float diffuse = 0.0f;

    //load the albedo
    float3 albedo = float3(1,1,1);
    if(g_useTexture)
        albedo = g_txDiffuse.Sample( samAniso, f.TextureUV ).rgb;

    //load the alphamask
    float alpha = 1.0f;
    if (g_useAlpha)
        alpha = g_txAlpha.Sample( samAniso, f.TextureUV ).r; 
    
    //load and use the normal map
    float3 Normal = loadNormal(f);

    //compute the direct lighting
    diffuse = max( dot( LightDir, Normal ), 0);
    diffuse *= g_directLight * saturate(1.f - LightDistSq * g_lightWorldPos.w);

    //compute the shadows
    float shadow = 1.0f;
    if(g_bUseSM)
    {
        float2 uv = f.LightPos.xy / f.LightPos.w;
        float z = f.LightPos.z / f.LightPos.w;    
        shadow = lookupShadow(uv,z);
    }

    //compute the amount of diffuse inter-reflected light
    float3 diffuseGI = float3(0,0,0);
    if(g_useDiffuseInterreflection)
    {
        //get the radiance distribution by trilinearly interpolating the SH coefficients
        //evaluate the irradiance by convolving the obtained radiant distribution with the cosine lobe of the surface's normal (from the presentation at I3D)

        float4 surfaceNormalLobe;
        clampledCosineCoeff(float3(Normal.x,Normal.y,Normal.z), surfaceNormalLobe);

        float inside = 1;
        float4 SHred   = float4(0,0,0,0);
        float4 SHgreen = float4(0,0,0,0);
        float4 SHblue  = float4(0,0,0,0);

        lookupSHSamples(f.LPVSpacePos, f.LPVSpaceUnsnappedPos, SHred, SHgreen, SHblue, inside, g_propagatedLPVRed,g_propagatedLPVGreen, g_propagatedLPVBlue, 
                        g_numCols1, g_numRows1, LPV2DWidth1, LPV2DHeight1, LPV3DWidth1, LPV3DHeight1, LPV3DDepth1 );


        if(g_useDirectionalDerivativeClamping)
        {
            float3 gridSpaceNormal = normalize( mul( f.Normal, (float3x3)g_WorldToLPVSpace ));    
            float3 offsetPosGrid = f.LPVSpacePos + gridSpaceNormal*0.2f / 32.f;//float3(LPV3DWidth1, LPV3DHeight1, LPV3DDepth1); // grid size

            float4 SHredDir    = float4(0,0,0,0);
            float4 SHgreenDir  = float4(0,0,0,0);
            float4 SHblueDir   = float4(0,0,0,0);
            float dirInside = 1;
            lookupSHSamples(offsetPosGrid, offsetPosGrid, SHredDir, SHgreenDir, SHblueDir, dirInside, g_propagatedLPVRed,g_propagatedLPVGreen, g_propagatedLPVBlue, 
                            g_numCols1, g_numRows1, LPV2DWidth1, LPV2DHeight1, LPV3DWidth1, LPV3DHeight1, LPV3DDepth1 );

            float4 cGradRed = SHSub(SHredDir, SHred);
            float4 cGradGreen = SHSub(SHgreenDir, SHgreen);
            float4 cGradBlue = SHSub(SHblueDir, SHblue);

            //based on deviation between SH Coefficients at point and derivative in normal direction
            float3 vAtten = float3( saturate(innerProductSH(SHCNormalize(SHred), SHCNormalize(cGradRed))),
                                    saturate(innerProductSH(SHCNormalize(SHgreen), SHCNormalize(cGradGreen))),
                                    saturate(innerProductSH(SHCNormalize(SHblue), SHCNormalize(cGradBlue)))
                                  );
            float sAtten = max(vAtten.r, max(vAtten.g, vAtten.b));
            sAtten = pow(sAtten, g_directionalDampingAmount);
            

            //TODO // if the offset sample is not inside (or is only partially inside), given by dirInside, then dont damp so much
            SHMul(SHred, sAtten);
            SHMul(SHgreen, sAtten);
            SHMul(SHblue, sAtten);
        }
    
        diffuseGI.r = innerProductSH( SHred, surfaceNormalLobe ) * inside;
        diffuseGI.g = innerProductSH( SHgreen, surfaceNormalLobe ) * inside;
        diffuseGI.b = innerProductSH( SHblue, surfaceNormalLobe ) * inside;

        if(inside<1 && g_minCascadeMethod && g_numCascadeLevels>1)
        {
            float inside2 = 1;
            
            lookupSHSamples(f.LPVSpacePos2, f.LPVSpaceUnsnappedPos2, SHred, SHgreen, SHblue, inside2, g_propagatedLPV2Red,g_propagatedLPV2Green, g_propagatedLPV2Blue, 
                           g_numCols2, g_numRows2, LPV2DWidth2, LPV2DHeight2, LPV3DWidth2, LPV3DHeight2, LPV3DDepth2 );        

            diffuseGI.r += (1.0f - inside)*innerProductSH( SHred, surfaceNormalLobe ) * inside2;
            diffuseGI.g += (1.0f - inside)*innerProductSH( SHgreen, surfaceNormalLobe ) * inside2;
            diffuseGI.b += (1.0f - inside)*innerProductSH( SHblue, surfaceNormalLobe ) * inside2;

        }

        diffuseGI *= float4(g_diffuseScale/PI,g_diffuseScale/PI,g_diffuseScale/PI,1);
    }

    return float4(shadow.xxx *diffuse*albedo*0.8 + diffuseGI.xyz*albedo + g_ambientLight*albedo, alpha);
}


float4 PS_separateFloatTextures( VS_OUTPUT f ) : SV_Target
{
    float3 LightDir = g_lightWorldPos.xyz - f.worldPos.xyz;
    float LightDistSq = dot(LightDir, LightDir);
    LightDir = normalize(LightDir);
    float diffuse = 0.0f;

    //load the albedo
    float3 albedo = float3(1,1,1);
    if(g_useTexture)
        albedo = g_txDiffuse.Sample( samAniso, f.TextureUV ).rgb;

    //load the alphamask
    float alpha = 1.0f;
    if (g_useAlpha)
        alpha = g_txAlpha.Sample( samAniso, f.TextureUV ).r; 
    
    //load and use the normal map
    float3 Normal = loadNormal(f);

    //compute the direct lighting
    diffuse = max( dot( LightDir, Normal ), 0);
    diffuse *= g_directLight * saturate(1.f - LightDistSq * g_lightWorldPos.w);

    //compute the shadows
    float shadow = 1.0f;
    if(g_bUseSM)
    {
        float2 uv = f.LightPos.xy / f.LightPos.w;
        float z = f.LightPos.z / f.LightPos.w;    
        shadow = lookupShadow(uv,z);
    }

    //compute the amount of diffuse inter-reflected light
    float3 diffuseGI = float3(0,0,0);
    if(g_useDiffuseInterreflection)
    {
        //get the radiance distribution by trilinearly interpolating the SH coefficients
        //evaluate the irradiance by convolving the obtained radiant distribution with the cosine lobe of the surface's normal (from the presentation at I3D)

        float4 surfaceNormalLobe;
        clampledCosineCoeff(float3(Normal.x,Normal.y,Normal.z), surfaceNormalLobe);

        float inside = 1;
        float4 SHred   = float4(0,0,0,0);
        float4 SHgreen = float4(0,0,0,0);
        float4 SHblue  = float4(0,0,0,0);
        
        lookupSHSamples(f.LPVSpacePos, f.LPVSpaceUnsnappedPos, SHred, SHgreen, SHblue, inside, 
                        g_propagatedLPVRed,   g_propagatedLPVRed_1,   g_propagatedLPVRed_2,   g_propagatedLPVRed_3,
                        g_propagatedLPVGreen, g_propagatedLPVGreen_1, g_propagatedLPVGreen_2, g_propagatedLPVGreen_3,
                        g_propagatedLPVBlue,  g_propagatedLPVBlue_1,  g_propagatedLPVBlue_2,  g_propagatedLPVBlue_3, 
                        g_numCols1, g_numRows1, LPV2DWidth1, LPV2DHeight1, LPV3DWidth1, LPV3DHeight1, LPV3DDepth1 );
        
        diffuseGI.r = innerProductSH( SHred, surfaceNormalLobe ) * inside;
        diffuseGI.g = innerProductSH( SHgreen, surfaceNormalLobe ) * inside;
        diffuseGI.b = innerProductSH( SHblue, surfaceNormalLobe ) * inside;

        if(inside<1 && g_minCascadeMethod && g_numCascadeLevels>1)
        {
            float inside2 = 1;
            
            lookupSHSamples(f.LPVSpacePos2, f.LPVSpaceUnsnappedPos, SHred, SHgreen, SHblue, inside2, 
                        g_propagatedLPV2Red, g_propagatedLPV2Red_1, g_propagatedLPV2Red_2, g_propagatedLPV2Red_3,
                        g_propagatedLPV2Green, g_propagatedLPV2Green_1, g_propagatedLPV2Green_2, g_propagatedLPV2Green_3, 
                        g_propagatedLPV2Blue, g_propagatedLPV2Blue_1, g_propagatedLPV2Blue_2, g_propagatedLPV2Blue_3, 
                        g_numCols2, g_numRows2, LPV2DWidth2, LPV2DHeight2, LPV3DWidth2, LPV3DHeight2, LPV3DDepth2 );

            diffuseGI.r += (1.0f - inside)*innerProductSH( SHred, surfaceNormalLobe ) * inside2;
            diffuseGI.g += (1.0f - inside)*innerProductSH( SHgreen, surfaceNormalLobe ) * inside2;
            diffuseGI.b += (1.0f - inside)*innerProductSH( SHblue, surfaceNormalLobe ) * inside2;

        }                

        diffuseGI *= float4(g_diffuseScale/PI,g_diffuseScale/PI,g_diffuseScale/PI,1);
    }    

    return float4(shadow.xxx *diffuse*albedo*0.8 + diffuseGI.xyz*albedo + g_ambientLight*albedo, alpha);
}


VS_OUTPUT_SM VS_ShadowMap( VS_INPUT input )
{
    VS_OUTPUT_SM f;
    f.Pos = mul( float4( input.Pos, 1 ), g_WorldViewProj ); 
    return f;
}



VS_OUTPUT_RSM VS_RSM( VS_INPUT input )
{
    VS_OUTPUT_RSM f;
    f.Pos = mul( float4( input.Pos, 1 ), g_WorldViewProj ); 
    f.Normal = normalize( mul( input.Normal, (float3x3)g_World ) );
    f.TextureUV = input.TextureUV; 
    f.worldPos = mul( float4( input.Pos, 1 ), g_World ); 

    return f;
}

//output color and light space depth to the color MRT (0)
//output world space normal to the normal MRT (1)
PS_MRTOutput PS_RSM( VS_OUTPUT_RSM f ) : SV_Target
{
    PS_MRTOutput output;
    
    float3 LightDir = g_lightWorldPos.xyz - f.worldPos.xyz;
    float LightDistSq = dot(LightDir, LightDir);
    LightDir = normalize(LightDir);
    float diffuse = max( dot( LightDir, normalize(f.Normal) ), 0) * saturate(1.f - LightDistSq * g_lightWorldPos.w);

    float3 albedo = float3(1,1,1);
    if(g_useTexture)
        albedo = g_txDiffuse.Sample( samAniso, f.TextureUV ).rgb;

    output.Color = float4(diffuse * albedo, 1);
    output.Albedo = float4(albedo,1);

    float3 encodedNormal = f.Normal*float3(0.5f,0.5f,0.5f) + float3(0.5f,0.5f,0.5f);
    output.Normal = float4(encodedNormal,1.0f);

    return output;
}


VS_OUTPUT_RSM_DP VS_RSM_DepthPeeling( VS_INPUT input )
{
    VS_OUTPUT_RSM_DP f;
    f.Pos = mul( float4( input.Pos, 1 ), g_WorldViewProj ); 
    f.Normal = normalize( mul( input.Normal, (float3x3)g_World ) );
    f.TextureUV = input.TextureUV; 
    f.worldPos = mul( float4( input.Pos, 1 ), g_World ); 
    f.depthMapuv = mul( float4( input.Pos, 1 ), g_WorldViewProjClip2Tex);

    // screenspace coordinates for the lookup into the depth buffer
    f.ScreenTex = f.Pos.xy/f.Pos.w;
    // output depth of this particle
    f.Depth = f.Pos.zw;

    return f;
}


//output color and light space depth to the color MRT (0)
//output world space normal to the normal MRT (1)
PS_MRTOutput PS_RSM_DepthPeeling( VS_OUTPUT_RSM_DP f ) : SV_Target
{
    PS_MRTOutput output;
    
    float epsilon = 0.0f;

    float2 uv = f.depthMapuv.xy / f.depthMapuv.w;
    float val = g_txPrevDepthBuffer.SampleLevel(DepthSampler, uv, 0);
    
    float currentZValue = f.Pos.z;
     if( currentZValue <= (val+epsilon) )        
        discard;

    float3 LightDir = g_lightWorldPos.xyz - f.worldPos.xyz;
    float LightDistSq = dot(LightDir, LightDir);
    LightDir = normalize(LightDir);
    float diffuse = max( dot( LightDir, normalize(f.Normal) ), 0) * saturate(1.f - LightDistSq * g_lightWorldPos.w);

    float3 albedo = float3(1,1,1);
    if(g_useTexture)
        albedo = g_txDiffuse.Sample( samAniso, f.TextureUV ).rgb;

    output.Color = float4(diffuse * albedo, 1.0f );

    output.Albedo = float4(albedo,1);

    float3 encodedNormal = f.Normal*float3(0.5f,0.5f,0.5f) + float3(0.5f,0.5f,0.5f);
    output.Normal = float4(encodedNormal,1.0f);
    

    return output;
}



struct VS_OUTPUT_SMALL { 
    float4 Pos : SV_POSITION;
};

VS_OUTPUT_SMALL VS_Simple( VS_INPUT input )
{
    VS_OUTPUT_SMALL f;
    float3 Pos = input.Pos;
    f.Pos = mul( float4( Pos, 1 ), g_WorldViewProjSimple ); 
    return f;
}

float4 PS_Simple(VS_OUTPUT_SMALL f) : SV_Target
{
    return g_color;
}



struct VS_VIZ_LPV { 
    float4 Pos : SV_POSITION;
    float3 LPVSpacePos : TEXCOORD1;
};

VS_VIZ_LPV VS_VizLPV( VS_INPUT input )
{
    VS_VIZ_LPV f;
    float3 Pos = input.Pos;
    Pos *= g_sphereScale;
    Pos += (g_LPVSpacePos-float3(0.5,0.5,0.5));
    f.Pos = mul( float4( Pos, 1 ), g_WorldViewProjSimple ); 
    f.LPVSpacePos = g_LPVSpacePos;
    return f;
}

float4 PS_VizLPV(VS_VIZ_LPV f) : SV_Target
{
    int row, col;
    bool outside;

    float4 color = g_LPVTex.Sample(samLinear, float3(f.LPVSpacePos.x,f.LPVSpacePos.y,f.LPVSpacePos.z*LPV3DDepth1) + float3(0.5f/LPV3DWidth1,0.5f/LPV3DHeight1, 0.5f));

    clip( abs(color.r) + abs(color.g) + abs(color.b) + abs(color.a) - 0.001); //kill visualizing this point if there is no radiance here
    return float4(color.r,color.g,color.b,1);

}