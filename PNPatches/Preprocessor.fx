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

Buffer<float4>      g_PositionsBuffer;
Buffer<float2>      g_CoordinatesBuffer;
Buffer<int2>        g_IndicesBuffer;
Buffer<float4>      g_NormalsBuffer;
Buffer<float4>      g_TangentsBuffer;

Texture2D           g_CoarsePositions;
Texture2D           g_CoarseNormals;
Texture2D           g_FineNormals;
Texture2D           g_FinePositions;

Texture2D           g_NormalMap;
Texture2D<float>    g_DisplacementMap;

struct PSIn_Diffuse
{
	nointerpolation int2 dummy : DUMMY;
    nointerpolation float sign : TEXCOORD4;
    nointerpolation int patchID : PATCHID;

    float4 position     : SV_Position;
    float2 texCoord     : TEXCOORD0;
    float3 positionWS   : TEXCOORD1;
    float3 normal       : TEXCOORD2;
    float3 tangent      : TEXCOORD3;
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
};

shared cbuffer cb1
{
    float2 g_TileOffset;
    float2 g_BorderOffset;
    float2 g_TileScale;
    int g_PatchOffset = 0;

    int g_DownsampleRatio = 1;
    float g_rTexelSize;
};

PSIn_Quad RenderQuadVS(uint VertexID : SV_VertexID)
{
    PSIn_Quad output;

    output.position = float4(QuadVertices[VertexID], 0, 1);
    output.texCoord = QuadTexCoordinates[VertexID];

    return output;
}

PSIn_Diffuse RenderDiffuseVS(uint vertexID : SV_VertexID)
{
    PSIn_Diffuse output;
    
    int2 indices = g_IndicesBuffer.Load(vertexID);

    float3 position = g_PositionsBuffer.Load(indices.x).xyz;
    output.position = mul(float4(position, 1), g_ModelViewProjectionMatrix);
    output.positionWS = position;

    output.texCoord = g_CoordinatesBuffer.Load(indices.x);
    
    output.normal = g_NormalsBuffer.Load(indices.x).xyz;
    
    float4 tangentData = g_TangentsBuffer.Load(indices.x);
    output.tangent = tangentData.xyz;

    return output;
}

struct HSIn_Diffuse
{
    float3 position     : POSITION;
    float2 texCoord     : TEXCOORD0;
    float3 normal       : TEXCOORD1;
    float3 tangent      : TEXCOORD2;
};

HSIn_Diffuse RenderTessellatedDiffuseVS(uint vertexID : SV_VertexID)
{
    HSIn_Diffuse output;
    
    int2 indices = g_IndicesBuffer.Load(vertexID);

    output.position = g_PositionsBuffer.Load(indices.x).xyz;
    output.texCoord = g_CoordinatesBuffer.Load(indices.x);
    output.normal = g_NormalsBuffer.Load(indices.x).xyz;

    float4 tangentData = g_TangentsBuffer.Load(indices.x);
    output.tangent = tangentData.xyz;

    return output;
}

#define PN_TRIANGLES

struct HS_CONSTANT_DATA_OUTPUT
{
    float Edges[3]  : SV_TessFactor;
    float Inside    : SV_InsideTessFactor;

    float3 normal   : NORMAL;
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
    
    int patchID     : PATCHID;
};

struct HS_CONSTANT_DATA_OUTPUT_QUADS
{
    float Edges[4]  : SV_TessFactor;
    float Inside[2] : SV_InsideTessFactor;

    int patchID     : PATCHID;
};

struct HS_CONSTANT_DATA_OUTPUT_TESS_QUADS
{
    float Edges[4]          : SV_TessFactor;
    float Inside[2]         : SV_InsideTessFactor;
    float3 vEdgePos[8]      : EDGEPOS;
    float3 vInteriorPos[4]  : INTERIORPOS;
    float3 vEdgeNorm[4]     : EDGENORM;
    float3 vInteriorNorm    : INTERIORNORM;

    float sign              : SIGN;
    
    int patchID             : PATCHID;
};

