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

#ifndef FLOOR_H
#define FLOOR_H

#include "DXUT.h"

#include "D3DX11Effect.h"

class CFloor
{
public:

    CFloor(FLOAT tileSize, FLOAT tileBevelSize, UINT tilesPerEdge);

    HRESULT OnD3D11CreateDevice(ID3D11Device* pd3dDevice);
    void OnD3D11DestroyDevice();

    void Draw(ID3D11DeviceContext* pDC, BOOL ShowOverdraw);

	HRESULT BindToEffect(ID3D11Device* pd3dDevice, ID3DX11Effect* pEffect);

private:

    FLOAT m_TileSize;
    FLOAT m_TileBevelSize;
    UINT m_TilesPerEdge;

    ID3DX11EffectPass*          m_pEffectPass;
    ID3DX11EffectPass*          m_pEffectPass_OverdrawMeteringOverride;

    ID3DX11EffectMatrixVariable* m_mLocalToWorld_EffectVar;

    ID3D11Buffer*               m_pVertexBuffer;
    ID3D11Buffer*               m_pIndexBuffer;
    ID3D11InputLayout*          m_pVertexLayout;
};

#endif // FLOOR_H
