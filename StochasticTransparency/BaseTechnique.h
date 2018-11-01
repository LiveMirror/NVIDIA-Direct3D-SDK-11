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

#pragma once
#include "SimpleRT.h"
#include "SDKmesh.h"
#include "RandomColors.h"

class BaseTechnique
{
public:
    BaseTechnique(ID3D11Device* pd3dDevice)
        : m_pNoCullRS(NULL)
        , m_pDepthNoStencilDS(NULL)
        , m_pDepthNoWriteDS(NULL)
        , m_pNoDepthNoStencilDS(NULL)
        , m_pFrontToBackBlendBS(NULL)
        , m_pBackToFrontBlendBS(NULL)
        , m_pNoBlendBS(NULL)
        , m_pGeometryVS(NULL)
        , m_pFullScreenTriangleVS(NULL)
        , m_pParamsCB(NULL)
        , m_pShadingParamsCB(NULL)
        , m_pInputLayout(NULL)
        , m_BackgroundColor(D3DXVECTOR3(1.f,1.f,1.f))
    {
        CreateRasterizerState(pd3dDevice);
        CreateDepthStencilStates(pd3dDevice);
        CreateBlendStates(pd3dDevice);
        CreateVertexShaders(pd3dDevice);
        CreateConstantBuffers(pd3dDevice);
    }

    void UpdateMatrices(D3DXMATRIX &ModelViewProj, D3DXMATRIX &ModelViewIT)
    {
        memcpy(&CBData.worldViewProj, &ModelViewProj, sizeof(D3DXMATRIX));
        memcpy(&CBData.worldViewIT, &ModelViewIT, sizeof(D3DXMATRIX));
    }

    ~BaseTechnique()
    {
        SAFE_RELEASE(m_pNoCullRS);
        SAFE_RELEASE(m_pDepthNoStencilDS);
        SAFE_RELEASE(m_pDepthNoWriteDS);
        SAFE_RELEASE(m_pNoDepthNoStencilDS);
        SAFE_RELEASE(m_pFrontToBackBlendBS);
        SAFE_RELEASE(m_pBackToFrontBlendBS);
        SAFE_RELEASE(m_pNoBlendBS);
        SAFE_RELEASE(m_pGeometryVS);
        SAFE_RELEASE(m_pFullScreenTriangleVS);
        SAFE_RELEASE(m_pParamsCB);
        SAFE_RELEASE(m_pShadingParamsCB);
        SAFE_RELEASE(m_pInputLayout);
    }

    void DrawMesh(ID3D11DeviceContext* pd3dImmediateContext, CDXUTSDKMesh &Mesh)
    {
        ++m_NumGeomPasses;

        assert(Mesh.GetNumMeshes() == 1);
        assert(Mesh.GetNumVBs() == 1);

        UINT Strides[1];
        UINT Offsets[1];
        ID3D11Buffer* pVB[1];
        pVB[0] = Mesh.GetVB11(0,0);
        Strides[0] = (UINT)Mesh.GetVertexStride(0,0);
        Offsets[0] = 0;
        pd3dImmediateContext->IASetVertexBuffers(0, 1, pVB, Strides, Offsets);
        pd3dImmediateContext->IASetIndexBuffer(Mesh.GetIB11(0), Mesh.GetIBFormat11(0), 0);

        for (UINT SubsetId = 0; SubsetId < Mesh.GetNumSubsets(0); ++SubsetId)
        {
            SDKMESH_SUBSET* pSubset = Mesh.GetSubset(0, SubsetId);

            D3D11_PRIMITIVE_TOPOLOGY PrimType = CDXUTSDKMesh::GetPrimitiveType11((SDKMESH_PRIMITIVE_TYPE)pSubset->PrimitiveType);
            pd3dImmediateContext->IASetPrimitiveTopology(PrimType);

            D3DXVECTOR3 Color;
            ComputeRandomColor(SubsetId, Color);

            float data[4] = { Color.x, Color.y, Color.z, m_Alpha };
            pd3dImmediateContext->UpdateSubresource(m_pShadingParamsCB, 0, NULL, data, 0, 0);

            pd3dImmediateContext->DrawIndexed( (UINT)pSubset->IndexCount, (UINT)pSubset->IndexStart, (UINT)pSubset->VertexStart );
        }
    }

