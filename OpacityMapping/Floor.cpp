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

#include "Floor.h"

namespace
{
    struct FloorVertex
    {
        D3DXVECTOR3 position;
        D3DXVECTOR2 faceAttrib;

        enum { NumLayoutElements = 2 };

        static const D3D11_INPUT_ELEMENT_DESC* GetLayout()
        {
            static const D3D11_INPUT_ELEMENT_DESC theLayout[NumLayoutElements] = {
                // SemanticName  SemanticIndex  Format                        InputSlot    AlignedByteOffset              InputSlotClass               InstanceDataStepRate
                { "POSITION",    0,             DXGI_FORMAT_R32G32B32_FLOAT,  0,           D3D11_APPEND_ALIGNED_ELEMENT,  D3D11_INPUT_PER_VERTEX_DATA, 0                    },
                { "ATTRIB",      0,             DXGI_FORMAT_R32G32_FLOAT,     0,           D3D11_APPEND_ALIGNED_ELEMENT,  D3D11_INPUT_PER_VERTEX_DATA, 0                    }
            };

            return theLayout;
        }
    };
}

CFloor::CFloor(FLOAT tileSize, FLOAT tileBevelSize, UINT tilesPerEdge) :
    m_TileSize(tileSize),
    m_TileBevelSize(tileBevelSize),
    m_TilesPerEdge(tilesPerEdge),
    m_pEffectPass(NULL),
    m_pEffectPass_OverdrawMeteringOverride(NULL),
    m_mLocalToWorld_EffectVar(NULL),
    m_pVertexBuffer(NULL),
    m_pIndexBuffer(NULL),
    m_pVertexLayout(NULL)
{
}

HRESULT CFloor::BindToEffect(ID3D11Device* pd3dDevice, ID3DX11Effect* pEffect)
{
	HRESULT hr;

    m_pEffectPass = pEffect->GetTechniqueByName("RenderFloor")->GetPassByIndex(0);
    m_pEffectPass_OverdrawMeteringOverride = pEffect->GetTechniqueByName("RenderToSceneOverdrawMeteringOverride")->GetPassByIndex(0);

    m_mLocalToWorld_EffectVar = pEffect->GetVariableByName("g_mLocalToWorld")->AsMatrix();

    // Set up floor layout
	SAFE_RELEASE(m_pVertexLayout);
    D3DX11_PASS_DESC passDesc;
    m_pEffectPass->GetDesc(&passDesc);
    V_RETURN( pd3dDevice->CreateInputLayout( FloorVertex::GetLayout(), FloorVertex::NumLayoutElements,
                                             passDesc.pIAInputSignature, passDesc.IAInputSignatureSize,
                                             &m_pVertexLayout) );

	return S_OK;
}

