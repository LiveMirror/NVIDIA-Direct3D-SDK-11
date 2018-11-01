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
#include "MersenneTwister.h"
#include <algorithm>

#define RANDOM_SIZE 2048
#define ALPHA_VALUES 256
#define NUM_MSAA_SAMPLES 8
#define MAX_NUM_PASSES 8

// Do not use DXGI_FORMAT_R8G8B8A8_UNORM because it would cause significant bias
// in the destination alpha of the stochastic color buffers
#define STOCHASTIC_COLOR_FORMAT DXGI_FORMAT_R16G16B16A16_FLOAT

class StochasticTransparency : public BaseTechnique, public Scene
{
public:
    StochasticTransparency(ID3D11Device* pd3dDevice, UINT Width, UINT Height)
        : BaseTechnique(pd3dDevice)
        , m_pStochTransparencyColorPS(NULL)
        , m_pStochTransparencyDepthPS(NULL)
        , m_pRndTexture(NULL)
        , m_pRndTextureSRV(NULL)
        , m_pTransmittanceRenderTarget(NULL)
        , m_pStochasticColorRenderTarget(NULL)
        , m_pStochasticColorRenderTarget1xAA(NULL)
        , m_pBackgroundRenderTarget(NULL)
        , m_pAdditiveBlendingBS(NULL)
        , m_pTransmittanceBS(NULL)
        , m_pStochasticTransparencyAlphaPS(NULL)
        , m_pDepthBuffer(NULL)
        , m_NumPasses(1)
    {
        memset(m_pStochasticTransparencyFinalPS, 0, sizeof(m_pStochasticTransparencyFinalPS));
        CreateRenderTargets(pd3dDevice, Width, Height);
        CreateDepthBuffer(pd3dDevice, Width, Height);
        CreateRandomBitmasks(pd3dDevice);
        CreateBlendStates(pd3dDevice);
        CreateShaders(pd3dDevice);
        CBData.randMaskSizePowOf2MinusOne = RANDOM_SIZE - 1;
        CBData.randMaskAlphaValues = ALPHA_VALUES;
    }

