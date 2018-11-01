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

#include "SDKmesh.h"
#include "SDKmisc.h"

#include "Util.h"
#include "OpaqueObject.h"

namespace
{
    struct OpaqueObjectVertex
    {
        D3DXVECTOR3 position;
        D3DXVECTOR3 normal;
        D3DXVECTOR2 texCoord;

        enum { NumLayoutElements = 3 };

        static const D3D11_INPUT_ELEMENT_DESC* GetLayout()
        {
            static const D3D11_INPUT_ELEMENT_DESC theLayout[NumLayoutElements] = {
                // SemanticName  SemanticIndex  Format                        InputSlot    AlignedByteOffset              InputSlotClass               InstanceDataStepRate
                { "POSITION",    0,             DXGI_FORMAT_R32G32B32_FLOAT,  0,           D3D11_APPEND_ALIGNED_ELEMENT,  D3D11_INPUT_PER_VERTEX_DATA, 0                    },
                { "NORMAL",      0,             DXGI_FORMAT_R32G32B32_FLOAT,  0,           D3D11_APPEND_ALIGNED_ELEMENT,  D3D11_INPUT_PER_VERTEX_DATA, 0                    },
                { "TEXCOORD",    0,             DXGI_FORMAT_R32G32_FLOAT,     0,           D3D11_APPEND_ALIGNED_ELEMENT,  D3D11_INPUT_PER_VERTEX_DATA, 0                    }
            };

            return theLayout;
        }
    };
}

COpaqueObject::COpaqueObject(const D3DXVECTOR3& anchorPoint, const D3DXVECTOR3& startPos, double phase) :
	m_phase(phase),
    m_Scale(1.f),
    m_AnchorPoint(anchorPoint),
    m_StartPosition(startPos),
    m_pMesh(0),
    m_pEffectPass_RenderToScene(0),
    m_pEffectPass_RenderToShadowMap(0),
    m_pEffectPass_OverdrawMeteringOverride(0),
    m_pEffectPass_RenderThreadOverride(0),
    m_mLocalToWorld_EffectVar(0),
    m_pVertexLayout(0)
{
}

HRESULT COpaqueObject::BindToEffect(ID3D11Device* pd3dDevice, ID3DX11Effect* pEffect)
{
	HRESULT hr;

    m_pEffectPass_RenderToScene = pEffect->GetTechniqueByName("RenderOpaqueObjectToScene")->GetPassByIndex(0);
    m_pEffectPass_RenderToShadowMap = pEffect->GetTechniqueByName("RenderOpaqueObjectToShadowMap")->GetPassByIndex(0);
    m_pEffectPass_OverdrawMeteringOverride = pEffect->GetTechniqueByName("RenderToSceneOverdrawMeteringOverride")->GetPassByIndex(0);
    m_pEffectPass_RenderThreadOverride = pEffect->GetTechniqueByName("RenderOpaqueObjectThreadOverride")->GetPassByIndex(0);

    m_mLocalToWorld_EffectVar = pEffect->GetVariableByName("g_mLocalToWorld")->AsMatrix();

    // Set up vertex layout
	SAFE_RELEASE(m_pVertexLayout);
    D3DX11_PASS_DESC passDesc;
    m_pEffectPass_RenderToScene->GetDesc(&passDesc);
    V_RETURN( pd3dDevice->CreateInputLayout( OpaqueObjectVertex::GetLayout(), OpaqueObjectVertex::NumLayoutElements,
                                             passDesc.pIAInputSignature, passDesc.IAInputSignatureSize,
                                             &m_pVertexLayout) );

	return S_OK;
}