HS_CONSTANT_DATA_OUTPUT DiffuseConstantHS( InputPatch<HSIn_Diffuse, 3> inputPatch, uint primitiveID : SV_PrimitiveID )
{    
    HS_CONSTANT_DATA_OUTPUT output;

    output.Edges[0] = output.Edges[1] = output.Edges[2] = g_TessellationFactor;
    output.Inside = g_TessellationFactor;

#ifdef PN_TRIANGLES
    // Assign Positions
    float3 f3B003 = inputPatch[0].position;
    float3 f3B030 = inputPatch[1].position;
    float3 f3B300 = inputPatch[2].position;
    // And Normals
    float3 f3N002 = inputPatch[0].normal;
    float3 f3N020 = inputPatch[1].normal;
    float3 f3N200 = inputPatch[2].normal;
        
    // Compute the cubic geometry control points
    // Edge control points
    output.f3B210 = ( ( 2.0f * f3B003 ) + f3B030 - ( dot( ( f3B030 - f3B003 ), f3N002 ) * f3N002 ) ) / 3.0f;
    output.f3B120 = ( ( 2.0f * f3B030 ) + f3B003 - ( dot( ( f3B003 - f3B030 ), f3N020 ) * f3N020 ) ) / 3.0f;
    output.f3B021 = ( ( 2.0f * f3B030 ) + f3B300 - ( dot( ( f3B300 - f3B030 ), f3N020 ) * f3N020 ) ) / 3.0f;
    output.f3B012 = ( ( 2.0f * f3B300 ) + f3B030 - ( dot( ( f3B030 - f3B300 ), f3N200 ) * f3N200 ) ) / 3.0f;
    output.f3B102 = ( ( 2.0f * f3B300 ) + f3B003 - ( dot( ( f3B003 - f3B300 ), f3N200 ) * f3N200 ) ) / 3.0f;
    output.f3B201 = ( ( 2.0f * f3B003 ) + f3B300 - ( dot( ( f3B300 - f3B003 ), f3N002 ) * f3N002 ) ) / 3.0f;
    // Center control point
    float3 f3E = ( output.f3B210 + output.f3B120 + output.f3B021 + output.f3B012 + output.f3B102 + output.f3B201 ) / 6.0f;
    float3 f3V = ( f3B003 + f3B030 + f3B300 ) / 3.0f;
    output.f3B111 = f3E + ( ( f3E - f3V ) / 2.0f );
#endif

    output.patchID = g_PatchOffset + primitiveID;

    float2 t01 = inputPatch[1].texCoord - inputPatch[0].texCoord;
    float2 t02 = inputPatch[2].texCoord - inputPatch[0].texCoord;
    output.sign = t01.x * t02.y - t01.y * t02.x > 0.0f ? 1 : -1;

    return output;
}

HS_CONSTANT_DATA_OUTPUT_QUADS DiffuseConstantQuadsHS( InputPatch<HSIn_Diffuse, 4> inputPatch, uint primitiveID : SV_PrimitiveID )
{    
    HS_CONSTANT_DATA_OUTPUT_QUADS output;
    output.Edges[0] = output.Edges[1] = output.Edges[2] = output.Edges[3] = g_TessellationFactor;
    output.Inside[0] = output.Inside[1] = g_TessellationFactor;

    output.patchID = g_PatchOffset + primitiveID;

    return output;
}

