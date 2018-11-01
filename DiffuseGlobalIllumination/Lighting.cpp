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

#pragma warning(disable : 4995)
#include "Lighting.h"
#include "grid.h"
#include "Defines.h"


CDXUTDialogResourceManager  g_DialogResourceManager;    // manager for shared resources of dialogs
CFirstPersonCamera          g_Camera;                   // A first person camera for viewing the scene
CModelViewerCamera          g_LightCamera;              // A way to manipulate the light
CD3DSettingsDlg             g_D3DSettingsDlg;           // Device settings dialog
CDXUTDialog                 g_HUD;                      // dialog for standard controls

ID3D11Device*               g_pd3dDevice;
CDXUTTextHelper*            g_pTxtHelper = NULL;

unsigned int                g_WindowWidth = 1024;
unsigned int                g_WindowHeight = 768;


//scene parameters:
int                         g_subsetToRender = -1;
int                         g_numHierarchyLevels = 2; //number of levels in the hierarchy
float                       g_LPVscale;
D3DXVECTOR3                 g_lightPos;
float                       g_lightRadius;
D3DXVECTOR3                 g_mUp = D3DXVECTOR3(0.0f,1.0f,0.0f);

enum PROP_TYPE
{
    CASCADE,
    HIERARCHY,
};
PROP_TYPE g_propType;
TCHAR* g_tstr_propType[] = 
{ 
    TEXT("Cascade"), 
    TEXT("Hierarchy"),
    NULL
};

bool g_useRSMCascade;
bool g_bUseSingleLPV;

int g_PropLevel;
int g_LPVLevelToInitialize = HIERARCHICAL_INIT_LEVEL;
bool bPropTypeChanged = false;

float g_cascadeScale = 1.75f;
D3DXVECTOR3 g_cascadeTranslate = D3DXVECTOR3(0,0,0);
bool g_movableLPV;

bool g_depthPeelFromCamera = true;
bool g_depthPeelFromLight = false;
int g_numDepthPeelingPasses;
bool g_useDirectionalLight = true;

D3DXVECTOR3 g_center = D3DXVECTOR3(0.0f,0.0f,-10.0f);
bool g_bVisualizeSM;
bool g_showUI = true;
bool g_printHelp = false;
bool g_bVisualizeLPV3D;
bool g_bUseSM;
bool g_bVizLPVBB;
bool g_renderMesh;
float g_depthBiasFromGUI;
int g_smTaps;
bool g_numTapsChanged=false;
float g_smFilterSize = 0.8f;

bool g_useTextureForFinalRender;
bool g_useTextureForRSMs;

float g_ambientLight =0.0f;
float g_directLight;
float g_normalMapMultiplier = 1.0f;
bool g_useDirectionalDerivativeClamping;
float g_directionalDampingAmount;

float g_fCameraFovy;
float g_cameraNear;
float g_cameraFar;

float g_fLightFov;
float g_lightNear;
float g_lightFar;

float g_diffuseInterreflectionScale;
int g_numPropagationStepsLPV;
bool g_useDiffuseInterreflection;
float g_directLightStrength;
Grid* g_grid;
D3DXVECTOR3 g_objectTranslate = D3DXVECTOR3(0,0,0);
bool g_showMovableMesh;
bool g_useRSMForLight = true;
bool g_useFinestGrid = true;
bool g_usePSPropagation = false;


D3DXVECTOR3    g_camViewVector;
D3DXVECTOR3    g_camTranslate;
bool g_resetLPVXform = true;

D3DXVECTOR3 g_vecEye;
D3DXVECTOR3 g_vecAt;

//variables for animating light
int g_lightPreset = 0;
D3DXVECTOR3 g_lightPosDest;
bool g_animateLight = false;



enum MOVABLE_MESH_TYPE
{
    TYPE_BOX = 0,
    TYPE_SPHERE = 1,
};
MOVABLE_MESH_TYPE movableMeshType = TYPE_SPHERE;
TCHAR* g_tstr_movableMeshType[] = 
{ 
    TEXT("Wall"), 
    TEXT("Sphere"),
    NULL
};

//LPVs used in the Hierarchical path
RTCollection_RGB* LPV0Propagate;
RTCollection_RGB* LPV0Accumulate;
RTCollection* GV0; //the geometry volume encoding all the occluders
RTCollection* GV0Color; //the color of the occluders in GV0

//Reflective shadow maps
SimpleRT* g_pRSMColorRT;
SimpleRT* g_pRSMAlbedoRT;
SimpleRT* g_pRSMNormalRT;
DepthRT* g_pShadowMapDS;
DepthRT* g_pDepthPeelingDS[2];

DepthRT* g_pSceneShadowMap;

ID3D11BlendState *g_pNoBlendBS;
ID3D11BlendState *g_pAlphaBlendBS;
ID3D11BlendState *g_pNoBlend1AddBlend2BS;
ID3D11BlendState *g_pMaxBlendBS;
ID3D11DepthStencilState* g_normalDepthStencilState;
ID3D11DepthStencilState* depthStencilStateComparisonLess;
ID3D11DepthStencilState* g_depthStencilStateDisableDepth;

D3DXMATRIX g_pSceneShadowMapProj;
D3DXMATRIX g_pShadowMapProjMatrixSingle;
#define SM_PROJ_MATS_SIZE 2
D3DXMATRIX g_pRSMProjMatrices[SM_PROJ_MATS_SIZE];

//viewports
D3D11_VIEWPORT               g_shadowViewport;
D3D11_VIEWPORT               g_shadowViewportScene;
D3D11_VIEWPORT               g_LPVViewport3D;

//rasterizer states
ID3D11RasterizerState*       g_pRasterizerStateMainRender;
ID3D11RasterizerState*       pRasterizerStateCullNone;
ID3D11RasterizerState*       pRasterizerStateCullFront;
ID3D11RasterizerState*       pRasterizerStateWireFrame;


//------------------------------------------------------
//variables needed for core RSM and LPV functions
//buffers:
ID3D11Buffer*                g_pcbVSPerObject = NULL;
ID3D11Buffer*                g_pcbVSGlobal = NULL;
ID3D11Buffer*                g_pcbLPVinitVS = NULL;
ID3D11Buffer*                g_pcbLPVinitVS2 = NULL;
ID3D11Buffer*                g_pcbLPVinitialize_LPVDims = NULL;
ID3D11Buffer*                g_pcbLPVpropagate=NULL;
ID3D11Buffer*                g_pcbRender = NULL;
ID3D11Buffer*                g_pcbRenderLPV = NULL;
ID3D11Buffer*                g_pcbRenderLPV2 = NULL;
ID3D11Buffer*                g_pcbLPVpropagateGather = NULL;
ID3D11Buffer*                g_pcbLPVpropagateGather2 = NULL;
ID3D11Buffer*                g_pcbGV = NULL;
ID3D11Buffer*                g_pcbSimple = NULL;
ID3D11Buffer*                g_pcbLPVViz = NULL;
ID3D11Buffer*                g_pcbMeshRenderOptions = NULL;
ID3D11Buffer*                g_pcbSlices3D = NULL;
ID3D11Buffer*                g_reconPos = NULL;
ID3D11Buffer*                g_pcbPSSMTapLocations = NULL;
ID3D11Buffer*                g_pcbInvProjMatrix = NULL;


//shaders:
ID3D11ComputeShader*         g_pCSAccumulateLPV = NULL;
ID3D11ComputeShader*         g_pCSAccumulateLPV_singleFloats_8 = NULL;
ID3D11ComputeShader*         g_pCSAccumulateLPV_singleFloats_4 = NULL;
ID3D11ComputeShader*         g_pCSPropagateLPV = NULL;
ID3D11ComputeShader*         g_pCSPropagateLPVSimple = NULL;
ID3D11VertexShader*          g_pVSPropagateLPV = NULL;
ID3D11GeometryShader*        g_pGSPropagateLPV = NULL;
ID3D11PixelShader*           g_pPSPropagateLPV = NULL;
ID3D11PixelShader*           g_pPSPropagateLPVSimple = NULL;
ID3D11VertexShader*          g_pVSInitializeLPV = NULL;
ID3D11VertexShader*          g_pVSInitializeLPV_Bilinear = NULL;
ID3D11GeometryShader*        g_pGSInitializeLPV = NULL;
ID3D11GeometryShader*        g_pGSInitializeLPV_Bilinear = NULL;
ID3D11PixelShader*           g_pPSInitializeLPV = NULL;
ID3D11PixelShader*           g_pPSInitializeGV = NULL;
ID3D11VertexShader*          g_pVSRSM = NULL;
ID3D11PixelShader*           g_pPSRSM = NULL;
ID3D11VertexShader*          g_pVSSM = NULL;
ID3D11VertexShader*          g_pVSRSMDepthPeeling = NULL;
ID3D11PixelShader*           g_pPSRSMDepthPeel = NULL;

//misc:
ID3D11InputLayout*           g_pMeshLayout = NULL;
D3D11_VIEWPORT               g_LPVViewport;
float                        g_VPLDisplacement;
ID3D11SamplerState*          g_pDepthPeelingTexSampler;
ID3D11SamplerState*          g_pLinearSampler;
ID3D11SamplerState*          g_pAnisoSampler;
bool                         g_useOcclusion;
bool                         g_useMultipleBounces;
float                        g_fluxAmplifier;
float                        g_reflectedLightAmplifier;
float                        g_occlusionAmplifier;
bool                         g_useBilinearInit; //only have bilinear init enabled for initializing LPV, not GV
bool                         g_bilinearInitGVenabled = false;
bool                         g_bilinearWasEnabled = false;
//------------------------------------------------------------



TCHAR* g_tstr_vizOptionLabels[] = 
{ 
    TEXT("Color RSM"), 
    TEXT("Normal RSM"),
    TEXT("Albedo RSM"),
    TEXT("Red LPV"),
    TEXT("Green Accum LPV"),
    TEXT("Blue Accum LPV"),
    TEXT("GV"),
    TEXT("GV Color"),
    NULL
};

enum VIZ_OPTIONS
{
    COLOR_RSM = 0,
    NORMAL_RSM = 1,
    ALBEDO_RSM = 2,
    RED_LPV = 3,
    GREEN_ACCUM_LPV = 4,
    BLUE_ACCUM_LPV = 5,
    GV = 6,
    GV_COLOR = 7,
};
VIZ_OPTIONS g_currVizChoice;

enum ADITIONAL_OPTIONS_SELECT
{
    SIMPLE_LIGHT,
    ADVANCED_LIGHT,
    SCENE,
    VIZ_TEXTURES,
};
ADITIONAL_OPTIONS_SELECT g_selectedAdditionalOption;
TCHAR* g_tstr_addOpts[] = 
{ 
    TEXT("Simple Light Setup"), 
    TEXT("Advanced Light Setup"), 
    TEXT("Scene Setup"), 
    TEXT("Viz Intermediates"),
    NULL
};

// scene depth
ID3D11Texture2D*               g_pSceneDepth;
ID3D11ShaderResourceView*      g_pSceneDepthRV;

// screen quad
ID3D11VertexShader*            g_pScreenQuadVS = NULL;
ID3D11VertexShader*            g_pScreenQuadPosTexVS2D = NULL;
ID3D11VertexShader*            g_pScreenQuadPosTexVS3D = NULL;
ID3D11PixelShader*             g_pScreenQuadDisplayPS2D = NULL;
ID3D11PixelShader*             g_pScreenQuadDisplayPS2D_floatTextures = NULL;
ID3D11PixelShader*             g_pScreenQuadReconstructPosFromDepth = NULL;
ID3D11PixelShader*             g_pScreenQuadDisplayPS3D = NULL;
ID3D11PixelShader*             g_pScreenQuadDisplayPS3D_floatTextures = NULL;
ID3D11Buffer*                  g_pScreenQuadVB = NULL;
ID3D11Buffer*                  g_pVizQuadVB = NULL;
ID3D11InputLayout*             g_pScreenQuadIL = NULL;
ID3D11InputLayout*             g_pScreenQuadPosTexIL = NULL;
ID3D11InputLayout*             g_pPos3Tex3IL = NULL;

// for screen quad geometry
struct SimpleVertex
{
    D3DXVECTOR3 Pos;
};

struct TexPosVertex
{
    D3DXVECTOR3 Pos;
    D3DXVECTOR2 Tex;
    TexPosVertex(D3DXVECTOR3 p, D3DXVECTOR2 t) {Pos=p; Tex=t;}
    TexPosVertex() {Pos=D3DXVECTOR3(0.0f,0.0f,0.0f); Tex=D3DXVECTOR2(0.0f,0.0f);}
};

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
enum{
    IDC_TOGGLEFULLSCREEN,
    IDC_TOGGLEREF,
    IDC_CHANGEDEVICE,
    IDC_INFO,
    IDC_DIRECT_SCALE,
    IDC_DIRECT_STATIC,
    IDC_RADIUS_SCALE,
    IDC_RADIUS_STATIC,
    IDC_SMDEPTHBIAS_STATIC,
    IDC_SMDEPTHBIAS_SCALE,
    IDC_VPLDISP_STATIC,
    IDC_VPLDISP_SCALE,
    IDC_GVDISP_STATIC,
    IDC_DIR_SCALE,
    IDC_DIR_STATIC,
    IDC_DIRECT_STRENGTH_STATIC,
    IDC_DIRECT_STRENGTH_SCALE,
    IDC_VIZSM,
    IDC_USESM,
    IDC_COMBO_VIZ,
    IDC_COMBO_MAIN_MESH,
    IDC_COMBO_MAIN_MESH_STATIC,
    IDC_COMBO_ADDITIONAL_OPTIONS,
    IDC_COMBO_OPTIONS_STATIC,
    IDC_VIZLPVBB,
    IDC_VIZMESH,
    IDC_VIZLPV3D,
    IDC_NUMDEPTHPEELINGPASSES_STATIC,
    IDC_NUMDEPTHPEELINGPASSES_SCALE,
    IDC_PROPAGATEITERATIONS_STATIC,
    IDC_PROPAGATEITERATIONS_SCALE,
    IDC_USEDIFINTERREFLECTION,
    IDC_USEDIRLIGHT,
    IDC_PROPLPVNORMALLY,
    IDC_MULTIPLE_BOUNCES,
    IDC_USE_OCCLUSION,
    IDC_USE_BILINEAR_INIT,
    IDC_FLUX_AMP_STATIC,
    IDC_FLUX_AMP_SCALE,
    IDC_REF_LIGHT_AMP_STATIC,
    IDC_REF_LIGHT_AMP_SCALE,
    IDC_OCCLUSION_AMP_STATIC,
    IDC_OCCLUSION_AMP_SCALE,    
    IDC_COMBO_MOVABLEMESH,
    IDC_SHOW_MESH,
    IDC_USE_SINGLELPV,
    IDC_USE_RSM_CASCADE,
    //IDC_LPVSCALE_SCALE,
    //IDC_LPVSCALE_STATIC,
    IDC_LEVEL_SCALE,
    IDC_LEVEL_STATIC,
    IDC_COMBO_PROP_TYPE,
    IDC_MOVABLE_LPV,
    IDC_RESET_LPV_XFORM,
    IDC_SUBSET_TO_RENDER_SCALE,
    IDC_SUBSET_TO_RENDER_STATIC,
    //IDC_SM_TAPS_STATIC,
    //IDC_SM_TAPS_SCALE,
    //IDC_SMFILTERSIZE_SCALE,
    //IDC_SMFILTERSIZE_STATIC,
    IDC_RESET_SETTINGS,
    IDC_TEX_FOR_MAIN_RENDER,
    IDC_TEX_FOR_RSM_RENDER,
    IDC_DIRECTIONAL_CLAMPING,
    IDC_DIR_OFFSET_AMOUNT_SCALE,
    IDC_DIR_OFFSET_AMOUNT_STATIC,
    IDC_ANIMATE_LIGHT,
};

// simple shading
ID3D11VertexShader*            g_pVS = NULL;
ID3D11PixelShader*             g_pPS = NULL;
ID3D11PixelShader*             g_pPS_separateFloatTextures = NULL;
ID3D11VertexShader*            g_pSimpleVS = NULL;
ID3D11PixelShader*             g_pSimplePS = NULL;
ID3D11VertexShader*            g_pVSVizLPV = NULL;
ID3D11PixelShader*             g_pPSVizLPV = NULL;


// meshes
CDXUTSDKMesh                  g_LowResMesh;
CDXUTSDKMesh                  g_MeshArrow;
CDXUTSDKMesh                  g_MeshBox;
D3DXVECTOR3                   g_BoXExtents;
D3DXVECTOR3                   g_BoxCenter;


RenderMesh*                   g_MainMesh;
RenderMesh*                   g_MainMeshSimplified;
RenderMesh*                   g_MainMovableMesh;
RenderMesh*                   g_MovableBoxMesh;


CB_LPV_PROPAGATE_GATHER*      g_LPVPropagateGather = NULL;
CB_LPV_PROPAGATE_GATHER2*     g_LPVPropagateGather2 = NULL;


ID3D11SamplerState*           g_pDefaultSampler;
ID3D11SamplerState*           g_pComparisonSampler;


HRESULT LoadMainMeshes(ID3D11Device* pd3dDevice);

void initializeReflectiveShadowMaps(ID3D11Device* pd3dDevice);
void visualizeMap(RenderTarget* RT, ID3D11DeviceContext* pd3dContext, int numChannels);
void VisualizeBB(ID3D11DeviceContext* pd3dContext, SimpleRT_RGB* LPV, D3DXMATRIX VPMatrix, D3DXVECTOR4 Color);
void updateAdditionalOptions();
void changeSMTaps(ID3D11DeviceContext* pd3dContext);

void initializePresetLight();
void updateLight(float dtime);

////////////////////////////////////////////////////////////////////////////////
//! Updating constant buffers
////////////////////////////////////////////////////////////////////////////////

