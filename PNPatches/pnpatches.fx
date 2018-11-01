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

#include "pnpatches.fxh"
#include "common.fx"

// DEBUG defines
//#define MESH_CONSTANT_LOD
//#define FLAT_NORMAL
//#define IGNORE_DISPLACEMENT
//#define SMOOTH_TCOORDS

Buffer<float4>      g_PositionsBuffer;
Buffer<float2>      g_CoordinatesBuffer;
Buffer<int2>        g_IndicesBuffer;
Buffer<float4>      g_NormalsBuffer;
Buffer<float4>      g_TangentsBuffer;
Buffer<float2>      g_CornerCoordinatesBuffer;
Buffer<float4>      g_EdgeCoordinatesBuffer;

Buffer<int4>        g_BoneIndicesBuffer;
Buffer<float4>      g_BoneWeightsBuffer;

// DEBUG visualization
Buffer<float4>      g_VectorsBuffer;

Texture2D           g_WSNormalMap;
Texture2D<float>    g_DisplacementTexture;

struct PSIn_Diffuse
{
    float4 position     : SV_Position;
    float2 texCoord     : TEXCOORD0;
    float3 positionWS   : TEXCOORD1;
    float3 normal       : TEXCOORD2;
    float3 tangent      : TEXCOORD3;
};

struct PSIn_TessellatedDiffuse
{
    float4 position     : SV_Position;
    float2 texCoord     : TEXCOORD0;
    float3 positionWS   : TEXCOORD1;
    float3 normal       : TEXCOORD2;
    float3 tangent      : TEXCOORD3;
    float sign          : TEXCOROD4;
};

struct PSIn_Depth
{
    float4 position     : SV_Position;
};

struct PSIn_Quad
{
    float4 position     : SV_Position;
    float2 texCoord     : TEXCOORD0;
};

//--------------------------------------------------------------------------------------
// Constant Buffers
//--------------------------------------------------------------------------------------

shared cbuffer cb0
{
    float4x4    g_ModelViewProjectionMatrix;
    float4x4    g_ModelViewProjectionMatrixInv;
    float4x4    g_ModelMatrix;
    float4x4    g_ModelRotationMatrix;
    float4x4    g_ViewProjectionMatrix;
    float3      g_CameraPosition;
    float4      g_BackBufferDimensions;

    float       g_TessellationFactor = 1;
    float       g_DisplacementScale = 1;

    int2        g_MapDimensions;
};

shared cbuffer cb1
{
    float3 g_FrustumOrigin;
    float4 g_FrustumNormals[4];
};

shared cbuffer cb2
{
    float g_DisplacementMin;
    float g_DisplacementMax;
};

shared cbuffer cb3
{
    float4x4 mAnimationMatrixArray[128];
};

const float3x3 MAnimatedScale = { -100, 0, 0,
						          0, 100, 0,
								  0, 0, -100 };

PSIn_Diffuse RenderDiffuseVS(uint vertexID : SV_VertexID, uniform bool renderAnimated = false)
{
    PSIn_Diffuse output;
    
    int2 indices = g_IndicesBuffer.Load(vertexID);

    float3 position = g_PositionsBuffer.Load(indices.x).xyz;
    output.position = mul(float4(position, 1.0f), g_ModelViewProjectionMatrix);
    output.positionWS = position;

    float3 normal = g_NormalsBuffer.Load(indices.x).xyz;
    output.normal = normal;

    float3 tangent = g_TangentsBuffer.Load(indices.x).xyz;
    output.tangent = tangent;
    
    if (renderAnimated)
    {
        int boneIndices[4] = (int[4])g_BoneIndicesBuffer.Load(indices.x);
        float boneWeights[4] = (float[4])g_BoneWeightsBuffer.Load(indices.x);

        float4x4 combinedAnimMatrix = 0;

        [unroll]
        for (int iBone=0; iBone<4; ++iBone)
        {
            combinedAnimMatrix += mAnimationMatrixArray[boneIndices[iBone]] * boneWeights[iBone];
        }

        float3 animatedPosition = mul(float4(position, 1), combinedAnimMatrix).xyz;
        output.positionWS = mul(animatedPosition, MAnimatedScale);
        output.position = mul(float4(output.positionWS, 1), g_ModelViewProjectionMatrix);

        float3 animatedNormal = mul(float4(position + normal, 1), combinedAnimMatrix).xyz - animatedPosition;
        output.normal = normalize(mul(animatedNormal, MAnimatedScale));

        float3 animatedTangent = mul(float4(position + tangent, 1), combinedAnimMatrix).xyz - animatedPosition;
        output.tangent = normalize(mul(animatedTangent, MAnimatedScale)); 
    }

    output.texCoord = g_CoordinatesBuffer.Load(indices.x);
    
    return output;
}