HS_CONSTANT_DATA_OUTPUT_TESS_QUADS DiffuseConstantTessQuadsHS( InputPatch<HSIn_Diffuse, 4> inputPatch, uint primitiveID : SV_PrimitiveID )
{    
    HS_CONSTANT_DATA_OUTPUT_TESS_QUADS output = (HS_CONSTANT_DATA_OUTPUT_TESS_QUADS)0;
    output.Edges[0] = output.Edges[1] = output.Edges[2] = output.Edges[3] = g_TessellationFactor;
    output.Inside[0] = output.Inside[1] = g_TessellationFactor;

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

        float3 vNormTmp = inputPatch[i].normal + inputPatch[j].normal;
        float Vij = 2 * dot(vPosTmp, vNormTmp) / dot(vPosTmp, vPosTmp);
        float3 Hij = vNormTmp - Vij * vPosTmp;
        output.vEdgeNorm[iedge] = normalize(Hij);
    }
    // interior
    float3 q = output.vEdgePos[0];
    for (int i = 1; i < 8; ++i)
    {
        q += output.vEdgePos[i];
    }
    float3 center = inputPatch[0].position + inputPatch[1].position + inputPatch[2].position + inputPatch[3].position;
    [unroll]
    for (i = 0; i < 4; ++i)
    {
        float3 Ei = (2 * (output.vEdgePos[i * 2] + output.vEdgePos[((i + 3) & 3) * 2 + 1] + q) - (output.vEdgePos[((i + 1) & 3) * 2 + 1] + output.vEdgePos[((i + 2) & 3) * 2])) / 18;
        float3 Vi = (center + 2 * (inputPatch[(i + 3) & 3].position + inputPatch[(i + 1) & 3].position) + inputPatch[(i + 2) & 3].position) / 9;
        output.vInteriorPos[i] = 3./2 * Ei - 1./2 * Vi;
    }
    output.vInteriorNorm = 2 * (output.vEdgeNorm[0] + output.vEdgeNorm[1] + output.vEdgeNorm[2] + output.vEdgeNorm[3]);
    output.vInteriorNorm += inputPatch[0].normal + inputPatch[1].normal + inputPatch[2].normal + inputPatch[3].normal;
    output.vInteriorNorm /= 12;

    output.patchID = g_PatchOffset + primitiveID;

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
HSIn_Diffuse DiffuseHS( InputPatch<HSIn_Diffuse, 3> inputPatch,
                        uint i : SV_OutputControlPointID)
{
    return inputPatch[i];
}

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("DiffuseConstantQuadsHS")]
[maxtessfactor(64.0)]
HSIn_Diffuse DiffuseQuadsHS(    InputPatch<HSIn_Diffuse, 4> inputPatch,
                                uint i : SV_OutputControlPointID)
{
    return inputPatch[i];
}

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("DiffuseConstantTessQuadsHS")]
[maxtessfactor(64.0)]
HSIn_Diffuse DiffuseTessQuadsHS(    InputPatch<HSIn_Diffuse, 4> inputPatch,
                                    uint i : SV_OutputControlPointID)
{
    return inputPatch[i];
}

[domain("tri")]
PSIn_Diffuse DiffuseDS( HS_CONSTANT_DATA_OUTPUT input, 
                        float3 barycentricCoords : SV_DomainLocation,
                        OutputPatch<HSIn_Diffuse, 3> inputPatch,
                        uniform bool tiled )
{
    PSIn_Diffuse output;

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
    float3 position = inputPatch[0].position * fWW * fW +
        inputPatch[1].position * fUU * fU +
        inputPatch[2].position * fVV * fV +
        input.f3B210 * fWW3 * fU +
        input.f3B120 * fW * fUU3 +
        input.f3B201 * fWW3 * fV +
        input.f3B021 * fUU3 * fV +
        input.f3B102 * fW * fVV3 +
        input.f3B012 * fU * fVV3 +
        input.f3B111 * 6.0f * fW * fU * fV;

    // Compute normal from quadratic control points and barycentric coords
    float3 normal = inputPatch[0].normal * fW + inputPatch[1].normal * fU + inputPatch[2].normal * fV;
	normal = normalize(normal);

    float3 tangent = inputPatch[0].tangent * fW + inputPatch[1].tangent * fU + inputPatch[2].tangent * fV;
	tangent = normalize(tangent);

    float2 texCoord = inputPatch[0].texCoord * coordinates.z + inputPatch[1].texCoord * coordinates.x + inputPatch[2].texCoord * coordinates.y;
    float2 textureSpacePos = texCoord;
    if (tiled)
    {
        textureSpacePos = (textureSpacePos - g_TileOffset) * g_TileScale + g_BorderOffset;
    }
    textureSpacePos = textureSpacePos * 2.0f - 1.0f;
    output.position = float4(textureSpacePos.x, -textureSpacePos.y, 0, 1);
    output.texCoord = texCoord;
    output.positionWS = position;

    output.normal = normal;
    output.tangent = tangent;
    output.sign = input.sign;//inputPatch[0].sign;
    output.patchID = input.patchID;

    return output; 
}

