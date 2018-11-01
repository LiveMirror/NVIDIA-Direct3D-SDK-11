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


#ifndef LIGHTINGCS_H
#define LIGHTINGCS_H

#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "SDKMesh.h"
#include <D3DX11tex.h>
#include <D3DX11.h>
#include <D3DX11core.h>
#include <D3DX11async.h>
#include <strsafe.h>
#include <math.h>
#include <assert.h>
#include <string>

#include "SimpleRT.h"


// uncomment to help track memory leaks
/*
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif
*/



class Mesh : public CDXUTSDKMesh
{
public:
    enum ALPHA_RENDER_STATE
    {
        ALL_ALPHA = 0,
        NO_ALPHA = 1,
        WITH_ALPHA = 2,
    };

private:
    int m_numMeshes;
    int* m_numSubsets;
    D3DXVECTOR3** m_BoundingBoxCenterSubsets;
    D3DXVECTOR3** m_BoundingBoxExtentsSubsets;

    unsigned int m_numMaterials;
    ID3D11ShaderResourceView** pAlphaMaskRV11s;

    void RenderFrameSubsetBounded( UINT iFrame,
                                 bool bAdjacent,
                                 ID3D11DeviceContext* pd3dDeviceContext,
                                 D3DXVECTOR3 minSize, D3DXVECTOR3 maxSize, 
                                 UINT iDiffuseSlot,
                                 UINT iNormalSlot,
                                 UINT iSpecularSlot,            
                                 UINT iAlphaSlot,
                                 ALPHA_RENDER_STATE alphaState);

    void RenderMeshSubsetBounded( UINT iMesh,
                                bool bAdjacent,
                                ID3D11DeviceContext* pd3dDeviceContext,
                                D3DXVECTOR3 minSize, D3DXVECTOR3 maxSize, 
                                UINT iDiffuseSlot,
                                UINT iNormalSlot,
                                UINT iSpecularSlot,
                                UINT iAlphaSlot,
                                ALPHA_RENDER_STATE alphaState);

public:

    void initializeAlphaMaskTextures();

    void LoadAlphaMasks( ID3D11Device* pd3dDevice, std::string stringToRemove, std::string stringToAdd );

    void LoadNormalmaps( ID3D11Device* pd3dDevice, std::string stringToRemove, std::string stringToAdd);

    void initializeDefaultNormalmaps(ID3D11Device* pd3dDevice, std::string mapName);

    void RenderBounded( ID3D11DeviceContext* pd3dDeviceContext, D3DXVECTOR3 minSize, D3DXVECTOR3 maxSize, 
                        UINT iDiffuseSlot = INVALID_SAMPLER_SLOT,
                        UINT iNormalSlot = INVALID_SAMPLER_SLOT,
                        UINT iSpecularSlot = INVALID_SAMPLER_SLOT,
                        UINT iAlphaSlot = INVALID_SAMPLER_SLOT,
                        ALPHA_RENDER_STATE alphaState = ALL_ALPHA);


    void RenderSubsetBounded( UINT iMesh,
                              UINT subset,
                              ID3D11DeviceContext* pd3dDeviceContext,
                              D3DXVECTOR3 minExtentsSize, D3DXVECTOR3 maxExtentsSize, 
                              bool bAdjacent=false,                            
                              UINT iDiffuseSlot = INVALID_SAMPLER_SLOT,
                               UINT iNormalSlot = INVALID_SAMPLER_SLOT,
                               UINT iSpecularSlot = INVALID_SAMPLER_SLOT,
                              UINT iAlphaSlot = INVALID_SAMPLER_SLOT,
                              ALPHA_RENDER_STATE alphaState = ALL_ALPHA);

    void ComputeSubmeshBoundingVolumes();
    

    int getNumSubsets(int iMesh)
    {
        if(iMesh<m_numMeshes)
            return m_numSubsets[iMesh];
        return 0;
    }


    Mesh()
    {
        m_BoundingBoxCenterSubsets = NULL;
        m_BoundingBoxExtentsSubsets = NULL;
        m_numSubsets = NULL;
        m_numMeshes = 0;
        pAlphaMaskRV11s = NULL;
        m_numMaterials = 0;
    }