struct HSIn_Diffuse
{
    float3 position     : POSITION;
    float2 texCoord     : TEXCOORD0;
    float3 normal       : TEXCOORD1;
    float3 tangent      : TEXCOORD2;
#ifdef FIX_THE_SEAMS
    float2 cornerCoord  : TEXCOORD4;
    float4 edgeCoord    : TEXCOORD5;
#endif
};

HSIn_Diffuse RenderTessellatedDiffuseVS(uint vertexID : SV_VertexID, uniform bool renderAnimated = false)
{
    HSIn_Diffuse output;
    
    int2 indices = g_IndicesBuffer.Load(vertexID);

    output.position = g_PositionsBuffer.Load(indices.x).xyz;
    output.texCoord = g_CoordinatesBuffer.Load(indices.xy);
    
    float4 normalData = g_NormalsBuffer.Load(indices.x);
    output.normal = normalData.xyz;

    float4 tangentData = g_TangentsBuffer.Load(indices.x);
    output.tangent = tangentData.xyz;

    if (renderAnimated)
    {
        int4 boneIndices = g_BoneIndicesBuffer.Load(indices.x);
        float4 boneWeights = g_BoneWeightsBuffer.Load(indices.x);

        float4x4 combinedAnimMatrix = 0;

        [unroll]
        for (int iBone=0; iBone<4; ++iBone)
        {
            combinedAnimMatrix += mAnimationMatrixArray[boneIndices[iBone]] * boneWeights[iBone];
        }

        float3 animatedPosition = mul(float4(output.position, 1), combinedAnimMatrix).xyz;
        output.position = mul(animatedPosition, MAnimatedScale);

        float3x3 rotationMatrix = (float3x3)combinedAnimMatrix;

        float3 animatedNormal = mul(float4(output.normal, 1), rotationMatrix).xyz;
        output.normal = normalize(mul(animatedNormal, MAnimatedScale));

        float3 animatedTangent = mul(float4(output.tangent, 1), rotationMatrix).xyz;
        output.tangent = normalize(mul(animatedTangent, MAnimatedScale));
    }

#ifdef FIX_THE_SEAMS
    output.cornerCoord = g_CornerCoordinatesBuffer.Load(indices.x);
    output.edgeCoord = g_EdgeCoordinatesBuffer.Load(vertexID);
#endif

    return output;
}

float4 RenderVectorsVS(uniform float scale, uint vertexID : SV_VertexID) : SV_Position
{
    HSIn_Diffuse output;
    
    int index = vertexID >> 1;
    int isOdd = vertexID & 0x1;

    float3 position = g_PositionsBuffer.Load(index).xyz;
    float4 directionScaled = g_VectorsBuffer.Load(index);
    float3 direction = directionScaled.xyz;

    //position += direction * directionScaled.w * isOdd;
    position += direction * isOdd * scale;

    return mul(float4(position, 1.0f), g_ModelViewProjectionMatrix);
}

float4 RenderVectorsAnimatedVS(uniform float scale, uint vertexID : SV_VertexID) : SV_Position
{
    HSIn_Diffuse output;
    
    int index = vertexID >> 1;
    int isOdd = vertexID & 0x1;

    float3 position = g_PositionsBuffer.Load(index).xyz;
    float3 direction = g_VectorsBuffer.Load(index).xyz;

    // Skinning
    int boneIndices[4] = (int[4])g_BoneIndicesBuffer.Load(index);
    float boneWeights[4] = (float[4])g_BoneWeightsBuffer.Load(index);
    float4x4 combinedAnimMatrix = 0;

    [unroll]
    for (int iBone=0; iBone<4; ++iBone)
    {
        combinedAnimMatrix += mAnimationMatrixArray[boneIndices[iBone]] * boneWeights[iBone];
    }

    float3 animatedPosition = mul(float4(position, 1), combinedAnimMatrix).xyz;

    float3x3 rotationMatrix = (float3x3)combinedAnimMatrix;
    float3 animatedDirection = mul(float4(direction, 1), rotationMatrix).xyz;
    animatedDirection = normalize(animatedDirection);

    animatedPosition += animatedDirection * isOdd * scale;
	animatedPosition = mul(animatedPosition, MAnimatedScale);
    
    return mul(float4(animatedPosition, 1), g_ModelViewProjectionMatrix);
}

#define PN_TRIANGLES