HRESULT UpdateSceneCB( ID3D11DeviceContext* pd3dContext, D3DXVECTOR3 lightPos, const float lightRadius, float depthBias, bool bUseSM, D3DXMATRIX *WVPMatrix, D3DXMATRIX *WVMatrixIT, D3DXMATRIX *WMatrix, D3DXMATRIX *lightViewProjClip2Tex)
{
    HRESULT hr;
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    V( pd3dContext->Map( g_pcbVSPerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    CB_VS_PER_OBJECT* pVSPerObject = ( CB_VS_PER_OBJECT* )MappedResource.pData;
    D3DXMatrixTranspose( &pVSPerObject->m_WorldViewProj, WVPMatrix );
    D3DXMatrixTranspose( &pVSPerObject->m_WorldViewIT, WVMatrixIT );
    D3DXMatrixTranspose( &pVSPerObject->m_World,WMatrix);
    if(lightViewProjClip2Tex)
        D3DXMatrixTranspose( &pVSPerObject->m_LightViewProjClip2Tex,lightViewProjClip2Tex);
     pd3dContext->Unmap( g_pcbVSPerObject, 0 );
    pd3dContext->VSSetConstantBuffers( 0, 1, &g_pcbVSPerObject );

    V( pd3dContext->Map( g_pcbVSGlobal, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    CB_VS_GLOBAL* pVSGlobal = ( CB_VS_GLOBAL* )MappedResource.pData;

    pVSGlobal->g_lightWorldPos = D3DXVECTOR4(lightPos.x,lightPos.y,lightPos.z,1.f / max(0.000001f, lightRadius*lightRadius));
    pVSGlobal->g_depthBiasFromGUI = depthBias;
    pVSGlobal->bUseSM = bUseSM;
    pVSGlobal->g_minCascadeMethod = g_propType==CASCADE ? 1 : 0;
    pVSGlobal->g_numCascadeLevels = g_numHierarchyLevels;

    pd3dContext->Unmap( g_pcbVSGlobal, 0 );
    pd3dContext->VSSetConstantBuffers( 1, 1, &g_pcbVSGlobal );
    pd3dContext->PSSetConstantBuffers( 1, 1, &g_pcbVSGlobal );

    return hr;
}

void setLPVScale()
{
    if(g_propType == CASCADE) 
        g_LPVscale = 12.f;
    else 
        g_LPVscale = 31.0f;

//    if(g_HUD.GetSlider( IDC_LPVSCALE_SCALE))
//    {
//        WCHAR sz[MAX_PATH];
//        g_HUD.GetSlider( IDC_LPVSCALE_SCALE)->SetValue( (int)g_LPVscale*10 );
//        StringCchPrintf( sz, 100, L"LPV scale: %0.1f", g_LPVscale ); 
//        g_HUD.GetStatic( IDC_LPVSCALE_STATIC )->SetText( sz );
//    }
}

void setFluxAmplifier()
{
    if(g_propType == CASCADE)
        g_fluxAmplifier = 3.8f;
    else
        g_fluxAmplifier = 3.0f;

    if(g_HUD.GetSlider( IDC_FLUX_AMP_SCALE))
    {
        WCHAR sz[MAX_PATH];
        g_HUD.GetSlider( IDC_FLUX_AMP_SCALE)->SetValue((int)(g_fluxAmplifier*100) );
        StringCchPrintf( sz, 100, L"Flux Amplifier: %0.3f", g_fluxAmplifier ); 
        g_HUD.GetStatic( IDC_FLUX_AMP_STATIC)->SetText(sz);
    }
}

void setNumIterations()
{
    if(g_propType == CASCADE)
        g_numPropagationStepsLPV = 8;
    else
        g_numPropagationStepsLPV = 9;

    if(g_HUD.GetSlider( IDC_PROPAGATEITERATIONS_SCALE))
    {
        WCHAR sz[MAX_PATH];
        g_HUD.GetSlider( IDC_PROPAGATEITERATIONS_SCALE)->SetValue( g_numPropagationStepsLPV );
        StringCchPrintf( sz, 100, L"LPV iterations: %d", g_numPropagationStepsLPV ); 
        g_HUD.GetStatic( IDC_PROPAGATEITERATIONS_STATIC)->SetText(sz);
    }
}

void setLightAndCamera()
{
    g_vecEye = D3DXVECTOR3(9.6254f, -3.69081f, 2.0234f);
    g_vecAt = D3DXVECTOR3(8.78361f, -3.68715f, 1.48362f);

    g_Camera.SetScalers(0.005f,3.f);
    g_lightPos = D3DXVECTOR3(-12.7857f, 22.4795f, 7.65355f);
    g_center = D3DXVECTOR3(0.0f,0.0f,0.0f);
    g_mUp = D3DXVECTOR3(1.0f, 0.0f, 0.0f);

    g_LightCamera.SetViewParams(&g_lightPos,&g_center);
    g_LightCamera.SetScalers(0.001f,0.2f);
    g_LightCamera.SetModelCenter( D3DXVECTOR3(0.0f,0.0f,0.0f) );

    g_Camera.SetViewParams( &g_vecEye, &g_vecAt );
    g_Camera.SetEnablePositionMovement(true);

    g_LightCamera.SetEnablePositionMovement(true);
    g_LightCamera.SetButtonMasks(NULL, NULL, MOUSE_LEFT_BUTTON );

    g_lightPreset = 0;
    g_lightPosDest = g_lightPos;
}

////////////////////////////////////////////////////////////////////////////////
//! Draw all objects in the scene
////////////////////////////////////////////////////////////////////////////////
void DrawScene(ID3D11Device* pd3dDevice,  ID3D11DeviceContext* pd3dContext )
{
    pd3dContext->OMSetDepthStencilState(g_normalDepthStencilState, 0);

    D3D11_MAPPED_SUBRESOURCE MappedResource;
    
    if(bPropTypeChanged)
    {
        SAFE_DELETE(LPV0Propagate);
        SAFE_DELETE(LPV0Accumulate);
        SAFE_DELETE(GV0);
        SAFE_DELETE(GV0Color);
        createPropagationAndGeometryVolumes(pd3dDevice);
        if(g_propType == CASCADE) g_LPVLevelToInitialize = g_PropLevel;
        else g_LPVLevelToInitialize = HIERARCHICAL_INIT_LEVEL;
        bPropTypeChanged = false;

        setLPVScale();
        setFluxAmplifier();
        setNumIterations();
    }

    if(g_numTapsChanged) changeSMTaps(pd3dContext);

    updateLight((1.0f / DXUTGetFPS()));

    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();

    float ClearColorRTV[4] = {0.3f, 0.3f, 0.3f, 1.0f};
    pd3dContext->ClearRenderTargetView( pRTV, ClearColorRTV );
    float ClearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float ClearColor2[4] = {0.0f, 0.0f, 0.0f, 0.0f};


    const D3DXMATRIX *p_mview = g_Camera.GetViewMatrix();
    D3DXMATRIX* p_cameraViewMatrix = (D3DXMATRIX*)p_mview;

    const D3DXMATRIX *p_cameraProjectionMatrix = g_Camera.GetProjMatrix();
    D3DXMATRIX VPMatrix = (*p_cameraViewMatrix) * (*p_cameraProjectionMatrix);

    //set up the shadow matrix for the light    
    D3DXMATRIX mShadowMatrix;
    mShadowMatrix = *(g_LightCamera.GetViewMatrix());

    g_MainMovableMesh->setWorldMatrixTranslate(g_objectTranslate.x,g_objectTranslate.y,g_objectTranslate.z);

    g_MovableBoxMesh->setWorldMatrixTranslate(g_objectTranslate.x,g_objectTranslate.y,g_objectTranslate.z);

    //transforms for the LPV
    D3DXMATRIX LPVWorldMatrix;

    if(g_resetLPVXform)
    {
        g_camTranslate = D3DXVECTOR3(g_vecEye.x, g_vecEye.y, g_vecEye.z );
        g_camViewVector = D3DXVECTOR3(1, 0, 0);
        g_resetLPVXform = false;
    }
    if(g_movableLPV)
    {
        D3DXMATRIX viewInverse;
        D3DXMatrixInverse(&viewInverse, NULL, &*p_cameraViewMatrix);
        g_camViewVector = D3DXVECTOR3(p_cameraViewMatrix->_13, p_cameraViewMatrix->_23, p_cameraViewMatrix->_33);
        g_camTranslate = D3DXVECTOR3(viewInverse._41, viewInverse._42, viewInverse._43 );
    }    

     LPV0Propagate->setLPVTransformsRotatedAndOffset (g_LPVscale, g_camTranslate, *p_cameraViewMatrix, g_camViewVector);
     LPV0Accumulate->setLPVTransformsRotatedAndOffset(g_LPVscale, g_camTranslate, *p_cameraViewMatrix, g_camViewVector);
     GV0->setLPVTransformsRotatedAndOffset           (g_LPVscale, g_camTranslate, *p_cameraViewMatrix, g_camViewVector);
     GV0Color->setLPVTransformsRotatedAndOffset      (g_LPVscale, g_camTranslate, *p_cameraViewMatrix, g_camViewVector);    


    CB_SIMPLE_OBJECTS* pPSSimple;

    bool useFloats = false;
    bool useFloat4s = false;
    if(LPV0Accumulate->getRed(0)->getNumChannels()==1 && LPV0Accumulate->getRed(0)->getNumRTs()==4) useFloats = true;
    if(LPV0Accumulate->getRed(0)->getNumChannels()>1 && LPV0Accumulate->getRed(0)->getNumRTs()==1) useFloat4s = true;    

    pd3dContext->Map( g_pcbRender, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    CB_RENDER* pcbRender = ( CB_RENDER* )MappedResource.pData;
    pcbRender->diffuseScale = g_diffuseInterreflectionScale;
    pcbRender->useDiffuseInterreflection = g_useDiffuseInterreflection;
    pcbRender->directLight = g_directLight;
    pcbRender->ambientLight = g_ambientLight;
    pcbRender->useFloat4s = useFloat4s;    
    pcbRender->useFloats = useFloats;
    pcbRender->invSMSize = 1.0f/RSM_RES;
    pcbRender->normalMapMultiplier = g_normalMapMultiplier;
    pcbRender->useDirectionalDerivativeClamping = g_useDirectionalDerivativeClamping;
    pcbRender->directionalDampingAmount = g_directionalDampingAmount;
    if(g_propType == HIERARCHY ||  g_bUseSingleLPV)
    {
        D3DXMatrixTranspose( &pcbRender->worldToLPVNormTex,&(LPV0Propagate->m_collection[g_PropLevel]->getWorldToLPVNormTex()));
        D3DXMatrixTranspose( &pcbRender->worldToLPVNormTexRender,&(LPV0Propagate->m_collection[g_PropLevel]->getWorldToLPVNormTexRender()));
    }
    else
    {
        D3DXMatrixTranspose( &pcbRender->worldToLPVNormTex,&(LPV0Propagate->m_collection[0]->getWorldToLPVNormTex()));
        D3DXMatrixTranspose( &pcbRender->worldToLPVNormTexRender,&(LPV0Propagate->m_collection[0]->getWorldToLPVNormTexRender()));
        if(LPV0Propagate->getNumLevels()>1)
        {
            D3DXMatrixTranspose( &pcbRender->worldToLPVNormTex1,&(LPV0Propagate->m_collection[1]->getWorldToLPVNormTex()));
            D3DXMatrixTranspose( &pcbRender->worldToLPVNormTexRender1,&(LPV0Propagate->m_collection[1]->getWorldToLPVNormTexRender()));
        }
        if(LPV0Propagate->getNumLevels()>2)
        {
            D3DXMatrixTranspose( &pcbRender->worldToLPVNormTex2,&(LPV0Propagate->m_collection[2]->getWorldToLPVNormTex()));
            D3DXMatrixTranspose( &pcbRender->worldToLPVNormTexRender2,&(LPV0Propagate->m_collection[2]->getWorldToLPVNormTexRender()));
        }
    }

    pd3dContext->Unmap( g_pcbRender, 0 );


    D3D11_VIEWPORT viewport;
    UINT nViewports = 1;
    pd3dContext->RSGetViewports(&nViewports, &viewport);


    // IA setup
    UINT Strides[1];
    UINT Offsets[1];
    ID3D11Buffer* pVB[1];
    pVB[0] = g_MainMesh->m_Mesh.GetVB11( 0, 0 );
    Strides[0] = ( UINT )g_MainMesh->m_Mesh.GetVertexStride( 0, 0 );
    Offsets[0] = 0;
    pd3dContext->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
    pd3dContext->IASetIndexBuffer( g_MainMesh->m_Mesh.GetIB11( 0 ), g_MainMesh->m_Mesh.GetIBFormat11( 0 ), 0 );
    pd3dContext->IASetInputLayout( g_pMeshLayout );


    //clear the LPVs
    float ClearColorLPV[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    if(g_propType == HIERARCHY)
    {
        LPV0Propagate->clearRenderTargetView(pd3dContext, ClearColorLPV, true, g_LPVLevelToInitialize);
        LPV0Propagate->clearRenderTargetView(pd3dContext, ClearColorLPV, false, g_LPVLevelToInitialize);
        LPV0Accumulate->clearRenderTargetView(pd3dContext, ClearColorLPV, true, g_LPVLevelToInitialize);
        LPV0Accumulate->clearRenderTargetView(pd3dContext, ClearColorLPV, false, g_LPVLevelToInitialize);
    }
    else if( g_propType == CASCADE )
    {
        if(g_bUseSingleLPV)
        {
            LPV0Propagate->clearRenderTargetView(pd3dContext, ClearColorLPV, true, g_LPVLevelToInitialize);
            LPV0Propagate->clearRenderTargetView(pd3dContext, ClearColorLPV, false, g_LPVLevelToInitialize);
            LPV0Accumulate->clearRenderTargetView(pd3dContext, ClearColorLPV, true, g_LPVLevelToInitialize);
            LPV0Accumulate->clearRenderTargetView(pd3dContext, ClearColorLPV, false, g_LPVLevelToInitialize);    
        }
        else
        for(int level=0; level<LPV0Propagate->getNumLevels(); level++)
        {
            LPV0Propagate->clearRenderTargetView(pd3dContext, ClearColorLPV, true, level);
            LPV0Propagate->clearRenderTargetView(pd3dContext, ClearColorLPV, false, level);
            LPV0Accumulate->clearRenderTargetView(pd3dContext, ClearColorLPV, true, level);
            LPV0Accumulate->clearRenderTargetView(pd3dContext, ClearColorLPV, false, level);
        }
    }


    //render the scene to the shadow map/ RSM and initialize the LPV and GV-----------------

    int numMeshes = 2;
    RenderMesh** meshes = new RenderMesh*[2];
    int m = 0;
    if(g_renderMesh)
        meshes[m++] = g_MainMesh;
    if(g_showMovableMesh)
    {
        if(movableMeshType == TYPE_BOX)
            meshes[m++] = g_MovableBoxMesh;
        else if(movableMeshType == TYPE_SPHERE)
            meshes[m++] = g_MainMovableMesh;
    }
    numMeshes = m;


    float BlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    if(g_renderMesh)
        meshes[0] = g_MainMesh;

    D3DXVECTOR3 lightPos = *g_LightCamera.GetEyePt();
    D3DXVECTOR3 lightAt = *g_LightCamera.GetLookAtPt();
    D3DXVECTOR3 eyePos = *g_Camera.GetEyePt();
    D3DXVECTOR3 eyeAt = *g_Camera.GetLookAtPt();

    //render the shadow map for the scene.
    renderShadowMap(pd3dContext, g_pSceneShadowMap, &g_pSceneShadowMapProj, &mShadowMatrix, numMeshes, meshes, 
                    g_shadowViewportScene, lightPos, g_lightRadius, g_depthBiasFromGUI);
        

    if(g_useDiffuseInterreflection)
    {
        //clear the Geometry Volumes
        float ClearColorGV[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        pd3dContext->ClearRenderTargetView( GV0->getRenderTargetView(g_LPVLevelToInitialize), ClearColorGV );
        pd3dContext->ClearRenderTargetView( GV0Color->getRenderTargetView(g_LPVLevelToInitialize), ClearColorGV );

        //initialize the LPV and GV with the RSM data:
        if(g_useRSMForLight)
        {
            if(g_propType==CASCADE && g_useRSMCascade && !g_bUseSingleLPV)
            {
                //render a separate RSM for each level
                for(int i=0;i<LPV0Accumulate->getNumLevels();i++)
                {
                    D3DXMATRIX* lightViewMatrix = &mShadowMatrix;
                    D3DXMATRIX* lightProjMatrix = &g_pRSMProjMatrices[min(i,SM_PROJ_MATS_SIZE-1)];
                    
                    //render the main RSM
                    renderRSM(pd3dContext, false, g_pRSMColorRT, g_pRSMAlbedoRT, g_pRSMNormalRT, g_pShadowMapDS, g_pShadowMapDS, lightProjMatrix, lightViewMatrix, numMeshes, meshes, g_shadowViewport, lightPos, g_lightRadius, g_depthBiasFromGUI, g_bUseSM);
                    //initialize the LPV
                    initializeLPV(pd3dContext, LPV0Accumulate->m_collection[i], LPV0Propagate->m_collection[i], mShadowMatrix, *lightProjMatrix, g_pRSMColorRT, g_pRSMNormalRT, g_pShadowMapDS, g_fLightFov, 1, g_lightNear, g_lightFar, g_useDirectionalLight);
                    //initialize the GV ( geometry volume) with the RSM data
                    initializeGV(pd3dContext, GV0->m_collection[i], GV0Color->m_collection[i], g_pRSMAlbedoRT, g_pRSMNormalRT, g_pShadowMapDS, g_fLightFov, 1, g_lightNear, g_lightFar, g_useDirectionalLight, lightProjMatrix, *lightViewMatrix);
                }
            }
            else if(g_propType==CASCADE && !g_bUseSingleLPV)
            {
                D3DXMATRIX* lightProjMatrix = &g_pRSMProjMatrices[0];
                D3DXMATRIX* lightViewMatrix = &mShadowMatrix;

                //render the main RSM
                renderRSM(pd3dContext, false, g_pRSMColorRT, g_pRSMAlbedoRT, g_pRSMNormalRT, g_pShadowMapDS, g_pShadowMapDS, lightProjMatrix, lightViewMatrix, numMeshes, meshes, g_shadowViewport, lightPos, g_lightRadius, g_depthBiasFromGUI, g_bUseSM);

                for(int i=0;i<LPV0Accumulate->getNumLevels();i++)
                {
                    //initialize the LPV
                    initializeLPV(pd3dContext, LPV0Accumulate->m_collection[i], LPV0Propagate->m_collection[i], *lightViewMatrix, *lightProjMatrix, g_pRSMColorRT, g_pRSMNormalRT, g_pShadowMapDS, g_fLightFov, 1, g_lightNear, g_lightFar, g_useDirectionalLight);
                    //initialize the GV ( geometry volume) with the RSM data
                    initializeGV(pd3dContext, GV0->m_collection[i], GV0Color->m_collection[i], g_pRSMAlbedoRT, g_pRSMNormalRT, g_pShadowMapDS, g_fLightFov, 1, g_lightNear, g_lightFar, g_useDirectionalLight, lightProjMatrix, *lightViewMatrix);
                }
            }
            else 
            {
                D3DXMATRIX* lightViewMatrix = &mShadowMatrix;
                D3DXMATRIX* lightProjMatrix;
                if(g_propType == HIERARCHY) lightProjMatrix = &g_pShadowMapProjMatrixSingle;
                else lightProjMatrix = &g_pRSMProjMatrices[min(g_LPVLevelToInitialize,SM_PROJ_MATS_SIZE-1)];

                //render the main RSM
                renderRSM(pd3dContext, false, g_pRSMColorRT, g_pRSMAlbedoRT, g_pRSMNormalRT, g_pShadowMapDS, g_pShadowMapDS, lightProjMatrix, lightViewMatrix, numMeshes, meshes, g_shadowViewport, lightPos, g_lightRadius, g_depthBiasFromGUI, g_bUseSM);
                //initialize the LPV
                initializeLPV(pd3dContext, LPV0Accumulate->m_collection[g_LPVLevelToInitialize], LPV0Propagate->m_collection[g_LPVLevelToInitialize], *lightViewMatrix, *lightProjMatrix, g_pRSMColorRT, g_pRSMNormalRT, g_pShadowMapDS, g_fLightFov, 1, g_lightNear, g_lightFar, g_useDirectionalLight);
                //initialize the GV ( geometry volume) with the RSM data
                initializeGV(pd3dContext, GV0->m_collection[g_LPVLevelToInitialize], GV0Color->m_collection[g_LPVLevelToInitialize], g_pRSMAlbedoRT, g_pRSMNormalRT, g_pShadowMapDS, g_fLightFov, 1, g_lightNear, g_lightFar, g_useDirectionalLight, lightProjMatrix, *lightViewMatrix);
            }
        }


        if(g_renderMesh)
            meshes[0] = g_MainMeshSimplified;

    
        //note: the depth peeling code is currently only rendering a single RSM, vs a cascade of them.
        //it is also initializing only one level of the LPV. 
        //it would be better (but slower) to call initializeGV at all levels if we are using a CASCADE

        //depth peeling from the POV of the light
        if(g_depthPeelFromLight && (g_useOcclusion || g_useMultipleBounces) )
        {

            for(int depthPeelingPass = 0; depthPeelingPass<g_numDepthPeelingPasses; depthPeelingPass++)
            {
                D3DXMATRIX* lightViewMatrix = &mShadowMatrix;

                D3DXMATRIX* lightProjMatrix = &g_pShadowMapProjMatrixSingle;
                pd3dContext->ClearDepthStencilView( *g_pDepthPeelingDS[depthPeelingPass%2], D3D11_CLEAR_DEPTH, 1.0, 0 );
                //render the RSMs
                renderRSM(pd3dContext, depthPeelingPass>0, g_pRSMColorRT, g_pRSMAlbedoRT, g_pRSMNormalRT, g_pDepthPeelingDS[depthPeelingPass%2], g_pDepthPeelingDS[(depthPeelingPass+1)%2], lightProjMatrix, lightViewMatrix, numMeshes, meshes, g_shadowViewport, lightPos, g_lightRadius, g_depthBiasFromGUI, g_bUseSM);
                //use RSMs to initialize GV
                initializeGV(pd3dContext, GV0->m_collection[g_LPVLevelToInitialize], GV0Color->m_collection[g_LPVLevelToInitialize],g_pRSMAlbedoRT, g_pRSMNormalRT, g_pDepthPeelingDS[depthPeelingPass%2],g_fLightFov, 1, g_lightNear, g_lightFar, g_useDirectionalLight, lightProjMatrix, *lightViewMatrix);
            }
        }

        //depth peeling from the POV of the camera        
        if(g_depthPeelFromCamera && (g_useOcclusion || g_useMultipleBounces))
        {
            for(int depthPeelingPass = 0; depthPeelingPass<g_numDepthPeelingPasses; depthPeelingPass++)
            {                
                pd3dContext->ClearDepthStencilView( *g_pDepthPeelingDS[depthPeelingPass%2], D3D11_CLEAR_DEPTH, 1.0, 0 );
                //render the RSMs
                renderRSM(pd3dContext, depthPeelingPass>0, g_pRSMColorRT, g_pRSMAlbedoRT, g_pRSMNormalRT, g_pDepthPeelingDS[depthPeelingPass%2], g_pDepthPeelingDS[(depthPeelingPass+1)%2], p_cameraProjectionMatrix, p_cameraViewMatrix, numMeshes, meshes, g_shadowViewport,  lightPos, g_lightRadius, g_depthBiasFromGUI, g_bUseSM); 
                //use RSMs to initialize GV
                initializeGV(pd3dContext, GV0->m_collection[g_LPVLevelToInitialize], GV0Color->m_collection[g_LPVLevelToInitialize], g_pRSMAlbedoRT, g_pRSMNormalRT, g_pDepthPeelingDS[depthPeelingPass%2], g_fCameraFovy, g_WindowWidth / (float)g_WindowHeight, g_cameraNear, g_cameraFar, false, p_cameraProjectionMatrix, *p_cameraViewMatrix);
            }
        }

    }


    //restore rasterizer, depth and blend states
    pd3dContext->RSSetState(g_pRasterizerStateMainRender);
    pd3dContext->OMSetBlendState(g_pNoBlendBS, BlendFactor, 0xffffffff);
    pd3dContext->OMSetDepthStencilState(g_normalDepthStencilState, 0);

    
    //propagate the light --------------------------------------------------------------------
    if(g_useDiffuseInterreflection) //only propagate light if we are actually using diffuse interreflections
    {
        if(g_propType == HIERARCHY)
            invokeHierarchyBasedPropagation(pd3dContext, g_bUseSingleLPV, g_numHierarchyLevels, g_numPropagationStepsLPV, g_PropLevel,
                                            dynamic_cast<LPV_Hierarchy*>(GV0), dynamic_cast<LPV_Hierarchy*>(GV0Color), dynamic_cast<LPV_RGB_Hierarchy*>(LPV0Accumulate),
                                            dynamic_cast<LPV_RGB_Hierarchy*>(LPV0Propagate));
        else
            invokeCascadeBasedPropagation(pd3dContext, g_bUseSingleLPV, g_PropLevel, dynamic_cast<LPV_RGB_Cascade*>(LPV0Accumulate), dynamic_cast<LPV_RGB_Cascade*>(LPV0Propagate),
                                            dynamic_cast<LPV_Cascade*>(GV0), dynamic_cast<LPV_Cascade*>(GV0Color), g_numPropagationStepsLPV);
    }


    //render the scene to the camera---------------------------------------------------

    if(g_renderMesh)
        meshes[0] = g_MainMesh;

    //update the constant buffer for rendering 
    if(g_propType == HIERARCHY || g_bUseSingleLPV)
    {
        pd3dContext->Map( g_pcbRenderLPV, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        CB_RENDER_LPV* pcbRenderLPV = ( CB_RENDER_LPV* )MappedResource.pData;
        pcbRenderLPV->g_numCols = LPV0Accumulate->getNumCols(g_PropLevel);    //the number of columns in the flattened 2D LPV
        pcbRenderLPV->g_numRows = LPV0Accumulate->getNumRows(g_PropLevel);    //the number of columns in the flattened 2D LPV
        pcbRenderLPV->LPV2DWidth =  LPV0Accumulate->getWidth2D(g_PropLevel);  //the total width of the flattened 2D LPV
        pcbRenderLPV->LPV2DHeight = LPV0Accumulate->getHeight2D(g_PropLevel); //the total height of the flattened 2D LPV
        pcbRenderLPV->LPV3DWidth = LPV0Accumulate->getWidth3D(g_PropLevel);   //the width of the LPV in 3D
        pcbRenderLPV->LPV3DHeight = LPV0Accumulate->getHeight3D(g_PropLevel); //the height of the LPV in 3D
        pcbRenderLPV->LPV3DDepth = LPV0Accumulate->getDepth3D(g_PropLevel);   //the depth of the LPV in 3D
        pd3dContext->Unmap( g_pcbRenderLPV, 0 );
    }
    else
    {
        pd3dContext->Map( g_pcbRenderLPV, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        CB_RENDER_LPV* pcbRenderLPV = ( CB_RENDER_LPV* )MappedResource.pData;
        pcbRenderLPV->g_numCols = LPV0Accumulate->getNumCols(0);    //the number of columns in the flattened 2D LPV
        pcbRenderLPV->g_numRows = LPV0Accumulate->getNumRows(0);    //the number of columns in the flattened 2D LPV
        pcbRenderLPV->LPV2DWidth =  LPV0Accumulate->getWidth2D(0);  //the total width of the flattened 2D LPV
        pcbRenderLPV->LPV2DHeight = LPV0Accumulate->getHeight2D(0); //the total height of the flattened 2D LPV
        pcbRenderLPV->LPV3DWidth = LPV0Accumulate->getWidth3D(0);   //the width of the LPV in 3D
        pcbRenderLPV->LPV3DHeight = LPV0Accumulate->getHeight3D(0); //the height of the LPV in 3D
        pcbRenderLPV->LPV3DDepth = LPV0Accumulate->getDepth3D(0);   //the depth of the LPV in 3D
        pd3dContext->Unmap( g_pcbRenderLPV, 0 );

        pd3dContext->Map( g_pcbRenderLPV2, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        CB_RENDER_LPV* pcbRenderLPV2 = ( CB_RENDER_LPV* )MappedResource.pData;
        pcbRenderLPV2->g_numCols = LPV0Accumulate->getNumCols(1);    //the number of columns in the flattened 2D LPV
        pcbRenderLPV2->g_numRows = LPV0Accumulate->getNumRows(1);    //the number of columns in the flattened 2D LPV
        pcbRenderLPV2->LPV2DWidth =  LPV0Accumulate->getWidth2D(1);  //the total width of the flattened 2D LPV
        pcbRenderLPV2->LPV2DHeight = LPV0Accumulate->getHeight2D(1); //the total height of the flattened 2D LPV
        pcbRenderLPV2->LPV3DWidth = LPV0Accumulate->getWidth3D(1);   //the width of the LPV in 3D
        pcbRenderLPV2->LPV3DHeight = LPV0Accumulate->getHeight3D(1); //the height of the LPV in 3D
        pcbRenderLPV2->LPV3DDepth = LPV0Accumulate->getDepth3D(1);   //the depth of the LPV in 3D
        pd3dContext->Unmap( g_pcbRenderLPV2, 0 );


    }

    pd3dContext->RSSetState(g_pRasterizerStateMainRender);

    pd3dContext->IASetInputLayout( g_pMeshLayout );

    // set shader matrices
    pd3dContext->VSSetConstantBuffers( 5, 1, &g_pcbRender );
    pd3dContext->PSSetConstantBuffers( 5, 1, &g_pcbRender );
    pd3dContext->PSSetConstantBuffers( 7, 1, &g_pcbRenderLPV );
    pd3dContext->PSSetConstantBuffers( 8, 1, &g_pcbRenderLPV2);

    // setup the RT
    pd3dContext->OMSetRenderTargets( 1,  &pRTV, pDSV );
    pd3dContext->RSSetViewports(1, &viewport);

    // clear color and depth
    float ClearColorScene[4] = {0.3f, 0.3f, 0.3f, 1.0f};
    pd3dContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );
    
    int srvsUsed = 1;

    //bind the shadow buffer and LPV buffers
    if(LPV0Accumulate->getRed(0)->getNumChannels()>1 && LPV0Accumulate->getRed(0)->getNumRTs()==1)
    {
        if(g_bUseSingleLPV || g_propType == HIERARCHY)
        {
            int level = 0;
            if(g_bUseSingleLPV) level = g_PropLevel;

            ID3D11ShaderResourceView* ppSRVs4[4] = { g_pSceneShadowMap->get_pSRV(0), LPV0Accumulate->getRed(level)->get_pSRV(0), LPV0Accumulate->getGreen(level)->get_pSRV(0), LPV0Accumulate->getBlue(level)->get_pSRV(0)  };
            pd3dContext->PSSetShaderResources( 1, 4, ppSRVs4);
            srvsUsed = 4;
        }
        else if(g_propType == CASCADE)
        {
            ID3D11ShaderResourceView* ppSRVs4[7] = { g_pSceneShadowMap->get_pSRV(0), LPV0Accumulate->getRed(0)->get_pSRV(0), LPV0Accumulate->getGreen(0)->get_pSRV(0), LPV0Accumulate->getBlue(0)->get_pSRV(0),
                                                                                  LPV0Accumulate->getRed(1)->get_pSRV(0), LPV0Accumulate->getGreen(1)->get_pSRV(0), LPV0Accumulate->getBlue(1)->get_pSRV(0)};
            pd3dContext->PSSetShaderResources( 1, 7, ppSRVs4);
            srvsUsed = 7;
        }
    }
    else if(LPV0Accumulate->getRed(0)->getNumChannels()==1 && LPV0Accumulate->getRed(0)->getNumRTs()==4)
    {
        if(g_bUseSingleLPV || g_propType == HIERARCHY)
        {
            int level = 0;
            if(g_bUseSingleLPV) level = g_PropLevel;
            ID3D11ShaderResourceView* ppSRVs_0[4] = { g_pSceneShadowMap->get_pSRV(0), 
                                                      LPV0Accumulate->getRed(level)->get_pSRV(0), LPV0Accumulate->getGreen(level)->get_pSRV(0), LPV0Accumulate->getBlue(level)->get_pSRV(0)  };
            ID3D11ShaderResourceView* ppSRVs_1[3] = { LPV0Accumulate->getRed(level)->get_pSRV(1), LPV0Accumulate->getGreen(level)->get_pSRV(1), LPV0Accumulate->getBlue(level)->get_pSRV(1)  };
            ID3D11ShaderResourceView* ppSRVs_2[3] = { LPV0Accumulate->getRed(level)->get_pSRV(2), LPV0Accumulate->getGreen(level)->get_pSRV(2), LPV0Accumulate->getBlue(level)->get_pSRV(2)  };
            ID3D11ShaderResourceView* ppSRVs_3[3] = { LPV0Accumulate->getRed(level)->get_pSRV(3), LPV0Accumulate->getGreen(level)->get_pSRV(3), LPV0Accumulate->getBlue(level)->get_pSRV(3)  };
            pd3dContext->PSSetShaderResources(  1, 4, ppSRVs_0);
            pd3dContext->PSSetShaderResources( 12, 3, ppSRVs_1);
            pd3dContext->PSSetShaderResources( 21, 3, ppSRVs_2);
            pd3dContext->PSSetShaderResources( 30, 3, ppSRVs_3);
            srvsUsed = 33;
        }
        else if(g_propType == CASCADE)
        {
            assert(0); //this path is not implemented yet, but needs to be implemented
        }
    }
    else
        assert(0); //this path is not implemented and either we are here by mistake, or we have to implement this path because it is really needed

    
    //bind the samplers
    ID3D11SamplerState *states[2] = { g_pLinearSampler, g_pComparisonSampler };
    pd3dContext->PSSetSamplers( 0, 2, states );
    ID3D11SamplerState *state[1] = { g_pAnisoSampler };
    pd3dContext->PSSetSamplers( 3, 1, state );

    // set the shaders
    pd3dContext->VSSetShader( g_pVS, NULL, 0 );
    if(useFloat4s)
        pd3dContext->PSSetShader( g_pPS, NULL, 0 );
    else
        pd3dContext->PSSetShader( g_pPS_separateFloatTextures, NULL, 0 );

    for(int i=0; i<numMeshes; i++)
    {
        RenderMesh* mesh = meshes[i];
        //set the matrices
        D3DXMATRIX ViewProjClip2TexLight, WVPMatrixLight, WVMatrixITLight, WVMatrixLight;
        mesh->createMatrices( g_pSceneShadowMapProj, mShadowMatrix, &WVMatrixLight, &WVMatrixITLight, &WVPMatrixLight, &ViewProjClip2TexLight );
        D3DXMATRIX ViewProjClip2TexCamera, WVPMatrixCamera, WVMatrixITCamera, WVMatrixCamera;
        mesh->createMatrices( *p_cameraProjectionMatrix, *p_cameraViewMatrix, &WVMatrixCamera, &WVMatrixITCamera, &WVPMatrixCamera, &ViewProjClip2TexCamera );

        UpdateSceneCB( pd3dContext, *g_LightCamera.GetEyePt(), g_lightRadius, g_depthBiasFromGUI, g_bUseSM, &(WVPMatrixCamera), &(WVMatrixITCamera), &(mesh->m_WMatrix), &(ViewProjClip2TexLight));

        //first render the meshes with no alpha
        pd3dContext->OMSetBlendState(g_pNoBlendBS, BlendFactor, 0xffffffff);
        pd3dContext->Map( g_pcbMeshRenderOptions, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        CB_MESH_RENDER_OPTIONS* pMeshRenderOptions = ( CB_MESH_RENDER_OPTIONS* )MappedResource.pData;
        if(g_useTextureForFinalRender)
            pMeshRenderOptions->useTexture = mesh->m_UseTexture;
        else 
            pMeshRenderOptions->useTexture = false;    
        pMeshRenderOptions->useAlpha = false;
        pd3dContext->Unmap( g_pcbMeshRenderOptions, 0 );
        pd3dContext->PSSetConstantBuffers( 6, 1, &g_pcbMeshRenderOptions );
        if(g_subsetToRender==-1)
            mesh->m_Mesh.RenderBounded( pd3dContext, D3DXVECTOR3(0,0,0), D3DXVECTOR3(100000,100000,100000), 0, 39, -1, 40, Mesh::NO_ALPHA );
        else
            mesh->m_Mesh.RenderSubsetBounded(0,g_subsetToRender, pd3dContext, D3DXVECTOR3(0,0,0), D3DXVECTOR3(10000,10000,10000), false, 0, 39, -1, 40, Mesh::NO_ALPHA );


        //then render the meshes with alpha
        pd3dContext->OMSetBlendState(g_pAlphaBlendBS, BlendFactor, 0xffffffff);
        pd3dContext->Map( g_pcbMeshRenderOptions, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        pMeshRenderOptions = ( CB_MESH_RENDER_OPTIONS* )MappedResource.pData;
        if(g_useTextureForFinalRender)
            pMeshRenderOptions->useTexture = mesh->m_UseTexture;
        else 
            pMeshRenderOptions->useTexture = false;    
        pMeshRenderOptions->useAlpha = true;
        pd3dContext->Unmap( g_pcbMeshRenderOptions, 0 );
        pd3dContext->PSSetConstantBuffers( 6, 1, &g_pcbMeshRenderOptions );
        if(g_subsetToRender==-1)
            mesh->m_Mesh.RenderBounded( pd3dContext, D3DXVECTOR3(0,0,0), D3DXVECTOR3(100000,100000,100000), 0, 39, -1, 40, Mesh::WITH_ALPHA );
        else
            mesh->m_Mesh.RenderSubsetBounded(0,g_subsetToRender, pd3dContext, D3DXVECTOR3(0,0,0), D3DXVECTOR3(10000,10000,10000), false, 0, 39, -1, 40, Mesh::WITH_ALPHA );
        pd3dContext->OMSetBlendState(g_pNoBlendBS, BlendFactor, 0xffffffff);
    }

    ID3D11ShaderResourceView** ppSRVsNULL = new ID3D11ShaderResourceView*[srvsUsed];
    for(int i=0; i<srvsUsed; i++) ppSRVsNULL[i]=NULL;
    pd3dContext->PSSetShaderResources( 1, srvsUsed, ppSRVsNULL);
    delete[] ppSRVsNULL;


    //render the box visualizing the LPV 
    if(g_bVizLPVBB)
    {
        D3DXVECTOR4 colors[3];
        colors[0] = D3DXVECTOR4(1.0f,0.0f,0.0f,1.0f);
        colors[1] = D3DXVECTOR4(0.0f,1.0f,0.0f,1.0f);
        colors[2] = D3DXVECTOR4(0.0f,0.0f,1.0f,1.0f);

        if(g_propType == HIERARCHY)
            VisualizeBB(pd3dContext, LPV0Propagate->m_collection[0], VPMatrix, colors[0]);
        else
            for(int i=0; i<LPV0Propagate->getNumLevels(); i++)            
                VisualizeBB(pd3dContext, LPV0Propagate->m_collection[i], VPMatrix, colors[min(2,i)]);
    }

    //render the light arrow
    pd3dContext->Map( g_pcbSimple, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    pPSSimple = ( CB_SIMPLE_OBJECTS* )MappedResource.pData;
    //calculate and set the world view projection matrix for transforming the arrow
    D3DXMATRIX objScale, objXForm;
    D3DXMatrixScaling(&objScale,0.06f,0.06f,0.06f);
    D3DXMATRIX mLookAtInv;
    D3DXMatrixInverse(&mLookAtInv, NULL, &mShadowMatrix);
    D3DXMATRIX mWorldS = objScale * mLookAtInv ;
    D3DXMatrixMultiply(&objXForm,&mWorldS,&VPMatrix);
    D3DXMatrixTranspose( &pPSSimple->m_WorldViewProj,&objXForm);
    pPSSimple->m_color = D3DXVECTOR4(1.0f,1.0f,0.0f,1.0f);
    pd3dContext->Unmap( g_pcbSimple, 0 );
    pd3dContext->PSSetConstantBuffers( 4, 1, &g_pcbSimple );
    pd3dContext->VSSetConstantBuffers( 4, 1, &g_pcbSimple );
    pd3dContext->RSSetState(g_pRasterizerStateMainRender);
    pd3dContext->VSSetShader( g_pSimpleVS, NULL, 0 );
    pd3dContext->PSSetShader( g_pSimplePS, NULL, 0 );
    g_MeshArrow.Render( pd3dContext, 0 );


    if(g_bVisualizeLPV3D)
    {
        //render little spheres filling the LPV region showing the value of the LPV at that location in 3D space

        pd3dContext->Map( g_pcbSimple, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        pPSSimple = ( CB_SIMPLE_OBJECTS* )MappedResource.pData;
        D3DXMATRIX wvp;
        D3DXMatrixMultiply(&wvp,&(LPV0Propagate->m_collection[0]->getWorldToLPVBB()),&VPMatrix);
        D3DXMatrixTranspose( &pPSSimple->m_WorldViewProj,&wvp);
        pPSSimple->m_color = D3DXVECTOR4(1.0f,0.0f,0.0f,1.0f);
        pPSSimple->m_sphereScale = D3DXVECTOR4(0.002f * g_LPVscale/23.0f,0.002f * g_LPVscale/23.0f,0.002f * g_LPVscale/23.0f,0.002f * g_LPVscale/23.0f);
        pd3dContext->Unmap( g_pcbSimple, 0 );
        pd3dContext->VSSetConstantBuffers( 4, 1, &g_pcbSimple );

    pd3dContext->VSSetConstantBuffers( 5, 1, &g_pcbRender );
    pd3dContext->PSSetConstantBuffers( 5, 1, &g_pcbRender );
    pd3dContext->PSSetConstantBuffers( 7, 1, &g_pcbRenderLPV ); //note: right now this is not visualizing the right level of the cascade

    if(g_currVizChoice == GV_COLOR )
        pd3dContext->PSSetShaderResources( 11, 1, GV0Color->getShaderResourceViewpp(g_PropLevel) );
    else if(g_currVizChoice == GV )
        pd3dContext->PSSetShaderResources( 11, 1, GV0->getShaderResourceViewpp(g_PropLevel) );
    else if(g_currVizChoice == GREEN_ACCUM_LPV )
         pd3dContext->PSSetShaderResources( 11, 1, LPV0Accumulate->getGreen(g_PropLevel)->get_ppSRV(0) );
    else if(g_currVizChoice == BLUE_ACCUM_LPV )
         pd3dContext->PSSetShaderResources( 11, 1, LPV0Accumulate->getBlue(g_PropLevel)->get_ppSRV(0) );
    else pd3dContext->PSSetShaderResources( 11, 1, LPV0Propagate->getRed(g_PropLevel)->get_ppSRV(0) );


        pd3dContext->RSSetState(g_pRasterizerStateMainRender);
        pd3dContext->VSSetShader( g_pVSVizLPV, NULL, 0 );
        pd3dContext->PSSetShader( g_pPSVizLPV, NULL, 0 );

        ID3D11SamplerState *states[1] = { g_pDefaultSampler };
        pd3dContext->PSSetSamplers( 0, 1, states );

        int xLimit = LPV0Propagate->m_collection[g_PropLevel]->getWidth3D();
        int yLimit = LPV0Propagate->m_collection[g_PropLevel]->getHeight3D();
        int zLimit = LPV0Propagate->m_collection[g_PropLevel]->getDepth3D();

        for(int x=0; x<xLimit; x++)
            for(int y=0; y<yLimit; y++)
                for(int z=0; z<zLimit; z++)
                {

                    pd3dContext->Map( g_pcbLPVViz, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
                    VIZ_LPV* cbLPVIndex = ( VIZ_LPV* )MappedResource.pData;
                    cbLPVIndex->LPVSpacePos = D3DXVECTOR3((float)x/xLimit,(float)y/yLimit,(float)z/zLimit);
                    pd3dContext->Unmap( g_pcbLPVViz, 0 );
                    pd3dContext->VSSetConstantBuffers( 2, 1, &g_pcbLPVViz );

                    g_LowResMesh.Render( pd3dContext, 0 );
                }

        ID3D11ShaderResourceView* ppSRVsNULL1[1] = { NULL };
        pd3dContext->PSSetShaderResources( 11, 1, ppSRVsNULL1);

    }

    if(g_bVisualizeSM)
    {
        if(g_currVizChoice == COLOR_RSM )
            visualizeMap(g_pRSMColorRT, pd3dContext, g_pRSMColorRT->getNumChannels() );
        else if(g_currVizChoice == NORMAL_RSM )
            visualizeMap(g_pRSMNormalRT, pd3dContext, g_pRSMNormalRT->getNumChannels() );
        else if(g_currVizChoice == ALBEDO_RSM )
            visualizeMap(g_pRSMAlbedoRT, pd3dContext, g_pRSMAlbedoRT->getNumChannels() );

        else if(g_currVizChoice == RED_LPV )
            visualizeMap(LPV0Propagate->getRed(g_PropLevel), pd3dContext, LPV0Propagate->getRed(g_PropLevel)->getNumChannels() );
        else if(g_currVizChoice == GREEN_ACCUM_LPV )
            visualizeMap(LPV0Accumulate->getGreen(g_PropLevel), pd3dContext, LPV0Accumulate->getGreen(g_PropLevel)->getNumChannels() );
        else if(g_currVizChoice == BLUE_ACCUM_LPV )
            visualizeMap(LPV0Accumulate->getBlue(g_PropLevel), pd3dContext, LPV0Accumulate->getBlue(g_PropLevel)->getNumChannels());

        else if(g_currVizChoice == GV )
            visualizeMap(GV0->getRenderTarget(g_PropLevel),pd3dContext, 1);
        else if(g_currVizChoice == GV_COLOR )
            visualizeMap(GV0Color->getRenderTarget(g_PropLevel),pd3dContext, 1);

    }

    delete[] meshes;
}

void resetSettingValues()
{
    setLightAndCamera();

    if(g_propType == HIERARCHY ) bPropTypeChanged = true;
    g_propType = CASCADE;
    setLPVScale();
    setFluxAmplifier();
    setNumIterations();

    g_selectedAdditionalOption = SIMPLE_LIGHT;
    g_useDiffuseInterreflection = true;
    g_directLightStrength = 1.0f;
    g_diffuseInterreflectionScale = 1.5f;
    g_directLight = 2.22f;
    g_lightRadius = 150.0f;
    g_useBilinearInit = false;
    g_useOcclusion = false;
    g_useMultipleBounces = false;
    g_numDepthPeelingPasses = 1;
    g_reflectedLightAmplifier = 4.8f;
    g_occlusionAmplifier = 0.8f;
    g_movableLPV = true;
    g_resetLPVXform = true;
    g_bUseSingleLPV = false;
    g_useRSMCascade = true;
    g_PropLevel = 0;
    g_VPLDisplacement = 1.f;
    g_bVizLPVBB = false;
    g_bUseSM = true;
    g_renderMesh = true;
    g_useTextureForFinalRender = true;
    g_useTextureForRSMs = true;
    g_showMovableMesh = false;
    g_smTaps = 8;
    g_smFilterSize = 0.8f;
    g_currVizChoice = RED_LPV;
    g_bVisualizeSM = false;
    g_bVisualizeLPV3D = false;
    if(g_useDirectionalLight)
        g_depthBiasFromGUI = 0.0021f;
    else
        g_depthBiasFromGUI = 0.00001f;
    g_useDirectionalDerivativeClamping = false;
    g_directionalDampingAmount = 0.1f;
}

void resetSettingsUI()
{
    g_HUD.GetComboBox( IDC_COMBO_ADDITIONAL_OPTIONS )->SetSelectedByIndex( g_selectedAdditionalOption );
    g_HUD.GetCheckBox( IDC_USEDIFINTERREFLECTION)->SetChecked(g_useDiffuseInterreflection);    
    g_HUD.GetSlider( IDC_DIR_SCALE)->SetValue((int)(g_diffuseInterreflectionScale*1000) );
    g_HUD.GetStatic( IDC_DIR_STATIC )->SetEnabled(g_useDiffuseInterreflection);
    g_HUD.GetSlider( IDC_DIRECT_STRENGTH_SCALE)->SetValue((int)(g_directLightStrength*10) );
    g_HUD.GetStatic( IDC_DIRECT_STRENGTH_STATIC )->SetEnabled(g_useDiffuseInterreflection);
    g_HUD.GetSlider( IDC_DIR_SCALE )->SetEnabled(g_useDiffuseInterreflection);
    g_HUD.GetSlider( IDC_DIRECT_SCALE)->SetValue( (int)(g_directLight*1000) );
    g_HUD.GetSlider( IDC_RADIUS_SCALE)->SetValue( (int)(g_lightRadius*100) );
    g_HUD.GetCheckBox( IDC_USE_BILINEAR_INIT )->SetChecked(g_useBilinearInit);
    g_HUD.GetCheckBox( IDC_USE_OCCLUSION )->SetChecked(g_useOcclusion);
    g_HUD.GetCheckBox( IDC_MULTIPLE_BOUNCES )->SetChecked(g_useMultipleBounces);
    g_HUD.GetSlider( IDC_NUMDEPTHPEELINGPASSES_SCALE)->SetValue(g_numDepthPeelingPasses );
    g_HUD.GetStatic( IDC_NUMDEPTHPEELINGPASSES_STATIC )->SetEnabled(g_useOcclusion || g_useMultipleBounces);
    g_HUD.GetSlider( IDC_NUMDEPTHPEELINGPASSES_SCALE )->SetEnabled(g_useOcclusion || g_useMultipleBounces);
    g_HUD.GetSlider( IDC_FLUX_AMP_SCALE)->SetValue((int)(g_fluxAmplifier*100) );
    g_HUD.GetSlider( IDC_REF_LIGHT_AMP_SCALE)->SetValue((int)(g_reflectedLightAmplifier*100) );
    g_HUD.GetSlider( IDC_OCCLUSION_AMP_SCALE)->SetValue((int)(g_occlusionAmplifier*100) );
    g_HUD.GetComboBox( IDC_COMBO_PROP_TYPE )->SetSelectedByIndex( g_propType );
    g_HUD.GetCheckBox( IDC_MOVABLE_LPV)->SetChecked(g_movableLPV);
    g_HUD.GetCheckBox( IDC_USE_SINGLELPV)->SetChecked(g_bUseSingleLPV);
    g_HUD.GetCheckBox(IDC_USE_RSM_CASCADE)->SetChecked(g_useRSMCascade);
    g_HUD.GetSlider( IDC_LEVEL_SCALE)->SetValue( g_PropLevel );
    g_HUD.GetSlider( IDC_LEVEL_SCALE )->SetEnabled(g_bUseSingleLPV);
    g_HUD.GetStatic( IDC_LEVEL_STATIC )->SetEnabled(g_bUseSingleLPV);
    g_HUD.GetSlider( IDC_VPLDISP_SCALE)->SetValue((int)(g_VPLDisplacement*100) );
    g_HUD.GetSlider( IDC_PROPAGATEITERATIONS_SCALE)->SetValue( g_numPropagationStepsLPV );
    //g_HUD.GetSlider( IDC_LPVSCALE_SCALE)->SetValue( (int)g_LPVscale*10 );
    g_HUD.GetCheckBox( IDC_VIZLPVBB )->SetChecked(g_bVizLPVBB);
    g_HUD.GetCheckBox( IDC_DIRECTIONAL_CLAMPING )->SetChecked(g_useDirectionalDerivativeClamping);
    g_HUD.GetSlider( IDC_DIR_OFFSET_AMOUNT_SCALE )->SetValue((int)(g_directionalDampingAmount*100.f));
    g_HUD.GetCheckBox( IDC_USESM)->SetChecked(g_bUseSM );
    g_HUD.GetCheckBox( IDC_VIZMESH)->SetChecked(g_renderMesh);
    g_HUD.GetCheckBox( IDC_ANIMATE_LIGHT)->SetChecked(g_animateLight);
    g_HUD.GetCheckBox( IDC_TEX_FOR_MAIN_RENDER)->SetChecked( g_useTextureForFinalRender );
    g_HUD.GetCheckBox( IDC_TEX_FOR_RSM_RENDER)->SetChecked( g_useTextureForRSMs );
    g_HUD.GetCheckBox( IDC_SHOW_MESH)->SetChecked(g_showMovableMesh);
    g_HUD.GetSlider( IDC_SMDEPTHBIAS_SCALE)->SetValue((int)(g_depthBiasFromGUI*1000000) );
    int smTaps=0;
    if(g_smTaps == 1) smTaps = 0;
    else if(g_smTaps == 8) smTaps = 1;
    else if(g_smTaps == 16) smTaps = 2;
    else if(g_smTaps == 28) smTaps = 3;
    //g_HUD.GetSlider( IDC_SM_TAPS_SCALE)->SetValue( smTaps );
    //g_HUD.GetSlider( IDC_SMFILTERSIZE_SCALE)->SetValue((int)(g_smFilterSize*10.f) );
    g_HUD.GetComboBox( IDC_COMBO_VIZ )->SetSelectedByIndex( g_currVizChoice );
    g_HUD.GetCheckBox( IDC_VIZSM)->SetChecked( g_bVisualizeSM );
    g_HUD.GetCheckBox( IDC_VIZLPV3D)->SetChecked( g_bVisualizeLPV3D);
    g_HUD.GetSlider( IDC_DIR_OFFSET_AMOUNT_SCALE)->SetValue((int)(g_directionalDampingAmount*100) );

    g_HUD.GetSlider( IDC_REF_LIGHT_AMP_SCALE )->SetEnabled(g_useMultipleBounces);
    g_HUD.GetSlider( IDC_OCCLUSION_AMP_SCALE )->SetEnabled(g_useOcclusion);
    g_HUD.GetStatic( IDC_REF_LIGHT_AMP_STATIC )->SetEnabled(g_useMultipleBounces);
    g_HUD.GetStatic( IDC_OCCLUSION_AMP_STATIC )->SetEnabled(g_useOcclusion);

    WCHAR sz[MAX_PATH];
    StringCchPrintf( sz, 100, L"interreflect contribution: %0.3f", g_diffuseInterreflectionScale ); 
    g_HUD.GetStatic( IDC_DIR_STATIC)->SetText(sz);
    StringCchPrintf( sz, 100, L"directLight contribution: %0.3f", g_directLight ); 
    g_HUD.GetStatic( IDC_DIRECT_STATIC)->SetText(sz);
    StringCchPrintf( sz, 100, L"Light radius: %0.2f", g_lightRadius ); 
    g_HUD.GetStatic( IDC_RADIUS_STATIC)->SetText(sz);
    StringCchPrintf( sz, 100, L"depth peel layers: %d", g_numDepthPeelingPasses ); 
    g_HUD.GetStatic( IDC_NUMDEPTHPEELINGPASSES_STATIC)->SetText(sz);
    StringCchPrintf( sz, 100, L"Flux Amplifier: %0.3f", g_fluxAmplifier ); 
    g_HUD.GetStatic( IDC_FLUX_AMP_STATIC)->SetText(sz);
    StringCchPrintf( sz, 100, L"Reflected Light Amp: %0.3f", g_reflectedLightAmplifier ); 
    g_HUD.GetStatic( IDC_REF_LIGHT_AMP_STATIC)->SetText(sz);
    StringCchPrintf( sz, 100, L"Occlusion Amplifier: %0.3f", g_occlusionAmplifier ); 
    g_HUD.GetStatic( IDC_OCCLUSION_AMP_STATIC)->SetText(sz);
    StringCchPrintf( sz, 100, L"LPV level: %d", g_PropLevel ); 
    g_HUD.GetStatic( IDC_LEVEL_STATIC)->SetText(sz);
    StringCchPrintf( sz, 100, L"VPL displace: %0.3f", g_VPLDisplacement ); 
    g_HUD.GetStatic( IDC_VPLDISP_STATIC)->SetText(sz);
    StringCchPrintf( sz, 100, L"LPV iterations: %d", g_numPropagationStepsLPV ); 
    g_HUD.GetStatic( IDC_PROPAGATEITERATIONS_STATIC)->SetText(sz);
    //StringCchPrintf( sz, 100, L"LPV scale: %d", (int)g_LPVscale ); 
    //g_HUD.GetStatic( IDC_LPVSCALE_STATIC)->SetText(sz);
    StringCchPrintf( sz, 100, L"Shadow Depth Bias: %0.4f", g_depthBiasFromGUI ); 
    g_HUD.GetStatic( IDC_SMDEPTHBIAS_STATIC)->SetText(sz);
    //StringCchPrintf( sz, 100, L"Shadow map taps: %d", g_smTaps ); 
    //g_HUD.GetStatic( IDC_SM_TAPS_STATIC)->SetText(sz);
    //StringCchPrintf( sz, 100, L"Shadowmap filter Size: %0.3f", g_smFilterSize ); 
    //g_HUD.GetStatic( IDC_SMFILTERSIZE_STATIC)->SetText(sz);
    StringCchPrintf( sz, 100, L"Dir Damping Offset: %0.3f", g_directionalDampingAmount ); 
    g_HUD.GetStatic( IDC_DIR_OFFSET_AMOUNT_STATIC)->SetText(sz);

    updateAdditionalOptions();

}

void visualizeMap(RenderTarget* RT, ID3D11DeviceContext* pd3dContext, int numChannels)
{
    // draw the 2d texture 
    UINT stride = sizeof( TexPosVertex );
    UINT offset = 0;
    pd3dContext->IASetInputLayout( g_pScreenQuadPosTexIL );
    pd3dContext->IASetVertexBuffers( 0, 1, &g_pVizQuadVB, &stride, &offset ); 
    pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP );

    int numRTs = RT->getNumRTs();
    assert(numRTs<=4); //the shaders are not setup for more than this amount of textures
    ID3D11ShaderResourceView** ppSRV = new ID3D11ShaderResourceView*[numRTs];
    for(int i=0; i<numRTs; i++)
        ppSRV[i] = RT->get_pSRV(i);
    pd3dContext->PSSetShaderResources( 5, numRTs, ppSRV );


    ID3D11SamplerState *states[1] = { g_pDefaultSampler };
    pd3dContext->PSSetSamplers( 0, 1, states );
    
    D3D11_MAPPED_SUBRESOURCE MappedResource;

    if(!RT->is2DTexture())
    {
        pd3dContext->VSSetShader( g_pScreenQuadPosTexVS3D, NULL, 0 );
        if(numRTs==1)
            pd3dContext->PSSetShader( g_pScreenQuadDisplayPS3D, NULL, 0 );
        else if(numChannels==1)
            pd3dContext->PSSetShader( g_pScreenQuadDisplayPS3D_floatTextures, NULL, 0 );    

        pd3dContext->Map( g_pcbSlices3D, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        CB_DRAW_SLICES_3D* pcbSlices3D = ( CB_DRAW_SLICES_3D* )MappedResource.pData;
        pcbSlices3D->width3D = (float)RT->m_width3D;
        pcbSlices3D->height3D = (float)RT->m_height3D;
        pcbSlices3D->depth3D = (float)RT->m_depth3D;
        pd3dContext->Unmap( g_pcbSlices3D, 0 );
        pd3dContext->VSSetConstantBuffers( 0, 1, &g_pcbSlices3D );
        
        g_grid->DrawSlicesToScreen();
    }
    else
    {
        pd3dContext->VSSetShader( g_pScreenQuadPosTexVS2D, NULL, 0 );
        if(numRTs==1)
            pd3dContext->PSSetShader( g_pScreenQuadDisplayPS2D, NULL, 0 );
        else  if(numChannels==1)
            pd3dContext->PSSetShader( g_pScreenQuadDisplayPS2D_floatTextures, NULL, 0 );

        pd3dContext->Draw( 4, 0 );
    }

    ID3D11ShaderResourceView* ppNullSRV[1] = { NULL };
    pd3dContext->PSSetShaderResources( 5, 1, ppNullSRV );
}

void VisualizeBB(ID3D11DeviceContext* pd3dContext, SimpleRT_RGB* LPV, D3DXMATRIX VPMatrix, D3DXVECTOR4 color)
{    
    //translate the box to the center and scale it to be 1.0 in size
    //then transform the box by the transform for the LPV

    D3D11_MAPPED_SUBRESOURCE MappedResource;
    pd3dContext->Map( g_pcbSimple, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    CB_SIMPLE_OBJECTS* pPSSimple = ( CB_SIMPLE_OBJECTS* )MappedResource.pData;
    D3DXMATRIX objScale, objXForm, objTranslate;
    D3DXMatrixScaling(&objScale,0.5f/g_BoXExtents.x,0.5f/g_BoXExtents.y,0.5f/g_BoXExtents.z);
    D3DXMatrixTranslation(&objTranslate,g_BoxCenter.x,g_BoxCenter.y,g_BoxCenter.z);
    D3DXMatrixMultiply(&objXForm,&objTranslate,&objScale);
    D3DXMatrixMultiply(&objXForm,&objXForm,&(LPV->getWorldToLPVBB()));
    D3DXMatrixMultiply(&objXForm,&objXForm,&VPMatrix);
    D3DXMatrixTranspose( &pPSSimple->m_WorldViewProj,&objXForm);
    pPSSimple->m_color = color;
    pd3dContext->Unmap( g_pcbSimple, 0 );
    pd3dContext->PSSetConstantBuffers( 4, 1, &g_pcbSimple );
    pd3dContext->VSSetConstantBuffers( 4, 1, &g_pcbSimple );
    pd3dContext->RSSetState(pRasterizerStateWireFrame);
    pd3dContext->VSSetShader( g_pSimpleVS, NULL, 0 );
    pd3dContext->PSSetShader( g_pSimplePS, NULL, 0 );
    g_MeshBox.Render(pd3dContext, 0);
}

void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    g_pTxtHelper->DrawFormattedTextLine( L"Time: %0.1f ms", (1.0f / DXUTGetFPS())*1000.0f );

    g_pTxtHelper->DrawTextLine(L"Press 'h' to toggle help, Press 'y' to toggle UI");

    if(g_printHelp)
    {
         g_pTxtHelper->DrawTextLine(L" ");
        g_pTxtHelper->DrawTextLine(L"A,D,W,S,Q,E: Move Camera");
        g_pTxtHelper->DrawTextLine(L"Left mouse drag: Rotate Camera");
        g_pTxtHelper->DrawTextLine(L"Left mouse drag + SHIFT: Move Light");
        g_pTxtHelper->DrawTextLine(L"I,K,J,L,U,O: Move Flag");
    }

    g_pTxtHelper->End();


}

void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }
    
    DrawScene( pd3dDevice, pd3dImmediateContext );
        
    // draw UI
    if(g_showUI)
    {
        DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
        g_HUD.OnRender( fElapsedTime );
        RenderText();
        DXUT_EndPerfEvent();
    }
}

void updateProjectionMatrices()
{
    // setup the camera's projection parameters
    g_fCameraFovy        = 55 * D3DX_PI / 180;
  float g_fAspectRatio = g_WindowWidth / (float)g_WindowHeight;
  g_cameraNear        = 0.01f;
  g_cameraFar         = 200.0f;

  g_Camera.SetProjParams ( g_fCameraFovy, g_fAspectRatio, g_cameraNear, g_cameraFar );

  
    //set the light camera's projection parameters
    g_lightNear = 0.01f;
    g_lightFar = max(100.f, g_lightRadius);

    {
        if(g_useDirectionalLight)
        {
#define RSM_CASCADE_0_SIZE 15.f
#define RSM_CASCADE_1_SIZE 25.f

            D3DXMatrixOrthoLH(&g_pSceneShadowMapProj, 20, 20, g_lightNear, g_lightFar);     //matrix for the scene shadow map
            D3DXMatrixOrthoLH(&g_pShadowMapProjMatrixSingle, 20, 20, g_lightNear, g_lightFar); //matrix to use if not using cascaded RSMs
            //matrices for the cascaded RSM
            D3DXMatrixOrthoLH(&g_pRSMProjMatrices[0], RSM_CASCADE_0_SIZE, RSM_CASCADE_0_SIZE, g_lightNear, g_lightFar); //first level of the cascade
            D3DXMatrixOrthoLH(&g_pRSMProjMatrices[1], RSM_CASCADE_1_SIZE, RSM_CASCADE_1_SIZE, g_lightNear, g_lightFar); //second level of the cascade

            const float fCascadeSize[SM_PROJ_MATS_SIZE] = { RSM_CASCADE_0_SIZE, RSM_CASCADE_1_SIZE };
            const float fCascadeTexelSizeSnapping[SM_PROJ_MATS_SIZE] = { RSM_CASCADE_0_SIZE / RSM_RES * 4.f, RSM_CASCADE_1_SIZE / RSM_RES * 4.f };

            if(g_movableLPV)
            {
                // Get light rotation matrix
                D3DXMATRIXA16 mShadowMatrix;
                mShadowMatrix = *(g_LightCamera.GetViewMatrix());
                mShadowMatrix._41 = 0.f;
                mShadowMatrix._42 = 0.f;
                mShadowMatrix._43 = 0.f;

                // get camera position and direction
                const D3DXVECTOR3 vEye = *g_Camera.GetEyePt();
                D3DXVECTOR3 vDir = (*g_Camera.GetLookAtPt() - *g_Camera.GetEyePt());
                D3DXVec3Normalize(&vDir, &vDir);

                // Move RSM cascades with camera with 4-texel snapping
                D3DXVECTOR3 vEyeCascade[SM_PROJ_MATS_SIZE];
                for(int i=0;i<SM_PROJ_MATS_SIZE;++i)
                {
                    // Shift the center of the RSM for 20% towards the view direction
                    D3DXVECTOR3 vEyeOffsetted = vEye + vDir * fCascadeSize[i] * .2f;
                    vEyeOffsetted.z = 0;
                    D3DXVECTOR4 vEye4;
                    D3DXVec3Transform(&vEye4, &vEyeOffsetted, &mShadowMatrix);

                    // Perform a 4-texels snapping in order to provide coherent movement-independent rasterization
                    vEyeCascade[i].x = floorf(vEye4.x / fCascadeTexelSizeSnapping[i]) * fCascadeTexelSizeSnapping[i];
                    vEyeCascade[i].y = floorf(vEye4.y / fCascadeTexelSizeSnapping[i]) * fCascadeTexelSizeSnapping[i];
                    vEyeCascade[i].z = floorf(vEye4.z / fCascadeTexelSizeSnapping[i]) * fCascadeTexelSizeSnapping[i];
                }

                // Translate the projection matrix of each RSM cascade
                D3DXMATRIXA16 mxTrans;
                for(int i=0;i<SM_PROJ_MATS_SIZE;++i)
                {
                    D3DXMatrixTranslation(&mxTrans, -vEyeCascade[i].x, -vEyeCascade[i].y, -vEyeCascade[i].z);
                    D3DXMatrixMultiply(&g_pRSMProjMatrices[i], &mxTrans, &g_pRSMProjMatrices[i]);
                }
            }
        }
        else
        {
            g_fLightFov =  D3DX_PI / 1.4f;
            D3DXMatrixPerspectiveFovLH( &g_pSceneShadowMapProj, D3DX_PI / 1.4f, 1, g_lightNear, g_lightFar );     //matrix for the scene shadow map
            D3DXMatrixPerspectiveFovLH( &g_pShadowMapProjMatrixSingle, D3DX_PI / 1.4f, 1, g_lightNear, g_lightFar ); //matrix to use if not using cascaded RSMs    
            //matrices for the cascaded RSM
            D3DXMatrixPerspectiveFovLH( &g_pRSMProjMatrices[0], D3DX_PI / 1.4f, 1, g_lightNear, g_lightFar );//first level of the cascade
            D3DXMatrixPerspectiveFovLH( &g_pRSMProjMatrices[1], D3DX_PI / 1.4f, 1, g_lightNear, g_lightFar );//second level of the cascade
        }
    }
}


HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    g_WindowWidth = pBackBufferSurfaceDesc->Width;
    g_WindowHeight = pBackBufferSurfaceDesc->Height;


    //
    // create scene depth texture
    //
    D3D11_TEXTURE2D_DESC depthDesc;
    ZeroMemory( &depthDesc, sizeof( D3D11_TEXTURE2D_DESC ) );
    depthDesc.ArraySize          = 1;
    depthDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    depthDesc.CPUAccessFlags     = NULL;
    depthDesc.Format             = DXGI_FORMAT_R32_TYPELESS;
    depthDesc.Width              = g_WindowWidth;
    depthDesc.Height             = g_WindowHeight;
    depthDesc.MipLevels          = 1;
    depthDesc.MiscFlags          = NULL;
    depthDesc.SampleDesc.Count   = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage              = D3D11_USAGE_DEFAULT;
    V_RETURN( pd3dDevice->CreateTexture2D( &depthDesc, NULL, &g_pSceneDepth ) );
        
    // create the shader resource view
    D3D11_SHADER_RESOURCE_VIEW_DESC depthDescSRV; 
    depthDescSRV.Format = DXGI_FORMAT_R32_FLOAT;
    depthDescSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    depthDescSRV.Texture2D.MostDetailedMip = 0;
    depthDescSRV.Texture2D.MipLevels = 1;
    V_RETURN( pd3dDevice->CreateShaderResourceView( g_pSceneDepth, &depthDescSRV, &g_pSceneDepthRV ) );

    g_LightCamera.SetWindow(g_WindowWidth, g_WindowHeight);

  g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
  g_HUD.SetSize( 170, 170 );

  return S_OK;
}


//--------------------------------------------------------------------------------------
// Helper function to compile an hlsl shader from file, 
// its binary compiled code is returned
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, ShaderCompileParams &params)
{
    HRESULT hr = S_OK;

    // find the file
    WCHAR str[MAX_PATH];
    hr = DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName );

    if (FAILED(hr))
    {
        WCHAR strFullError[MAX_PATH] = L"The file ";
        StringCchCat( strFullError, MAX_PATH, szFileName );
        StringCchCat( strFullError, MAX_PATH, L" was not found in the search path.\nCannot continue - exiting." );

        MessageBoxW(NULL, strFullError, L"Media file not found", MB_OK);
        exit(0);
    }

    D3D_SHADER_MACRO *defines = NULL;
    
    // compile the shader
    ID3D10Blob* pErrorBlob = NULL;
    hr = D3DX11CompileFromFile( str, defines, NULL, szEntryPoint, szShaderModel, D3D10_SHADER_ENABLE_STRICTNESS | D3D10_SHADER_IEEE_STRICTNESS, NULL, NULL, ppBlobOut, &pErrorBlob, NULL );
   
    // output error info
    if( FAILED(hr) )
    {
        WCHAR strFullError[MAX_PATH] = L"The file ";
        StringCchCat( strFullError, MAX_PATH, str );
        StringCchCat( strFullError, MAX_PATH, L" was not found in the search path.\nCannot continue - exiting." );
        MessageBoxW(NULL, strFullError, L"Media file not found", MB_OK);
        OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        return hr;
    }
    SAFE_RELEASE( pErrorBlob );
    delete [] defines;

    return S_OK;
}


HRESULT LoadMainMeshes(ID3D11Device* pd3dDevice)
{
    HRESULT hr = S_OK;

    V_RETURN( g_MainMesh->m_Mesh.Create( pd3dDevice, L"..\\Media\\sponza\\sponzaNoFlag.sdkmesh", true ) );
    V_RETURN( g_MainMeshSimplified->m_Mesh.Create( pd3dDevice, L"..\\Media\\sponza\\SponzaNoFlag.sdkmesh", true ) ); //can also use SponzaNoFlagSimplified.sdkmesh which is a bit smaller

    g_MainMesh->m_Mesh.LoadNormalmaps(pd3dDevice, std::string("diff.dds"), std::string("ddn.dds"));
    g_MainMesh->m_Mesh.LoadNormalmaps(pd3dDevice, std::string(".dds"), std::string("_ddn.dds"));
    g_MainMesh->m_Mesh.LoadNormalmaps(pd3dDevice, std::string("dif.dds"), std::string("ddn.dds"));
    g_MainMesh->m_Mesh.initializeDefaultNormalmaps(pd3dDevice, std::string("defaultNormalTexture.dds"));
    g_MainMesh->m_Mesh.initializeAlphaMaskTextures();
    g_MainMesh->m_Mesh.LoadAlphaMasks( pd3dDevice, std::string(".dds"), std::string("_mask.dds") );
    g_MainMesh->m_Mesh.LoadAlphaMasks( pd3dDevice, std::string("_diff.dds"), std::string("_mask.dds") );

    g_MainMesh->m_UseTexture = 1;
    g_MainMeshSimplified->m_UseTexture = 1;

    D3DXVECTOR3 meshExtents;
    D3DXVECTOR3 meshCenter;

    meshExtents = g_MainMesh->m_Mesh.GetMeshBBoxExtents(0);
    meshCenter = g_MainMesh->m_Mesh.GetMeshBBoxCenter(0);
    g_MainMesh->setWorldMatrix(          0.01f,0.01f,0.01f,0,0,0,-meshCenter.x,-meshCenter.y,-meshCenter.z);
    meshExtents = g_MainMeshSimplified->m_Mesh.GetMeshBBoxExtents(0);
    meshCenter = g_MainMeshSimplified->m_Mesh.GetMeshBBoxCenter(0);
    g_MainMeshSimplified->setWorldMatrix(0.01f,0.01f,0.01f,0,0,0,-meshCenter.x,-meshCenter.y,-meshCenter.z);


    V_RETURN( g_MainMovableMesh->m_Mesh.Create( pd3dDevice, L"..\\Media\\sponza\\flag.sdkmesh", true ) );
    meshExtents = g_MainMovableMesh->m_Mesh.GetMeshBBoxExtents(0);
    meshCenter = g_MainMovableMesh->m_Mesh.GetMeshBBoxCenter(0);
    g_MainMovableMesh->setWorldMatrix(0.01f,0.01f,0.01f,0,0,0,-meshCenter.x,-meshCenter.y,-meshCenter.z);
    g_MainMovableMesh->m_Mesh.initializeDefaultNormalmaps(pd3dDevice, std::string("defaultNormalTexture.dds"));

    g_MainMesh->m_Mesh.ComputeSubmeshBoundingVolumes();
    g_MainMeshSimplified->m_Mesh.ComputeSubmeshBoundingVolumes();
    g_MainMovableMesh->m_Mesh.ComputeSubmeshBoundingVolumes();

    g_MainMovableMesh->m_UseTexture = 0;

    return hr;
}

//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been 
// created, which will happen during application initialization and windowed/full screen 
// toggles. This is the best location to create D3DPOOL_MANAGED resources since these 
// resources need to be reloaded whenever the device is destroyed. Resources created  
// here should be released in the OnD3D11DestroyDevice callback. 
//--------------------------------------------------------------------------------------

HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
    HRESULT hr;

#if 1
    D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS options;
    hr = pd3dDevice->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &options, sizeof(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS));
    if( !options.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x )
    {
        MessageBox(NULL, L"Compute Shaders are not supported on your hardware", L"Unsupported HW", MB_OK);
        return E_FAIL;
    }
#endif

    if(g_propType==CASCADE)
        g_LPVLevelToInitialize = g_PropLevel;
    else
        g_LPVLevelToInitialize = HIERARCHICAL_INIT_LEVEL;

    g_pd3dDevice = pd3dDevice;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    D3D11_SAMPLER_DESC desc[1] = { 
        D3D11_FILTER_MIN_MAG_MIP_POINT, 
        D3D11_TEXTURE_ADDRESS_CLAMP, 
        D3D11_TEXTURE_ADDRESS_CLAMP, 
        D3D11_TEXTURE_ADDRESS_CLAMP, 
        0.0, 0, D3D11_COMPARISON_NEVER, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f,
    };
    pd3dDevice->CreateSamplerState(desc, &g_pDefaultSampler);
    pd3dDevice->CreateSamplerState(desc, &g_pDepthPeelingTexSampler);



    D3D11_SAMPLER_DESC desc2[1] = { 
        D3D11_FILTER_MIN_MAG_MIP_LINEAR, 
        D3D11_TEXTURE_ADDRESS_CLAMP,
        D3D11_TEXTURE_ADDRESS_CLAMP,
        D3D11_TEXTURE_ADDRESS_CLAMP,
        0.0, 0, D3D11_COMPARISON_NEVER, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f,
    };
    pd3dDevice->CreateSamplerState(desc2, &g_pLinearSampler);

    D3D11_SAMPLER_DESC desc3 = 
    {
        D3D11_FILTER_ANISOTROPIC,// D3D11_FILTER Filter;
        D3D11_TEXTURE_ADDRESS_WRAP, //D3D11_TEXTURE_ADDRESS_MODE AddressU;
        D3D11_TEXTURE_ADDRESS_WRAP, //D3D11_TEXTURE_ADDRESS_MODE AddressV;
        D3D11_TEXTURE_ADDRESS_WRAP, //D3D11_TEXTURE_ADDRESS_MODE AddressW;
        0,//FLOAT MipLODBias;
        D3DSAMP_MAXANISOTROPY,//UINT MaxAnisotropy;
        D3D11_COMPARISON_NEVER , //D3D11_COMPARISON_FUNC ComparisonFunc;
        0.0,0.0,0.0,0.0,//FLOAT BorderColor[ 4 ];
        0,//FLOAT MinLOD;
        D3D11_FLOAT32_MAX//FLOAT MaxLOD;   
    };
    pd3dDevice->CreateSamplerState(&desc3, &g_pAnisoSampler);


    D3D11_SAMPLER_DESC SamDescShad = 
    {
        D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,// D3D11_FILTER Filter;
        D3D11_TEXTURE_ADDRESS_BORDER, //D3D11_TEXTURE_ADDRESS_MODE AddressU;
        D3D11_TEXTURE_ADDRESS_BORDER, //D3D11_TEXTURE_ADDRESS_MODE AddressV;
        D3D11_TEXTURE_ADDRESS_BORDER, //D3D11_TEXTURE_ADDRESS_MODE AddressW;
        0,//FLOAT MipLODBias;
        0,//UINT MaxAnisotropy;
        D3D11_COMPARISON_LESS , //D3D11_COMPARISON_FUNC ComparisonFunc;
        0.0,0.0,0.0,0.0,//FLOAT BorderColor[ 4 ];
        0,//FLOAT MinLOD;
        0//FLOAT MaxLOD;   
    };

    V_RETURN( pd3dDevice->CreateSamplerState( &SamDescShad, &g_pComparisonSampler ) );

    //
    // load the screen quad geometry and shaders
    //
    ID3DBlob* pBlobVS = NULL;
    ID3DBlob* pBlobGS = NULL;
    ID3DBlob* pBlobPS = NULL;

    // create shaders
    V_RETURN( CompileShaderFromFile( L"ScreenQuad.hlsl", "VS", "vs_4_0", &pBlobVS ) );

    V_RETURN( pd3dDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), NULL, &g_pScreenQuadVS ) );

    // create the input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = 
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    UINT numElements = sizeof( layout ) / sizeof( layout[0] );
    V_RETURN( pd3dDevice->CreateInputLayout( layout, numElements, pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), &g_pScreenQuadIL) );
    SAFE_RELEASE( pBlobVS );

    //vertex shader and input layout for screen quad with position and texture

    V_RETURN( CompileShaderFromFile( L"ScreenQuad.hlsl", "DisplayTextureVS", "vs_4_0", &pBlobVS ) );
    V_RETURN( pd3dDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), NULL, &g_pScreenQuadPosTexVS3D ) );
    SAFE_RELEASE( pBlobVS );

    V_RETURN( CompileShaderFromFile( L"ScreenQuad.hlsl", "VS_POS_TEX", "vs_4_0", &pBlobVS ) );
    V_RETURN( pd3dDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), NULL, &g_pScreenQuadPosTexVS2D ) );

    D3D11_INPUT_ELEMENT_DESC layout2[] = 
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    numElements = sizeof( layout2 ) / sizeof( layout2[0] );
    V_RETURN( pd3dDevice->CreateInputLayout( layout2, numElements, pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), &g_pScreenQuadPosTexIL) );
    SAFE_RELEASE( pBlobVS );

    
    V_RETURN( CompileShaderFromFile( L"ScreenQuad.hlsl", "DisplayTexturePS2D", "ps_4_0", &pBlobPS ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &g_pScreenQuadDisplayPS2D ) );
    SAFE_RELEASE( pBlobPS );

    V_RETURN( CompileShaderFromFile( L"ScreenQuad.hlsl", "DisplayTexturePS2D_floatTextures", "ps_4_0", &pBlobPS ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &g_pScreenQuadDisplayPS2D_floatTextures ) );
    SAFE_RELEASE( pBlobPS );

    V_RETURN( CompileShaderFromFile( L"ScreenQuad.hlsl", "ReconstructPosFromDepth", "ps_4_0", &pBlobPS ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &g_pScreenQuadReconstructPosFromDepth ) );
    SAFE_RELEASE( pBlobPS );


    V_RETURN( CompileShaderFromFile( L"ScreenQuad.hlsl", "DisplayTexturePS3D", "ps_4_0", &pBlobPS ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &g_pScreenQuadDisplayPS3D ) );
    SAFE_RELEASE( pBlobPS );

    V_RETURN( CompileShaderFromFile( L"ScreenQuad.hlsl", "DisplayTexturePS3D_floatTextures", "ps_4_0", &pBlobPS ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &g_pScreenQuadDisplayPS3D_floatTextures ) );
    SAFE_RELEASE( pBlobPS );


    ID3DBlob* pBlob = NULL;
    V_RETURN( CompileShaderFromFile( L"LPV_Propagate.hlsl", "PropagateLPV", "cs_5_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pCSPropagateLPV ) );
    SAFE_RELEASE( pBlob );

    V_RETURN( CompileShaderFromFile( L"LPV_Propagate.hlsl", "PropagateLPV_Simple", "cs_5_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pCSPropagateLPVSimple ) );
    SAFE_RELEASE( pBlob );

    V_RETURN( CompileShaderFromFile( L"LPV_Propagate.hlsl", "PropagateLPV_VS", "vs_4_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pVSPropagateLPV ) );
    D3D11_INPUT_ELEMENT_DESC layout3[] = 
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    numElements = sizeof( layout3 ) / sizeof( layout3[0] );
    V_RETURN( pd3dDevice->CreateInputLayout( layout3, numElements, pBlob->GetBufferPointer(), pBlob->GetBufferSize(), &g_pPos3Tex3IL) );
    SAFE_RELEASE( pBlob );


    V_RETURN( CompileShaderFromFile( L"LPV_Propagate.hlsl", "PropagateLPV_GS", "gs_4_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreateGeometryShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pGSPropagateLPV ) );
    SAFE_RELEASE( pBlob );

    V_RETURN( CompileShaderFromFile( L"LPV_Propagate.hlsl", "PropagateLPV_PS", "ps_4_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pPSPropagateLPV ) );
    SAFE_RELEASE( pBlob );

    V_RETURN( CompileShaderFromFile( L"LPV_Propagate.hlsl", "PropagateLPV_PS_Simple", "ps_4_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pPSPropagateLPVSimple ) );
    SAFE_RELEASE( pBlob );

    V_RETURN( CompileShaderFromFile( L"LPV_Accumulate.hlsl", "AccumulateLPV", "cs_5_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pCSAccumulateLPV ) );
    SAFE_RELEASE( pBlob );


    V_RETURN( CompileShaderFromFile( L"LPV_Accumulate4.hlsl", "AccumulateLPV_singleFloats_8", "cs_5_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pCSAccumulateLPV_singleFloats_8 ) );
    SAFE_RELEASE( pBlob );

    V_RETURN( CompileShaderFromFile( L"LPV_Accumulate4.hlsl", "AccumulateLPV_singleFloats_4", "cs_5_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pCSAccumulateLPV_singleFloats_4 ) );
    SAFE_RELEASE( pBlob );

    // create the vertex buffer for the screen quad
    SimpleVertex vertices[] =
    {
        D3DXVECTOR3( -1.0f, -1.0f, 0.0f ),
        D3DXVECTOR3( -1.0f, 1.0f, 0.0f ),
        D3DXVECTOR3( 1.0f, -1.0f, 0.0f ),
        D3DXVECTOR3( 1.0f, 1.0f, 0.0f ),
    };
    D3D11_BUFFER_DESC bd;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( SimpleVertex ) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = vertices;
    V_RETURN(  pd3dDevice->CreateBuffer( &bd, &InitData, &g_pScreenQuadVB ) );

    // create the vertex buffer for a small visualization
    TexPosVertex verticesViz[] =
    {
        TexPosVertex(D3DXVECTOR3( -1.0f, -1.0f, 0.0f ),D3DXVECTOR2( 0.0f, 1.0f)),
        TexPosVertex(D3DXVECTOR3( -1.0f, -0.25f, 0.0f ),D3DXVECTOR2( 0.0f, 0.0f)),
        TexPosVertex(D3DXVECTOR3( -0.25f, -1.0f, 0.0f ),D3DXVECTOR2( 1.0f, 1.0f)),
        TexPosVertex(D3DXVECTOR3( -0.25f,-0.25f, 0.0f ),D3DXVECTOR2( 1.0f, 0.0f)),
    };
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( TexPosVertex ) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    InitData.pSysMem = verticesViz;
    V_RETURN(  pd3dDevice->CreateBuffer( &bd, &InitData, &g_pVizQuadVB ) );


    // 
    // load mesh and shading effects
    //
    V_RETURN( CompileShaderFromFile( L"SimpleShading.hlsl", "VS", "vs_4_0", &pBlobVS ) );
    V_RETURN( CompileShaderFromFile( L"SimpleShading.hlsl", "PS", "ps_4_0", &pBlobPS ) );
    V_RETURN( pd3dDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), NULL, &g_pVS ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &g_pPS ) );

    // create the vertex input layout
    const D3D11_INPUT_ELEMENT_DESC meshLayout[] =
    {
        { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    V_RETURN( pd3dDevice->CreateInputLayout( meshLayout, ARRAYSIZE( meshLayout ), pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), &g_pMeshLayout ) );

    SAFE_RELEASE( pBlobVS );
    SAFE_RELEASE( pBlobPS );

    V_RETURN( CompileShaderFromFile( L"SimpleShading.hlsl", "PS_separateFloatTextures", "ps_4_0", &pBlobPS ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &g_pPS_separateFloatTextures ) );

    V_RETURN( CompileShaderFromFile( L"SimpleShading.hlsl", "VS_Simple", "vs_4_0", &pBlobVS ) );
    V_RETURN( CompileShaderFromFile( L"SimpleShading.hlsl", "PS_Simple", "ps_4_0", &pBlobPS ) );
    V_RETURN( pd3dDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), NULL, &g_pSimpleVS ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &g_pSimplePS ) );
    SAFE_RELEASE( pBlobVS );
    SAFE_RELEASE( pBlobPS );


    V_RETURN( CompileShaderFromFile( L"SimpleShading.hlsl", "VS_RSM", "vs_4_0", &pBlobVS ) );
    V_RETURN( CompileShaderFromFile( L"SimpleShading.hlsl", "PS_RSM", "ps_4_0", &pBlobPS ) );
    V_RETURN( pd3dDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), NULL, &g_pVSRSM ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &g_pPSRSM ) );
    SAFE_RELEASE( pBlobVS );
    SAFE_RELEASE( pBlobPS );

    V_RETURN( CompileShaderFromFile( L"SimpleShading.hlsl", "VS_ShadowMap", "vs_4_0", &pBlobVS ) );
    V_RETURN( pd3dDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), NULL, &g_pVSSM ) );
    SAFE_RELEASE( pBlobVS );    
    
    V_RETURN( CompileShaderFromFile( L"SimpleShading.hlsl", "VS_RSM_DepthPeeling", "vs_4_0", &pBlobVS ) );
    V_RETURN( CompileShaderFromFile( L"SimpleShading.hlsl", "PS_RSM_DepthPeeling", "ps_4_0", &pBlobPS ) );
    V_RETURN( pd3dDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), NULL, &g_pVSRSMDepthPeeling ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &g_pPSRSMDepthPeel ) );
    SAFE_RELEASE( pBlobVS );
    SAFE_RELEASE( pBlobPS );
    

    V_RETURN( CompileShaderFromFile( L"SimpleShading.hlsl", "VS_VizLPV", "vs_4_0", &pBlobVS ) );
    V_RETURN( CompileShaderFromFile( L"SimpleShading.hlsl", "PS_VizLPV", "ps_4_0", &pBlobPS ) );
    V_RETURN( pd3dDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), NULL, &g_pVSVizLPV ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &g_pPSVizLPV ) );
    SAFE_RELEASE( pBlobVS );
    SAFE_RELEASE( pBlobPS );


    g_MainMesh = new RenderMesh();
    g_MainMeshSimplified = new RenderMesh();
    g_MainMovableMesh = new RenderMesh();
    g_MovableBoxMesh = new RenderMesh();

    V_RETURN(LoadMainMeshes(pd3dDevice));

    V_RETURN( g_MovableBoxMesh->m_Mesh.Create( pd3dDevice, L"..\\Media\\box.sdkmesh", true ) );
    
    V_RETURN( g_LowResMesh.Create( pd3dDevice, L"..\\Media\\unitSphere.sdkmesh", true ) );
    V_RETURN( g_MeshArrow.Create( pd3dDevice, L"..\\Media\\arrow.sdkmesh", true ) );
    V_RETURN( g_MeshBox.Create( pd3dDevice, L"..\\Media\\box.sdkmesh", true ) );
 
    g_BoXExtents = g_MeshBox.GetMeshBBoxExtents(0);
    g_BoxCenter = g_MeshBox.GetMeshBBoxCenter(0);

    g_MovableBoxMesh->setWorldMatrix(1.0f/g_BoXExtents.x,16.0f/g_BoXExtents.y,16.0f/g_BoXExtents.z,0,0,0,0,0,0);
    g_MovableBoxMesh->m_UseTexture = 0;

    // setup constant buffers
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;
    
    Desc.ByteWidth = sizeof( CB_VS_PER_OBJECT );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbVSPerObject ) );

    Desc.ByteWidth = sizeof( CB_VS_GLOBAL );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbVSGlobal ) );
    
    Desc.ByteWidth = sizeof( CB_LPV_INITIALIZE );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbLPVinitVS ) );

    Desc.ByteWidth = sizeof( CB_LPV_INITIALIZE3 );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbLPVinitialize_LPVDims ) );

    Desc.ByteWidth = sizeof( CB_LPV_PROPAGATE );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbLPVpropagate ) );

    Desc.ByteWidth = sizeof( CB_RENDER );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbRender ) );

    Desc.ByteWidth = sizeof( CB_RENDER_LPV );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbRenderLPV ) );

    Desc.ByteWidth = sizeof( CB_SM_TAP_LOCS );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbPSSMTapLocations ) );

    Desc.ByteWidth = sizeof( CB_RENDER_LPV );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbRenderLPV2 ) );

    Desc.ByteWidth = sizeof( CB_LPV_INITIALIZE2 );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbLPVinitVS2 ) );

    Desc.ByteWidth = sizeof( CB_LPV_PROPAGATE_GATHER );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbLPVpropagateGather ) );

    Desc.ByteWidth = sizeof( CB_LPV_PROPAGATE_GATHER2 );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbLPVpropagateGather2 ) );
    
    Desc.ByteWidth = sizeof( CB_GV );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbGV ) );

    Desc.ByteWidth = sizeof( CB_SIMPLE_OBJECTS );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbSimple ) );

    Desc.ByteWidth = sizeof( VIZ_LPV );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbLPVViz ) );

    Desc.ByteWidth = sizeof( CB_MESH_RENDER_OPTIONS );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbMeshRenderOptions ) );

    Desc.ByteWidth = sizeof( CB_DRAW_SLICES_3D );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbSlices3D ) );

    Desc.ByteWidth = sizeof( RECONSTRUCT_POS );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_reconPos ) );

    Desc.ByteWidth = sizeof( CB_INVPROJ_MATRIX );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbInvProjMatrix ) );


    //shadow map for the scene--------------------------------------------------------------

     D3D11_TEXTURE2D_DESC texDesc;
    texDesc.ArraySize          = 1;
    texDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags     = NULL;
    texDesc.Format             = DXGI_FORMAT_R32_TYPELESS;
    texDesc.Width              = SM_SIZE;
    texDesc.Height             = SM_SIZE;
    texDesc.MipLevels          = 1;
    texDesc.MiscFlags          = NULL;
    texDesc.SampleDesc.Count   = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage              = D3D11_USAGE_DEFAULT;
    g_pSceneShadowMap = new DepthRT( pd3dDevice, &texDesc );

    //stuff for RSMs and LPVs etc ----------------------------------------------------------

    //initialize the shadow map data
    initializeReflectiveShadowMaps(pd3dDevice);
    
    D3D11_RASTERIZER_DESC rasterizerState;
    rasterizerState.CullMode = D3D11_CULL_BACK;
    rasterizerState.FillMode = D3D11_FILL_SOLID;
    rasterizerState.FrontCounterClockwise = false;
    rasterizerState.DepthBias = 0;
    rasterizerState.DepthBiasClamp = 0.0f;
    rasterizerState.SlopeScaledDepthBias = 0.0f;
    rasterizerState.DepthClipEnable = true;
    rasterizerState.ScissorEnable = false;
    rasterizerState.MultisampleEnable = true;
    rasterizerState.AntialiasedLineEnable = false;
    pd3dDevice->CreateRasterizerState( &rasterizerState, &g_pRasterizerStateMainRender);
    rasterizerState.CullMode = D3D11_CULL_NONE;
    pd3dDevice->CreateRasterizerState( &rasterizerState, &pRasterizerStateCullNone);
    rasterizerState.CullMode = D3D11_CULL_FRONT;
    pd3dDevice->CreateRasterizerState( &rasterizerState, &pRasterizerStateCullFront);
    rasterizerState.FillMode = D3D11_FILL_WIREFRAME;
    pd3dDevice->CreateRasterizerState( &rasterizerState, &pRasterizerStateWireFrame);

    //create the grid used to render to our flat 3D textures (splatted into large 2D textures)

    ID3D11VertexShader* pLPV_init_VS = NULL;
    V_RETURN( CompileShaderFromFile( L"ScreenQuad.hlsl", "DisplayTextureVS", "vs_4_0", &pBlobVS ) );
    V_RETURN( pd3dDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), NULL, &pLPV_init_VS ) );
    g_grid = new Grid(pd3dDevice,pd3dImmediateContext);
    g_grid->Initialize(g_LPVWIDTH,g_LPVHEIGHT,g_LPVDEPTH,pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize());
    SAFE_RELEASE( pBlobVS );
    SAFE_RELEASE( pLPV_init_VS );

    createPropagationAndGeometryVolumes(pd3dDevice);

    g_LPVViewport.MinDepth = 0;
    g_LPVViewport.MaxDepth = 1;
    g_LPVViewport.TopLeftX = 0;
    g_LPVViewport.TopLeftY = 0;

    
    g_LPVViewport3D.Width  = g_LPVWIDTH;
    g_LPVViewport3D.Height = g_LPVHEIGHT;
    g_LPVViewport3D.MinDepth = 0;
    g_LPVViewport3D.MaxDepth = 1;
    g_LPVViewport3D.TopLeftX = 0;
    g_LPVViewport3D.TopLeftY = 0;


    //load the shaders and create the layout for the LPV initialization
    V_RETURN( CompileShaderFromFile( L"LPV.hlsl", "VS_initializeLPV", "vs_4_0", &pBlobVS ) );
    V_RETURN( pd3dDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), NULL, &g_pVSInitializeLPV ) );
    SAFE_RELEASE( pBlobVS );

    V_RETURN( CompileShaderFromFile( L"LPV.hlsl", "VS_initializeLPV_Bilnear", "vs_4_0", &pBlobVS ) );
    V_RETURN( pd3dDevice->CreateVertexShader( pBlobVS->GetBufferPointer(), pBlobVS->GetBufferSize(), NULL, &g_pVSInitializeLPV_Bilinear ) );
    SAFE_RELEASE( pBlobVS );

    V_RETURN( CompileShaderFromFile( L"LPV.hlsl", "PS_initializeLPV", "ps_4_0", &pBlobPS ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &g_pPSInitializeLPV ) );
    SAFE_RELEASE( pBlobPS );

    V_RETURN( CompileShaderFromFile( L"LPV.hlsl", "GS_initializeLPV", "gs_4_0", &pBlobGS ) );
    V_RETURN( pd3dDevice->CreateGeometryShader( pBlobGS->GetBufferPointer(), pBlobGS->GetBufferSize(), NULL, &g_pGSInitializeLPV ) );
    SAFE_RELEASE( pBlobGS );
    V_RETURN( CompileShaderFromFile( L"LPV.hlsl", "GS_initializeLPV_Bilinear", "gs_4_0", &pBlobGS ) );
    V_RETURN( pd3dDevice->CreateGeometryShader( pBlobGS->GetBufferPointer(), pBlobGS->GetBufferSize(), NULL, &g_pGSInitializeLPV_Bilinear ) );
    SAFE_RELEASE( pBlobGS );

    
    //load the shaders and create the layout for the GV initialization
    V_RETURN( CompileShaderFromFile( L"LPV.hlsl", "PS_initializeGV", "ps_4_0", &pBlobPS ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlobPS->GetBufferPointer(), pBlobPS->GetBufferSize(), NULL, &g_pPSInitializeGV ) );
    SAFE_RELEASE( pBlobPS );
    


    //initialize the precomputed values of g_LPVPropagateGather
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    pd3dImmediateContext->Map( g_pcbLPVpropagateGather, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    g_LPVPropagateGather = ( CB_LPV_PROPAGATE_GATHER* )MappedResource.pData;

    D3D11_MAPPED_SUBRESOURCE MappedResource2;
    pd3dImmediateContext->Map( g_pcbLPVpropagateGather2, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource2 );
    g_LPVPropagateGather2 = ( CB_LPV_PROPAGATE_GATHER2* )MappedResource2.pData;


    D3DXVECTOR3 offsets[6];
    //offsets to the six neighbors of a cell at 0,0,0
    offsets[0] = D3DXVECTOR3(0,0,1);
    offsets[1] = D3DXVECTOR3(1,0,0);
    offsets[2] = D3DXVECTOR3(0,0,-1);
    offsets[3] = D3DXVECTOR3(-1,0,0);
    offsets[4] = D3DXVECTOR3(0,1,0);
    offsets[5] = D3DXVECTOR3(0,-1,0);

    for(int neighbor=0; neighbor<6; neighbor++)
    {
        D3DXVECTOR3 neighborCellCenter = offsets[neighbor];

        D3DXVECTOR4 occlusionOffsets = D3DXVECTOR4(0,0,0,0);
        if(neighborCellCenter.x>0) occlusionOffsets = D3DXVECTOR4(6,1,2,0);
        else if(neighborCellCenter.x<0) occlusionOffsets = D3DXVECTOR4(7,5,4,3); 
        else if(neighborCellCenter.y>0) occlusionOffsets = D3DXVECTOR4(0,3,1,5);
        else if(neighborCellCenter.y<0) occlusionOffsets = D3DXVECTOR4(2,4,6,7);
        else if(neighborCellCenter.z>0) occlusionOffsets = D3DXVECTOR4(0,3,2,4);
        else if(neighborCellCenter.z<0) occlusionOffsets = D3DXVECTOR4(1,5,6,7);

        D3DXVECTOR4 multiBounceOffsets = D3DXVECTOR4(0,0,0,0);
        if(neighborCellCenter.x>0) multiBounceOffsets = D3DXVECTOR4(7,5,4,3);
        else if(neighborCellCenter.x<0) multiBounceOffsets = D3DXVECTOR4(6,1,2,0);
        else if(neighborCellCenter.y>0) multiBounceOffsets = D3DXVECTOR4(2,4,6,7);
        else if(neighborCellCenter.y<0) multiBounceOffsets = D3DXVECTOR4(0,3,1,5);
        else if(neighborCellCenter.z>0) multiBounceOffsets = D3DXVECTOR4(1,5,6,7);
        else if(neighborCellCenter.z<0) multiBounceOffsets = D3DXVECTOR4(0,3,2,4);

        g_LPVPropagateGather2->propConsts2[neighbor].multiBounceOffsetX = (int)multiBounceOffsets.x;
        g_LPVPropagateGather2->propConsts2[neighbor].multiBounceOffsetY = (int)multiBounceOffsets.y;
        g_LPVPropagateGather2->propConsts2[neighbor].multiBounceOffsetZ = (int)multiBounceOffsets.z;
        g_LPVPropagateGather2->propConsts2[neighbor].multiBounceOffsetW = (int)multiBounceOffsets.w;
        g_LPVPropagateGather2->propConsts2[neighbor].occlusionOffsetX   = (int)occlusionOffsets.x;
        g_LPVPropagateGather2->propConsts2[neighbor].occlusionOffsetY   = (int)occlusionOffsets.y;
        g_LPVPropagateGather2->propConsts2[neighbor].occlusionOffsetZ   = (int)occlusionOffsets.z;
        g_LPVPropagateGather2->propConsts2[neighbor].occlusionOffsetW   = (int)occlusionOffsets.w;

        //for each of the six faces of a cell
        for(int face=0;face<6;face++)
        {    
            D3DXVECTOR3 facePosition = offsets[face]*0.5f;
            //the vector from the neighbor's cell center
            D3DXVECTOR3 vecFromNCC = D3DXVECTOR3(facePosition - neighborCellCenter);
            float length = D3DXVec3Length(&vecFromNCC);    
            vecFromNCC /= length;

            g_LPVPropagateGather->propConsts[neighbor*6+face].neighborOffset = D3DXVECTOR4(neighborCellCenter,1.0f);
            g_LPVPropagateGather->propConsts[neighbor*6+face].x = vecFromNCC.x;
            g_LPVPropagateGather->propConsts[neighbor*6+face].y = vecFromNCC.y;
            g_LPVPropagateGather->propConsts[neighbor*6+face].z = vecFromNCC.z;
            //the solid angle subtended by the face onto the neighbor cell center is one of two values below, depending on whether the cell center is directly below
            //the face or off to one side.
            //note, there is a derivation for these numbers (based on the solid angle subtended at the apex of a foursided right regular pyramid)
            //we also normalize the solid angle by dividing by 4*PI (the solid angle of a sphere measured from a point in its interior)
            if(length<=0.5f)
                g_LPVPropagateGather->propConsts[neighbor*6+face].solidAngle = 0.0f; 
            else
                g_LPVPropagateGather->propConsts[neighbor*6+face].solidAngle = length>=1.5f ? 22.95668f/(4*180.0f) : 24.26083f/(4*180.0f) ; 
        }
    }

    pd3dImmediateContext->Unmap( g_pcbLPVpropagateGather, 0 );
    pd3dImmediateContext->Unmap( g_pcbLPVpropagateGather2, 0 );
    


    D3D11_BLEND_DESC blendStateAlpha;
    blendStateAlpha.AlphaToCoverageEnable = FALSE;
    blendStateAlpha.IndependentBlendEnable = FALSE;
    for (int i = 0; i < 8; ++i)
    {
        blendStateAlpha.RenderTarget[i].BlendEnable = TRUE;
        blendStateAlpha.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
        blendStateAlpha.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendStateAlpha.RenderTarget[i].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blendStateAlpha.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ONE;
        blendStateAlpha.RenderTarget[i].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blendStateAlpha.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendStateAlpha.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }
    V( pd3dDevice->CreateBlendState(&blendStateAlpha, &g_pAlphaBlendBS) );


    D3D11_BLEND_DESC blendState;
    blendState.AlphaToCoverageEnable = FALSE;
    blendState.IndependentBlendEnable = FALSE;
    for (int i = 0; i < 8; ++i)
    {
        blendState.RenderTarget[i].BlendEnable = FALSE;
        blendState.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }
    V( pd3dDevice->CreateBlendState(&blendState, &g_pNoBlendBS) );
    for (int i = 0; i < 8; ++i)
    {
        blendState.RenderTarget[i].BlendEnable = TRUE;
        blendState.RenderTarget[i].BlendOp = D3D11_BLEND_OP_MAX;
        blendState.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_MAX;
        blendState.RenderTarget[i].DestBlend = D3D11_BLEND_ONE;
        blendState.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ONE;
        blendState.RenderTarget[i].SrcBlend = D3D11_BLEND_ONE;
        blendState.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendState.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }
    V( pd3dDevice->CreateBlendState(&blendState, &g_pMaxBlendBS) );

    blendState.IndependentBlendEnable = true;
    for (int i = 0; i < 3; ++i)
    {
        blendState.RenderTarget[i].BlendEnable = FALSE;
        blendState.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }
    for (int i = 3; i < 6; ++i)
    {
        blendState.RenderTarget[i].BlendEnable = TRUE;
        blendState.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
        blendState.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blendState.RenderTarget[i].DestBlend = D3D11_BLEND_ONE;
        blendState.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ONE;
        blendState.RenderTarget[i].SrcBlend = D3D11_BLEND_ONE;
        blendState.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
        blendState.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    }
    V( pd3dDevice->CreateBlendState(&blendState, &g_pNoBlend1AddBlend2BS) );

    D3D11_DEPTH_STENCIL_DESC depthState;
    depthState.DepthEnable = TRUE;
    depthState.StencilEnable = FALSE;
    depthState.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    depthState.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    V(pd3dDevice->CreateDepthStencilState(&depthState,&g_normalDepthStencilState));
    depthState.DepthFunc = D3D11_COMPARISON_LESS;
    V(pd3dDevice->CreateDepthStencilState(&depthState,&depthStencilStateComparisonLess));
    depthState.DepthEnable = FALSE;
    V(pd3dDevice->CreateDepthStencilState(&depthState,&g_depthStencilStateDisableDepth));    

    changeSMTaps(pd3dImmediateContext);

    return S_OK;
}

bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, 
                                       const CD3D11EnumDeviceInfo *DeviceInfo, DXGI_FORMAT BackBufferFormat, 
                                       bool bWindowed, void* pUserContext )
{
    return true;
}

//--------------------------------------------------------------------------------------
// This callback function will be called once at the beginning of every frame. This is the
// best location for your application to handle updates to the scene, but is not 
// intended to contain actual rendering calls, which should instead be placed in the 
// OnFrameRender callback.  
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
  // Update the camera's and light's position based on user input 
    g_LightCamera.FrameMove(fElapsedTime);
    g_Camera.FrameMove( fElapsedTime );

    updateProjectionMatrices();
}

//--------------------------------------------------------------------------------------
// As a convenience, DXUT inspects the incoming windows messages for
// keystroke messages and decodes the message parameters to pass relevant keyboard
// messages to the application.  The framework does not remove the underlying keystroke 
// messages, which are still passed to the application's MsgProc callback.
//--------------------------------------------------------------------------------------
void CALLBACK KeyboardProc( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if( bKeyDown )
    {
        switch( nChar )
        {
        case 'I':
        case 'i':
            {
                g_objectTranslate.z += 5.0f;
                break;
            }
        case 'K':
        case 'k':
            {
                g_objectTranslate.z -= 5.0f;
                break;
            }
        case 'J':
        case 'j':
            {
                g_objectTranslate.x += 5.0f;
                break;
            }
        case 'L':
        case 'l':
            {
                g_objectTranslate.x -= 5.0f;
                break;
            }
        case 'U':
        case 'u':
            {
                g_objectTranslate.y += 5.0f;
                break;
            }
        case 'O':
        case 'o':
            {
                g_objectTranslate.y -= 5.0f;
                break;
            }
        case 'y':
        case 'Y':
            {
                g_showUI = !g_showUI;
                break;
            }
        case 'h':
        case 'H':
            {
                g_printHelp = !g_printHelp;
                break;
            }

            case VK_F1:
                break;
        }
    }
}

