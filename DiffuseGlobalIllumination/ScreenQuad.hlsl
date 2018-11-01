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

Texture2D g_Diffused                        : register( t0 ); 
Texture2D g_Scene                           : register( t1 ); 
Texture2D g_Radius                          : register ( t2 );

StructuredBuffer<float> g_Buff : register( t4 );

Texture2DArray g_textureToDisplay3D         : register( t5);
Texture2DArray g_textureToDisplay3D_1       : register( t6);
Texture2DArray g_textureToDisplay3D_2       : register( t7);
Texture2DArray g_textureToDisplay3D_3       : register( t8);

Texture2D g_textureToDisplay2D              : register( t5);
Texture2D g_textureToDisplay2D_1            : register( t6);
Texture2D g_textureToDisplay2D_2            : register( t7);
Texture2D g_textureToDisplay2D_3            : register( t8);


cbuffer cbConstantsLPVinitialize : register( b0 )
{
    float g_3DWidth        : packoffset(c0.x);
    float g_3DHeight       : packoffset(c0.y);
    float g_3DDepth        : packoffset(c0.z);
    int temp               : packoffset(c0.w);
}

cbuffer cbConstantsLPVReconstruct : register( b1 )
{
    float farClip          : packoffset(c0.x);   
    float temp2            : packoffset(c0.y); 
    float temp3            : packoffset(c0.z); 
    float temp4            : packoffset(c0.w); 
}

cbuffer cbConstantsLPVReconstruct2 : register( b2 )
{
    matrix g_InverseProjection    : packoffset( c0 );
}

SamplerState samLinear 
{ 
    Filter = MIN_MAG_LINEAR_MIP_POINT; 
};

struct VS_OUTPUT
{ 
    float4 Pos : SV_POSITION;
    float2 Tex : TEXCOORD0; 
};

struct VS_OUTPUT3
{ 
    float4 Pos : SV_POSITION;
    float3 Tex : TEXCOORD0; 
};

struct VS_SCREEN_3D
{
    float3 position          : POSITION;    // 2D slice vertex coordinates in clip space
    float3 textureCoords0    : TEXCOORD;    // 3D cell coordinates (x,y,z in 0-dimension range)
};


VS_OUTPUT VS( float4 Pos : POSITION )
{
    VS_OUTPUT f;
    f.Pos = Pos;
    f.Tex = float2(0, 1) - float2(-1, 1) * (float2(Pos.x, Pos.y) * 0.5 + 0.5);
    return f;
}

VS_OUTPUT VS_POS_TEX(float4 Pos : POSITION, float2 Tex : TEXCOORD0)
{
    VS_OUTPUT f;
    f.Pos = Pos;
    f.Tex = Tex;
    return f;
}


VS_OUTPUT3 DisplayTextureVS(VS_SCREEN_3D input)
{
    VS_OUTPUT3 output;
    output.Pos = float4(input.position.x, input.position.y, input.position.z, 1.0);

    output.Tex = float3( (input.textureCoords0.x)/g_3DWidth,
                         (input.textureCoords0.y)/g_3DHeight, 
                         (input.textureCoords0.z+0.5));
    return output;
}


float4 DisplayTexturePS3D( VS_OUTPUT3 f ) : SV_Target
{
    float4 col = saturate(g_textureToDisplay3D.Sample( samLinear, float3(f.Tex.xyz)) ); 
    return col;
}

float4 DisplayTexturePS3D_floatTextures( VS_OUTPUT3 f ) : SV_Target
{
    float4 col;
    col.x = saturate( g_textureToDisplay3D.Sample( samLinear, float3(f.Tex.xyz)).x );
    col.y = saturate( g_textureToDisplay3D_1.Sample( samLinear, float3(f.Tex.xyz)).x );
    col.z = saturate( g_textureToDisplay3D_2.Sample( samLinear, float3(f.Tex.xyz)).x );
    col.w = saturate( g_textureToDisplay3D_3.Sample( samLinear, float3(f.Tex.xyz)).x );
    return col;
}

float4 DisplayTexturePS2D( VS_OUTPUT f ) : SV_Target
{
    float4 col = saturate(g_textureToDisplay2D.Sample( samLinear, f.Tex.xy ));
    return col;
}


float4 DisplayTexturePS2D_floatTextures( VS_OUTPUT f ) : SV_Target
{
    float4 col;
    col.x = saturate(g_textureToDisplay2D.Sample( samLinear, f.Tex.xy ).x);
    col.y = saturate(g_textureToDisplay2D_1.Sample( samLinear, f.Tex.xy ).x);
    col.z = saturate(g_textureToDisplay2D_2.Sample( samLinear, f.Tex.xy ).x);
    col.w = saturate(g_textureToDisplay2D_3.Sample( samLinear, f.Tex.xy ).x);
    return col;
}


float4 ReconstructPosFromDepth( VS_OUTPUT f ) : SV_Target
{
    float2 inputPos = float2( f.Tex.x*2.0f-1, (1.0f-f.Tex.y)*2.0f-1);
    float depthSample = g_textureToDisplay2D.Sample( samLinear, f.Tex.xy );

    float4 vProjectedPos = float4(inputPos.x, inputPos.y, depthSample, 1.0f);
    float4 viewSpacePos = mul(vProjectedPos, g_InverseProjection);  
    viewSpacePos.xyz = viewSpacePos.xyz / viewSpacePos.w;  

    float4 col = saturate(float4(viewSpacePos.xyz,1));
    if(viewSpacePos.z >= farClip) col = float4(0,0,0,0);

    return col;
}