struct HS_CONSTANT_DATA_OUTPUT
{
    float Edges[3]  : SV_TessFactor;
    float Inside    : SV_InsideTessFactor;
#ifdef PN_TRIANGLES
    // Geometry cubic generated control points
    float3 f3B210   : POSITION3;
    float3 f3B120   : POSITION4;
    float3 f3B021   : POSITION5;
    float3 f3B012   : POSITION6;
    float3 f3B102   : POSITION7;
    float3 f3B201   : POSITION8;
    float3 f3B111   : CENTER;
#endif

    float sign      : SIGN;
};

struct HS_CONSTANT_DATA_OUTPUT_QUADS
{
    float Edges[4] : SV_TessFactor;
    float Inside[2] : SV_InsideTessFactor;
};

struct HS_CONSTANT_DATA_OUTPUT_TESS_QUADS
{
    float Edges[4] : SV_TessFactor;
    float Inside[2] : SV_InsideTessFactor;
    float3 vEdgePos[8] : EDGEPOS;
    float3 vInteriorPos[4] : INTERIORPOS;
    float sign : SIGN;
};

HS_CONSTANT_DATA_OUTPUT DiffuseConstantHS( InputPatch<HSIn_Diffuse, 3> inputPatch)
{    
	HS_CONSTANT_DATA_OUTPUT output;

	// tessellation factors are proportional to model space edge length
	for (uint ie = 0; ie < 3; ++ie)
	{
#ifdef MESH_CONSTANT_LOD
		output.Edges[ie] = g_TessellationFactor / (float)512 * (float)64;
#else
		float3 edge = inputPatch[(ie + 1) % 3].position - inputPatch[ie].position;
		float3 vec = (inputPatch[(ie + 1) % 3].position + inputPatch[ie].position) / 2 - g_FrustumOrigin;
		float len = sqrt(dot(edge, edge) / dot(vec, vec));
		output.Edges[(ie + 1) % 3] = max(1, len * g_TessellationFactor);
#endif
	}

	// culling
    int culled[4];
    for (int ip = 0; ip < 4; ++ip)
    {
        culled[ip] = 1;
        culled[ip] &= dot(inputPatch[0].position - g_FrustumOrigin, g_FrustumNormals[ip].xyz) > 0;
        culled[ip] &= dot(inputPatch[1].position - g_FrustumOrigin, g_FrustumNormals[ip].xyz) > 0;
        culled[ip] &= dot(inputPatch[2].position - g_FrustumOrigin, g_FrustumNormals[ip].xyz) > 0;
    }
    if (culled[0] || culled[1] || culled[2] || culled[3]) output.Edges[0] = 0;

#ifdef PN_TRIANGLES
    // compute the cubic geometry control points
    // edge control points
    output.f3B210 = ( ( 2.0f * inputPatch[0].position ) + inputPatch[1].position - ( dot( ( inputPatch[1].position - inputPatch[0].position ), inputPatch[0].normal ) * inputPatch[0].normal ) ) / 3.0f;
    output.f3B120 = ( ( 2.0f * inputPatch[1].position ) + inputPatch[0].position - ( dot( ( inputPatch[0].position - inputPatch[1].position ), inputPatch[1].normal ) * inputPatch[1].normal ) ) / 3.0f;
    output.f3B021 = ( ( 2.0f * inputPatch[1].position ) + inputPatch[2].position - ( dot( ( inputPatch[2].position - inputPatch[1].position ), inputPatch[1].normal ) * inputPatch[1].normal ) ) / 3.0f;
    output.f3B012 = ( ( 2.0f * inputPatch[2].position ) + inputPatch[1].position - ( dot( ( inputPatch[1].position - inputPatch[2].position ), inputPatch[2].normal ) * inputPatch[2].normal ) ) / 3.0f;
    output.f3B102 = ( ( 2.0f * inputPatch[2].position ) + inputPatch[0].position - ( dot( ( inputPatch[0].position - inputPatch[2].position ), inputPatch[2].normal ) * inputPatch[2].normal ) ) / 3.0f;
    output.f3B201 = ( ( 2.0f * inputPatch[0].position ) + inputPatch[2].position - ( dot( ( inputPatch[2].position - inputPatch[0].position ), inputPatch[0].normal ) * inputPatch[0].normal ) ) / 3.0f;
    // center control point
    float3 f3E = ( output.f3B210 + output.f3B120 + output.f3B021 + output.f3B012 + output.f3B102 + output.f3B201 ) / 6.0f;
    float3 f3V = ( inputPatch[0].position + inputPatch[1].position + inputPatch[2].position ) / 3.0f;
    output.f3B111 = f3E + ( ( f3E - f3V ) / 2.0f );
#endif

	output.Inside = (output.Edges[0] + output.Edges[1] + output.Edges[2]) / 3;

    float2 t01 = inputPatch[1].texCoord - inputPatch[0].texCoord;
    float2 t02 = inputPatch[2].texCoord - inputPatch[0].texCoord;
    output.sign = t01.x * t02.y - t01.y * t02.x > 0.0f ? 1 : -1;

    return output;
}

