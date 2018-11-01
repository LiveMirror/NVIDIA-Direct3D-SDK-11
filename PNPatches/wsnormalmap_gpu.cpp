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

#include "wsnormalmap.h"
#include <dxut.h>
#include "SDKmisc.h"
#include <assert.h>

#include <D3DX11tex.h>
#include "mymesh.h"

bool WSNormalMap::s_bInited                     = false;

ID3D11Device* WSNormalMap::m_pDevice            = NULL;
ID3D11DeviceContext* WSNormalMap::m_pContext    = NULL;
ID3DX11Effect* WSNormalMap::m_pEffect           = NULL;
ID3DX11Effect* WSNormalMap::m_pEffectCS         = NULL;
ID3DX11EffectPass* WSNormalMap::m_pDefaultPass  = NULL;

WSNormalMap::WSNormalMap(int2 dimensions, DXGI_FORMAT format)
{
    m_MapDimensions = dimensions;
    m_Format = format;
}

void WSNormalMap::GenerateNormalMapAndPositionMap_GPU(MyMesh *pMesh, ID3D11ShaderResourceView **ppNormalMapSRV, ID3D11ShaderResourceView **ppPositionMapSRV)
{
    assert(s_bInited == true);
    if (!s_bInited) return;

    ID3D11Texture2D* pNormalMap = NULL;
    ID3D11RenderTargetView *pNormalMapRTV = NULL;
    ID3D11Texture2D* pPositionMap = NULL;
    ID3D11RenderTargetView *pPositionMapRTV = NULL;

    const float clearColor[4] = {0, 0, 0, 0};
    ID3D11RenderTargetView* pRenderTargetViews[2] = {NULL, NULL};

    if (!ppNormalMapSRV && !ppPositionMapSRV) return;

    if (ppNormalMapSRV)
    {
        D3D11_TEXTURE2D_DESC Desc;
        memset(&Desc, 0, sizeof(Desc));
        Desc.Width = m_MapDimensions[0];
        Desc.Height = m_MapDimensions[1];
        Desc.ArraySize = 1;
        Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        Desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        Desc.MipLevels = 1;
        Desc.SampleDesc.Count = 1;
        Desc.Usage = D3D11_USAGE_DEFAULT;
        m_pDevice->CreateTexture2D(&Desc, NULL, &pNormalMap);

        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
        memset(&SRVDesc, 0, sizeof(SRVDesc));
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Format = Desc.Format;
        SRVDesc.Texture2D.MipLevels = 1;
        m_pDevice->CreateShaderResourceView(pNormalMap, &SRVDesc, ppNormalMapSRV);

        D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
        memset(&RTVDesc, 0, sizeof(RTVDesc));
        RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        RTVDesc.Format = Desc.Format;
        m_pDevice->CreateRenderTargetView(pNormalMap, &RTVDesc, &pNormalMapRTV);

        pRenderTargetViews[0] = pNormalMapRTV;
        m_pContext->ClearRenderTargetView(pNormalMapRTV, clearColor);
    }
    
    if (ppPositionMapSRV)
    {
        D3D11_TEXTURE2D_DESC Desc;
        memset(&Desc, 0, sizeof(Desc));
        Desc.Width = m_MapDimensions[0];
        Desc.Height = m_MapDimensions[1];
        Desc.ArraySize = 1;
        Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        Desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        Desc.MipLevels = 1;
        Desc.SampleDesc.Count = 1;
        Desc.Usage = D3D11_USAGE_DEFAULT;
        m_pDevice->CreateTexture2D(&Desc, NULL, &pPositionMap);

        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
        memset(&SRVDesc, 0, sizeof(SRVDesc));
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Format = Desc.Format;
        SRVDesc.Texture2D.MipLevels = 1;
        m_pDevice->CreateShaderResourceView(pPositionMap, &SRVDesc, ppPositionMapSRV);

        D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
        memset(&RTVDesc, 0, sizeof(RTVDesc));
        RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        RTVDesc.Format = Desc.Format;
        m_pDevice->CreateRenderTargetView(pPositionMap, &RTVDesc, &pPositionMapRTV);

        pRenderTargetViews[1] = pPositionMapRTV;
        m_pContext->ClearRenderTargetView(pPositionMapRTV, clearColor);
    }

    m_pContext->IASetInputLayout(NULL);
    D3D11_VIEWPORT viewport = {0, 0, m_MapDimensions[0], m_MapDimensions[1], 0, 1};
    m_pContext->RSSetViewports(1, &viewport);
    m_pContext->OMSetRenderTargets(2, pRenderTargetViews, NULL);
        
    m_pEffect->GetVariableByName("g_TessellationFactor")->AsScalar()->SetFloat(16.0f);
    m_pEffect->GetVariableByName("g_CoordinatesBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshCoordinatesBufferSRV);
    m_pEffect->GetVariableByName("g_NormalsBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshNormalsBufferSRV);
    m_pEffect->GetVariableByName("g_PositionsBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshVertexBufferSRV);

    D3D11_BLEND_DESC blendDesc;
    memset(&blendDesc, 0, sizeof(blendDesc));
    blendDesc.AlphaToCoverageEnable = false;
    blendDesc.IndependentBlendEnable = true;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = 0xF;
    blendDesc.RenderTarget[1].RenderTargetWriteMask = 0xF;
    blendDesc.RenderTarget[1].BlendEnable = true;
    blendDesc.RenderTarget[1].DestBlend = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[1].SrcBlend = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[1].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[1].DestBlendAlpha = D3D11_BLEND_DEST_ALPHA;
    blendDesc.RenderTarget[1].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[1].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    
    ID3D11BlendState* pBlendState = NULL;
    m_pDevice->CreateBlendState(&blendDesc, &pBlendState);

    const float blendFactor[4] = {0.0f,0.0f,0.0f,0.0f}; 

    if (pMesh->g_MeshQuadIndicesNum)
    {
        m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
        m_pEffect->GetVariableByName("g_IndicesBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshQuadsIndexBufferSRV);
        m_pEffect->GetTechniqueByName("RenderTessellatedQuads")->GetPassByIndex(0)->Apply(0, m_pContext);
        m_pContext->OMSetBlendState(pBlendState, blendFactor, 0xffffffff);
        m_pContext->Draw(pMesh->g_MeshQuadIndicesNum, 0);
    }

    if (pMesh->g_MeshTrgIndicesNum)
    {
        m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
        m_pEffect->GetVariableByName("g_IndicesBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshTrgsIndexBufferSRV);
        m_pEffect->GetTechniqueByName("RenderTessellatedTriangles")->GetPassByIndex(0)->Apply(0, m_pContext);
        m_pContext->OMSetBlendState(pBlendState, blendFactor, 0xffffffff);
        m_pContext->Draw(pMesh->g_MeshTrgIndicesNum, 0);
    }

    SAFE_RELEASE(pBlendState);

    DisableTessellation();
    m_pContext->OMSetRenderTargets(0, NULL, NULL);
    
    SAFE_RELEASE(pNormalMap);
    SAFE_RELEASE(pNormalMapRTV);
    SAFE_RELEASE(pPositionMap);
    SAFE_RELEASE(pPositionMapRTV);
}

void WSNormalMap::GetMapsTiled(MyMesh *pMesh, ID3D11Texture2D **ppNormalMapTexture, ID3D11Texture2D **ppPositionMapTexture, int2 tile_origin, int tile_size, int ss_scale, int border)
{
    if (!Initialized()) return;

    if (!ppNormalMapTexture && !ppPositionMapTexture) return;

    const float clearColor[4] = {0, 0, 0, 0};
    ID3D11RenderTargetView* pRenderTargetViews[2] = {NULL, NULL};

    int tileSize = (tile_size + 2 * border);

    D3D11_TEXTURE2D_DESC desc;
    memset(&desc, 0, sizeof(desc));
    desc.Width = tileSize * ss_scale;
    desc.Height = tileSize * ss_scale;
    desc.ArraySize = 1;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.Format = m_Format;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;

    D3D11_RENDER_TARGET_VIEW_DESC descRTV;
    memset(&descRTV, 0, sizeof(descRTV));
    descRTV.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    descRTV.Format = m_Format;

    if (ppNormalMapTexture)
    {
        if (!*ppNormalMapTexture) m_pDevice->CreateTexture2D(&desc, NULL, ppNormalMapTexture);

        m_pDevice->CreateRenderTargetView(*ppNormalMapTexture, &descRTV, &pRenderTargetViews[0]);
        m_pContext->ClearRenderTargetView(pRenderTargetViews[0], clearColor);
    }

    if (ppPositionMapTexture)
    {
        if (!*ppPositionMapTexture) m_pDevice->CreateTexture2D(&desc, NULL, ppPositionMapTexture);

        m_pDevice->CreateRenderTargetView(*ppPositionMapTexture, &descRTV, &pRenderTargetViews[1]);
        m_pContext->ClearRenderTargetView(pRenderTargetViews[1], clearColor);
    }

    m_pContext->IASetInputLayout(NULL);
    D3D11_VIEWPORT viewport = {0, 0, (float)tileSize * ss_scale, (float)tileSize * ss_scale, 0, 1};
    m_pContext->RSSetViewports(1, &viewport);
    m_pContext->OMSetRenderTargets(2, pRenderTargetViews, NULL);

    float2 tileOffset((float)(tile_origin[0]) / m_MapDimensions[0], (float)(tile_origin[1]) / m_MapDimensions[1]);
    float2 tileScale((float)m_MapDimensions[0] / tileSize, (float)m_MapDimensions[1] / tileSize);
    float2 borderOffset((float)border / tileSize, (float)border / tileSize);
    
    m_pEffect->GetVariableByName("g_TileOffset")->AsVector()->SetFloatVector(tileOffset.vector);
    m_pEffect->GetVariableByName("g_TileScale")->AsVector()->SetFloatVector(tileScale.vector);
    m_pEffect->GetVariableByName("g_BorderOffset")->AsVector()->SetFloatVector(borderOffset.vector);
        
    m_pEffect->GetVariableByName("g_TessellationFactor")->AsScalar()->SetFloat(pMesh->m_TessellationFactor);
    m_pEffect->GetVariableByName("g_CoordinatesBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshCoordinatesBufferSRV);
    m_pEffect->GetVariableByName("g_NormalsBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshNormalsBufferSRV);
    m_pEffect->GetVariableByName("g_PositionsBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshVertexBufferSRV);

    if (pMesh->g_MeshQuadIndicesNum)
    {
        m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
        m_pEffect->GetVariableByName("g_PatchOffset")->AsScalar()->SetInt(0);
        m_pEffect->GetVariableByName("g_IndicesBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshQuadsIndexBufferSRV);
        m_pEffect->GetTechniqueByName("RenderTessellatedQuadsTiled")->GetPassByIndex(0)->Apply(0, m_pContext);
        m_pContext->Draw(pMesh->g_MeshQuadIndicesNum, 0);
    }

    if (pMesh->g_MeshTrgIndicesNum)
    {
        m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
        m_pEffect->GetVariableByName("g_PatchOffset")->AsScalar()->SetInt(pMesh->g_MeshQuadIndicesNum / 4);
        m_pEffect->GetVariableByName("g_IndicesBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshTrgsIndexBufferSRV);
        m_pEffect->GetTechniqueByName("RenderTessellatedTrianglesTiled")->GetPassByIndex(0)->Apply(0, m_pContext);
        m_pContext->Draw(pMesh->g_MeshTrgIndicesNum, 0);
    }

    DisableTessellation();
    m_pContext->OMSetRenderTargets(0, NULL, NULL);

    SAFE_RELEASE(pRenderTargetViews[0]);
    SAFE_RELEASE(pRenderTargetViews[1]);
}

void WSNormalMap::FixupNormals_GPU(MyMesh *pMesh, ID3D11ShaderResourceView *pDisplacedPositionsSRV)
{
    if (!Initialized()) return;
    
    ID3D11Buffer* pNormalsBuffer = NULL;
    ID3D11UnorderedAccessView* pNormalsBufferUAV = NULL;
    
    D3D11_BUFFER_DESC desc;
    pMesh->g_pMeshNormalsBuffer->GetDesc(&desc);

    BYTE* pData = new BYTE[desc.ByteWidth];
    memset(pData, 0, desc.ByteWidth);
    
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = sizeof(float4);
    D3D11_SUBRESOURCE_DATA subresourceData;
    subresourceData.pSysMem = pData;
    m_pDevice->CreateBuffer(&desc, &subresourceData, &pNormalsBuffer);
    
    delete [] pData;
    
    D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
    memset(&descUAV, 0, sizeof(descUAV));
    descUAV.Format = DXGI_FORMAT_UNKNOWN;
    descUAV.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    descUAV.Buffer.NumElements = desc.ByteWidth / sizeof(float4);
    descUAV.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_COUNTER;
    m_pDevice->CreateUnorderedAccessView(pNormalsBuffer, &descUAV, &pNormalsBufferUAV);

    m_pEffectCS->GetVariableByName("g_CoordinatesBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshCoordinatesBufferSRV);
    m_pEffectCS->GetVariableByName("g_NormalsBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshNormalsBufferSRV);
    
    m_pEffectCS->GetVariableByName("g_CoarsePositions")->AsShaderResource()->SetResource(pMesh->g_pPositionMapSRV);
    m_pEffectCS->GetVariableByName("g_FinePositions")->AsShaderResource()->SetResource(pDisplacedPositionsSRV);
    m_pEffectCS->GetVariableByName("RWNormalsBuffer")->AsUnorderedAccessView()->SetUnorderedAccessView(pNormalsBufferUAV);

    if(pMesh->g_MeshQuadIndicesNum)
    {
        m_pEffectCS->GetVariableByName("g_IndicesBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshQuadsIndexBufferSRV);
        m_pEffectCS->GetVariableByName("g_PrimitivesNum")->AsScalar()->SetInt(pMesh->g_MeshQuadIndicesNum / 4);
        m_pEffectCS->GetTechniqueByName("FixupNormals")->GetPassByIndex(1)->Apply(0, m_pContext);
        m_pContext->Dispatch((UINT)ceil((float)(pMesh->g_MeshQuadIndicesNum / 4) / 256), 1, 1);
    }

    if(pMesh->g_MeshTrgIndicesNum)
    {
        m_pEffectCS->GetVariableByName("g_IndicesBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshTrgsIndexBufferSRV);
        m_pEffectCS->GetVariableByName("g_PrimitivesNum")->AsScalar()->SetInt(pMesh->g_MeshTrgIndicesNum / 3);
        m_pEffectCS->GetTechniqueByName("FixupNormals")->GetPassByIndex(0)->Apply(0, m_pContext);
        m_pContext->Dispatch((UINT)ceil((float)(pMesh->g_MeshTrgIndicesNum / 3) / 256), 1, 1);
    }

    m_pContext->CopyResource(pMesh->g_pMeshNormalsBuffer, pNormalsBuffer);

    SAFE_RELEASE(pNormalsBuffer);
    SAFE_RELEASE(pNormalsBufferUAV);
}

void WSNormalMap::TransformNormalMapToTangent_GPU(MyMesh *pMesh)
{
    if (!Initialized()) return;
    
    if (pMesh->g_pNormalMapSRV == NULL || pMesh->g_pMeshTangentsBuffer == NULL) return;

    ID3D11Texture2D* pNormalMapTexture              = NULL;
    ID3D11RenderTargetView* pNormalMapTextureRTV    = NULL;
    ID3D11ShaderResourceView* pNormalMapTextureSRV  = NULL;

    D3D11_TEXTURE2D_DESC desc;
    ID3D11Resource* pResource = NULL;
    pMesh->g_pNormalMapSRV->GetResource(&pResource);
    ((ID3D11Texture2D*)pResource)->GetDesc(&desc);
    pResource->Release();

    desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    m_pDevice->CreateTexture2D(&desc, NULL, &pNormalMapTexture);

    D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
    pMesh->g_pNormalMapSRV->GetDesc(&descSRV);
    m_pDevice->CreateShaderResourceView(pNormalMapTexture, &descSRV, &pNormalMapTextureSRV);

    D3D11_RENDER_TARGET_VIEW_DESC descRTV;
    memset(&descRTV, 0, sizeof(descRTV));
    descRTV.Format = descSRV.Format;
    descRTV.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    m_pDevice->CreateRenderTargetView(pNormalMapTexture, &descRTV, &pNormalMapTextureRTV);

    m_pContext->IASetInputLayout(NULL);
    D3D11_VIEWPORT viewport = {0, 0, (float)desc.Width, (float)desc.Height, 0, 1};
    m_pContext->RSSetViewports(1, &viewport);
    m_pContext->OMSetRenderTargets(1, &pNormalMapTextureRTV, NULL);
    
    const float clearColor[4] = {0, 0, 0, 0};
    m_pContext->ClearRenderTargetView(pNormalMapTextureRTV, clearColor);
        
    m_pEffect->GetVariableByName("g_TessellationFactor")->AsScalar()->SetFloat(1.0f);
    m_pEffect->GetVariableByName("g_CoordinatesBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshCoordinatesBufferSRV);
    m_pEffect->GetVariableByName("g_NormalsBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshNormalsBufferSRV);
    m_pEffect->GetVariableByName("g_TangentsBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshTangentsBufferSRV);
    m_pEffect->GetVariableByName("g_PositionsBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshVertexBufferSRV);
    m_pEffect->GetVariableByName("g_CoarseNormals")->AsShaderResource()->SetResource(pMesh->g_pNormalMapSRV);

    if (pMesh->g_MeshQuadIndicesNum)
    {
        m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
        m_pEffect->GetVariableByName("g_IndicesBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshQuadsIndexBufferSRV);
        m_pEffect->GetTechniqueByName("RenderTessellatedQuads")->GetPassByIndex(0)->Apply(0, m_pContext);
        m_pEffect->GetTechniqueByName("ConvertNormalToTangent")->GetPassByIndex(0)->Apply(0, m_pContext); // override PS
        m_pContext->Draw(pMesh->g_MeshQuadIndicesNum, 0);
    }

    if (pMesh->g_MeshTrgIndicesNum)
    {
        m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
        m_pEffect->GetVariableByName("g_IndicesBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshTrgsIndexBufferSRV);
        m_pEffect->GetTechniqueByName("RenderTessellatedTriangles")->GetPassByIndex(0)->Apply(0, m_pContext);
        m_pEffect->GetTechniqueByName("ConvertNormalToTangent")->GetPassByIndex(0)->Apply(0, m_pContext); // override PS
        m_pContext->Draw(pMesh->g_MeshTrgIndicesNum, 0);
    }

    m_pContext->OMSetRenderTargets(0, NULL, NULL);

    pMesh->g_pNormalMapSRV->Release();
    pMesh->g_pNormalMapSRV = pNormalMapTextureSRV;
    
    SAFE_RELEASE(pNormalMapTexture);
    SAFE_RELEASE(pNormalMapTextureRTV);
}

void WSNormalMap::TransformNormalMapTileToTangent_GPU(MyMesh *pMesh, ID3D11Texture2D **ppNormalMapTile, ID3D11Texture2D **ppNormalMapTileTS, int2 tile_origin, int tile_size, int ss_scale, int border)
{
    if (!Initialized()) return;

    if (!ppNormalMapTile || !ppNormalMapTileTS) return;

    ID3D11ShaderResourceView* pNormalMapTileSRV = NULL;
    ID3D11RenderTargetView* pNormalMapTileRTV = NULL;
    
    D3D11_TEXTURE2D_DESC desc;
    ((ID3D11Texture2D*)*ppNormalMapTile)->GetDesc(&desc);

    D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
    memset(&descSRV, 0, sizeof(descSRV));
    descSRV.Format = desc.Format;
    descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    descSRV.Texture2D.MipLevels= 1;
    m_pDevice->CreateShaderResourceView(*ppNormalMapTile, &descSRV, &pNormalMapTileSRV);
    
    if (*ppNormalMapTileTS == NULL)
    {
        desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
        m_pDevice->CreateTexture2D(&desc, NULL, ppNormalMapTileTS);
    }

    D3D11_RENDER_TARGET_VIEW_DESC descRTV;
    memset(&descRTV, 0, sizeof(descRTV));
    descRTV.Format = desc.Format;
    descRTV.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    m_pDevice->CreateRenderTargetView(*ppNormalMapTileTS, &descRTV, &pNormalMapTileRTV);

    m_pContext->IASetInputLayout(NULL);
    D3D11_VIEWPORT viewport = {0, 0, (float)desc.Width, (float)desc.Height, 0, 1};
    m_pContext->RSSetViewports(1, &viewport);
    m_pContext->OMSetRenderTargets(1, &pNormalMapTileRTV, NULL);
    
    const float clearColor[4] = {0, 0, 0, 0};
    m_pContext->ClearRenderTargetView(pNormalMapTileRTV, clearColor);

    int tileSize = tile_size + 2 * border;
    float2 tileOffset((float)(tile_origin[0]) / m_MapDimensions[0], (float)(tile_origin[1]) / m_MapDimensions[1]);
    float2 tileScale((float)m_MapDimensions[0] / tileSize, (float)m_MapDimensions[1] / tileSize);
    float2 borderOffset((float)border / tileSize, (float)border / tileSize);
    
    m_pEffect->GetVariableByName("g_TileOffset")->AsVector()->SetFloatVector(tileOffset.vector);
    m_pEffect->GetVariableByName("g_TileScale")->AsVector()->SetFloatVector(tileScale.vector);
    m_pEffect->GetVariableByName("g_BorderOffset")->AsVector()->SetFloatVector(borderOffset.vector);
        
    m_pEffect->GetVariableByName("g_TessellationFactor")->AsScalar()->SetFloat(1.0f);
    m_pEffect->GetVariableByName("g_CoordinatesBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshCoordinatesBufferSRV);
    m_pEffect->GetVariableByName("g_NormalsBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshNormalsBufferSRV);
    m_pEffect->GetVariableByName("g_TangentsBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshTangentsBufferSRV);
    m_pEffect->GetVariableByName("g_PositionsBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshVertexBufferSRV);
    m_pEffect->GetVariableByName("g_CoarseNormals")->AsShaderResource()->SetResource(pNormalMapTileSRV);

    if (pMesh->g_MeshQuadIndicesNum)
    {
        m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
        m_pEffect->GetVariableByName("g_IndicesBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshQuadsIndexBufferSRV);
        m_pEffect->GetTechniqueByName("RenderTessellatedQuadsTiled")->GetPassByIndex(0)->Apply(0, m_pContext);
        m_pEffect->GetTechniqueByName("ConvertNormalToTangent")->GetPassByIndex(0)->Apply(0, m_pContext); // override PS
        m_pContext->Draw(pMesh->g_MeshQuadIndicesNum, 0);
    }

    if (pMesh->g_MeshTrgIndicesNum)
    {
        m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST);
        m_pEffect->GetVariableByName("g_IndicesBuffer")->AsShaderResource()->SetResource(pMesh->g_pMeshTrgsIndexBufferSRV);
        m_pEffect->GetTechniqueByName("RenderTessellatedTrianglesTiled")->GetPassByIndex(0)->Apply(0, m_pContext);
        m_pEffect->GetTechniqueByName("ConvertNormalToTangent")->GetPassByIndex(0)->Apply(0, m_pContext); // override PS
        m_pContext->Draw(pMesh->g_MeshTrgIndicesNum, 0);
    }

    m_pContext->OMSetRenderTargets(0, NULL, NULL);

    SAFE_RELEASE(pNormalMapTileSRV);
    SAFE_RELEASE(pNormalMapTileRTV);
}

void WSNormalMap::DilateNormalMap(ID3D11Texture2D **ppNormalMapTexture, ID3D11Texture2D **ppDilatedNormalMapTexture, int borderSize)
{
    if (!Initialized()) return;
    
    if (!ppNormalMapTexture || !ppDilatedNormalMapTexture) return;

    ID3D11ShaderResourceView* pNormalMapSRV[2] = {NULL, NULL};
    ID3D11RenderTargetView*   pNormalMapRTV[2] = {NULL, NULL};
    
    D3D11_TEXTURE2D_DESC desc;
    (*ppNormalMapTexture)->GetDesc(&desc);

    if (*ppDilatedNormalMapTexture == NULL)
    {
        m_pDevice->CreateTexture2D(&desc, NULL, ppDilatedNormalMapTexture);
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
    memset(&descSRV, 0, sizeof(descSRV));
    descSRV.Format = desc.Format;
    descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    descSRV.Texture2D.MipLevels = 1;
    
    m_pDevice->CreateShaderResourceView(*ppNormalMapTexture, &descSRV, &pNormalMapSRV[0]);

    D3D11_RENDER_TARGET_VIEW_DESC descRTV;
    memset(&descRTV, 0, sizeof(descRTV));
    descRTV.Format = descSRV.Format;
    descRTV.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    
    m_pDevice->CreateRenderTargetView(*ppDilatedNormalMapTexture, &descRTV, &pNormalMapRTV[1]);

    if (borderSize > 1)
    {
        m_pDevice->CreateShaderResourceView(*ppDilatedNormalMapTexture, &descSRV, &pNormalMapSRV[1]);
        m_pDevice->CreateRenderTargetView(*ppNormalMapTexture, &descRTV, &pNormalMapRTV[0]);
    }
    
    m_pContext->IASetInputLayout(NULL);
    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    D3D11_VIEWPORT viewport = {0, 0, (float)desc.Width, (float)desc.Height, 0, 1};
    m_pContext->RSSetViewports(1, &viewport);
    
    int renderTarget = 1;
    
    for (int i=0; i<borderSize; ++i)
    {
        m_pContext->OMSetRenderTargets(1, &pNormalMapRTV[renderTarget], NULL);

        m_pEffect->GetVariableByName("g_NormalMap")->AsShaderResource()->SetResource(pNormalMapSRV[1-renderTarget]);
        m_pEffect->GetTechniqueByName("DilateNormalMap")->GetPassByIndex(0)->Apply(0, m_pContext);
        m_pContext->Draw(4, 0);

        m_pContext->OMSetRenderTargets(0, NULL, NULL);

        renderTarget = 1 - renderTarget;
    }

    SAFE_RELEASE(pNormalMapSRV[0]);
    SAFE_RELEASE(pNormalMapSRV[1]);
    SAFE_RELEASE(pNormalMapRTV[0]);
    SAFE_RELEASE(pNormalMapRTV[1]);
}

void WSNormalMap::DilateDisplacementMap(ID3D11Texture2D **ppDisplacementMapTexture, ID3D11Texture2D **ppDilatedDisplacementMapTexture, int borderSize)
{
    if (!Initialized()) return;
    
    if (!ppDisplacementMapTexture || !ppDilatedDisplacementMapTexture) return;

    ID3D11ShaderResourceView* pDisplacementMapSRV[2] = {NULL, NULL};
    ID3D11RenderTargetView*   pDisplacementMapRTV[2] = {NULL, NULL};
    
    D3D11_TEXTURE2D_DESC desc;
    (*ppDisplacementMapTexture)->GetDesc(&desc);

    if (*ppDilatedDisplacementMapTexture == NULL)
    {
        m_pDevice->CreateTexture2D(&desc, NULL, ppDilatedDisplacementMapTexture);
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
    memset(&descSRV, 0, sizeof(descSRV));
    descSRV.Format = desc.Format;
    descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    descSRV.Texture2D.MipLevels = 1;
    
    m_pDevice->CreateShaderResourceView(*ppDisplacementMapTexture, &descSRV, &pDisplacementMapSRV[0]);

    D3D11_RENDER_TARGET_VIEW_DESC descRTV;
    memset(&descRTV, 0, sizeof(descRTV));
    descRTV.Format = descSRV.Format;
    descRTV.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    
    m_pDevice->CreateRenderTargetView(*ppDilatedDisplacementMapTexture, &descRTV, &pDisplacementMapRTV[1]);

    if (borderSize > 1)
    {
        m_pDevice->CreateShaderResourceView(*ppDilatedDisplacementMapTexture, &descSRV, &pDisplacementMapSRV[1]);
        m_pDevice->CreateRenderTargetView(*ppDisplacementMapTexture, &descRTV, &pDisplacementMapRTV[0]);
    }
    
    m_pContext->IASetInputLayout(NULL);
    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    D3D11_VIEWPORT viewport = {0, 0, (float)desc.Width, (float)desc.Height, 0, 1};
    m_pContext->RSSetViewports(1, &viewport);
    
    int renderTarget = 1;
    
    for (int i=0; i<borderSize; ++i)
    {
        m_pContext->OMSetRenderTargets(1, &pDisplacementMapRTV[renderTarget], NULL);

        m_pEffect->GetVariableByName("g_DisplacementMap")->AsShaderResource()->SetResource(pDisplacementMapSRV[1-renderTarget]);
        m_pEffect->GetTechniqueByName("DilateDisplacementMap")->GetPassByIndex(0)->Apply(0, m_pContext);
        m_pContext->Draw(4, 0);

        m_pContext->OMSetRenderTargets(0, NULL, NULL);

        renderTarget = 1 - renderTarget;
    }

    SAFE_RELEASE(pDisplacementMapSRV[0]);
    SAFE_RELEASE(pDisplacementMapSRV[1]);
    SAFE_RELEASE(pDisplacementMapRTV[0]);
    SAFE_RELEASE(pDisplacementMapRTV[1]);
}

void WSNormalMap::DownsampleNormalMap(ID3D11Texture2D **ppNormalMapTexture, ID3D11Texture2D **ppDownsampledNormalMapTexture, int ss_scale)
{
    if (!Initialized()) return;
    
    if (!ppNormalMapTexture || !ppDownsampledNormalMapTexture) return;

    ID3D11ShaderResourceView* pNormalMapSRV = NULL;
    ID3D11RenderTargetView*   pNormalMapRTV = NULL;
    
    D3D11_TEXTURE2D_DESC desc;
    (*ppNormalMapTexture)->GetDesc(&desc);

    int srcTileSize = desc.Width;
    int dstTileSize = srcTileSize / ss_scale;
       
    if (*ppDownsampledNormalMapTexture == NULL)
    {
        desc.Width = dstTileSize;
        desc.Height = dstTileSize;
        m_pDevice->CreateTexture2D(&desc, NULL, ppDownsampledNormalMapTexture);
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
    memset(&descSRV, 0, sizeof(descSRV));
    descSRV.Format = desc.Format;
    descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    descSRV.Texture2D.MipLevels = 1;
    m_pDevice->CreateShaderResourceView(*ppNormalMapTexture, &descSRV, &pNormalMapSRV);

    D3D11_RENDER_TARGET_VIEW_DESC descRTV;
    memset(&descRTV, 0, sizeof(descRTV));
    descRTV.Format = descSRV.Format;
    descRTV.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    m_pDevice->CreateRenderTargetView(*ppDownsampledNormalMapTexture, &descRTV, &pNormalMapRTV);
    
    m_pContext->IASetInputLayout(NULL);
    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    D3D11_VIEWPORT viewport = {0, 0, (float)dstTileSize, (float)dstTileSize, 0, 1};
    m_pContext->RSSetViewports(1, &viewport);
    
    m_pContext->OMSetRenderTargets(1, &pNormalMapRTV, NULL);

    m_pEffect->GetVariableByName("g_DownsampleRatio")->AsScalar()->SetInt(ss_scale);
    m_pEffect->GetVariableByName("g_rTexelSize")->AsScalar()->SetFloat(1.0f / srcTileSize);
    m_pEffect->GetVariableByName("g_NormalMap")->AsShaderResource()->SetResource(pNormalMapSRV);
    m_pEffect->GetTechniqueByName("DownsampleNormalMap")->GetPassByIndex(0)->Apply(0, m_pContext);
    m_pContext->Draw(4, 0);

    m_pContext->OMSetRenderTargets(0, NULL, NULL);

    SAFE_RELEASE(pNormalMapSRV);
    SAFE_RELEASE(pNormalMapRTV);
}

void WSNormalMap::DownsampleDisplacementMap(ID3D11Texture2D **ppDisplacementMapTexture, ID3D11Texture2D **ppDownsampledDisplacementMapTexture, int ss_scale)
{
    if (!Initialized()) return;
    
    if (!ppDisplacementMapTexture || !ppDownsampledDisplacementMapTexture) return;

    ID3D11ShaderResourceView* pDisplacementMapSRV = NULL;
    ID3D11RenderTargetView*   pDisplacementMapRTV = NULL;
    
    D3D11_TEXTURE2D_DESC desc;
    (*ppDisplacementMapTexture)->GetDesc(&desc);

    int srcTileSize = desc.Width;
    int dstTileSize = srcTileSize / ss_scale;

    if (*ppDownsampledDisplacementMapTexture == NULL)
    {
        desc.Width = dstTileSize;
        desc.Height = dstTileSize;
        m_pDevice->CreateTexture2D(&desc, NULL, ppDownsampledDisplacementMapTexture);
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
    memset(&descSRV, 0, sizeof(descSRV));
    descSRV.Format = desc.Format;
    descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    descSRV.Texture2D.MipLevels = 1;
    m_pDevice->CreateShaderResourceView(*ppDisplacementMapTexture, &descSRV, &pDisplacementMapSRV);

    D3D11_RENDER_TARGET_VIEW_DESC descRTV;
    memset(&descRTV, 0, sizeof(descRTV));
    descRTV.Format = descSRV.Format;
    descRTV.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    m_pDevice->CreateRenderTargetView(*ppDownsampledDisplacementMapTexture, &descRTV, &pDisplacementMapRTV);
    
    m_pContext->IASetInputLayout(NULL);
    m_pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    D3D11_VIEWPORT viewport = {0, 0, (float)dstTileSize, (float)dstTileSize, 0, 1};
    m_pContext->RSSetViewports(1, &viewport);
    
    m_pContext->OMSetRenderTargets(1, &pDisplacementMapRTV, NULL);

    m_pEffect->GetVariableByName("g_DownsampleRatio")->AsScalar()->SetInt(ss_scale);
    m_pEffect->GetVariableByName("g_rTexelSize")->AsScalar()->SetFloat(1.0f / srcTileSize);
    m_pEffect->GetVariableByName("g_DisplacementMap")->AsShaderResource()->SetResource(pDisplacementMapSRV);
    m_pEffect->GetTechniqueByName("DownsampleDisplacementMap")->GetPassByIndex(0)->Apply(0, m_pContext);
    m_pContext->Draw(4, 0);

    m_pContext->OMSetRenderTargets(0, NULL, NULL);

    SAFE_RELEASE(pDisplacementMapSRV);
    SAFE_RELEASE(pDisplacementMapRTV);
}

void WSNormalMap::Init(ID3D11Device *pDevice, ID3D11DeviceContext *pContext)
{
    if (s_bInited) return;

    HRESULT hr = S_OK;

    ID3DXBuffer* pShader = NULL;
    WCHAR path[1024];
    RETURN_ON_ERROR(DXUTFindDXSDKMediaFileCch(path, MAX_PATH, L"Preprocessor.fx"));
    RETURN_ON_ERROR(D3DXCompileShaderFromFile(path, NULL, NULL, NULL, "fx_5_0", 0, &pShader, NULL, NULL));
    RETURN_ON_ERROR(D3DX11CreateEffectFromMemory(pShader->GetBufferPointer(), pShader->GetBufferSize(), 0, pDevice, &m_pEffect));
    SAFE_RELEASE(pShader);

    RETURN_ON_ERROR(DXUTFindDXSDKMediaFileCch(path, MAX_PATH, L"PreprocessorCS.fx"));
    RETURN_ON_ERROR(D3DXCompileShaderFromFile(path, NULL, NULL, NULL, "fx_5_0", 0, &pShader, NULL, NULL));
    RETURN_ON_ERROR(D3DX11CreateEffectFromMemory(pShader->GetBufferPointer(), pShader->GetBufferSize(), 0, pDevice, &m_pEffectCS));
    SAFE_RELEASE(pShader);

    m_pDevice = pDevice;
    m_pContext = pContext;

    m_pDefaultPass = m_pEffect->GetTechniqueByName("Default")->GetPassByIndex(0);

    s_bInited = true;
}

void WSNormalMap::Destroy()
{
    s_bInited = false;
    
    SAFE_RELEASE(m_pEffect);
    SAFE_RELEASE(m_pEffectCS);
}

void WSNormalMap::DisableTessellation()
{
    if (!Initialized()) return;
    
    m_pDefaultPass->Apply(0, m_pContext);
}

bool WSNormalMap::Initialized()
{
    assert(s_bInited == true);

    return s_bInited;
}