    ~Mesh()
    {
        for (int meshi=0; meshi <m_numMeshes; ++meshi) 
        {
            delete[] m_BoundingBoxCenterSubsets[meshi];
            delete[] m_BoundingBoxExtentsSubsets[meshi];
        }

        for(unsigned int i=0;i<m_numMaterials; i++)
            if( pAlphaMaskRV11s[i] && !IsErrorResource( pAlphaMaskRV11s[i] ) )
                SAFE_RELEASE(pAlphaMaskRV11s[i]);
        delete[] pAlphaMaskRV11s;

        delete[] m_BoundingBoxCenterSubsets;
        delete[] m_BoundingBoxExtentsSubsets;
        delete[] m_numSubsets;
    }

};



#define MAX_LEVELS 10
struct PropSpecs
{
    int m_up;
    int m_down;
    PropSpecs(){m_up=0; m_down=0;};
    PropSpecs(int up, int down){m_up=up; m_down=down;};
};

struct RenderMesh
{
private:
    float m_translateX;
    float m_translateY;
    float m_translateZ;
    float m_rotateX;
    float m_rotateY;
    float m_rotateZ;
    float m_scaleX;
    float m_scaleY;
    float m_scaleZ;

public:
    Mesh m_Mesh;
    
    D3DXMATRIX m_WMatrix;
    D3DXMATRIX m_Clip2Tex;

    bool m_UseTexture;

    RenderMesh()
    {
        D3DXMatrixIdentity(&m_WMatrix);
        m_Clip2Tex = D3DXMATRIX(
        0.5,    0,    0,   0,
        0,       -0.5,  0,   0,
        0,     0,     1,   0,
        0.5,   0.5,   0,   1 );

        m_UseTexture = false;
    }

    void setWorldMatrix(float scaleX, float scaleY, float scaleZ, float rotateX, float rotateY, float rotateZ, float translateX, float translateY, float translateZ)
    {
        m_translateX = translateX;
        m_translateY = translateY;
        m_translateZ = translateZ;
        m_rotateX = rotateX;
        m_rotateY = rotateY;
        m_rotateZ = rotateZ;
        m_scaleX = scaleX;
        m_scaleY = scaleY;
        m_scaleZ = scaleZ;
        calculateWorldMatrix();
    }

    void calculateWorldMatrix()
    {
        D3DXMATRIX mTranslate, mRotateX, mRotateY, mRotateZ, mScale;
        D3DXMatrixTranslation( &mTranslate, m_translateX, m_translateY, m_translateZ);
        D3DXMatrixRotationX( &mRotateX, m_rotateX);
        D3DXMatrixRotationY( &mRotateY, m_rotateY);
        D3DXMatrixRotationZ( &mRotateZ, m_rotateZ);
        D3DXMatrixScaling( &mScale, m_scaleX, m_scaleY, m_scaleZ);
        m_WMatrix = mTranslate*mScale*mRotateX*mRotateY*mRotateZ;

    }

    void setWorldMatrix(D3DXMATRIX m) { m_WMatrix = m; }

    void setWorldMatrixTranslate(float translateX, float translateY, float translateZ)
    {
        m_translateX = translateX;
        m_translateY = translateY;
        m_translateZ = translateZ;
        calculateWorldMatrix();
    }


    void createMatrices(D3DXMATRIX lightProjection, D3DXMATRIX viewMatrix, D3DXMATRIX* WVMatrix, D3DXMATRIX* WVMatrixIT, D3DXMATRIX* WVPMatrix, D3DXMATRIX* ViewProjClip2Tex )
    {
        *WVMatrix     = m_WMatrix * viewMatrix;
        D3DXMatrixInverse( WVMatrixIT, NULL, WVMatrix ); 
        *WVPMatrix    = (*WVMatrix)  * lightProjection;    
        D3DXMatrixMultiply(ViewProjClip2Tex, WVPMatrix, &m_Clip2Tex);
    }

    ~RenderMesh() 
    {
        m_Mesh.Destroy();
    }

};

struct CB_VS_PER_OBJECT
{
    D3DXMATRIX m_WorldViewProj;
    D3DXMATRIX m_WorldViewIT;
    D3DXMATRIX m_World;
    D3DXMATRIX m_LightViewProjClip2Tex;
};

struct CB_VS_GLOBAL
{
    D3DXVECTOR4 g_lightWorldPos;
    float g_depthBiasFromGUI;
    int bUseSM;
    int g_minCascadeMethod;
    int g_numCascadeLevels;
};

struct CB_SIMPLE_OBJECTS
{
    D3DXMATRIX m_WorldViewProj;
    D3DXVECTOR4 m_color;    
    D3DXVECTOR4 m_lpvScale;
    D3DXVECTOR4 m_sphereScale;
};