[domain("quad")]
PSIn_Diffuse DiffuseQuadsDS(    HS_CONSTANT_DATA_OUTPUT_QUADS input, 
                                float2 uv : SV_DomainLocation,
                                OutputPatch<HSIn_Diffuse, 4> inputPatch )
{
    PSIn_Diffuse output;

    output.positionWS = uv.y * (inputPatch[0].position * uv.x + inputPatch[1].position * (1 - uv.x)) +
        (1 - uv.y) * (inputPatch[3].position * uv.x + inputPatch[2].position * (1 - uv.x));
    output.normal = uv.y * (inputPatch[0].normal * uv.x + inputPatch[1].normal * (1 - uv.x)) +
        (1 - uv.y) * (inputPatch[3].normal * uv.x + inputPatch[2].normal * (1 - uv.x));
    output.tangent = uv.y * (inputPatch[0].tangent * uv.x + inputPatch[1].normal * (1 - uv.x)) +
        (1 - uv.y) * (inputPatch[3].tangent * uv.x + inputPatch[2].tangent * (1 - uv.x));
    output.texCoord = uv.y * (inputPatch[0].texCoord * uv.x + inputPatch[1].texCoord * (1 - uv.x)) +
        (1 - uv.y) * (inputPatch[3].texCoord * uv.x + inputPatch[2].texCoord * (1 - uv.x));
    output.position = mul(float4(output.positionWS, 1.0f), g_ModelViewProjectionMatrix);

    output.patchID = input.patchID;

    return output;
}

[domain("quad")]
PSIn_Diffuse DiffuseTessQuadsDS(    HS_CONSTANT_DATA_OUTPUT_TESS_QUADS input, 
                                    float2 uv : SV_DomainLocation,
                                    OutputPatch<HSIn_Diffuse, 4> inputPatch,
                                    uniform bool tiled )
{
    PSIn_Diffuse output;

    float4 BasisU = BernsteinBasis( uv.x );
    float4 BasisV = BernsteinBasis( uv.y );
    
    output.positionWS = EvaluateBezier(inputPatch[0].position, input.vEdgePos[0], input.vEdgePos[1], inputPatch[1].position,
                                       input.vEdgePos[7], input.vInteriorPos[0], input.vInteriorPos[1], input.vEdgePos[2],
                                       input.vEdgePos[6], input.vInteriorPos[3], input.vInteriorPos[2], input.vEdgePos[3],
                                       inputPatch[3].position, input.vEdgePos[5], input.vEdgePos[4], inputPatch[2].position,
                                       BasisU, BasisV);

    output.normal = (1 - uv.y) * (inputPatch[0].normal * (1 - uv.x) + inputPatch[1].normal * uv.x) +
        uv.y * (inputPatch[3].normal * (1 - uv.x) + inputPatch[2].normal * uv.x);

    output.tangent = (1 - uv.y) * (inputPatch[0].tangent * (1 - uv.x) + inputPatch[1].tangent * uv.x) +
        uv.y * (inputPatch[3].tangent * (1 - uv.x) + inputPatch[2].tangent * uv.x);
    
    float2 texCoord = (1 - uv.y) * (inputPatch[0].texCoord * (1 - uv.x) + inputPatch[1].texCoord * uv.x) +
        uv.y * (inputPatch[3].texCoord * (1 - uv.x) + inputPatch[2].texCoord * uv.x);

    float2 textureSpacePos = texCoord;
    if (tiled)
    {
        textureSpacePos = (textureSpacePos - g_TileOffset) * g_TileScale + g_BorderOffset;
    }
    textureSpacePos = textureSpacePos * 2.0f - 1.0f;
    output.position = float4(textureSpacePos.x, -textureSpacePos.y, 0, 1);
    output.texCoord = texCoord;
    output.sign = input.sign;//inputPatch[0].sign;
    output.patchID = input.patchID;

    return output;
}

struct OutputPS
{
    float4 normal   : SV_Target0;
    float4 position : SV_Target1;
};