HS_CONSTANT_DATA_OUTPUT_QUADS DiffuseConstantQuadsHS( InputPatch<HSIn_Diffuse, 4> inputPatch )
{    
    HS_CONSTANT_DATA_OUTPUT_QUADS output;
    output.Edges[0] = output.Edges[1] = output.Edges[2] = output.Edges[3] = g_TessellationFactor;
    output.Inside[0] = output.Inside[1] = g_TessellationFactor;
    return output;
}

HS_CONSTANT_DATA_OUTPUT_TESS_QUADS DiffuseConstantTessQuadsHS(InputPatch<HSIn_Diffuse, 4> inputPatch															  )
{    
    HS_CONSTANT_DATA_OUTPUT_TESS_QUADS output = (HS_CONSTANT_DATA_OUTPUT_TESS_QUADS)0;

	// tessellation factors are proportional to model space edge length
	for (int ie = 0; ie < 4; ++ie)
	{
#ifdef MESH_CONSTANT_LOD
		output.Edges[ie] = g_TessellationFactor / (float)512 * (float)64;
#else
		float3 edge = inputPatch[(ie + 1) & 3].position - inputPatch[ie].position;
		float3 vec = (inputPatch[(ie + 1) & 3].position + inputPatch[ie].position) / 2 - g_FrustumOrigin;
		float len = sqrt(dot(edge, edge) / dot(vec, vec));
		output.Edges[(ie + 1) & 3] = max(1, len * g_TessellationFactor);
#endif
	}
	output.Inside[1] = (output.Edges[0] + output.Edges[2]) / 2;
	output.Inside[0] = (output.Edges[1] + output.Edges[3]) / 2;

    // culling
    for (int ip = 0; ip < 4; ++ip)
    {
		int culled = 1;
        culled &= dot(inputPatch[0].position - g_FrustumOrigin, g_FrustumNormals[ip].xyz) > 0;
        culled &= dot(inputPatch[1].position - g_FrustumOrigin, g_FrustumNormals[ip].xyz) > 0;
        culled &= dot(inputPatch[2].position - g_FrustumOrigin, g_FrustumNormals[ip].xyz) > 0;
        culled &= dot(inputPatch[3].position - g_FrustumOrigin, g_FrustumNormals[ip].xyz) > 0;
		if (culled) output.Edges[ip] = 0;
    }

	// edges
    [unroll]
    for (int iedge = 0; iedge < 4; ++iedge)
    {
        int i = iedge;
        int j = (iedge + 1) & 3;
        float3 vPosTmp = 0;

        vPosTmp = inputPatch[j].position - inputPatch[i].position;
        output.vEdgePos[    iedge * 2] = (2 * inputPatch[i].position + inputPatch[j].position - dot(vPosTmp, inputPatch[i].normal) * inputPatch[i].normal) / 3;

        i = j;
        j = iedge;
        vPosTmp = inputPatch[j].position - inputPatch[i].position;
        output.vEdgePos[iedge * 2 + 1] = (2 * inputPatch[i].position + inputPatch[j].position - dot(vPosTmp, inputPatch[i].normal) * inputPatch[i].normal) / 3;
    }

    // interior
    float3 q = output.vEdgePos[0];
    for (int i = 1; i < 8; ++i)
    {
        q += output.vEdgePos[i];
    }
    float3 center = inputPatch[0].position + inputPatch[1].position + inputPatch[2].position + inputPatch[3].position;

    for (i = 0; i < 4; ++i)
    {
        float3 Ei = (2 * (output.vEdgePos[i * 2] + output.vEdgePos[((i + 3) & 3) * 2 + 1] + q) - (output.vEdgePos[((i + 1) & 3) * 2 + 1] + output.vEdgePos[((i + 2) & 3) * 2])) / 18;
        float3 Vi = (center + 2 * (inputPatch[(i + 3) & 3].position + inputPatch[(i + 1) & 3].position) + inputPatch[(i + 2) & 3].position) / 9;
        output.vInteriorPos[i] = 3./2 * Ei - 1./2 * Vi;
    }

    float2 t01 = inputPatch[1].texCoord - inputPatch[0].texCoord;
    float2 t02 = inputPatch[2].texCoord - inputPatch[0].texCoord;
    output.sign = t01.x * t02.y - t01.y * t02.x > 0.0f ? 1 : -1;

    return output;
}

