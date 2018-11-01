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

#ifndef OPAQUEOBJECT_H
#define OPAQUEOBJECT_H

#include "DXUT.h"

#include "D3DX11Effect.h"

class CDXUTSDKMesh;

class COpaqueObject
{
public:

    COpaqueObject(const D3DXVECTOR3& anchorPoint, const D3DXVECTOR3& startPos, double phase);

    HRESULT OnD3D11CreateDevice(ID3D11Device* pd3dDevice);
    void OnD3D11DestroyDevice();

    void OnFrameMove( double fTime, float fElapsedTime);

    void DrawToScene(ID3D11DeviceContext* pDC, BOOL ShowOverdraw);

    void DrawToShadowMap(ID3D11DeviceContext* pDC);

    void GetBounds(D3DXVECTOR3& MinCorner, D3DXVECTOR3& MaxCorner);

	HRESULT BindToEffect(ID3D11Device* pd3dDevice, ID3DX11Effect* pEffect);

private:

    D3DXMATRIX GetLocalToWorld() const;

	double m_phase;

    FLOAT m_Scale;

    D3DXVECTOR3 m_AnchorPoint;
    D3DXVECTOR3 m_StartPosition;

    D3DXMATRIX m_CurrWorldMatrix;

    D3DXVECTOR3 m_MeshBoundsMinCorner;
    D3DXVECTOR3 m_MeshBoundsMaxCorner;

    CDXUTSDKMesh* m_pMesh;

    ID3DX11EffectPass*          m_pEffectPass_RenderToScene;
    ID3DX11EffectPass*          m_pEffectPass_RenderToShadowMap;
    ID3DX11EffectPass*          m_pEffectPass_OverdrawMeteringOverride;
    ID3DX11EffectPass*          m_pEffectPass_RenderThreadOverride;

    ID3DX11EffectMatrixVariable* m_mLocalToWorld_EffectVar;

    ID3D11InputLayout*          m_pVertexLayout;
};

#endif // OPAQUEOBJECT_H