    virtual void Render(ID3D11DeviceContext* pd3dImmediateContext, ID3D11RenderTargetView *pBackBuffer)
    {
        // The MSAA background colors should be initialized by drawing the opaque objects in the scene.
        float ClearColorBack[4] = { m_BackgroundColor.x, m_BackgroundColor.y, m_BackgroundColor.z, 0 };
        pd3dImmediateContext->ClearRenderTargetView( m_pBackgroundRenderTarget->pRTV, ClearColorBack );

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
        // 1. Render total transmittance
        //----------------------------------------------------------------------------------

        float ClearColorAlpha[4] = { 1.0f };
        pd3dImmediateContext->ClearRenderTargetView(m_pTransmittanceRenderTarget->pRTV, ClearColorAlpha);

        pd3dImmediateContext->OMSetRenderTargets(1, &m_pTransmittanceRenderTarget->pRTV, NULL);
        pd3dImmediateContext->OMSetBlendState(m_pTransmittanceBS, m_BlendFactor, 0xffffffff);
        pd3dImmediateContext->OMSetDepthStencilState(m_pNoDepthNoStencilDS, 0);

        pd3dImmediateContext->PSSetShader(m_pStochasticTransparencyAlphaPS, NULL, 0);

        DrawMesh(pd3dImmediateContext, m_Mesh);

        for (UINT LayerId = 0; LayerId < m_NumPasses; ++LayerId)
        {
            CBData.randomOffset = LayerId;
            pd3dImmediateContext->UpdateSubresource(m_pParamsCB, 0, NULL, &CBData, 0, 0);

            //----------------------------------------------------------------------------------
            // 2. Render MSAA "stochastic depths", writting SV_Coverage in the pixel shader
            //----------------------------------------------------------------------------------

            pd3dImmediateContext->ClearDepthStencilView(m_pDepthBuffer->pDSV, D3D11_CLEAR_DEPTH, 1.0, 0);

            pd3dImmediateContext->OMSetRenderTargets(0, NULL, m_pDepthBuffer->pDSV);
            pd3dImmediateContext->OMSetBlendState(m_pNoBlendBS, m_BlendFactor, 0xffffffff);
            pd3dImmediateContext->OMSetDepthStencilState(m_pDepthNoStencilDS, 0);

            pd3dImmediateContext->PSSetShader(m_pStochTransparencyDepthPS, NULL, 0);
            pd3dImmediateContext->PSSetShaderResources(0, 1, &m_pRndTextureSRV);

            DrawMesh(pd3dImmediateContext, m_Mesh);

            //----------------------------------------------------------------------------------
            // 3. Render colors in front of the stochastic depths with additive blending
            //----------------------------------------------------------------------------------

            float Zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            pd3dImmediateContext->ClearRenderTargetView(m_pStochasticColorRenderTarget->pRTV, Zero);

            pd3dImmediateContext->OMSetRenderTargets(1, &m_pStochasticColorRenderTarget->pRTV, m_pDepthBuffer->pDSV);
            pd3dImmediateContext->OMSetBlendState(m_pAdditiveBlendingBS, m_BlendFactor, 0xffffffff);
            pd3dImmediateContext->OMSetDepthStencilState(m_pDepthNoWriteDS, 0);

            pd3dImmediateContext->PSSetShader(m_pStochTransparencyColorPS, NULL, 0);

            DrawMesh(pd3dImmediateContext, m_Mesh);

            //----------------------------------------------------------------------------------
            // 4. Resolve MSAA colors
            //----------------------------------------------------------------------------------

            UINT MipSlice = 0;
            UINT MipLevels = 1;
            UINT DstSubresource = D3D11CalcSubresource(MipSlice, LayerId, MipLevels);
            pd3dImmediateContext->ResolveSubresource(m_pStochasticColorRenderTarget1xAA->pTexture, DstSubresource,
                                                     m_pStochasticColorRenderTarget->pTexture, 0,
                                                     STOCHASTIC_COLOR_FORMAT);
        }

        //----------------------------------------------------------------------------------
        // 5. Final full-screen pass, blending the transparent colors over the background
        //----------------------------------------------------------------------------------

        pd3dImmediateContext->OMSetRenderTargets(1, &pBackBuffer, NULL);
        pd3dImmediateContext->OMSetDepthStencilState(m_pNoDepthNoStencilDS, 0);
        pd3dImmediateContext->OMSetBlendState(m_pNoBlendBS, m_BlendFactor, 0xffffffff);

        pd3dImmediateContext->VSSetShader(m_pFullScreenTriangleVS, NULL, 0);
        pd3dImmediateContext->PSSetShader(m_pStochasticTransparencyFinalPS[m_NumPasses-1], NULL, 0);

        ID3D11ShaderResourceView *pSRVs[3] =
        {
            m_pStochasticColorRenderTarget1xAA->pSRV,
            m_pTransmittanceRenderTarget->pSRV,
            m_pBackgroundRenderTarget->pSRV
        };
        pd3dImmediateContext->PSSetShaderResources(0, 3, pSRVs);

        pd3dImmediateContext->Draw(3, 0);
    }

    void SetNumPasses(UINT NumPasses)
    {
        m_NumPasses = min(NumPasses, MAX_NUM_PASSES);
    }