[domain("tri")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(3)]
[patchconstantfunc("DiffuseConstantHS")]
[maxtessfactor(64.0)]
HSIn_Diffuse DiffuseHS( InputPatch<HSIn_Diffuse, 3> inputPatch, uint i : SV_OutputControlPointID)
{
    return inputPatch[i];
}

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("DiffuseConstantQuadsHS")]
[maxtessfactor(64.0)]
HSIn_Diffuse DiffuseQuadsHS( InputPatch<HSIn_Diffuse, 4> inputPatch, uint i : SV_OutputControlPointID)
{
    return inputPatch[i];
}

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("DiffuseConstantTessQuadsHS")]
[maxtessfactor(64.0)]
HSIn_Diffuse DiffuseTessQuadsHS( InputPatch<HSIn_Diffuse, 4> inputPatch,
                        uint i : SV_OutputControlPointID)
{
    return inputPatch[i];
}

[domain("tri")]
PSIn_TessellatedDiffuse DiffuseDS( HS_CONSTANT_DATA_OUTPUT input, 
                       float3 barycentricCoords : SV_DomainLocation,
                       OutputPatch<HSIn_Diffuse, 3> inputPatch )
{
    PSIn_TessellatedDiffuse output;

    float3 coordinates = barycentricCoords;

    // The barycentric coordinates
    float fU = barycentricCoords.x;
    float fV = barycentricCoords.y;
    float fW = barycentricCoords.z;

    // Precompute squares and squares * 3 
    float fUU = fU * fU;
    float fVV = fV * fV;
    float fWW = fW * fW;
    float fUU3 = fUU * 3.0f;
    float fVV3 = fVV * 3.0f;
    float fWW3 = fWW * 3.0f;

    // Compute position from cubic control points and barycentric coords
    float3 position = inputPatch[0].position * fWW * fW + inputPatch[1].position * fUU * fU + inputPatch[2].position * fVV * fV +
					  input.f3B210 * fWW3 * fU + input.f3B120 * fW * fUU3 + input.f3B201 * fWW3 * fV + input.f3B021 * fUU3 * fV +
					  input.f3B102 * fW * fVV3 + input.f3B012 * fU * fVV3 + input.f3B111 * 6.0f * fW * fU * fV;

    // Compute normal from quadratic control points and barycentric coords
    float3 normal = inputPatch[0].normal * coordinates.z + inputPatch[1].normal * coordinates.x + inputPatch[2].normal * coordinates.y;
    normal = normalize(normal);

    float2 texCoord = inputPatch[0].texCoord * coordinates.z + inputPatch[1].texCoord * coordinates.x + inputPatch[2].texCoord * coordinates.y;

    float2 displacementTexCoord = texCoord;

#ifdef FIX_THE_SEAMS
    // Edge point
    if(coordinates.z == 0)
        displacementTexCoord = lerp(inputPatch[1].edgeCoord.xy, inputPatch[1].edgeCoord.zw, coordinates.y);
    else if(coordinates.x == 0)
        displacementTexCoord = lerp(inputPatch[2].edgeCoord.xy, inputPatch[2].edgeCoord.zw, coordinates.z);
    else if(coordinates.y == 0)
        displacementTexCoord = lerp(inputPatch[0].edgeCoord.xy, inputPatch[0].edgeCoord.zw, coordinates.x);

    // Corner point
    if(coordinates.z == 1)
        displacementTexCoord = inputPatch[0].cornerCoord;
    else if(coordinates.x == 1)
        displacementTexCoord = inputPatch[1].cornerCoord;
    else if(coordinates.y == 1)
        displacementTexCoord = inputPatch[2].cornerCoord;
#endif

#ifndef IGNORE_DISPLACEMENT
    float offset = g_DisplacementTexture.SampleLevel(SamplerLinearClamp, displacementTexCoord, 0).x;
    position += normal * offset;
#endif

    float3 tangent = inputPatch[0].tangent * coordinates.z + inputPatch[1].tangent * coordinates.x + inputPatch[2].tangent * coordinates.y;
    tangent = normalize(tangent);

    output.position = mul(float4(position, 1.0f), g_ModelViewProjectionMatrix);
#ifdef SMOOTH_TCOORDS
    output.texCoord = displacementTexCoord;
#else
    output.texCoord = texCoord;
#endif
    output.positionWS = position;
    output.normal = normal;
    output.tangent = tangent;
    
    output.sign = input.sign;//inputPatch[0].sign;

    return output;
}

