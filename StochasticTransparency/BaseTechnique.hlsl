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

#pragma pack_matrix(row_major)

// Declare constant buffer at buffer slot 0
cbuffer GlobalConstants : register(b0)
{
    // float4 aligned
    float4x4 g_worldViewProj;
    float4x4 g_worldViewIT;
    // float4 aligned
    uint g_randMaskSizePowOf2MinusOne;
    uint g_randMaskAlphaValues;
    uint g_randomOffset;
    uint g_pad;
};

// Declare constant buffer at buffer slot 1
cbuffer ShadingConstants : register(b1)
{
    float4 g_color;
}

//--------------------------------------------------------------------------------------
// Geometry rendering
//--------------------------------------------------------------------------------------

struct Geometry_VSIn
{
    float4 position : position;
    float3 normal   : normal;
};

// Use centroid interpolation for the normal to avoid any shading artifacts with MSAA
struct Geometry_VSOut
{
    float4 HPosition            : SV_Position;
    centroid float3 Normal      : TexCoord;
};

Geometry_VSOut GeometryVS ( Geometry_VSIn IN )
{
    Geometry_VSOut OUT;
    OUT.HPosition = mul(IN.position, g_worldViewProj);
    OUT.Normal = normalize(mul(IN.normal, (float3x3)g_worldViewIT).xyz);
    return OUT;
}

float4 ShadeFragment( float3 Normal )
{
    return float4(g_color.rgb * abs(Normal.z), g_color.a);
}

//--------------------------------------------------------------------------------------
// Full-screen rendering
//--------------------------------------------------------------------------------------

struct FullscreenVSOut
{
    float4 pos : SV_Position;
    float2 tex : TEXCOORD;
};

// Vertex shader that generates a full screen quad with texcoords from vertexIDs
// To use, draw 3 vertices with primitive type triangle list
FullscreenVSOut FullScreenTriangleVS( uint id : SV_VertexID )
{
    FullscreenVSOut output = (FullscreenVSOut)0.0f;
    output.tex = float2( (id << 1) & 2, id & 2 );
    output.pos = float4( output.tex * float2( 2.0f, -2.0f ) + float2( -1.0f, 1.0f), 0.0f, 1.0f );
    return output;
}
