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

#ifndef LOWRESOFFSCREEN_H
#define LOWRESOFFSCREEN_H

#include "DXUT.h"

#include "D3DX11Effect.h"

class CLowResOffscreen
{
public:

    CLowResOffscreen(UINT ReductionInMipLevels);
	~CLowResOffscreen();

    HRESULT OnD3D11CreateDevice(ID3D11Device* pd3dDevice);
    void OnD3D11DestroyDevice();

    HRESULT OnD3D11ResizedSwapChain(ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, ID3D11ShaderResourceView* pFullResDepthStencilSRV);
    void OnD3D11ReleasingSwapChain();

    void BeginRenderToLowResOffscreen(ID3D11DeviceContext* pDC);
    void EndRenderToLowResOffscreen(ID3D11DeviceContext* pDC, FLOAT zNear, FLOAT zFar, BOOL ShowOverdraw);

	FLOAT GetUpsampleFilterDepthThreshold() const { return m_UpsampleFilterDepthThreshold; }
	void SetUpsampleFilterDepthThreshold(FLOAT f) { m_UpsampleFilterDepthThreshold = f; }

	ID3D11ShaderResourceView* GetReducedDepthStencilSRV() const { return m_pReducedDepthStencilSRV; }

	HRESULT BindToEffect(ID3DX11Effect* pEffect);

private:

    UINT m_ReductionInMipLevels;
	FLOAT m_UpsampleFilterDepthThreshold;

    HRESULT DownSampleDepth(ID3D11DeviceContext* pDC, const D3D11_VIEWPORT& reducedVP);

    ID3DX11EffectPass*						m_pEffectPass_LowResUpsample;
    ID3DX11EffectPass*						m_pEffectPass_LowResUpsampleWithOverdrawMetering;
	ID3DX11EffectShaderResourceVariable*	m_pEffectVar_FullResDepth;
	ID3DX11EffectShaderResourceVariable*	m_pEffectVar_ReducedResDepth;
	ID3DX11EffectVectorVariable*			m_pEffectVar_ZLinParams;
	ID3DX11EffectScalarVariable*			m_pEffectVar_DepthThreshold;
	ID3DX11EffectScalarVariable*			m_pEffectVar_ReductionFactor;

    ID3DX11EffectPass*						m_pEffectPass_DownSampleDepth;
    ID3DX11EffectScalarVariable*			m_pEffectVar_TileSize;
	ID3DX11EffectShaderResourceVariable*	m_pEffectVar_DepthStencilSrc;

    ID3D11Texture2D*            m_pOffscreenTexture4x16F;
    ID3D11ShaderResourceView*   m_pOffscreenTextureSRV4x16F;
    ID3D11RenderTargetView*     m_pOffscreenTextureRTV4x16F;

	ID3D11ShaderResourceView*	m_pFullResDepthStencilSRV;

    ID3D11Texture2D*            m_pReducedDepthStencil;
    ID3D11DepthStencilView*     m_pReducedDepthStencilDSV;
	ID3D11DepthStencilView*     m_pReducedDepthStencilRODSV;
	ID3D11ShaderResourceView*	m_pReducedDepthStencilSRV;

    // State saved between Begin/End pairs
    ID3D11RenderTargetView*     m_pSavedRenderTargetRTV;
    ID3D11DepthStencilView*     m_pSavedDepthStencilDSV;
    D3D11_VIEWPORT              m_SavedViewport;
};

#endif // LOWRESOFFSCREEN_H