struct     VIZ_LPV
{
    D3DXVECTOR3 LPVSpacePos;
    float padding;
};

struct CB_GV
{
    int useGVOcclusion;
    int temp; 
    int useMultipleBounces;
    int copyPropToAccum; // in addition to adding the newly propagated values to the accumulation buffer also add in the original value of the LPV. This is only done for the first propagation iteration, to allow the accumulation buffer to have the original VPLs

    float fluxAmplifier;
    float reflectedLightAmplifier;
    float occlusionAmplifier;
    float temp2;
};



struct CB_RENDER
{
    float diffuseScale;
    int useDiffuseInterreflection;
    float directLight;
    float ambientLight;

    int useFloat4s;
    int useFloats;
    float temp;
    float temp2;

    float invSMSize;
    float normalMapMultiplier;
    int useDirectionalDerivativeClamping;
    float directionalDampingAmount;

    D3DXMATRIX worldToLPVNormTex;
    D3DXMATRIX worldToLPVNormTex1;
    D3DXMATRIX worldToLPVNormTex2;

    D3DXMATRIX worldToLPVNormTexRender;
    D3DXMATRIX worldToLPVNormTexRender1;
    D3DXMATRIX worldToLPVNormTexRender2;
};

struct CB_RENDER_LPV
{
    int g_numCols;    //the number of columns in the flattened 2D LPV
    int g_numRows;    //the number of columns in the flattened 2D LPV
    int LPV2DWidth; //the total width of the flattened 2D LPV
    int LPV2DHeight; //the total height of the flattened 2D LPV

    int LPV3DWidth;    //the width of the LPV in 3D
    int LPV3DHeight;   //the height of the LPV in 3D
    int LPV3DDepth;
    float padding;
};

struct CB_LPV_INITIALIZE
{
    int RSMWidth;
    int RSMHeight;
    float lightStrength;
    float temp;

    D3DXMATRIX InvProj;
};

struct CB_LPV_INITIALIZE3
{
    int g_numCols;    //the number of columns in the flattened 2D LPV
    int g_numRows;    //the number of columns in the flattened 2D LPV
    int LPV2DWidth; //the total width of the flattened 2D LPV
    int LPV2DHeight; //the total height of the flattened 2D LPV

    int LPV3DWidth;    //the width of the LPV in 3D
    int LPV3DHeight;   //the height of the LPV in 3D
    int LPV3DDepth;
    float fluxWeight;

    int useFluxWeight; //flux weight only needed for perspective light matrix not orthogonal
    float padding0;
    float padding1;
    float padding2;
};

struct CB_LPV_PROPAGATE
{
    int g_numCols;    //the number of columns in the flattened 2D LPV
    int g_numRows;    //the number of columns in the flattened 2D LPV
    int LPV2DWidth; //the total width of the flattened 2D LPV
    int LPV2DHeight; //the total height of the flattened 2D LPV

    int LPV3DWidth;    //the width of the LPV in 3D
    int LPV3DHeight;   //the height of the LPV in 3D
    int LPV3DDepth;
    int b_accumulate;
};

struct CB_LPV_INITIALIZE2
{
    D3DXMATRIX g_ViewToLPV;
    D3DXMATRIX g_LPVtoView;
    D3DXVECTOR3 lightDirGridSpace;   //light direction in the grid's space    
    float displacement;
};

struct CB_MESH_RENDER_OPTIONS 
{
    int useTexture;
    int useAlpha;
    float padding1;
    float padding2;
};

//values needed in the propagation step of the LPV
//these values let us save time on the propapation step
//xyz give the normalized direction to each of the 6 pertinent faces of the 6 neighbors of a cube
//solid angle gives the fractional solid angle that that face subtends
struct propagateConsts
{
    D3DXVECTOR4 neighborOffset;
    float solidAngle;
    float x;
    float y;
    float z;
};

struct propagateConsts2
{
    int occlusionOffsetX;
    int occlusionOffsetY;
    int occlusionOffsetZ;
    int occlusionOffsetW;

    int multiBounceOffsetX;
    int multiBounceOffsetY;
    int multiBounceOffsetZ;
    int multiBounceOffsetW;
};

struct CB_LPV_PROPAGATE_GATHER
{
    propagateConsts propConsts[36];
};

struct CB_LPV_PROPAGATE_GATHER2
{
    propagateConsts2 propConsts2[36];
};

