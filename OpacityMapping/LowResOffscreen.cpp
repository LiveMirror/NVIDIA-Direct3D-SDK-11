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

#include "DXUT.h"

#include "LowResOffscreen.h"

#include "Util.h"

CLowResOffscreen::CLowResOffscreen(UINT ReductionInMipLevels) :
    m_ReductionInMipLevels(ReductionInMipLevels),
	m_UpsampleFilterDepthThreshold(0.0002f),
    m_pEffectPass_LowResUpsample(NULL),
    m_pEffectPass_LowResUpsampleWithOverdrawMetering(NULL),
	m_pEffectVar_FullResDepth(NULL),
	m_pEffectVar_ReducedResDepth(NULL),
	m_pEffectVar_ZLinParams(NULL),
	m_pEffectVar_DepthThreshold(NULL),
	m_pEffectVar_ReductionFactor(NULL),
	m_pEffectPass_DownSampleDepth(NULL),
	m_pEffectVar_TileSize(NULL),
	m_pEffectVar_DepthStencilSrc(NULL),
    m_pOffscreenTexture4x16F(NULL),
    m_pOffscreenTextureSRV4x16F(NULL),
    m_pOffscreenTextureRTV4x16F(NULL),
	m_pFullResDepthStencilSRV(NULL),
    m_pReducedDepthStencil(NULL),
    m_pReducedDepthStencilDSV(NULL),
	m_pReducedDepthStencilRODSV(NULL),
	m_pReducedDepthStencilSRV(NULL),
    m_pSavedRenderTargetRTV(NULL),
    m_pSavedDepthStencilDSV(NULL)
{
}

CLowResOffscreen::~CLowResOffscreen()
{
}

HRESULT CLowResOffscreen::BindToEffect(ID3DX11Effect* pEffect)
{
    m_pEffectPass_LowResUpsample = pEffect->GetTechniqueByName("LowResUpsample")->GetPassByIndex(0);
    m_pEffectPass_LowResUpsampleWithOverdrawMetering = pEffect->GetTechniqueByName("LowResUpsampleMetered")->GetPassByIndex(0);

	m_pEffectVar_FullResDepth = pEffect->GetVariableByName("g_FullResDepth")->AsShaderResource();
	m_pEffectVar_ReducedResDepth = pEffect->GetVariableByName("g_ReducedResDepth")->AsShaderResource();
	m_pEffectVar_ZLinParams = pEffect->GetVariableByName("g_ZLinParams")->AsVector();
	m_pEffectVar_DepthThreshold = pEffect->GetVariableByName("g_DepthThreshold")->AsScalar();
	m_pEffectVar_ReductionFactor = pEffect->GetVariableByName("g_ReductionFactor")->AsScalar();

	m_pEffectPass_DownSampleDepth = pEffect->GetTechniqueByName("DownSampleDepth")->GetPassByIndex(0);
	m_pEffectVar_TileSize = pEffect->GetVariableByName("g_TileSize")->AsScalar();
	m_pEffectVar_DepthStencilSrc = pEffect->GetVariableByName("g_DepthStencilSrc")->AsShaderResource();

	return S_OK;
}

HRESULT CLowResOffscreen::OnD3D11CreateDevice(ID3D11Device* pd3dDevice)
{
    return S_OK;
}

void CLowResOffscreen::OnD3D11DestroyDevice()
{
}

