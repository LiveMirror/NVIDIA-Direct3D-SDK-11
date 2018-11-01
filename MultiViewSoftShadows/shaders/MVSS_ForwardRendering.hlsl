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

#include "MVSS_Common.hlsl"

//--------------------------------------------------------------------------------------
// Shading-pass vertex shader
//--------------------------------------------------------------------------------------

struct Geometry_VSIn
{
    float4 WorldPos : position;
    float3 Normal   : normal;
};

// vs_5_0 allows up to 32 output float4 registers
struct Geometry_VSOut
{
    float4 HPosition                                : SV_Position;
    float4 ViewPos                                  : viewpos;
    float4 WorldPos                                 : worldpos;
    float3 Normal                                   : normal;
    float4 LightTexCoords[SHADOW_MAP_ARRAY_SIZE]    : texcoords;
};

Geometry_VSOut EyeRenderVS(Geometry_VSIn IN)
{
    Geometry_VSOut OUT;
    OUT.HPosition = mul(IN.WorldPos, g_WorldToEyeClip);
    OUT.ViewPos = mul(IN.WorldPos, g_WorldToEyeView);
    OUT.WorldPos = IN.WorldPos;
    OUT.Normal = IN.Normal;

    // Transform the vertices to each shadow-map's texture space.
    // On Fermi, performing this in the vertex shader is ~30% faster than in the pixel shader.
    [unroll]
    for (int SliceId = 0; SliceId < SHADOW_MAP_ARRAY_SIZE; ++SliceId)
    {
        OUT.LightTexCoords[SliceId] = mul(OUT.ViewPos, g_EyeViewToLightTex[SliceId]);
    }

    return OUT;
}

//--------------------------------------------------------------------------------------
// Shading-pass pixel shaders
//--------------------------------------------------------------------------------------

float SoftShadow(Geometry_VSOut IN)
{
    float Shadow = 0;

    // Perform light-space perspective division and bilinear hardware-PCF filtering (SampleCmp)
    // Requires pixel-shader model >= 4.1 for calling SampleCmp on a Texture2DArray
    [unroll]
    for (int SliceId = 0; SliceId < SHADOW_MAP_ARRAY_SIZE; ++SliceId)
    {
        float4 TexCoord = IN.LightTexCoords[SliceId];
        TexCoord.xyz /= TexCoord.w;
        Shadow += tShadowMapArray.SampleCmp(PCF_Sampler, float3(TexCoord.xy, SliceId), TexCoord.z);
    }

    return Shadow / SHADOW_MAP_ARRAY_SIZE;
}

float4 EyeRenderGroundPS(Geometry_VSOut IN) : SV_Target0
{
    float4 Color = ShadeGround(IN.WorldPos.xyz, IN.Normal.xyz);
    return Color * SoftShadow(IN);
}

float4 EyeRenderCharacterPS(Geometry_VSOut IN) : SV_Target0
{
    float4 Color = ShadeCharacter(IN.WorldPos.xyz, IN.Normal.xyz);
    return Color * SoftShadow(IN);
}