struct CB_DRAW_SLICES_3D
{
    float width3D;
    float height3D;
    float depth3D;
    float temp;
};

struct RECONSTRUCT_POS
{
    float farClip;
    float temp1;
    float temp2;
    float temp3;
};

struct CB_SM_TAP_LOCS
{
    int numTaps;
    float filterSize;
    float padding7;
    float padding8;
    D3DXVECTOR4 samples[MAX_P_SAMPLES];
};    

struct CB_INVPROJ_MATRIX
{
    D3DXMATRIX m_InverseProjMatrix;
};

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
HRESULT CompileSolverShaders( ID3D11Device* pd3dDevice );
HRESULT UpdateSceneCB( ID3D11DeviceContext* pd3dContext, D3DXVECTOR3 lightPos, const float lightRadius, float depthBias, bool bUseSM, D3DXMATRIX *WVPMatrix, D3DXMATRIX *WVMatrixIT, D3DXMATRIX *WMatrix, D3DXMATRIX *lightViewProjClip2Tex=NULL);


//creation code
void createPropagationAndGeometryVolumes(ID3D11Device* pd3dDevice);

//propagation code
void propagateLPV(ID3D11DeviceContext* pd3dContext, int iteration, SimpleRT_RGB* LPVPropagate, SimpleRT_RGB* LPVAccumulate, SimpleRT* GV, SimpleRT* GVColor );
void invokeCascadeBasedPropagation(ID3D11DeviceContext* pd3dContext, bool useSingleLPV, int propLevel, LPV_RGB_Cascade* LPVAccumulate, LPV_RGB_Cascade* LPVPropagate, LPV_Cascade* GV, LPV_Cascade* GVColor, int num_iterations);
void propagateLightHierarchy(ID3D11DeviceContext* pd3dContext, LPV_RGB_Hierarchy* LPVAccumulate, LPV_RGB_Hierarchy* LPVPropagate, LPV_Hierarchy* GV, LPV_Hierarchy* GVColor, int level, PropSpecs propAmounts[MAX_LEVELS], int numLevels);
void invokeHierarchyBasedPropagation(ID3D11DeviceContext* pd3dContext, bool useHierarchy, int numHierarchyLevels, int numPropagationStepsLPV, int PropLevel,
                                     LPV_Hierarchy* GV, LPV_Hierarchy* GVColor, LPV_RGB_Hierarchy* LPVAccumulate, LPV_RGB_Hierarchy* LPVPropagate);

void accumulateLPVs(ID3D11DeviceContext* pd3dContext,int batch, SimpleRT_RGB* LPVPropagate, SimpleRT_RGB* LPVAccumulate );

//LPV initialization code
void initializeLPV(ID3D11DeviceContext* pd3dContext, SimpleRT_RGB* LPVAccumulate, SimpleRT_RGB* LPVPropagate, D3DXMATRIX mShadowMatrix, D3DXMATRIX mProjectionMatrix, SimpleRT* RSMColorRT, SimpleRT* RSMNormalRT, DepthRT* g_pShadowMapDS, float fovy, float aspectRatio, float nearPlane, float farPlane, bool useDirectional);

//GV initialization code
void initializeGV(ID3D11DeviceContext* pd3dContext, SimpleRT* GV, SimpleRT* GVColor, SimpleRT* RSMAlbedoRT, SimpleRT* RSMNormalRT, DepthRT* shadowMapDS, float fovy, float aspectRatio, float nearPlane, float farPlane, bool useDirectional, const D3DXMATRIX* projectionMatrix, D3DXMATRIX viewMatrix);

//RSM initialization code
void renderRSM(ID3D11DeviceContext* pd3dContext, bool depthPeel, SimpleRT* pRSMColorRT, SimpleRT* pRSMAlbedoRT, SimpleRT* pRSMNormalRT, DepthRT* pShadowMapDS,DepthRT* pShadowTex,
               const D3DXMATRIX* projectionMatrix, const D3DXMATRIX* viewMatrix, int numMeshes, RenderMesh** meshes, D3D11_VIEWPORT shadowViewport, D3DXVECTOR3 lightPos, const float lightRadius, float depthBiasFromGUI, bool bUseSM);