//--------------------------------------------------------------------------------------
// Before handling window messages, DXUT passes incoming windows 
// messages to the application through this callback function. If the application sets 
// *pbNoFurtherProcessing to TRUE, then DXUT will not process this message.
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                         void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all windows messages to camera so it can respond to user input
    if(!DXUTIsKeyDown( VK_LSHIFT ) )
        g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );
    else
        g_LightCamera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}

//--------------------------------------------------------------------------------------
// This callback function is called immediately before a device is created to allow the 
// application to modify the device settings. The supplied pDeviceSettings parameter 
// contains the settings that the framework has selected for the new device, and the 
// application can make any desired changes directly to this structure.  Note however that 
// DXUT will not correct invalid device settings so care must be taken 
// to return valid device settings, otherwise CreateDevice() will fail.  
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    assert( pDeviceSettings->ver == DXUT_D3D11_DEVICE );

    // Add UAC flag to back buffer Texture2D resource, so we can create an UAV on the back buffer of the swap chain,
    // then it can be bound as the output resource of the CS
//    pDeviceSettings->d3d11.sd.BufferUsage |= DXGI_USAGE_UNORDERED_ACCESS;

    // For the first device created if it is a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF ) ||
            ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
            pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }

        // Disable vsync
        pDeviceSettings->d3d11.SyncInterval = 0;
        //enable multisampling
        pDeviceSettings->d3d11.sd.SampleDesc.Count = D3DMULTISAMPLE_2_SAMPLES;
        pDeviceSettings->d3d11.sd.SampleDesc.Quality = 0;

    }

    return true;
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    WCHAR sz[MAX_PATH];

    switch( nControlID )
    {
    case IDC_TOGGLEFULLSCREEN:
        DXUTToggleFullScreen(); break;
    case IDC_TOGGLEREF:
        DXUTToggleREF(); break;
    case IDC_CHANGEDEVICE:
        g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;

    case IDC_RESET_LPV_XFORM:
        g_resetLPVXform = true; break;


    case IDC_RESET_SETTINGS:
        {
            resetSettingValues();
            resetSettingsUI();
            break;
        }

    case IDC_SMDEPTHBIAS_SCALE:
        {
            g_depthBiasFromGUI = (float) (g_HUD.GetSlider( IDC_SMDEPTHBIAS_SCALE )->GetValue()* 0.000001f);
            StringCchPrintf( sz, 100, L"Shadow Depth Bias: %0.4f", g_depthBiasFromGUI ); 
            g_HUD.GetStatic( IDC_SMDEPTHBIAS_STATIC )->SetText( sz );
            break;
        }
/*
    case IDC_SM_TAPS_SCALE:
        {
            int smtaps = g_HUD.GetSlider( IDC_SM_TAPS_SCALE )->GetValue();
            if(smtaps == 0) g_smTaps = 1;
            else if(smtaps == 1) g_smTaps = 8;
            else if(smtaps == 2) g_smTaps = 16;
            else if(smtaps == 3) g_smTaps = 28;
            StringCchPrintf( sz, 100, L"Shadow map taps: %d", g_smTaps ); 
            g_HUD.GetStatic( IDC_SM_TAPS_STATIC )->SetText( sz );
            g_numTapsChanged=true;
            break;
        }

    case IDC_SMFILTERSIZE_SCALE:
        {
            g_smFilterSize = (float) (g_HUD.GetSlider( IDC_SMFILTERSIZE_SCALE )->GetValue()* 0.1f);
            StringCchPrintf( sz, 100, L"Shadowmap filter Size: %0.2f", g_smFilterSize ); 
            g_HUD.GetStatic( IDC_SMFILTERSIZE_STATIC )->SetText( sz );
            g_numTapsChanged=true;
            break;
        }
*/
    case IDC_VPLDISP_SCALE:
        {
            g_VPLDisplacement = (float) (g_HUD.GetSlider( IDC_VPLDISP_SCALE )->GetValue()* 0.01f);
            StringCchPrintf( sz, 100, L"VPL displace: %0.3f", g_VPLDisplacement ); 
            g_HUD.GetStatic( IDC_VPLDISP_STATIC )->SetText( sz );
            break;
        }
    case IDC_DIR_SCALE:
        {
            g_diffuseInterreflectionScale = (float) (g_HUD.GetSlider( IDC_DIR_SCALE )->GetValue()* 0.001f);
            StringCchPrintf( sz, 100, L"interreflect contribution: %0.3f", g_diffuseInterreflectionScale ); 
            g_HUD.GetStatic( IDC_DIR_STATIC )->SetText( sz );
            break;
        }
    case IDC_DIRECT_STRENGTH_SCALE:
        {
            g_directLightStrength = (float) (g_HUD.GetSlider( IDC_DIRECT_STRENGTH_SCALE )->GetValue()* 0.1f);
            StringCchPrintf( sz, 100, L"directLight strength: %0.1f", g_directLightStrength ); 
            g_HUD.GetStatic( IDC_DIRECT_STRENGTH_STATIC )->SetText( sz );
            break;
        }

    case IDC_DIRECT_SCALE:
        {
            g_directLight = (float) (g_HUD.GetSlider( IDC_DIRECT_SCALE )->GetValue()*0.001f);
            StringCchPrintf( sz, 100, L"directLight contribution: %0.3f", g_directLight ); 
            g_HUD.GetStatic( IDC_DIRECT_STATIC )->SetText( sz );
            break;        
        }    
    case IDC_RADIUS_SCALE:
        {
            g_lightRadius = (float) (g_HUD.GetSlider( IDC_RADIUS_SCALE )->GetValue()*0.01f);
            StringCchPrintf( sz, 100, L"Light radius: %0.2f", g_lightRadius ); 
            g_HUD.GetStatic( IDC_RADIUS_STATIC )->SetText( sz );
            break;        
        }    
    case IDC_PROPAGATEITERATIONS_SCALE:
        {
            g_numPropagationStepsLPV = (g_HUD.GetSlider( IDC_PROPAGATEITERATIONS_SCALE )->GetValue());
            StringCchPrintf( sz, 100, L"LPV iterations: %d", g_numPropagationStepsLPV ); 
            g_HUD.GetStatic( IDC_PROPAGATEITERATIONS_STATIC )->SetText( sz );
            break;
        }
    case IDC_NUMDEPTHPEELINGPASSES_SCALE:
        {
            g_numDepthPeelingPasses = (g_HUD.GetSlider( IDC_NUMDEPTHPEELINGPASSES_SCALE )->GetValue());
            StringCchPrintf( sz, 100, L"depth peel layers: %d", g_numDepthPeelingPasses ); 
            g_HUD.GetStatic( IDC_NUMDEPTHPEELINGPASSES_STATIC )->SetText( sz );
            break;
        }
        /*
    case IDC_LPVSCALE_SCALE:
        {
            g_LPVscale = (float)(g_HUD.GetSlider( IDC_LPVSCALE_SCALE )->GetValue())*0.1f;
            StringCchPrintf( sz, 100, L"LPV scale: %0.1f", g_LPVscale ); 
            g_HUD.GetStatic( IDC_LPVSCALE_STATIC )->SetText( sz );
            break;
        }
        */
    case IDC_FLUX_AMP_SCALE:
        {
            g_fluxAmplifier = (float)(g_HUD.GetSlider( IDC_FLUX_AMP_SCALE )->GetValue())*0.01f;
            StringCchPrintf( sz, 100, L"Flux Amplifier: %0.3f", g_fluxAmplifier ); 
            g_HUD.GetStatic( IDC_FLUX_AMP_STATIC )->SetText( sz );
            break;
        }
    case IDC_REF_LIGHT_AMP_SCALE:
        {
            g_reflectedLightAmplifier = (float)(g_HUD.GetSlider( IDC_REF_LIGHT_AMP_SCALE )->GetValue())*0.01f;
            StringCchPrintf( sz, 100, L"Reflected Light Amp: %0.3f", g_reflectedLightAmplifier ); 
            g_HUD.GetStatic( IDC_REF_LIGHT_AMP_STATIC )->SetText( sz );
            break;
        }
    case IDC_OCCLUSION_AMP_SCALE:
        {
            g_occlusionAmplifier = (float)(g_HUD.GetSlider( IDC_OCCLUSION_AMP_SCALE )->GetValue())*0.01f;
            StringCchPrintf( sz, 100, L"Occlusion Amplifier: %0.3f", g_occlusionAmplifier ); 
            g_HUD.GetStatic( IDC_OCCLUSION_AMP_STATIC )->SetText( sz );
            break;
        }
    case IDC_DIR_OFFSET_AMOUNT_SCALE:
        {
            g_directionalDampingAmount = (float)(g_HUD.GetSlider( IDC_DIR_OFFSET_AMOUNT_SCALE )->GetValue())*0.01f;
            StringCchPrintf( sz, 100, L"Dir Damping Offset: %0.3f", g_directionalDampingAmount ); 
            g_HUD.GetStatic( IDC_DIR_OFFSET_AMOUNT_STATIC )->SetText( sz );
            break;
        }
    case IDC_DIRECTIONAL_CLAMPING:
        {
            g_useDirectionalDerivativeClamping = g_HUD.GetCheckBox( IDC_DIRECTIONAL_CLAMPING )->GetChecked(); 
            break;
        }
    case IDC_VIZSM:
        {
            g_bVisualizeSM = g_HUD.GetCheckBox( IDC_VIZSM )->GetChecked(); 
            break;
        }
    case IDC_VIZLPV3D:
        {
            g_bVisualizeLPV3D =  g_HUD.GetCheckBox( IDC_VIZLPV3D )->GetChecked(); 
            break;
        }
    case IDC_USESM:
        {
            g_bUseSM = g_HUD.GetCheckBox( IDC_USESM )->GetChecked(); 
            break;
        }
    case IDC_VIZLPVBB:
        {
            g_bVizLPVBB = g_HUD.GetCheckBox( IDC_VIZLPVBB )->GetChecked(); 
            break;
        }
    case IDC_VIZMESH:
        {
            g_renderMesh = g_HUD.GetCheckBox( IDC_VIZMESH )->GetChecked();
            break;
        }
    case IDC_USEDIFINTERREFLECTION:
        {
            g_useDiffuseInterreflection = g_HUD.GetCheckBox( IDC_USEDIFINTERREFLECTION )->GetChecked();
            g_HUD.GetStatic( IDC_DIR_STATIC )->SetEnabled(g_useDiffuseInterreflection);
            g_HUD.GetSlider( IDC_DIR_SCALE )->SetEnabled(g_useDiffuseInterreflection);
            break;
        }        
    case IDC_MULTIPLE_BOUNCES:
        {
            g_useMultipleBounces = g_HUD.GetCheckBox( IDC_MULTIPLE_BOUNCES )->GetChecked(); 
            g_HUD.GetStatic( IDC_NUMDEPTHPEELINGPASSES_STATIC )->SetEnabled(g_useOcclusion || g_useMultipleBounces);
            g_HUD.GetSlider( IDC_NUMDEPTHPEELINGPASSES_SCALE )->SetEnabled(g_useOcclusion || g_useMultipleBounces);
            g_HUD.GetSlider( IDC_REF_LIGHT_AMP_SCALE )->SetEnabled(g_useMultipleBounces);
            g_HUD.GetStatic( IDC_REF_LIGHT_AMP_STATIC )->SetEnabled(g_useMultipleBounces);
            break;
        }            
    case IDC_USE_OCCLUSION:
        {
            g_useOcclusion = g_HUD.GetCheckBox( IDC_USE_OCCLUSION )->GetChecked();
            g_HUD.GetStatic( IDC_NUMDEPTHPEELINGPASSES_STATIC )->SetEnabled(g_useOcclusion || g_useMultipleBounces);
            g_HUD.GetSlider( IDC_NUMDEPTHPEELINGPASSES_SCALE )->SetEnabled(g_useOcclusion || g_useMultipleBounces);
            g_HUD.GetSlider( IDC_OCCLUSION_AMP_SCALE )->SetEnabled(g_useOcclusion);
            g_HUD.GetStatic( IDC_OCCLUSION_AMP_STATIC )->SetEnabled(g_useOcclusion);
            break;
        }
    case IDC_USE_BILINEAR_INIT:
        {
            g_useBilinearInit = g_HUD.GetCheckBox( IDC_USE_BILINEAR_INIT )->GetChecked();
            break;
        }
    case IDC_ANIMATE_LIGHT:
        {
            g_animateLight = g_HUD.GetCheckBox(IDC_ANIMATE_LIGHT)->GetChecked();
            if(g_animateLight)
            {
                g_bilinearWasEnabled = g_useBilinearInit;
                g_useBilinearInit = true;
            }
            else
                g_useBilinearInit = g_bilinearWasEnabled;
            g_HUD.GetCheckBox( IDC_USE_BILINEAR_INIT )->SetChecked(g_useBilinearInit);
            break;
        }
    case IDC_SHOW_MESH:
        {
            g_showMovableMesh = g_HUD.GetCheckBox( IDC_SHOW_MESH )->GetChecked();
            break;
        }
    case IDC_MOVABLE_LPV:
        {
            g_movableLPV = g_HUD.GetCheckBox( IDC_MOVABLE_LPV )->GetChecked();
            break;
        }
    case IDC_USE_SINGLELPV:
        {
            g_bUseSingleLPV = g_HUD.GetCheckBox( IDC_USE_SINGLELPV )->GetChecked();        
            g_HUD.GetSlider( IDC_LEVEL_SCALE )->SetEnabled(g_bUseSingleLPV);
            g_HUD.GetStatic( IDC_LEVEL_STATIC )->SetEnabled(g_bUseSingleLPV);
            break;
        }
    case IDC_USE_RSM_CASCADE:
        {
            g_useRSMCascade = g_HUD.GetCheckBox( IDC_USE_RSM_CASCADE )->GetChecked();        
            break;
        }
    case IDC_COMBO_PROP_TYPE:
        {
            g_propType = (PROP_TYPE)g_HUD.GetComboBox( IDC_COMBO_PROP_TYPE )->GetSelectedIndex();
            bPropTypeChanged = true;
            break;    
        }
    case IDC_LEVEL_SCALE:
        {
            int level = (g_HUD.GetSlider( IDC_LEVEL_SCALE )->GetValue());
            StringCchPrintf( sz, 100, L"LPV level: %d", level ); 
            g_HUD.GetStatic( IDC_LEVEL_STATIC )->SetText( sz );
            g_PropLevel = level;
            if(g_propType == CASCADE) g_LPVLevelToInitialize = level;
            else g_LPVLevelToInitialize = HIERARCHICAL_INIT_LEVEL;
            break;
        }
    case IDC_COMBO_VIZ:
        {
            g_currVizChoice = (VIZ_OPTIONS)g_HUD.GetComboBox( IDC_COMBO_VIZ )->GetSelectedIndex();
            break;
        }
    case IDC_COMBO_ADDITIONAL_OPTIONS:
        {
            g_selectedAdditionalOption = (ADITIONAL_OPTIONS_SELECT)g_HUD.GetComboBox( IDC_COMBO_ADDITIONAL_OPTIONS )->GetSelectedIndex();
            updateAdditionalOptions();
            break;
        }
    case IDC_TEX_FOR_MAIN_RENDER:
        {
            g_useTextureForFinalRender =  g_HUD.GetCheckBox( IDC_TEX_FOR_MAIN_RENDER )->GetChecked(); 
            break;
        }
    case IDC_TEX_FOR_RSM_RENDER:
        {
            g_useTextureForRSMs =  g_HUD.GetCheckBox( IDC_TEX_FOR_RSM_RENDER )->GetChecked(); 
            break;
        }
    }
}
//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------

