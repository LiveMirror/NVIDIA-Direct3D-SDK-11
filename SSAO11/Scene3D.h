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

#include <DXUT.h>
#include <strsafe.h>
#include "d3dx11effect.h"
#include <SDKmisc.h>
#include <SDKmesh.h>

#define MAX_PATH_STR 512

typedef enum
{
    SCENE_NO_GROUND_PLANE,
    SCENE_USE_GROUND_PLANE,
} SceneGroundPlaneType;

typedef enum
{
    SCENE_NO_SHADING,
    SCENE_USE_SHADING,
} SceneShadingType;

typedef enum
{
    SCENE_USE_ORBITAL_CAMERA,
    SCENE_USE_FIRST_PERSON_CAMERA,
} SceneCameraType;

struct MeshDescriptor
{
    LPWSTR Name;
    LPWSTR Path;
    SceneGroundPlaneType UseGroundPlane;
    SceneShadingType UseShading;
    SceneCameraType CameraType;
    float SceneScale;
};

struct SceneMesh
{
    HRESULT OnCreateDevice(ID3D11Device* pd3dDevice, MeshDescriptor &MeshDesc)
    {
        HRESULT hr = S_OK;
        WCHAR Filepath[MAX_PATH_STR+1];
        V_RETURN( DXUTFindDXSDKMediaFileCch(Filepath, MAX_PATH_STR, MeshDesc.Path) );
        V_RETURN( m_SDKMesh.Create(pd3dDevice, Filepath) );

        D3DXVECTOR3 bbExtents = m_SDKMesh.GetMeshBBoxExtents(0);
        D3DXVECTOR3 bbCenter  = m_SDKMesh.GetMeshBBoxCenter(0);
        m_GroundHeight = bbCenter.y - bbExtents.y;
        m_Desc = MeshDesc;
        return hr;
    }

    void OnDestroyDevice()
    {
        m_SDKMesh.Destroy();
    }

    CDXUTSDKMesh &GetSDKMesh()
    {
        return m_SDKMesh;
    }

    bool UseShading()
    {
        return (m_Desc.UseShading == SCENE_USE_SHADING);
    }

    bool UseGroundPlane()
    {
        return (m_Desc.UseGroundPlane == SCENE_USE_GROUND_PLANE);
    }

    bool UseOrbitalCamera()
    {
        return (m_Desc.CameraType == SCENE_USE_ORBITAL_CAMERA);
    }

    float GetSceneScale()
    {
        return m_Desc.SceneScale;
    }

    float GetGroundHeight()
    {
        return m_GroundHeight;
    }

protected:
    CDXUTSDKMesh    m_SDKMesh;
    float           m_GroundHeight;
    MeshDescriptor  m_Desc;
};

class SceneRenderer
{
public:
    SceneRenderer();

    HRESULT OnCreateDevice      ( ID3D11Device *pd3dDevice );
    HRESULT OnFrameRender       ( const D3DXMATRIX *p_mWorld,
                                  const D3DXMATRIX *p_mView,
                                  const D3DXMATRIX *p_mProj,
                                  SceneMesh *pMesh );
    void CopyColors(ID3D11ShaderResourceView *pColorSRV);
    HRESULT OnDestroyDevice     ();

    void RecompileShader();

private:

    // Geometry
    ID3D11InputLayout          *m_VertexLayout;
    ID3D11Buffer               *m_VertexBuffer;
    ID3D11Buffer               *m_IndexBuffer;
    
    // The device
    ID3D11Device               *m_D3DDevice;

    // Immediate context 
    ID3D11DeviceContext        *m_D3DImmediateContext;
    
    // The effects and rendering techniques
    ID3DX11Effect               *m_Effect;
    ID3DX11EffectPass           *m_RenderSceneDiffusePass;
    ID3DX11EffectPass           *m_CopyColorPass;

    // Effect variable pointers
    ID3DX11EffectMatrixVariable *m_VarWVP;
    ID3DX11EffectScalarVariable *m_VarIsWhite;
};