OutputPS RenderPositionAndNormalPS(PSIn_Diffuse input)
{
    OutputPS output;

    // sample reference positions
    output.normal = float4(normalize(input.normal), 1);
    output.position = float4(input.positionWS, asfloat(input.patchID));

    return output;
}

OutputPS _RenderPositionAndNormalPS(PSIn_Diffuse input)
{
    OutputPS output;

    // sample reference positions
    output.normal = float4(normalize(input.normal), 1);
    output.position = float4(input.positionWS, 1);

    return output;
}

float4 ConvertNormalToTangentPS(PSIn_Diffuse input) : SV_Target
{
    float3 normal = normalize(input.normal);
    float3 tangent = normalize(input.tangent);
    float3 bitangent = normalize(cross(normal, tangent)) * input.sign;
    float3x3 tangentBasis = float3x3(tangent, bitangent, normal);
    
    float4 normalWS = g_CoarseNormals.Load(uint3(input.position.xy, 0));

    float3 normalLength = length(normalWS.xyz);

    // Skip conversion if no valid normal provided
    if (length(normalWS.xyz) == 0)
    {
        return normalWS;
    }

    float3 normalTS = normalize(mul(tangentBasis, normalWS.xyz));
    return float4(normalTS, normalWS.w);
}

float4 DilateNormalMapPS(PSIn_Quad input) : SV_Target
{
    float4 normalData = g_NormalMap.Sample(SamplerPointClamp, input.texCoord);
    float3 normal = normalData.xyz;

    int emptyTexel = length(normal) == 0;

    if (emptyTexel)
    {
        const int2 offsets[8] =
        {
            {-1,-1},
            {-1, 0},
            {-1, 1},
            { 0,-1},
            { 0, 1},
            { 1,-1},
            { 1, 0},
            { 1, 1}
        };

        float3 normals = 0;

        [unroll]
        for (int i=0; i<8; ++i)
        {
            normals += g_NormalMap.Sample(SamplerPointClamp, input.texCoord, offsets[i]).xyz;
        }
        
        float normalsLength = length(normals);

        return normalsLength ? float4(normalize(normals), 0) : 0;
    }

    return normalData;
}

float DilateDisplacementMapPS(PSIn_Quad input) : SV_Target
{
    int neigboursNum = 0;
    
    float displacement = g_DisplacementMap.Sample(SamplerPointClamp, input.texCoord);

    if (displacement == -999)
    {
        const int2 offsets[8] =
        {
            {-1,-1},
            {-1, 0},
            {-1, 1},
            { 0,-1},
            { 0, 1},
            { 1,-1},
            { 1, 0},
            { 1, 1}
        };

        float displacements = 0;

        [unroll]
        for (int i=0; i<8; ++i)
        {
            displacement = g_DisplacementMap.Sample(SamplerPointClamp, input.texCoord, offsets[i]);
            if (displacement != -999)
            {
                displacements += displacement;
                neigboursNum++;
            }
        }

        return neigboursNum ? displacements / neigboursNum : -999;
    }

    return displacement;
}

float4 DownsampleNormalMapPS(PSIn_Quad input) : SV_Target
{
    float3 normal = 0;

    float2 texCoord = input.texCoord - g_rTexelSize * (g_DownsampleRatio / 2 - 0.5);

    for (int i=0; i<g_DownsampleRatio; ++i)
    {
        for(int j=0; j<g_DownsampleRatio; ++j)
        {
            normal += g_NormalMap.Sample(SamplerPointClamp, float2(texCoord.x + j * g_rTexelSize, texCoord.y + i * g_rTexelSize)).xyz;
        }
    }

    if (length(normal) == 0)
    {
        return 0;
    }
    
    return float4(normalize(normal), 1);
}

float DownsampleDisplacementMapPS(PSIn_Quad input) : SV_Target
{
    float displacements = 0;
    int neigboursNum = 0;

    float2 texCoord = input.texCoord - g_rTexelSize * (g_DownsampleRatio / 2 - 0.5);

    for (int i=0; i<g_DownsampleRatio; ++i)
    {
        for(int j=0; j<g_DownsampleRatio; ++j)
        {
            float displacement = g_DisplacementMap.Sample(SamplerPointClamp, float2(texCoord.x + j * g_rTexelSize, texCoord.y + i * g_rTexelSize));
            if (displacement != -999)
            {
                displacements += displacement;
                neigboursNum++;
            }
        }
    }

    return neigboursNum ? displacements / neigboursNum : -999;
}

