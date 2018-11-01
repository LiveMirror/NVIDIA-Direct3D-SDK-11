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

#include "Scene3D.h"

// Vertex Data
struct Scene3DVertex
{
    D3DXVECTOR3 pos;
    D3DXVECTOR3 nor;
};

// Input layout definitions
D3D11_INPUT_ELEMENT_DESC VertexLayout[] =
{
    { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
    { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
};

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
SceneRenderer::SceneRenderer()
    : m_VertexLayout(NULL)
    , m_VertexBuffer(NULL)
    , m_Effect(NULL)
    , m_RenderSceneDiffusePass(NULL)
    , m_VarWVP(NULL)
    , m_VarIsWhite(NULL)
{
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void SceneRenderer::RecompileShader()
{
    HRESULT hr = S_OK;
    ID3DX11Effect *NewEffect = NULL;
    ID3DXBuffer* pShader;
    WCHAR str[MAX_PATH_STR+1];

    DXUTFindDXSDKMediaFileCch(str, MAX_PATH_STR, L"..\\Source\\SSAO11\\Scene3D.fx");
    V(D3DXCompileShaderFromFile(str, NULL, NULL, NULL, "fx_5_0", 0, &pShader, NULL, NULL));
    V(D3DX11CreateEffectFromMemory(pShader->GetBufferPointer(), pShader->GetBufferSize(), 0, m_D3DDevice, &NewEffect));

    if (NewEffect && NewEffect->IsValid())
    {
        SAFE_RELEASE(m_Effect);
        m_Effect = NewEffect;
    }
    else
    {
        MessageBox(NULL, L"Scene3D.fx compilation failed.", L"ERROR", MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
        return;
    }

    m_RenderSceneDiffusePass = m_Effect->GetTechniqueByName("RenderSceneDiffuse")->GetPassByIndex(0);
    m_CopyColorPass          = m_Effect->GetTechniqueByName("CopyColor")->GetPassByIndex(0);
    m_VarWVP                 = m_Effect->GetVariableByName("g_WorldViewProjection")->AsMatrix();
    m_VarIsWhite             = m_Effect->GetVariableByName("g_IsWhite")->AsScalar();
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
HRESULT SceneRenderer::OnCreateDevice(ID3D11Device* pd3dDevice)
{
    HRESULT hr = S_OK;

    m_D3DDevice = pd3dDevice ;
    m_D3DDevice->GetImmediateContext(&m_D3DImmediateContext);

    RecompileShader();

    // Create the input layout
    D3DX11_PASS_DESC PassDesc;
    m_RenderSceneDiffusePass->GetDesc(&PassDesc);
    UINT numElements = sizeof(VertexLayout)/sizeof(VertexLayout[0]);
    V( m_D3DDevice->CreateInputLayout(VertexLayout, numElements, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &m_VertexLayout) );

    // Create the floor plane
    float w = 1.0f;
    Scene3DVertex vertices[] =
    {
        { D3DXVECTOR3(-w, 0,  w), D3DXVECTOR3(0.0f, 1.0f, 0.0f) },
        { D3DXVECTOR3( w, 0,  w), D3DXVECTOR3(0.0f, 1.0f, 0.0f) },
        { D3DXVECTOR3( w, 0, -w), D3DXVECTOR3(0.0f, 1.0f, 0.0f) },
        { D3DXVECTOR3(-w, 0, -w), D3DXVECTOR3(0.0f, 1.0f, 0.0f) },
    };

    D3D11_BUFFER_DESC bd;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(Scene3DVertex) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    bd.StructureByteStride = 0;
    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = vertices;
    V( pd3dDevice->CreateBuffer(&bd, &InitData, &m_VertexBuffer) );

    // Create index buffer
    DWORD indices[] =
    {
        0,1,2,
        0,2,3,
    };

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(DWORD) * 6;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    bd.StructureByteStride = 0;
    InitData.pSysMem = indices;
    V( pd3dDevice->CreateBuffer(&bd, &InitData, &m_IndexBuffer) );

    return S_OK;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
HRESULT SceneRenderer::OnFrameRender(const D3DXMATRIX *p_mWorld,
                                     const D3DXMATRIX *p_mView, 
                                     const D3DXMATRIX *p_mProj,
                                     SceneMesh *pMesh)
{
    HRESULT hr = S_OK;

    // Compute view matrices
    D3DXMATRIX WVPMatrix = (*p_mWorld) * (*p_mView) * (*p_mProj);
    D3DXMATRIX WVMatrix  = (*p_mWorld) * (*p_mView);

    // Set effect variables
    m_VarWVP->SetMatrix(WVPMatrix);
    m_VarIsWhite->SetBool(!pMesh->UseShading());

    // Draw mesh
    UINT Strides[1];
    UINT Offsets[1];
    ID3D11Buffer* pVB[1];
    CDXUTSDKMesh &SDKMesh = pMesh->GetSDKMesh();
    pVB[0] = SDKMesh.GetVB11(0,0);
    Strides[0] = (UINT)SDKMesh.GetVertexStride(0,0);
    Offsets[0] = 0;
    m_D3DImmediateContext->IASetVertexBuffers(0, 1, pVB, Strides, Offsets);
    m_D3DImmediateContext->IASetIndexBuffer(SDKMesh.GetIB11(0), SDKMesh.GetIBFormat11(0), 0);
    m_D3DImmediateContext->IASetInputLayout(m_VertexLayout);

    for (UINT subset = 0; subset < SDKMesh.GetNumSubsets(0); ++subset)
    {
        SDKMESH_SUBSET* pSubset = SDKMesh.GetSubset(0, subset);
        D3D11_PRIMITIVE_TOPOLOGY PrimType = SDKMesh.GetPrimitiveType11((SDKMESH_PRIMITIVE_TYPE)pSubset->PrimitiveType);
        m_D3DImmediateContext->IASetPrimitiveTopology(PrimType);

        m_RenderSceneDiffusePass->Apply(0, m_D3DImmediateContext);
        m_D3DImmediateContext->DrawIndexed((UINT)pSubset->IndexCount, (UINT)pSubset->IndexStart, (UINT)pSubset->VertexStart);
    }

    // Draw ground plane
    if (pMesh->UseGroundPlane())
    {
        D3DXMATRIX mTranslate;
        D3DXMatrixTranslation(&mTranslate, 0.0f, pMesh->GetGroundHeight(), 0.0f);

        WVMatrix = mTranslate * WVMatrix;
        WVPMatrix = WVMatrix * (*p_mProj);
        m_VarWVP->SetMatrix(WVPMatrix);

        UINT stride = sizeof(Scene3DVertex);
        UINT offset = 0;
        m_D3DImmediateContext->IASetVertexBuffers    (0, 1, &m_VertexBuffer, &stride, &offset);
        m_D3DImmediateContext->IASetIndexBuffer      (m_IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
        m_D3DImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_D3DImmediateContext->IASetInputLayout      (m_VertexLayout);

        m_RenderSceneDiffusePass->Apply(0, m_D3DImmediateContext);
        m_D3DImmediateContext->DrawIndexed(6, 0, 0);
    }

    return hr;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void SceneRenderer::CopyColors(ID3D11ShaderResourceView *pColorSRV)
{
    m_Effect->GetVariableByName("tColor")->AsShaderResource()->SetResource(pColorSRV);
    m_CopyColorPass->Apply(0, m_D3DImmediateContext);
    m_D3DImmediateContext->Draw(3, 0);
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
HRESULT SceneRenderer::OnDestroyDevice()
{
    SAFE_RELEASE(m_Effect);
    SAFE_RELEASE(m_VertexLayout);
    SAFE_RELEASE(m_D3DImmediateContext);
    SAFE_RELEASE(m_VertexBuffer);
    SAFE_RELEASE(m_IndexBuffer);
    return S_OK;
}