    ~StochasticTransparency()
    {
        SAFE_RELEASE(m_pStochTransparencyColorPS);
        SAFE_RELEASE(m_pStochTransparencyDepthPS);
        SAFE_RELEASE(m_pStochasticTransparencyAlphaPS);
        for (UINT i = 0; i < MAX_NUM_PASSES; ++i)
        {
            SAFE_RELEASE(m_pStochasticTransparencyFinalPS[i]);
        }
        SAFE_RELEASE(m_pRndTexture);
        SAFE_RELEASE(m_pRndTextureSRV);
        SAFE_RELEASE(m_pAdditiveBlendingBS);
        SAFE_RELEASE(m_pTransmittanceBS);
        SAFE_RELEASE(m_pDepthNoWriteDS);
        SAFE_DELETE(m_pTransmittanceRenderTarget);
        SAFE_DELETE(m_pStochasticColorRenderTarget);
        SAFE_DELETE(m_pStochasticColorRenderTarget1xAA);
        SAFE_DELETE(m_pBackgroundRenderTarget);
        SAFE_DELETE(m_pDepthBuffer);
    }

protected:
    void CreateShaders(ID3D11Device* pd3dDevice)
    {
        HRESULT hr;

        WCHAR ShaderPath[MAX_PATH];
        V( DXUTFindDXSDKMediaFileCch( ShaderPath, MAX_PATH, L"StochasticTransparency11.hlsl" ) );

        LPD3DXBUFFER pBlob;
        V( D3DXCompileShaderFromFile( ShaderPath, NULL, NULL, "StochasticTransparencyAlphaPS", "ps_5_0", 0, &pBlob, NULL, NULL ) );
        V( pd3dDevice->CreatePixelShader( (DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pStochasticTransparencyAlphaPS ) );
        pBlob->Release();

        V( D3DXCompileShaderFromFile( ShaderPath, NULL, NULL, "StochasticTransparencyDepthPS", "ps_5_0", 0, &pBlob, NULL, NULL ) );
        V( pd3dDevice->CreatePixelShader( (DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pStochTransparencyDepthPS ) );
        pBlob->Release();

        V( D3DXCompileShaderFromFile( ShaderPath, NULL, NULL, "StochasticTransparencyColorPS", "ps_5_0", 0, &pBlob, NULL, NULL ) );
        V( pd3dDevice->CreatePixelShader( (DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pStochTransparencyColorPS ) );
        pBlob->Release();

        for (UINT ShaderId = 0; ShaderId < MAX_NUM_PASSES; ++ShaderId)
        {
            CHAR str[100];
            sprintf_s(str, sizeof(str), "StochasticTransparencyComposite%dPS", ShaderId+1);

            V( D3DXCompileShaderFromFile( ShaderPath, NULL, NULL, str, "ps_5_0", 0, &pBlob, NULL, NULL ) );
            V( pd3dDevice->CreatePixelShader( (DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pStochasticTransparencyFinalPS[ShaderId] ) );
            pBlob->Release();
        }
    }

    void CreateBlendStates(ID3D11Device* pd3dDevice)
    {
        D3D11_BLEND_DESC BlendStateDesc;
        BlendStateDesc.AlphaToCoverageEnable = FALSE;
        BlendStateDesc.IndependentBlendEnable = FALSE;
        for (int i = 0; i < 8; ++i)
        {
            BlendStateDesc.RenderTarget[i].BlendEnable = FALSE;
            BlendStateDesc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        }

        BlendStateDesc.RenderTarget[0].BlendEnable = TRUE;
        BlendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        // Create m_pAdditiveBlendingBS
        // dst.rgba += src.rgba

        BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

        BlendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        BlendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        BlendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;

        pd3dDevice->CreateBlendState(&BlendStateDesc, &m_pAdditiveBlendingBS);

        // Create m_pTransmittanceBS
        // dst.r *= (1.0 - src.r)

        BlendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ZERO;
        BlendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_COLOR;
        BlendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;

        pd3dDevice->CreateBlendState(&BlendStateDesc, &m_pTransmittanceBS);
    }

    void CreateRandomBitmasks(ID3D11Device* pd3dDevice)
    {
        MTRand rng;
        rng.seed((unsigned)0);

        int numbers[NUM_MSAA_SAMPLES];
        unsigned int *allmasks = new unsigned int[RANDOM_SIZE * (ALPHA_VALUES + 1)];

        for (int y = 0; y <= ALPHA_VALUES; y++) // Inclusive, we need alpha = 1.0
        {
            for (int x = 0; x < RANDOM_SIZE; x++)
            {
                // Initialize array
                for (int i = 0; i < NUM_MSAA_SAMPLES; i++)
                {
                    numbers[i] = i;
                }

                // Scramble!
                for (int i = 0; i < NUM_MSAA_SAMPLES * 2; i++)
                {
                    std::swap(numbers[rng.randInt() % NUM_MSAA_SAMPLES], numbers[rng.randInt() % NUM_MSAA_SAMPLES]);
                }

                // Create the mask
                unsigned int mask = 0;
                float nof_bits_to_set = (float(y) / float(ALPHA_VALUES)) * NUM_MSAA_SAMPLES;
                for (int bit = 0; bit < int(nof_bits_to_set); bit++)
                {
                    mask |= (1 << numbers[bit]);
                }
                float prob_of_last_bit = (nof_bits_to_set - floor(nof_bits_to_set));
                if (rng.randExc() < prob_of_last_bit)
                {
                    mask |= (1 << numbers[int(nof_bits_to_set)]);
                }

                allmasks[y * RANDOM_SIZE + x] = mask;
            }
        }

        D3D11_TEXTURE2D_DESC texDesc;
        texDesc.Width            = RANDOM_SIZE;
        texDesc.Height           = ALPHA_VALUES + 1;
        texDesc.MipLevels        = 1;
        texDesc.ArraySize        = 1;
        texDesc.Format           = DXGI_FORMAT_R32_UINT;
        texDesc.SampleDesc.Count = 1;
        texDesc.SampleDesc.Quality = 0;
        texDesc.Usage            = D3D11_USAGE_IMMUTABLE;
        texDesc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags   = 0;
        texDesc.MiscFlags        = 0;

        D3D11_SUBRESOURCE_DATA srDesc;
        srDesc.pSysMem          = allmasks;
        srDesc.SysMemPitch      = texDesc.Width * sizeof(unsigned int);
        srDesc.SysMemSlicePitch = 0;

        SAFE_RELEASE(m_pRndTexture);
        pd3dDevice->CreateTexture2D(&texDesc, &srDesc, &m_pRndTexture);

        SAFE_RELEASE(m_pRndTextureSRV);
        pd3dDevice->CreateShaderResourceView(m_pRndTexture, NULL, &m_pRndTextureSRV);

        delete[] allmasks;
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

        // Create MSAA render targets
        m_pTransmittanceRenderTarget = new SimpleRT(pd3dDevice, &texDesc, DXGI_FORMAT_R8_UNORM);
        m_pStochasticColorRenderTarget = new SimpleRT(pd3dDevice, &texDesc, STOCHASTIC_COLOR_FORMAT);
        m_pBackgroundRenderTarget = new SimpleRT(pd3dDevice, &texDesc, DXGI_FORMAT_R8G8B8A8_UNORM);

        // Create non-MSAA texture array, to store the resolved colors for each pass
        texDesc.SampleDesc.Count = 1;
        texDesc.ArraySize = MAX_NUM_PASSES;
        m_pStochasticColorRenderTarget1xAA = new SimpleRTArray(pd3dDevice, &texDesc, STOCHASTIC_COLOR_FORMAT);
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

    ID3D11PixelShader* m_pStochTransparencyColorPS;
    ID3D11PixelShader* m_pStochTransparencyDepthPS;
    ID3D11PixelShader* m_pStochasticTransparencyAlphaPS;
    ID3D11PixelShader* m_pStochasticTransparencyFinalPS[MAX_NUM_PASSES];
    ID3D11Texture2D *m_pRndTexture;
    ID3D11ShaderResourceView *m_pRndTextureSRV;
    ID3D11BlendState *m_pAdditiveBlendingBS;
    ID3D11BlendState *m_pTransmittanceBS;
    SimpleRT *m_pTransmittanceRenderTarget;
    SimpleRT *m_pStochasticColorRenderTarget;
    SimpleRTArray *m_pStochasticColorRenderTarget1xAA;
    SimpleRT *m_pBackgroundRenderTarget;
    SimpleDepthStencil *m_pDepthBuffer;
    UINT m_NumPasses;
};