HRESULT CLowResOffscreen::OnD3D11ResizedSwapChain(ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, ID3D11ShaderResourceView* pFullResDepthStencilSRV)
{
    HRESULT hr;

	m_pFullResDepthStencilSRV = pFullResDepthStencilSRV;
	m_pFullResDepthStencilSRV->AddRef();

	const UINT reductionFactor = 1 << m_ReductionInMipLevels;

    {
        D3D11_TEXTURE2D_DESC texDesc;
        texDesc.Width = (pBackBufferSurfaceDesc->Width + reductionFactor - 1)/reductionFactor;
        texDesc.Height = (pBackBufferSurfaceDesc->Height + reductionFactor - 1)/reductionFactor;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        texDesc.SampleDesc.Count = 1; 
        texDesc.SampleDesc.Quality = 0; 
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags = 0;

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        rtvDesc.Format = texDesc.Format;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;

        D3D11_SHADER_RESOURCE_VIEW_DESC rvDesc;
        rvDesc.Format = rtvDesc.Format;
        rvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        rvDesc.Texture2D.MostDetailedMip = 0;
        rvDesc.Texture2D.MipLevels = texDesc.MipLevels;

        V_RETURN( pd3dDevice->CreateTexture2D(&texDesc, NULL, &m_pOffscreenTexture4x16F) );
        V_RETURN( pd3dDevice->CreateRenderTargetView(m_pOffscreenTexture4x16F, &rtvDesc, &m_pOffscreenTextureRTV4x16F) );
        V_RETURN( pd3dDevice->CreateShaderResourceView(m_pOffscreenTexture4x16F, &rvDesc, &m_pOffscreenTextureSRV4x16F) );
    }

	{
        D3D11_TEXTURE2D_DESC texDesc;
        texDesc.Width = (pBackBufferSurfaceDesc->Width + reductionFactor - 1)/reductionFactor;
        texDesc.Height = (pBackBufferSurfaceDesc->Height + reductionFactor - 1)/reductionFactor;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
        texDesc.SampleDesc.Count = 1; 
        texDesc.SampleDesc.Quality = 0; 
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags = 0;

		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsvDesc.Flags = 0;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = 0;

		D3D11_DEPTH_STENCIL_VIEW_DESC roDsvDesc = dsvDesc;
		roDsvDesc.Flags = D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = texDesc.MipLevels;

        V_RETURN( pd3dDevice->CreateTexture2D(&texDesc, NULL, &m_pReducedDepthStencil) );
        V_RETURN( pd3dDevice->CreateDepthStencilView(m_pReducedDepthStencil, &dsvDesc, &m_pReducedDepthStencilDSV) );
		V_RETURN( pd3dDevice->CreateDepthStencilView(m_pReducedDepthStencil, &roDsvDesc, &m_pReducedDepthStencilRODSV) );
		V_RETURN( pd3dDevice->CreateShaderResourceView(m_pReducedDepthStencil, &srvDesc, &m_pReducedDepthStencilSRV ) );
	}

    return S_OK;
}

void CLowResOffscreen::OnD3D11ReleasingSwapChain()
{
    SAFE_RELEASE(m_pOffscreenTexture4x16F);
    SAFE_RELEASE(m_pOffscreenTextureRTV4x16F);
    SAFE_RELEASE(m_pOffscreenTextureSRV4x16F);

    SAFE_RELEASE(m_pReducedDepthStencil);
    SAFE_RELEASE(m_pReducedDepthStencilDSV);
	SAFE_RELEASE(m_pReducedDepthStencilRODSV);
	SAFE_RELEASE(m_pReducedDepthStencilSRV);

	SAFE_RELEASE(m_pFullResDepthStencilSRV);
}