HRESULT COpaqueObject::OnD3D11CreateDevice(ID3D11Device* pd3dDevice)
{
    HRESULT hr;

    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"OpacityMapping\\thurible.sdkmesh" ) );

    m_pMesh = new CDXUTSDKMesh();
    V_RETURN(m_pMesh->Create(pd3dDevice, str));

    m_Scale = 0.05f;

    // Calc mesh bounds
    const UINT NumMeshes = m_pMesh->GetNumMeshes();
    if(NumMeshes)
    {
        m_MeshBoundsMinCorner = m_pMesh->GetMeshBBoxCenter(0) - m_pMesh->GetMeshBBoxExtents(0);
        m_MeshBoundsMaxCorner = m_pMesh->GetMeshBBoxCenter(0) + m_pMesh->GetMeshBBoxExtents(0);
    }

    for(UINT Mesh = 1; Mesh != NumMeshes; ++Mesh)
    {
        const D3DXVECTOR3 MinCorner = m_pMesh->GetMeshBBoxCenter(Mesh) - m_pMesh->GetMeshBBoxExtents(Mesh);
        const D3DXVECTOR3 MaxCorner = m_pMesh->GetMeshBBoxCenter(Mesh) + m_pMesh->GetMeshBBoxExtents(Mesh);

        m_MeshBoundsMinCorner.x = min(m_MeshBoundsMinCorner.x, MinCorner.x);
        m_MeshBoundsMinCorner.y = min(m_MeshBoundsMinCorner.y, MinCorner.y);
        m_MeshBoundsMinCorner.z = min(m_MeshBoundsMinCorner.z, MinCorner.z);

        m_MeshBoundsMaxCorner.x = max(m_MeshBoundsMaxCorner.x, MaxCorner.x);
        m_MeshBoundsMaxCorner.y = max(m_MeshBoundsMaxCorner.y, MaxCorner.y);
        m_MeshBoundsMaxCorner.z = max(m_MeshBoundsMaxCorner.z, MaxCorner.z);
    }

    return S_OK;
}

void COpaqueObject::OnD3D11DestroyDevice()
{
    SAFE_DELETE(m_pMesh);
    SAFE_RELEASE(m_pVertexLayout);
}

D3DXMATRIX COpaqueObject::GetLocalToWorld() const
{
    const D3DXMATRIXA16 mScaleAndAxisFlip = D3DXMATRIXA16(
        m_Scale, 0.f,     0.f,     0.f,
        0.f,     m_Scale, 0.f,     0.f,
        0.f,     0.f,     m_Scale, 0.f,
        0.f,     0.f,     0.f,     1.f
    );

    return mScaleAndAxisFlip * m_CurrWorldMatrix;
}

void COpaqueObject::DrawToShadowMap(ID3D11DeviceContext* pDC)
{
    HRESULT hr;

    D3DXMATRIXA16 mLocalToWorld = GetLocalToWorld();
    V( m_mLocalToWorld_EffectVar->SetMatrix((FLOAT*)&mLocalToWorld) );

    V( m_pEffectPass_RenderToShadowMap->Apply(0, pDC) );

    pDC->IASetInputLayout(m_pVertexLayout);
    m_pMesh->Render(pDC);
}

void COpaqueObject::DrawToScene(ID3D11DeviceContext* pDC, BOOL ShowOverdraw)
{
    HRESULT hr;

    D3DXMATRIXA16 mLocalToWorld = GetLocalToWorld();
    V( m_mLocalToWorld_EffectVar->SetMatrix((FLOAT*)&mLocalToWorld) );

    V( m_pEffectPass_RenderToScene->Apply(0, pDC) );
    if(ShowOverdraw)
    {
        V( m_pEffectPass_OverdrawMeteringOverride->Apply(0, pDC) );
    }

    pDC->IASetInputLayout(m_pVertexLayout);
    m_pMesh->Render(pDC);

    // Also, draw the thread connecting the object to the anchor point - we supply
    // a vector parallel to the thread in the normal, the shader works out the true
    // normal
    const D3DXVECTOR3 currPos = D3DXVECTOR3(m_CurrWorldMatrix._41, m_CurrWorldMatrix._42, m_CurrWorldMatrix._43);
    D3DXVECTOR3 nml = currPos - m_AnchorPoint;
    D3DXVec3Normalize(&nml, &nml);

    OpaqueObjectVertex verts[2];
    verts[0].position = currPos;
    verts[0].normal = nml;
    verts[1].position = m_AnchorPoint;
    verts[1].normal = nml;

    D3DXMATRIXA16 mIdentity;
    D3DXMatrixIdentity(&mIdentity);
    V( m_mLocalToWorld_EffectVar->SetMatrix((FLOAT*)&mIdentity) );

    V( m_pEffectPass_RenderToScene->Apply(0, pDC) );
    V( m_pEffectPass_RenderThreadOverride->Apply(0, pDC) );
    if(ShowOverdraw)
    {
        V( m_pEffectPass_OverdrawMeteringOverride->Apply(0, pDC) );
    }

    pDC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
    CDrawUP::Instance().DrawUP(pDC, &verts, sizeof(verts[0]), sizeof(verts)/sizeof(verts[0]));
}

