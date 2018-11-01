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

#ifndef PARTICLESYSTEM_H
#define PARTICLESYSTEM_H

#include "DXUT.h"

#include "D3DX11Effect.h"

// Fwd. decls
struct ParticleInstance;

class CParticleSystem
{
public:

    CParticleSystem(UINT InstanceTextureWH);
    ~CParticleSystem();

    HRESULT SaveParticleState(double fTime);

    void InitPlumeSim();
    HRESULT PlumeSimTick(double fTime);
    HRESULT SortByDepth(const D3DXMATRIX& mView); 

    HRESULT UpdateD3D11Resources(ID3D11DeviceContext* pDC);

    HRESULT OnD3D11CreateDevice(ID3D11Device* pd3dDevice);
    void OnD3D11DestroyDevice();

    const D3DXVECTOR3& GetBoundsMin() const { return m_MinCorner; }
    const D3DXVECTOR3& GetBoundsMax() const { return m_MaxCorner; }

    UINT GetNumActiveParticles() const { return m_NumActiveParticles; }

    HRESULT DrawToOpacity(ID3D11DeviceContext* pDC);
    HRESULT DrawToSceneLowRes(ID3D11DeviceContext* pDC);

	HRESULT DrawSoftToSceneLowRes(	ID3D11DeviceContext* pDC,
									ID3D11ShaderResourceView* pDepthStencilSRV,
									FLOAT zNear, FLOAT zFar
									);

    HRESULT DrawToScene(ID3D11DeviceContext* pDC);

	enum LightingMode {
		LM_Vertex = 0,
		LM_Tessellated,
		LM_Pixel,
		Num_LightingModes
	};

    void SetLightingMode(LightingMode lm);
    BOOL GetLightingMode() const { return m_LightingMode; }

    void SetWireframe(BOOL);
    BOOL GetWireframe() const { return m_bWireframe; }

    void SetMeteredOverdraw(BOOL b) { m_bMeteredOverdraw = b; }

	// NB: Effectively measured in 'particle widths'
	void SetSoftParticleFadeDistance(FLOAT f) { m_SoftParticleFadeDistance = f; }
	FLOAT GetSoftParticleFadeDistance() const { return m_SoftParticleFadeDistance; }

	HRESULT BindToEffect(ID3D11Device* pd3dDevice, ID3DX11Effect* pEffect);

private:

    HRESULT Draw(ID3D11DeviceContext* pDC, BOOL UseTessellation);
    HRESULT LoadParticleState(double fTime);
    void UpdateBounds(BOOL& firstUpdate, const D3DXVECTOR4& positionAndScale);
    HRESULT SetEffectVars();

    BOOL m_bWireframe;
    BOOL m_bMeteredOverdraw;

	LightingMode m_LightingMode;

    UINT m_InstanceTextureWH;
    ID3D11Texture2D*            m_pInstanceTexture;
    ID3D11ShaderResourceView*   m_pInstanceTextureSRV;

    UINT m_AbsoluteMaxNumActiveParticles;
    UINT m_MaxNumActiveParticles;
    UINT m_NumActiveParticles;

    ID3D11Buffer*               m_pParticleVertexBuffer;
    ID3D11Buffer*               m_pParticleIndexBuffer;
    ID3D11Buffer*               m_pParticleIndexBufferTessellated;
    ID3D11InputLayout*          m_pParticleVertexLayout;

    ID3D11ShaderResourceView*   m_pParticleTextureSRV;

    FLOAT m_ParticleLifeTime;
    FLOAT m_ParticleScale;
	FLOAT m_SoftParticleFadeDistance;

    FLOAT m_TessellationDensity;
    FLOAT m_MaxTessellation;

    ParticleInstance* m_pParticlePrevBuffer;
    ParticleInstance* m_pParticleCurrBuffer;

    BOOL m_FirstUpdate;
    double m_LastUpdateTime;
    FLOAT m_EmissionRate;

    ID3DX11EffectScalarVariable* m_TessellationDensity_EffectVar;
    ID3DX11EffectScalarVariable* m_MaxTessellation_EffectVar;

    ID3DX11EffectPass*          m_pEffectPass_ToScene[Num_LightingModes];
	ID3DX11EffectPass*          m_pEffectPass_SoftToScene[Num_LightingModes];
    ID3DX11EffectPass*          m_pEffectPass_ToSceneWireframeOverride;
    ID3DX11EffectPass*          m_pEffectPass_ToSceneOverdrawMeteringOverride;
    ID3DX11EffectPass*          m_pEffectPass_LowResToSceneOverride;
    ID3DX11EffectPass*          m_pEffectPass_ToOpacity;

    ID3DX11EffectShaderResourceVariable* m_InstanceTexture_EffectVar;
    ID3DX11EffectScalarVariable*         m_ParticleScale_EffectVar;
    ID3DX11EffectShaderResourceVariable* m_ParticleTexture_EffectVar;
    ID3DX11EffectMatrixVariable*         m_mLocalToWorld_EffectVar;
	ID3DX11EffectVectorVariable*		 m_ZLinParams_EffectVar;
	ID3DX11EffectScalarVariable*         m_SoftParticlesFadeDistance_EffectVar;
	ID3DX11EffectShaderResourceVariable* m_SoftParticlesDepthTexture_EffectVar;

    D3DXVECTOR3 m_MinCorner;
    D3DXVECTOR3 m_MaxCorner;
};

#endif // PARTICLESYSTEM_H