    virtual void Render(ID3D11DeviceContext* pd3dImmediateContext, ID3D11RenderTargetView *pBackBuffer) = 0;

    static UINT GetNumGeometryPasses()
    {
        return m_NumGeomPasses;
    }

    static void ResetNumGeometryPasses()
    {
        m_NumGeomPasses = 0;
    }

    static void SetAlpha(float alpha)
    {
        m_Alpha = alpha;
    }

    static float GetAlpha()
    {
        return m_Alpha;
    }

protected:
    static UINT m_NumGeomPasses;
    static float m_Alpha;

    void CreateDepthStencilStates(ID3D11Device* pd3dDevice)
    {
        HRESULT hr;

        D3D11_DEPTH_STENCIL_DESC depthstencilState;
        depthstencilState.DepthEnable = TRUE;
        depthstencilState.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depthstencilState.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        depthstencilState.StencilEnable = FALSE;
        V( pd3dDevice->CreateDepthStencilState( &depthstencilState, &m_pDepthNoStencilDS) );

        depthstencilState.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        depthstencilState.DepthEnable = FALSE;
        depthstencilState.StencilEnable = FALSE;
        V( pd3dDevice->CreateDepthStencilState( &depthstencilState, &m_pNoDepthNoStencilDS) );

        depthstencilState.DepthEnable = TRUE;
        depthstencilState.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        depthstencilState.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        depthstencilState.StencilEnable = FALSE;
        pd3dDevice->CreateDepthStencilState( &depthstencilState, &m_pDepthNoWriteDS );
    }

    void CreateRasterizerState(ID3D11Device* pd3dDevice)
    {
        HRESULT hr;

        D3D11_RASTERIZER_DESC rasterizerState;
        rasterizerState.FillMode = D3D11_FILL_SOLID;
        rasterizerState.CullMode = D3D11_CULL_NONE;
        rasterizerState.FrontCounterClockwise = FALSE;
        rasterizerState.DepthBias = FALSE;
        rasterizerState.DepthBiasClamp = 0;
        rasterizerState.SlopeScaledDepthBias = 0;
        rasterizerState.DepthClipEnable = FALSE;
        rasterizerState.ScissorEnable = FALSE;
        rasterizerState.MultisampleEnable = FALSE;
        rasterizerState.AntialiasedLineEnable = FALSE;
        V( pd3dDevice->CreateRasterizerState( &rasterizerState, &m_pNoCullRS ) );
    }

    void CreateBlendStates(ID3D11Device* pd3dDevice)
    {
        HRESULT hr;

        //--------------------------------------------------------------------------------------
        // Front-to-back alpha-blending
        //--------------------------------------------------------------------------------------

        D3D11_BLEND_DESC blendState;
        blendState.AlphaToCoverageEnable = FALSE;
        // If IndependentBlendEnable==FALSE, only the RenderTarget[0] members are used.
        blendState.IndependentBlendEnable = TRUE;
        {
            blendState.RenderTarget[0].BlendEnable = TRUE;
            blendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }
        for (int i = 1; i < 8; ++i)
        {
            blendState.RenderTarget[i].BlendEnable = FALSE;
            blendState.RenderTarget[i].RenderTargetWriteMask = 0;
        }
        blendState.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_ALPHA;
        blendState.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        blendState.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendState.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
        blendState.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        blendState.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        V( pd3dDevice->CreateBlendState( &blendState, &m_pFrontToBackBlendBS ));

        //--------------------------------------------------------------------------------------
        // Back-to-front alpha-blending
        //--------------------------------------------------------------------------------------

        blendState.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendState.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendState.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blendState.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
        blendState.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        blendState.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        V( pd3dDevice->CreateBlendState( &blendState, &m_pBackToFrontBlendBS ));

        //--------------------------------------------------------------------------------------
        // No blending
        //--------------------------------------------------------------------------------------

        blendState.IndependentBlendEnable = FALSE;
        for (int i = 0; i < 8; ++i)
        {
            blendState.RenderTarget[i].BlendEnable = FALSE;
            blendState.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }
        V( pd3dDevice->CreateBlendState( &blendState, &m_pNoBlendBS ));

        //--------------------------------------------------------------------------------------
        // Default blend factor
        //--------------------------------------------------------------------------------------

        m_BlendFactor[0] =
        m_BlendFactor[1] =
        m_BlendFactor[2] =
        m_BlendFactor[3] = 1.0f;
    }