void updateAdditionalOptions()
{
    bool renderSceneOptions = g_selectedAdditionalOption==SCENE? true : false;
    bool renderVizOptions = g_selectedAdditionalOption==VIZ_TEXTURES? true : false;
    bool simpleRenderLightOptions = g_selectedAdditionalOption==SIMPLE_LIGHT? true : false;
    bool advancedRenderLightOptions = g_selectedAdditionalOption==ADVANCED_LIGHT? true : false;


    g_HUD.GetCheckBox( IDC_USEDIFINTERREFLECTION)->SetVisible(simpleRenderLightOptions);
    g_HUD.GetStatic( IDC_DIRECT_STATIC)->SetVisible(simpleRenderLightOptions);
    g_HUD.GetSlider( IDC_DIRECT_SCALE)->SetVisible(simpleRenderLightOptions);
    g_HUD.GetStatic( IDC_DIR_STATIC)->SetVisible(simpleRenderLightOptions);
    g_HUD.GetSlider( IDC_DIR_SCALE)->SetVisible(simpleRenderLightOptions);
    g_HUD.GetStatic( IDC_DIRECT_STRENGTH_STATIC)->SetVisible(simpleRenderLightOptions);
    g_HUD.GetSlider( IDC_DIRECT_STRENGTH_SCALE)->SetVisible(simpleRenderLightOptions);
    g_HUD.GetStatic( IDC_FLUX_AMP_STATIC)->SetVisible(simpleRenderLightOptions);
    g_HUD.GetSlider( IDC_FLUX_AMP_SCALE)->SetVisible(simpleRenderLightOptions);

    g_HUD.GetStatic( IDC_RADIUS_STATIC)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetSlider( IDC_RADIUS_SCALE)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetCheckBox( IDC_USE_OCCLUSION)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetCheckBox( IDC_USE_BILINEAR_INIT)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetCheckBox( IDC_MULTIPLE_BOUNCES)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetStatic( IDC_REF_LIGHT_AMP_STATIC)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetSlider( IDC_REF_LIGHT_AMP_SCALE)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetStatic( IDC_OCCLUSION_AMP_STATIC)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetSlider( IDC_OCCLUSION_AMP_SCALE)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetComboBox( IDC_COMBO_PROP_TYPE)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetCheckBox( IDC_MOVABLE_LPV)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetButton( IDC_RESET_LPV_XFORM)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetCheckBox( IDC_USE_SINGLELPV)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetCheckBox( IDC_USE_RSM_CASCADE)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetStatic( IDC_LEVEL_STATIC)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetSlider( IDC_LEVEL_SCALE)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetStatic( IDC_VPLDISP_STATIC)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetSlider( IDC_VPLDISP_SCALE)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetStatic( IDC_PROPAGATEITERATIONS_STATIC)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetSlider( IDC_PROPAGATEITERATIONS_SCALE)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetStatic( IDC_NUMDEPTHPEELINGPASSES_STATIC)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetSlider( IDC_NUMDEPTHPEELINGPASSES_SCALE)->SetVisible(advancedRenderLightOptions);
    //g_HUD.GetStatic( IDC_LPVSCALE_STATIC)->SetVisible(advancedRenderLightOptions);
    //g_HUD.GetSlider( IDC_LPVSCALE_SCALE)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetCheckBox( IDC_VIZLPVBB)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetCheckBox( IDC_DIRECTIONAL_CLAMPING)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetStatic( IDC_DIR_OFFSET_AMOUNT_STATIC)->SetVisible(advancedRenderLightOptions);
    g_HUD.GetSlider( IDC_DIR_OFFSET_AMOUNT_SCALE)->SetVisible(advancedRenderLightOptions);

    g_HUD.GetCheckBox( IDC_USESM)->SetVisible(renderSceneOptions);
    g_HUD.GetCheckBox( IDC_VIZMESH)->SetVisible(renderSceneOptions);
    g_HUD.GetCheckBox( IDC_TEX_FOR_MAIN_RENDER)->SetVisible(renderSceneOptions);
    g_HUD.GetCheckBox( IDC_TEX_FOR_RSM_RENDER)->SetVisible(renderSceneOptions);
    g_HUD.GetCheckBox( IDC_SHOW_MESH)->SetVisible(renderSceneOptions);
    g_HUD.GetStatic( IDC_SMDEPTHBIAS_STATIC)->SetVisible(renderSceneOptions);
    g_HUD.GetSlider( IDC_SMDEPTHBIAS_SCALE)->SetVisible(renderSceneOptions);
    //g_HUD.GetStatic( IDC_SM_TAPS_STATIC)->SetVisible(renderSceneOptions);
    //g_HUD.GetSlider( IDC_SM_TAPS_SCALE)->SetVisible(renderSceneOptions);
    //g_HUD.GetStatic( IDC_SMFILTERSIZE_STATIC)->SetVisible(renderSceneOptions);
    //g_HUD.GetSlider( IDC_SMFILTERSIZE_SCALE)->SetVisible(renderSceneOptions);
    g_HUD.GetCheckBox(IDC_ANIMATE_LIGHT)->SetVisible(renderSceneOptions);

    g_HUD.GetComboBox( IDC_COMBO_VIZ)->SetVisible(renderVizOptions);
    g_HUD.GetCheckBox( IDC_VIZSM)->SetVisible(renderVizOptions);
    g_HUD.GetCheckBox( IDC_VIZLPV3D)->SetVisible(renderVizOptions);


}