[domain("quad")]
PSIn_Diffuse DiffuseQuadsDS(HS_CONSTANT_DATA_OUTPUT_QUADS input, 
                            float2 uv : SV_DomainLocation,
							OutputPatch<HSIn_Diffuse, 4> inputPatch)
{
    PSIn_Diffuse output;

    output.positionWS = uv.y * (inputPatch[0].position * uv.x + inputPatch[1].position * (1 - uv.x)) +
        (1 - uv.y) * (inputPatch[3].position * uv.x + inputPatch[2].position * (1 - uv.x));
    output.normal = uv.y * (inputPatch[0].normal * uv.x + inputPatch[1].normal * (1 - uv.x)) +
        (1 - uv.y) * (inputPatch[3].normal * uv.x + inputPatch[2].normal * (1 - uv.x));
    output.tangent = uv.y * (inputPatch[0].tangent * uv.x + inputPatch[1].tangent * (1 - uv.x)) +
        (1 - uv.y) * (inputPatch[3].tangent * uv.x + inputPatch[2].tangent * (1 - uv.x));
    output.texCoord = uv.y * (inputPatch[0].texCoord * uv.x + inputPatch[1].texCoord * (1 - uv.x)) +
        (1 - uv.y) * (inputPatch[3].texCoord * uv.x + inputPatch[2].texCoord * (1 - uv.x));
    output.position = mul(float4(output.positionWS, 1.0f), g_ModelViewProjectionMatrix);

    return output;
}

[domain("quad")]
PSIn_TessellatedDiffuse DiffuseTessQuadsDS(HS_CONSTANT_DATA_OUTPUT_TESS_QUADS input, 
                            float2 uv : SV_DomainLocation,
                            OutputPatch<HSIn_Diffuse, 4> inputPatch )
{
    PSIn_TessellatedDiffuse output;

    float4 BasisU = BernsteinBasis( uv.x );
    float4 BasisV = BernsteinBasis( uv.y );
    output.positionWS = EvaluateBezier(inputPatch[0].position, input.vEdgePos[0], input.vEdgePos[1], inputPatch[1].position,
                                       input.vEdgePos[7], input.vInteriorPos[0], input.vInteriorPos[1], input.vEdgePos[2],
                                       input.vEdgePos[6], input.vInteriorPos[3], input.vInteriorPos[2], input.vEdgePos[3],
                                       inputPatch[3].position, input.vEdgePos[5], input.vEdgePos[4], inputPatch[2].position,
                                       BasisU, BasisV);
    output.normal = (1 - uv.y) * (inputPatch[0].normal * (1 - uv.x) + inputPatch[1].normal * uv.x) +
                    uv.y * (inputPatch[3].normal * (1 - uv.x) + inputPatch[2].normal * uv.x);
    output.normal = normalize(output.normal);

    output.texCoord = (1 - uv.y) * (inputPatch[0].texCoord * (1 - uv.x) + inputPatch[1].texCoord * uv.x) +
                      uv.y * (inputPatch[3].texCoord * (1 - uv.x) + inputPatch[2].texCoord * uv.x);
    
    float2 displacementTexCoord = output.texCoord;

#ifdef FIX_THE_SEAMS
    if(uv.x == 0)
        displacementTexCoord = lerp(inputPatch[3].edgeCoord.zw, inputPatch[3].edgeCoord.xy, uv.y);
    else if(uv.x == 1)
        displacementTexCoord = lerp(inputPatch[1].edgeCoord.xy, inputPatch[1].edgeCoord.zw, uv.y);

    if(uv.y == 0)
        displacementTexCoord = lerp(inputPatch[0].edgeCoord.xy, inputPatch[0].edgeCoord.zw, uv.x);
    else if(uv.y == 1)
        displacementTexCoord = lerp(inputPatch[2].edgeCoord.zw, inputPatch[2].edgeCoord.xy, uv.x);

    if ((uv.x==0 && uv.y==0) || (uv.x==0 && uv.y==1) || (uv.x==1 && uv.y==1) || (uv.x==1 && uv.y==0))
    {
        displacementTexCoord = (1 - uv.y) * (inputPatch[0].cornerCoord * (1 - uv.x) + inputPatch[1].cornerCoord * uv.x) +
                               uv.y * (inputPatch[3].cornerCoord * (1 - uv.x) + inputPatch[2].cornerCoord * uv.x);
    }
#endif

#ifndef IGNORE_DISPLACEMENT
    float offset = g_DisplacementTexture.SampleLevel(SamplerLinearClamp, displacementTexCoord, 0.0f).x;
    output.positionWS += output.normal * offset;
#endif

    float3 tangent =    (1 - uv.y) * (inputPatch[0].tangent * (1 - uv.x) + inputPatch[1].tangent * uv.x) +
                        uv.y * (inputPatch[3].tangent * (1 - uv.x) + inputPatch[2].tangent * uv.x);

#ifdef SMOOTH_TCOORDS
    output.texCoord = displacementTexCoord;
#endif

    output.tangent = tangent;
    output.sign = input.sign;//inputPatch[0].sign;
    output.position = mul(float4(output.positionWS, 1.0f), g_ModelViewProjectionMatrix);

    return output;
}

