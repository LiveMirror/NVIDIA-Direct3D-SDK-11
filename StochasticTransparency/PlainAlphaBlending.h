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

#define NUM_MSAA_SAMPLES 8

class PlainAlphaBlending : public BaseTechnique, public Scene
{
public:
    PlainAlphaBlending(ID3D11Device* pd3dDevice, UINT Width, UINT Height)
        : BaseTechnique(pd3dDevice)
        , m_pShadingPS(NULL)
        , m_pFinalPS(NULL)
        , m_pColorRenderTarget(NULL)
        , m_pColorRenderTarget1xAA(NULL)
        , m_pDepthBuffer(NULL)
    {
        CreateRenderTargets(pd3dDevice, Width, Height);
        CreateDepthBuffer(pd3dDevice, Width, Height);
        CreateShaders(pd3dDevice);
    }

    virtual void Render(ID3D11DeviceContext* pd3dImmediateContext, ID3D11RenderTargetView *pBackBuffer)
    {
        // In a real application, the MSAA color and depth buffers would be initialized with opaque geometry
        float ClearColorBack[4] = { m_BackgroundColor.x, m_BackgroundColor.y, m_BackgroundColor.z, 0 };
        pd3dImmediateContext->ClearRenderTargetView( m_pColorRenderTarget->pRTV, ClearColorBack );
        pd3dImmediateContext->ClearDepthStencilView( m_pDepthBuffer->pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );

        // Update the constant buffer
        pd3dImmediateContext->UpdateSubresource(m_pParamsCB, 0, NULL, &CBData, 0, 0);

        // Set shared states
        pd3dImmediateContext->IASetInputLayout(m_pInputLayout);
        pd3dImmediateContext->VSSetShader(m_pGeometryVS, NULL, 0);
        pd3dImmediateContext->GSSetShader(NULL, NULL, 0);
        pd3dImmediateContext->RSSetState(m_pNoCullRS);
        pd3dImmediateContext->VSSetConstantBuffers(0, 1, &m_pParamsCB);
        pd3dImmediateContext->PSSetConstantBuffers(0, 1, &m_pParamsCB);
        pd3dImmediateContext->PSSetConstantBuffers(1, 1, &m_pShadingParamsCB);

        //----------------------------------------------------------------------------------
        // Plain alpha blending with MSAA
        //----------------------------------------------------------------------------------

        pd3dImmediateContext->OMSetRenderTargets(1, &m_pColorRenderTarget->pRTV, m_pDepthBuffer->pDSV);
        pd3dImmediateContext->OMSetBlendState(m_pBackToFrontBlendBS, m_BlendFactor, 0xffffffff);
        pd3dImmediateContext->OMSetDepthStencilState(m_pDepthNoWriteDS, 0);
        pd3dImmediateContext->PSSetShader(m_pShadingPS, NULL, 0);

        DrawMesh(pd3dImmediateContext, m_Mesh);

        //----------------------------------------------------------------------------------
        // Resolve colors
        //----------------------------------------------------------------------------------

        pd3dImmediateContext->ResolveSubresource(m_pColorRenderTarget1xAA->pTexture, 0,
                                                 m_pColorRenderTarget->pTexture, 0,
                                                 DXGI_FORMAT_R16G16B16A16_FLOAT);

        //----------------------------------------------------------------------------------
        // Final full-screen pass, blending the transparent colors over the background
        //----------------------------------------------------------------------------------

        pd3dImmediateContext->OMSetRenderTargets(1, &pBackBuffer, NULL);
        pd3dImmediateContext->OMSetDepthStencilState(m_pNoDepthNoStencilDS, 0);
        pd3dImmediateContext->OMSetBlendState(m_pNoBlendBS, m_BlendFactor, 0xffffffff);

        pd3dImmediateContext->VSSetShader(m_pFullScreenTriangleVS, NULL, 0);
        pd3dImmediateContext->PSSetShader(m_pFinalPS, NULL, 0);
        pd3dImmediateContext->PSSetShaderResources(0, 1, &m_pColorRenderTarget1xAA->pSRV);

        pd3dImmediateContext->Draw(3, 0);
    }

    ~PlainAlphaBlending()
    {
        SAFE_RELEASE(m_pShadingPS);
        SAFE_RELEASE(m_pFinalPS);
        SAFE_DELETE(m_pColorRenderTarget);
        SAFE_DELETE(m_pColorRenderTarget1xAA);
        SAFE_DELETE(m_pDepthBuffer);
    }

protected:
    void CreateShaders(ID3D11Device* pd3dDevice)
    {
        HRESULT hr;

        WCHAR ShaderPath[MAX_PATH];
        V( DXUTFindDXSDKMediaFileCch( ShaderPath, MAX_PATH, L"PlainAlphaBlending.hlsl" ) );

        LPD3DXBUFFER pBlob;
        V( D3DXCompileShaderFromFile( ShaderPath, NULL, NULL, "ShadingPS", "ps_5_0", 0, &pBlob, NULL, NULL ) );
        V( pd3dDevice->CreatePixelShader( (DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pShadingPS ) );
        pBlob->Release();

        V( D3DXCompileShaderFromFile( ShaderPath, NULL, NULL, "FinalPS", "ps_5_0", 0, &pBlob, NULL, NULL ) );
        V( pd3dDevice->CreatePixelShader( (DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pFinalPS ) );
        pBlob->Release();
    }

    void CreateRenderTargets(ID3D11Device* pd3dDevice, UINT Width, UINT Height)
    {
        D3D11_TEXTURE2D_DESC texDesc;
        texDesc.Width = Width;
        texDesc.Height = Height;
        texDesc.ArraySize = 1;
        texDesc.MiscFlags = 0;
        texDesc.MipLevels = 1;
        texDesc.SampleDesc.Count = NUM_MSAA_SAMPLES;
        texDesc.SampleDesc.Quality = 0;
        texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.CPUAccessFlags = NULL;
        m_pColorRenderTarget = new SimpleRT(pd3dDevice, &texDesc, DXGI_FORMAT_R16G16B16A16_FLOAT);

        texDesc.SampleDesc.Count = 1;
        m_pColorRenderTarget1xAA = new SimpleRT(pd3dDevice, &texDesc, DXGI_FORMAT_R16G16B16A16_FLOAT);
    }

    void CreateDepthBuffer(ID3D11Device* pd3dDevice, UINT Width, UINT Height)
    {
        D3D11_TEXTURE2D_DESC texDesc;
        texDesc.ArraySize          = 1;
        texDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL;
        texDesc.CPUAccessFlags     = NULL;
        texDesc.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT;
        texDesc.Width              = Width;
        texDesc.Height             = Height;
        texDesc.MipLevels          = 1;
        texDesc.MiscFlags          = NULL;
        texDesc.SampleDesc.Count   = NUM_MSAA_SAMPLES;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage              = D3D11_USAGE_DEFAULT;
        m_pDepthBuffer = new SimpleDepthStencil(pd3dDevice, &texDesc);
    }

    ID3D11PixelShader* m_pShadingPS;
    ID3D11PixelShader* m_pFinalPS;
    SimpleRT *m_pColorRenderTarget;
    SimpleRT *m_pColorRenderTarget1xAA;
    SimpleDepthStencil *m_pDepthBuffer;
};