technique11 RenderTessellatedTriangles
{
    pass p0
    {
        SetRasterizerState(NoCullMS);
        SetDepthStencilState(NoDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetVertexShader(CompileShader(vs_5_0, RenderTessellatedDiffuseVS()));
        SetHullShader(CompileShader(hs_5_0, DiffuseHS()));
        SetDomainShader(CompileShader(ds_5_0, DiffuseDS(false)));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, _RenderPositionAndNormalPS()));
    }
}

technique11 RenderTessellatedTrianglesTiled
{
    pass p0
    {
        SetRasterizerState(NoCullMS);
        SetDepthStencilState(NoDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetVertexShader(CompileShader(vs_5_0, RenderTessellatedDiffuseVS()));
        SetHullShader(CompileShader(hs_5_0, DiffuseHS()));
        SetDomainShader(CompileShader(ds_5_0, DiffuseDS(true)));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, RenderPositionAndNormalPS()));
    }
}

technique11 RenderTessellatedQuads
{
    pass p0
    {
        SetRasterizerState(NoCullMS);
        SetDepthStencilState(NoDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetVertexShader(CompileShader(vs_5_0, RenderTessellatedDiffuseVS()));
        SetHullShader(CompileShader(hs_5_0, DiffuseTessQuadsHS()));
        SetDomainShader(CompileShader(ds_5_0, DiffuseTessQuadsDS(false)));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, _RenderPositionAndNormalPS()));
    }
}

technique11 RenderTessellatedQuadsTiled
{
    pass p0
    {
        SetRasterizerState(NoCullMS);
        SetDepthStencilState(NoDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);

        SetVertexShader(CompileShader(vs_5_0, RenderTessellatedDiffuseVS()));
        SetHullShader(CompileShader(hs_5_0, DiffuseTessQuadsHS()));
        SetDomainShader(CompileShader(ds_5_0, DiffuseTessQuadsDS(true)));
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, RenderPositionAndNormalPS()));
    }
}

technique11 ConvertNormalToTangent
{
    pass p0
    {
        SetPixelShader(CompileShader(ps_5_0, ConvertNormalToTangentPS()));
    }
}

technique11 DilateNormalMap
{
    pass p0
    {
        SetRasterizerState(NoCullMS);
        SetDepthStencilState(NoDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
        
        SetVertexShader(CompileShader(vs_5_0, RenderQuadVS()));
        SetHullShader(NULL);
        SetDomainShader(NULL);
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, DilateNormalMapPS()));
    }
}

technique11 DilateDisplacementMap
{
    pass p0
    {
        SetRasterizerState(NoCullMS);
        SetDepthStencilState(NoDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
        
        SetVertexShader(CompileShader(vs_5_0, RenderQuadVS()));
        SetHullShader(NULL);
        SetDomainShader(NULL);
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, DilateDisplacementMapPS()));
    }
}

technique11 DownsampleNormalMap
{
    pass p0
    {
        SetRasterizerState(NoCullMS);
        SetDepthStencilState(NoDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
        
        SetVertexShader(CompileShader(vs_5_0, RenderQuadVS()));
        SetHullShader(NULL);
        SetDomainShader(NULL);
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, DownsampleNormalMapPS()));
    }
}

technique11 DownsampleDisplacementMap
{
    pass p0
    {
        SetRasterizerState(NoCullMS);
        SetDepthStencilState(NoDepthStencil, 0);
        SetBlendState(NoBlending, float4(0.0f, 0.0f, 0.0f, 0.0f), 0xFFFFFFFF);
        
        SetVertexShader(CompileShader(vs_5_0, RenderQuadVS()));
        SetHullShader(NULL);
        SetDomainShader(NULL);
        SetGeometryShader(NULL);
        SetPixelShader(CompileShader(ps_5_0, DownsampleDisplacementMapPS()));
    }
}