void InitApp()
{
    resetSettingValues();

    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent );
    int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22 );
    
    WCHAR sz[100];
    StringCchPrintf( sz, 100, L"Window size: %dx%d", g_WindowWidth, g_WindowHeight );
    g_HUD.AddStatic( IDC_INFO, sz, 200, iY, 125, 22 );
    
    //g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2 );

    //reset settings to default
    g_HUD.AddButton( IDC_RESET_SETTINGS, L"reset", 35, iY += 24, 125, 22 );
    iY += 5;

       //--------------------------------------------------------------------------------------
    //choose between which option to show
    iY += 35;
    g_HUD.AddStatic( IDC_COMBO_OPTIONS_STATIC, L"Options Subset", 0, iY += 24, 145, 22 );
    iY += 5;
    g_HUD.AddComboBox( IDC_COMBO_ADDITIONAL_OPTIONS, -30, iY += 24, 208, 22 );
    for(int i=0; g_tstr_addOpts[i] != NULL; i++)
        g_HUD.GetComboBox( IDC_COMBO_ADDITIONAL_OPTIONS )->AddItem( g_tstr_addOpts[i], NULL );
    g_HUD.GetComboBox( IDC_COMBO_ADDITIONAL_OPTIONS )->SetSelectedByIndex( g_selectedAdditionalOption );
    iY += 15;

    int iyPos = iY;

    //Basic light properties----------------------------------------------------------------------

    iY = iyPos;

    StringCchPrintf( sz, 100, L"directLight strength: %0.1f", g_directLightStrength ); 
    g_HUD.AddStatic( IDC_DIRECT_STRENGTH_STATIC, sz, -70, iY += 24, 155, 22 );
    g_HUD.AddSlider( IDC_DIRECT_STRENGTH_SCALE, -70, iY += 20, 210, 22, 0, 50, (int)(g_directLightStrength*10) );

    g_HUD.AddCheckBox( IDC_USEDIFINTERREFLECTION, TEXT("Enable interreflection"), -70, iY += 24, 177, 22, g_useDiffuseInterreflection);    
    StringCchPrintf( sz, 100, L"interreflect contribution: %0.3f", g_diffuseInterreflectionScale ); 
    g_HUD.AddStatic( IDC_DIR_STATIC, sz, -70, iY += 24, 125, 22 );
    g_HUD.AddSlider( IDC_DIR_SCALE, -70, iY += 20, 210, 22, 0, 2000, (int)(g_diffuseInterreflectionScale*1000) );
    g_HUD.GetStatic( IDC_DIR_STATIC )->SetEnabled(g_useDiffuseInterreflection);
    g_HUD.GetSlider( IDC_DIR_SCALE )->SetEnabled(g_useDiffuseInterreflection);

    StringCchPrintf( sz, 100, L"directLight contribution: %0.3f", g_directLight ); 
    g_HUD.AddStatic( IDC_DIRECT_STATIC, sz, -70, iY += 24, 155, 22 );
    g_HUD.AddSlider( IDC_DIRECT_SCALE, -70, iY += 20, 210, 22, 0, 3200, (int)(g_directLight*1000) );

    StringCchPrintf( sz, 100, L"Flux Amplifier: %0.3f", g_fluxAmplifier ); 
    g_HUD.AddStatic( IDC_FLUX_AMP_STATIC, sz, -32, iY += 24, 177, 22 );
    g_HUD.AddSlider( IDC_FLUX_AMP_SCALE, -70, iY += 20, 210, 22, 200, 600, (int)(g_fluxAmplifier*100) );

    //Advanced light properties----------------------------------------------------------------------
    iY = iyPos;

    StringCchPrintf( sz, 100, L"Light radius: %0.2f", g_lightRadius ); 
    g_HUD.AddStatic( IDC_RADIUS_STATIC, sz, -32, iY += 24, 125, 22 );
    g_HUD.AddSlider( IDC_RADIUS_SCALE, 50, iY += 20, 100, 22, 0, 10000, (int)(g_lightRadius*100) );

    g_HUD.AddCheckBox( IDC_USE_BILINEAR_INIT, TEXT("bilinear init"), -32, iY += 24, 125, 22, g_useBilinearInit);
    g_HUD.AddCheckBox( IDC_USE_RSM_CASCADE, TEXT("use RSM cacade"), -32, iY += 24, 125, 22, g_useRSMCascade);

    g_HUD.AddCheckBox( IDC_USE_OCCLUSION, TEXT("use occlusion"), -32, iY += 24, 125, 22, g_useOcclusion);
    g_HUD.AddCheckBox( IDC_MULTIPLE_BOUNCES, TEXT("multiple bounces"), -32, iY += 24, 125, 22, g_useMultipleBounces);

    StringCchPrintf( sz, 100, L"depth peel layers: %d", g_numDepthPeelingPasses ); 
    g_HUD.AddStatic( IDC_NUMDEPTHPEELINGPASSES_STATIC, sz, -32, iY += 24, 125, 22 );
    g_HUD.AddSlider( IDC_NUMDEPTHPEELINGPASSES_SCALE, 50, iY += 20, 100, 22, 0, 6, g_numDepthPeelingPasses );
    g_HUD.GetStatic( IDC_NUMDEPTHPEELINGPASSES_STATIC )->SetEnabled(g_useOcclusion || g_useMultipleBounces);
    g_HUD.GetSlider( IDC_NUMDEPTHPEELINGPASSES_SCALE )->SetEnabled(g_useOcclusion || g_useMultipleBounces);

    StringCchPrintf( sz, 100, L"Occlusion Amplifier: %0.3f", g_occlusionAmplifier ); 
    g_HUD.AddStatic( IDC_OCCLUSION_AMP_STATIC, sz, -32, iY += 24, 177, 22 );
    g_HUD.AddSlider( IDC_OCCLUSION_AMP_SCALE, 50, iY += 20, 100, 22, 0, 200, (int)(g_occlusionAmplifier*100) );

    StringCchPrintf( sz, 100, L"Reflected Light Amp: %0.3f", g_reflectedLightAmplifier ); 
    g_HUD.AddStatic( IDC_REF_LIGHT_AMP_STATIC, sz, -32, iY += 24, 177, 22 );
    g_HUD.AddSlider( IDC_REF_LIGHT_AMP_SCALE, 50, iY += 20, 100, 22, 0, 600, (int)(g_reflectedLightAmplifier*100) );

    g_HUD.GetStatic( IDC_REF_LIGHT_AMP_STATIC )->SetEnabled(g_useMultipleBounces);
    g_HUD.GetSlider( IDC_REF_LIGHT_AMP_SCALE )->SetEnabled(g_useMultipleBounces);
    g_HUD.GetStatic( IDC_OCCLUSION_AMP_STATIC )->SetEnabled(g_useOcclusion);
    g_HUD.GetSlider( IDC_OCCLUSION_AMP_SCALE )->SetEnabled(g_useOcclusion);

    iY += 5;
    g_HUD.AddComboBox( IDC_COMBO_PROP_TYPE, -32, iY += 24, 178, 22 );
    for(int i=0; g_tstr_propType[i] != NULL; i++)
        g_HUD.GetComboBox( IDC_COMBO_PROP_TYPE )->AddItem( g_tstr_propType[i], NULL );
    g_HUD.GetComboBox( IDC_COMBO_PROP_TYPE )->SetSelectedByIndex( g_propType );
    iY += 5;

    g_HUD.AddCheckBox( IDC_MOVABLE_LPV, TEXT("movable LPV"), -32, iY += 24, 125, 22, g_movableLPV);
    
    iY+=10;
    g_HUD.AddButton( IDC_RESET_LPV_XFORM, TEXT("reset LPV xform"), -32, iY += 24, 125, 22);
    iY+=10;

    g_HUD.AddCheckBox( IDC_USE_SINGLELPV, TEXT("use single LPV"), -32, iY += 24, 125, 22, g_bUseSingleLPV);

    StringCchPrintf( sz, 100, L"LPV level: %d", g_PropLevel ); 
    g_HUD.AddStatic( IDC_LEVEL_STATIC, sz, -32, iY += 24, 125, 22 );
    g_HUD.AddSlider( IDC_LEVEL_SCALE, 50, iY += 20, 100, 22, 0, g_numHierarchyLevels-1, g_PropLevel );
    g_HUD.GetSlider( IDC_LEVEL_SCALE )->SetEnabled(g_bUseSingleLPV);
    g_HUD.GetStatic( IDC_LEVEL_STATIC )->SetEnabled(g_bUseSingleLPV);

    StringCchPrintf( sz, 100, L"VPL displace: %0.3f", g_VPLDisplacement ); 
    g_HUD.AddStatic( IDC_VPLDISP_STATIC, sz, -32, iY += 24, 125, 22 );
    g_HUD.AddSlider( IDC_VPLDISP_SCALE, 50, iY += 20, 100, 22, 0, 400, (int)(g_VPLDisplacement*100) );

    StringCchPrintf( sz, 100, L"LPV iterations: %d", g_numPropagationStepsLPV ); 
    g_HUD.AddStatic( IDC_PROPAGATEITERATIONS_STATIC, sz, -32, iY += 24, 125, 22 );
    g_HUD.AddSlider( IDC_PROPAGATEITERATIONS_SCALE, 50, iY += 20, 100, 22, 0, max(g_LPVWIDTH,max(g_LPVHEIGHT,g_LPVDEPTH)), g_numPropagationStepsLPV );

