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

#ifndef __NBODYSYSTEM_H__
#define __NBODYSYSTEM_H__

#include <DXUT.h>
#include <SDKmisc.h>
#include <d3d10.h>
#include <d3dx10.h>

typedef struct{
    unsigned int nBodies;
    float       *position;
    float       *velocity;
} BodyData;

class NBodySystemCS
{
public:
    NBodySystemCS();

    // Update the simulation
    HRESULT updateBodies (float dt);

    // Reset the simulation
	HRESULT resetBodies  (BodyData config);

    // Render the particles
    HRESULT renderBodies (const D3DXMATRIX *p_mWorld, const D3DXMATRIX *p_mWiew, const D3DXMATRIX *p_mProj);

    void    setPointSize(float size) { m_fPointSize = size; } 
    float	getPointSize()  const	 { return m_fPointSize; } 

    // D3D Event handlers
	HRESULT onCreateDevice      (ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext);
    void    onDestroyDevice     ( );
	void    onResizedSwapChain  (ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC *pBackBufferSurfaceDesc);
	void    onReleasingSwapChain( );

protected:
	// The device
	ID3D11Device* m_pd3dDevice;
	ID3D11DeviceContext *m_pd3dImmediateContext;

	float m_fAspectRatio;

	// Particle texture and resource views
	ID3D11Texture2D	         *m_pParticleTex;    // Texture for displaying particles
    ID3D11ShaderResourceView *m_pParticleTexSRV;

	// Vertex layout
	ID3D11InputLayout *m_pIndLayout;
	
    // state
    UINT    m_numBodies;
    float   m_fPointSize;
    UINT    m_readBuffer;

	//Render states
	ID3D11BlendState		*m_pBlendingEnabled;
	ID3D11DepthStencilState *m_pDepthDisabled;
	ID3D11SamplerState		*m_pParticleSamplerState;
	ID3D11RasterizerState   *m_pRasterizerState;

	// shaders
	ID3D11VertexShader		*m_pVSDisplayParticleStructBuffer;
	ID3D11GeometryShader	*m_pGSDisplayParticle;
	ID3D11PixelShader		*m_pPSDisplayParticle;
	ID3D11PixelShader		*m_pPSDisplayParticleTex;

	ID3D11ComputeShader		*m_pCSUpdatePositionAndVelocity;

	// structured buffer
    ID3D11Texture1D           *m_pBodiesTex1D[2];
    ID3D11ShaderResourceView  *m_pBodiesTexSRV[2];
    ID3D11UnorderedAccessView *m_pBodiesTexUAV[2];
	ID3D11Buffer			  *m_pStructuredBuffer;
	ID3D11ShaderResourceView  *m_pStructuredBufferSRV;
	ID3D11UnorderedAccessView *m_pStructuredBufferUAV;

	// constant buffers
	ID3D11Buffer			*m_pcbDraw;
	ID3D11Buffer			*m_pcbUpdate;
	ID3D11Buffer			*m_pcbImmutable;

private:
};

#endif // __NBODYSYSTEM_H__