void COpaqueObject::OnFrameMove( double fTime, float fElapsedTime)
{
    const FLOAT g = 9.81f;
	fTime += m_phase;

    // Simple harmonic motion
    const D3DXVECTOR3 vStringStart = m_StartPosition - m_AnchorPoint;
    const FLOAT l = D3DXVec3Length(&vStringStart);
    const FLOAT timeConstant = sqrtf(g/l);
    const FLOAT swingPosition = FLOAT(cos(double(timeConstant) * fTime));

    D3DXVECTOR3 vString;
    vString.x = vStringStart.x * swingPosition;
    vString.z = vStringStart.z * swingPosition;
    vString.y = -sqrtf(l*l - (vString.x*vString.x + vString.z*vString.z));

    // Finally, update the world matrix
    D3DXMatrixIdentity(&m_CurrWorldMatrix);

    D3DXVECTOR3 yAxis = -vString;
    D3DXVec3Normalize(&yAxis, &yAxis);

    D3DXVECTOR3 xAxis = D3DXVECTOR3(0,1,0);
    D3DXVec3Cross(&xAxis, &xAxis, &vStringStart);
    D3DXVec3Normalize(&xAxis, &xAxis);

    D3DXVECTOR3 zAxis;
    D3DXVec3Cross(&zAxis, &xAxis, &yAxis);

    m_CurrWorldMatrix._11 = xAxis.x;
    m_CurrWorldMatrix._12 = xAxis.y;
    m_CurrWorldMatrix._13 = xAxis.z;

    m_CurrWorldMatrix._21 = yAxis.x;
    m_CurrWorldMatrix._22 = yAxis.y;
    m_CurrWorldMatrix._23 = yAxis.z;

    m_CurrWorldMatrix._31 = zAxis.x;
    m_CurrWorldMatrix._32 = zAxis.y;
    m_CurrWorldMatrix._33 = zAxis.z;

    D3DXVECTOR3 currPos = m_AnchorPoint + vString;
    m_CurrWorldMatrix._41 = currPos.x;
    m_CurrWorldMatrix._42 = currPos.y;
    m_CurrWorldMatrix._43 = currPos.z;
}

void COpaqueObject::GetBounds(D3DXVECTOR3& MinCorner, D3DXVECTOR3& MaxCorner)
{
    D3DXVECTOR4 WorldMinCorner, WorldMaxCorner;
    TransformBounds(GetLocalToWorld(), D3DXVECTOR4(m_MeshBoundsMinCorner,0.f), D3DXVECTOR4(m_MeshBoundsMaxCorner,0.f), WorldMinCorner, WorldMaxCorner);

    MinCorner.x = WorldMinCorner.x;
    MinCorner.y = WorldMinCorner.y;
    MinCorner.z = WorldMinCorner.z;

    MaxCorner.x = WorldMaxCorner.x;
    MaxCorner.y = WorldMaxCorner.y;
    MaxCorner.z = WorldMaxCorner.z;
}
