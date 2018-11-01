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
#include "BaseTechnique.h"
#include "Scene.h"

#define MAX_DEPTH 1.0f

class DualDepthPeeling : public BaseTechnique, public Scene
{
public:
    DualDepthPeeling(ID3D11Device* pd3dDevice, UINT Width, UINT Height)
        : BaseTechnique(pd3dDevice)
        , m_pFrontBlenderRenderTarget(NULL)
        , m_pBackBlenderRenderTarget(NULL)
        , m_pDDPFirstPassPS(NULL)
        , m_pDDPDepthPeelPS(NULL)
        , m_pDDPBlendingPS(NULL)
        , m_pDDPFinalPS(NULL)
        , m_pDualDepthPeelingBS(NULL)
        , m_pMaxBlendBS(NULL)
        , m_NumDualPasses(3)
    {
        CreateRenderTargets(pd3dDevice, Width, Height);
        CreateBlendStates(pd3dDevice);
        CreateShaders(pd3dDevice);
    }

    virtual void Render(ID3D11DeviceContext* pd3dImmediateContext, ID3D11RenderTargetView *pBackBuffer)
    {
        if (m_NumDualPasses == 0) return;

        float ClearColorFront[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        pd3dImmediateContext->ClearRenderTargetView( m_pFrontBlenderRenderTarget->pRTV, ClearColorFront );

        float ClearColorBack[4] = { m_BackgroundColor.x, m_BackgroundColor.y, m_BackgroundColor.z, 0 };
        pd3dImmediateContext->ClearRenderTargetView( m_pBackBlenderRenderTarget->pRTV, ClearColorBack );

        float ClearColorMinZ[4] = { -MAX_DEPTH, -MAX_DEPTH, 0, 0 };
        pd3dImmediateContext->ClearRenderTargetView( m_pMinMaxZRenderTargets[0]->pRTV, ClearColorMinZ );

        pd3dImmediateContext->UpdateSubresource(m_pParamsCB, 0, NULL, &CBData, 0, 0);

        // Set shared state

        pd3dImmediateContext->IASetInputLayout(m_pInputLayout);
        pd3dImmediateContext->VSSetShader(m_pGeometryVS, NULL, 0);
        pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
        pd3dImmediateContext->RSSetState(m_pNoCullRS);
        pd3dImmediateContext->PSSetShader(m_pDDPFirstPassPS, NULL, 0);
        pd3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pParamsCB);
        pd3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pParamsCB);
        pd3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pShadingParamsCB);

        // 1. Initialize Min-Max Z render target

        pd3dImmediateContext->OMSetRenderTargets(1, &m_pMinMaxZRenderTargets[0]->pRTV, NULL);
        pd3dImmediateContext->OMSetDepthStencilState(m_pNoDepthNoStencilDS, 0);
        pd3dImmediateContext->OMSetBlendState(m_pMaxBlendBS, m_BlendFactor, 0xffffffff);
        DrawMesh(pd3dImmediateContext, m_Mesh);

        // 2. Dual Depth Peeling

        UINT currId = 0;
        for (UINT layer = 1; layer < m_NumDualPasses; ++layer)
        {
            currId = layer % 2;
            UINT prevId = 1 - currId;

            pd3dImmediateContext->ClearRenderTargetView( m_pMinMaxZRenderTargets[currId]->pRTV, ClearColorMinZ );

            ID3D11RenderTargetView *MRTs[3] = {
                m_pMinMaxZRenderTargets[currId]->pRTV,
                m_pFrontBlenderRenderTarget->pRTV,
                m_pBackBlenderRenderTarget->pRTV,
            };
            pd3dImmediateContext->OMSetRenderTargets(3, MRTs, NULL);

            pd3dImmediateContext->VSSetShader(m_pGeometryVS, NULL, 0);
            pd3dImmediateContext->PSSetShader(m_pDDPDepthPeelPS, NULL, 0);
            pd3dImmediateContext->PSSetShaderResources(0, 1, &m_pMinMaxZRenderTargets[prevId]->pSRV);
            pd3dImmediateContext->OMSetBlendState( m_pDualDepthPeelingBS, m_BlendFactor, 0xffffffff );

            DrawMesh(pd3dImmediateContext, m_Mesh);
        }

        // 3. Final full-screen pass

        pd3dImmediateContext->OMSetRenderTargets( 1, &pBackBuffer, NULL );
        pd3dImmediateContext->OMSetBlendState( m_pNoBlendBS, m_BlendFactor, 0xffffffff );

        pd3dImmediateContext->VSSetShader(m_pFullScreenTriangleVS, NULL, 0);
        pd3dImmediateContext->PSSetShader(m_pDDPFinalPS, NULL, 0);

        ID3D11ShaderResourceView *pSRVs[3] =
        {
            m_pMinMaxZRenderTargets[currId]->pSRV,
            m_pFrontBlenderRenderTarget->pSRV,
            m_pBackBlenderRenderTarget->pSRV
        };
        pd3dImmediateContext->PSSetShaderResources(0, 3, pSRVs);

        pd3dImmediateContext->Draw(3, 0);
    }

    ~DualDepthPeeling()
    {
        for (int i = 0; i < 2; ++i)
        {
            SAFE_DELETE(m_pMinMaxZRenderTargets[i]);
        }

        SAFE_DELETE(m_pFrontBlenderRenderTarget);
        SAFE_DELETE(m_pBackBlenderRenderTarget);
        SAFE_RELEASE(m_pDDPFirstPassPS);
        SAFE_RELEASE(m_pDDPDepthPeelPS);
        SAFE_RELEASE(m_pDDPBlendingPS);
        SAFE_RELEASE(m_pDDPFinalPS);
        SAFE_RELEASE(m_pDualDepthPeelingBS);
        SAFE_RELEASE(m_pMaxBlendBS);
    }

    void SetNumGeometryPasses(UINT n)
    {
        m_NumDualPasses = n;
    }