float4 RenderDiffusePS(PSIn_Diffuse input) : SV_Target
{
#ifdef FLAT_NORMAL
	float3 dir_x = ddx(input.positionWS);
	float3 dir_y = ddy(input.positionWS);
    float3 normal = normalize(cross(dir_x, dir_y));
#else
    float3 normal = normalize(input.normal);
#endif

    float3 lightDir = normalize(g_CameraPosition - input.positionWS);
    
    float dotNL = max(dot(normal, lightDir), 0.0f);
    float diffuse = dotNL * 0.75f + 0.25f;
    float specular = pow(dotNL, 100.0f);

    float3 diffuseColor = float3(1.0, 0.5, 0.35) * 0.75;

    float3 color = diffuseColor * diffuse + specular * 0.25;
    
    return float4(color, 0);
}

float4 RenderTessellatedDiffusePS(PSIn_TessellatedDiffuse input) : SV_Target
{
#ifdef FLAT_NORMAL
	float3 dir_x = ddx(input.positionWS);
	float3 dir_y = ddy(input.positionWS);
    float3 normal = normalize(cross(dir_x, dir_y));
    float3 lightDir = normalize(g_CameraPosition - input.positionWS);
#else    
    float3 normal = normalize(input.normal);
    float3 tangent = normalize(input.tangent);
    float3 bitangent = cross(normal, tangent) * input.sign;
    float3x3 tangentBasis = float3x3(tangent, bitangent, normal);
    
    float3 lightDir = normalize(g_CameraPosition - input.positionWS);
    lightDir = normalize(mul(tangentBasis, lightDir));
   
    normal = normalize(g_WSNormalMap.Sample(SamplerLinearClamp, input.texCoord).xyz);
#endif
    
    float dotNL = max(dot(normal, lightDir), 0.0f);
    float diffuse = dotNL * 0.75f + 0.25f;
    float specular = pow(dotNL, 100.0f);

    float3 diffuseColor = float3(1.0, 0.5, 0.35) * 0.75;// * 0.75 + (input.sign * 0.5 + 0.5) * 0.25;

    float3 color = diffuseColor * diffuse + specular * 0.25;
    
    return float4(color, 0);
}

float4 RenderTexCoordPS(PSIn_Diffuse input) : SV_Target
{
    return float4(input.texCoord, 0, 1);
}

float4 RenderNormalMapPS(PSIn_Diffuse input) : SV_Target
{
    float3 normal = g_WSNormalMap.Sample(SamplerLinearClamp, input.texCoord).xyz;
    normal = normalize(normal);

    return float4(normal, 0);
}

float4 RenderDisplacementMapPS(PSIn_Diffuse input) : SV_Target
{
    float displacement = g_DisplacementTexture.Sample(SamplerLinearClamp, input.texCoord).x;

    return float4(displacement, displacement, displacement, 0);
}

float4 RenderCheckerBoardPS(PSIn_Diffuse input) : SV_Target
{
    int column = input.texCoord.x * g_MapDimensions.x;
    int row = input.texCoord.y * g_MapDimensions.y;
    int color = (column + row) & 0x1;
    
    return float4(color, color, color, 0);
}

PSIn_Quad RenderQuadVS(uint VertexID : SV_VertexID)
{
    PSIn_Quad output;

    output.position = float4(QuadVertices[VertexID], 0, 1);
    output.texCoord = QuadTexCoordinates[VertexID];

    return output;
}

// Code for custom post processing of displacement map
float PostProcessDisplacementPS(PSIn_Quad input) : SV_Target
{
    float bumpScale = max(abs(g_DisplacementMin), abs(g_DisplacementMax));

    float displacement = g_DisplacementTexture.Sample(SamplerPointClamp, input.texCoord);
    displacement /= bumpScale;
    displacement = saturate(displacement * 0.5f + 0.5f);

    return displacement;
}

technique11 RenderTriangles
{
    pass p0
    {
        SetRasterizerState(CullBackMS);
        SetDepthStencilState(DepthNormal, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetVertexShader(CompileShader(vs_4_0, RenderDiffuseVS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, RenderDiffusePS()));
    }
}

technique11 RenderTrianglesAnimated
{
    pass p0
    {
        SetRasterizerState(CullBackMS);
        SetDepthStencilState(DepthNormal, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetVertexShader(CompileShader(vs_4_0, RenderDiffuseVS(true)));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, RenderDiffusePS()));
    }
}