//just render to the scene shadow map
void renderShadowMap(ID3D11DeviceContext* pd3dContext, DepthRT* pShadowMapDS, const D3DXMATRIX* projectionMatrix, const D3DXMATRIX* viewMatrix, 
                    int numMeshes, RenderMesh** meshes, D3D11_VIEWPORT shadowViewport, D3DXVECTOR3 lightPos, const float lightRadius, float depthBiasFromGUI);

//--------------------------------------------------------------------------------
//variables:
//--------------------------------------------------------------------------------

extern int g_subsetToRender;
extern float g_directLightStrength;

//buffers:
extern ID3D11Buffer*                g_pcbVSPerObject;
extern ID3D11Buffer*                g_pcbVSGlobal;
extern ID3D11Buffer*                g_pcbLPVinitVS;
extern ID3D11Buffer*                g_pcbLPVinitVS2;
extern ID3D11Buffer*                g_pcbLPVinitialize_LPVDims;
extern ID3D11Buffer*                g_pcbLPVpropagate;
extern ID3D11Buffer*                g_pcbRender;
extern ID3D11Buffer*                g_pcbRenderLPV;
extern ID3D11Buffer*                g_pcbRenderLPV2;
extern ID3D11Buffer*                g_pcbLPVpropagateGather;
extern ID3D11Buffer*                g_pcbLPVpropagateGather2;
extern ID3D11Buffer*                g_pcbGV;
extern ID3D11Buffer*                g_pcbSimple;
extern ID3D11Buffer*                g_pcbLPVViz;
extern ID3D11Buffer*                g_pcbMeshRenderOptions;
extern ID3D11Buffer*                g_pcbSlices3D;
extern ID3D11Buffer*                g_reconPos;
extern ID3D11Buffer*                g_pcbInvProjMatrix;

//shaders:
extern ID3D11ComputeShader*         g_pCSAccumulateLPV;
extern ID3D11ComputeShader*         g_pCSAccumulateLPV_singleFloats_8;
extern ID3D11ComputeShader*         g_pCSAccumulateLPV_singleFloats_4;
extern ID3D11ComputeShader*         g_pCSPropagateLPV;
extern ID3D11ComputeShader*         g_pCSPropagateLPVSimple;
extern ID3D11VertexShader*          g_pVSPropagateLPV;
extern ID3D11GeometryShader*        g_pGSPropagateLPV;
extern ID3D11PixelShader*           g_pPSPropagateLPV;
extern ID3D11PixelShader*           g_pPSPropagateLPVSimple;
extern ID3D11VertexShader*          g_pVSInitializeLPV;
extern ID3D11VertexShader*          g_pVSInitializeLPV_Bilinear;
extern ID3D11GeometryShader*        g_pGSInitializeLPV;
extern ID3D11GeometryShader*        g_pGSInitializeLPV_Bilinear;
extern ID3D11PixelShader*           g_pPSInitializeLPV;
extern ID3D11PixelShader*           g_pPSInitializeGV;
extern ID3D11VertexShader*          g_pVSRSM;
extern ID3D11PixelShader*           g_pPSRSM;
extern ID3D11VertexShader*          g_pVSSM;
extern ID3D11VertexShader*          g_pVSRSMDepthPeeling;
extern ID3D11PixelShader*           g_pPSRSMDepthPeel;

//misc:
extern ID3D11InputLayout*            g_pPos3Tex3IL;
extern ID3D11InputLayout*            g_pMeshLayout;
extern D3D11_VIEWPORT                g_LPVViewport;
extern float                         g_VPLDisplacement;
extern ID3D11SamplerState*           g_pDepthPeelingTexSampler;
extern ID3D11SamplerState*           g_pLinearSampler;
extern ID3D11SamplerState*           g_pAnisoSampler;
extern bool                          g_useOcclusion;
extern bool                          g_useMultipleBounces;
extern float                         g_fluxAmplifier;
extern float                         g_reflectedLightAmplifier;
extern float                         g_occlusionAmplifier;
extern bool                          g_useBilinearInit;
extern bool                          g_bilinearInitGVenabled;
extern bool                          g_usePSPropagation;
extern ID3D11BlendState*             g_pNoBlendBS;
extern ID3D11BlendState*             g_pNoBlend1AddBlend2BS;
extern ID3D11BlendState*             g_pMaxBlendBS;
extern bool                          g_useTextureForRSMs;
extern ID3D11DepthStencilState*      g_depthStencilStateDisableDepth;
extern ID3D11DepthStencilState*      g_normalDepthStencilState;
extern ID3D11RasterizerState*        g_pRasterizerStateMainRender;
#endif