HRESULT CFloor::OnD3D11CreateDevice(ID3D11Device* pd3dDevice)
{
    HRESULT hr;

    // Set up floor vertex/index buffers
    const UINT NumFloorTiles = m_TilesPerEdge * m_TilesPerEdge;
    const UINT NumFloorVerts = 20 * NumFloorTiles;
    const UINT NumFloorIndices = 30 * NumFloorTiles;
    FloorVertex* pFloorVertices = new FloorVertex[NumFloorVerts];
    WORD* pFloorIndices = new WORD[NumFloorIndices];
    {
        FloorVertex* pCurrVertex = pFloorVertices;
        WORD* pCurrIndex = pFloorIndices;
        WORD currBaseIndex = 0;
        for(UINT iy = 0; iy != m_TilesPerEdge; ++iy)
        {
            for(UINT ix = 0; ix != m_TilesPerEdge; ++ix)
            {
                // Is this an odd or even face?
                const FLOAT tileAttrib = (ix%2) == (iy%2) ? 0.f : 1.f;

                D3DXVECTOR2 faces[5];
                faces[0] = D3DXVECTOR2(tileAttrib, 0.f);
                faces[1] = D3DXVECTOR2(tileAttrib, 1.f);
                faces[2] = D3DXVECTOR2(tileAttrib, 2.f);
                faces[3] = D3DXVECTOR2(tileAttrib, 3.f);
                faces[4] = D3DXVECTOR2(tileAttrib, 4.f);

                D3DXVECTOR3 positions[8];
                positions[0].x = m_TileSize * (FLOAT(ix) - 0.5f * FLOAT(m_TilesPerEdge));
                positions[0].y = -m_TileBevelSize;
                positions[0].z = m_TileSize * (FLOAT(iy) - 0.5f * FLOAT(m_TilesPerEdge));
                positions[1] = positions[0] + D3DXVECTOR3(0,0,m_TileSize);
                positions[2] = positions[1] + D3DXVECTOR3(m_TileSize,0,0);
                positions[3] = positions[0] + D3DXVECTOR3(m_TileSize,0,0);
                positions[4] = positions[0] + D3DXVECTOR3( m_TileBevelSize, m_TileBevelSize, m_TileBevelSize);
                positions[5] = positions[1] + D3DXVECTOR3( m_TileBevelSize, m_TileBevelSize,-m_TileBevelSize);
                positions[6] = positions[2] + D3DXVECTOR3(-m_TileBevelSize, m_TileBevelSize,-m_TileBevelSize);
                positions[7] = positions[3] + D3DXVECTOR3(-m_TileBevelSize, m_TileBevelSize, m_TileBevelSize);

                // Centre face
                pCurrVertex[0].position = positions[4];
                pCurrVertex[0].faceAttrib = faces[0];
                pCurrVertex[1].position = positions[5];
                pCurrVertex[1].faceAttrib = faces[0];
                pCurrVertex[2].position = positions[6];
                pCurrVertex[2].faceAttrib = faces[0];
                pCurrVertex[3].position = positions[7];
                pCurrVertex[3].faceAttrib = faces[0];
                pCurrVertex += 4;
                pCurrIndex[0] = currBaseIndex + 0;
                pCurrIndex[1] = currBaseIndex + 1;
                pCurrIndex[2] = currBaseIndex + 2;
                pCurrIndex[3] = currBaseIndex + 2;
                pCurrIndex[4] = currBaseIndex + 3;
                pCurrIndex[5] = currBaseIndex + 0;
                currBaseIndex += 4;
                pCurrIndex += 6;

                // Left bevel
                pCurrVertex[0].position = positions[0];
                pCurrVertex[0].faceAttrib = faces[1];
                pCurrVertex[1].position = positions[1];
                pCurrVertex[1].faceAttrib = faces[1];
                pCurrVertex[2].position = positions[5];
                pCurrVertex[2].faceAttrib = faces[1];
                pCurrVertex[3].position = positions[4];
                pCurrVertex[3].faceAttrib = faces[1];
                pCurrVertex += 4;
                pCurrIndex[0] = currBaseIndex + 0;
                pCurrIndex[1] = currBaseIndex + 1;
                pCurrIndex[2] = currBaseIndex + 2;
                pCurrIndex[3] = currBaseIndex + 2;
                pCurrIndex[4] = currBaseIndex + 3;
                pCurrIndex[5] = currBaseIndex + 0;
                currBaseIndex += 4;
                pCurrIndex += 6;

                // Top bevel
                pCurrVertex[0].position = positions[1];
                pCurrVertex[0].faceAttrib = faces[2];
                pCurrVertex[1].position = positions[2];
                pCurrVertex[1].faceAttrib = faces[2];
                pCurrVertex[2].position = positions[6];
                pCurrVertex[2].faceAttrib = faces[2];
                pCurrVertex[3].position = positions[5];
                pCurrVertex[3].faceAttrib = faces[2];
                pCurrVertex += 4;
                pCurrIndex[0] = currBaseIndex + 0;
                pCurrIndex[1] = currBaseIndex + 1;
                pCurrIndex[2] = currBaseIndex + 2;
                pCurrIndex[3] = currBaseIndex + 2;
                pCurrIndex[4] = currBaseIndex + 3;
                pCurrIndex[5] = currBaseIndex + 0;
                currBaseIndex += 4;
                pCurrIndex += 6;

                // Right bevel
                pCurrVertex[0].position = positions[2];
                pCurrVertex[0].faceAttrib = faces[3];
                pCurrVertex[1].position = positions[3];
                pCurrVertex[1].faceAttrib = faces[3];
                pCurrVertex[2].position = positions[7];
                pCurrVertex[2].faceAttrib = faces[3];
                pCurrVertex[3].position = positions[6];
                pCurrVertex[3].faceAttrib = faces[3];
                pCurrVertex += 4;
                pCurrIndex[0] = currBaseIndex + 0;
                pCurrIndex[1] = currBaseIndex + 1;
                pCurrIndex[2] = currBaseIndex + 2;
                pCurrIndex[3] = currBaseIndex + 2;
                pCurrIndex[4] = currBaseIndex + 3;
                pCurrIndex[5] = currBaseIndex + 0;
                currBaseIndex += 4;
                pCurrIndex += 6;

                // Bottom bevel
                pCurrVertex[0].position = positions[0];
                pCurrVertex[0].faceAttrib = faces[4];
                pCurrVertex[1].position = positions[4];
                pCurrVertex[1].faceAttrib = faces[4];
                pCurrVertex[2].position = positions[7];
                pCurrVertex[2].faceAttrib = faces[4];
                pCurrVertex[3].position = positions[3];
                pCurrVertex[3].faceAttrib = faces[4];
                pCurrVertex += 4;
                pCurrIndex[0] = currBaseIndex + 0;
                pCurrIndex[1] = currBaseIndex + 1;
                pCurrIndex[2] = currBaseIndex + 2;
                pCurrIndex[3] = currBaseIndex + 2;
                pCurrIndex[4] = currBaseIndex + 3;
                pCurrIndex[5] = currBaseIndex + 0;
                currBaseIndex += 4;
                pCurrIndex += 6;
            }
        }
    }

    {
        D3D11_BUFFER_DESC vbDesc;
        vbDesc.ByteWidth = NumFloorVerts * sizeof(FloorVertex);
        vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbDesc.CPUAccessFlags = 0;
        vbDesc.MiscFlags = 0;
        vbDesc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA vbSRD;
        vbSRD.pSysMem = pFloorVertices;
        vbSRD.SysMemPitch = 0;
        vbSRD.SysMemSlicePitch = 0;

        V_RETURN( pd3dDevice->CreateBuffer(&vbDesc, &vbSRD, &m_pVertexBuffer) );
    }

    {
        D3D11_BUFFER_DESC ibDesc;
        ibDesc.ByteWidth = NumFloorIndices * sizeof(WORD);
        ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
        ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibDesc.CPUAccessFlags = 0;
        ibDesc.MiscFlags = 0;
        ibDesc.StructureByteStride = 0;

        D3D11_SUBRESOURCE_DATA ibSRD;
        ibSRD.pSysMem = pFloorIndices;
        ibSRD.SysMemPitch = 0;
        ibSRD.SysMemSlicePitch = 0;

        V_RETURN( pd3dDevice->CreateBuffer(&ibDesc, &ibSRD, &m_pIndexBuffer) );
    }

    delete [] pFloorVertices;
    delete [] pFloorIndices;

    return S_OK;
}

void CFloor::OnD3D11DestroyDevice()
{
    SAFE_RELEASE( m_pVertexLayout );
    SAFE_RELEASE( m_pVertexBuffer );
    SAFE_RELEASE( m_pIndexBuffer );
}

void CFloor::Draw(ID3D11DeviceContext* pDC, BOOL ShowOverdraw)
{
    HRESULT hr;

    D3DXMATRIXA16 mIdentity;
    D3DXMatrixIdentity(&mIdentity);
    V( m_mLocalToWorld_EffectVar->SetMatrix((FLOAT*)&mIdentity) );

    V( m_pEffectPass->Apply(0, pDC) );
    pDC->IASetInputLayout(m_pVertexLayout);
    pDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    if(ShowOverdraw)
    {
        V( m_pEffectPass_OverdrawMeteringOverride->Apply(0, pDC) );
    }

    const UINT Stride = sizeof(FloorVertex);
    const UINT Offset = 0;
    pDC->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &Stride, &Offset);
    pDC->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
    const UINT NumFloorIndices = m_TilesPerEdge * m_TilesPerEdge * 30;
    pDC->DrawIndexed(NumFloorIndices, 0, 0);
}