    void CreateVertexShaders(ID3D11Device* pd3dDevice)
    {
        HRESULT hr;

        // Input layout description for all the geometry passes
        const D3D11_INPUT_ELEMENT_DESC InputLayoutDesc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        UINT NumElements = sizeof(InputLayoutDesc)/sizeof(InputLayoutDesc[0]);

        WCHAR ShaderPath[MAX_PATH];
        V( DXUTFindDXSDKMediaFileCch(ShaderPath, MAX_PATH, L"BaseTechnique.hlsl") );

        // Vertex shader and input layout for the geometry passes
        LPD3DXBUFFER pBlob;
        V( D3DXCompileShaderFromFile(ShaderPath, NULL, NULL, "GeometryVS", "vs_5_0", 0, &pBlob, NULL, NULL ));
        V( pd3dDevice->CreateVertexShader((DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pGeometryVS ));
        V( pd3dDevice->CreateInputLayout(InputLayoutDesc, NumElements, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), &m_pInputLayout));
        pBlob->Release();

        // Vertex shader for the full-screen passes
        V( D3DXCompileShaderFromFile(ShaderPath, NULL, NULL, "FullScreenTriangleVS", "vs_5_0", 0, &pBlob, NULL, NULL) );
        V( pd3dDevice->CreateVertexShader((DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pFullScreenTriangleVS) );
        pBlob->Release();
    }

    void CreateConstantBuffers(ID3D11Device* pd3dDevice)
    {
        HRESULT hr;

        D3D11_BUFFER_DESC cbDesc;
        cbDesc.ByteWidth = sizeof(CBData);
        cbDesc.Usage = D3D11_USAGE_DEFAULT;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = 0;
        cbDesc.MiscFlags = 0;
        cbDesc.StructureByteStride = 0; // new in D3D11
        V( pd3dDevice->CreateBuffer(&cbDesc, NULL, &m_pParamsCB) );

        cbDesc.ByteWidth = sizeof(float) * 4;
        V( pd3dDevice->CreateBuffer(&cbDesc, NULL, &m_pShadingParamsCB) );
    }

    ID3D11RasterizerState* m_pNoCullRS;
    ID3D11DepthStencilState *m_pDepthNoStencilDS;
    ID3D11DepthStencilState *m_pNoDepthNoStencilDS;
    ID3D11DepthStencilState *m_pDepthNoWriteDS;
    ID3D11BlendState *m_pFrontToBackBlendBS;
    ID3D11BlendState *m_pBackToFrontBlendBS;
    ID3D11BlendState *m_pNoBlendBS;
    ID3D11VertexShader *m_pGeometryVS;
    ID3D11VertexShader *m_pFullScreenTriangleVS;
    ID3D11Buffer *m_pParamsCB;
    ID3D11Buffer *m_pShadingParamsCB;
    ID3D11InputLayout *m_pInputLayout;
    float m_BlendFactor[4];
    D3DXVECTOR3 m_BackgroundColor;

    // With D3D10 and 11, constant buffers need to be float4 aligned
    struct
    {
        // float4 aligned
        D3DXMATRIX worldViewProj;
        D3DXMATRIX worldViewIT;
        // float4 aligned
        UINT randMaskSizePowOf2MinusOne;
        UINT randMaskAlphaValues;
        UINT randomOffset;
        UINT pad;
    } CBData;
};