/*
    StringCchPrintf( sz, 100, L"LPV scale: %d", (int)g_LPVscale ); 
    g_HUD.AddStatic( IDC_LPVSCALE_STATIC, sz, -32, iY += 24, 125, 22 );
    g_HUD.AddSlider( IDC_LPVSCALE_SCALE, 50, iY += 20, 100, 22, 10, 500, (int)g_LPVscale*10 );
*/
    g_HUD.AddCheckBox( IDC_VIZLPVBB, TEXT("viz LPV Bounding Box"), -32, iY += 24, 125, 22, g_bVizLPVBB);

    g_HUD.AddCheckBox( IDC_DIRECTIONAL_CLAMPING, TEXT("normal based damping"), -32, iY += 24, 125, 22, g_useDirectionalDerivativeClamping);

    StringCchPrintf( sz, 100, L"Dir Damping Amount: %0.3f", g_directionalDampingAmount ); 
    g_HUD.AddStatic( IDC_DIR_OFFSET_AMOUNT_STATIC, sz, -32, iY += 24, 125, 22 );
    g_HUD.AddSlider( IDC_DIR_OFFSET_AMOUNT_SCALE, 50, iY += 20, 100, 22, 0, 100, (int)(g_directionalDampingAmount*100) );

    //scene rendering parameters--------------------------------------------
    iY = iyPos;

    g_HUD.AddCheckBox( IDC_USESM, TEXT("use shadowmap") , -30, iY += 24, 145, 22, g_bUseSM );
    g_HUD.AddCheckBox( IDC_VIZMESH, TEXT("render scene"), -30, iY += 24, 145, 22, g_renderMesh);
    g_HUD.AddCheckBox( IDC_TEX_FOR_MAIN_RENDER, TEXT("textures for rendering"), -30, iY += 24, 145, 22, g_useTextureForFinalRender );
    g_HUD.AddCheckBox( IDC_TEX_FOR_RSM_RENDER, TEXT("textures for RSM"), -30, iY += 24, 145, 22, g_useTextureForRSMs );

    iY += 5;
    g_HUD.AddCheckBox( IDC_SHOW_MESH, TEXT("show movable mesh"), -30, iY += 24, 145, 22, g_showMovableMesh);

    StringCchPrintf( sz, 100, L"Shadow Depth Bias: %0.4f", g_depthBiasFromGUI ); 
    g_HUD.AddStatic( IDC_SMDEPTHBIAS_STATIC, sz, -32, iY += 24, 177, 22 );
    g_HUD.AddSlider( IDC_SMDEPTHBIAS_SCALE, 20, iY += 20, 100, 22, 0, 50000, (int)(g_depthBiasFromGUI*1000000) );

    g_HUD.AddCheckBox( IDC_ANIMATE_LIGHT, TEXT("animate light"), -30, iY += 24, 145, 22, g_animateLight);

    /*
    StringCchPrintf( sz, 100, L"Shadow map taps: %d", g_smTaps ); 
    g_HUD.AddStatic( IDC_SM_TAPS_STATIC, sz, -32, iY += 24, 177, 22 );
    int smTaps=0;
    if(g_smTaps == 1) smTaps = 0;
    else if(g_smTaps == 8) smTaps = 1;
    else if(g_smTaps == 16) smTaps = 2;
    else if(g_smTaps == 28) smTaps = 3;
    g_HUD.AddSlider( IDC_SM_TAPS_SCALE, 50, iY += 20, 100, 22, 0, 3, smTaps );

    StringCchPrintf( sz, 100, L"Shadowmap filter Size: %0.3f", g_smFilterSize ); 
    g_HUD.AddStatic( IDC_SMFILTERSIZE_STATIC, sz, -32, iY += 24, 177, 22 );
    g_HUD.AddSlider( IDC_SMFILTERSIZE_SCALE, 50, iY += 20, 100, 22, 0, 30, int(g_smFilterSize*10.f) );
    */



    //visualize intermediates parameters------------------------------------
    iY = iyPos;
    //choose the texture to visualize
    iY += 5;
    g_HUD.AddComboBox( IDC_COMBO_VIZ, 0, iY += 24, 178, 22 );
    for(int i=0; g_tstr_vizOptionLabels[i] != NULL; i++)
        g_HUD.GetComboBox( IDC_COMBO_VIZ )->AddItem( g_tstr_vizOptionLabels[i], NULL );
    g_HUD.GetComboBox( IDC_COMBO_VIZ )->SetSelectedByIndex( g_currVizChoice );

    g_HUD.AddCheckBox( IDC_VIZSM, TEXT("viz texture") , 20, iY += 24, 125, 22, g_bVisualizeSM );
    g_HUD.AddCheckBox( IDC_VIZLPV3D, TEXT("viz LPV"), 20, iY += 24, 125, 22, g_bVisualizeLPV3D);

    //----------------------------------------------------------------------

    updateAdditionalOptions();

    //initialize variables
    g_shadowViewport.Width  = RSM_RES;
    g_shadowViewport.Height = RSM_RES;
    g_shadowViewport.MinDepth = 0;
    g_shadowViewport.MaxDepth = 1;
    g_shadowViewport.TopLeftX = 0;
    g_shadowViewport.TopLeftY = 0;
    
    g_shadowViewportScene.Width  = SM_SIZE;
    g_shadowViewportScene.Height = SM_SIZE;
    g_shadowViewportScene.MinDepth = 0;
    g_shadowViewportScene.MaxDepth = 1;
    g_shadowViewportScene.TopLeftX = 0;
    g_shadowViewportScene.TopLeftY = 0;

    initializePresetLight();
}