technique11 RenderTessellatedTriangles
{
    pass p0
    {
        SetRasterizerState(CullBackMS);
        SetDepthStencilState(DepthNormal, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetVertexShader(CompileShader(vs_4_0, RenderTessellatedDiffuseVS()));
        SetHullShader(CompileShader(hs_5_0, DiffuseHS()));
        SetDomainShader(CompileShader(ds_5_0, DiffuseDS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, RenderTessellatedDiffusePS()));
    }
}

technique11 RenderTessellatedTrianglesAnimated
{
    pass p0
    {
        SetRasterizerState(CullBackMS);
        SetDepthStencilState(DepthNormal, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetVertexShader(CompileShader(vs_4_0, RenderTessellatedDiffuseVS(true)));
        SetHullShader(CompileShader(hs_5_0, DiffuseHS()));
        SetDomainShader(CompileShader(ds_5_0, DiffuseDS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, RenderTessellatedDiffusePS()));
    }
}

technique11 RenderQuads
{
    pass p0
    {
        SetRasterizerState(CullBackMS);
        SetDepthStencilState(DepthNormal, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetVertexShader(CompileShader(vs_4_0, RenderTessellatedDiffuseVS()));
		SetHullShader(CompileShader(hs_5_0, DiffuseQuadsHS()));
        SetDomainShader(CompileShader(ds_5_0, DiffuseQuadsDS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, RenderDiffusePS()));
    }
}

technique11 RenderTessellatedQuads
{
    pass p0
    {
        SetRasterizerState(CullBackMS);
        SetDepthStencilState(DepthNormal, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetVertexShader(CompileShader(vs_4_0, RenderTessellatedDiffuseVS()));
        SetHullShader(CompileShader(hs_5_0, DiffuseTessQuadsHS()));
        SetDomainShader(CompileShader(ds_5_0, DiffuseTessQuadsDS()));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, RenderTessellatedDiffusePS()));
    }
}

technique11 RenderVectors
{
    pass p0
    {
        SetRasterizerState(WireframeMS);
        SetDepthStencilState(DepthNormal, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetVertexShader(CompileShader(vs_4_0, RenderVectorsVS(1.0f)));
        SetHullShader(NULL);
        SetDomainShader(NULL);
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, ColorPS(float4(1.0f, 0.0f, 0.0f, 0.0f))));
    }

    pass p1
    {
        SetRasterizerState(WireframeMS);
        SetDepthStencilState(DepthNormal, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetVertexShader(CompileShader(vs_4_0, RenderVectorsVS(1.0f)));
        SetHullShader(NULL);
        SetDomainShader(NULL);
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, ColorPS(float4(0.0f, 1.0f, 0.0f, 0.0f))));
    }
}

technique11 RenderVectorsAnimated
{
    pass p0
    {
        SetRasterizerState(WireframeMS);
        SetDepthStencilState(DepthNormal, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetVertexShader(CompileShader(vs_4_0, RenderVectorsAnimatedVS(0.02f)));
        SetHullShader(NULL);
        SetDomainShader(NULL);
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, ColorPS(float4(1.0f, 0.0f, 0.0f, 0.0f))));
    }

    pass p1
    {
        SetRasterizerState(WireframeMS);
        SetDepthStencilState(DepthNormal, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetVertexShader(CompileShader(vs_4_0, RenderVectorsAnimatedVS(0.02f)));
        SetHullShader(NULL);
        SetDomainShader(NULL);
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, ColorPS(float4(0.0f, 1.0f, 0.0f, 0.0f))));
    }
}

technique11 PostProcessDisplacement
{
    pass p0
    {
        SetRasterizerState(NoCullMS);
        SetDepthStencilState(NoDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetVertexShader(CompileShader(vs_4_0, RenderQuadVS()));
        SetHullShader(NULL);
        SetDomainShader(NULL);
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_4_0, PostProcessDisplacementPS()));
    }
}

technique11 RenderDebug
{
    pass RenderTexCoord
    {
        SetPixelShader(CompileShader(ps_4_0, RenderTexCoordPS()));
    }

    pass RenderNormalMap
    {
        SetPixelShader(CompileShader(ps_4_0, RenderNormalMapPS()));
    }

    pass RenderCheckerBoard
    {
         SetPixelShader(CompileShader(ps_5_0, RenderCheckerBoardPS()));
    }

    pass RenderDisplacementMap
    {
        SetPixelShader(CompileShader(ps_5_0, RenderDisplacementMapPS()));
    }
}

technique11 RenderWireframe
{
    pass p0 
    {
        SetRasterizerState(Wireframe);
        SetDepthStencilState(DepthWireframePass, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetPixelShader(CompileShader(ps_4_0, ColorPS(float4(1.0f, 1.0f, 1.0f, 1.0f))));
    }
}
