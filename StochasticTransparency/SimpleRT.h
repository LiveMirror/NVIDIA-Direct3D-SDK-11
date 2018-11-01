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

// Encapsulates a Texture2D render target and its associated
// render target view (for rendering into the texture) and
// shader resource view (for binding the texture to a shader).
class SimpleRT
{
public:
    ID3D11Texture2D* pTexture;
    ID3D11RenderTargetView* pRTV;
    ID3D11ShaderResourceView* pSRV;

    SimpleRT(ID3D11Device* pd3dDevice, D3D11_TEXTURE2D_DESC* pTexDesc, DXGI_FORMAT Format)
        : pTexture(NULL)
        , pRTV(NULL)
        , pSRV(NULL)
    {
        pTexDesc->Format = Format;

        HRESULT hr;
        V( pd3dDevice->CreateTexture2D(pTexDesc, NULL, &pTexture) );
        V( pd3dDevice->CreateShaderResourceView(pTexture, NULL, &pSRV) );
        V( pd3dDevice->CreateRenderTargetView(pTexture, NULL, &pRTV) );
    }

    ~SimpleRT()
    {
        SAFE_RELEASE(pTexture);
        SAFE_RELEASE(pRTV);
        SAFE_RELEASE(pSRV);
    }
};

// Encapsulates a Texture2DArray render target and its associated
// render target views (for rendering into the slices of the array) and
// shader resource view (for binding the entire texture array to a shader).
class SimpleRTArray : public SimpleRT
{
public:
    static const UINT MaxNumLayers = 8;
    ID3D11RenderTargetView* pRTVs[MaxNumLayers];

    SimpleRTArray(ID3D11Device* pd3dDevice, D3D11_TEXTURE2D_DESC* pTexDesc, DXGI_FORMAT Format)
        : SimpleRT(pd3dDevice, pTexDesc, Format)
        , m_ArraySize(pTexDesc->ArraySize)
    {
        assert(m_ArraySize <= MaxNumLayers);

        for (UINT LayerId = 0; LayerId < m_ArraySize; ++LayerId)
        {
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
            rtvDesc.Format = Format;
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.MipSlice = 0;
            rtvDesc.Texture2DArray.FirstArraySlice = LayerId;
            rtvDesc.Texture2DArray.ArraySize = 1;

            HRESULT hr;
            V( pd3dDevice->CreateRenderTargetView(pTexture, &rtvDesc, &pRTVs[LayerId]) );
        }
    }

    ~SimpleRTArray()
    {
        for (UINT LayerId = 0; LayerId < m_ArraySize; ++LayerId)
        {
            SAFE_RELEASE(pRTVs[LayerId]);
        }
    }
protected:
    UINT m_ArraySize;
};

// Encapsulates a Texture2D depth-stencil buffer (D24_UNORM_S8_UINT or D32_FLOAT)
// and its associated depth-stencil view for binding it as depth buffer.
class SimpleDepthStencil
{
public:
    ID3D11Texture2D* pTexture;
    ID3D11DepthStencilView* pDSV;

    SimpleDepthStencil( ID3D11Device* pd3dDevice, D3D11_TEXTURE2D_DESC* pTexDesc )
       : pTexture(NULL)
       , pDSV(NULL)
    {
        HRESULT hr;
        V( pd3dDevice->CreateTexture2D(pTexDesc, NULL, &pTexture) );
        V( pd3dDevice->CreateDepthStencilView(pTexture, NULL, &pDSV) );
    }

    ~SimpleDepthStencil()
    {
        SAFE_RELEASE(pTexture);
        SAFE_RELEASE(pDSV);
    }
};