//initialize all the shadowmap buffers and variables
void initializeReflectiveShadowMaps(ID3D11Device* pd3dDevice)
{
    g_pRSMColorRT = new SimpleRT;
    g_pRSMColorRT->Create2D( pd3dDevice, RSM_RES, RSM_RES, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM );

    g_pRSMAlbedoRT = new SimpleRT;
    g_pRSMAlbedoRT->Create2D( pd3dDevice, RSM_RES, RSM_RES, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM );

    g_pRSMNormalRT = new SimpleRT;
    g_pRSMNormalRT->Create2D( pd3dDevice, RSM_RES, RSM_RES, 1, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM );
    
     D3D11_TEXTURE2D_DESC texDesc;
    texDesc.ArraySize          = 1;
    texDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags     = NULL;
    texDesc.Format             = DXGI_FORMAT_R32_TYPELESS;
    texDesc.Width              = RSM_RES;
    texDesc.Height             = RSM_RES;
    texDesc.MipLevels          = 1;
    texDesc.MiscFlags          = NULL;
    texDesc.SampleDesc.Count   = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Usage              = D3D11_USAGE_DEFAULT;
    g_pShadowMapDS = new DepthRT( pd3dDevice, &texDesc );
    g_pDepthPeelingDS[0] = new DepthRT( pd3dDevice, &texDesc );
    g_pDepthPeelingDS[1] = new DepthRT( pd3dDevice, &texDesc );
}
//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    HRESULT hr;
    V_RETURN(DXUTSetMediaSearchPath(L"..\\Source\\DiffuseGlobalIllumination"));
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Disable gamma correction on this sample
    DXUTSetIsInGammaCorrectMode( false );

    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( KeyboardProc );
    DXUTSetCallbackFrameMove( OnFrameMove );
    
    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    InitApp();

    // Force create a ref device so that feature level D3D_FEATURE_LEVEL_11_0 is guaranteed
    DXUTInit( true, true, 0 ); // Parse the command line, show msgboxes on error, no extra command line params

    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"DirectX 11 Lighting Sample" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_10_0, true, g_WindowWidth, g_WindowHeight);
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}

//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// been destroyed, which generally happens as a result of application termination or 
// windowed/full screen toggles. Resources created in the OnD3D11CreateDevice callback 
// should be released here, which generally includes all D3DPOOL_MANAGED resources. 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();

    SAFE_DELETE( g_pTxtHelper );

    SAFE_RELEASE( g_pVSInitializeLPV );
    SAFE_RELEASE( g_pVSInitializeLPV_Bilinear );
    SAFE_RELEASE( g_pGSInitializeLPV );
    SAFE_RELEASE( g_pGSInitializeLPV_Bilinear );
    SAFE_RELEASE( g_pPSInitializeLPV );
    SAFE_RELEASE( g_pPSInitializeGV );
    SAFE_RELEASE( g_pScreenQuadVS );
    SAFE_RELEASE( g_pScreenQuadPosTexVS2D );
    SAFE_RELEASE( g_pScreenQuadPosTexVS3D );
    SAFE_RELEASE( g_pScreenQuadDisplayPS2D );
    SAFE_RELEASE( g_pScreenQuadDisplayPS2D_floatTextures );
    SAFE_RELEASE( g_pScreenQuadReconstructPosFromDepth );
    SAFE_RELEASE( g_pScreenQuadDisplayPS3D );
    SAFE_RELEASE( g_pScreenQuadDisplayPS3D_floatTextures );
    SAFE_RELEASE( g_pScreenQuadVB );
    SAFE_RELEASE( g_pVizQuadVB );
    SAFE_RELEASE( g_pScreenQuadIL );
    SAFE_RELEASE( g_pScreenQuadPosTexIL );
    SAFE_RELEASE( g_pPos3Tex3IL );

    SAFE_RELEASE( g_pCSAccumulateLPV );
    SAFE_RELEASE( g_pCSAccumulateLPV_singleFloats_8 );
    SAFE_RELEASE( g_pCSAccumulateLPV_singleFloats_4 );
    SAFE_RELEASE( g_pCSPropagateLPV );
    SAFE_RELEASE( g_pCSPropagateLPVSimple );
    SAFE_RELEASE( g_pVSPropagateLPV );
    SAFE_RELEASE( g_pGSPropagateLPV );
    SAFE_RELEASE( g_pPSPropagateLPV );
    SAFE_RELEASE( g_pPSPropagateLPVSimple );
    SAFE_RELEASE( g_pVS );
    SAFE_RELEASE( g_pPS );
    SAFE_RELEASE( g_pPS_separateFloatTextures );
    SAFE_RELEASE( g_pVSRSM );
    SAFE_RELEASE( g_pPSRSM );
    SAFE_RELEASE( g_pVSSM );
    SAFE_RELEASE( g_pVSRSMDepthPeeling );
    SAFE_RELEASE( g_pPSRSMDepthPeel );
    SAFE_RELEASE( g_pSimpleVS );
    SAFE_RELEASE( g_pSimplePS );
    SAFE_RELEASE( g_pVSVizLPV );
    SAFE_RELEASE( g_pPSVizLPV );

    SAFE_RELEASE( g_pcbVSGlobal );
    SAFE_RELEASE( g_pcbLPVinitVS );
    SAFE_RELEASE( g_pcbLPVinitVS2 );
    SAFE_RELEASE( g_pcbLPVinitialize_LPVDims );
    SAFE_RELEASE( g_pcbLPVpropagate );
    SAFE_RELEASE( g_pcbRender );
    SAFE_RELEASE( g_pcbRenderLPV );
    SAFE_RELEASE( g_pcbRenderLPV2 );
    SAFE_RELEASE( g_pcbLPVpropagateGather );
    SAFE_RELEASE( g_pcbLPVpropagateGather2 );
    SAFE_RELEASE( g_pcbGV );
    SAFE_RELEASE( g_pcbSimple );
    SAFE_RELEASE( g_pcbVSPerObject );
    SAFE_RELEASE( g_pcbLPVViz );
    SAFE_RELEASE( g_pcbMeshRenderOptions );
    SAFE_RELEASE( g_pcbSlices3D );
    SAFE_RELEASE( g_reconPos );
    SAFE_RELEASE( g_pcbPSSMTapLocations );
    SAFE_RELEASE( g_pcbInvProjMatrix );

    SAFE_RELEASE( g_pDepthPeelingTexSampler );
    SAFE_RELEASE( g_pDefaultSampler );
    SAFE_RELEASE( g_pComparisonSampler );
    SAFE_RELEASE( g_pLinearSampler );
    SAFE_RELEASE( g_pAnisoSampler );
    
    SAFE_RELEASE( g_pMeshLayout );
    delete g_MainMesh;
    delete g_MainMeshSimplified;
    delete g_MainMovableMesh;
    delete g_MovableBoxMesh;
    g_LowResMesh.Destroy();
    g_MeshBox.Destroy();
    g_MeshArrow.Destroy();

    SAFE_DELETE(g_grid);
    SAFE_DELETE(LPV0Propagate);
    SAFE_DELETE(LPV0Accumulate);
    SAFE_DELETE(GV0);
    SAFE_DELETE(GV0Color);
    SAFE_DELETE(g_pRSMColorRT);
    SAFE_DELETE(g_pRSMAlbedoRT);
    SAFE_DELETE(g_pRSMNormalRT);
    SAFE_DELETE(g_pShadowMapDS);
    SAFE_DELETE(g_pDepthPeelingDS[0]);
    SAFE_DELETE(g_pDepthPeelingDS[1]);

    SAFE_DELETE(g_pSceneShadowMap);

    SAFE_RELEASE(g_pRasterizerStateMainRender);
    SAFE_RELEASE(pRasterizerStateCullNone);
    SAFE_RELEASE(pRasterizerStateWireFrame);
    SAFE_RELEASE(pRasterizerStateCullFront);

    SAFE_RELEASE(g_pNoBlendBS);
    SAFE_RELEASE(g_pAlphaBlendBS);
    SAFE_RELEASE(g_pNoBlend1AddBlend2BS);
    SAFE_RELEASE(g_pMaxBlendBS);
    SAFE_RELEASE(g_normalDepthStencilState);
    SAFE_RELEASE(depthStencilStateComparisonLess);
    SAFE_RELEASE(g_depthStencilStateDisableDepth);

}

void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    SAFE_RELEASE( g_pSceneDepth );
    SAFE_RELEASE( g_pSceneDepthRV );

    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}

void renderShadowMap(ID3D11DeviceContext* pd3dContext, DepthRT* pShadowMapDS, const D3DXMATRIX* projectionMatrix, const D3DXMATRIX* viewMatrix, 
                    int numMeshes, RenderMesh** meshes, D3D11_VIEWPORT shadowViewport, D3DXVECTOR3 lightPos, const float lightRadius, float depthBiasFromGUI)
{
    ID3D11RenderTargetView* pRTVNULL[1] = { NULL };
    pd3dContext->ClearDepthStencilView( *pShadowMapDS, D3D11_CLEAR_DEPTH, 1.0, 0 );
    pd3dContext->OMSetRenderTargets( 0, pRTVNULL, *pShadowMapDS );
    pd3dContext->RSSetViewports(1, &shadowViewport);

    ID3D11PixelShader* NULLPS = NULL;
    pd3dContext->VSSetShader( g_pVSSM, NULL, 0 );
    pd3dContext->PSSetShader( NULLPS, NULL, 0 );

    //render the meshes
    for(int i=0; i<numMeshes; i++)
    {
        RenderMesh* mesh = meshes[i];
        D3DXMATRIX ViewProjClip2TexLight, WVPMatrixLight, WVMatrixITLight, WVMatrix;
        mesh->createMatrices( g_pSceneShadowMapProj, *viewMatrix, &WVMatrix, &WVMatrixITLight, &WVPMatrixLight, &ViewProjClip2TexLight );

        UpdateSceneCB( pd3dContext, lightPos, lightRadius, depthBiasFromGUI, false, &(WVPMatrixLight), &(WVMatrixITLight), &(mesh->m_WMatrix), &(ViewProjClip2TexLight) );

        if(g_subsetToRender==-1)
            mesh->m_Mesh.RenderBounded( pd3dContext, D3DXVECTOR3(0,0,0), D3DXVECTOR3(100000,100000,100000), 0 );
        else
            mesh->m_Mesh.RenderSubsetBounded(0,g_subsetToRender, pd3dContext, D3DXVECTOR3(0,0,0), D3DXVECTOR3(10000,10000,10000), false, 0 );
    }
}

void createPropagationAndGeometryVolumes(ID3D11Device* pd3dDevice)
{
    if(g_propType == HIERARCHY)
    {
        LPV0Propagate = new LPV_RGB_Hierarchy(pd3dDevice);
        LPV0Accumulate = new LPV_RGB_Hierarchy(pd3dDevice);
        GV0 = new LPV_Hierarchy(pd3dDevice);
        GV0Color = new LPV_Hierarchy(pd3dDevice);
    }
    else
    {
        LPV0Propagate = new LPV_RGB_Cascade(pd3dDevice, g_cascadeScale, g_cascadeTranslate );
        LPV0Accumulate = new LPV_RGB_Cascade(pd3dDevice, g_cascadeScale, g_cascadeTranslate );
        GV0 = new LPV_Cascade(pd3dDevice, g_cascadeScale, g_cascadeTranslate );
        GV0Color = new LPV_Cascade(pd3dDevice, g_cascadeScale, g_cascadeTranslate );    
    }

    LPV0Propagate->Create( g_numHierarchyLevels, pd3dDevice, g_LPVWIDTH, g_LPVHEIGHT, g_LPVWIDTH, g_LPVHEIGHT, g_LPVDEPTH, DXGI_FORMAT_R16G16B16A16_FLOAT , true, true, false, true );
    
#ifndef USE_SINGLE_CHANNELS
    LPV0Accumulate->Create( g_numHierarchyLevels, pd3dDevice, g_LPVWIDTH, g_LPVHEIGHT, g_LPVWIDTH, g_LPVHEIGHT, g_LPVDEPTH, DXGI_FORMAT_R16G16B16A16_FLOAT, true, true, false, true);
#else
    //reading from a float16 uav is not allowed in d3d11!
    LPV0Accumulate->Create( g_numHierarchyLevels, pd3dDevice, g_LPVWIDTH, g_LPVHEIGHT, g_LPVWIDTH, g_LPVHEIGHT, g_LPVDEPTH, DXGI_FORMAT_R32_FLOAT, true, false, false, true, 4 );
#endif

    GV0->Create2DArray(g_numHierarchyLevels, pd3dDevice, g_LPVWIDTH, g_LPVHEIGHT, g_LPVDEPTH, DXGI_FORMAT_R16G16B16A16_FLOAT , true );
    GV0Color->Create2DArray(g_numHierarchyLevels, pd3dDevice, g_LPVWIDTH, g_LPVHEIGHT, g_LPVDEPTH, DXGI_FORMAT_R8G8B8A8_UNORM , true );

}

void ComputeRowColsForFlat3DTexture( int depth, int& outCols, int& outRows )
{
    // Compute # of rows and cols for a "flat 3D-texture" configuration
    // (in this configuration all the slices in the volume are spread in a single 2D texture)

    int rows =(int)floorf(sqrtf((float)depth));
    int cols = rows;
    while( rows * cols < depth ) {
        cols++;
    }
    assert( rows*cols >= depth );
    
    outCols = cols;
    outRows = rows;
}


// Poisson disk generated with http://www.coderhaus.com/?p=11
void changeSMTaps(ID3D11DeviceContext* pd3dContext)
{
    if(g_smTaps!=1 && g_smTaps!=8 && g_smTaps!=16 && g_smTaps!=28) g_smTaps = 1;

    D3DXVECTOR4 PoissonDisk1[1] = {D3DXVECTOR4(0.0f, 0.0f, 0, 0)};

    D3DXVECTOR4 PoissonDisk8[8] =
    {
        D3DXVECTOR4(0.02902336f, -0.762744f,0,0),
        D3DXVECTOR4(-0.4718729f, -0.09262539f,0,0),
        D3DXVECTOR4(0.1000665f, -0.09762577f,0,0),
        D3DXVECTOR4(0.2378338f, 0.4170297f,0,0),
        D3DXVECTOR4(0.9537742f, 0.1807702f,0,0),
        D3DXVECTOR4(0.6016041f, -0.4252017f,0,0),
        D3DXVECTOR4(-0.741717f, -0.5353929f,0,0),
        D3DXVECTOR4(-0.1786781f, 0.8091267f,0,0)
    };

    D3DXVECTOR4 PoissonDisk16[16] =
    {
        D3DXVECTOR4(0.1904656f, -0.6218426f,0,0),
        D3DXVECTOR4(-0.1258488f, -0.9434036f,0,0),
        D3DXVECTOR4(0.5911888f, -0.345617f,0,0),
        D3DXVECTOR4(0.1664507f, -0.04516677f,0,0),
        D3DXVECTOR4(-0.1608483f, -0.3104914f,0,0),
        D3DXVECTOR4(-0.5286239f, -0.6659128f,0,0),
        D3DXVECTOR4(-0.3251964f, 0.05574534f,0,0),
        D3DXVECTOR4(0.7012196f, 0.05406655f,0,0),
        D3DXVECTOR4(0.3361487f, 0.4192253f,0,0),
        D3DXVECTOR4(0.7241808f, 0.5223625f,0,0),
        D3DXVECTOR4(-0.599312f, 0.6524374f,0,0),
        D3DXVECTOR4(-0.8909158f, -0.3729527f,0,0),
        D3DXVECTOR4(-0.2111304f, 0.4643686f,0,0),
        D3DXVECTOR4(0.1620989f, 0.9808305f,0,0),
        D3DXVECTOR4(-0.8806558f, 0.09435279f,0,0),
        D3DXVECTOR4(-0.2311532f, 0.8682256f,0,0)
    };

    D3DXVECTOR4 PoissonDisk24[24] =
    {
        D3DXVECTOR4(0.3818467f, 0.5925183f,0,0),
        D3DXVECTOR4(0.1798417f, 0.8695328f,0,0),
        D3DXVECTOR4(0.09424125f, 0.3906686f,0,0),
        D3DXVECTOR4(0.1988628f, 0.05610655f,0,0),
        D3DXVECTOR4(0.7975256f, 0.6026196f,0,0),
        D3DXVECTOR4(0.7692417f, 0.1346178f,0,0),
        D3DXVECTOR4(-0.3684688f, 0.5602454f,0,0),
        D3DXVECTOR4(-0.1773221f, 0.1597976f,0,0),
        D3DXVECTOR4(-0.1607566f, 0.8796939f,0,0),
        D3DXVECTOR4(-0.766114f, 0.4488805f,0,0),
        D3DXVECTOR4(-0.601667f, 0.7814722f,0,0),
        D3DXVECTOR4(-0.506153f, 0.1493255f,0,0),
        D3DXVECTOR4(-0.8958725f, -0.01973226f,0,0),
        D3DXVECTOR4(0.8752386f, -0.4413323f,0,0),
        D3DXVECTOR4(0.5006013f, -0.07411311f,0,0),
        D3DXVECTOR4(0.4929055f, -0.4686971f,0,0),
        D3DXVECTOR4(-0.05599103f, -0.2501699f,0,0),
        D3DXVECTOR4(-0.5142418f, -0.3453796f,0,0),
        D3DXVECTOR4(-0.493443f, -0.762339f,0,0),
        D3DXVECTOR4(-0.2623769f, -0.5478004f,0,0),
        D3DXVECTOR4(0.1288256f, -0.5584031f,0,0),
        D3DXVECTOR4(-0.8512651f, -0.4920075f,0,0),
        D3DXVECTOR4(-0.1360606f, -0.9041532f,0,0),
        D3DXVECTOR4(0.3511299f, -0.8271493f,0,0)
    };

    D3DXVECTOR4 PoissonDisk28[28] =
    {
        D3DXVECTOR4(-0.6905488f, 0.09492259f,0,0),
        D3DXVECTOR4(-0.7239041f, -0.3711901f,0,0),
        D3DXVECTOR4(-0.1990684f, -0.1351167f,0,0),
        D3DXVECTOR4(-0.8588699f, 0.4396836f,0,0),
        D3DXVECTOR4(-0.4826424f, 0.320396f,0,0),
        D3DXVECTOR4(-0.9968387f, 0.01040132f,0,0),
        D3DXVECTOR4(-0.5230064f, -0.596889f,0,0),
        D3DXVECTOR4(-0.2146133f, -0.6254999f,0,0),
        D3DXVECTOR4(-0.6389362f, 0.7377159f,0,0),
        D3DXVECTOR4(-0.1776157f, 0.6040277f,0,0),
        D3DXVECTOR4(-0.01479932f, 0.2212604f,0,0),
        D3DXVECTOR4(-0.3635045f, -0.8955025f,0,0),
        D3DXVECTOR4(0.3450507f, -0.7505886f,0,0),
        D3DXVECTOR4(0.1438699f, -0.1978877f,0,0),
        D3DXVECTOR4(0.06733564f, -0.9922826f,0,0),
        D3DXVECTOR4(0.1302602f, 0.758476f,0,0),
        D3DXVECTOR4(-0.3056195f, 0.9038011f,0,0),
        D3DXVECTOR4(0.387158f, 0.5397643f,0,0),
        D3DXVECTOR4(0.1010145f, -0.5530168f,0,0),
        D3DXVECTOR4(0.6531418f, 0.08325134f,0,0),
        D3DXVECTOR4(0.3876107f, -0.4529504f,0,0),
        D3DXVECTOR4(0.7198777f, -0.3464415f,0,0),
        D3DXVECTOR4(0.9582281f, -0.1639438f,0,0),
        D3DXVECTOR4(0.6608706f, -0.7009276f,0,0),
        D3DXVECTOR4(0.2853746f, 0.1097673f,0,0),
        D3DXVECTOR4(0.715556f, 0.3905755f,0,0),
        D3DXVECTOR4(0.6451758f, 0.7568412f,0,0),
        D3DXVECTOR4(0.4597791f, -0.1513058f,0,0)
    };

    D3D11_MAPPED_SUBRESOURCE MappedResource;
    pd3dContext->Map( g_pcbPSSMTapLocations, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    CB_SM_TAP_LOCS* pSMTaps = ( CB_SM_TAP_LOCS* )MappedResource.pData;
    for(int i=0;i<MAX_P_SAMPLES;++i) pSMTaps->samples[i] = D3DXVECTOR4(1.f, 1.f, 1.f, 1.f);
    if(g_smTaps==1) memcpy(pSMTaps->samples,PoissonDisk1,g_smTaps*sizeof(D3DXVECTOR4));
    else if(g_smTaps==8) memcpy(pSMTaps->samples,PoissonDisk8,g_smTaps*sizeof(D3DXVECTOR4));
    else if(g_smTaps==16) memcpy(pSMTaps->samples,PoissonDisk16,g_smTaps*sizeof(D3DXVECTOR4));
    else if(g_smTaps==28) memcpy(pSMTaps->samples,PoissonDisk28,g_smTaps*sizeof(D3DXVECTOR4));
    pSMTaps->numTaps = g_smTaps;
    pSMTaps->filterSize = g_smFilterSize;
    pd3dContext->Unmap( g_pcbPSSMTapLocations, 0 );
    pd3dContext->PSSetConstantBuffers( 9, 1, &g_pcbPSSMTapLocations );

}




//---------------------------------------------------------------------------
//code for animating the light position over time

float speedDelta = 0.25f;

#define NUM_LIGHT_PRESETS 6
D3DXVECTOR3 g_LightPresetPos[NUM_LIGHT_PRESETS];

void initializePresetLight()
{
    g_LightPresetPos[0] = D3DXVECTOR3(-8.19899f, 19.372f, -7.36969f);
    g_LightPresetPos[1] = D3DXVECTOR3(-11.424f, 19.1181f, -0.895488f);
    g_LightPresetPos[2] = D3DXVECTOR3(-6.54583f, 21.3054f, 0.202703f);
    g_LightPresetPos[3] = D3DXVECTOR3(-6.06447f, 20.4372f, 6.50802f);
    g_LightPresetPos[4] = D3DXVECTOR3(-5.75932f, 19.2476f, 9.65256f);
    g_LightPresetPos[5] = D3DXVECTOR3(1.22359f, 11.173f, 21.2478f);
}

void incrementPresetCamera()
{
    if(g_animateLight)
    {

        g_lightPreset = (g_lightPreset+1);
        g_lightPreset = g_lightPreset%NUM_LIGHT_PRESETS;
        g_lightPosDest = g_LightPresetPos[g_lightPreset];
    }
}


static void stepVelocity(D3DXVECTOR3 &vel, const D3DXVECTOR3 &currPos, const D3DXVECTOR3 &destPos)
{
    D3DXVECTOR3 deltaVel = destPos- currPos;
    const float dist = D3DXVec3Length(&deltaVel);
    D3DXVec3Normalize(&deltaVel, &deltaVel);
    deltaVel *= speedDelta;
    vel = deltaVel;
}

void updateLight(float dtime)
{
    if(g_animateLight)
    {
        D3DXVECTOR3 lightPosVel = D3DXVECTOR3(0,0,0);
        stepVelocity(lightPosVel, g_lightPos, g_lightPosDest);
        g_lightPos   += lightPosVel*dtime;
        g_LightCamera.SetViewParams(&g_lightPos,&g_center);

        D3DXVECTOR3 dist = D3DXVECTOR3(g_lightPosDest-g_lightPos);
        if(D3DXVec3Length(&dist)<0.1)
            incrementPresetCamera();
    }
}