protected:
    void CreateRenderTargets(ID3D11Device* pd3dDevice, UINT Width, UINT Height)
    {
        D3D11_TEXTURE2D_DESC texDesc;
        texDesc.Width = Width;
        texDesc.Height = Height;
        texDesc.ArraySize = 1;
        texDesc.MiscFlags = 0;
        texDesc.MipLevels = 1;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.CPUAccessFlags = NULL;

        for (int i = 0; i < 2; ++i)
        {
            m_pMinMaxZRenderTargets[i] = new SimpleRT(pd3dDevice, &texDesc, DXGI_FORMAT_R32G32_FLOAT);
        }

        m_pFrontBlenderRenderTarget = new SimpleRT(pd3dDevice, &texDesc, DXGI_FORMAT_R8G8B8A8_UNORM);
        m_pBackBlenderRenderTarget = new SimpleRT(pd3dDevice, &texDesc, DXGI_FORMAT_R8G8B8A8_UNORM);
    }

    void CreateBlendStates(ID3D11Device* pd3dDevice)
    {
        HRESULT hr;

        D3D11_BLEND_DESC blendState;
        blendState.AlphaToCoverageEnable = FALSE;
        // If IndependentBlendEnable==FALSE, only the RenderTarget[0] members are used.
        blendState.IndependentBlendEnable = TRUE;
        for (int i = 0; i < 3; ++i)
        {
            blendState.RenderTarget[i].BlendEnable = TRUE;
            blendState.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }
        for (int i = 3; i < 8; ++i)
        {
            blendState.RenderTarget[i].BlendEnable = FALSE;
            blendState.RenderTarget[i].RenderTargetWriteMask = 0;
        }

        // Max blending
        blendState.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        blendState.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        blendState.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MAX;
        blendState.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendState.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        blendState.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_MAX;

        // Front-to-back blending
        blendState.RenderTarget[1].SrcBlend = D3D11_BLEND_DEST_ALPHA;
        blendState.RenderTarget[1].DestBlend = D3D11_BLEND_ONE;
        blendState.RenderTarget[1].BlendOp = D3D11_BLEND_OP_ADD;
        blendState.RenderTarget[1].SrcBlendAlpha = D3D11_BLEND_ZERO;
        blendState.RenderTarget[1].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        blendState.RenderTarget[1].BlendOpAlpha = D3D11_BLEND_OP_ADD;

        // Back-to-front blending
        blendState.RenderTarget[2].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendState.RenderTarget[2].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendState.RenderTarget[2].BlendOp = D3D11_BLEND_OP_ADD;
        blendState.RenderTarget[2].SrcBlendAlpha = D3D11_BLEND_ZERO;
        blendState.RenderTarget[2].DestBlendAlpha = D3D11_BLEND_ONE;
        blendState.RenderTarget[2].BlendOpAlpha = D3D11_BLEND_OP_ADD;

        V( pd3dDevice->CreateBlendState( &blendState, &m_pDualDepthPeelingBS ));

        // Max blending

        for (int i = 0; i < 3; ++i)
        {
            blendState.RenderTarget[i].BlendEnable = TRUE;
            blendState.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            blendState.RenderTarget[i].SrcBlend = D3D11_BLEND_ONE;
            blendState.RenderTarget[i].DestBlend = D3D11_BLEND_ONE;
            blendState.RenderTarget[i].BlendOp = D3D11_BLEND_OP_MAX;
            blendState.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
            blendState.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ONE;
            blendState.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_MAX;
        }
        V( pd3dDevice->CreateBlendState( &blendState, &m_pMaxBlendBS ));
    }

    void CreateShaders(ID3D11Device* pd3dDevice)
    {
        HRESULT hr;

        WCHAR ShaderPath[MAX_PATH];
        V( DXUTFindDXSDKMediaFileCch( ShaderPath, MAX_PATH, L"DualDepthPeeling11.hlsl" ) );

        LPD3DXBUFFER pBlob;
        V( D3DXCompileShaderFromFile(ShaderPath, NULL, NULL, "DDPFirstPassPS", "ps_5_0", 0, &pBlob, NULL, NULL ) );
        V( pd3dDevice->CreatePixelShader( (DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pDDPFirstPassPS ) );
        pBlob->Release();

        V( D3DXCompileShaderFromFile(ShaderPath, NULL, NULL, "DDPDepthPeelPS", "ps_5_0", 0, &pBlob, NULL, NULL ) );
        V( pd3dDevice->CreatePixelShader( (DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pDDPDepthPeelPS ) );
        pBlob->Release();

        V( D3DXCompileShaderFromFile(ShaderPath, NULL, NULL, "DDPBlendingPS", "ps_5_0", 0, &pBlob, NULL, NULL ) );
        V( pd3dDevice->CreatePixelShader( (DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pDDPBlendingPS ) );
        pBlob->Release();

        V( D3DXCompileShaderFromFile(ShaderPath, NULL, NULL, "DDPFinalPS", "ps_5_0", 0, &pBlob, NULL, NULL ) );
        V( pd3dDevice->CreatePixelShader( (DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pDDPFinalPS ) );
        pBlob->Release();
    }

    SimpleRT *m_pMinMaxZRenderTargets[2];
    SimpleRT *m_pFrontBlenderRenderTarget;
    SimpleRT *m_pBackBlenderRenderTarget;
    ID3D11PixelShader *m_pDDPFirstPassPS;
    ID3D11PixelShader *m_pDDPDepthPeelPS;
    ID3D11PixelShader *m_pDDPBlendingPS;
    ID3D11PixelShader *m_pDDPFinalPS;
    ID3D11BlendState *m_pDualDepthPeelingBS;
    ID3D11BlendState *m_pMaxBlendBS;
    UINT m_NumDualPasses;
};