void CLowResOffscreen::BeginRenderToLowResOffscreen(ID3D11DeviceContext* pDC)
{
    pDC->OMGetRenderTargets(1, &m_pSavedRenderTargetRTV, &m_pSavedDepthStencilDSV);
    UINT NumViewports = 1;
    pDC->RSGetViewports(&NumViewports, &m_SavedViewport);

    // Scale viewport
    const FLOAT kReductionScale = 1.f/FLOAT(0x00000001 << m_ReductionInMipLevels);
    D3D11_VIEWPORT lowResViewport;
    lowResViewport.Height = m_SavedViewport.Height * kReductionScale;
    lowResViewport.Width = m_SavedViewport.Width * kReductionScale;
    lowResViewport.TopLeftX = m_SavedViewport.TopLeftX * kReductionScale;
    lowResViewport.TopLeftY = m_SavedViewport.TopLeftY * kReductionScale;
    lowResViewport.MinDepth = m_SavedViewport.MinDepth;
    lowResViewport.MaxDepth = m_SavedViewport.MaxDepth;

	// Down-sample DSV, using a padded viewport (to ensure coverage of last pixels in rows/columns)
	D3D11_VIEWPORT paddedLowResViewport = lowResViewport;
	paddedLowResViewport.Width = ceilf(paddedLowResViewport.Width);
	paddedLowResViewport.Height = ceilf(paddedLowResViewport.Height);
	DownSampleDepth(pDC, paddedLowResViewport);

    const D3DXVECTOR4 ClearColor(0.f,0.f,0.f,1.f);
    pDC->ClearRenderTargetView(m_pOffscreenTextureRTV4x16F, (FLOAT*)&ClearColor);

    pDC->OMSetRenderTargets(1, &m_pOffscreenTextureRTV4x16F, m_pReducedDepthStencilRODSV);
    pDC->RSSetViewports(1, &lowResViewport);
}

void CLowResOffscreen::EndRenderToLowResOffscreen(ID3D11DeviceContext* pDC, FLOAT zNear, FLOAT zFar, BOOL ShowOverdraw)
{
    //HRESULT hr;

    // Restore main render target
    pDC->OMSetRenderTargets(1, &m_pSavedRenderTargetRTV, NULL);
    pDC->RSSetViewports(1, &m_SavedViewport);

    // Blend off-screen rendering into main scene
	FLOAT ZLinParams[4] = { zFar/zNear, (zFar/zNear - 1.f), zFar, 0.f };
	m_pEffectVar_ZLinParams->SetFloatVector(ZLinParams);
	m_pEffectVar_DepthThreshold->SetFloat(m_UpsampleFilterDepthThreshold);
	const FLOAT kReductionScale = 1.f/FLOAT(0x00000001 << m_ReductionInMipLevels);
	m_pEffectVar_ReductionFactor->SetFloat(kReductionScale);

	m_pEffectVar_FullResDepth->SetResource(m_pFullResDepthStencilSRV);
	m_pEffectVar_ReducedResDepth->SetResource(m_pReducedDepthStencilSRV);

    ID3DX11EffectPass* pPass = ShowOverdraw ? m_pEffectPass_LowResUpsampleWithOverdrawMetering : m_pEffectPass_LowResUpsample;
    CScreenAlignedQuad::Instance().DrawQuad(pDC, m_pOffscreenTextureSRV4x16F, 0, NULL, pPass);

	// Unset DSV/RTV in device state
	m_pEffectVar_FullResDepth->SetResource(NULL);
	m_pEffectVar_ReducedResDepth->SetResource(NULL);
	pPass->Apply(0, pDC);

	// Restore DSV
	pDC->OMSetRenderTargets(1, &m_pSavedRenderTargetRTV, m_pSavedDepthStencilDSV);

    // Release saved resources
    SAFE_RELEASE(m_pSavedRenderTargetRTV);
    SAFE_RELEASE(m_pSavedDepthStencilDSV);
}

HRESULT CLowResOffscreen::DownSampleDepth(ID3D11DeviceContext* pDC, const D3D11_VIEWPORT& reducedVP)
{
	HRESULT hr;

    pDC->IASetInputLayout(NULL);
    pDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	const UINT tileSize = 1 << m_ReductionInMipLevels;
	m_pEffectVar_TileSize->SetInt(tileSize);

	m_pEffectVar_DepthStencilSrc->SetResource(m_pFullResDepthStencilSRV);

    pDC->OMSetRenderTargets(0, NULL, m_pReducedDepthStencilDSV);
    pDC->RSSetViewports(1, &reducedVP);

	pDC->ClearDepthStencilView(m_pReducedDepthStencilDSV, D3D11_CLEAR_DEPTH, 1.f, 0);

	V_RETURN(m_pEffectPass_DownSampleDepth->Apply(0, pDC));

    pDC->Draw( 4, 0 );

	return S_OK;
}
