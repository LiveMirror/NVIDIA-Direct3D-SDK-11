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


//Notes:
//collision detection for multistrand interpolated hair strands:
//In a prepass we render all the interpolant hair to a texture, all vertices of an interpolated hair strand write to the same pixel. 
//for each vertex we output its vertexID if that vertex collides with a collision implicit. pixels are rendered with min blending. 
//The result is a texture that encodes for each interpolated strand whether any of its vertices intersect with a collision implicit, 
//and if they do, what is the first vertex that does so.
//The use of this is that when we are creating interpolated hair strands using multistrand interpolation, often interpolated strands 
//end up intersecting collision implicits even tho the guide hair did not.
//When such a situation occurs its useful to switch interpolation mode of the strand over to single strand based interpolation. 
//However, it is not enough just to switch interpolation mode of the colliding vertices of the interpolated strand; all vertices after 
//the colliding vertex (as well as some before) have to be switched over to singlestrand based interpolation otherwise the results are not satisfactory

//Mouse movements:
//left click: move the camera
//right click: move the model
//shift + left click: move the light
//shift + right click: move the wind vector
//--------------------------------------------------------------------------------------

#pragma warning(disable:4201) //avoid warning from mmsystem.h "warning C4201: nonstandard extension used : nameless struct/union"

/* uncomment to help track memory leaks
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
*/

#include "Common.h"
#include "Hair.h"
#include "Fluid.h"

#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsdlg.h"
#include "DXUTShapes.h"
#include <sdkmisc.h>

#include "resource.h"
#include "DensityGrid.h"
#include "HairShadows.h"
#include <sdkmesh.h>

#include <fstream>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <stdio.h>
#include <stddef.h>

#include <Strsafe.h>

using namespace std;


/* uncomment to help track memory leaks
#ifdef _DEBUG
#define DEBUG_NEW new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define new DEBUG_NEW
#endif
*/

#define sqr(a) ( (a)*(a) )

bool g_useSOInterpolatedAttributes = true;
INTERPOLATE_MODEL g_InterpolateModel = HYBRID;

//super sampling
ID3D11Texture2D* g_pSceneColorBuffer2XMS;
ID3D11Texture2D* g_pSceneColorBuffer2X;
ID3D11RenderTargetView* g_pSceneRTV2X;
ID3D11Texture2D* g_pSceneDepthBuffer2X;
ID3D11DepthStencilView* g_pSceneDSV2X;
D3D11_TEXTURE2D_DESC g_pRTVTex2DDesc;

// Window Dimensions
int                         g_Width  = 1920.0f;
int                         g_Height = 1200.0f; 

int g_fluidGridWidth = 32;
int g_fluidGridHeight = 32;
int g_fluidGridDepth = 32;
int g_gridSize = 32; //density grid
static const unsigned NHAIRS_PER_PATCH = 64;
static const unsigned NSEGMENTS_PER_PATCH = 64;

D3DXMATRIX g_currentHairTransform;
D3DXMATRIX g_oldHairTransform;
D3DXMATRIX g_InvInitialTotalTransform;
D3DXMATRIX g_InvInitialRotationScale;
D3DXMATRIX g_InitialTotalTransform;
D3DXMATRIX g_ScalpToMesh;
D3DXMATRIX g_gridWorldInv;
string g_HairBoneName; //this is the name of the bone that the hair is attached to
D3DXMATRIX g_ExternalCameraView;
D3DXVECTOR3 g_ExternalCameraEye;
D3DXVECTOR3 g_ExternalCameaLookAtPt;
unsigned g_MaxSStrandsPerControlHair = 0;
unsigned g_MaxMStrandsPerWisp = 0;
float g_MaxWorldScale = 1;

bool g_useSkinnedCam = false;


//recording mouse movements as transforms that can be replayed later
ofstream g_transformsFileOut;
ifstream g_transformsFileIn;
int g_animationIndex = 0;
bool g_recordTransforms = false;
bool g_playTransforms = false;
bool g_loopTransforms = false;
TCHAR* g_tstr_animations[] = 
{ 
	TEXT("RecordedTransforms.txt"), 
	TEXT("RecordedTransforms1.txt"),
	TEXT("RecordedTransforms2.txt"),
	NULL
};


bool g_RestoreHairStrandsToDefault = false;
bool g_useShortHair = false;

bool g_printHelp = false;

float skinColor[4] = {205/255.0f,172/255.0f,131/255.0f,1};

hairShadingParameters hairShadingParams[3];
int g_currHairShadingParams = 0;
TCHAR* g_tstr_shadingOptionLabels[] = 
{ 
	TEXT("blond"), 
	TEXT("brown"),
	TEXT("red"),
	NULL
};


char g_loadDirectory[MAX_PATH];
char* MayaHairDirectory = "LongHairResources";
char* ProceduralHairDirectory = "ProceduralHairResources";

ofstream g_ForcesFile;

TCHAR* g_tstr_blurTypeLabels[] = 
{ 
	TEXT("Gaussian"), 
	TEXT("Box"),
	NULL
};
BLURTYPE g_currentBlurChoice = GAUSSIAN_BLUR;

float g_fNumHairsLOD = 1.0f;
float g_fWidthHairsLOD = 1.0f;
float g_LODScalingFactor = 1.2f;

TCHAR* g_tstr_interHairForceTypeLabels[] = 
{ 
	TEXT("Gradient based"), 
	TEXT("From cell center"),
	NULL
};
int g_currentInterHairForcesChoice = 0; //0 is gradient based


// Any macros to be passed to the shaders used in this sample (for conditional compilation)
D3DXMACRO g_shadersMacros[] = 
{
	NULL
};
D3DXMACRO *g_pShadersMacros = g_shadersMacros;


HairGrid* g_HairGrid = NULL;
Fluid* g_fluid = NULL;

#define MAX_INTERPOLATED_HAIR_PER_WISP 200 //note the g_NumMStrandsPerWisp variable cannot/should not be larger than this
int g_NumMStrandsPerWisp = 45;
int g_NumSStrandsPerWisp = 80;
int g_SSFACTOR = 2;
//#define SUPERSAMPLE //undefine this to enable super sampling for the hair, this will reduce the aliasing but will also be costly

bool g_useCS = true;
bool g_hairReset = false;
bool g_bApplyAdditionalTransform = false;
bool g_useSimulationLOD = true;

int g_totalMStrands = 0;
int g_totalSStrands = 0;

float g_maxHairLength = 0;
float g_scalpMaxRadius;
float g_maxHairLengthWorld;	
float g_scalpMaxRadiusWorld;
float g_maxHairLengthHairSpace;
float g_scalpMaxRadiusHairSpace;

bool g_bVisualizeTessellatedCFs = false;
bool g_bVisualizeCFs = false;

bool g_firstFrame = true;
bool g_renderGUI = true;
bool g_firstMovedFrame = true;

float* g_lengths = NULL;
D3DXVECTOR2* g_clumpCoordinates = NULL;
float2* g_randStrandDeviations = NULL;
float* g_masterStrandLengthToRoot = NULL;
float* g_masterStrandLengths = NULL;
float* g_Strand_Sizes = NULL;
D3DXVECTOR2* g_tangentJitter = NULL;
D3DXVECTOR2* g_BarycentricCoordinates = NULL;
Attributes* g_MasterAttributes = NULL;


D3DXMATRIX g_Transform;
D3DXMATRIX g_RootTransform;
D3DXMATRIX g_TotalTransform;
D3DXVECTOR3 g_Center;
bool g_UseTransformations;

//ground plane
#define GROUND_PLANE_RADIUS 30.0f
ID3D11Buffer*  g_pPlaneVertexBuffer = NULL;
ID3D11Buffer*  g_pPlaneIndexBuffer = NULL;
ID3D11InputLayout* g_pPlaneVertexLayout = NULL;
ID3DX11EffectTechnique*  g_pTechniqueRenderPlane = NULL;

D3D11_VIEWPORT g_superSampleViewport;

//variables for demo---------------------------------------------------------------------------------

bool g_bCurlyHair = false;
bool g_renderDensityOnHair = false;

bool g_applyHForce = true;
bool g_simulate = true;
bool g_reloadEffect = false;
bool g_addGravity = true;
bool g_useLinearStiffness = true;
bool g_useAngularStiffness = false; 
bool g_collisions = true;
bool g_addAngularForces = false; //note this term can lead to some pulsing behaviour in stiff hair when high amounts of force are applied
bool g_RenderCollsionImplicits = false;

float g_timeStep =  0.1f;
float g_WindForce = 0.08f;
float g_windImpulseRandomness = 0.16f;
int g_windImpulseRandomnessSmoothingInterval = 15;
float g_AngularSpringStiffness = 0.025f;
int g_NumTotalConstraintIterations = 20;
int g_numCFIterations = 50;
float g_angularSpringStiffnessMultiplier = 1.0;
float g_GravityStrength = 0.5f; 

//inter hair forces:
int g_BlurRadius = 5;
float g_BlurSigma = 4;
bool g_useBlurredDensityForInterHairForces = true;
float g_densityThreshold = 10.4f;
float g_interHairForces = 0.001f;

//shadows
HairShadows g_HairShadows;
bool g_VisShadowMap = false;
float g_SigmaA = 8.0f;
float g_SoftEdges = 0.0f;
bool g_RenderScene = true;
bool g_RenderScalp = true;
bool g_RenderFloor = true;
bool g_MeshWithShadows = true;
bool g_renderMStrands = true;
bool g_renderSStrands = true;
bool g_useDX11 = true;
bool g_bDynamicLOD = true;
bool g_renderShadows = true;

bool g_takeImage = false;


//shading:
float g_fHairAlpha = 1.0f;


enum UI_OPTION
{
	SHOW_SIMULATION,
	SHOW_HAIR_RENDERING,
	SHOW_SCENE_RENDERING,
	NUM_UI_OPTIONS
};
UI_OPTION g_UIOption = SHOW_HAIR_RENDERING;

TCHAR* g_tstr_uiLabels[] = 
{ 
	TEXT("Simulation UI"), 
	TEXT("Hair Rendering UI"),
	TEXT("Scene Rendering UI"),
	NULL
};

int g_cameraAndWindPreset = 1;
int NumCameraAndWindPresets = 3;



struct HairAdjacencyVertex
{
	int controlVertexIndex;
	float u;
};

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------

enum {
	IDC_TOGGLEFULLSCREEN,
	IDC_TOGGLEREF,
	IDC_CHANGEDEVICE,
	IDC_PLAY_TRANSFORMS,
	IDC_LOOP_ANIMATION,
	IDC_SHOW_SCENE,
	IDC_STRANDB,
	IDC_STRANDC,
	IDC_USEDX11,
	IDC_DYNAMIC_LOD,
	IDC_RESET_SIM,
	IDC_WINDFORCE_STATIC,
	IDC_WINDFORCE_SCALE,
    IDC_NHAIRSLOD_SCALE,
	IDC_NHAIRSWIDTHLOD_SCALE,
	IDC_NHAIRSWIDTHLOD_STATIC,
    IDC_LODSCALEFACTOR_SCALE,
	IDC_LODSCALEFACTOR_STATIC,
	IDC_USE_SHORT_HAIR,
	//different simulation controls
	IDC_SIMULATE,
	IDC_RENDER_COLLISION_IMPLICITS,
	IDC_ADDFORCES,
	IDC_USECOMPUTESHADER,
	IDC_SIMULATION_LOD,
	//rendering:
	IDC_COMBO_SHADING,
	IDC_CURLYHAIR,
	IDC_USE_SHADOWS,
};
//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CModelViewerCamera      g_Camera;               // A model viewing camera
CDXUTDialogResourceManager g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg         g_D3DSettingsDlg;       // Device settings dialog
CDXUTDialog             g_HUD;                  // manages the 3D UI
CDXUTDialog             g_SampleUI;             // dialog for sample specific controls
ID3DX10Font*            g_pFont10;       
ID3DX10Sprite*          g_pSprite10;
ID3DX11Effect*           g_Effect;
TCHAR                   g_pEffectPath[MAX_PATH];
CDXUTTextHelper*        g_pTxtHelper = NULL;
CDXUTDirectionWidget    g_windDirectionWidget;
CDXUTSDKMesh             g_MeshArrow;
CDXUTSDKMesh*             g_Scalp;
CDXUTSDKMesh*             g_FaceMesh;
CDXUTSDKMesh             g_Sphere;

ID3D11ComputeShader* g_pCSSimulateParticles;

int* g_indices = NULL;
float* g_coordinateFrames = NULL;

bool g_bUseGSTessellation = false;
bool g_bUseGSInterpolation = false;
bool g_bUseInstancedInterpolation = true;

//techniques
ID3DX11EffectTechnique*  g_pTechniqueRenderArrow           = NULL;

//render targets
ID3D11RenderTargetView* g_CollisionsRTV = NULL; 

//textures 
ID3D11Texture3D* g_pVelTexture3D = NULL; //3D texture of the simulated wind velocity

int g_currentVB = 0;
int g_currentForceVB = 0;
int g_currentVBCoordinateFrame = 0; 

// Vertex buffers
ID3D11Buffer* g_MasterStrandVB[2];
ID3D11Buffer* g_ForceVB[2];
ID3D11Buffer* g_TessellatedMasterStrandVB = NULL;
ID3D11Buffer* g_TessellatedMasterStrandVBInitial = NULL;
ID3D11Buffer* g_MasterStrandLinearSpringLengthsVB = NULL;
ID3D11Buffer* g_MasterStrandAngularSpringConstraintsVB = NULL;
ID3D11Buffer* g_TessellatedWispVB = NULL;
ID3D11Buffer* g_MasterStrandVBOldTemp = NULL; 
ID3D11Buffer* g_MasterStrandVBOld = NULL;
ID3D11Buffer* g_rootsIndices = NULL;
ID3D11Buffer* g_rootsIndicesUntessellated = NULL;
ID3D11Buffer* g_masterLengths = NULL;
ID3D11Buffer* g_masterLengthsUntessellated = NULL;
ID3D11Buffer* g_UnTessellatedWispVB = NULL;
ID3D11Buffer* g_CoordinateFrameVB[2];
ID3D11Buffer* g_TessellatedCoordinateFrameVB = NULL;
ID3D11Buffer* g_TessellatedMasterStrandLengthToRootVB = NULL;
ID3D11Buffer* g_TessellatedTangentsVB = NULL;
ID3D11Buffer* g_BarycentricInterpolatedPositionsWidthsVB = NULL;
ID3D11Buffer* g_BarycentricInterpolatedIdAlphaTexVB = NULL;
ID3D11Buffer* g_BarycentricInterpolatedTangentVB = NULL;
ID3D11Buffer* g_ClumpInterpolatedPositionsWidthsVB = NULL;
ID3D11Buffer* g_ClumpInterpolatedIdAlphaTexVB = NULL;
ID3D11Buffer* g_ClumpInterpolatedTangentVB = NULL;

//data for compute shaders
ID3D11Buffer* g_MasterStrandUAB = NULL;
ID3D11Buffer* g_MasterStrandPrevUAB = NULL;
ID3D11Buffer* g_CoordinateFrameUAB = NULL;
ID3D11Buffer* g_pcbCSPerFrame = NULL;
ID3D11Buffer* g_pcbCSTransforms = NULL;
ID3D11Buffer* g_pcbCSRootXform = NULL;
ID3D11Buffer* g_MasterStrandSimulationIndicesBuffer = NULL;
ID3D11ShaderResourceView* g_MasterStrandUAB_SRV = NULL;
ID3D11ShaderResourceView* g_MasterStrandLinearSpringLengthsSRV = NULL;
ID3D11ShaderResourceView* g_MasterStrandSimulationIndicesSRV = NULL;
ID3D11ShaderResourceView* g_CoordinateFrameUAB_SRV = NULL;
ID3D11ShaderResourceView* g_AngularStiffnessUAB_SRV = NULL;
ID3D11ShaderResourceView* g_MasterStrandAngularSpringConstraintsSRV = NULL;
ID3D11UnorderedAccessView* g_MasterStrandUAB_UAV = NULL;
ID3D11UnorderedAccessView* g_MasterStrandPrevUAB_UAV = NULL;
ID3D11UnorderedAccessView* g_CoordinateFrameUAB_UAV = NULL;
ID3D11SamplerState *g_pLinearClampSampler = NULL;


ID3D11Buffer* g_StrandSizes = NULL;
ID3D11Buffer* g_StrandCoordinates = NULL;
ID3D11Buffer* g_MasterAttributesBuffer = NULL;
ID3D11Buffer* g_rootsIndicesMasterStrand = NULL;
ID3D11Buffer* g_collisionScreenQuadBuffer = NULL;
ID3D11Buffer* g_ScreenQuadBuffer = NULL;
ID3D11Buffer* g_SuperSampleScreenQuadBuffer = NULL;

// Index buffers
ID3D11Buffer* g_MasterStrandSimulation1IB = NULL;
ID3D11Buffer* g_MasterStrandSimulation2IB = NULL;
ID3D11Buffer* g_MasterStrandSimulationAngular1IB = NULL;
ID3D11Buffer* g_MasterStrandSimulationAngular2IB = NULL;


// Vertex layouts
ID3D11InputLayout* g_MasterStrandIL = NULL;
ID3D11InputLayout* g_MasterStrandILAddForces = NULL;
ID3D11InputLayout* g_MasterStrandSpringConstraintsIL = NULL;
ID3D11InputLayout* g_MasterStrandAngularSpringConstraintsIL = NULL;
ID3D11InputLayout* g_RenderMeshIL = NULL;
ID3D11InputLayout* g_MasterStrandTesselatedInputIL = NULL;
ID3D11InputLayout* g_pVertexLayoutArrow = NULL; // Vertex Layout for arrow
ID3D11InputLayout* g_CoordinateFrameIL = NULL;
ID3D11InputLayout* g_screenQuadIL = NULL;

// Resource views
ID3D11ShaderResourceView* g_TessellatedMasterStrandRV = NULL;
ID3D11ShaderResourceView* g_MasterStrandRV[2];
ID3D11ShaderResourceView* g_ForceRV[2];
ID3D11ShaderResourceView* g_StrandSizesRV = NULL;
ID3D11ShaderResourceView* g_StrandCoordinatesRV = NULL;
ID3D11ShaderResourceView* g_StrandCircularCoordinatesRV = NULL;
ID3D11ShaderResourceView* g_StrandAttributesRV = NULL;
ID3D11ShaderResourceView* g_RootsIndicesRV = NULL;
ID3D11ShaderResourceView* g_RootsIndicesRVUntessellated = NULL;
ID3D11ShaderResourceView* g_MasterLengthsRV = NULL;
ID3D11ShaderResourceView* g_MasterLengthsRVUntessellated = NULL;
ID3D11ShaderResourceView* g_MasterStrandLengthToRootRV = NULL;
ID3D11ShaderResourceView* g_rootsIndicesMasterStrandRV = NULL;
ID3D11ShaderResourceView* g_CoordinateFrameRV[2];
ID3D11ShaderResourceView* g_TessellatedMasterStrandRootIndexRV = NULL;
ID3D11ShaderResourceView* g_TessellatedCoordinateFrameRV = NULL;
ID3D11ShaderResourceView* g_TessellatedMasterStrandLengthToRootRV = NULL;
ID3D11ShaderResourceView* g_StrandLengthsRV = NULL;
ID3D11ShaderResourceView* g_TessellatedTangentsRV = NULL;
ID3D11ShaderResourceView* g_StrandColorsRV = NULL;
ID3D11ShaderResourceView* g_StrandDeviationsRV = NULL;
ID3D11ShaderResourceView* g_CurlDeviationsRV = NULL;
ID3D11ShaderResourceView* g_OriginalMasterStrandRV = NULL;
ID3D11ShaderResourceView* g_CollisionsRV = NULL;
ID3D11ShaderResourceView* g_FluidVelocityRV = NULL;
ID3D11ShaderResourceView* g_originalVectorsRV = NULL;
ID3D11ShaderResourceView* g_MasterStrandLocalVertexNumberRV = NULL;
ID3D11ShaderResourceView* hairTextureRV = NULL;
ID3D11ShaderResourceView* g_TangentJitterRV = NULL;
ID3D11ShaderResourceView* g_MasterStrandLengthsRV = NULL;
ID3D11ShaderResourceView* g_pSceneColorBufferShaderResourceView = NULL;
ID3D11ShaderResourceView* g_BarycentricInterpolatedPositionsWidthsRV = NULL;
ID3D11ShaderResourceView* g_BarycentricInterpolatedIdAlphaTexRV = NULL;
ID3D11ShaderResourceView* g_BarycentricInterpolatedTangentRV = NULL;
ID3D11ShaderResourceView* g_ClumpInterpolatedPositionsWidthsRV = NULL;
ID3D11ShaderResourceView* g_ClumpInterpolatedIdAlphaTexRV = NULL;
ID3D11ShaderResourceView* g_ClumpInterpolatedTangentRV = NULL;
ID3D11ShaderResourceView* g_MstrandsIndexRV = NULL;
ID3D11ShaderResourceView* g_SstrandsIndexRV = NULL;
ID3D11ShaderResourceView* g_SStrandsPerMasterStrandCumulative = NULL;
ID3D11ShaderResourceView* g_MStrandsPerWispCumulative = NULL;
ID3D11ShaderResourceView* g_HairBaseRV[3];

// effect variables
ID3DX11EffectScalarVariable* g_pShadowsShaderVariable = NULL;
ID3DX11EffectScalarVariable* g_pDOMShadowsShaderVariable = NULL;
ID3DX11EffectScalarVariable* g_pApplyHForceShaderVariable = NULL;
ID3DX11EffectScalarVariable* g_pCurlyHairShaderVariable = NULL;
ID3DX11EffectScalarVariable* g_pAddGravityShaderVariable = NULL;
ID3DX11EffectScalarVariable* g_pSimulateShaderVariable = NULL;
ID3DX11EffectScalarVariable* g_InterHairForcesShaderVariable = NULL;
ID3DX11EffectScalarVariable* g_AngularStiffnessMultiplierShaderVariable = NULL;
ID3DX11EffectScalarVariable* g_BlurSigmaShaderVariable = NULL;
ID3DX11EffectScalarVariable* g_BlurRadiusShaderVariable = NULL;
ID3DX11EffectScalarVariable* g_UseBlurTextureForForcesShaderVariable = NULL;
ID3DX11EffectScalarVariable* g_UseGradientBasedForceShaderVariable = NULL;
ID3DX11EffectScalarVariable* g_GravityStrengthShaderVariable = NULL;
ID3DX11EffectScalarVariable* g_UseScalpTextureShaderVariable = NULL;
ID3DX11EffectVectorVariable* g_ArrowColorShaderVariable = NULL;
ID3DX11EffectVectorVariable* g_BaseColorShaderVariable = NULL;
ID3DX11EffectVectorVariable* g_SpecColorShaderVariable = NULL;
ID3DX11EffectShaderResourceVariable* g_hairTextureArrayShaderVariable = NULL;
ID3DX11EffectShaderResourceVariable* g_SceneDepthShaderResourceVariable = NULL;
ID3DX11EffectShaderResourceVariable* g_SceneColor2XShaderResourceVariable = NULL;
ID3DX11EffectMatrixVariable* g_collisionSphereTransformationsEV = NULL;
ID3DX11EffectMatrixVariable* g_collisionSphereInverseTransformationsEV = NULL;
ID3DX11EffectMatrixVariable* g_collisionCylinderTransformationsEV = NULL;
ID3DX11EffectMatrixVariable* g_collisionCylinderInverseTransformationsEV = NULL;
ID3DX11EffectMatrixVariable* g_SphereNoMoveImplicitInverseTransformEV = NULL;
ID3DX11EffectMatrixVariable* g_SphereNoMoveImplicitTransformEV = NULL;

int g_numIndicesFirst;
int g_numIndicesSecond;
int g_numIndicesFirstAngular;
int g_numIndicesSecondAngular;
int g_NumWisps;
int g_NumMasterStrands;
unsigned int g_TessellatedMasterStrandLengthMax;
int g_MaxMStrandSegments;
int g_MasterStrandLengthMax;
int g_NumStrands;
int g_NumMasterStrandControlVertices;
HairVertex* g_MasterStrandControlVertices = NULL;
Index* g_MasterStrandControlVertexOffsets = NULL;
Index* g_TessellatedMasterStrandVertexOffsets = NULL;
D3DXVECTOR3* g_masterCVIndices = NULL;

// Rendering
int g_NumTessellatedMasterStrandVertices;
int g_NumTessellatedWisps;
int g_NumUntessellatedWisps;

const int g_NumStrandVariables = 1024;//size of Interpolated hair variables. this is used for hair thickness, interpolation coordinates
float g_thinning = 0.5; 


bool g_Reset;

//wind
D3DXVECTOR3                 g_windVector = D3DXVECTOR3(0.75089055f, 0.12556140f, 0.64838076f );

//collisions
int g_numSpheres = 0;
int g_numCylinders = 0;
int g_numSpheresNoMoveConstraint = 0;
vector<collisionObject> collisionObjectTransforms;
vector<int> sphereIsBarycentricObstacle;
vector<int> cylinderIsBarycentricObstacle;
vector<int> sphereNoMoveIsBarycentricObstacle;

float g_blendAxis = 0.1f;

//constants for compute shader
const int g_iCBPSPerFrameBind = 0;
const int g_iCBCSTransformBind = 1;
const int g_iCBCSRootXFormBind = 2;

struct CB_CS_PER_FRAME
{
	D3DXMATRIX additionalTransformation;
	D3DXMATRIX RootTransformation;
    D3DXMATRIX currentHairTransformation;
    D3DXMATRIX WorldToGrid;

    int bApplyHorizontalForce;
    int bAddGravity;
    int numConstraintIterations;
    int numCFIterations;

	float angularStiffness;
    float gravityStrength;
    float TimeStep;
	int integrate;

	int bApplyAdditionalTransform;
	int restoreToDefault;
	float blendAxis;
	int addAngularForces;
};

struct CB_CS_TRANSFORMS
{
    int g_NumSphereImplicits;
    int g_NumCylinderImplicits;
    int g_NumSphereNoMoveImplicits;
    int padding;

    D3DXMATRIX CollisionSphereTransformations[MAX_IMPLICITS];
    D3DXMATRIX CollisionSphereInverseTransformations[MAX_IMPLICITS];
    D3DXMATRIX CollisionCylinderTransformations[MAX_IMPLICITS];
    D3DXMATRIX CollisionCylinderInverseTransformations[MAX_IMPLICITS];
    D3DXMATRIX SphereNoMoveImplicitInverseTransform[MAX_IMPLICITS];
};

CB_CS_TRANSFORMS g_cbTransformsInitialData;

struct SimpleVertex
{
	D3DXVECTOR3 Pos;  
	D3DXVECTOR4 Color; 
};

#define NUM_LINES_MAX 1000

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext);
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);
HRESULT CALLBACK OnD3D11SwapChainResized(ID3D11Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);
HRESULT InitApp();
HRESULT CreateHaircut(ID3D11Device*);
HRESULT LoadScalp(ID3D11Device*);
HRESULT loadTextureFromFile(LPCWSTR file,LPCSTR shaderTextureName, ID3D11Device* pd3dDevice);
HRESULT loadTextureFromFile(LPCWSTR file,ID3D11ShaderResourceView** textureRview, ID3D11Device* pd3dDevice);
HRESULT LoadCollisionObstacles(ID3D11Device* pd3dDevice);
HRESULT CreateResources(ID3D11Device*, ID3D11DeviceContext*);
HRESULT CreateBuffers(ID3D11Device* pd3dDevice, ID3D11DeviceContext *pd3dContext);
HRESULT CreateMasterStrandVB(ID3D11Device* pd3dDevice);
HRESULT CreateForceVB(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext);
HRESULT CreateTessellatedMasterStrandVB(ID3D11Device* pd3dDevice);
HRESULT CreateTessellatedWispVB(ID3D11Device*);
HRESULT CreateMasterStrandTesselatedInputVB(ID3D11Device* pd3dDevice);
HRESULT CreateStrandColors(ID3D11Device* pd3dDevice);
HRESULT CreateStrandLengths(ID3D11Device* pd3dDevice);
HRESULT CreateStrandBarycentricCoordinates(ID3D11Device* pd3dDevice);
HRESULT CreateMasterStrandSimulationIBs(ID3D11Device* pd3dDevice);
HRESULT CreateStrandAttributes(ID3D11Device* pd3dDevice);
HRESULT CreateMasterStrandSpringLengthVBs(ID3D11Device* pd3dDevice);
HRESULT CreateCoordinateFrameVB(ID3D11Device* pd3dDevice);
HRESULT CreateTessellatedCoordinateFrameVB(ID3D11Device* pd3dDevice);
HRESULT CreateRandomCircularCoordinates(ID3D11Device* pd3dDevice);
HRESULT CreateDeviantStrandsAndStrandSizes(ID3D11Device* pd3dDevice);
HRESULT CreateCurlDeviations(ID3D11Device* pd3dDevice);
HRESULT CreateCollisionDetectionResources(ID3D11Device* pd3dDevice);
HRESULT CreateTangentJitter(ID3D11Device* pd3dDevice);
HRESULT CreateInterpolatedValuesVBs(ID3D11Device* pd3dDevice);
HRESULT CreateStrandToBarycentricIndexMap(ID3D11Device* pd3dDevice);
HRESULT CreateStrandToClumpIndexMap(ID3D11Device* pd3dDevice);
HRESULT CreateComputeShaderConstantBuffers(ID3D11Device* pd3dDevice);
HRESULT CreateCollisionScreenQuadVB(ID3D11Device* pd3dDevice);
HRESULT CreateScreenQuadVB(ID3D11Device* pd3dDevice);
HRESULT CreateSuperSampleScreenQuadVB(ID3D11Device* pd3dDevice);
HRESULT CreateInputLayouts(ID3D11Device*);
HRESULT CreateMasterStrandIL(ID3D11Device*);
HRESULT CreateMasterStrandTesselatedInputIL(ID3D11Device* pd3dDevice);
HRESULT saveHair();
HRESULT renderInstancedInterpolatedHairForGrid(D3DXMATRIX& worldViewProjection,ID3D11Device* pd3dDevice,ID3D11DeviceContext* pd3dContext,float ZMin, float ZStep, int numRTs );
HRESULT LoadTextureArray( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext, char* sTexturePrefix, int iNumTextures, ID3D11Texture2D** ppTex2D, ID3D11ShaderResourceView** ppSRV,int offset);
HRESULT CreatePlane( ID3D11Device* pd3dDevice, float radius, float height );
HRESULT RepropagateCoordinateFrames(ID3D11DeviceContext* pd3dContext, int iterations);
HRESULT InitEffect(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext);
HRESULT InitializeFluidState(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext);
HRESULT ExtractAndSetCollisionImplicitMatrices();
HRESULT ReinitSuperSampleBuffers(ID3D11Device* pd3dDevice);

//helper functions for hair rendering
void setVariablesForRendering(INTERPOLATE_MODEL interpolateModel, RENDERTYPE renderType);
void unsetVariablesForRendering(INTERPOLATE_MODEL interpolateModel, RENDERTYPE renderType);
void renderLineStrip(ID3D11DeviceContext* pd3dContext, int numPoints);
void renderPointList(ID3D11DeviceContext* pd3dContext, int numPoints);
void renderPointListInstanced(ID3D11DeviceContext* pd3dContext, int vertexCountPerInstance, int instanceCount);
void renderHairWithTessellation(ID3D11DeviceContext* pd3dContext, INTERPOLATE_MODEL interpolateMode, ID3DX11EffectPass* pEffectPass);
bool getEffectPass(INTERPOLATE_MODEL interpolateModel, RENDERTYPE renderType, ID3DX11EffectPass** pEffectPass, bool& bUnsetHSandDS);

//helper functios for hair simulation
void stepFluidSimulation();
HRESULT stepHairSimulationDX11(ID3D11DeviceContext* pd3dContext, int frame);
HRESULT stepHairSimulationDX10(ID3D11DeviceContext* pd3dContext, int frame);

float random();
void PlayTransforms();
HRESULT resetScene(ID3D11Device* pd3dDevice, ID3D11DeviceContext *pd3dContext);
void reloadEffect(ID3D11Device* pd3dDevice, ID3D11DeviceContext *pd3dContext);
void RenderScreenQuad(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext, ID3D11Buffer* buffer);
void RenderArrow(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext, D3DXVECTOR3 pos);
void VisualizeCoordinateFrames(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext, bool drawTessellatedCFs);
void DrawPlane(ID3D11DeviceContext *pd3dContext);
void changeCameraAndWindPresets();
void ReleaseResources();
void vectorMatrixMultiply(D3DXVECTOR3* vecOut,const D3DXMATRIX matrix,const D3DXVECTOR3 vecIn);
void vectorMatrixMultiply(D3DXVECTOR4* vecOut,const D3DXMATRIX matrix,const D3DXVECTOR4 vecIn);
float3 TrasformFromWorldToLocalCF(int index,float3 oldVector);
void RecreateCoordinateFrames(ID3D11DeviceContext* pd3dContext);
void RenderText();
void MoveModel(HWND hWnd, int msg, WPARAM wParam, LPARAM lParam);

void CALLBACK OnD3D11FrameRender(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext, double fTime, float fElapsedTime, void* pUserContext);
void CALLBACK OnD3D11SwapChainReleasing(void* pUserContext);
void CALLBACK OnD3D11DestroyDevice(void* pUserContext);
void CALLBACK OnKeyboard(UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext);
void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext);
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo, DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext);
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings, void* pUserContext);
void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext);

//--------------------------------------------------------------------------------------------------------------
//render the hair
//this function renders hair for all the different options that we have (including with and without tessellation
//---------------------------------------------------------------------------------------------------------------

void RenderInterpolatedHair(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext, RENDERTYPE renderType, INTERPOLATE_MODEL interpolateModel, int numMStrands, int numSStrands)
{
	if (g_useDX11 && renderType == SOATTRIBUTES)
	{
		// we want to streamout attributes, but we don't do use streamout for dx11. dx11 computes and renders directly
		return;
	}
    if(interpolateModel==NO_HAIR) return;

	// we dont need a vertex buffer since we are only using vertexID and instanceID
	unsigned int stride = 0;
	unsigned int offset = 0;
	ID3D11Buffer* buffer[] = { NULL };
	pd3dContext->IASetVertexBuffers(0, 1, buffer, &stride, &offset);
	pd3dContext->IASetInputLayout(NULL);

	//get the effect that we are going to be using, which depends on the interpolateModel, the renderType and on the global variables g_useSOInterpolatedAttributes(are we using StreamOut in the intermediate steps of the Dx10 path?) and g_useDX11(are we using DX11 tessellation?)
	ID3DX11EffectPass* pEffectPass = 0;
	bool bUnsetHSandDS = false;
	bool isValidEffect = getEffectPass(interpolateModel, renderType, &pEffectPass, bUnsetHSandDS);
	if(!isValidEffect) return;

	//set the variables needed
	setVariablesForRendering(interpolateModel, renderType);

    //set the streamout buffers if we are going to be doing streamout for a subsequent pass
	if(interpolateModel == MULTI_HYBRID && renderType == SOATTRIBUTES)
	{
		ID3D11Buffer* SO_buffers[] = { g_BarycentricInterpolatedPositionsWidthsVB, 
			g_BarycentricInterpolatedIdAlphaTexVB,
			g_BarycentricInterpolatedTangentVB};
		UINT offsets[] = { 0, 0, 0 };
		pd3dContext->SOSetTargets(3, SO_buffers, offsets);
	}
	else if(interpolateModel == SINGLESTRAND && renderType == SOATTRIBUTES)
	{
		ID3D11Buffer* SO_buffers[] = { g_ClumpInterpolatedPositionsWidthsVB, 
			g_ClumpInterpolatedIdAlphaTexVB,
			g_ClumpInterpolatedTangentVB};
		UINT offsets[] = { 0, 0, 0 };
		pd3dContext->SOSetTargets(3, SO_buffers, offsets);
	}
	else
		pd3dContext->SOSetTargets(0, 0, 0);
	
	pEffectPass->Apply(0, pd3dContext);    


	//do the actual primitive rendering
	if((interpolateModel == MULTISTRAND || interpolateModel == MULTI_HYBRID) && renderType == SOATTRIBUTES) renderPointList(pd3dContext, g_TessellatedMasterStrandLengthMax*numMStrands);
	else if(interpolateModel == SINGLESTRAND && g_useDX11) renderHairWithTessellation(pd3dContext, interpolateModel, pEffectPass);
	else if(interpolateModel == SINGLESTRAND && renderType == SOATTRIBUTES && !g_useDX11) renderPointList(pd3dContext, g_TessellatedMasterStrandLengthMax*numSStrands);
	else if(interpolateModel == MULTI_HYBRID && g_useDX11) renderHairWithTessellation(pd3dContext, interpolateModel, pEffectPass);
	else if(interpolateModel == MULTI_HYBRID && !g_useDX11) renderLineStrip(pd3dContext, g_TessellatedMasterStrandLengthMax * numMStrands);
	else if (interpolateModel == SINGLESTRAND) renderLineStrip(pd3dContext, g_TessellatedMasterStrandLengthMax * numSStrands);
	else if(interpolateModel == MULTISTRAND && renderType == INSTANCED_INTERPOLATED_COLLISION && g_useDX11)	renderHairWithTessellation(pd3dContext, interpolateModel, pEffectPass);
	else if(interpolateModel == MULTISTRAND && renderType == INSTANCED_INTERPOLATED_COLLISION && !g_useDX11)renderPointListInstanced(pd3dContext,g_TessellatedMasterStrandLengthMax, numMStrands);
			

	//unset the variables
	unsetVariablesForRendering(interpolateModel,renderType);
	pd3dContext->SOSetTargets(0, 0, 0);
	pEffectPass->Apply(0, pd3dContext);
}


void setVariablesForRendering(INTERPOLATE_MODEL interpolateModel, RENDERTYPE renderType)
{
	if (g_useDX11)
	{
		g_Effect->GetVariableByName("g_iSubHairFirstVert")->AsScalar()->SetInt(0);
		g_Effect->GetVariableByName("g_iFirstPatchHair")->AsScalar()->SetInt(0);
	}

	g_useCS ? g_Effect->GetVariableByName("g_MasterStrandSB")->AsShaderResource()->SetResource(g_MasterStrandUAB_SRV) : g_Effect->GetVariableByName("g_MasterStrand")->AsShaderResource()->SetResource(g_MasterStrandRV[g_currentVB]);
	g_Effect->GetVariableByName("g_TessellatedMasterStrand")->AsShaderResource()->SetResource(g_TessellatedMasterStrandRV);
	g_Effect->GetVariableByName("g_tessellatedCoordinateFrames")->AsShaderResource()->SetResource(g_TessellatedCoordinateFrameRV);
	g_Effect->GetVariableByName("g_TessellatedLengthsToRoots")->AsShaderResource()->SetResource(g_TessellatedMasterStrandLengthToRootRV);
	g_Effect->GetVariableByName("g_TessellatedTangents")->AsShaderResource()->SetResource(g_TessellatedTangentsRV);	
	g_HairGrid->setDemuxTexture();
	g_HairGrid->setBlurTexture();

	if (renderType != INSTANCED_INTERPOLATED_COLLISION)
		g_Effect->GetVariableByName("g_CollisionsTexture")->AsShaderResource()->SetResource(g_CollisionsRV);

	if(!g_useDX11)
	{
		if(g_useSOInterpolatedAttributes && interpolateModel == MULTI_HYBRID && renderType != SOATTRIBUTES)
		{
			g_Effect->GetVariableByName("g_InterpolatedPositionAndWidth")->AsShaderResource()->SetResource(g_BarycentricInterpolatedPositionsWidthsRV);
			g_Effect->GetVariableByName("g_InterpolatedIdAlphaTex")->AsShaderResource()->SetResource(g_BarycentricInterpolatedIdAlphaTexRV);
			g_Effect->GetVariableByName("g_Interpolatedtangent")->AsShaderResource()->SetResource(g_BarycentricInterpolatedTangentRV);
		}
		else if(g_useSOInterpolatedAttributes && interpolateModel == SINGLESTRAND && renderType != SOATTRIBUTES)
		{
			g_Effect->GetVariableByName("g_InterpolatedPositionAndWidthClump")->AsShaderResource()->SetResource(g_ClumpInterpolatedPositionsWidthsRV);
			g_Effect->GetVariableByName("g_InterpolatedIdAlphaTexClump")->AsShaderResource()->SetResource(g_ClumpInterpolatedIdAlphaTexRV);
			g_Effect->GetVariableByName("g_InterpolatedtangentClump")->AsShaderResource()->SetResource(g_ClumpInterpolatedTangentRV);
		}
	}
	else
	{
		if (interpolateModel == SINGLESTRAND)
		{
			g_Effect->GetVariableByName("g_SStrandsPerMasterStrandCumulative")->AsShaderResource()->SetResource(g_SStrandsPerMasterStrandCumulative);
		}
		else
		{
			g_Effect->GetVariableByName("g_MStrandsPerWispCumulative")->AsShaderResource()->SetResource(g_MStrandsPerWispCumulative);
		}
	}

	if(!g_simulate)
	    g_Effect->GetVariableByName("g_bApplyAdditionalRenderingTransform")->AsScalar()->SetBool(true);
	else
	    g_Effect->GetVariableByName("g_bApplyAdditionalRenderingTransform")->AsScalar()->SetBool(false);
	g_Effect->GetVariableByName("additionalTransformation")->AsMatrix()->SetMatrix(g_RootTransform);


	if(renderType == INSTANCED_INTERPOLATED_COLLISION)
		g_HairGrid->setObstacleVoxelizedTexture();

	//bind the shadow texture if we are not actually in the pass that renders the shadows
	if( renderType != INSTANCED_DEPTH && renderType != INSTANCED_DEPTH_DOM)
		g_HairShadows.SetHairShadowTexture();

	if(renderType == INSTANCED_DENSITY)
	{
		g_HairGrid->setDemuxTexture();
		g_HairGrid->setBlurTexture(); 
	}
}

void unsetVariablesForRendering(INTERPOLATE_MODEL interpolateModel, RENDERTYPE renderType)
{
	g_useCS ? g_Effect->GetVariableByName("g_MasterStrandSB")->AsShaderResource()->SetResource(NULL) : g_Effect->GetVariableByName("g_MasterStrand")->AsShaderResource()->SetResource(NULL);
	g_Effect->GetVariableByName("g_TessellatedMasterStrand")->AsShaderResource()->SetResource(NULL);
	g_Effect->GetVariableByName("g_tessellatedCoordinateFrames")->AsShaderResource()->SetResource(NULL);
	g_Effect->GetVariableByName("g_TessellatedLengthsToRoots")->AsShaderResource()->SetResource(NULL);
	g_Effect->GetVariableByName("g_TessellatedTangents")->AsShaderResource()->SetResource(NULL);	
	g_HairGrid->unsetDemuxTexture();
	g_HairGrid->unsetBlurTexture();
	if( renderType!= INSTANCED_INTERPOLATED_COLLISION)
		g_Effect->GetVariableByName("g_CollisionsTexture")->AsShaderResource()->SetResource(NULL);
	
	if(g_useSOInterpolatedAttributes && interpolateModel == MULTI_HYBRID)
	{
		if(renderType == INSTANCED_NORMAL_HAIR)
		{
			g_Effect->GetVariableByName("g_InterpolatedPositionAndWidth")->AsShaderResource()->SetResource(NULL);
			g_Effect->GetVariableByName("g_InterpolatedIdAlphaTex")->AsShaderResource()->SetResource(NULL);
			g_Effect->GetVariableByName("g_Interpolatedtangent")->AsShaderResource()->SetResource(NULL);
		}
		else if(renderType == INSTANCED_DEPTH || renderType == INSTANCED_DEPTH_DOM)
		{
			g_Effect->GetVariableByName("g_InterpolatedPositionAndWidth")->AsShaderResource()->SetResource(NULL);
			g_Effect->GetVariableByName("g_InterpolatedIdAlphaTex")->AsShaderResource()->SetResource(NULL);
			g_Effect->GetVariableByName("g_Interpolatedtangent")->AsShaderResource()->SetResource(NULL);
		}
		else if(renderType == INSTANCED_HAIR_DEPTHPASS)
		{
			g_Effect->GetVariableByName("g_InterpolatedPositionAndWidth")->AsShaderResource()->SetResource(NULL);
			g_Effect->GetVariableByName("g_InterpolatedIdAlphaTex")->AsShaderResource()->SetResource(NULL);
			g_Effect->GetVariableByName("g_Interpolatedtangent")->AsShaderResource()->SetResource(NULL);
		}
	}
	if(g_useSOInterpolatedAttributes && interpolateModel == SINGLESTRAND)
	{
		if(renderType == INSTANCED_NORMAL_HAIR)
		{
			g_Effect->GetVariableByName("g_InterpolatedPositionAndWidthClump")->AsShaderResource()->SetResource(NULL);
			g_Effect->GetVariableByName("g_InterpolatedIdAlphaTexClump")->AsShaderResource()->SetResource(NULL);
			g_Effect->GetVariableByName("g_InterpolatedtangentClump")->AsShaderResource()->SetResource(NULL);
		}
		else if(renderType == INSTANCED_DEPTH || renderType == INSTANCED_DEPTH_DOM)
		{
			g_Effect->GetVariableByName("g_InterpolatedPositionAndWidthClump")->AsShaderResource()->SetResource(NULL);
			g_Effect->GetVariableByName("g_InterpolatedIdAlphaTexClump")->AsShaderResource()->SetResource(NULL);
			g_Effect->GetVariableByName("g_InterpolatedtangentClump")->AsShaderResource()->SetResource(NULL);
		}
		else if(renderType == INSTANCED_HAIR_DEPTHPASS)
		{
			g_Effect->GetVariableByName("g_InterpolatedPositionAndWidthClump")->AsShaderResource()->SetResource(NULL);
			g_Effect->GetVariableByName("g_InterpolatedIdAlphaTexClump")->AsShaderResource()->SetResource(NULL);
			g_Effect->GetVariableByName("g_InterpolatedtangentClump")->AsShaderResource()->SetResource(NULL);
		}
	}
	if(renderType == INSTANCED_INTERPOLATED_COLLISION)
		g_HairGrid->unsetObstacleVoxelizedTexture();

	if(renderType == INSTANCED_DENSITY)
	{
		g_HairGrid->unsetDemuxTexture();
		g_HairGrid->unsetBlurTexture(); 
	}

	g_HairShadows.UnSetHairShadowTexture();
}
			
void renderPointList(ID3D11DeviceContext* pd3dContext, int numPoints)
{
    pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	pd3dContext->Draw(numPoints,0); 
}

void renderPointListInstanced(ID3D11DeviceContext* pd3dContext, int vertexCountPerInstance, int instanceCount)
{		
	pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	pd3dContext->DrawInstanced(vertexCountPerInstance,instanceCount,0,0);
}

void renderLineStrip(ID3D11DeviceContext* pd3dContext, int numPoints)
{
    pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
	pd3dContext->Draw(numPoints,0); 
}

void renderHairWithTessellation(ID3D11DeviceContext* pd3dContext, INTERPOLATE_MODEL interpolateModel, ID3DX11EffectPass* pEffectPass)
{
	if(interpolateModel == MULTISTRAND || interpolateModel == MULTI_HYBRID)
	{		
		pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST);
		for (unsigned int g_iFirstPatchHair = 0; ; )
		{
			for (int g_iSubHairFirstVert = 0; ; )
			{
				pEffectPass->Apply(0, pd3dContext);
				pd3dContext->Draw(g_NumWisps, 0);
				g_iSubHairFirstVert += NSEGMENTS_PER_PATCH;
				if (g_iSubHairFirstVert >= g_MaxMStrandSegments)
					break;
				g_Effect->GetVariableByName("g_iSubHairFirstVert")->AsScalar()->SetInt(g_iSubHairFirstVert);
			}
			g_iFirstPatchHair += NHAIRS_PER_PATCH;
			if (g_iFirstPatchHair >= g_MaxMStrandsPerWisp)
				break;
			g_Effect->GetVariableByName("g_iFirstPatchHair")->AsScalar()->SetInt(g_iFirstPatchHair);
			g_Effect->GetVariableByName("g_iSubHairFirstVert")->AsScalar()->SetInt(0);
		}
	}
	else if(interpolateModel == SINGLESTRAND)
	{
	    pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST);
		// render first segments of all hairs (up to 64 first segments - HW limitation)
		for (unsigned int iFirstPatchHair = 0; ; )
		{
			pd3dContext->Draw(g_NumMasterStrands, 0);
			iFirstPatchHair += NHAIRS_PER_PATCH;
			if (iFirstPatchHair >= g_MaxSStrandsPerControlHair)
				break;
			g_Effect->GetVariableByName("g_iFirstPatchHair")->AsScalar()->SetInt(iFirstPatchHair);
			pEffectPass->Apply(0, pd3dContext);
		}
		// if there are more segments - render the rest (64 in each go)
		unsigned iSubHairFirstVert = NSEGMENTS_PER_PATCH;
		while (iSubHairFirstVert < g_TessellatedMasterStrandLengthMax - 1)
		{
			g_Effect->GetVariableByName("g_iSubHairFirstVert")->AsScalar()->SetInt(iSubHairFirstVert);
			for (unsigned int iFirstPatchHair = 0; ; )
			{
				g_Effect->GetVariableByName("g_iFirstPatchHair")->AsScalar()->SetInt(iFirstPatchHair);
				pEffectPass->Apply(0, pd3dContext);
				pd3dContext->Draw(g_NumMasterStrands, 0);
				iFirstPatchHair += NHAIRS_PER_PATCH;
				if (iFirstPatchHair >= g_MaxSStrandsPerControlHair)
					break;
			}
			iSubHairFirstVert += NSEGMENTS_PER_PATCH;
		}
	}

}
			


bool getEffectPass(INTERPOLATE_MODEL interpolateModel, RENDERTYPE renderType, ID3DX11EffectPass** pEffectPass, bool& bUnsetHSandDS)
{
	//in this interpolation mode we render multistrand interpolated hair, but we detect collisions and switch colliding strand vertices to singlestrand based interpolation
	if(interpolateModel == MULTI_HYBRID)
	{
		if(renderType == INSTANCED_NORMAL_HAIR && g_useSOInterpolatedAttributes && !g_useDX11)
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedVS_LOAD");
		else if(renderType == INSTANCED_NORMAL_HAIR && g_useDX11)
		{
			bUnsetHSandDS = true;
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateAndRenderM_HardwareTess");
		}
		else if(renderType == INSTANCED_NORMAL_HAIR)
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedVS");


		else if(renderType == INSTANCED_DEPTH && g_useDX11)
		{
			bUnsetHSandDS = true;
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateAndRenderDepth_HardwareTess");
		}
		else if(renderType == INSTANCED_DEPTH && g_useSOInterpolatedAttributes)
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedDepthVS_LOAD");
		else if(renderType == INSTANCED_DEPTH)
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedDepthVS");


		else if(renderType == INSTANCED_DEPTH_DOM && g_useDX11)
		{
			bUnsetHSandDS = true;
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateAndRenderDepth_HardwareTess_DOM");
		}
		else if(renderType == INSTANCED_DEPTH_DOM && g_useSOInterpolatedAttributes)
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedDepthVS_LOAD_DOM");
		else if(renderType == INSTANCED_DEPTH_DOM)
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedDepthVS_DOM");




		else if(renderType == INSTANCED_HAIR_DEPTHPASS && g_useSOInterpolatedAttributes)
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedDepthPrepassVS_LOAD");
		else if(renderType == INSTANCED_HAIR_DEPTHPASS)
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedDepthPrepassVS");


		else if(renderType == SOATTRIBUTES)
		{
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("SO_M_Attributes");
		}
		else
			return false;
	}
	else if(interpolateModel == SINGLESTRAND) 	//singlestrand based interpolation
	{
		if(renderType == INSTANCED_NORMAL_HAIR && g_useSOInterpolatedAttributes && !g_useDX11)
		{
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedVSSingleStrand_LOAD");
		}
		else if(renderType == INSTANCED_NORMAL_HAIR && !g_useDX11)
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedVSSingleStrand");
		else if(renderType == INSTANCED_DEPTH && g_useSOInterpolatedAttributes && !g_useDX11)
		{
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedDepthVSSingleStrand_LOAD");	
		}
		else if(renderType == INSTANCED_DEPTH && !g_useDX11)
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedDepthVSSingleStrand");	
		
		else if(renderType == INSTANCED_DEPTH_DOM && g_useSOInterpolatedAttributes && !g_useDX11)
		{
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedDepthVSSingleStrand_LOAD_DOM");	
		}
		else if(renderType == INSTANCED_DEPTH_DOM && !g_useDX11)
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedDepthVSSingleStrand_DOM");	



		else if(renderType == INSTANCED_HAIR_DEPTHPASS && g_useSOInterpolatedAttributes)
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedVSSingleStrandDepthPrepass_LOAD");
		else if(renderType == INSTANCED_HAIR_DEPTHPASS)
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedVSSingleStrandDepthPrepass");


		else if (renderType == SOATTRIBUTES)
		{
			assert(!g_useDX11);
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("SO_S_Attributes");
		}
		else if (g_useDX11) //use tessellation to render singlestrand based interpolated hair
		{
			bUnsetHSandDS = true;
			if (renderType == INSTANCED_DEPTH)
				*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedDepthVSSingleStrand11");
			else if (renderType == INSTANCED_DEPTH_DOM)
				*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedDepthVSSingleStrand11_DOM");
			else if (renderType == INSTANCED_NORMAL_HAIR)
				*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateAndRenderS_HardwareTess");
		}
		else
			return false;
	}
	else if(interpolateModel == MULTISTRAND)//multistrand interpolation without detecting or fixing collisions of interpolated strands with collision geometry
	{
		if(renderType == INSTANCED_INTERPOLATED_COLLISION)
		{
			if(g_useDX11)
			{
				bUnsetHSandDS = true;
				*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateAndRenderCollisions_HardwareTess");
			}
			else
				*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedBaryCentricCollisions");
		}
		else if(renderType == INSTANCED_COLLISION_RESULTS)
			*pEffectPass = g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedVSRenderCollisionsNormal");
		else
			return false;
	}
	else
		return false;

	return true;
}


//------------------------------------------------------------------------------------------
//GPU based simulation of hair. This function calls either compute shader based hair 
//simulation or stream out based simulation, depending on what the current user choice is.
//this function is also responsible for invoking the fluid simulation for the wind
//------------------------------------------------------------------------------------------

HRESULT StepHairSimulation(ID3D11DeviceContext* pd3dContext, ID3D11Device* pd3dDevice)
{
	//keeping track of the frames since simulation started. 
	//This is useful for damping the forces in the beginning as hair resolves constraints and settles down (for example the initial hairstyle might have hair strands colliding with obstacles) 
    static int frame = 0;
	if(g_hairReset)
	{    
		frame = 0;
        g_hairReset = false;
	}
	frame++;


	//add wind velocity to the grid, and advect it forward
	//only do this if we are simulating
	if(g_applyHForce && g_simulate)
		stepFluidSimulation();

    //hair simulation using compute shader
    if(g_useCS)
		return stepHairSimulationDX11(pd3dContext, frame);
	
    //hair simulation using Dx10 style GPGPU with Streamout
	return stepHairSimulationDX10(pd3dContext, frame);
}

void stepFluidSimulation()
{
	//transform wind to be in the correct coordinate frame
	D3DXVECTOR3 transformedWind;
	D3DXVECTOR3 windNormVector = g_windDirectionWidget.GetLightDirection();
 	D3DXVec3Normalize(&windNormVector,&windNormVector);
	D3DXVec3TransformCoord(&transformedWind,&windNormVector,&g_InvInitialRotationScale);
	D3DXVec3Normalize(&transformedWind,&transformedWind);


	D3DXVECTOR3 gridCenter = D3DXVECTOR3( g_fluidGridWidth/2.0, g_fluidGridHeight/2.0, g_fluidGridDepth/2.0); 
	float center[4] = { gridCenter.x - transformedWind.x*gridCenter.x,
		gridCenter.y - transformedWind.y*gridCenter.y,
		gridCenter.z - transformedWind.z*gridCenter.z, 0 };
	if( center[0] < 0)
		center[0] = 0;
	if( center[0] >= g_fluidGridWidth )
		center[0] = g_fluidGridWidth-1;

	if( center[1] < 0)
		center[1] = 0;
	if( center[1] >= g_fluidGridHeight )
		center[1] = g_fluidGridHeight-1;

	if( center[2] < 0)
		center[2] = 0;
	if( center[2] >= g_fluidGridDepth )
		center[2] = g_fluidGridDepth-1;


	//demux the hair density into the obstacle texture of the fluid
	g_HairGrid->demultiplexTextureTo3DFluidObstacles();

    //compute a linearly smoothed random variation
	static int time = 0;
    time++;
	static float prevRandOffset = (random()-0.5f)*g_windImpulseRandomness;
	static float currRandOffset = (random()-0.5f)*g_windImpulseRandomness;
	if(time%g_windImpulseRandomnessSmoothingInterval==0)
	{
		time = 0;
	    prevRandOffset = currRandOffset;
	    currRandOffset = (random()-0.5f)*g_windImpulseRandomness;
	}
	float t = (float)time/g_windImpulseRandomnessSmoothingInterval;
	float smoothVariation = t*currRandOffset + (1.0f-t)*prevRandOffset;
	float windForce = max(g_WindForce+smoothVariation,0.0f);

	//add wind impulse to the fluid sim and advect forward
	g_fluid->Impulse( center[0], center[1], center[2], transformedWind.x*windForce, transformedWind.y*windForce, transformedWind.z*windForce );

	float lowerWindAmount = 0.05f;
	float higherWindAmount = 0.15f;
	float windScale = (g_WindForce-lowerWindAmount)/(higherWindAmount-lowerWindAmount);

	//determining wind impulse amount
	//the smaller the wind force the smaller the radius of it we want, and thus the larger the impulsesize (this is a weird parameter that is inversely proportional)
	//empirically, these values look good.
	float gridDiagonal = sqrt( float(sqr(g_fluidGridWidth) + sqr(g_fluidGridHeight) + sqr(g_fluidGridDepth)) );
	float lowerImpulseSize = 0.065*166/gridDiagonal;
	float upperImpulseSize = 0.025*166/gridDiagonal;
	if(windScale<0) windScale=0;
	if(windScale>1) windScale=1;
	float impulseSize = windScale*upperImpulseSize + (1-windScale)*lowerImpulseSize;
	float vorticityConfinement = 0;

	g_fluid->Update(2.0f, false, true, vorticityConfinement,0.9f,0,0,impulseSize,0.4f);
}

HRESULT stepHairSimulationDX11(ID3D11DeviceContext* pd3dContext, int frame)
{
	HRESULT hr = S_OK;

	UINT initCounts = 0;
	pd3dContext->CSSetShader( g_pCSSimulateParticles, NULL, 0 );

	// Per frame cb update
	D3D11_MAPPED_SUBRESOURCE MappedResource;

	V( pd3dContext->Map( g_pcbCSTransforms, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_CS_TRANSFORMS* pTransform = ( CB_CS_TRANSFORMS* )MappedResource.pData;
	memcpy(pTransform, &g_cbTransformsInitialData, sizeof(CB_CS_TRANSFORMS));
	pd3dContext->Unmap( g_pcbCSTransforms, 0 );
	pd3dContext->CSSetConstantBuffers( g_iCBCSTransformBind, 1, &g_pcbCSTransforms );

	V( pd3dContext->Map( g_pcbCSPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
	CB_CS_PER_FRAME* pPerFrame = ( CB_CS_PER_FRAME* )MappedResource.pData;
	pPerFrame->angularStiffness = g_angularSpringStiffnessMultiplier;
    pPerFrame->numConstraintIterations = g_NumTotalConstraintIterations;
    pPerFrame->numCFIterations = g_numCFIterations;
	pPerFrame->gravityStrength = g_GravityStrength;
	pPerFrame->bApplyHorizontalForce = g_applyHForce;
	pPerFrame->bAddGravity = g_addGravity;
	pPerFrame->TimeStep = g_timeStep;
	pPerFrame->RootTransformation = g_RootTransform;
	pPerFrame->currentHairTransformation = g_currentHairTransform;
	pPerFrame->WorldToGrid = g_gridWorldInv;
	D3DXMATRIX additionalTransformMatrix;
    D3DXMatrixIdentity(&additionalTransformMatrix);
    if(g_bApplyAdditionalTransform) 
		additionalTransformMatrix = g_RootTransform; 
	pPerFrame->additionalTransformation = additionalTransformMatrix;
	pPerFrame->integrate = frame<5?false:true;
	pPerFrame->bApplyAdditionalTransform = g_bApplyAdditionalTransform;
	pPerFrame->restoreToDefault = g_RestoreHairStrandsToDefault;
	pPerFrame->blendAxis = g_blendAxis;
	pPerFrame->addAngularForces = g_addAngularForces;
	pd3dContext->Unmap( g_pcbCSPerFrame, 0 );
	pd3dContext->CSSetConstantBuffers( g_iCBPSPerFrameBind, 1, &g_pcbCSPerFrame );

	//set the shader resources that only need to be read in the CS (vs written to)
	ID3D11ShaderResourceView* ppSRV[7] = { g_MasterStrandSimulationIndicesSRV, 
										   g_MasterStrandLinearSpringLengthsSRV, 
										   g_AngularStiffnessUAB_SRV, 
										   g_OriginalMasterStrandRV,
										   g_FluidVelocityRV,
										   g_MasterStrandAngularSpringConstraintsSRV, 
	                                       g_originalVectorsRV};
	pd3dContext->CSSetShaderResources( 0, 7, ppSRV);

	//set the unordered access views - this is what the CS shader will read from and write to
	//we bind two buffers, one containing the particle positions, and the other containing particle positions from the last frame
	ID3D11UnorderedAccessView* ppUAV[3] = { g_MasterStrandUAB_UAV, g_MasterStrandPrevUAB_UAV, g_CoordinateFrameUAB_UAV };
	pd3dContext->CSSetUnorderedAccessViews( 0, 3, ppUAV, &initCounts );

	//set the sampler
	ID3D11SamplerState *states[1] = { g_pLinearClampSampler };
	pd3dContext->CSSetSamplers( 0, 1, states );

	// Run the CS. we run one CTA for each strand. All the threads in a CTA will work co-operatively on a single strand
	pd3dContext->Dispatch( g_NumMasterStrands, 1, 1 );

	// Unbind resources for CS
	ID3D11UnorderedAccessView* ppUAViewNULL[3] = { NULL, NULL, NULL };
	pd3dContext->CSSetUnorderedAccessViews( 0, 3, ppUAViewNULL, &initCounts );
	ID3D11ShaderResourceView* ppSRVNULL[7] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL };
	pd3dContext->CSSetShaderResources( 0, 7, ppSRVNULL );

	if(g_RestoreHairStrandsToDefault)
	{
	    g_RestoreHairStrandsToDefault = false;
	    g_hairReset = true;
	}

	return hr;
}

HRESULT stepHairSimulationDX10(ID3D11DeviceContext* pd3dContext, int frame)
{
	HRESULT hr = S_OK;

	unsigned int stride = sizeof(HairVertex);
	unsigned int offset = 0;

	if(g_RestoreHairStrandsToDefault)
	{
		pd3dContext->IASetInputLayout(g_MasterStrandIL);

		pd3dContext->IASetVertexBuffers(0, 1, &g_MasterStrandVB[g_currentVB], &stride, &offset);
		pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("restoreToDefaultPositions")->Apply(0, pd3dContext);
		pd3dContext->SOSetTargets(1, &g_MasterStrandVB[(g_currentVB+1)%2], &offset);
		pd3dContext->Draw(g_NumMasterStrandControlVertices, 0);
		pd3dContext->SOSetTargets(0, 0, 0);
		g_currentVB = (g_currentVB+1)%2;

		pd3dContext->IASetVertexBuffers(0, 1, &g_MasterStrandVB[g_currentVB], &stride, &offset);
		pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("restoreToDefaultPositions")->Apply(0, pd3dContext);
		pd3dContext->SOSetTargets(1, &g_MasterStrandVB[(g_currentVB+1)%2], &offset);
		pd3dContext->Draw(g_NumMasterStrandControlVertices, 0);
		pd3dContext->SOSetTargets(0, 0, 0);
		g_currentVB = (g_currentVB+1)%2;

		g_RestoreHairStrandsToDefault = false;
		g_hairReset = true;
	}

	if(frame < 5) //this is a way of not appling verlet integration in the beginning, when we are introducing strong distance changes due to collision constraints
		pd3dContext->CopyResource(g_MasterStrandVBOldTemp, g_MasterStrandVB[g_currentVB]);
	else 
		pd3dContext->CopyResource(g_MasterStrandVBOldTemp, g_MasterStrandVBOld);

	pd3dContext->CopyResource(g_MasterStrandVBOld, g_MasterStrandVB[g_currentVB]);    

	ID3D11Buffer* buffers[] = { g_MasterStrandVB[g_currentVB], g_MasterStrandVBOldTemp };
	UINT strides[] = { sizeof(HairVertex), sizeof(HairVertex) };
	UINT offsets[] = { 0, 0 };

	g_Effect->GetVariableByName("g_bApplyAdditionalTransform")->AsScalar()->SetBool(g_bApplyAdditionalTransform);
	g_Effect->GetVariableByName("additionalTransformation")->AsMatrix()->SetMatrix(g_RootTransform);

    RepropagateCoordinateFrames(pd3dContext, g_numCFIterations); 

	//add angular
	if(g_addAngularForces)
	{
		g_Effect->GetVariableByName("g_MasterStrand")->AsShaderResource()->SetResource(g_MasterStrandRV[g_currentVB]);
		g_Effect->GetVariableByName("g_coordinateFrames")->AsShaderResource()->SetResource(g_CoordinateFrameRV[g_currentVBCoordinateFrame]);
		g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("addAngularForces")->Apply(0, pd3dContext);

		//first batch
		g_Effect->GetVariableByName("g_bClearForces")->AsScalar()->SetBool(true);
		g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("addAngularForces")->Apply(0, pd3dContext);
		pd3dContext->IASetInputLayout(g_MasterStrandIL);
		pd3dContext->IASetVertexBuffers(0, 1, &g_ForceVB[g_currentForceVB], &stride, &offset);
		pd3dContext->SOSetTargets(1, &g_ForceVB[(g_currentForceVB+1)%2], &offset);
		pd3dContext->IASetIndexBuffer(g_MasterStrandSimulation1IB, DXGI_FORMAT_R16_UINT, 0);
		pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		pd3dContext->DrawIndexed(g_numIndicesFirst*2.0, 0, 0);
		pd3dContext->SOSetTargets(0, 0, 0);
		g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("addAngularForces")->Apply(0, pd3dContext);
		g_currentForceVB = (g_currentForceVB+1)%2;

		//second batch
		g_Effect->GetVariableByName("g_bClearForces")->AsScalar()->SetBool(false);
		g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("addAngularForces")->Apply(0, pd3dContext);
		pd3dContext->IASetInputLayout(g_MasterStrandIL);
		pd3dContext->IASetVertexBuffers(0, 1, &g_ForceVB[g_currentForceVB], &stride, &offset);
		pd3dContext->SOSetTargets(1, &g_ForceVB[(g_currentForceVB+1)%2], &offset);
		pd3dContext->IASetIndexBuffer(g_MasterStrandSimulation2IB, DXGI_FORMAT_R16_UINT, 0);
		pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
		pd3dContext->DrawIndexed(g_numIndicesSecond*2.0, 0, 0);
		pd3dContext->SOSetTargets(0, 0, 0);
		g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("addAngularForces")->Apply(0, pd3dContext);
		g_currentForceVB = (g_currentForceVB+1)%2;

		//unbind resources
		g_Effect->GetVariableByName("g_MasterStrand")->AsShaderResource()->SetResource(NULL);
		g_Effect->GetVariableByName("g_coordinateFrames")->AsShaderResource()->SetResource(NULL);
		g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("addAngularForces")->Apply(0, pd3dContext);
	}

	//add per vertex forces and integrate using verlet integration: this is not iterated; we only add forces once
	{
		g_Effect->GetVariableByName("g_FluidVelocityTexture")->AsShaderResource()->SetResource(g_FluidVelocityRV);
		g_Effect->GetVariableByName("g_Forces")->AsShaderResource()->SetResource(g_ForceRV[g_currentForceVB]);
		g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("addForcesAndIntegrate")->Apply(0, pd3dContext);

		pd3dContext->IASetInputLayout(g_MasterStrandILAddForces);
		pd3dContext->IASetVertexBuffers(0, 2, buffers, strides, offsets);
		pd3dContext->SOSetTargets(1, &g_MasterStrandVB[(g_currentVB+1)%2], &offset);
		pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		pd3dContext->Draw(g_NumMasterStrandControlVertices, 0);
		pd3dContext->SOSetTargets(0, 0, 0);
		g_currentVB = (g_currentVB+1)%2;

		g_Effect->GetVariableByName("g_FluidVelocityTexture")->AsShaderResource()->SetResource(NULL);
		g_Effect->GetVariableByName("g_Forces")->AsShaderResource()->SetResource(NULL);
		g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("addForcesAndIntegrate")->Apply(0, pd3dContext);
	}

    //iterate over all the constraints multiple times
    {
        for(int totalSimIterations=0;totalSimIterations<g_NumTotalConstraintIterations ;totalSimIterations++)
        {
		    stride = sizeof(HairVertex);

			//satisfy spring constraints
			//linear springs--------------------------------------------------------------------
			if(g_useLinearStiffness)
			{
				strides[0] = sizeof(HairVertex);
				strides[1] = sizeof(float);

				//first batch of linear springs
				buffers[0] = g_MasterStrandVB[g_currentVB];
				buffers[1] = g_MasterStrandLinearSpringLengthsVB;
				pd3dContext->IASetInputLayout(g_MasterStrandSpringConstraintsIL);
				pd3dContext->IASetVertexBuffers(0, 2, buffers, strides, offsets);
				pd3dContext->SOSetTargets(1, &g_MasterStrandVB[(g_currentVB+1)%2], &offset);
				pd3dContext->IASetIndexBuffer(g_MasterStrandSimulation1IB, DXGI_FORMAT_R16_UINT, 0);
				pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
				g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("SatisfySpringConstraints")->Apply(0, pd3dContext);
				pd3dContext->DrawIndexed(g_numIndicesFirst*2.0, 0, 0);
				pd3dContext->SOSetTargets(0, 0, 0);
				g_currentVB = (g_currentVB+1)%2;

				//second batch of linear springs
				buffers[0] = g_MasterStrandVB[g_currentVB];
				buffers[1] = g_MasterStrandLinearSpringLengthsVB;
				pd3dContext->IASetInputLayout(g_MasterStrandSpringConstraintsIL);
				pd3dContext->IASetVertexBuffers(0, 2, buffers, strides, offsets);
				pd3dContext->SOSetTargets(1, &g_MasterStrandVB[(g_currentVB+1)%2], &offset);
				pd3dContext->IASetIndexBuffer(g_MasterStrandSimulation2IB, DXGI_FORMAT_R16_UINT, 0);
				pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
				g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("SatisfySpringConstraints")->Apply(0, pd3dContext);
				pd3dContext->DrawIndexed(g_numIndicesSecond*2.0, 0, 0);
				pd3dContext->SOSetTargets(0, 0, 0);
				g_currentVB = (g_currentVB+1)%2;
			}

			//angular springs--------------------------------------------------------------------
			if(g_useAngularStiffness)
			{
				strides[0] = sizeof(HairVertex);
				strides[1] = sizeof(D3DXVECTOR2);

				//first batch of angular-linear springs
				buffers[0] = g_MasterStrandVB[g_currentVB];
				buffers[1] = g_MasterStrandAngularSpringConstraintsVB;
				pd3dContext->IASetInputLayout(g_MasterStrandAngularSpringConstraintsIL);
				pd3dContext->IASetVertexBuffers(0, 2, buffers, strides, offsets);
				pd3dContext->IASetIndexBuffer(g_MasterStrandSimulationAngular1IB, DXGI_FORMAT_R16_UINT, 0);
				pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ);
				pd3dContext->SOSetTargets(1, &g_MasterStrandVB[(g_currentVB+1)%2], &offset);
				g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("SatisfyAngularSpringConstraints")->Apply(0, pd3dContext);
				pd3dContext->DrawIndexed(g_numIndicesFirstAngular*4.0, 0, 0);
				pd3dContext->SOSetTargets(0, 0, 0);
				g_currentVB = (g_currentVB+1)%2;

				//second batch of angular-linear springs
				buffers[0] = g_MasterStrandVB[g_currentVB];
				buffers[1] = g_MasterStrandAngularSpringConstraintsVB;
				pd3dContext->IASetInputLayout(g_MasterStrandAngularSpringConstraintsIL);
				pd3dContext->IASetVertexBuffers(0, 2, buffers, strides, offsets);
				pd3dContext->SOSetTargets(1, &g_MasterStrandVB[(g_currentVB+1)%2], &offset);
				pd3dContext->IASetIndexBuffer(g_MasterStrandSimulationAngular2IB, DXGI_FORMAT_R16_UINT, 0);
				pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ);
				pd3dContext->SOSetTargets(1, &g_MasterStrandVB[(g_currentVB+1)%2], &offset);
				g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("SatisfyAngularSpringConstraints")->Apply(0, pd3dContext);
				pd3dContext->DrawIndexed(g_numIndicesSecondAngular*4.0, 0, 0);
				pd3dContext->SOSetTargets(0, 0, 0);
				g_currentVB = (g_currentVB+1)%2;
			}

			if( g_collisions)
			{
				
				//collisions-------------------------------------------------------------------------------------
				//satisfy collision constraints
				pd3dContext->IASetInputLayout(g_MasterStrandIL);
				pd3dContext->IASetVertexBuffers(0, 1, &g_MasterStrandVB[g_currentVB], &stride, &offset);
				pd3dContext->SOSetTargets(1, &g_MasterStrandVB[(g_currentVB+1)%2], &offset);
				pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
				g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("SatisfyCollisionConstraints")->Apply(0, pd3dContext);
				pd3dContext->Draw(g_NumMasterStrandControlVertices, 0);
				pd3dContext->SOSetTargets(0, 0, 0);
				g_currentVB = (g_currentVB+1)%2;
				
			}            
		}
	}

    return hr;
}

//--------------------------------------------------------------------------------------
//main loop
//--------------------------------------------------------------------------------------


void CALLBACK OnD3D11FrameRender(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext, double fTime, float fElapsedTime, void* pUserContext)
{
	HRESULT hr;

	static bool s_firstFrame=true;

	//get the depth stencil and render target
	ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
	ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
	pd3dContext->OMSetRenderTargets( 1, &pRTV , pDSV ); 
	D3D11_VIEWPORT viewport;
	UINT nViewports = 1;
	pd3dContext->RSGetViewports(&nViewports, &viewport);


	if (g_Reset)
		resetScene(pd3dDevice, pd3dContext);

	float clearColor[] = { 0.3176f, 0.3176f, 0.3176f, 0.0f  };
#ifdef SUPERSAMPLE
	pd3dContext->ClearRenderTargetView(g_pSceneRTV2X, clearColor);
	pd3dContext->ClearDepthStencilView(g_pSceneDSV2X, D3D11_CLEAR_DEPTH, 1.0, 0);
#else
	pd3dContext->ClearRenderTargetView(pRTV, clearColor);
	pd3dContext->ClearDepthStencilView(pDSV, D3D11_CLEAR_DEPTH, 1.0, 0);

#endif

	if (g_reloadEffect)
		reloadEffect(pd3dDevice, pd3dContext);


	// If the settings dialog is being shown, then
	//  render it instead of rendering the app's scene
	if( g_D3DSettingsDlg.IsActive() )
	{
		g_D3DSettingsDlg.OnRender( fElapsedTime );
		return;
	}

	if(g_playTransforms)
		PlayTransforms();

	pd3dContext->OMSetRenderTargets( 0, NULL , NULL ); 

	//animate the mesh------------------------------------------------------------------------

	D3DXMATRIX HairTransform;
	D3DXMatrixIdentity(&HairTransform);

	//mouse transformations for scalp and hair
	if(g_firstFrame)
	{
		D3DXMatrixIdentity(&g_InitialTotalTransform);
		D3DXMatrixIdentity(&g_InvInitialTotalTransform); 
		D3DXMatrixIdentity(&g_InvInitialRotationScale);  
	}
	if(g_UseTransformations)
	{
		g_TotalTransform *= g_Transform;
		g_UseTransformations = false;
	}
	else
		D3DXMatrixIdentity(&g_Transform);

	g_currentHairTransform = g_TotalTransform;	

	//-----------------------------------------------------------------------------------
	// View and projection matrices
	D3DXMATRIX projection;
	D3DXMATRIX view;
	D3DXVECTOR3 eyePosition;
	// Get the projection & view matrix from the camera class
	projection = *g_Camera.GetProjMatrix();
	if(g_useSkinnedCam)
	{
		view = g_ExternalCameraView;
		eyePosition = g_ExternalCameraEye;
	}
	else
	{
		view = *g_Camera.GetViewMatrix();
		eyePosition = *g_Camera.GetEyePt();
	}

	// View projection matrix
	D3DXMATRIX viewProjection;
	viewProjection = view * projection;

	//mesh world matrix; setting to identity currently
	D3DXMATRIX meshWorld;
	D3DXMatrixIdentity(&meshWorld);

	D3DXMATRIX transformViewProjection;


	D3DXVECTOR3 old_center = D3DXVECTOR3(0,0,0);
	D3DXVec3TransformCoord(&g_Center, &old_center, &g_TotalTransform);
	D3DXMATRIX currentHairTransformInverse;
	D3DXMatrixInverse(&currentHairTransformInverse, NULL, &g_currentHairTransform);
	V(g_Effect->GetVariableByName("currentHairTransformation")->AsMatrix()->SetMatrix(g_currentHairTransform));
	V(g_Effect->GetVariableByName("currentHairTransformationInverse")->AsMatrix()->SetMatrix(currentHairTransformInverse));
	D3DXMATRIX WorldViewProjection = HairTransform * viewProjection; 
	transformViewProjection = g_TotalTransform*viewProjection;
	V(g_Effect->GetVariableByName("RootTransformation")->AsMatrix()->SetMatrix(g_RootTransform));
	V(g_Effect->GetVariableByName("HairToWorldTransform")->AsMatrix()->SetMatrix(g_InitialTotalTransform));
	V(g_Effect->GetVariableByName("TotalTransformation")->AsMatrix()->SetMatrix(g_TotalTransform));
	D3DXVECTOR3 transformedEyePosition;//transform the eye position from world space into the hair space
	D3DXVec3TransformCoord(&transformedEyePosition, &eyePosition, &g_InvInitialTotalTransform);

	D3DXVECTOR3 lightVector = g_HairShadows.GetLightWorldDir();

	D3DXVECTOR3 transformedLight;
	D3DXVec3TransformCoord(&transformedLight,&lightVector,&g_InvInitialRotationScale);
	D3DXVec3Normalize(&transformedLight,&transformedLight);
	V(g_Effect->GetVariableByName("vLightDirObjectSpace")->AsVector()->SetFloatVector(transformedLight));

	//set all the collision transform matrices
	ExtractAndSetCollisionImplicitMatrices();



	// Effect variables
	V(g_Effect->GetVariableByName("ViewProjection")->AsMatrix()->SetMatrix(viewProjection));
	V(g_Effect->GetVariableByName("TransformedEyePosition")->AsVector()->SetFloatVector(transformedEyePosition));


	D3D11_VIEWPORT collisionsViewport;
	collisionsViewport.TopLeftX = 0;
	collisionsViewport.TopLeftY = 0;
	collisionsViewport.MinDepth = 0;
	collisionsViewport.MaxDepth = 1;
	collisionsViewport.Width = MAX_INTERPOLATED_HAIR_PER_WISP;  
	collisionsViewport.Height = g_NumWisps; 


	g_Effect->GetVariableByName("TimeStep")->AsScalar()->SetFloat(g_timeStep);


	//------------------------------------------------------------------------------------
	//render the wind directional control, and update the value of the wind vector
	//------------------------------------------------------------------------------------

	V(g_Effect->GetVariableByName("ViewProjection")->AsMatrix()->SetMatrix(viewProjection));

#ifdef SUPERSAMPLE
	pd3dContext->RSSetViewports(1,&g_superSampleViewport);
	pd3dContext->OMSetRenderTargets( 1, &g_pSceneRTV2X , g_pSceneDSV2X ); 
#else
	pd3dContext->OMSetRenderTargets(  1, &pRTV , pDSV ); 
#endif

	g_windVector = g_windDirectionWidget.GetLightDirection();
	D3DXVec3Normalize(&g_windVector,&g_windVector);
	D3DXVECTOR3 windNormVector = g_windVector;
	g_windVector *= g_WindForce;
	V(g_Effect->GetVariableByName("windForce")->AsVector()->SetFloatVector((float*)&g_windVector));

	D3DXVECTOR3 vWindForce = g_windVector * g_windDirectionWidget.GetRadius()*10;
	float greenColor[4] = {0,1,0,1};
	g_ArrowColorShaderVariable->SetFloatVector(greenColor);
	RenderArrow(pd3dDevice, pd3dContext, vWindForce);

	//render arrow for the light
	float whiteColor[4] = {1,1,1,1};
	g_ArrowColorShaderVariable->SetFloatVector(whiteColor);
	RenderArrow(pd3dDevice, pd3dContext, g_HairShadows.GetLightWorldDir());

	//-------------------------------------------------------------------------------------------
	// Render the mesh, just for setting depth to enable better zculling for the hair later
	// the actual face mesh is rendered later on because we would possibly like to cast shadows from the simulated hair onto it
	//-------------------------------------------------------------------------------------------

	if(g_RenderScalp && g_RenderScene)
	{ 

		V(g_Effect->GetVariableByName("ViewProjection")->AsMatrix()->SetMatrix(transformViewProjection));
		pd3dContext->IASetInputLayout(g_RenderMeshIL);
		g_UseScalpTextureShaderVariable->SetBool(false);
		g_BaseColorShaderVariable->SetFloatVector(skinColor);
		g_Effect->GetTechniqueByName("RenderMesh")->GetPassByName("RenderMeshDepthPass")->Apply(0, pd3dContext);
		g_FaceMesh->Render(pd3dContext);
		g_Scalp->Render(pd3dContext);
	}


	pd3dContext->OMSetRenderTargets( 0, NULL , NULL );
	V(g_Effect->GetVariableByName("ViewProjection")->AsMatrix()->SetMatrix(viewProjection));


	//------------------------------------------------------------------------------------
	//render the collision spheres for debugging
	if(g_RenderCollsionImplicits)
		for(int i=0;i<g_numSpheres;i++)
		{

	#ifdef SUPERSAMPLE
			pd3dContext->RSSetViewports(1,&g_superSampleViewport);
			pd3dContext->OMSetRenderTargets( 1, &g_pSceneRTV2X , g_pSceneDSV2X ); 
	#else
			pd3dContext->OMSetRenderTargets(  1, &pRTV , pDSV ); 
	#endif

			//this technique uses the sphere transforms, the hairToWorld matrix, and the viewProjection matrix
			g_Effect->GetVariableByName("currentCollisionSphere")->AsScalar()->SetInt(i); 

			g_Effect->GetTechniqueByName("RenderCollisionObjects")->GetPassByIndex(0)->Apply(0, pd3dContext);
			g_Sphere.Render(pd3dContext);
		}

	//---------------------------------------------------------------------------------------
	// call simulation code
    //---------------------------------------------------------------------------------------

	//World matrix for grid
	D3DXMATRIX gridScale;
	float diameter = (g_maxHairLengthHairSpace+g_scalpMaxRadiusHairSpace)*2.0;
	D3DXMatrixScaling(&gridScale, diameter, diameter, diameter ); 
	D3DXMATRIX gridWorld = gridScale*g_currentHairTransform;

	//World View projection matrix for grid
	D3DXMATRIX worldViewProjectionGrid;
	worldViewProjectionGrid = gridWorld * viewProjection;

	D3DXMatrixInverse(&g_gridWorldInv, NULL, &gridWorld);
	D3DXMATRIX objToVolumeXForm = g_gridWorldInv;

	V(g_Effect->GetVariableByName("WorldToGrid")->AsMatrix()->SetMatrix(g_gridWorldInv));
	V(g_Effect->GetVariableByName("GridToWorld")->AsMatrix()->SetMatrix(gridWorld));

	unsigned int stride = sizeof(HairVertex);
	unsigned int offset = 0;

	//voxelize the collision obstacles into the grid
	if(g_simulate || g_firstMovedFrame)
	    g_HairGrid->voxelizeObstacles();

    if(g_simulate)
	    StepHairSimulation(pd3dContext, pd3dDevice);

	//------------------------------------------------------------------------------------------------------------------

	//add the hair to the density grid if we need it for display, or for the wind forces in the next time step
	if(g_renderDensityOnHair || g_applyHForce)
	{
		//note: add in the g_MasterStrandSB option here, or get rid of voxelization all together..
		g_Effect->GetVariableByName("g_MasterStrand")->AsShaderResource()->SetResource(g_MasterStrandRV[g_currentVB]);
		g_HairGrid->AddHairDensity(objToVolumeXForm,&renderInstancedInterpolatedHairForGrid);
	}
	if(g_renderDensityOnHair)
	{
		g_HairGrid->demultiplexTexture();
	}


	pd3dContext->RSSetViewports(1,&viewport);
	V(g_Effect->GetVariableByName("TransformedEyePosition")->AsVector()->SetFloatVector(transformedEyePosition));

	//Tessellate master strands--------------------------------------------------------------------------------

	if( (g_simulate || g_firstMovedFrame) && g_InterpolateModel != NO_HAIR)
	{
		stride = sizeof(HairAdjacencyVertex);
		if(g_useCS)
		{
		    g_Effect->GetVariableByName("g_MasterStrandSB")->AsShaderResource()->SetResource(g_MasterStrandUAB_SRV);                   
		    g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("TessellateSB")->Apply(0, pd3dContext);
		}
		else
		{
			g_Effect->GetVariableByName("g_MasterStrand")->AsShaderResource()->SetResource(g_MasterStrandRV[g_currentVB]);                   
 			g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("Tessellate")->Apply(0, pd3dContext);
		}

		pd3dContext->IASetInputLayout(g_MasterStrandTesselatedInputIL);
		pd3dContext->IASetVertexBuffers(0, 1, &g_TessellatedMasterStrandVBInitial, &stride, &offset);
		ID3D11Buffer* SO_buffers[] = { g_TessellatedMasterStrandVB, g_TessellatedTangentsVB };
		UINT offsets[] = { 0, 0 };
		pd3dContext->SOSetTargets(2, SO_buffers, offsets);
		pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		pd3dContext->Draw(g_NumTessellatedMasterStrandVertices, 0); 

		pd3dContext->SOSetTargets(0, 0, 0);
		if(g_useCS)
		{
		    g_Effect->GetVariableByName("g_MasterStrandSB")->AsShaderResource()->SetResource(NULL);                   
		    g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("TessellateSB")->Apply(0, pd3dContext);
		}
		else
		{
			g_Effect->GetVariableByName("g_MasterStrand")->AsShaderResource()->SetResource(NULL);                   
 			g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("Tessellate")->Apply(0, pd3dContext);
		}

	}	
	//tessellate the coordinate frames--------------------------------------------------------------------------------
	if( (g_simulate || g_firstMovedFrame) && g_InterpolateModel != NO_HAIR)
	{
		stride = sizeof(HairAdjacencyVertex);
		g_Effect->GetVariableByName("g_TessellatedMasterStrand")->AsShaderResource()->SetResource(g_TessellatedMasterStrandRV);
       
		if(g_useCS)
		{
		    g_Effect->GetVariableByName("g_coordinateFramesSB")->AsShaderResource()->SetResource(g_CoordinateFrameUAB_SRV);
		    g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("TessellateCoordinateFramesSB")->Apply(0, pd3dContext);
		}
		else
		{
		    g_Effect->GetVariableByName("g_coordinateFrames")->AsShaderResource()->SetResource(g_CoordinateFrameRV[g_currentVBCoordinateFrame]);
		    g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("TessellateCoordinateFrames")->Apply(0, pd3dContext);
		}

		pd3dContext->IASetInputLayout(g_MasterStrandTesselatedInputIL);
		pd3dContext->IASetVertexBuffers(0, 1, &g_TessellatedMasterStrandVBInitial, &stride, &offset);
		pd3dContext->SOSetTargets(1, &g_TessellatedCoordinateFrameVB, &offset);
		pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		pd3dContext->Draw(g_NumTessellatedMasterStrandVertices, 0);

		pd3dContext->SOSetTargets(0, 0, 0);
		g_Effect->GetVariableByName("g_TessellatedMasterStrand")->AsShaderResource()->SetResource(NULL);
		if(g_useCS)
		{
		    g_Effect->GetVariableByName("g_coordinateFramesSB")->AsShaderResource()->SetResource(NULL);
		    g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("TessellateCoordinateFramesSB")->Apply(0, pd3dContext);
		}
		else
		{
		    g_Effect->GetVariableByName("g_coordinateFrames")->AsShaderResource()->SetResource(NULL);
		    g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("TessellateCoordinateFrames")->Apply(0, pd3dContext);
		}
	}
	//tesssellate the hair lengths--------------------------------------------------------------------------------------
	if( (g_simulate || g_firstMovedFrame) && g_InterpolateModel != NO_HAIR)
	{
		stride = sizeof(HairAdjacencyVertex);

		pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		pd3dContext->IASetInputLayout(g_MasterStrandTesselatedInputIL);
		pd3dContext->IASetVertexBuffers(0, 1, &g_TessellatedMasterStrandVBInitial, &stride, &offset);
		pd3dContext->SOSetTargets(1, &g_TessellatedMasterStrandLengthToRootVB, &offset);
		g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("TessellateLengths")->Apply(0, pd3dContext);
		pd3dContext->Draw(g_NumTessellatedMasterStrandVertices, 0);
		pd3dContext->SOSetTargets(0, 0, 0);		
	}

	//-------------------------------------------------------------------------------------------------------------------
	//find which of the interpolated strands in the multistrand interpolation are colliding with obstacles
	//render to a texture. see Notes 1 above for details.

	if(g_firstMovedFrame || (g_simulate && (g_InterpolateModel==MULTI_HYBRID || g_InterpolateModel==HYBRID)))
	{
		//set the viewport and render target
		pd3dContext->RSSetViewports(1,&collisionsViewport);
		float clearValue[4] = {1000, 1000, 1000, 1000 };
		pd3dContext->ClearRenderTargetView( g_CollisionsRTV, clearValue );
		pd3dContext->OMSetRenderTargets( 1, &g_CollisionsRTV , NULL ); 

		RenderInterpolatedHair(pd3dDevice, pd3dContext, INSTANCED_INTERPOLATED_COLLISION, MULTISTRAND, g_totalMStrands, g_totalSStrands);

		//restore the viewport and render targets
		pd3dContext->OMSetRenderTargets( 0, NULL, NULL );
		pd3dContext->RSSetViewports(1,&viewport);
	}

	//-------------------------------------------------------------------------------------------------
	//streamout all the calculated attributes to buffers so that we dont have to keep calculating them

	if(g_useSOInterpolatedAttributes && (g_firstMovedFrame || g_simulate))
	{
		pd3dContext->IASetInputLayout(g_MasterStrandIL);
		if(g_InterpolateModel==HYBRID )
		{
			RenderInterpolatedHair(pd3dDevice, pd3dContext, SOATTRIBUTES, MULTI_HYBRID, g_totalMStrands, g_totalSStrands); 
			RenderInterpolatedHair(pd3dDevice, pd3dContext, SOATTRIBUTES, SINGLESTRAND, g_totalMStrands, g_totalSStrands); 
		}
		else
		{
			RenderInterpolatedHair(pd3dDevice, pd3dContext, SOATTRIBUTES,g_InterpolateModel,g_totalMStrands, g_totalSStrands); 
		}
	}

	//render hair to a  shadow map--------------------------------------------------------------------------

	if( g_renderShadows )
	{
		V(g_Effect->GetVariableByName("ViewProjection")->AsMatrix()->SetMatrix(g_HairShadows.GetLightWorldViewProj()));
		V(g_Effect->GetVariableByName("TransformedEyePosition")->AsVector()->SetFloatVector(g_HairShadows.GetLightCenterWorld()));
		pd3dContext->IASetInputLayout(g_MasterStrandIL);
		g_HairShadows.BeginShadowMapRendering(pd3dDevice, pd3dContext);
		{
			if(g_InterpolateModel==HYBRID)
			{
				RenderInterpolatedHair(pd3dDevice, pd3dContext, INSTANCED_DEPTH, MULTI_HYBRID, g_totalMStrands, g_totalSStrands); 
				RenderInterpolatedHair(pd3dDevice, pd3dContext, INSTANCED_DEPTH, SINGLESTRAND, g_totalMStrands, g_totalSStrands); 
			}
			else
				RenderInterpolatedHair(pd3dDevice, pd3dContext, INSTANCED_DEPTH, g_InterpolateModel,g_totalMStrands, g_totalSStrands); 
		}
		if(g_RenderScalp  && g_RenderScene) 
		{
			// unset hull and domain shaders
			g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("AllNulls")->Apply(0, pd3dContext);
			pd3dContext->IASetInputLayout(g_RenderMeshIL);
			g_Effect->GetTechniqueByName("RenderMeshDepth")->GetPassByIndex(0)->Apply(0, pd3dContext);
			g_FaceMesh->Render(pd3dContext);
		}
		g_HairShadows.EndShadowMapRendering(pd3dContext);

	}//end if(g_renderShadows)

	V(g_Effect->GetVariableByName("ViewProjection")->AsMatrix()->SetMatrix(WorldViewProjection));
	V(g_Effect->GetVariableByName("TransformedEyePosition")->AsVector()->SetFloatVector(transformedEyePosition));
	pd3dContext->RSSetViewports( 1, &viewport );

#ifdef SUPERSAMPLE

	//set bigger Render Target and Viewport
	pd3dContext->RSSetViewports(1,&g_superSampleViewport);
	pd3dContext->OMSetRenderTargets( 1, &g_pSceneRTV2X , g_pSceneDSV2X ); 
#else
	pd3dContext->OMSetRenderTargets(  1, &pRTV , pDSV ); 
#endif

	if (g_VisShadowMap)
	{
		g_HairShadows.VisualizeShadowMap(pd3dDevice, pd3dContext);
		return;
	}

	V(g_Effect->GetVariableByName("ViewProjection")->AsMatrix()->SetMatrix(WorldViewProjection));
	V(g_Effect->GetVariableByName("TransformedEyePosition")->AsVector()->SetFloatVector(transformedEyePosition));


#ifdef SUPERSAMPLE

	//set bigger Render Target and Viewport
	pd3dContext->RSSetViewports(1,&g_superSampleViewport);
	pd3dContext->OMSetRenderTargets( 1, &g_pSceneRTV2X , g_pSceneDSV2X ); 
#else
	pd3dContext->OMSetRenderTargets(  1, &pRTV , pDSV ); 
#endif

	// unset hull and domain shaders
	g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("AllNulls")->Apply(0, pd3dContext);
	pd3dContext->IASetInputLayout(g_RenderMeshIL);
	g_Effect->GetTechniqueByName("RenderMeshDepth")->GetPassByIndex(0)->Apply(0, pd3dContext);

	//render the floor--------------------------------------------------------------------------------------

	if(g_RenderFloor && g_RenderScene)
	{
		D3DXVECTOR3 lightVector = g_HairShadows.GetLightWorldDir();
		D3DXVECTOR3 transformedLight;
		D3DXVec3TransformCoord(&transformedLight,&lightVector,&g_InvInitialRotationScale);
		D3DXVec3Normalize(&transformedLight,&transformedLight);
		D3DXVECTOR3 lightPos = g_Center - lightVector;

		V(g_Effect->GetVariableByName("ViewProjection")->AsMatrix()->SetMatrix(viewProjection));
		V(g_Effect->GetVariableByName("vLightPos")->AsVector()->SetFloatVector(lightPos));
		DrawPlane(pd3dContext);
		V(g_Effect->GetVariableByName("ViewProjection")->AsMatrix()->SetMatrix(transformViewProjection));
	}

	//render the face/scalp, possibly with shadows-----------------------------------------------------------

	if(g_RenderScalp && g_RenderScene)
	{
		
		V(g_Effect->GetVariableByName("ViewProjection")->AsMatrix()->SetMatrix(transformViewProjection));
		pd3dContext->IASetInputLayout(g_RenderMeshIL);
		g_UseScalpTextureShaderVariable->SetBool(false);
		g_BaseColorShaderVariable->SetFloatVector(skinColor);
		g_HairShadows.SetHairShadowTexture();
		g_Effect->GetTechniqueByName("RenderMesh")->GetPassByName("RenderWithShadows")->Apply(0, pd3dContext);
		g_FaceMesh->Render(pd3dContext);
		g_HairShadows.UnSetHairShadowTexture();
		g_Effect->GetTechniqueByName("RenderMesh")->GetPassByName("RenderWithShadows")->Apply(0, pd3dContext);
		
		V(g_Effect->GetVariableByName("ViewProjection")->AsMatrix()->SetMatrix(transformViewProjection));
		pd3dContext->IASetInputLayout(g_RenderMeshIL);
		g_UseScalpTextureShaderVariable->SetBool(true);
		g_BaseColorShaderVariable->SetFloatVector(hairShadingParams[g_currHairShadingParams].m_baseColor);
		g_HairShadows.SetHairShadowTexture();
		g_Effect->GetTechniqueByName("RenderMesh")->GetPassByName("RenderWithoutShadows")->Apply(0, pd3dContext);
		g_Scalp->Render(pd3dContext);
		g_HairShadows.UnSetHairShadowTexture();
		g_Effect->GetTechniqueByName("RenderMesh")->GetPassByName("RenderWithoutShadows")->Apply(0, pd3dContext);
	}

	V(g_Effect->GetVariableByName("ViewProjection")->AsMatrix()->SetMatrix(WorldViewProjection));
	V(g_Effect->GetVariableByName("TransformedEyePosition")->AsVector()->SetFloatVector(transformedEyePosition));


	//render the hair in the scene------------------------------------------------------------------------
	pd3dContext->IASetInputLayout(g_MasterStrandIL);
	g_BaseColorShaderVariable->SetFloatVector(hairShadingParams[g_currHairShadingParams].m_baseColor);
	g_SpecColorShaderVariable->SetFloatVector(hairShadingParams[g_currHairShadingParams].m_specColor);

	if(g_renderDensityOnHair)
	{
		if(g_InterpolateModel==HYBRID)
		{
			RenderInterpolatedHair(pd3dDevice, pd3dContext, INSTANCED_DENSITY, MULTI_HYBRID, g_totalMStrands, g_totalSStrands); 
			RenderInterpolatedHair(pd3dDevice, pd3dContext, INSTANCED_DENSITY, SINGLESTRAND, g_totalMStrands, g_totalSStrands); 
		}
		else
			RenderInterpolatedHair(pd3dDevice, pd3dContext, INSTANCED_DENSITY,g_InterpolateModel,g_totalMStrands, g_totalSStrands); 
	}
	else
	{	
		if(g_InterpolateModel==HYBRID)
		{
			RenderInterpolatedHair(pd3dDevice, pd3dContext, INSTANCED_NORMAL_HAIR, MULTI_HYBRID,g_totalMStrands, g_totalSStrands); 
			RenderInterpolatedHair(pd3dDevice, pd3dContext, INSTANCED_NORMAL_HAIR,SINGLESTRAND,g_totalMStrands, g_totalSStrands);
		}
		else
		{
			RenderInterpolatedHair(pd3dDevice, pd3dContext, INSTANCED_NORMAL_HAIR,g_InterpolateModel,g_totalMStrands, g_totalSStrands); 
		}

	}



	// apply all changes
	g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateAndRenderM_HardwareTess")->Apply(0, pd3dContext);
	// unset hull and domain shaders
	g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("AllNulls")->Apply(0, pd3dContext);

	// unset shadow map
	g_Effect->GetVariableByName("tShadowMap")->AsShaderResource()->SetResource(NULL);

	//--------------------------------------------------------------------------------------------------
#ifdef SUPERSAMPLE
	//resolve the MS surface and set it to the fx file-------------------------------------------------------
	pd3dContext->ResolveSubresource(g_pSceneColorBuffer2X, 0, g_pSceneColorBuffer2XMS, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
	g_SceneColor2XShaderResourceVariable->SetResource(g_pSceneColorBufferShaderResourceView);

	//draw a full screen quad to the backbuffer--------------------------------------------------------------
	pd3dContext->OMSetRenderTargets( 1, &pRTV , pDSV ); 
	pd3dContext->RSSetViewports(1,&viewport);

	g_Effect->GetVariableByName("SStextureWidth")->AsScalar()->SetFloat(g_Width*g_SSFACTOR);
	g_Effect->GetVariableByName("SStextureHeight")->AsScalar()->SetFloat(g_Height*g_SSFACTOR);
	g_Effect->GetTechniqueByName("DrawTexture")->GetPassByName( "ResolveSuperSample" )->Apply(0, pd3dContext);       
	RenderScreenQuad(pd3dDevice,pd3dContext,g_ScreenQuadBuffer);	   
	g_SceneColor2XShaderResourceVariable->SetResource(NULL);	   
	g_Effect->GetTechniqueByName("DrawTexture")->GetPassByName( "ResolveSuperSample" )->Apply(0, pd3dContext);
#endif

	//--------------------------------------------------------------------------------------------------
	//visualize the coordinate frames
	if(g_bVisualizeCFs)
		VisualizeCoordinateFrames(pd3dDevice, pd3dContext, g_bVisualizeTessellatedCFs);

	//--------------------------------------------------------------------------------------------------

	pd3dContext->OMSetRenderTargets( 1, &pRTV , pDSV ); 
	pd3dContext->RSSetViewports(1,&viewport);


	g_firstFrame = false;
	g_firstMovedFrame = false;
	pd3dContext->OMSetRenderTargets( 1, &pRTV , pDSV ); 
	pd3dContext->RSSetViewports(1,&viewport);

	if(g_renderGUI)
	{
		RenderText();
		g_SampleUI.OnRender( fElapsedTime ); 
		g_HUD.OnRender( fElapsedTime );
	}

    if(g_simulate)
		g_RootTransform = g_Transform;
	else 
		g_RootTransform = g_RootTransform * g_Transform;
    
	g_bApplyAdditionalTransform = false;
}


void reloadEffect(ID3D11Device* pd3dDevice, ID3D11DeviceContext *pd3dContext)
{
	g_reloadEffect = false;
	SAFE_RELEASE(g_Effect);
	InitEffect(pd3dDevice, pd3dContext);
	g_HairGrid->ReloadShader(g_Effect,g_pEffectPath);

	//initialize the fluid again
	SAFE_DELETE(g_fluid);
	SAFE_RELEASE(g_pVelTexture3D);
	SAFE_RELEASE(g_FluidVelocityRV);
	InitializeFluidState(pd3dDevice, pd3dContext);
}

HRESULT resetScene(ID3D11Device* pd3dDevice, ID3D11DeviceContext *pd3dContext)
{
	HRESULT hr = S_OK;

	g_firstMovedFrame = true;
	g_firstFrame = true;
	g_hairReset = true;
	D3DXMatrixIdentity(&g_Transform);
	D3DXMatrixIdentity(&g_TotalTransform);

	g_playTransforms = false;
    g_SampleUI.GetCheckBox( IDC_PLAY_TRANSFORMS )->SetChecked(g_playTransforms);
	V(g_Effect->GetVariableByName("TotalTransformation")->AsMatrix()->SetMatrix(g_TotalTransform));

#ifdef SUPERSAMPLE
    ReinitSuperSampleBuffers(pd3dDevice);
#endif

	LoadMayaHair(g_loadDirectory, g_useShortHair );
	LoadCollisionObstacles(pd3dDevice);
	CreateBuffers(pd3dDevice, pd3dContext);
	g_Effect->GetVariableByName("g_TessellatedMasterStrandLengthMax")->AsScalar()->SetInt(g_TessellatedMasterStrandLengthMax);
	g_Effect->GetVariableByName("g_NumTotalWisps")->AsScalar()->SetInt(g_NumWisps);
	g_Reset = false;

	return hr;
}



void PlayTransforms()
{
    if(g_loopTransforms && g_transformsFileIn.eof())
	{
		g_transformsFileIn.close();
		g_transformsFileIn.clear();
		TCHAR RecordedTransformsFile[MAX_PATH];
		NVUTFindDXSDKMediaFileCchT( RecordedTransformsFile, MAX_PATH,g_tstr_animations[g_animationIndex]);
		g_transformsFileIn.open (RecordedTransformsFile);
	}

	if(	g_transformsFileIn && !g_transformsFileIn.eof() )
	{
		//char c;
		//g_transformsFileIn>>c;
		float v00,v01,v02,v03,
			v10,v11,v12,v13,
			v20,v21,v22,v23,
			v30,v31,v32,v33;
		g_transformsFileIn>>v00>>v01>>v02>>v03
			>>v10>>v11>>v12>>v13
			>>v20>>v21>>v22>>v23
			>>v30>>v31>>v32>>v33;
		g_Transform = D3DXMATRIX(v00,v01,v02,v03,
			v10,v11,v12,v13,
			v20,v21,v22,v23,
			v30,v31,v32,v33);
		g_UseTransformations = true;
	}
	else
	{
		g_transformsFileIn.close();
		g_transformsFileIn.clear();
		g_playTransforms = false;
		g_UseTransformations = false;
		g_SampleUI.GetCheckBox( IDC_PLAY_TRANSFORMS )->SetChecked(g_playTransforms);

		D3DXMatrixIdentity(&g_TotalTransform); //clear previous transformations before playing
		g_RestoreHairStrandsToDefault = true;

		TCHAR RecordedTransformsFile[MAX_PATH];
		NVUTFindDXSDKMediaFileCchT( RecordedTransformsFile, MAX_PATH,g_tstr_animations[g_animationIndex]);
		g_transformsFileIn.open (RecordedTransformsFile);
	}
}


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    HRESULT hr;
    V_RETURN(DXUTSetMediaSearchPath(L"..\\Source\\Hair"));
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	// DXUT will create and use the best device (either D3D9 or D3D11) 
	// that is available on the system depending on which D3D callbacks are set below

	// Set DXUT callbacks
	DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);
	DXUTSetIsInGammaCorrectMode(false);
	DXUTSetCallbackMsgProc(MsgProc);
	DXUTSetCallbackKeyboard(OnKeyboard);
	DXUTSetCallbackFrameMove(OnFrameMove);
	DXUTSetCallbackD3D11DeviceAcceptable(IsD3D11DeviceAcceptable);
	DXUTSetCallbackD3D11DeviceCreated(OnD3D11CreateDevice);
	DXUTSetCallbackD3D11SwapChainResized(OnD3D11SwapChainResized);
	DXUTSetCallbackD3D11FrameRender(OnD3D11FrameRender);
	DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11SwapChainReleasing);
	DXUTSetCallbackD3D11DeviceDestroyed(OnD3D11DestroyDevice);

	InitApp();
	DXUTInit(true, true, NULL); // Parse the command line, show msgboxes on error, no extra command line params
	DXUTSetCursorSettings(true, true); // Show the cursor and clip it when in full screen
	DXUTCreateWindow(L"Hair", 0, 0, 0, 0, 0);
	DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, 1024, 768);
	DXUTMainLoop(); // Enter into the DXUT render loop

	return DXUTGetExitCode();
}

//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------

HRESULT InitApp()
{
	HRESULT hr = S_OK;

	WCHAR sz[100];

	g_D3DSettingsDlg.Init(&g_DialogResourceManager);
	g_HUD.Init(&g_DialogResourceManager);
	g_SampleUI.Init(&g_DialogResourceManager);

	g_HUD.SetCallback(OnGUIEvent); int iY = 10; 
	g_HUD.AddButton(IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 145, 22);
	g_HUD.AddButton(IDC_TOGGLEREF, L"Toggle REF", 35, iY += 24, 125, 22);
	g_HUD.AddButton(IDC_CHANGEDEVICE, L"Change device", 35, iY += 24, 125, 22);
	g_HUD.AddButton(IDC_RESET_SIM, L"Reset", 35, iY+=24, 125, 22);
	g_SampleUI.SetCallback(OnGUIEvent);
	iY -= 140;

	g_windDirectionWidget.SetLightDirection( g_windVector );
	g_windDirectionWidget.SetRadius( 2.0f );


	g_SampleUI.AddCheckBox( IDC_PLAY_TRANSFORMS, TEXT("Play Animation"), 20, iY += 24, 125, 22, g_playTransforms );
	//g_SampleUI.AddCheckBox( IDC_REC_TRANSFORMS, TEXT("record animation"), 20, iY += 24, 125, 22, g_recordTransforms );
	g_SampleUI.AddCheckBox( IDC_LOOP_ANIMATION, TEXT("Loop Animation"), 20, iY += 24, 125, 22, g_loopTransforms );

	//hair rendering variables
//	iY += 5;
	g_SampleUI.AddComboBox( IDC_COMBO_SHADING, 0, iY += 24, 178, 22 );
	for(int i=0; g_tstr_shadingOptionLabels[i] != NULL; i++)
		g_SampleUI.GetComboBox( IDC_COMBO_SHADING )->AddItem( g_tstr_shadingOptionLabels[i], NULL );
	g_SampleUI.GetComboBox( IDC_COMBO_SHADING )->SetSelectedByIndex( g_currHairShadingParams );
//	iY += 5;
	g_SampleUI.AddCheckBox( IDC_USE_SHORT_HAIR, TEXT("Short Hair"), 20, iY += 24, 125, 22, g_useShortHair );
	g_SampleUI.AddCheckBox( IDC_CURLYHAIR, TEXT("Curly Hair"), 20, iY += 24, 125, 22, g_bCurlyHair );
	g_SampleUI.AddCheckBox( IDC_USE_SHADOWS, TEXT("Shadows"), 20, iY += 24, 125, 22, g_renderShadows);
	g_SampleUI.AddCheckBox( IDC_STRANDB, TEXT("Render M strands"), 20, iY += 24, 125, 22, g_renderMStrands);
	g_SampleUI.AddCheckBox( IDC_STRANDC, TEXT("Render S strands"), 20, iY += 24, 125, 22, g_renderSStrands);
	g_SampleUI.AddCheckBox( IDC_USEDX11, TEXT("HW Tessellation"), 20, iY += 24, 125, 22, g_useDX11);
	g_SampleUI.AddCheckBox( IDC_DYNAMIC_LOD, TEXT("Dynamic LOD"), 35, iY += 24, 125, 22, g_bDynamicLOD);
    g_SampleUI.AddSlider( IDC_NHAIRSLOD_SCALE, 50, iY += 20, 100, 22, 0, 100, g_fNumHairsLOD * 100);
	g_SampleUI.GetSlider( IDC_NHAIRSLOD_SCALE )->SetEnabled(false);
	g_SampleUI.AddStatic( IDC_NHAIRSWIDTHLOD_STATIC, TEXT("Hair Width"), 55, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_NHAIRSWIDTHLOD_SCALE, 50, iY += 20, 100, 22, 1, 10, g_fWidthHairsLOD);
	g_SampleUI.GetSlider( IDC_NHAIRSWIDTHLOD_SCALE )->SetEnabled(false);
	g_SampleUI.AddStatic( IDC_LODSCALEFACTOR_STATIC, TEXT("LOD Rate"), 55, iY += 24, 125, 22 );
    g_SampleUI.AddSlider( IDC_LODSCALEFACTOR_SCALE, 50, iY += 20, 100, 22, 1, 400, g_LODScalingFactor*100);
	g_SampleUI.GetSlider( IDC_LODSCALEFACTOR_SCALE )->SetEnabled(true);
	
	//simulation variables
	g_SampleUI.AddCheckBox( IDC_ADDFORCES, TEXT("Add Wind Force"), 20, iY += 24, 125, 22, g_applyHForce );
	StringCchPrintf( sz, 100, L"Wind Strength: %0.2f", g_WindForce ); 
	g_SampleUI.AddStatic( IDC_WINDFORCE_STATIC, sz, 20, iY += 24, 125, 22 );
	g_SampleUI.AddSlider( IDC_WINDFORCE_SCALE, 50, iY += 20, 100, 22, 0, 25, (int)(g_WindForce*100) );
	g_SampleUI.AddCheckBox( IDC_USECOMPUTESHADER, TEXT("Compute Shader") , 20, iY += 24, 125, 22, g_useCS );
	g_SampleUI.AddCheckBox( IDC_SIMULATION_LOD, TEXT("SimulationLOD") , 20, iY += 24, 125, 22, g_useSimulationLOD );
	g_SampleUI.AddCheckBox( IDC_SIMULATE, TEXT("Simulate") , 20, iY += 24, 125, 22, g_simulate );
	g_SampleUI.GetCheckBox( IDC_SIMULATE)->SetEnabled(false);
	g_SampleUI.AddCheckBox( IDC_RENDER_COLLISION_IMPLICITS, TEXT("Show Collison"), 20, iY += 24, 125, 22, g_RenderCollsionImplicits ); 

	//scene rendering
	g_SampleUI.AddCheckBox( IDC_SHOW_SCENE,TEXT("Show Scene"),20, iY += 24, 125, 22, g_RenderScene);

	TCHAR RecordedTransformsFile[MAX_PATH];
	NVUTFindDXSDKMediaFileCchT( RecordedTransformsFile, MAX_PATH,g_tstr_animations[g_animationIndex]);
	g_transformsFileIn.open (RecordedTransformsFile);

    if(g_renderMStrands && g_renderSStrands) g_InterpolateModel=HYBRID;
	else if(g_renderMStrands) g_InterpolateModel=MULTI_HYBRID;
	else if(g_renderSStrands) g_InterpolateModel=SINGLESTRAND;
	else g_InterpolateModel=NO_HAIR;


	g_superSampleViewport.TopLeftX = 0;
	g_superSampleViewport.TopLeftY = 0;
	g_superSampleViewport.MinDepth = 0;
	g_superSampleViewport.MaxDepth = 1;
	g_superSampleViewport.Width = g_Width*g_SSFACTOR;  
	g_superSampleViewport.Height = g_Height*g_SSFACTOR; 

	return hr;
}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D11 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings, void* pUserContext)
{
	if(g_firstFrame)
	{
		pDeviceSettings->d3d11.SyncInterval = 0; //turn off vsync
		pDeviceSettings->d3d11.sd.SampleDesc.Count = D3DMULTISAMPLE_8_SAMPLES;
		pDeviceSettings->d3d11.sd.SampleDesc.Quality = 0;
	}
	return true;
}

//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext)
{
	// Update the camera's position based on user input 
	g_Camera.FrameMove(fElapsedTime);
	g_HairShadows.GetCamera().FrameMove(fElapsedTime);
	D3DXVECTOR3 bbExtents = D3DXVECTOR3((g_maxHairLengthWorld+g_scalpMaxRadiusWorld),
		(g_maxHairLengthWorld+g_scalpMaxRadiusWorld),
		(g_maxHairLengthWorld+g_scalpMaxRadiusWorld));
	g_HairShadows.UpdateMatrices(g_Center, bbExtents);


	D3DXVECTOR3 v = *g_Camera.GetEyePt() - g_Center;
	double fDist = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    // LOD has to be updated according to distance from camera

	//rendering / tessellation LOD
	if (g_useDX11 && g_bDynamicLOD) 
	{
		g_fNumHairsLOD = min(1., 20 / fDist); // at distance 20 or closer we will have maximum LOD
        g_fNumHairsLOD = pow(g_fNumHairsLOD,g_LODScalingFactor);
		g_Effect->GetVariableByName("g_fNumHairsLOD")->AsScalar()->SetFloat((float)g_fNumHairsLOD);
		g_SampleUI.GetSlider( IDC_NHAIRSLOD_SCALE )->SetValue(g_fNumHairsLOD * 100);

        float fNumHair = max(0.1f,g_fNumHairsLOD);
		fNumHair = min(1.0f, fNumHair);
		g_fWidthHairsLOD = 1.0f/fNumHair;
		g_Effect->GetVariableByName("g_fWidthHairsLOD")->AsScalar()->SetFloat((float)g_fWidthHairsLOD);
		g_SampleUI.GetSlider( IDC_NHAIRSWIDTHLOD_SCALE )->SetValue(g_fWidthHairsLOD);
	}

    static int frame = 0;

	//simulation LOD
    if(g_useSimulationLOD)
	{
	    int skipFrames = 1 + (int)(15.0f * max(0.0f, 0.4f - min(1.f, 20 / fDist)));
		if(frame%skipFrames == 0) g_simulate = true; else g_simulate = false;
		g_SampleUI.GetCheckBox( IDC_SIMULATE)->SetChecked(g_simulate);
		frame++;
	}
}

//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext)
{
	// Pass messages to dialog resource manager calls so GUI state is updated correctly
	*pbNoFurtherProcessing = g_DialogResourceManager.MsgProc(hWnd, uMsg, wParam, lParam);
	if(*pbNoFurtherProcessing)
		return 0;

	// Pass messages to settings dialog if its active
	if(g_D3DSettingsDlg.IsActive())
	{
		g_D3DSettingsDlg.MsgProc(hWnd, uMsg, wParam, lParam);
		return 0;
	}

	// Give the dialogs a chance to handle the message first
	*pbNoFurtherProcessing = g_HUD.MsgProc(hWnd, uMsg, wParam, lParam);
	if(*pbNoFurtherProcessing)
		return 0;
	*pbNoFurtherProcessing = g_SampleUI.MsgProc(hWnd, uMsg, wParam, lParam);
	if(*pbNoFurtherProcessing)
		return 0;

	// Pass all remaining windows messages to camera so it can respond to user input
	if( DXUTIsMouseButtonDown(VK_RBUTTON) && DXUTIsKeyDown( VK_LSHIFT ) )
		g_windDirectionWidget.HandleMessages( hWnd, uMsg, wParam, lParam );	//handle messages for the wind directional control
	else //if something happened with the left button, or middle button
	{
		if(!DXUTIsKeyDown( VK_LSHIFT ) )
			g_Camera.HandleMessages(hWnd, uMsg, wParam, lParam);
		else
			g_HairShadows.GetCamera().HandleMessages(hWnd, uMsg, wParam, lParam);

	}

	if( !DXUTIsKeyDown( VK_LSHIFT ) )
		MoveModel(hWnd,uMsg, wParam,lParam); //this is the option for moving the model


	return 0;
}

//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------


void changeCameraAndWindPresets()
{
    if(g_cameraAndWindPreset == 0)
    {
        g_windVector = D3DXVECTOR3(0.75089055f, 0.12556140f, 0.64838076f );
        g_windDirectionWidget.SetLightDirection( g_windVector );
        g_applyHForce = true;
		g_WindForce = 0.08f;
        
        D3DXVECTOR3 at = D3DXVECTOR3(0, 0, 0);
        D3DXVECTOR3 lightPos = D3DXVECTOR3(4.7908502f,18.887268f,-9.7619190f);
        g_HairShadows.GetCamera().Reset();
        g_HairShadows.GetCamera().SetViewParams(&lightPos, &at);

        D3DXVECTOR3 eyePos = D3DXVECTOR3(0.0f, -1.0f, -15.0f);
        g_Camera.SetViewParams(&eyePos, &at);
    }
    else if(g_cameraAndWindPreset == 1)
    {
        g_windVector = D3DXVECTOR3(0.75089055f, 0.12556140f, 0.64838076f );
        g_windDirectionWidget.SetLightDirection( g_windVector );
        g_applyHForce = true;
		g_WindForce = 0.05f;
        g_windImpulseRandomness = 0.16f;
        g_windImpulseRandomnessSmoothingInterval = 15;
        
        D3DXVECTOR3 at = D3DXVECTOR3(0, 0, 0);
        D3DXVECTOR3 lightPos = D3DXVECTOR3(2.19152f, 15.6187f, -14.4954f);
        g_HairShadows.GetCamera().Reset();
        g_HairShadows.GetCamera().SetViewParams(&lightPos, &at);

        D3DXVECTOR3 eyePos = D3DXVECTOR3(-9.81684f, -0.238484f, -15.0457f);
        g_Camera.SetViewParams(&eyePos, &at);
    }
    else if(g_cameraAndWindPreset == 2)
    {
        g_windVector = D3DXVECTOR3(0.0f,0.0f,1.0f);
        g_windDirectionWidget.SetLightDirection( g_windVector );
        g_applyHForce = false;
		g_WindForce = 0.08f;
        
        D3DXVECTOR3 at = D3DXVECTOR3(0, 0, 0);
        D3DXVECTOR3 lightPos = D3DXVECTOR3(-11.6984f, 16.5866f, 13.4442f);
        g_HairShadows.GetCamera().Reset();
        g_HairShadows.GetCamera().SetViewParams(&lightPos, &at);

        D3DXVECTOR3 eyePos = D3DXVECTOR3(9.0111f, -0.293344f, 14.9507f);
        g_Camera.SetViewParams(&eyePos, &at);
    }

    g_pApplyHForceShaderVariable->SetBool(g_applyHForce);
    g_SampleUI.GetCheckBox( IDC_ADDFORCES )->SetChecked(g_applyHForce); 
	g_SampleUI.GetSlider( IDC_WINDFORCE_SCALE)->SetValue( (int)(g_WindForce*100) );

    if(!g_applyHForce && g_fluid)
        g_fluid->Reset();

}

void CALLBACK OnKeyboard(UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext)
{

	if (!bKeyDown)
		return;
	switch(nChar)
	{

	case 'h':
	case 'H':
		{
		    g_printHelp = !g_printHelp;
			break;
		}


	case 'o': //record the mouse transformations to file
	case 'O':
		{
			/* commenting this out to prevent accidental recording of transformations
			g_recordTransforms = !g_recordTransforms;
			if(g_recordTransforms)//open the text file
			{ 
			g_playTransforms = false;//stop playing a recorded animation
			g_transformsFileOut.open (g_tstr_animations[g_animationIndex],ios::trunc|ios::out);
			}
			else //close the text file
			g_transformsFileOut.close();
			*/
			break;
		}
	case 'p': //play recorded transformations from file
	case 'P':
		{  
			g_playTransforms = !g_playTransforms;
			g_SampleUI.GetCheckBox( IDC_PLAY_TRANSFORMS )->SetChecked(g_playTransforms);
			break;    
		}
	case 'k':
	case 'K':
		{
			g_applyHForce = !g_applyHForce;
			g_pApplyHForceShaderVariable->SetBool(g_applyHForce);
			g_SampleUI.GetCheckBox( IDC_ADDFORCES )->SetChecked(g_applyHForce); 
			if(!g_applyHForce)			
				g_fluid->Reset();
			break;
		}
	case 'g':
	case 'G':
		{
			g_renderGUI = !g_renderGUI;
			break;
		}
	case 'b':
	case 'B':
		{
		    g_takeImage = true;
            break;
		}
	case 'v':
	case 'V':
		{
			g_VisShadowMap = !g_VisShadowMap;
			break;
		}

	}
}

//--------------------------------------------------------------------------------------
// move the model
//--------------------------------------------------------------------------------------

void WorldToScreen(const D3DXVECTOR3& position, float& x, float& y)
{
	D3DXVECTOR3 screenPosition;
	D3DXMATRIX projection;
	D3DXMATRIX view;
	projection = *g_Camera.GetProjMatrix();
	view = *g_Camera.GetViewMatrix();
	D3DXMATRIX viewProjection;
	viewProjection = view * projection;

	D3DXVec3TransformCoord(&screenPosition, &position, &viewProjection);
	x = ( screenPosition.x + 1) * g_Width  / 2;
	y = (-screenPosition.y + 1) * g_Height / 2;
}

D3DXVECTOR3 ScreenToTrackballPoint(float x, float y)
{
	float scale = 4;
	x = - x / (scale * g_Width / 2);
	y =   y / (scale * g_Height / 2);
	float z = 0;
	float mag = x * x + y * y;
	if (mag > 1) {
		float scale = 1.0f / sqrtf(mag);
		x *= scale;
		y *= scale;
	}
	else
		z = sqrtf(1.0f - mag);
	return D3DXVECTOR3(x, y, z);
}


void MoveModel(HWND hWnd, int msg, WPARAM wParam, LPARAM lParam)
{
	static LPARAM g_LastMouse;
	static D3DXVECTOR3 g_LastTrackballPoint;
	static bool g_ModelIsMoved;
	static D3DXMATRIX g_PivotMatrix, g_PivotMatrixInv;
	static float g_ScreenModelCenterX, g_ScreenModelCenterY;

	int mouseX = (short)LOWORD(lParam);
	int mouseY = (short)HIWORD(lParam);
	switch (msg) {
case WM_RBUTTONDOWN:
case WM_MBUTTONDOWN:
	// Pivot transform
	{
		D3DXMATRIX pivotTranslation;
		D3DXMatrixTranslation(&pivotTranslation, -g_Center.x, -g_Center.y, -g_Center.z);
		D3DXMATRIX pivotRotation = *g_Camera.GetViewMatrix();
		pivotRotation(3, 0) = pivotRotation(3, 1) = pivotRotation(3, 2) = 0;
		g_PivotMatrix = pivotTranslation * pivotRotation;
		D3DXMatrixInverse(&g_PivotMatrixInv, 0, &g_PivotMatrix);
	}

	WorldToScreen(g_Center, g_ScreenModelCenterX, g_ScreenModelCenterY);
	g_LastTrackballPoint = ScreenToTrackballPoint(mouseX - g_ScreenModelCenterX, mouseY - g_ScreenModelCenterY);
	g_LastMouse = lParam;
	g_ModelIsMoved = true;
	break;
case WM_MOUSEMOVE:
	if (g_ModelIsMoved) 
	{
		D3DXMATRIX transform;
		D3DXMatrixIdentity(&transform);

		// Translate
		if (wParam & MK_RBUTTON) 
		{
			float scale = 2;
			float dx = (scale * ((short)LOWORD(g_LastMouse) - mouseX)) / g_Width;
			float dy = (scale * ((short)HIWORD(g_LastMouse) - mouseY)) / g_Height;
			D3DXMatrixTranslation(&transform, -dx, dy, 0);
			g_LastMouse = lParam;
		}


		// Rotate
		else if (wParam & MK_MBUTTON) 
		{
			D3DXVECTOR3 trackballPoint = ScreenToTrackballPoint(mouseX - g_ScreenModelCenterX, mouseY - g_ScreenModelCenterY);
			D3DXVECTOR3 crossV;
			D3DXVec3Cross(&crossV, &g_LastTrackballPoint, &trackballPoint);
			float dotV = D3DXVec3Dot(&g_LastTrackballPoint, &trackballPoint);
			D3DXQUATERNION dq(crossV.x, crossV.y, crossV.z, dotV);
			D3DXMatrixRotationQuaternion(&transform, &dq);
			g_LastTrackballPoint = trackballPoint;
		}

		// Final transform
		g_Transform = g_PivotMatrix * transform * g_PivotMatrixInv;
		g_UseTransformations = true;

		if(g_recordTransforms) //save the transformations to file
		{
			g_transformsFileOut<<endl;
			g_transformsFileOut<<g_Transform(0,0)<<' '<<g_Transform(0,1)<<' '<<g_Transform(0,2)<<' '<<g_Transform(0,3)<<' '
				<<g_Transform(1,0)<<' '<<g_Transform(1,1)<<' '<<g_Transform(1,2)<<' '<<g_Transform(1,3)<<' '
				<<g_Transform(2,0)<<' '<<g_Transform(2,1)<<' '<<g_Transform(2,2)<<' '<<g_Transform(2,3)<<' '
				<<g_Transform(3,0)<<' '<<g_Transform(3,1)<<' '<<g_Transform(3,2)<<' '<<g_Transform(3,3);
		}
	}
	break;
case WM_RBUTTONUP:
case WM_MBUTTONUP:
	{
		g_ModelIsMoved = false;
		break;
	}
default:
	break;
	}
}

//--------------------------------------------------------------------------------------
// Handle the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext)
{
	WCHAR sz[MAX_PATH];

	switch(nControlID)
	{
	case IDC_TOGGLEFULLSCREEN: DXUTToggleFullScreen(); break;
	case IDC_TOGGLEREF:        DXUTToggleREF(); break;
	case IDC_CHANGEDEVICE:     g_D3DSettingsDlg.SetActive(!g_D3DSettingsDlg.IsActive()); break;

	case IDC_SIMULATE : 
		{	
			g_simulate = g_SampleUI.GetCheckBox( IDC_SIMULATE )->GetChecked(); 
			g_pSimulateShaderVariable->SetBool(g_simulate);
			g_firstMovedFrame=true;
			g_hairReset = true;
			g_bApplyAdditionalTransform = true;
			//if we are not simulating then we start accumulating the transforms that have to be applied 
			//to the hair root vertices when we do start simulating again
            if(!g_simulate) 
			    D3DXMatrixIdentity(&g_RootTransform);
			break;	 
		}
	case IDC_SHOW_SCENE:
		{
			g_RenderScene = g_SampleUI.GetCheckBox( IDC_SHOW_SCENE )->GetChecked(); 
			break;
		}
	case IDC_USECOMPUTESHADER :
		{
			g_useCS = g_SampleUI.GetCheckBox( IDC_USECOMPUTESHADER)->GetChecked();
			g_hairReset = true;
			break;
		}
	case IDC_SIMULATION_LOD :
		{
			g_useSimulationLOD = g_SampleUI.GetCheckBox( IDC_SIMULATION_LOD)->GetChecked();
			g_SampleUI.GetCheckBox( IDC_SIMULATE)->SetEnabled(!g_useSimulationLOD);
    		g_simulate = true;
			g_SampleUI.GetCheckBox( IDC_SIMULATE)->SetChecked(g_simulate);
			break;
		}
	case IDC_USE_SHADOWS : 
		{
			g_renderShadows = g_SampleUI.GetCheckBox( IDC_USE_SHADOWS )->GetChecked(); 
			g_pShadowsShaderVariable->SetBool(g_renderShadows);
			break;
		}
	case IDC_COMBO_SHADING:
		{
			g_currHairShadingParams = (UI_OPTION) g_SampleUI.GetComboBox( IDC_COMBO_SHADING )->GetSelectedIndex();
            hairShadingParams[g_currHairShadingParams].setShaderVariables(g_Effect); 
		    g_Effect->GetVariableByName("hairTexture")->AsShaderResource()->SetResource( g_HairBaseRV[g_currHairShadingParams] );
			break;
		}
	case IDC_PLAY_TRANSFORMS:
		{
			g_playTransforms = g_SampleUI.GetCheckBox( IDC_PLAY_TRANSFORMS )->GetChecked();
			break;
		}
   
	case IDC_USE_SHORT_HAIR:
		{
			g_useShortHair = g_SampleUI.GetCheckBox( IDC_USE_SHORT_HAIR)->GetChecked();
			g_Reset = true;
			break;
		}
	case IDC_LOOP_ANIMATION:
		{
			g_loopTransforms = g_SampleUI.GetCheckBox( IDC_LOOP_ANIMATION)->GetChecked();
			break;
		}
	case IDC_STRANDB:
		{
			g_renderMStrands = g_SampleUI.GetCheckBox( IDC_STRANDB )->GetChecked();
			if(g_renderMStrands && g_renderSStrands) g_InterpolateModel=HYBRID;
			else if(g_renderMStrands) g_InterpolateModel=MULTI_HYBRID;
			else if(g_renderSStrands) g_InterpolateModel=SINGLESTRAND;
			else g_InterpolateModel=NO_HAIR;
			break;
		}
	case IDC_STRANDC:
		{
			g_renderSStrands = g_SampleUI.GetCheckBox( IDC_STRANDC )->GetChecked();
			if(g_renderMStrands && g_renderSStrands) g_InterpolateModel=HYBRID;
			else if(g_renderMStrands) g_InterpolateModel=MULTI_HYBRID;
			else if(g_renderSStrands) g_InterpolateModel=SINGLESTRAND;
			else g_InterpolateModel=NO_HAIR;
			break;
		}
	case IDC_USEDX11:
		{
			g_useDX11 = g_SampleUI.GetCheckBox( IDC_USEDX11 )->GetChecked();
			g_SampleUI.GetCheckBox( IDC_DYNAMIC_LOD )->SetEnabled(g_useDX11);
			g_SampleUI.GetSlider( IDC_NHAIRSLOD_SCALE )->SetEnabled(g_useDX11 && !g_bDynamicLOD);
			g_SampleUI.GetSlider( IDC_NHAIRSWIDTHLOD_SCALE )->SetEnabled(g_useDX11 && !g_bDynamicLOD);
            g_SampleUI.GetSlider( IDC_LODSCALEFACTOR_SCALE )->SetEnabled(g_useDX11);

			//if we are not using Dx11 then we dont have LOD (since it is not that straightforward to implement)
			if(!g_useDX11)
			{
         		g_SampleUI.GetSlider( IDC_NHAIRSLOD_SCALE )->SetValue(100);
        		g_SampleUI.GetSlider( IDC_NHAIRSWIDTHLOD_SCALE )->SetValue(1);
			}

			break;
		}
	case IDC_DYNAMIC_LOD:
		{
			g_bDynamicLOD = g_SampleUI.GetCheckBox( IDC_DYNAMIC_LOD )->GetChecked();
			g_SampleUI.GetSlider( IDC_NHAIRSLOD_SCALE )->SetEnabled(g_useDX11 && !g_bDynamicLOD);
			g_SampleUI.GetSlider( IDC_NHAIRSWIDTHLOD_SCALE )->SetEnabled(g_useDX11 && !g_bDynamicLOD);
            g_SampleUI.GetSlider( IDC_LODSCALEFACTOR_SCALE )->SetEnabled(g_useDX11);
			break;
		}
	case IDC_ADDFORCES: 
		{
			g_applyHForce = g_SampleUI.GetCheckBox( IDC_ADDFORCES )->GetChecked(); 
			g_pApplyHForceShaderVariable->SetBool(g_applyHForce);
			if(!g_applyHForce)
				g_fluid->Reset();
			break;
		}
	case IDC_RENDER_COLLISION_IMPLICITS:
		{
			g_RenderCollsionImplicits = g_SampleUI.GetCheckBox( IDC_RENDER_COLLISION_IMPLICITS )->GetChecked(); 
			break;
		}
	case IDC_CURLYHAIR :
		{
			g_bCurlyHair = g_SampleUI.GetCheckBox( IDC_CURLYHAIR )->GetChecked();
			g_pCurlyHairShaderVariable->SetBool(g_bCurlyHair);
			g_firstMovedFrame=true;
			break;
		}
	case IDC_RESET_SIM: 
		{
			g_useLinearStiffness = true;
			g_collisions = true;
		    g_addGravity = true; 
			g_pAddGravityShaderVariable->SetBool(g_addGravity);


            g_cameraAndWindPreset = 1;
            g_animationIndex = 0;
            g_playTransforms = false; 
   			g_SampleUI.GetCheckBox( IDC_PLAY_TRANSFORMS )->SetChecked(g_playTransforms);
            g_loopTransforms = false;
   			g_SampleUI.GetCheckBox( IDC_LOOP_ANIMATION )->SetChecked(g_loopTransforms);
			g_useShortHair = false;
			g_SampleUI.GetCheckBox( IDC_USE_SHORT_HAIR )->SetChecked(g_useShortHair);
            g_UIOption = SHOW_HAIR_RENDERING;
			g_applyHForce = false;
			g_pApplyHForceShaderVariable->SetBool(g_applyHForce);
			g_SampleUI.GetCheckBox( IDC_ADDFORCES )->SetChecked(g_applyHForce); 
            g_useCS = true;
			g_SampleUI.GetCheckBox( IDC_USECOMPUTESHADER )->SetChecked(g_useCS);
            g_useSimulationLOD = true;
			g_SampleUI.GetCheckBox( IDC_SIMULATION_LOD )->SetChecked(g_useSimulationLOD);
			g_simulate = true;
			g_pSimulateShaderVariable->SetBool(g_simulate);
			g_SampleUI.GetCheckBox( IDC_SIMULATE )->SetChecked(g_simulate); 
            g_currHairShadingParams = 0;
         	g_SampleUI.GetComboBox( IDC_COMBO_SHADING )->SetSelectedByIndex( g_currHairShadingParams );
			g_bCurlyHair = false;
			g_pCurlyHairShaderVariable->SetBool(g_bCurlyHair);
			g_SampleUI.GetCheckBox( IDC_CURLYHAIR )->SetChecked(g_bCurlyHair);
			g_renderShadows = true;
			g_SampleUI.GetCheckBox( IDC_USE_SHADOWS )->SetChecked(g_renderShadows);
			g_pShadowsShaderVariable->SetBool(g_renderShadows);
            g_useDX11 = true;
			g_SampleUI.GetCheckBox( IDC_USEDX11 )->SetChecked(g_useDX11);
            g_bDynamicLOD = true;
			g_SampleUI.GetCheckBox( IDC_DYNAMIC_LOD )->SetChecked(g_bDynamicLOD);
            g_fNumHairsLOD = 1;
            g_SampleUI.GetSlider( IDC_NHAIRSLOD_SCALE)->SetValue(g_fNumHairsLOD * 100);
			g_SampleUI.GetSlider( IDC_NHAIRSLOD_SCALE )->SetEnabled(false);
			g_fWidthHairsLOD = 1;
            g_SampleUI.GetSlider( IDC_NHAIRSWIDTHLOD_SCALE )->SetValue(g_fWidthHairsLOD);
			g_SampleUI.GetSlider( IDC_NHAIRSWIDTHLOD_SCALE )->SetEnabled(false);
            g_LODScalingFactor = 1.2f;
            g_SampleUI.GetSlider( IDC_LODSCALEFACTOR_SCALE)->SetValue(g_LODScalingFactor*100);
	        g_SampleUI.GetSlider( IDC_LODSCALEFACTOR_SCALE )->SetEnabled(true);
            g_RenderFloor = true;
			g_RenderScalp = true;
            g_MeshWithShadows = true;
            g_bVisualizeCFs = false;
			changeCameraAndWindPresets();
			g_fluid->Reset();
			g_Reset = true;  
			break;
		}


	case IDC_WINDFORCE_SCALE:
		{
			g_WindForce = (float) (g_SampleUI.GetSlider( IDC_WINDFORCE_SCALE )->GetValue()* 0.01f);
			StringCchPrintf( sz, 100, L"Wind Strength: %0.2f", g_WindForce ); 
			g_SampleUI.GetStatic( IDC_WINDFORCE_STATIC )->SetText( sz );
			break;
		}
	case IDC_NHAIRSLOD_SCALE:
		{
			g_fNumHairsLOD = g_SampleUI.GetSlider( IDC_NHAIRSLOD_SCALE )->GetValue() / 100.f;
			g_Effect->GetVariableByName("g_fNumHairsLOD")->AsScalar()->SetFloat((float)g_fNumHairsLOD);
			break;
		}
	case IDC_NHAIRSWIDTHLOD_SCALE:
		{
			g_fWidthHairsLOD = g_SampleUI.GetSlider( IDC_NHAIRSWIDTHLOD_SCALE )->GetValue();
			g_Effect->GetVariableByName("g_fWidthHairsLOD")->AsScalar()->SetFloat((float)g_fWidthHairsLOD);
			break;
		}
	case IDC_LODSCALEFACTOR_SCALE:
		{
			g_LODScalingFactor = g_SampleUI.GetSlider( IDC_LODSCALEFACTOR_SCALE )->GetValue()/100.0f;
			break;
		}
	}
}


HRESULT ExtractAndSetCollisionImplicitMatrices()
{
	HRESULT hr = S_OK;

	vector<D3DXMATRIX> sphereTransformationMatrices;
	sphereTransformationMatrices.reserve(g_numSpheres);
	vector<D3DXMATRIX> sphereInverseTransformationMatrices;
	sphereInverseTransformationMatrices.reserve(g_numSpheres);

	vector<D3DXMATRIX> cylinderTransformationMatrices;
	cylinderTransformationMatrices.reserve(g_numCylinders);
	vector<D3DXMATRIX> cylinderInverseTransformationMatrices;
	cylinderInverseTransformationMatrices.reserve(g_numCylinders);

	vector<D3DXMATRIX> sphereNoMoveConstraintTransformationMatrices;
	sphereNoMoveConstraintTransformationMatrices.reserve(g_numSpheresNoMoveConstraint);
	vector<D3DXMATRIX> sphereNoMoveConstraintInverseTransformationMatrices;
	sphereNoMoveConstraintInverseTransformationMatrices.reserve(g_numSpheresNoMoveConstraint);


	for(int i=0;i<int(collisionObjectTransforms.size());i++)
	{
		D3DXMATRIX initialTransformation = collisionObjectTransforms.at(i).InitialTransform;
		//if this is a static object all the collision objects have the same transform
		D3DXMATRIX currentTransformation = g_TotalTransform;
		D3DXMATRIX totalTransformation; 
		totalTransformation = initialTransformation * currentTransformation;
		D3DXMATRIX inverseTotalTransformation;
		D3DXMatrixInverse(&inverseTotalTransformation, NULL, &totalTransformation);
		if(collisionObjectTransforms.at(i).implicitType==SPHERE)
		{
			sphereTransformationMatrices.push_back(totalTransformation); 
			sphereInverseTransformationMatrices.push_back(inverseTotalTransformation);
		}
		else if(collisionObjectTransforms.at(i).implicitType==CYLINDER)
		{
			cylinderTransformationMatrices.push_back(totalTransformation); 
			cylinderInverseTransformationMatrices.push_back(inverseTotalTransformation);
		}
		else if(collisionObjectTransforms.at(i).implicitType==SPHERE_NO_MOVE_CONSTRAINT)
		{
			sphereNoMoveConstraintTransformationMatrices.push_back(totalTransformation);	
			sphereNoMoveConstraintInverseTransformationMatrices.push_back(inverseTotalTransformation);		
		}
	}

	//spheres
	assert(sphereTransformationMatrices.size()==(unsigned int)g_numSpheres);
	assert(sphereTransformationMatrices.size()<MAX_IMPLICITS); //make sure the number of matrices is less than that declared in fx file
	if(g_numSpheres>0)
		V(g_collisionSphereTransformationsEV->SetMatrixArray((float*)&sphereTransformationMatrices[0], 0, static_cast<UINT>(sphereTransformationMatrices.size())));
	assert(sphereInverseTransformationMatrices.size()==(unsigned int)g_numSpheres);
	assert(sphereInverseTransformationMatrices.size()<MAX_IMPLICITS); //make sure the number of matrices is less than that declared in fx file
	if(g_numSpheres>0)
		V(g_collisionSphereInverseTransformationsEV->SetMatrixArray((float*)&sphereInverseTransformationMatrices[0], 0, static_cast<UINT>(sphereInverseTransformationMatrices.size())));

	//cylinders
	assert(cylinderTransformationMatrices.size()==(unsigned int)g_numCylinders);
	assert(cylinderTransformationMatrices.size()<MAX_IMPLICITS); //make sure the number of matrices is less than that declared in fx file
	if(g_numCylinders>0)
		V(g_collisionCylinderTransformationsEV->SetMatrixArray((float*)&cylinderTransformationMatrices[0], 0, static_cast<UINT>(cylinderTransformationMatrices.size())));
	assert(cylinderInverseTransformationMatrices.size()==(unsigned int)g_numCylinders);
	assert(cylinderInverseTransformationMatrices.size()<MAX_IMPLICITS); //make sure the number of matrices is less than that declared in fx file
	if(g_numCylinders>0)
		V(g_collisionCylinderInverseTransformationsEV->SetMatrixArray((float*)&cylinderInverseTransformationMatrices[0], 0, static_cast<UINT>(cylinderInverseTransformationMatrices.size())));

	//sphere no move constraint
	assert(sphereNoMoveConstraintTransformationMatrices.size()==(unsigned int)g_numSpheresNoMoveConstraint);
	assert(sphereNoMoveConstraintTransformationMatrices.size()<MAX_IMPLICITS); //make sure the number of matrices is less than that declared in fx file
	if(g_numSpheresNoMoveConstraint>0)
		V(g_SphereNoMoveImplicitTransformEV->SetMatrixArray((float*)&sphereNoMoveConstraintTransformationMatrices[0], 0, static_cast<UINT>(sphereNoMoveConstraintTransformationMatrices.size())));
	assert(sphereNoMoveConstraintInverseTransformationMatrices.size()==(unsigned int)g_numSpheresNoMoveConstraint);
	assert(sphereNoMoveConstraintInverseTransformationMatrices.size()<MAX_IMPLICITS); //make sure the number of matrices is less than that declared in fx file
	if(g_numSpheresNoMoveConstraint>0)
		V(g_SphereNoMoveImplicitInverseTransformEV->SetMatrixArray((float*)&sphereNoMoveConstraintInverseTransformationMatrices[0], 0, static_cast<UINT>(sphereNoMoveConstraintInverseTransformationMatrices.size())));


    //also put these transforms in a buffer for the compute shader
    g_cbTransformsInitialData.g_NumSphereImplicits = g_numSpheres;
    g_cbTransformsInitialData.g_NumCylinderImplicits = g_numCylinders;
    g_cbTransformsInitialData.g_NumSphereNoMoveImplicits = g_numSpheresNoMoveConstraint;
    g_cbTransformsInitialData.padding = 0;

    for(unsigned int i=0;i<sphereTransformationMatrices.size();i++)
        g_cbTransformsInitialData.CollisionSphereTransformations[i] = sphereTransformationMatrices[i];
    for(unsigned int i=0;i<sphereInverseTransformationMatrices.size();i++)
         g_cbTransformsInitialData.CollisionSphereInverseTransformations[i] = sphereInverseTransformationMatrices[i];
    for(unsigned int i=0;i<cylinderTransformationMatrices.size();i++)
         g_cbTransformsInitialData.CollisionCylinderTransformations[i] = cylinderTransformationMatrices[i];
    for(unsigned int i=0;i<cylinderInverseTransformationMatrices.size();i++)
        g_cbTransformsInitialData.CollisionCylinderInverseTransformations[i] = cylinderInverseTransformationMatrices[i];
    for(unsigned int i=0;i<sphereNoMoveConstraintInverseTransformationMatrices.size();i++)
        g_cbTransformsInitialData.SphereNoMoveImplicitInverseTransform[i] = sphereNoMoveConstraintInverseTransformationMatrices[i];

	return hr;

}


//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo, DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext)
{
	return true;
}

HRESULT MyCreateEffectFromFile(TCHAR *fName, DWORD dwShaderFlags, ID3D11Device *pDevice, ID3DX11Effect **pEffect);
HRESULT MyCreateEffectFromCompiledFile(TCHAR *fName, DWORD dwShaderFlags, ID3D11Device *pDevice, ID3DX11Effect **pEffect);
HRESULT LoadComputeShader(ID3D11Device* pd3dDevice, WCHAR *szFilename, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3D11ComputeShader **ppComputeShader);

HRESULT InitEffect(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext)
{
	HRESULT hr;

	// Read the D3DX effect file

	//Try to read the pre-compiled file since its much faster, if that does not work read and compile the original file
	hr = DXUTFindDXSDKMediaFileCch(g_pEffectPath, MAX_PATH, L"Hair.fxo");
    if (FAILED(hr))
    {
		V_RETURN(NVUTFindDXSDKMediaFileCchT(g_pEffectPath, MAX_PATH, L"Hair.fx"));
		V_RETURN(MyCreateEffectFromFile(g_pEffectPath, D3DXSHADER_SKIPVALIDATION, pd3dDevice, &g_Effect));
    }
    else
    {
	    MyCreateEffectFromCompiledFile(g_pEffectPath, D3DXSHADER_SKIPVALIDATION, pd3dDevice, &g_Effect);
    }

	g_pTechniqueRenderArrow  = g_Effect->GetTechniqueByName("RenderArrow");

	V_RETURN( LoadComputeShader(pd3dDevice, L"HairSimulateCS.hlsl", "UpdateParticlesSimulate", "cs_4_0", &g_pCSSimulateParticles) );

	//get shader variables
	g_pShadowsShaderVariable = g_Effect->GetVariableByName( "useShadows")->AsScalar();
	g_pShadowsShaderVariable->SetBool(g_renderShadows);
	g_pApplyHForceShaderVariable = g_Effect->GetVariableByName( "g_bApplyHorizontalForce")->AsScalar();
	g_pApplyHForceShaderVariable->SetBool(g_applyHForce);
	g_Effect->GetVariableByName( "g_densityThreshold")->AsScalar()->SetFloat(g_densityThreshold);
	g_pSimulateShaderVariable = g_Effect->GetVariableByName( "g_bSimulate")->AsScalar();
	g_pSimulateShaderVariable->SetBool(g_simulate);
	g_pAddGravityShaderVariable = g_Effect->GetVariableByName( "g_bAddGravity")->AsScalar();
	g_pAddGravityShaderVariable->SetBool(g_addGravity);
	g_pCurlyHairShaderVariable = g_Effect->GetVariableByName("doCurlyHair")->AsScalar();
	g_pCurlyHairShaderVariable->SetBool(g_bCurlyHair); 
	g_InterHairForcesShaderVariable = g_Effect->GetVariableByName( "g_interHairForces")->AsScalar();
	g_InterHairForcesShaderVariable->SetFloat(g_interHairForces);
	g_AngularStiffnessMultiplierShaderVariable = g_Effect->GetVariableByName("g_angularStiffness")->AsScalar();
	g_AngularStiffnessMultiplierShaderVariable->SetFloat(g_angularSpringStiffnessMultiplier);
	g_GravityStrengthShaderVariable = g_Effect->GetVariableByName("g_gravityStrength")->AsScalar();
	g_GravityStrengthShaderVariable->SetFloat(g_GravityStrength);
	g_BlurRadiusShaderVariable = g_Effect->GetVariableByName("blurRadius")->AsScalar();
	g_BlurRadiusShaderVariable->SetInt(g_BlurRadius);
	g_BlurSigmaShaderVariable = g_Effect->GetVariableByName("blurSigma")->AsScalar();
	g_BlurSigmaShaderVariable->SetFloat(g_BlurSigma);
	V_GET_VARIABLE_RET( g_Effect, g_pEffectPath, g_hairTextureArrayShaderVariable, "hairTextureArray", AsShaderResource );
	g_hairTextureArrayShaderVariable->SetResource( hairTextureRV );
	g_UseBlurTextureForForcesShaderVariable = g_Effect->GetVariableByName("useBlurTexture")->AsScalar(); 
	g_UseBlurTextureForForcesShaderVariable->SetBool(g_useBlurredDensityForInterHairForces); 
	g_UseGradientBasedForceShaderVariable = g_Effect->GetVariableByName("useGradientBasedForce")->AsScalar(); 
	g_currentInterHairForcesChoice==0 ? g_UseGradientBasedForceShaderVariable->SetBool(true):g_UseGradientBasedForceShaderVariable->SetBool(false);
	g_ArrowColorShaderVariable = g_Effect->GetVariableByName("arrowColor")->AsVector();
	g_BaseColorShaderVariable = g_Effect->GetVariableByName("g_baseColor")->AsVector();
	g_SpecColorShaderVariable = g_Effect->GetVariableByName("g_specColor")->AsVector();
	g_UseScalpTextureShaderVariable = g_Effect->GetVariableByName("g_useScalpTexture")->AsScalar();
	V_GET_VARIABLE_RETBOOL(g_Effect, g_pEffectPath, g_collisionSphereTransformationsEV, "CollisionSphereTransformations", AsMatrix);
	V_GET_VARIABLE_RETBOOL(g_Effect, g_pEffectPath, g_collisionSphereInverseTransformationsEV, "CollisionSphereInverseTransformations", AsMatrix);
	V_GET_VARIABLE_RETBOOL(g_Effect, g_pEffectPath, g_collisionCylinderTransformationsEV, "CollisionCylinderTransformations", AsMatrix);
	V_GET_VARIABLE_RETBOOL(g_Effect, g_pEffectPath, g_collisionCylinderInverseTransformationsEV, "CollisionCylinderInverseTransformations", AsMatrix);
	V_GET_VARIABLE_RETBOOL(g_Effect, g_pEffectPath, g_SphereNoMoveImplicitInverseTransformEV, "SphereNoMoveImplicitInverseTransform", AsMatrix);
	V_GET_VARIABLE_RETBOOL(g_Effect, g_pEffectPath, g_SphereNoMoveImplicitTransformEV, "SphereNoMoveImplicitTransform", AsMatrix);
	V_GET_VARIABLE_RETBOOL(g_Effect, g_pEffectPath, g_SceneColor2XShaderResourceVariable, "g_SupersampledSceneColor", AsShaderResource);
	V_GET_VARIABLE_RETBOOL(g_Effect, g_pEffectPath, g_SceneDepthShaderResourceVariable, "g_sceneDepth", AsShaderResource);


	g_Effect->GetVariableByName("g_SoftEdges")->AsScalar()->SetFloat(g_SoftEdges);
	g_Effect->GetVariableByName("g_SigmaA")->AsScalar()->SetFloat(-g_SigmaA/g_MaxWorldScale);
	g_Effect->GetVariableByName("g_TessellatedMasterStrandLengthMax")->AsScalar()->SetInt(g_TessellatedMasterStrandLengthMax);
	g_Effect->GetVariableByName("g_thinning")->AsScalar()->SetFloat(g_thinning);

	g_Effect->GetVariableByName("g_NumTotalWisps")->AsScalar()->SetInt(g_NumWisps);
	g_Effect->GetVariableByName("g_NumMaxStrandsPerWisp")->AsScalar()->SetInt(MAX_INTERPOLATED_HAIR_PER_WISP);
	g_Effect->GetVariableByName("g_maxLengthToRoot")->AsScalar()->SetFloat(g_maxHairLength);



	g_Effect->GetVariableByName("g_StrandCircularCoordinates")->AsShaderResource()->SetResource(g_StrandCircularCoordinatesRV);
	g_Effect->GetVariableByName("g_StrandCoordinates")->AsShaderResource()->SetResource(g_StrandCoordinatesRV);
	g_Effect->GetVariableByName("g_StrandSizes")->AsShaderResource()->SetResource(g_StrandSizesRV);
	g_Effect->GetVariableByName("g_MasterStrandRootIndices")->AsShaderResource()->SetResource(g_RootsIndicesRV);
	g_Effect->GetVariableByName("g_OriginalMasterStrandRootIndices")->AsShaderResource()->SetResource(g_rootsIndicesMasterStrandRV);
	g_Effect->GetVariableByName("g_MasterStrandLengths")->AsShaderResource()->SetResource(g_MasterLengthsRV);
	g_Effect->GetVariableByName("g_MasterStrandLengthsUntessellated")->AsShaderResource()->SetResource(g_MasterLengthsRVUntessellated);
	g_Effect->GetVariableByName("g_MasterStrandRootIndicesUntessellated")->AsShaderResource()->SetResource(g_RootsIndicesRVUntessellated); 
	g_Effect->GetVariableByName("g_Attributes")->AsShaderResource()->SetResource(g_StrandAttributesRV);
	g_Effect->GetVariableByName("g_LengthsToRoots")->AsShaderResource()->SetResource(g_MasterStrandLengthToRootRV);		
	g_Effect->GetVariableByName("g_tessellatedMasterStrandRootIndex")->AsShaderResource()->SetResource(g_TessellatedMasterStrandRootIndexRV);
	g_Effect->GetVariableByName("g_tessellatedCoordinateFrames")->AsShaderResource()->SetResource(g_TessellatedCoordinateFrameRV);	
	g_Effect->GetVariableByName("g_StrandLengths")->AsShaderResource()->SetResource(g_StrandLengthsRV);
	g_Effect->GetVariableByName("g_StrandColors")->AsShaderResource()->SetResource(g_StrandColorsRV);
	g_Effect->GetVariableByName("g_strandDeviations")->AsShaderResource()->SetResource(g_StrandDeviationsRV);
	g_Effect->GetVariableByName("g_curlDeviations")->AsShaderResource()->SetResource(g_CurlDeviationsRV);
	g_Effect->GetVariableByName("g_OriginalMasterStrand")->AsShaderResource()->SetResource(g_OriginalMasterStrandRV);
	g_Effect->GetVariableByName("g_OriginalVectors")->AsShaderResource()->SetResource(g_originalVectorsRV);
	g_Effect->GetVariableByName("g_VertexNumber")->AsShaderResource()->SetResource(g_MasterStrandLocalVertexNumberRV);
	g_Effect->GetVariableByName("g_TangentJitter")->AsShaderResource()->SetResource(g_TangentJitterRV);
	g_Effect->GetVariableByName("g_Lengths")->AsShaderResource()->SetResource(g_MasterStrandLengthsRV);


	//collisions
	g_Effect->GetVariableByName("g_NumSphereImplicits")->AsScalar()->SetInt(g_numSpheres);
	g_Effect->GetVariableByName("g_NumCylinderImplicits")->AsScalar()->SetInt(g_numCylinders);
	g_Effect->GetVariableByName("g_NumSphereNoMoveImplicits")->AsScalar()->SetInt(g_numSpheresNoMoveConstraint);

	if(g_numSpheres>0)
		g_Effect->GetVariableByName("g_UseSphereForBarycentricInterpolant")->AsScalar()->SetIntArray(&sphereIsBarycentricObstacle[0],0,g_numSpheres);
	if(g_numCylinders>0)
		g_Effect->GetVariableByName("g_UseCylinderForBarycentricInterpolant")->AsScalar()->SetIntArray(&cylinderIsBarycentricObstacle[0],0,g_numCylinders);
	if(g_numSpheresNoMoveConstraint>0)
		g_Effect->GetVariableByName("g_UseSphereNoMoveForBarycentricInterpolant")->AsScalar()->SetIntArray(&sphereNoMoveIsBarycentricObstacle[0],0,g_numSpheresNoMoveConstraint);

	ExtractAndSetCollisionImplicitMatrices();

	V(g_Effect->GetVariableByName("TotalTransformation")->AsMatrix()->SetMatrix(g_TotalTransform));

	TCHAR fullPath[MAX_PATH];

	//textures
	V_RETURN(NVUTFindDXSDKMediaFileCchT( fullPath, MAX_PATH, TEXT("HairTextures\\specNoise.dds") ));
	V_RETURN(loadTextureFromFile(fullPath,"specularNoise",pd3dDevice));

	ID3D11Texture2D* hairTexture = NULL;
	V_RETURN(LoadTextureArray(pd3dDevice, pd3dContext, "HairTextures/HairSpecMap", 4 , &hairTexture, &hairTextureRV,1));
	g_hairTextureArrayShaderVariable->SetResource( hairTextureRV );
	SAFE_RELEASE(hairTexture);


	char filename[MAX_PATH];
	WCHAR wfilename[MAX_PATH];

	sprintf_s(filename,100,"%s\\%s",g_loadDirectory,"hairbase.dds");
	mbstowcs_s(NULL, wfilename, MAX_PATH, filename, MAX_PATH);
	V_RETURN(NVUTFindDXSDKMediaFileCchT( fullPath, MAX_PATH,wfilename));
	V_RETURN(loadTextureFromFile(fullPath,&g_HairBaseRV[0],pd3dDevice));

	sprintf_s(filename,100,"%s\\%s",g_loadDirectory,"hairbaseBrown.dds");
	mbstowcs_s(NULL, wfilename, MAX_PATH, filename, MAX_PATH);
	V_RETURN(NVUTFindDXSDKMediaFileCchT( fullPath, MAX_PATH,wfilename));
	V_RETURN(loadTextureFromFile(fullPath,&g_HairBaseRV[1],pd3dDevice));

	sprintf_s(filename,100,"%s\\%s",g_loadDirectory,"hairbaseRed.dds");
	mbstowcs_s(NULL, wfilename, MAX_PATH, filename, MAX_PATH);
	V_RETURN(NVUTFindDXSDKMediaFileCchT( fullPath, MAX_PATH,wfilename));
	V_RETURN(loadTextureFromFile(fullPath,&g_HairBaseRV[2],pd3dDevice));

	sprintf_s(filename,100,"%s\\%s",g_loadDirectory,"meshAOMap.dds");
	mbstowcs_s(NULL, wfilename, MAX_PATH, filename, MAX_PATH);
	V_RETURN(NVUTFindDXSDKMediaFileCchT( fullPath, MAX_PATH,wfilename));
	V_RETURN(loadTextureFromFile(fullPath,"meshAOMap",pd3dDevice));

	sprintf_s(filename,100,"%s\\%s",g_loadDirectory,"densityThicknessMapBarycentric.dds");
	mbstowcs_s(NULL, wfilename, MAX_PATH, filename, MAX_PATH);
	V_RETURN(NVUTFindDXSDKMediaFileCchT( fullPath, MAX_PATH,wfilename));
	V_RETURN(loadTextureFromFile(fullPath,"densityThicknessMapBarycentric",pd3dDevice));

	sprintf_s(filename,100,"%s\\%s",g_loadDirectory,"densityThicknessMapClump.dds");
	mbstowcs_s(NULL, wfilename, MAX_PATH, filename, MAX_PATH);
	V_RETURN(NVUTFindDXSDKMediaFileCchT( fullPath, MAX_PATH, wfilename));
	V_RETURN(loadTextureFromFile(fullPath,"densityThicknessMapClump",pd3dDevice));

	changeCameraAndWindPresets();

	return hr;
}

//--------------------------------------------------------------------------------------
// Initialize the super sampling buffers
//--------------------------------------------------------------------------------------
HRESULT ReinitSuperSampleBuffers(ID3D11Device* pd3dDevice)
{
	HRESULT hr(S_OK);
	ID3D11RenderTargetView *pRTV = DXUTGetD3D11RenderTargetView();
	ID3D11Resource *pRTVResource;
	pRTV->GetResource(&pRTVResource);
	ID3D11Texture2D *pRTVTex2D = static_cast<ID3D11Texture2D*>(pRTVResource);
	assert(pRTVTex2D);
	D3D11_TEXTURE2D_DESC pRTVTex2DDesc;
	pRTVTex2D->GetDesc(&pRTVTex2DDesc);
	pRTVResource->Release();    
	g_pRTVTex2DDesc = pRTVTex2DDesc;

	//supersampling buffer and RTV along with an additional resource to resolve to and read from 
	SAFE_RELEASE(g_pSceneColorBuffer2XMS);
	SAFE_RELEASE(g_pSceneColorBuffer2X);
	SAFE_RELEASE(g_pSceneRTV2X);

	//supersampling depth buffer
	SAFE_RELEASE(g_pSceneDepthBuffer2X);
	SAFE_RELEASE(g_pSceneDSV2X);


	CreateScreenQuadVB(pd3dDevice);
	CreateSuperSampleScreenQuadVB(pd3dDevice);


	D3D11_TEXTURE2D_DESC descTex;
	D3D11_RENDER_TARGET_VIEW_DESC descRTV;


	//create the color super sample buffer----------------------------------------------------------
	//textures
	descTex.ArraySize = 1;
	descTex.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	descTex.CPUAccessFlags = 0;
	descTex.MipLevels = 1;
	descTex.MiscFlags = 0;
	//descTex.SampleDesc = pRTVTex2DDesc.SampleDesc;
	descTex.SampleDesc.Count = 8; 
	descTex.SampleDesc.Quality = 0;  
	descTex.Usage = D3D11_USAGE_DEFAULT;	
	descTex.Width = g_Width*g_SSFACTOR;
	descTex.Height = g_Height*g_SSFACTOR;
	descTex.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	V_RETURN(pd3dDevice->CreateTexture2D(&descTex,NULL,&g_pSceneColorBuffer2XMS));

	//also create a non-MS texture to resolve to
	descTex.SampleDesc.Count = 1;
	descTex.SampleDesc.Quality = 0;
	descTex.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	V_RETURN(pd3dDevice->CreateTexture2D(&descTex,NULL,&g_pSceneColorBuffer2X));


	//render target view
	descRTV.Format = descTex.Format;
	descRTV.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
	descRTV.Texture2D.MipSlice = 0;
	V_RETURN( pd3dDevice->CreateRenderTargetView( g_pSceneColorBuffer2XMS, &descRTV, &g_pSceneRTV2X ) );

	//create shader resource view
	D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
	viewDesc.Format = descTex.Format;
	viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	viewDesc.Texture2D.MostDetailedMip = 0;
	viewDesc.Texture2D.MipLevels = 1;
	V_RETURN( pd3dDevice->CreateShaderResourceView( g_pSceneColorBuffer2X, &viewDesc, &g_pSceneColorBufferShaderResourceView) );



	//create the depth super sample buffer----------------------------------------------------------
	descTex.ArraySize = 1;
	descTex.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descTex.CPUAccessFlags = 0;
	descTex.MipLevels = 1;
	descTex.MiscFlags = 0;
	//descTex.SampleDesc = pRTVTex2DDesc.SampleDesc;
	descTex.SampleDesc.Count = 8; 
	descTex.SampleDesc.Quality = 0;  
	descTex.Usage = D3D11_USAGE_DEFAULT;
	descTex.Width = g_Width*g_SSFACTOR;
	descTex.Height = g_Height*g_SSFACTOR;
	descTex.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	V_RETURN(pd3dDevice->CreateTexture2D(&descTex,NULL,&g_pSceneDepthBuffer2X));
	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	descDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; 
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
	descDSV.Flags = 0;
	V_RETURN( pd3dDevice->CreateDepthStencilView( g_pSceneDepthBuffer2X, &descDSV, &g_pSceneDSV2X ) );


	return hr;
}



//--------------------------------------------------------------------------------------
// Initialize fluid and voxelizer states
//--------------------------------------------------------------------------------------
HRESULT InitializeFluidState(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext)
{
	HRESULT hr(S_OK);

	SAFE_DELETE(g_fluid);

	// Initialize fluid state
	g_fluid = new Fluid(pd3dDevice, pd3dContext);
	if( !g_fluid ) 
		return E_OUTOFMEMORY;

	V_RETURN(g_fluid->Initialize(g_fluidGridWidth, g_fluidGridHeight, g_fluidGridDepth, Fluid::FT_SMOKE));

	g_fluid->SetNumJacobiIterations(2);
	g_fluid->SetUseMACCORMACK(false);
	g_fluid->SetMouseDown(true);

	// Create the shader resource view for velocity
	SAFE_ACQUIRE(g_pVelTexture3D, g_fluid->Get3DTexture(Fluid::RENDER_TARGET_VELOCITY0));
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	ZeroMemory( &SRVDesc, sizeof(SRVDesc) );
	SRVDesc.Format = g_fluid->GetFormat(Fluid::RENDER_TARGET_VELOCITY0);
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	SRVDesc.Texture3D.MipLevels = 1;
	SRVDesc.Texture3D.MostDetailedMip = 0;
	V_RETURN(pd3dDevice->CreateShaderResourceView( g_pVelTexture3D, &SRVDesc, &g_FluidVelocityRV));
	

	//pass off the obstacles texture to the Hair grid, which is going to write obstacles into this
	g_HairGrid->setFluidObstacleTexture(g_fluid->Get3DTexture(Fluid::RENDER_TARGET_OBSTACLES),
		g_fluid->GetFormat(Fluid::RENDER_TARGET_OBSTACLES),
		g_fluidGridWidth,g_fluidGridHeight,g_fluidGridDepth);

	return hr;
}

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
	HRESULT hr;

    g_MasterStrandVB[0] = NULL;
    g_MasterStrandVB[1] = NULL;
    g_ForceVB[0] = NULL;
    g_ForceVB[1] = NULL;
    g_CoordinateFrameVB[0] = NULL;
    g_CoordinateFrameVB[1] = NULL;
    g_MasterStrandRV[0] = NULL;
    g_MasterStrandRV[1] = NULL;
    g_ForceRV[0] = NULL;
    g_ForceRV[1] = NULL;
    g_CoordinateFrameRV[0] = NULL;
    g_CoordinateFrameRV[1] = NULL;
    g_HairBaseRV[0] = NULL;
    g_HairBaseRV[1] = NULL;
    g_HairBaseRV[2] = NULL;

	ID3D11DeviceContext* pd3dContext = DXUTGetD3D11DeviceContext();
	V_RETURN(g_DialogResourceManager.OnD3D11CreateDevice(pd3dDevice, pd3dContext));
	V_RETURN(g_D3DSettingsDlg.OnD3D11CreateDevice(pd3dDevice));
#if 0
	V_RETURN(D3DX11CreateFont(pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET, 
		OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, 
		L"Arial", &g_pFont10));
	V_RETURN(D3DX11CreateSprite(pd3dDevice, 512, &g_pSprite11));
#endif
	g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dContext, &g_DialogResourceManager, 15 );
	if(!g_pTxtHelper)
		return E_OUTOFMEMORY;

	g_firstFrame = true;
	g_firstMovedFrame = true;

	g_Center = D3DXVECTOR3(0, 0, 0);
	D3DXMatrixIdentity(&g_Transform);
	D3DXMatrixIdentity(&g_TotalTransform);
    D3DXMatrixIdentity(&g_RootTransform);

	g_UseTransformations = false;

	sprintf_s(g_loadDirectory,MAX_PATH,"%s",MayaHairDirectory);

	InitEffect(pd3dDevice, pd3dContext);

	// Setup the camera's view parameters
	D3DXVECTOR3 eye, at;
	eye = D3DXVECTOR3(0.0f, -1.0f, -15.0f); 
	at = D3DXVECTOR3(0, 0, 0);

	g_Camera.SetViewParams(&eye, &at);
	g_Camera.SetModelCenter( at );
	g_Camera.SetEnablePositionMovement(true);
	g_Camera.SetScalers(0.001f,20.f);

	D3DXVECTOR3 lightPos = D3DXVECTOR3(4.7908502f,18.887268f,-9.7619190f);//D3DXVECTOR3(-5,10.0f,10.0f);

	g_HairShadows.GetCamera().SetViewParams(&lightPos, &at);
	g_HairShadows.GetCamera().SetRadius(30);

	g_ExternalCameraView = *g_Camera.GetViewMatrix();


	V_RETURN(LoadScalp(pd3dDevice));
	LoadMayaHair(g_loadDirectory, g_useShortHair);
	LoadCollisionObstacles(pd3dDevice);

	changeCameraAndWindPresets();


	V_RETURN(CreateResources(pd3dDevice, pd3dContext));

	V_RETURN(CDXUTDirectionWidget::StaticOnD3D11CreateDevice(pd3dDevice, DXUTGetD3D11DeviceContext()));

	g_Effect->GetVariableByName("g_TessellatedMasterStrandLengthMax")->AsScalar()->SetInt(g_TessellatedMasterStrandLengthMax);
	g_Effect->GetVariableByName("g_NumTotalWisps")->AsScalar()->SetInt(g_NumWisps);
	g_Effect->GetVariableByName("g_maxLengthToRoot")->AsScalar()->SetFloat(g_maxHairLength);


	//initialize grid
	SAFE_DELETE(g_HairGrid);
	g_HairGrid = new HairGrid(pd3dDevice, pd3dContext);
	if( !g_HairGrid ) 
		return E_OUTOFMEMORY;
	V_RETURN(g_HairGrid->Initialize(g_gridSize ,g_gridSize ,g_gridSize ,g_Effect, g_pEffectPath));


	//Initialize fluid state
	InitializeFluidState(pd3dDevice, pd3dContext);

	//load the mesh for the arrow---------------------------------------------------------------------

	// Create the input layout
	const D3D11_INPUT_ELEMENT_DESC layoutArrow[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	D3DX11_PASS_DESC PassDesc;
	UINT numElements = sizeof(layoutArrow)/sizeof(layoutArrow[0]);
	g_pTechniqueRenderArrow->GetPassByIndex( 0 )->GetDesc( &PassDesc );
	V_RETURN( pd3dDevice->CreateInputLayout( layoutArrow, numElements, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &g_pVertexLayoutArrow ) );

	WCHAR str[MAX_PATH];
	V_RETURN( NVUTFindDXSDKMediaFileCch( str, MAX_PATH, L"arrow.sdkmesh" ) ); 
	V_RETURN( g_MeshArrow.Create( pd3dDevice, str) );

	//-------------------------------------------------------------------------------------------------
	V_RETURN(CreatePlane(pd3dDevice,GROUND_PLANE_RADIUS,-10.0f));

	g_HairShadows.OnD3D11CreateDevice(pd3dDevice, DXUTGetD3D11DeviceContext());

	//initialize the hair shading presets
	{
    //blond hair
	float baseColor[4] = {111/255.0f,77/255.0f,42/255.0f,1};
	float specColor[4] = {111/255.0f,77/255.0f,42/255.0f,1};
	float ksP = 0.4f;
	float ksS = 0.3f;
	float kd = 1.0f;
	float specPowerPrimary = 6.0f;
	float specPowerSecondary = 6.0f;    
	float ksP_sparkles = 0.2f;
	float specPowerPrimarySparkles = 600;
	float ka = 0.3f;
    hairShadingParams[0].assignValues(baseColor,specColor,ksP,ksS,kd,specPowerPrimary,specPowerSecondary,ksP_sparkles,specPowerPrimarySparkles,ka);
	}
	{
    //brown hair
	float baseColor[4] = {72.0f/255.0f,51.0f/255.0f,43.0f/255.0f,1.0f};
	float specColor[4] = {132.0f/255.0f,106.0f/255.0f,79.0f/255.0f,1};
    float ksP = 0.3f;
    float ksS = 0.4f;
    float kd = 0.6f;
    float specPowerPrimary = 30.0f;
    float specPowerSecondary = 15.0f;    
    float ksP_sparkles = 0.2f;
    float specPowerPrimarySparkles = 600;
    float ka = 0.2f;
	hairShadingParams[1].assignValues(baseColor,specColor,ksP,ksS,kd,specPowerPrimary,specPowerSecondary,ksP_sparkles,specPowerPrimarySparkles,ka);
	}
	{
    //red hair
	float baseColor[4] = {81/255.0f,41/255.0f,13/255.0f,1};
	float specColor[4] = {255/255.0f,75/255.0f,4/255.0f,1};
    float ksP = 0.4f;
    float ksS = 0.4f;
    float kd = 0.8f;
    float specPowerPrimary = 60.0f;
    float specPowerSecondary = 15.0f;    
    float ksP_sparkles = 0.1f;
    float specPowerPrimarySparkles = 600;
    float ka = 0.2f;
    hairShadingParams[2].assignValues(baseColor,specColor,ksP,ksS,kd,specPowerPrimary,specPowerSecondary,ksP_sparkles,specPowerPrimarySparkles,ka);
	}
	hairShadingParams[g_currHairShadingParams].setShaderVariables(g_Effect);
    g_Effect->GetVariableByName("hairTexture")->AsShaderResource()->SetResource( g_HairBaseRV[g_currHairShadingParams] );

	D3D11_SAMPLER_DESC desc2[1] = { 
		D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT, 
		D3D11_TEXTURE_ADDRESS_CLAMP, 
		D3D11_TEXTURE_ADDRESS_CLAMP, 
		D3D11_TEXTURE_ADDRESS_CLAMP, 
		0.0, 0, D3D11_COMPARISON_NEVER, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f,
	};
	pd3dDevice->CreateSamplerState(desc2, &g_pLinearClampSampler);
	

	//saveHair();

	//delete all the data we were carying around
	delete[] g_clumpCoordinates;
	g_clumpCoordinates = NULL;
	delete[] g_randStrandDeviations;
	g_randStrandDeviations = NULL;
	delete[] g_masterStrandLengthToRoot;
	g_masterStrandLengthToRoot = NULL;
	delete[] g_masterStrandLengths;
	g_masterStrandLengths = NULL;
	delete[] g_Strand_Sizes;  
	g_Strand_Sizes = NULL;
	delete[] g_tangentJitter;   
	g_tangentJitter = NULL;
	delete[] g_BarycentricCoordinates;  
	g_BarycentricCoordinates = NULL;

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11SwapChainResized(ID3D11Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
	HRESULT hr = S_OK;

	V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
	V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

	g_Width = pBackBufferSurfaceDesc->Width;
	g_Height = pBackBufferSurfaceDesc->Height;

	g_superSampleViewport.TopLeftX = 0;
	g_superSampleViewport.TopLeftY = 0;
	g_superSampleViewport.MinDepth = 0;
	g_superSampleViewport.MaxDepth = 1;
	g_superSampleViewport.Width = g_Width*g_SSFACTOR;  
	g_superSampleViewport.Height = g_Height*g_SSFACTOR; 

	V(g_Effect->GetVariableByName("g_ScreenWidth")->AsScalar()->SetFloat(g_Width));
	V(g_Effect->GetVariableByName("g_ScreenHeight")->AsScalar()->SetFloat(g_Height));

	// Setup the camera's projection parameters
	float fAspectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
	g_Camera.SetProjParams(D3DX_PI/3, fAspectRatio, 0.1f, 5000.0f);
	g_Camera.SetWindow(pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height);
	g_HairShadows.GetCamera().SetWindow(pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height);
	g_Camera.SetButtonMasks(NULL, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );

#ifdef SUPERSAMPLE
	V_RETURN(ReinitSuperSampleBuffers(pd3dDevice));
#endif

	g_HUD.SetLocation(pBackBufferSurfaceDesc->Width-180, 0);
	g_HUD.SetSize(170, 170);
	g_SampleUI.SetLocation(pBackBufferSurfaceDesc->Width-180, pBackBufferSurfaceDesc->Height-530);
	g_SampleUI.SetSize(170, 300);

	return hr;
}



void RenderArrow(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext, D3DXVECTOR3 pos)
{
	HRESULT hr;
	D3DXVECTOR3 g_modelCentroid = g_Center;

	//calculate and set the world view projection matrix for transforming the arrow
	float arrowScale = 0.03f;
	D3DXMATRIX mScale;
	D3DXMatrixScaling( &mScale, arrowScale, arrowScale, arrowScale);
	D3DXMATRIX mLookAtMatrix;
	D3DXVECTOR3 mArrowPos =  g_modelCentroid - pos;
	D3DXVECTOR3 mOrigin = g_Center;
	D3DXVECTOR3 mUp = D3DXVECTOR3(0, 1, 0);
	D3DXMatrixLookAtLH(&mLookAtMatrix, &mArrowPos, &mOrigin, &mUp);
	D3DXMATRIX mLookAtInv;
	D3DXMatrixInverse(&mLookAtInv, NULL, &mLookAtMatrix);
	D3DXMATRIX mWorldView,mWorldViewProj;
	D3DXMATRIX mWorld = mScale * mLookAtInv ;
	if(g_useSkinnedCam)
		D3DXMatrixMultiply( &mWorldView, &mWorld, &g_ExternalCameraView);
	else
		D3DXMatrixMultiply( &mWorldView, &mWorld, g_Camera.GetViewMatrix());
	D3DXMatrixMultiply( &mWorldViewProj, &mWorldView, g_Camera.GetProjMatrix() );
	V(g_Effect->GetVariableByName("ViewProjection")->AsMatrix()->SetMatrix(mWorldViewProj));

	//render the arrow
	pd3dContext->IASetInputLayout(g_pVertexLayoutArrow);
	g_pTechniqueRenderArrow->GetPassByIndex(0)->Apply(0, pd3dContext);
	g_MeshArrow.Render(pd3dContext);
}

HRESULT RepropagateCoordinateFrames(ID3D11DeviceContext* pd3dContext, int iterations)
{ 
	HRESULT hr= S_OK;

	g_Effect->GetVariableByName("g_blendAxis")->AsScalar()->SetFloat(g_blendAxis);
	RecreateCoordinateFrames(pd3dContext);

	unsigned int stride = 3*4*sizeof(float);     
	unsigned int offset = 0;
	pd3dContext->IASetInputLayout(g_CoordinateFrameIL);
	V_RETURN(g_Effect->GetVariableByName("g_MasterStrand")->AsShaderResource()->SetResource(g_MasterStrandRV[g_currentVB]));
	V_RETURN(g_Effect->GetVariableByName("g_VertexNumber")->AsShaderResource()->SetResource(g_MasterStrandLocalVertexNumberRV));

	for(int i=0;i<iterations;i++)
	{
		g_Effect->GetVariableByName("g_coordinateFrames")->AsShaderResource()->SetResource(g_CoordinateFrameRV[g_currentVBCoordinateFrame]);
		g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("PropagateCoordinateFrame")->Apply(0, pd3dContext);
		pd3dContext->IASetVertexBuffers(0, 1, &g_CoordinateFrameVB[g_currentVBCoordinateFrame], &stride, &offset);
		pd3dContext->SOSetTargets(1, &g_CoordinateFrameVB[(g_currentVBCoordinateFrame+1)%2], &offset);
		pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
		pd3dContext->Draw(g_NumMasterStrandControlVertices, 0);
		pd3dContext->SOSetTargets(0, 0, 0);
		g_Effect->GetVariableByName("g_coordinateFrames")->AsShaderResource()->SetResource(NULL);
		g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("PropagateCoordinateFrame")->Apply(0, pd3dContext);
	}

	pd3dContext->SOSetTargets(0, 0, 0);
	g_Effect->GetVariableByName("g_MasterStrand")->AsShaderResource()->SetResource(NULL);
	g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("PropagateCoordinateFrame")->Apply(0, pd3dContext);
	g_currentVBCoordinateFrame = (g_currentVBCoordinateFrame+1)%2;

	return hr;

}


void RecreateCoordinateFrames(ID3D11DeviceContext* pd3dContext)
{
	unsigned int stride = 3*4*sizeof(float);     
	unsigned int offset = 0;
	pd3dContext->IASetInputLayout(g_CoordinateFrameIL);

	pd3dContext->SOSetTargets(0, 0, 0);
	g_Effect->GetVariableByName("g_MasterStrand")->AsShaderResource()->SetResource(g_MasterStrandRV[g_currentVB]);
	//g_Effect->GetVariableByName("g_coordinateFrames")->AsShaderResource()->SetResource(NULL);
	g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("UpdateCoordinateFrame")->Apply(0, pd3dContext);

	pd3dContext->IASetVertexBuffers(0, 1, &g_CoordinateFrameVB[g_currentVBCoordinateFrame], &stride, &offset);
	//g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("UpdateCoordinateFrame")->Apply(0, pd3dContext);
	pd3dContext->SOSetTargets(1, &g_CoordinateFrameVB[(g_currentVBCoordinateFrame+1)%2], &offset);
	pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	pd3dContext->Draw(g_NumMasterStrandControlVertices, 0);

	pd3dContext->SOSetTargets(0, 0, 0);
	g_Effect->GetVariableByName("g_MasterStrand")->AsShaderResource()->SetResource(NULL);
	g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("UpdateCoordinateFrame")->Apply(0, pd3dContext);
	g_currentVBCoordinateFrame = (g_currentVBCoordinateFrame+1)%2;

}

void VisualizeCoordinateFrames(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext, bool drawTessellatedCFs)
{
	unsigned int stride =  3*4*sizeof(float);        
	unsigned int offset = 0;
	pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
	pd3dContext->IASetInputLayout(g_CoordinateFrameIL);	
	if(drawTessellatedCFs)
	{   
		//set variables
		g_Effect->GetVariableByName("g_TessellatedMasterStrand")->AsShaderResource()->SetResource(g_TessellatedMasterStrandRV);
		g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("RenderCoordinateFrameTessellated")->Apply(0, pd3dContext);

		pd3dContext->IASetVertexBuffers(0, 1, &g_TessellatedCoordinateFrameVB, &stride, &offset);
		pd3dContext->Draw(g_NumTessellatedMasterStrandVertices, 0); 

		//unset variables
		g_Effect->GetVariableByName("g_TessellatedMasterStrand")->AsShaderResource()->SetResource(NULL);
		g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("RenderCoordinateFrameTessellated")->Apply(0, pd3dContext);

	}
	else    
	{
        if(g_useCS)
		{
		    g_Effect->GetVariableByName("g_MasterStrandSB")->AsShaderResource()->SetResource(g_MasterStrandUAB_SRV);
			g_Effect->GetVariableByName("g_coordinateFramesSB")->AsShaderResource()->SetResource(g_CoordinateFrameUAB_SRV);
		    g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("RenderCoordinateFrameUnTessellatedSB")->Apply(0, pd3dContext);
		}
		else
		{
		    g_Effect->GetVariableByName("g_MasterStrand")->AsShaderResource()->SetResource(g_MasterStrandRV[g_currentVB]);
		    g_Effect->GetVariableByName("g_coordinateFrames")->AsShaderResource()->SetResource(g_CoordinateFrameRV[g_currentVBCoordinateFrame]);
		    g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("RenderCoordinateFrameUnTessellated")->Apply(0, pd3dContext);
    	}
	
		unsigned int stride = 0;
	    unsigned int offset = 0;
	    ID3D11Buffer* buffer[] = { NULL };
    	pd3dContext->IASetVertexBuffers(0, 1, buffer, &stride, &offset);
	    pd3dContext->IASetInputLayout(NULL);

		pd3dContext->Draw(g_NumMasterStrandControlVertices, 0); 

        if(g_useCS)
		{
		    g_Effect->GetVariableByName("g_MasterStrandSB")->AsShaderResource()->SetResource(NULL);
			g_Effect->GetVariableByName("g_coordinateFramesSB")->AsShaderResource()->SetResource(NULL);
		    g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("RenderCoordinateFrameUnTessellatedSB")->Apply(0, pd3dContext);
		}
		else
		{
		    g_Effect->GetVariableByName("g_MasterStrand")->AsShaderResource()->SetResource(NULL);
		    g_Effect->GetVariableByName("g_coordinateFrames")->AsShaderResource()->SetResource(NULL);
		    g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("RenderCoordinateFrameUnTessellated")->Apply(0, pd3dContext);
    	}
	}

}



HRESULT renderInstancedInterpolatedHairForGrid(D3DXMATRIX& worldViewProjection,ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext, float ZMin, float ZStep, int numRTs )
{
	HRESULT hr = S_OK;

	g_Effect->GetVariableByName("WorldViewProjection")->AsMatrix()->SetMatrix(worldViewProjection);
	g_Effect->GetVariableByName("g_StrandWidthMultiplier")->AsScalar()->SetFloat(25); 
	D3DXVECTOR3 camPosition(1,0,0);
	g_Effect->GetVariableByName("EyePosition")->AsVector()->SetFloatVector(camPosition);
	g_Effect->GetVariableByName("g_gridZMin")->AsScalar()->SetFloat(ZMin);
	g_Effect->GetVariableByName("g_gridZStep")->AsScalar()->SetFloat(ZStep);
	g_Effect->GetVariableByName("g_gridNumRTs")->AsScalar()->SetInt(numRTs);

	unsigned int stride = 0;
	unsigned int offset = 0;
	ID3D11Buffer* buffer[] = { NULL };
	pd3dContext->IASetVertexBuffers( 0, 1, buffer, &stride, &offset );
	pd3dContext->IASetInputLayout(NULL);

	pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
	g_Effect->GetVariableByName("g_MasterStrandLengthsUntessellated")->AsShaderResource()->SetResource(g_MasterLengthsRVUntessellated);
	g_Effect->GetVariableByName("g_MasterStrandRootIndicesUntessellated")->AsShaderResource()->SetResource(g_RootsIndicesRVUntessellated); 
	g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedVSForGrid3DTextureMRT")->Apply(0, pd3dContext);

	pd3dContext->DrawInstanced(g_MasterStrandLengthMax,g_NumWisps,0,0); 

	g_Effect->GetVariableByName("g_MasterStrandLengthsUntessellated")->AsShaderResource()->SetResource(NULL);
	g_Effect->GetVariableByName("g_MasterStrandRootIndicesUntessellated")->AsShaderResource()->SetResource(NULL); 
	g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("InterpolateInstancedVSForGrid3DTextureMRT")->Apply(0, pd3dContext);

	return hr;
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
	g_pTxtHelper->Begin();
	g_pTxtHelper->SetInsertionPos(2, 0);
	g_pTxtHelper->SetForegroundColor(D3DXCOLOR(1.0f, 1.0f, 0.0f, 1.0f));
	g_pTxtHelper->DrawTextLine(DXUTGetFrameStats(true));
	g_pTxtHelper->DrawTextLine(DXUTGetDeviceStats());
    g_pTxtHelper->SetForegroundColor(D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f));
	g_pTxtHelper->DrawTextLine(L"Press 'h' for help");


    if(g_printHelp)
	{
     	g_pTxtHelper->DrawTextLine(L" ");
	    g_pTxtHelper->DrawTextLine(L"Left mouse drag: Move Camera");
	    g_pTxtHelper->DrawTextLine(L"Right mouse drag: Move Model");
		g_pTxtHelper->DrawTextLine(L"Midddle mouse drag: Rotate Model");
	    g_pTxtHelper->DrawTextLine(L"Left mouse drag + SHIFT: Move Light");
	    g_pTxtHelper->DrawTextLine(L"Right mouse drag + SHIFT: Move Wind");
	}

	g_pTxtHelper->SetForegroundColor(D3DXCOLOR(1.0f, 0.2f, 0.0f, 1.0f));
	if(g_playTransforms)
		g_pTxtHelper->DrawTextLine(L"\n\nPlaying recorded transforms from file, press 'p' to stop");
	if(g_recordTransforms)
		g_pTxtHelper->DrawTextLine(L"\n\nRecording input transforms to file");
	if(g_applyHForce)
		g_pTxtHelper->DrawTextLine(L"\n\nApplying wind force, press 'k' to stop");

	if(g_useSkinnedCam)
		g_pTxtHelper->DrawTextLine(L"\n\nUsing recorded camera. Press 'c' to stop");

	g_pTxtHelper->End();
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11SwapChainReleasing(void* pUserContext)
{
	g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice(void* pUserContext)
{
	g_DialogResourceManager.OnD3D11DestroyDevice();
	g_D3DSettingsDlg.OnD3D11DestroyDevice();
	CDXUTDirectionWidget::StaticOnD3D11DestroyDevice();

	ReleaseResources();

	//gui stuff
	SAFE_RELEASE(g_pFont10);
	SAFE_RELEASE(g_pSprite10);
	SAFE_DELETE( g_pTxtHelper );
	SAFE_RELEASE(g_Effect);

	SAFE_DELETE(g_fluid);
	SAFE_RELEASE(g_pVelTexture3D);
	SAFE_RELEASE(g_FluidVelocityRV);
	SAFE_DELETE(g_HairGrid);

	g_Sphere.Destroy();

	SAFE_RELEASE(g_pLinearClampSampler);

	g_MeshArrow.Destroy();
	SAFE_RELEASE( g_pVertexLayoutArrow );
	g_HairShadows.OnD3D11DestroyDevice();

	//mesh
	if(g_indices!=NULL)
		delete[] g_indices;
	g_indices = NULL;

	SAFE_DELETE(g_Scalp);
	g_Scalp = NULL;

	SAFE_DELETE(g_FaceMesh);
    g_FaceMesh = NULL;

	SAFE_RELEASE(g_RenderMeshIL);

    //the ground plane
    SAFE_RELEASE(g_pPlaneVertexBuffer);
    SAFE_RELEASE(g_pPlaneIndexBuffer);
    SAFE_RELEASE(g_pPlaneVertexLayout);

	g_hairReset = true;
}


//--------------------------------------------------------------------------------------
// Load scalp 
//--------------------------------------------------------------------------------------
HRESULT LoadScalp(ID3D11Device* pd3dDevice)
{
	HRESULT hr;
	static D3D11_INPUT_ELEMENT_DESC elements[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 }, 
	};
	D3DX11_PASS_DESC passDesc;    
	V_RETURN(g_Effect->GetTechniqueByName("RenderMesh")->GetPassByIndex(0)->GetDesc(&passDesc));
	V_RETURN(pd3dDevice->CreateInputLayout(elements, sizeof(elements) / sizeof(D3D11_INPUT_ELEMENT_DESC), passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &g_RenderMeshIL));       

	WCHAR xModelFile[MAX_PATH];
	char filename[MAX_PATH];
	sprintf_s(filename,100,"%s\\%s",g_loadDirectory,"scalp_mesh.sdkmesh");
	mbstowcs_s(NULL, xModelFile, MAX_PATH, filename, MAX_PATH);
	g_Scalp = new CDXUTSDKMesh();
	V_RETURN(g_Scalp->Create(pd3dDevice, xModelFile));

    //load the unit sphere to display the collision objects with
	V_RETURN(g_Sphere.Create(pd3dDevice, L"unitSphere.sdkmesh"));

	sprintf_s(filename,100,"%s\\%s",g_loadDirectory,"Face.sdkmesh");
	mbstowcs_s(NULL, xModelFile, MAX_PATH, filename, MAX_PATH);
	g_FaceMesh = new CDXUTSDKMesh();
	V_RETURN(g_FaceMesh->Create(pd3dDevice, xModelFile));
	

	return S_OK;
}


//--------------------------------------------------------------------------------------
// Load the Collision Obstacles
// this file also contains the transforms from hair base pose to mesh initial pose, 
// and from hair base pose to mesh bind pose
//--------------------------------------------------------------------------------------

HRESULT LoadCollisionObstacles(ID3D11Device* pd3dDevice)
{
	HRESULT hr = S_OK;


	TCHAR CollisionImplicitsFile[MAX_PATH];
	WCHAR wfilename[MAX_PATH];
	char collisionFile[MAX_PATH];
	sprintf_s(collisionFile,100,"%s\\%s",g_loadDirectory,"collision_objects.txt");
	mbstowcs_s(NULL, wfilename, MAX_PATH, collisionFile, MAX_PATH);
	V(NVUTFindDXSDKMediaFileCchT( CollisionImplicitsFile, MAX_PATH,wfilename));



	collisionObjectTransforms.clear();
	collisionObjectTransforms.reserve(MAX_IMPLICITS*2);

	sphereIsBarycentricObstacle.clear();
	sphereIsBarycentricObstacle.reserve(MAX_IMPLICITS*2);
	cylinderIsBarycentricObstacle.clear();
	cylinderIsBarycentricObstacle.reserve(MAX_IMPLICITS*2);
	sphereNoMoveIsBarycentricObstacle.clear();
	sphereNoMoveIsBarycentricObstacle.reserve(MAX_IMPLICITS*2);


	//read the collisions file ------------------------------------------------------------------------------
	ifstream infileCollisions;
	infileCollisions.open(CollisionImplicitsFile);
	IMPLICIT_TYPE type;

	g_numSpheres = 0;
	g_numCylinders = 0;
	g_numSpheresNoMoveConstraint = 0;

	char c[MAX_PATH];


	if (infileCollisions) 
	{
		while(!infileCollisions.eof())
		{      
			type = NOT_AN_IMPLICIT;
			infileCollisions.getline(c,MAX_PATH);
			string s(c);
			string::size_type locComment = s.find("//",0);
			string::size_type locHash = s.find("#",0);

			if( locComment != string::npos && locHash != 0 )//if line is not part of the header, and it is the beginning of an implicit
			{
				//read the type of implicit
				if( s.find("'s'",0) != string::npos && s.find("'NoMoveConstraint'",0) != string::npos )
					type = SPHERE_NO_MOVE_CONSTRAINT;	
				else if( s.find("'s'",0) != string::npos )
					type = SPHERE;
				else if( s.find("'c'",0) != string::npos )
					type = CYLINDER;        
				else if( ( s.find("'scalpToMesh'",0) != string::npos ))
				{
					//read the scalpToMesh matrix
					float scalpToMeshMatrix[16];
					infileCollisions.getline(c,MAX_PATH);
					string stringvalues(c);
					istringstream valuesString(stringvalues);
					for(int i=0;i<16;i++)
						valuesString>>scalpToMeshMatrix[i];

					g_ScalpToMesh = D3DXMATRIX(scalpToMeshMatrix);
				}
				else if( ( s.find("'InitialTotalTransform'",0) != string::npos )) 
				{
					//read the g_InitialTotalTransform matrix
					float initialTransform[16];
					infileCollisions.getline(c,MAX_PATH);
					string stringvalues(c);
					istringstream valuesString(stringvalues);
					for(int i=0;i<16;i++)
						valuesString>>initialTransform[i];
					g_InitialTotalTransform = D3DXMATRIX(initialTransform);
				}

				if(type == SPHERE || type == CYLINDER || type == SPHERE_NO_MOVE_CONSTRAINT)
				{
					//read the values
					infileCollisions.getline(c,MAX_PATH);
					string valuesString(c);

					//see if this is a barycentric obstacle
					bool isBarycentricObstacle = false;
					if(valuesString.find("'barycentricObstacle'",0) != string::npos)
					{
						isBarycentricObstacle = true;
						infileCollisions.getline(c,MAX_PATH);
						valuesString = string(c);
					}

					//see if there is a bone
					string boneName;
					unsigned int posBone = (unsigned int) valuesString.find("'bone'",0);
					if( posBone != string::npos)
					{
						//read the name of the bone
						posBone += 7; //number of letters in 'bone' followed by space  
						boneName = valuesString.substr(posBone, valuesString.length()-posBone);

						//read the next line 
						infileCollisions.getline(c,MAX_PATH);
						valuesString = string(c);
					}

					//see if there is a basePoseToMeshBind transform
					D3DXMATRIX basePoseToMeshBind;
					D3DXMatrixIdentity(&basePoseToMeshBind);
					if( valuesString.find("'basePoseToMeshBind'",0) != string::npos )
					{
						infileCollisions.getline(c,MAX_PATH);
						valuesString = string(c);
						istringstream valuesIString(valuesString);
						float transform[16];
						for(int i=0;i<16;i++)
							valuesIString>>transform[i];
						basePoseToMeshBind = D3DXMATRIX(transform);	

						//read the next line 
						infileCollisions.getline(c,MAX_PATH);
						valuesString = string(c);
					}


					//scale
					collisionImplicit imp;
					istringstream valuesIString(valuesString);
					valuesIString>>imp.scale.x>>imp.scale.y>>imp.scale.z;
					//rotation
					infileCollisions.getline(c,MAX_PATH);
					valuesString = string(c);
					valuesIString.clear();
					valuesIString.str(valuesString);
					valuesIString>>imp.rotation.x>>imp.rotation.y>>imp.rotation.z; 
					//translation
					infileCollisions.getline(c,MAX_PATH);
					valuesString = string(c);
					valuesIString.clear();
					valuesIString.str(valuesString);
					valuesIString>>imp.center.x>>imp.center.y>>imp.center.z; 


					// maya applies scale,then rotation and then translation
					// for rotation assuming maya does x then y then z
					D3DXMATRIX initialScale, initialRotationX, initialRotationY, initialRotationZ, initialTranslation, initalTransformation;

					D3DXMatrixScaling( &initialScale, imp.scale.x, imp.scale.y, imp.scale.z);
					D3DXMatrixRotationX(&initialRotationX, imp.rotation.x);
					D3DXMatrixRotationY(&initialRotationY, imp.rotation.y);
					D3DXMatrixRotationZ(&initialRotationZ, imp.rotation.z);
					D3DXMatrixTranslation( &initialTranslation, imp.center.x, imp.center.y, imp.center.z);

					D3DXMatrixMultiply( &initalTransformation, &initialScale, &initialRotationX );
					D3DXMatrixMultiply( &initalTransformation, &initalTransformation, &initialRotationY );
					D3DXMatrixMultiply( &initalTransformation, &initalTransformation, &initialRotationZ );
					D3DXMatrixMultiply( &initalTransformation, &initalTransformation, &initialTranslation );

					collisionObjectTransforms.push_back(collisionObject());

					if(type == SPHERE)
					{
						collisionObjectTransforms.back().implicitType = SPHERE;
						if(isBarycentricObstacle)
							sphereIsBarycentricObstacle.push_back(1);
						else
							sphereIsBarycentricObstacle.push_back(0);
						g_numSpheres++;						
					}
					else if(type == CYLINDER)
					{
						collisionObjectTransforms.back().implicitType = CYLINDER;  
						if(isBarycentricObstacle)
							cylinderIsBarycentricObstacle.push_back(1);
						else
							cylinderIsBarycentricObstacle.push_back(0);
						g_numCylinders++;
					}
					else if(type == SPHERE_NO_MOVE_CONSTRAINT)
					{
						collisionObjectTransforms.back().implicitType = SPHERE_NO_MOVE_CONSTRAINT;
						if(isBarycentricObstacle)
							sphereNoMoveIsBarycentricObstacle.push_back(1);
						else
							sphereNoMoveIsBarycentricObstacle.push_back(0);
						g_numSpheresNoMoveConstraint++;					
					}

					collisionObjectTransforms.back().InitialTransform = initalTransformation;
					collisionObjectTransforms.back().boneName = boneName;
					collisionObjectTransforms.back().objToMesh = basePoseToMeshBind;
					collisionObjectTransforms.back().isHead = false;
					D3DXMATRIX CurrrentTransform;
					D3DXMatrixIdentity(&CurrrentTransform);
					collisionObjectTransforms.back().currentTransform = CurrrentTransform;

					//find out if this is the head
					if( s.find("'head'",0) != string::npos )
					{
						collisionObjectTransforms.back().isHead = true;
						g_scalpMaxRadius = max(imp.scale.x,max(imp.scale.y,imp.scale.z));
						g_HairBoneName = boneName;
					}
				}
			}
		}
		infileCollisions.close();
	} 

	D3DXMatrixInverse(&g_InvInitialTotalTransform,NULL,&g_InitialTotalTransform);
	g_InvInitialRotationScale = g_InvInitialTotalTransform;
	g_InvInitialRotationScale._41 = 0;
	g_InvInitialRotationScale._42 = 0;
	g_InvInitialRotationScale._43 = 0;
	g_InvInitialRotationScale._44 = 1;   

	//transform the implicit by the inverse initial total transform, to get it into hair space
	for(int i=0;i<int(collisionObjectTransforms.size());i++)
	{

		D3DXMATRIX initalTransformation = collisionObjectTransforms.at(i).InitialTransform;
		D3DXMatrixMultiply( &initalTransformation, &initalTransformation, &g_InvInitialTotalTransform);
		collisionObjectTransforms.at(i).InitialTransform = initalTransformation;
	}

	//numbers
	g_Effect->GetVariableByName("g_NumSphereImplicits")->AsScalar()->SetInt(g_numSpheres);
	g_Effect->GetVariableByName("g_NumCylinderImplicits")->AsScalar()->SetInt(g_numCylinders);
	g_Effect->GetVariableByName("g_NumSphereNoMoveImplicits")->AsScalar()->SetInt(g_numSpheresNoMoveConstraint);

	g_Effect->GetVariableByName("g_UseSphereForBarycentricInterpolant")->AsScalar()->SetIntArray(&sphereIsBarycentricObstacle[0],0,g_numSpheres);
	if(g_numCylinders)
	g_Effect->GetVariableByName("g_UseCylinderForBarycentricInterpolant")->AsScalar()->SetIntArray(&cylinderIsBarycentricObstacle[0],0,g_numCylinders);
	if (g_numSpheresNoMoveConstraint > 0) g_Effect->GetVariableByName("g_UseSphereNoMoveForBarycentricInterpolant")->AsScalar()->SetIntArray(&sphereNoMoveIsBarycentricObstacle[0],0,g_numSpheresNoMoveConstraint);

	//set the transforms
	ExtractAndSetCollisionImplicitMatrices();

	g_MaxWorldScale = max(g_InitialTotalTransform._11,max(g_InitialTotalTransform._22,g_InitialTotalTransform._33));

	g_Effect->GetVariableByName("g_SigmaA")->AsScalar()->SetFloat(-g_SigmaA/g_MaxWorldScale);

	//transform from hair space to world
	g_maxHairLengthWorld = g_maxHairLength*g_MaxWorldScale;
	g_scalpMaxRadiusWorld = g_scalpMaxRadius;
	g_maxHairLengthHairSpace = g_maxHairLength;
	//transform from world to hair space
	g_scalpMaxRadiusHairSpace = g_scalpMaxRadius*max(g_InvInitialTotalTransform._11,max(g_InvInitialTotalTransform._22,g_InvInitialTotalTransform._33)); 

	return hr;   

}


//--------------------------------------------------------------------------------------
// Create resources 
//--------------------------------------------------------------------------------------
HRESULT CreateResources(ID3D11Device* pd3dDevice, ID3D11DeviceContext *pd3dContext)
{
	HRESULT hr;
	V_RETURN(CreateBuffers(pd3dDevice, pd3dContext));
	V_RETURN(CreateInputLayouts(pd3dDevice));
	return S_OK;
}

//--------------------------------------------------------------------------------------
// Create buffers 
//--------------------------------------------------------------------------------------
HRESULT CreateBuffers(ID3D11Device* pd3dDevice, ID3D11DeviceContext *pd3dContext)
{

	HRESULT hr = S_OK;
	V_RETURN(CreateMasterStrandSimulationIBs(pd3dDevice));
	V_RETURN(CreateMasterStrandVB(pd3dDevice));
	V_RETURN(CreateForceVB(pd3dDevice, pd3dContext));
	V_RETURN(CreateTessellatedMasterStrandVB(pd3dDevice));
	V_RETURN(CreateTessellatedWispVB(pd3dDevice));
	V_RETURN(CreateMasterStrandTesselatedInputVB(pd3dDevice));
	V_RETURN(CreateStrandColors(pd3dDevice));
	V_RETURN(CreateStrandLengths(pd3dDevice));
	V_RETURN(CreateDeviantStrandsAndStrandSizes(pd3dDevice));
	V_RETURN(CreateCurlDeviations(pd3dDevice));
	V_RETURN(CreateStrandBarycentricCoordinates(pd3dDevice));
	V_RETURN(CreateRandomCircularCoordinates(pd3dDevice));
	V_RETURN(CreateStrandAttributes(pd3dDevice));
	V_RETURN(CreateMasterStrandSpringLengthVBs(pd3dDevice));
	V_RETURN(CreateCoordinateFrameVB(pd3dDevice));
	V_RETURN(CreateTessellatedCoordinateFrameVB(pd3dDevice));
	V_RETURN(CreateCollisionDetectionResources(pd3dDevice));
	V_RETURN(CreateCollisionScreenQuadVB(pd3dDevice));
	V_RETURN(CreateTangentJitter(pd3dDevice));
	V_RETURN(CreateStrandToBarycentricIndexMap(pd3dDevice));
	V_RETURN(CreateStrandToClumpIndexMap(pd3dDevice));
	V_RETURN(CreateInterpolatedValuesVBs(pd3dDevice));
    V_RETURN(CreateComputeShaderConstantBuffers(pd3dDevice));

	return S_OK;
}

HRESULT CreateMasterStrandSimulationIBs(ID3D11Device* pd3dDevice)
{
	D3D11_SUBRESOURCE_DATA initialData;

	//linear springs --------------------------------------------------------------------------------------
	//indicesFirst :  (0,1) (2,3) (4,5) (6,6)
	//indicesSecond : (0,0) (1,2) (3,4) (5,6)

	//overestimate, since exact estimate would require going over all of g_MasterStrandControlVertexOffsets
	g_numIndicesFirst =  (g_NumMasterStrandControlVertices/2) + g_NumMasterStrands;
	g_numIndicesSecond = (g_NumMasterStrandControlVertices/2) + g_NumMasterStrands*2;

	//make two index buffers that batch up the control vertices into two groups
	short (*indicesFirst)[2] = new short[g_numIndicesFirst][2];
	short (*indicesSecond)[2] = new short[g_numIndicesSecond][2];

	int ind = 0;
	short v = 0;
	for (int s = 0; s < g_NumMasterStrands; ++s) 
	{
		for (; v < g_MasterStrandControlVertexOffsets[s]; ++ind, v += 2) 
		{
			indicesFirst[ind][0] = v;
			if(v+1 < g_MasterStrandControlVertexOffsets[s])
				indicesFirst[ind][1] = v + 1;
			else
				indicesFirst[ind][1] = v;            
		}
		v = g_MasterStrandControlVertexOffsets[s];
	}
	//chop off the part of this buffer that wasnt used
	g_numIndicesFirst = ind;


	ind = 0;
	v = 0;
	for (int s = 0; s < g_NumMasterStrands; ++s) 
	{   
		indicesSecond[ind][0] = v;
		indicesSecond[ind][1] = v;
		v++;
		ind++;
		for (; v < g_MasterStrandControlVertexOffsets[s]; ++ind, v += 2) 
		{
			indicesSecond[ind][0] = v;
			if(v+1 < g_MasterStrandControlVertexOffsets[s])
				indicesSecond[ind][1] = v + 1;
			else
				indicesSecond[ind][1] = v;
		}
		v = g_MasterStrandControlVertexOffsets[s];
	}
	g_numIndicesSecond = ind;


	//angular linear springs --------------------------------------------------------------------------------------
	//indicesFirstAngular :  (0,1,2,3) (4,5,6,6)
	//indicesSecondAngular : (0,1,1,1) (2,3,4,5) (6,6,6,6)

	//overestimate, since exact estimate would require going over all of g_MasterStrandControlVertexOffsets
	g_numIndicesFirstAngular =  (g_NumMasterStrandControlVertices/4) + g_NumMasterStrands;
	g_numIndicesSecondAngular = (g_NumMasterStrandControlVertices/4) + g_NumMasterStrands*2;

	//make two index buffers that batch up the control vertices into two groups
	short (*indicesFirstAngular)[4] = new short[g_numIndicesFirstAngular][4];
	short (*indicesSecondAngular)[4] = new short[g_numIndicesSecondAngular][4];

	ind = 0;
	v = 0;
	short index;
	for (int s = 0; s < g_NumMasterStrands; ++s) 
	{
		for (; v < g_MasterStrandControlVertexOffsets[s]; ++ind, v += 4) 
		{   
			index = v-1;
			for(int i=0;i<4;i++)
			{    
				if( v+i < g_MasterStrandControlVertexOffsets[s])
					index++; 
				indicesFirstAngular[ind][i] = index;

			}
		}
		v = g_MasterStrandControlVertexOffsets[s];
	}
	//chop off the part of this buffer that wasnt used
	g_numIndicesFirstAngular = ind;


	ind = 0;
	v = 0;
	for (int s = 0; s < g_NumMasterStrands; ++s) 
	{
		indicesSecondAngular[ind][0] = v;
		indicesSecondAngular[ind][1] = v+1;
		indicesSecondAngular[ind][2] = v+1;
		indicesSecondAngular[ind][3] = v+1;
		v+=2;
		ind++;

		for (; v < g_MasterStrandControlVertexOffsets[s]; ++ind, v += 4) 
		{   
			index = v-1;
			for(int i=0;i<4;i++)
			{    
				if( v+i < g_MasterStrandControlVertexOffsets[s])
					index++; 
				indicesSecondAngular[ind][i] = index;

			}
		}
		v = g_MasterStrandControlVertexOffsets[s];
	}
	//chop off the part of this buffer that wasnt used
	g_numIndicesSecondAngular = ind;



	//create all 4 Vertex Buffers from the data calculated---------------------------------------------

	initialData.pSysMem = indicesFirst;
	HRESULT hr;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_numIndicesFirst*2*sizeof(short); 
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	SAFE_RELEASE(g_MasterStrandSimulation1IB);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_MasterStrandSimulation1IB));

	initialData.pSysMem = indicesSecond;
	bufferDesc.ByteWidth = g_numIndicesSecond*2*sizeof(short);
	SAFE_RELEASE(g_MasterStrandSimulation2IB);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_MasterStrandSimulation2IB));

	initialData.pSysMem = indicesFirstAngular;
	bufferDesc.ByteWidth = g_numIndicesFirstAngular*4*sizeof(short);
	SAFE_RELEASE(g_MasterStrandSimulationAngular1IB);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_MasterStrandSimulationAngular1IB));

	initialData.pSysMem = indicesSecondAngular;
	bufferDesc.ByteWidth = g_numIndicesSecondAngular*4*sizeof(short);
	SAFE_RELEASE(g_MasterStrandSimulationAngular2IB);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_MasterStrandSimulationAngular2IB));

	delete[] indicesFirst;
	delete[] indicesSecond;
	delete[] indicesFirstAngular;
	delete[] indicesSecondAngular;

	//also create a buffer and SRV to read and index from in the compute shader
	initialData.pSysMem = g_MasterStrandControlVertexOffsets;
	bufferDesc.ByteWidth = g_NumMasterStrands*sizeof(int);
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	SAFE_RELEASE(g_MasterStrandSimulationIndicesBuffer);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_MasterStrandSimulationIndicesBuffer));
    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	SRVDesc.Format = DXGI_FORMAT_R32_SINT;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SRVDesc.Buffer.ElementOffset = 0;
	SRVDesc.Buffer.ElementWidth = g_NumMasterStrands;
	SAFE_RELEASE(g_MasterStrandSimulationIndicesSRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_MasterStrandSimulationIndicesBuffer, &SRVDesc, &g_MasterStrandSimulationIndicesSRV));

	return S_OK;
}

float vec3Length(D3DXVECTOR4 vec)
{
	return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

//create linear springs, angular springs, and original vectors (for real angular forces)
HRESULT CreateMasterStrandSpringLengthVBs(ID3D11Device* pd3dDevice)
{
	float *masterStrandLinearLengths = new float[g_NumMasterStrandControlVertices];
	D3DXVECTOR2 *masterStrandAngularLengthsStiffness = new D3DXVECTOR2[g_NumMasterStrandControlVertices];

	for(int i=0;i<g_NumMasterStrandControlVertices-1;i++)
	{
		masterStrandLinearLengths[i] =  vec3Length(g_MasterStrandControlVertices[i].Position - g_MasterStrandControlVertices[i+1].Position); 
		masterStrandAngularLengthsStiffness[i] =  D3DXVECTOR2(vec3Length(g_MasterStrandControlVertices[i].Position - g_MasterStrandControlVertices[i+2].Position),0); 
	}

	//linear springs
	HRESULT hr;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumMasterStrandControlVertices * sizeof(float);
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags =  D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA initialData;
	initialData.pSysMem = masterStrandLinearLengths;
	SAFE_RELEASE(g_MasterStrandLinearSpringLengthsVB);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_MasterStrandLinearSpringLengthsVB));
	delete[] masterStrandLinearLengths;

	//create a shader resource view to read this in the compute shader
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_R32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumMasterStrandControlVertices;
	SAFE_RELEASE(g_MasterStrandLinearSpringLengthsSRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_MasterStrandLinearSpringLengthsVB, &desc, &g_MasterStrandLinearSpringLengthsSRV));


	int ind = 0;
	int v = 0;
	float lt = 0.25f;
	float ht = 0.4f;
	int lastEndVertex = 0;

	for (int s = 0; s < g_NumMasterStrands; ++s) 
	{   
		for (; v < g_MasterStrandControlVertexOffsets[s]; ++ind, v ++) 
		{	
			float ratio = float(v-lastEndVertex)/(g_MasterStrandControlVertexOffsets[s]-1 - lastEndVertex);

			if( (g_MasterStrandControlVertexOffsets[s]-1 - lastEndVertex) < 5 )
				masterStrandAngularLengthsStiffness[ind].y = 0.0;
			else if( ratio < lt ) 
				masterStrandAngularLengthsStiffness[ind].y = g_AngularSpringStiffness;
			else if(ratio > ht)
				masterStrandAngularLengthsStiffness[ind].y = 0.0;
			else
				masterStrandAngularLengthsStiffness[ind].y = g_AngularSpringStiffness*(1.0 - ((ratio - lt)/(ht - lt)));

			//maximum stiffness for long demo hair
			if(!g_useShortHair)
			    masterStrandAngularLengthsStiffness[ind].y = 1.0;   
		}
		v = g_MasterStrandControlVertexOffsets[s];
		lastEndVertex = g_MasterStrandControlVertexOffsets[s];
	}

	bufferDesc.ByteWidth = g_NumMasterStrandControlVertices * sizeof(D3DXVECTOR2);
	D3D11_SUBRESOURCE_DATA initialDataStiffness;
	initialDataStiffness.pSysMem = masterStrandAngularLengthsStiffness;
	SAFE_RELEASE(g_MasterStrandAngularSpringConstraintsVB);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialDataStiffness, &g_MasterStrandAngularSpringConstraintsVB));

	//create a shader resource view to read this in the compute shader
	D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
	descSRV.Format = DXGI_FORMAT_R32G32_FLOAT;
	descSRV.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	descSRV.Buffer.ElementOffset = 0;
	descSRV.Buffer.ElementWidth = g_NumMasterStrandControlVertices;
	SAFE_RELEASE(g_MasterStrandAngularSpringConstraintsSRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_MasterStrandAngularSpringConstraintsVB, &descSRV, &g_MasterStrandAngularSpringConstraintsSRV));

	delete[] masterStrandAngularLengthsStiffness; 


	//original hair vectors-----------------------------------------------------------------------------------------------------
	float3* original_vectorsInLocalCF = new float3[g_NumMasterStrandControlVertices];
	v=0;
	for (int s = 0; s < g_NumMasterStrands; ++s) 
	{   
		original_vectorsInLocalCF[v] = float3( g_MasterStrandControlVertices[v+1].Position.x-g_MasterStrandControlVertices[v].Position.x,
			g_MasterStrandControlVertices[v+1].Position.y-g_MasterStrandControlVertices[v].Position.y,
			g_MasterStrandControlVertices[v+1].Position.z-g_MasterStrandControlVertices[v].Position.z );
		v++;
		for (; v < g_MasterStrandControlVertexOffsets[s]-1; v++) 
		{	
			original_vectorsInLocalCF[v] = TrasformFromWorldToLocalCF( v-1, 
				float3( g_MasterStrandControlVertices[v+1].Position.x-g_MasterStrandControlVertices[v].Position.x,
				g_MasterStrandControlVertices[v+1].Position.y-g_MasterStrandControlVertices[v].Position.y,
				g_MasterStrandControlVertices[v+1].Position.z-g_MasterStrandControlVertices[v].Position.z  )  
				);

		}

		//the last vertex dosnt need a vector since it does not make sense
		original_vectorsInLocalCF[v] = float3( 0, 0, 0 );
		v++;
	}


	ID3D11Buffer* originalVectorsVB;
	D3D11_SUBRESOURCE_DATA initialData2;
	initialData2.pSysMem = original_vectorsInLocalCF;
	initialData2.SysMemPitch = 0;
	initialData2.SysMemSlicePitch = 0;
	D3D11_BUFFER_DESC bufferDesc2;
	bufferDesc2.ByteWidth = g_NumMasterStrandControlVertices * sizeof(D3DXVECTOR3);
	bufferDesc2.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc2.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc2.CPUAccessFlags = 0;
	bufferDesc2.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc2, &initialData2, &originalVectorsVB));
	D3D11_SHADER_RESOURCE_VIEW_DESC desc2;
	ZeroMemory( &desc2, sizeof(desc2) );
	desc2.Format = DXGI_FORMAT_R32G32B32_FLOAT;
	desc2.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc2.Buffer.ElementOffset = 0;
	desc2.Buffer.ElementWidth = g_NumMasterStrandControlVertices;
	SAFE_RELEASE(g_originalVectorsRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(originalVectorsVB, &desc2, &g_originalVectorsRV));
	g_Effect->GetVariableByName("g_OriginalVectors")->AsShaderResource()->SetResource(g_originalVectorsRV);
	SAFE_RELEASE(originalVectorsVB);
	delete[] original_vectorsInLocalCF; 


	//hair lengths: ---------------------------------------------------------------------------------------------------------
	//keep them as a fraction of the total hair length
	if(g_masterStrandLengthToRoot) delete [] g_masterStrandLengthToRoot;
	if(g_masterStrandLengths) delete [] g_masterStrandLengths;
	g_masterStrandLengthToRoot = new float[g_NumMasterStrandControlVertices];
	g_masterStrandLengths = new float[g_NumMasterStrands];
	int strand = 0;
	ind = 0;
	v = 0;
	float hairLength = 0;
	for (int s = 0; s < g_NumMasterStrands; ++s) 
	{   
		hairLength = 0;
		int ind_original = ind;

		g_masterStrandLengthToRoot[ind++] = 0;
		v++;

		//get the actual hair lengths 
		for (; v < g_MasterStrandControlVertexOffsets[s]; ++ind, v ++) 
		{
			hairLength += vec3Length(g_MasterStrandControlVertices[v].Position - g_MasterStrandControlVertices[v-1].Position); 
			g_masterStrandLengthToRoot[ind] = hairLength;
		}

		//create the fractional hair lengths
		for (; ind_original < ind; ++ind_original) 
			g_masterStrandLengthToRoot[ind_original] /= hairLength;

		g_masterStrandLengths[strand++] = hairLength;
		v = g_MasterStrandControlVertexOffsets[s];
	}

	//original hair lengths
	ID3D11Buffer* MasterStrandLengthToRootVB;
	bufferDesc.BindFlags =  D3D11_BIND_SHADER_RESOURCE;
	initialData.pSysMem = g_masterStrandLengthToRoot;
	bufferDesc.ByteWidth = g_NumMasterStrandControlVertices * sizeof(float);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &MasterStrandLengthToRootVB));
	desc.Format = DXGI_FORMAT_R32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumMasterStrandControlVertices;
	SAFE_RELEASE(g_MasterStrandLengthToRootRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(MasterStrandLengthToRootVB, &desc, &g_MasterStrandLengthToRootRV));
	g_Effect->GetVariableByName("g_LengthsToRoots")->AsShaderResource()->SetResource(g_MasterStrandLengthToRootRV);
	SAFE_RELEASE(MasterStrandLengthToRootVB);

	ID3D11Buffer* MasterStrandLengthsVB;
	initialData.pSysMem = g_masterStrandLengths;
	bufferDesc.ByteWidth = g_NumMasterStrands * sizeof(float);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &MasterStrandLengthsVB));
	desc.Buffer.ElementWidth = g_NumMasterStrands;
	SAFE_RELEASE(g_MasterStrandLengthsRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(MasterStrandLengthsVB, &desc, &g_MasterStrandLengthsRV));
	g_Effect->GetVariableByName("g_Lengths")->AsShaderResource()->SetResource(g_MasterStrandLengthsRV);
	SAFE_RELEASE(MasterStrandLengthsVB);

	//tessellated hair lengths
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags =  D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_STREAM_OUTPUT;
	bufferDesc.ByteWidth = g_NumTessellatedMasterStrandVertices * sizeof(float);
	SAFE_RELEASE(g_TessellatedMasterStrandLengthToRootVB);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, NULL, &g_TessellatedMasterStrandLengthToRootVB));
	desc.Format = DXGI_FORMAT_R32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumTessellatedMasterStrandVertices;
	SAFE_RELEASE(g_TessellatedMasterStrandLengthToRootRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_TessellatedMasterStrandLengthToRootVB, &desc, &g_TessellatedMasterStrandLengthToRootRV));


	//vertex number along hair----------------------------------------------------------------------------------------------------------
	//resource to convert vertexID of the one long VB we have to a vertex number from the root
	int *vertexNumber = new int[g_NumMasterStrandControlVertices];
	v = 0;
	int rootV = 0;
	for (int s = 0; s < g_NumMasterStrands; ++s) 
	{	for (; v < g_MasterStrandControlVertexOffsets[s]; v++) 
	{
		vertexNumber[v] = v-rootV;
	}
	rootV = v;
	}
	//original hair lengths
	ID3D11Buffer* vertexNumberVB;
	bufferDesc.BindFlags =  D3D11_BIND_SHADER_RESOURCE;
	initialData.pSysMem = vertexNumber;
	bufferDesc.ByteWidth = g_NumMasterStrandControlVertices * sizeof(int);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &vertexNumberVB));
	desc.Format = DXGI_FORMAT_R32_SINT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumMasterStrandControlVertices;
	SAFE_RELEASE(g_MasterStrandLocalVertexNumberRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(vertexNumberVB, &desc, &g_MasterStrandLocalVertexNumberRV));
	g_Effect->GetVariableByName("g_VertexNumber")->AsShaderResource()->SetResource(g_MasterStrandLocalVertexNumberRV);
	SAFE_RELEASE(vertexNumberVB);
	delete[] vertexNumber;

	return S_OK; 
}

HRESULT CreateMasterStrandTesselatedInputVB(ID3D11Device* pd3dDevice)
{   
	D3D11_SUBRESOURCE_DATA initialData;
	HairAdjacencyVertex* vertices = new HairAdjacencyVertex[g_NumTessellatedMasterStrandVertices];

	int ind = 0;
	short v = 0;

	//this is for b splines:
	for (int s = 0; s < g_NumMasterStrands; ++s)
	{
		bool firstSegment = true;
		for (v = v-1 ; v < g_MasterStrandControlVertexOffsets[s] - 2; v += 1) //starting one vertex early, and ending one vertex late, to get extra beginning and end segments 
		{   
			for (int i = 0; i < NUM_TESSELLATION_SEGMENTS; ++i, ++ind) 
			{
				HairAdjacencyVertex tempVertex;
				tempVertex.controlVertexIndex = v;
				tempVertex.u = i / (float)(NUM_TESSELLATION_SEGMENTS);

				if(firstSegment) //first segment
				{   
					tempVertex.controlVertexIndex = v+1;
					tempVertex.u -= 2;
				}
				else if( v == g_MasterStrandControlVertexOffsets[s] - 3 ) //last segment
				{
					tempVertex.controlVertexIndex = v-1;
					tempVertex.u += 2;
				}

				vertices[ind] = tempVertex;
			}
			firstSegment = false;
		}

		//extra end vertex
		HairAdjacencyVertex tempVertex;
		tempVertex.controlVertexIndex = v-2; 
		tempVertex.u = 3.0; 
		vertices[ind++] = tempVertex; 

		v+=2;
	}
	/*
	// this is for bezier curves: 
	for (int s = 0; s < g_NumMasterStrands; ++s) 
	{
	for (; v < g_MasterStrandControlVertexOffsets[s] - 1; v += 3) 
	{   
	for (int i = 0; i < NUM_TESSELLATION_SEGMENTS; ++i, ++ind) 
	{
	HairAdjacencyVertex tempVertex;
	tempVertex.controlVertexIndex = v;
	tempVertex.u = i / (float)(NUM_TESSELLATION_SEGMENTS);
	vertices[ind] = tempVertex;
	}
	}
	HairAdjacencyVertex tempVertex;
	tempVertex.controlVertexIndex = v; 
	tempVertex.u = 0.0;
	vertices[ind] = tempVertex;
	++v;
	++ind;       

	assert(v == g_MasterStrandControlVertexOffsets[s]);
	}*/

	initialData.pSysMem = vertices;
	HRESULT hr;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumTessellatedMasterStrandVertices * sizeof(HairAdjacencyVertex);
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
    SAFE_RELEASE(g_TessellatedMasterStrandVBInitial);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_TessellatedMasterStrandVBInitial));
	delete [] vertices;
	return S_OK;
}



HRESULT CreateInterpolatedValuesVBs(ID3D11Device* pd3dDevice)
{
	HRESULT hr;

	int numBVertices = g_TessellatedMasterStrandLengthMax*g_totalMStrands;
	int numCVertices = g_TessellatedMasterStrandLengthMax*g_totalSStrands;


	//create the vertex buffers
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = numBVertices*4*sizeof(float);
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags =  D3D11_BIND_SHADER_RESOURCE |D3D11_BIND_STREAM_OUTPUT;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	SAFE_RELEASE(g_BarycentricInterpolatedPositionsWidthsVB);
	SAFE_RELEASE(g_BarycentricInterpolatedIdAlphaTexVB);
	SAFE_RELEASE(g_BarycentricInterpolatedTangentVB);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, 0, &g_BarycentricInterpolatedPositionsWidthsVB));//position and width
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, 0, &g_BarycentricInterpolatedIdAlphaTexVB)); //ID, alpha and texture
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, 0, &g_BarycentricInterpolatedTangentVB)); //tangent  

	bufferDesc.ByteWidth = numCVertices*4*sizeof(float);
    SAFE_RELEASE(g_ClumpInterpolatedPositionsWidthsVB);
    SAFE_RELEASE(g_ClumpInterpolatedIdAlphaTexVB);
    SAFE_RELEASE(g_ClumpInterpolatedTangentVB);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, 0, &g_ClumpInterpolatedPositionsWidthsVB));//position and width
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, 0, &g_ClumpInterpolatedIdAlphaTexVB)); //ID, alpha and texture
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, 0, &g_ClumpInterpolatedTangentVB)); //tangent  

	//create the shader resource views
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = numBVertices;
    SAFE_RELEASE(g_BarycentricInterpolatedPositionsWidthsRV);
    SAFE_RELEASE(g_BarycentricInterpolatedIdAlphaTexRV);
    SAFE_RELEASE(g_BarycentricInterpolatedTangentRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_BarycentricInterpolatedPositionsWidthsVB, &desc, &g_BarycentricInterpolatedPositionsWidthsRV));
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_BarycentricInterpolatedIdAlphaTexVB, &desc, &g_BarycentricInterpolatedIdAlphaTexRV));
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_BarycentricInterpolatedTangentVB, &desc, &g_BarycentricInterpolatedTangentRV));

	desc.Buffer.ElementWidth = numCVertices;
    SAFE_RELEASE(g_ClumpInterpolatedPositionsWidthsRV);
    SAFE_RELEASE(g_ClumpInterpolatedIdAlphaTexRV);
    SAFE_RELEASE(g_ClumpInterpolatedTangentRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_ClumpInterpolatedPositionsWidthsVB, &desc, &g_ClumpInterpolatedPositionsWidthsRV));
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_ClumpInterpolatedIdAlphaTexVB, &desc, &g_ClumpInterpolatedIdAlphaTexRV));
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_ClumpInterpolatedTangentVB, &desc, &g_ClumpInterpolatedTangentRV));

	return hr;
}


HRESULT CreateMasterStrandVB(ID3D11Device* pd3dDevice)
{
	HRESULT hr;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumMasterStrandControlVertices * sizeof(HairVertex);
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags =  D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE |D3D11_BIND_STREAM_OUTPUT;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA initialData;
	initialData.pSysMem = g_MasterStrandControlVertices;
	SAFE_RELEASE(g_MasterStrandVB[0]);
	SAFE_RELEASE(g_MasterStrandVB[1]);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_MasterStrandVB[0]));
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_MasterStrandVB[1])); 
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	SAFE_RELEASE(g_MasterStrandVBOldTemp);
	SAFE_RELEASE(g_MasterStrandVBOld);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_MasterStrandVBOldTemp));
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_MasterStrandVBOld)); 

	//create shader resource views for the master strand; this is for non-dynamic tesselation
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumMasterStrandControlVertices;
	SAFE_RELEASE(g_MasterStrandRV[0]);
	SAFE_RELEASE(g_MasterStrandRV[1]);
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_MasterStrandVB[0], &desc, &g_MasterStrandRV[0]));
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_MasterStrandVB[1], &desc, &g_MasterStrandRV[1]));

	ID3D11Buffer* originalMasterStrand = NULL;
	initialData.pSysMem = g_MasterStrandControlVertices;
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	SAFE_RELEASE(originalMasterStrand);
	SAFE_RELEASE(g_OriginalMasterStrandRV);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &originalMasterStrand));
	V_RETURN(pd3dDevice->CreateShaderResourceView(originalMasterStrand, &desc, &g_OriginalMasterStrandRV));
	g_Effect->GetVariableByName("g_OriginalMasterStrand")->AsShaderResource()->SetResource(g_OriginalMasterStrandRV);
	SAFE_RELEASE(originalMasterStrand);


    //create buffers for compute shader to use--------------------------------------

	//the structured buffers
	D3D11_BUFFER_DESC bufferDescUA;
	bufferDescUA.ByteWidth = g_NumMasterStrandControlVertices * sizeof(HairVertex);
	bufferDescUA.Usage = D3D11_USAGE_DEFAULT;
	bufferDescUA.BindFlags =  D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	bufferDescUA.CPUAccessFlags = 0;
	bufferDescUA.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bufferDescUA.StructureByteStride  = sizeof(D3DXVECTOR4);
	initialData.pSysMem = g_MasterStrandControlVertices;
	SAFE_RELEASE(g_MasterStrandUAB);
	SAFE_RELEASE(g_MasterStrandPrevUAB);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDescUA, &initialData, &g_MasterStrandUAB));
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDescUA, &initialData, &g_MasterStrandPrevUAB));	

	// create the Shader Resource View (SRV) for the structured buffers
	// this will be used by the shaders trying to read data output by the compute shader pass
	D3D11_SHADER_RESOURCE_VIEW_DESC sbSRVDesc;
	sbSRVDesc.Buffer.ElementOffset          = 0;
	sbSRVDesc.Buffer.ElementWidth           = sizeof(D3DXVECTOR4); //g_NumMasterStrandControlVertices
	sbSRVDesc.Buffer.FirstElement           = 0;
	sbSRVDesc.Buffer.NumElements            = g_NumMasterStrandControlVertices;
	sbSRVDesc.Format                        = DXGI_FORMAT_UNKNOWN; //DXGI_FORMAT_R32G32B32A32_FLOAT
	sbSRVDesc.ViewDimension                 = D3D11_SRV_DIMENSION_BUFFER;
	SAFE_RELEASE(g_MasterStrandUAB_SRV);
	V_RETURN( pd3dDevice->CreateShaderResourceView(g_MasterStrandUAB, &sbSRVDesc, &g_MasterStrandUAB_SRV) );

	// create the UAV for the structured buffers
	//this will be used by the compute shader to write data
	D3D11_UNORDERED_ACCESS_VIEW_DESC sbUAVDesc;
	sbUAVDesc.Buffer.FirstElement       = 0;
	sbUAVDesc.Buffer.Flags              = 0;
	sbUAVDesc.Buffer.NumElements        = g_NumMasterStrandControlVertices;
	sbUAVDesc.Format                    = DXGI_FORMAT_UNKNOWN; //DXGI_FORMAT_R32G32B32A32_FLOAT
	sbUAVDesc.ViewDimension             = D3D11_UAV_DIMENSION_BUFFER;
	SAFE_RELEASE(g_MasterStrandUAB_UAV);
	SAFE_RELEASE(g_MasterStrandPrevUAB_UAV);
	V_RETURN( pd3dDevice->CreateUnorderedAccessView(g_MasterStrandUAB, &sbUAVDesc, &g_MasterStrandUAB_UAV) );
	V_RETURN( pd3dDevice->CreateUnorderedAccessView(g_MasterStrandPrevUAB, &sbUAVDesc, &g_MasterStrandPrevUAB_UAV) );
	


	return S_OK;
}


HRESULT CreateForceVB(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext)
{
	//initialize forces to be (0,0,0,k)
	//k is the angular stiffness of the hair
	//k is 10 for the first 15 vertices and 3 for the rest of the vertices in a strand with some blend region
	//make shorter (<15 vertices) hair stiff all over, unless texture specifies otherwise

	HRESULT hr;

	//load the stiffness texture
	WCHAR str[MAX_PATH];
	V_RETURN(NVUTFindDXSDKMediaFileCch(str, MAX_PATH, L"LongHairResources/stiffnessMap.dds"));
	ID3D11Resource *pRes = NULL;
	D3DX11_IMAGE_LOAD_INFO loadInfo;
	ZeroMemory( &loadInfo, sizeof( D3DX11_IMAGE_LOAD_INFO ) );
	loadInfo.Width = D3DX_FROM_FILE;
	loadInfo.Height  = D3DX_FROM_FILE;
	loadInfo.Depth  = D3DX_FROM_FILE;
	loadInfo.FirstMipLevel = 0;
	loadInfo.MipLevels = 1;
	loadInfo.Usage = D3D11_USAGE_STAGING;
	loadInfo.BindFlags = 0;
	loadInfo.CpuAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
	loadInfo.MiscFlags = 0;
	loadInfo.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	loadInfo.Filter = D3DX10_FILTER_NONE;
	loadInfo.MipFilter = D3DX10_FILTER_NONE;
	V_RETURN(D3DX11CreateTextureFromFile( pd3dDevice, str, &loadInfo, NULL, &pRes, &hr ));
	if(!pRes )
		return S_FALSE;


	ID3D11Texture2D* pTemp;
	pRes->QueryInterface( __uuidof( ID3D11Texture2D ), (LPVOID*)&pTemp );
	D3D11_TEXTURE2D_DESC texDesc;
	pTemp->GetDesc( &texDesc );

	D3D11_MAPPED_SUBRESOURCE mappedTex2D;
	pd3dContext->Map(pTemp, 0, D3D11_MAP_READ, 0, &mappedTex2D);
	FLOAT* pTexels = (FLOAT*)mappedTex2D.pData;


	int shortStiffHairLength = 15;
	float flex = 0.0;
	float stiff = 1.0; 
	int blendRegion = 8;
	int stiffRegion = 9;
	float hairStiffnessMultiplier = 1.0f;
    if(g_useShortHair)
	{
	    shortStiffHairLength = 10;
		stiffRegion = 5;
        hairStiffnessMultiplier = 0.15f; 
	}

	int blendBegin = stiffRegion-blendRegion/2.0;
	int blendEnd = stiffRegion+blendRegion/2.0;

	float4* forces = new float4[g_NumMasterStrandControlVertices];
    float* stiffness = new float[g_NumMasterStrandControlVertices];
	int v=0;
	int rootV = 0;


	for (int s = 0; s < g_NumMasterStrands; ++s) 
	{   
		float hairTexStiffness;

		unsigned int x = g_MasterAttributes[s].texcoord.x*texDesc.Width;
		if(x>=texDesc.Width)
			x = texDesc.Width-1;

		unsigned int y = g_MasterAttributes[s].texcoord.y*texDesc.Height;
		if(y>=texDesc.Height)
			y = texDesc.Height-1;


		int location =   x*4 
			+ y*texDesc.Width*4;

		hairTexStiffness = pTexels[location]*hairStiffnessMultiplier; 


		int strandLength = g_MasterStrandControlVertexOffsets[s] - rootV;
		for (; v < g_MasterStrandControlVertexOffsets[s]; v++) 
		{	
			if(strandLength < shortStiffHairLength && hairTexStiffness>0.1)
			{
				forces[v] = float4(0,0,0,1);
				stiffness[v] = 1.0f;
			}
			else
			{   
				float localStiff = hairTexStiffness*stiff + (1-hairTexStiffness)*0.1;

				if(v-rootV < blendBegin) 
				{
					forces[v] = float4(0,0,0,localStiff);
					stiffness[v] = localStiff;
				}
				else if(v-rootV >= blendEnd)
				{
					forces[v] = float4(0,0,0,flex);
					stiffness[v] = flex;
				}
				else
				{    
					float blend = float(v-rootV-blendBegin)/blendRegion; 
					float stif = blend*flex + (1-blend)*localStiff;
					forces[v] = float4(0,0,0,stif);
					stiffness[v] = stif;
				}
			}
		}
		rootV = v;
	}

	pd3dContext->Unmap(pTemp, 0);
	SAFE_RELEASE( pTemp );
	SAFE_RELEASE( pRes );


	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumMasterStrandControlVertices * sizeof(HairVertex);
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags =  D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE |D3D11_BIND_STREAM_OUTPUT;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA initialData;
	initialData.pSysMem = forces;
	SAFE_RELEASE(g_ForceVB[0]);
	SAFE_RELEASE(g_ForceVB[1]);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_ForceVB[0]));
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_ForceVB[1])); 
	delete[] forces;

	//create shader resource views; this is for reading in the final integrate pass
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumMasterStrandControlVertices;
	SAFE_RELEASE(g_ForceRV[0]);
	SAFE_RELEASE(g_ForceRV[1]);
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_ForceVB[0], &desc, &g_ForceRV[0]));
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_ForceVB[1], &desc, &g_ForceRV[1]));

    //create a buffer and shader resource for the compute shader (angular forces are directly computed and used in compute shader, so we just need angular stiffness)
	//the structured buffers
	ID3D11Buffer* stiffnessUAB;
    bufferDesc.ByteWidth = g_NumMasterStrandControlVertices * sizeof(float);
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags =  D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride  = sizeof(float);
	initialData.pSysMem = stiffness;
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &stiffnessUAB));	

	// create the Shader Resource View (SRV) for the structured buffers
	// this will be used by the shaders trying to read data output by the compute shader pass
	D3D11_SHADER_RESOURCE_VIEW_DESC sbSRVDesc;
	sbSRVDesc.Buffer.ElementOffset          = 0;
	sbSRVDesc.Buffer.ElementWidth           = sizeof(float); 
	sbSRVDesc.Buffer.FirstElement           = 0;
	sbSRVDesc.Buffer.NumElements            = g_NumMasterStrandControlVertices;
	sbSRVDesc.Format                        = DXGI_FORMAT_R32_FLOAT; 
	sbSRVDesc.ViewDimension                 = D3D11_SRV_DIMENSION_BUFFER;
	SAFE_RELEASE(g_AngularStiffnessUAB_SRV);
	V_RETURN( pd3dDevice->CreateShaderResourceView(stiffnessUAB, &sbSRVDesc, &g_AngularStiffnessUAB_SRV) );

	delete[] stiffness;
	SAFE_RELEASE(stiffnessUAB);

	return S_OK;
}

HRESULT CreateCoordinateFrameVB(ID3D11Device* pd3dDevice)
{
	HRESULT hr;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumMasterStrandControlVertices * 3 * 4 * sizeof(float); 
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags =  D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE |D3D11_BIND_STREAM_OUTPUT;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA initialData;
	initialData.pSysMem = g_coordinateFrames;
	SAFE_RELEASE(g_CoordinateFrameVB[0]);
	SAFE_RELEASE(g_CoordinateFrameVB[1]);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_CoordinateFrameVB[0]));
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_CoordinateFrameVB[1]));

	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumMasterStrandControlVertices * 3;
	SAFE_RELEASE(g_CoordinateFrameRV[0]);
	SAFE_RELEASE(g_CoordinateFrameRV[1]);
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_CoordinateFrameVB[0], &desc, &g_CoordinateFrameRV[0]));
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_CoordinateFrameVB[1], &desc, &g_CoordinateFrameRV[1]));


	//buffers for the compute shader-----------------------------------------------------------

	bufferDesc.BindFlags =  D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bufferDesc.StructureByteStride  = 4 * sizeof(float);
	//bufferDesc.ByteWidth = g_NumMasterStrandControlVertices * 3 * 4 * sizeof(float);
	SAFE_RELEASE(g_CoordinateFrameUAB);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_CoordinateFrameUAB));

	// create the Shader Resource View (SRV) for the structured buffers
	// this will be used by the shaders trying to read data output by the compute shader pass
	desc.Buffer.ElementOffset          = 0;
	desc.Buffer.ElementWidth           = 4 * sizeof(float); //g_NumMasterStrandControlVertices
	desc.Buffer.FirstElement           = 0;
	desc.Buffer.NumElements            = g_NumMasterStrandControlVertices * 3;
	desc.Format                        = DXGI_FORMAT_UNKNOWN; 
	desc.ViewDimension                 = D3D11_SRV_DIMENSION_BUFFER;
	SAFE_RELEASE(g_CoordinateFrameUAB_SRV);
	V_RETURN( pd3dDevice->CreateShaderResourceView(g_CoordinateFrameUAB, &desc, &g_CoordinateFrameUAB_SRV) );

	// create the UAV for the structured buffer
	//this will be used by the compute shader to read and write data
	D3D11_UNORDERED_ACCESS_VIEW_DESC sbUAVDesc;
	sbUAVDesc.Buffer.FirstElement       = 0;
	sbUAVDesc.Buffer.Flags              = 0;
	sbUAVDesc.Buffer.NumElements        = g_NumMasterStrandControlVertices * 3;
	sbUAVDesc.Format                    = DXGI_FORMAT_UNKNOWN; 
	sbUAVDesc.ViewDimension             = D3D11_UAV_DIMENSION_BUFFER;
	SAFE_RELEASE(g_CoordinateFrameUAB_UAV);
	V_RETURN( pd3dDevice->CreateUnorderedAccessView(g_CoordinateFrameUAB, &sbUAVDesc, &g_CoordinateFrameUAB_UAV) );

	return S_OK; 
}

HRESULT CreateTessellatedCoordinateFrameVB(ID3D11Device* pd3dDevice)
{
	HRESULT hr;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumTessellatedMasterStrandVertices * 3 * 4 * sizeof(float); 
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags =   D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE |D3D11_BIND_STREAM_OUTPUT;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	SAFE_RELEASE(g_TessellatedCoordinateFrameVB);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, 0, &g_TessellatedCoordinateFrameVB));

	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumTessellatedMasterStrandVertices * 3;
	SAFE_RELEASE(g_TessellatedCoordinateFrameRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_TessellatedCoordinateFrameVB, &desc, &g_TessellatedCoordinateFrameRV));
	g_Effect->GetVariableByName("g_tessellatedCoordinateFrames")->AsShaderResource()->SetResource(g_TessellatedCoordinateFrameRV);

	return S_OK;	
}

HRESULT CreateCollisionDetectionResources(ID3D11Device* pd3dDevice)
{
	HRESULT hr = S_OK;

	// Create the texture
	D3D11_TEXTURE2D_DESC TexDesc;
	TexDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	TexDesc.CPUAccessFlags = 0; 
	TexDesc.MipLevels = 1;
	TexDesc.MiscFlags = 0;
	TexDesc.Usage = D3D11_USAGE_DEFAULT;
	TexDesc.Width =  MAX_INTERPOLATED_HAIR_PER_WISP;
	TexDesc.Height = g_NumWisps;
	TexDesc.ArraySize = 1;
	TexDesc.Format = DXGI_FORMAT_R32_FLOAT;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.SampleDesc.Quality = 0;
	ID3D11Texture2D* texture; 
	V_RETURN( pd3dDevice->CreateTexture2D(&TexDesc,NULL,&texture));

	// Create the render target view
	D3D11_RENDER_TARGET_VIEW_DESC DescRT;
	ZeroMemory( &DescRT, sizeof(DescRT) );
	DescRT.Format = TexDesc.Format;
	DescRT.ViewDimension =  D3D11_RTV_DIMENSION_TEXTURE2D;
	DescRT.Texture2D.MipSlice = 0;
	SAFE_RELEASE(g_CollisionsRTV);
	V_RETURN( pd3dDevice->CreateRenderTargetView( texture, &DescRT, &g_CollisionsRTV) );

	// Create the "shader resource view" (SRView) and "shader resource variable" (SRVar) for the given texture 
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	ZeroMemory( &SRVDesc, sizeof(SRVDesc) );
	SRVDesc.Format = TexDesc.Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;
	SRVDesc.Texture2D.MostDetailedMip = 0;
    SAFE_RELEASE(g_CollisionsRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView( texture, &SRVDesc, &g_CollisionsRV));

	SAFE_RELEASE( texture );
	return hr;
}

HRESULT CreateTessellatedMasterStrandVB(ID3D11Device* pd3dDevice)
{
	HRESULT hr;

	//create the positions ---------------------------------------------------
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumTessellatedMasterStrandVertices * sizeof(HairVertex);
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_STREAM_OUTPUT | D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	SAFE_RELEASE(g_TessellatedMasterStrandVB);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, 0, &g_TessellatedMasterStrandVB));

	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumTessellatedMasterStrandVertices;
	SAFE_RELEASE(g_TessellatedMasterStrandRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_TessellatedMasterStrandVB, &desc, &g_TessellatedMasterStrandRV));


	//create the tangents -----------------------------------------------------------
	bufferDesc.ByteWidth = g_NumTessellatedMasterStrandVertices * sizeof(HairVertex);
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_STREAM_OUTPUT | D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
    SAFE_RELEASE(g_TessellatedTangentsVB);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, 0, &g_TessellatedTangentsVB));
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumTessellatedMasterStrandVertices;
	SAFE_RELEASE(g_TessellatedTangentsRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_TessellatedTangentsVB, &desc, &g_TessellatedTangentsRV));

	return S_OK;
}

HRESULT CreateTessellatedWispVB(ID3D11Device* pd3dDevice)
{
	HRESULT hr;

	Wisp* wisps = new Wisp[g_NumWisps * g_TessellatedMasterStrandLengthMax];
	Wisp* untessellatedWisps = new Wisp[g_NumWisps * g_MasterStrandLengthMax];
	g_NumTessellatedWisps = 0;
	g_NumUntessellatedWisps = 0;

	D3DXVECTOR3* rootsS = new D3DXVECTOR3[g_NumWisps];
	g_masterCVIndices = new D3DXVECTOR3[g_NumWisps];
	D3DXVECTOR4*  masterLengths = new D3DXVECTOR4[g_NumWisps];

	D3DXVECTOR3* rootsSUntessellated = new D3DXVECTOR3[g_NumWisps];
	D3DXVECTOR4* masterLengthsUntessellated = new D3DXVECTOR4[g_NumWisps];

	int* indices = g_indices;
	g_MaxMStrandSegments = 0;

	for (Index w = 0; w < g_NumWisps; w++, indices += 3) 
	{
		int offset[3];
		Index num[3];
		int offsetUnTessellated[3];
		int numUnTessellated[3];

		for (int i = 0; i < 3; ++i) 
		{
			int index = indices[i];
			offset[i] = index == 0 ? 0 : g_TessellatedMasterStrandVertexOffsets[index - 1];
			num[i] = g_TessellatedMasterStrandVertexOffsets[index] - offset[i];

			offsetUnTessellated[i] = index == 0 ? 0 : g_MasterStrandControlVertexOffsets[index - 1];
			numUnTessellated[i] = g_MasterStrandControlVertexOffsets[index] - offsetUnTessellated[i];
		}

		//tessellated
		rootsS[w] = D3DXVECTOR3(offset[0], offset[1], offset[2]);
		g_masterCVIndices[w] = D3DXVECTOR3(indices[0], indices[1], indices[2]); //the positions of the 3 vertices of the triangle in the original VB of CVs
		Index n = (num[0] + num[1] + num[2]) / 3;
		masterLengths[w] = D3DXVECTOR4(num[0]-1,num[1]-1,num[2]-1,min(min(num[0]-1,num[1]-1),num[2]-1));
		g_MaxMStrandSegments = max((int)masterLengths[w].w - 1, g_MaxMStrandSegments);

		//Un - tessellated
		rootsSUntessellated[w] = D3DXVECTOR3(offsetUnTessellated[0], offsetUnTessellated[1], offsetUnTessellated[2]);
		Index nUntessellated = (numUnTessellated[0] + numUnTessellated[1] + numUnTessellated[2]) / 3;
		masterLengthsUntessellated[w] = D3DXVECTOR4(numUnTessellated[0]-1,numUnTessellated[1]-1,numUnTessellated[2]-1,n);

		//tessellated VB
		for (Index v = 0; v < n; ++v, ++g_NumTessellatedWisps) 
		{
			for (int i = 0; i < 3; ++i)
			{
				wisps[g_NumTessellatedWisps][i] = offset[i] + (v >= num[i] ? num[i] - 1 : v);
			}
			wisps[g_NumTessellatedWisps][3] = w + 1;
		}
		wisps[g_NumTessellatedWisps - 1][3] *= - 1;


		//untessellated VB
		for (Index v = 0; v < nUntessellated; ++v, ++g_NumUntessellatedWisps) 
		{
			for (int i = 0; i < 3; ++i)
			{
				untessellatedWisps[g_NumUntessellatedWisps][i] = offsetUnTessellated[i] + (v >= numUnTessellated[i] ? numUnTessellated[i] - 1 : v);
			}
			untessellatedWisps[g_NumUntessellatedWisps][3] = w + 1;
		}
		untessellatedWisps[g_NumUntessellatedWisps - 1][3] *= - 1;


	}

	//create a constant buffer for the root indices
	D3D11_SUBRESOURCE_DATA initialData2;
	initialData2.pSysMem = rootsS;
	initialData2.SysMemPitch = 0;
	initialData2.SysMemSlicePitch = 0;
	D3D11_BUFFER_DESC bufferDesc2;
	bufferDesc2.ByteWidth = g_NumWisps * sizeof(D3DXVECTOR3);
	bufferDesc2.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc2.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc2.CPUAccessFlags = 0;
	bufferDesc2.MiscFlags = 0;
	SAFE_RELEASE(g_rootsIndices);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc2, &initialData2, &g_rootsIndices));
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	ZeroMemory( &desc, sizeof(desc) );
	desc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumWisps;
	SAFE_RELEASE(g_RootsIndicesRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_rootsIndices, &desc, &g_RootsIndicesRV));
	g_Effect->GetVariableByName("g_MasterStrandRootIndices")->AsShaderResource()->SetResource(g_RootsIndicesRV);
	delete [] rootsS;
	//untessellated
	initialData2.pSysMem = rootsSUntessellated;
	SAFE_RELEASE(g_rootsIndicesUntessellated);
	SAFE_RELEASE(g_RootsIndicesRVUntessellated);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc2, &initialData2, &g_rootsIndicesUntessellated));
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_rootsIndicesUntessellated, &desc, &g_RootsIndicesRVUntessellated));
	g_Effect->GetVariableByName("g_MasterStrandRootIndicesUntessellated")->AsShaderResource()->SetResource(g_RootsIndicesRVUntessellated); 
	delete[] rootsSUntessellated;
	//single strand
	ID3D11Buffer* rootsIndexVB;
	initialData2.pSysMem = g_TessellatedMasterStrandVertexOffsets;
	initialData2.SysMemPitch = 0;
	initialData2.SysMemSlicePitch = 0;
	bufferDesc2.ByteWidth = g_NumMasterStrands * sizeof(Index); 
	bufferDesc2.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc2.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc2.CPUAccessFlags = 0;
	bufferDesc2.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc2, &initialData2, &rootsIndexVB));
	ZeroMemory( &desc, sizeof(desc) );
	desc.Format = DXGI_FORMAT_R32_SINT; 
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumMasterStrands;
	SAFE_RELEASE(g_TessellatedMasterStrandRootIndexRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(rootsIndexVB, &desc, &g_TessellatedMasterStrandRootIndexRV));
	g_Effect->GetVariableByName("g_tessellatedMasterStrandRootIndex")->AsShaderResource()->SetResource(g_TessellatedMasterStrandRootIndexRV);
	SAFE_RELEASE(rootsIndexVB);


	initialData2.pSysMem = g_masterCVIndices;
	initialData2.SysMemPitch = 0;
	initialData2.SysMemSlicePitch = 0;
	D3D11_BUFFER_DESC bufferDesc3;
	bufferDesc3.ByteWidth = g_NumWisps * sizeof(D3DXVECTOR3);
	bufferDesc3.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc3.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc3.CPUAccessFlags = 0;
	bufferDesc3.MiscFlags = 0;
	D3D11_SHADER_RESOURCE_VIEW_DESC desc3;
	ZeroMemory( &desc3, sizeof(desc3) );
	desc3.Format = DXGI_FORMAT_R32G32B32_FLOAT; 
	desc3.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc3.Buffer.ElementOffset = 0;
	desc3.Buffer.ElementWidth = g_NumWisps;
	SAFE_RELEASE(g_rootsIndicesMasterStrand);
	SAFE_RELEASE(g_rootsIndicesMasterStrandRV);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc3, &initialData2, &g_rootsIndicesMasterStrand));
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_rootsIndicesMasterStrand, &desc3, &g_rootsIndicesMasterStrandRV));
	g_Effect->GetVariableByName("g_OriginalMasterStrandRootIndices")->AsShaderResource()->SetResource(g_rootsIndicesMasterStrandRV);

	initialData2.pSysMem = masterLengths;
	initialData2.SysMemPitch = 0;
	initialData2.SysMemSlicePitch = 0;
	bufferDesc2.ByteWidth = g_NumWisps * sizeof(D3DXVECTOR4);
	bufferDesc2.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc2.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc2.CPUAccessFlags = 0;
	bufferDesc2.MiscFlags = 0;
	SAFE_RELEASE(g_masterLengths);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc2, &initialData2, &g_masterLengths));
	ZeroMemory( &desc, sizeof(desc) );
	desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumWisps;
	SAFE_RELEASE(g_MasterLengthsRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_masterLengths, &desc, &g_MasterLengthsRV));
	g_Effect->GetVariableByName("g_MasterStrandLengths")->AsShaderResource()->SetResource(g_MasterLengthsRV);
	delete[] masterLengths;
	//untessellated
	initialData2.pSysMem = masterLengthsUntessellated;
	SAFE_RELEASE(g_masterLengthsUntessellated);
	SAFE_RELEASE(g_MasterLengthsRVUntessellated);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc2, &initialData2, &g_masterLengthsUntessellated));
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_masterLengthsUntessellated, &desc, &g_MasterLengthsRVUntessellated));
	g_Effect->GetVariableByName("g_MasterStrandLengthsUntessellated")->AsShaderResource()->SetResource(g_MasterLengthsRVUntessellated);
	delete[] masterLengthsUntessellated;


	//The VBs
	D3D11_SUBRESOURCE_DATA initialData;
	initialData.pSysMem = wisps;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumTessellatedWisps * sizeof(Wisp);
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	SAFE_RELEASE(g_TessellatedWispVB);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_TessellatedWispVB));
	//untessellated
	initialData.pSysMem = untessellatedWisps;
	bufferDesc.ByteWidth = g_NumUntessellatedWisps * sizeof(Wisp);
	SAFE_RELEASE(g_UnTessellatedWispVB);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_UnTessellatedWispVB));

	delete [] wisps;
	delete [] untessellatedWisps;

	return S_OK;
}

HRESULT CreateComputeShaderConstantBuffers(ID3D11Device* pd3dDevice)
{
	HRESULT hr = S_OK;

    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;
    Desc.ByteWidth = sizeof( CB_CS_PER_FRAME );
	SAFE_RELEASE(g_pcbCSPerFrame);
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbCSPerFrame ) );

    Desc.ByteWidth = sizeof( CB_CS_TRANSFORMS );
	SAFE_RELEASE(g_pcbCSTransforms);
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbCSTransforms ) );

    Desc.ByteWidth = sizeof( D3DXMATRIX );	
	SAFE_RELEASE(g_pcbCSRootXform);
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbCSRootXform ) );

    return S_OK;
}

//-----------------------------------------------------------------------------------------

HRESULT saveHair()
{
	HRESULT hr = S_OK;

	ofstream outfile;
	outfile.open ("hairData.txt",ios::trunc|ios::out);

	//outfile<<fixed;
	outfile<<showpoint;

	//write out the vertex count
	outfile<<g_NumMasterStrandControlVertices<<endl;

	//write out the master strand control vertices
	for(int i=0;i<g_NumMasterStrandControlVertices;i++)
		outfile<<'('<<g_MasterStrandControlVertices[i].Position.x<<
		','<<g_MasterStrandControlVertices[i].Position.y<<
		','<<g_MasterStrandControlVertices[i].Position.z<<')';
	outfile<<endl;

	//write out the number of master strands
	outfile<<g_NumMasterStrands<<endl;

	//write out the control vertex offset array
	for(int i=0;i<g_NumMasterStrands;i++)
		outfile<<g_MasterStrandControlVertexOffsets[i]<<' '; 
	outfile<<endl;

	//write out the texcoords
	for(int i=0;i<g_NumMasterStrands;i++)
		outfile<<'('<<g_MasterAttributes[i].texcoord.x<<','<<g_MasterAttributes[i].texcoord.y<<')'; 
	outfile<<endl;

	//write out the coordinate frames of the master strands
	for(int i=0;i<g_NumMasterStrandControlVertices;i++)
	{
		outfile<<'('<<g_coordinateFrames[i*12+0]<<','
			<<g_coordinateFrames[i*12+1]<<','
			<<g_coordinateFrames[i*12+2]<<','
			<<g_coordinateFrames[i*12+4]<<','
			<<g_coordinateFrames[i*12+5]<<','
			<<g_coordinateFrames[i*12+6]<<','
			<<g_coordinateFrames[i*12+8]<<','
			<<g_coordinateFrames[i*12+9]<<','
			<<g_coordinateFrames[i*12+10]<<')';
	}
	outfile<<endl;

	//write out the fractional length to root for each vertex
	for(int i=0;i<g_NumMasterStrandControlVertices;i++)
		outfile<<g_masterStrandLengthToRoot[i]<<' ';
	outfile<<endl;

	//write out the total real length for each strand
	for(int i=0;i<g_NumMasterStrands;i++)
		outfile<<g_masterStrandLengths[i]<<' ';
	outfile<<endl;

	//write out the number of strand variables
	outfile<<64<<endl;

	//write out the clump offsets
	for(int i=0;i<64;i++)
		outfile<<'('<<g_clumpCoordinates[i].x<<','<<g_clumpCoordinates[i].y<<')';
	outfile<<endl;

	//write out the strand widths
	for(int i=0;i<64;i++)
		outfile<<g_Strand_Sizes[i]<<' ';
	outfile<<endl;

	//write out the strand lengths
	for(int i=0;i<64;i++)
		outfile<<g_lengths[i]<<' ';
	outfile<<endl;

	//write out the tangent jitter
	for(int i=0;i<64;i++)
		outfile<<'('<<g_tangentJitter[i].x<<','<<g_tangentJitter[i].y<<')';
	outfile<<endl;

	//barycentric hair--------------------------------

	//write out the face count
	int faceCount = g_NumWisps;
	outfile<<faceCount<<endl; 

	//write out the index buffer
	for(int i=0;i<faceCount*3;i++)
		outfile<<g_indices[i]<<" ";
	outfile<<endl;

	//write out the random barycentric coordinates
	for(int i=0;i<64;i++)
		outfile<<'('<<g_BarycentricCoordinates[i].x<<','<<g_BarycentricCoordinates[i].y<<')';
	outfile<<endl;

	outfile.close();
	return hr;
}


//random numbers; replace with a better quality generator for better particle distribution
//(0,1]
float random()
{
	return (float(   (double)rand() / ((double)(RAND_MAX)+(double)(1)) ));
}



void BoxMullerTransform(float& var1, float& var2)
{
	float unifVar1 = random();
	float unifVar2 = random();
	float temp = sqrt(-2*log(unifVar1));
	var1 = temp*cos(2*D3DX_PI*unifVar2);
	var2 = temp*sin(2*D3DX_PI*unifVar2);
}



void RGBtoHSV( float r, float g, float b, float *h, float *s, float *v )
{
	float minVal, maxVal, delta;

	minVal = min( min(r, g), b );
	maxVal = max( max(r, g), b );
	*v = maxVal;				// v

	delta = maxVal - minVal;

	if( maxVal != 0 )
		*s = delta / maxVal;		// s
	else {
		// r = g = b = 0		// s = 0, v is undefined
		*s = 0;
		*h = -1;
		return;
	}

	if( r == maxVal )
		*h = ( g - b ) / delta;		// between yellow & magenta
	else if( g == maxVal )
		*h = 2 + ( b - r ) / delta;	// between cyan & yellow
	else
		*h = 4 + ( r - g ) / delta;	// between magenta & cyan

	*h *= 60;				// degrees
	if( *h < 0 )
		*h += 360;

}

void HSVtoRGB( float *r, float *g, float *b, float h, float s, float v )
{
	int i;
	float f, p, q, t;

	if( s == 0 ) {
		// achromatic (grey)
		*r = *g = *b = v;
		return;
	}

	h /= 60;			// sector 0 to 5
	i = floor( h );
	f = h - i;			// factorial part of h
	p = v * ( 1 - s );
	q = v * ( 1 - s * f );
	t = v * ( 1 - s * ( 1 - f ) );

	switch( i ) {
case 0:
	*r = v;
	*g = t;
	*b = p;
	break;
case 1:
	*r = q;
	*g = v;
	*b = p;
	break;
case 2:
	*r = p;
	*g = v;
	*b = t;
	break;
case 3:
	*r = p;
	*g = q;
	*b = v;
	break;
case 4:
	*r = t;
	*g = p;
	*b = v;
	break;
default:		// case 5:
	*r = v;
	*g = p;
	*b = q;
	break;
	}

}

// r,g,b values are from 0 to 1
// h = [0,360], s = [0,1], v = [0,1]
//		if s == 0, then h = -1 (undefined)

//create random strand colors
HRESULT CreateStrandColors(ID3D11Device* pd3dDevice)
{
	D3D11_SUBRESOURCE_DATA initialData;
	float3* colors = new float3[g_NumStrandVariables];


	float3 baseColorRGB = float3(117/255.0f,76/255.0f,36/255.0f);
	float3 baseColorHSV; //this is the mean
	RGBtoHSV(baseColorRGB.x,baseColorRGB.y,baseColorRGB.z,&baseColorHSV.x,&baseColorHSV.y,&baseColorHSV.z);
	float3 HSVSDs = float3(1,0.05f,0.05f);

	for (int i = 0; i < g_NumStrandVariables; ++i) 
	{
		float h,s,v,t,r,g,b;
		BoxMullerTransform(h,s);
		BoxMullerTransform(v,t); 
		h = HSVSDs.x*h + baseColorHSV.x;
		s = HSVSDs.y*s + baseColorHSV.y;
		v = HSVSDs.z*v + baseColorHSV.z;
		HSVtoRGB(&r,&g,&b,h,s,v);
		colors[i] = float3(r,g,b);

	}

	ID3D11Buffer* StrandColors;
	initialData.pSysMem = colors;
	HRESULT hr;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumStrandVariables * sizeof(float3);
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &StrandColors));
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumStrandVariables;
	SAFE_RELEASE(g_StrandColorsRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(StrandColors, &desc, &g_StrandColorsRV));
	g_Effect->GetVariableByName("g_StrandColors")->AsShaderResource()->SetResource(g_StrandColorsRV);
	delete [] colors;
	SAFE_RELEASE(StrandColors);
	return S_OK;
}


//create strand deviations and strand sizes
HRESULT CreateDeviantStrandsAndStrandSizes(ID3D11Device* pd3dDevice)
{
	D3D11_SUBRESOURCE_DATA initialData;
	float2* randStrandDeviations = new float2[g_NumStrandVariables*g_TessellatedMasterStrandLengthMax];


	//deviant strands-------------------------------------------------------------------------
	//using bsplines:
	int numCVs = 7;
	int numSegments = numCVs - 3;
	int numVerticesPerSegment = ceil(g_TessellatedMasterStrandLengthMax/float(numSegments));
	float uStep = 1.0/numVerticesPerSegment;
	D3DXVECTOR2* CVs = new D3DXVECTOR2[numCVs];
	CVs[0] = D3DXVECTOR2(0,0);
	CVs[1] = D3DXVECTOR2(0,0);
	float x,y;
	int index = 0;
	int lastIndex = 0;

	D3DXMATRIX basisMatrix = D3DXMATRIX
		(
		-1/6.0f,  3/6.0f, -3/6.0f,  1/6.0f,
		3/6.0f, -6/6.0f,      0,  4/6.0f,
		-3/6.0f,  3/6.0f,  3/6.0f,  1/6.0f,
		1/6.0f,      0,       0,      0
		);


	//strand sizes---------------------------------------------------------------------------
	//make the deviant strands thinner
	g_Strand_Sizes = new float[g_NumStrandVariables];
	float size;

	float minSize;
	float threshold;
	float variance;

	minSize = 0.06f;
	threshold = 0.75f;
	variance = 0.0208f;

	//-------------------------------------------------------------------------------------

	float sdScale = 1.0f;
	if(g_useShortHair) sdScale = 0.6f;

	for (int i = 0; i < g_NumStrandVariables; i++) 
	{
		float randomChoice = random();
		if(randomChoice>0.6)
		{
			float maxLength = (1.0-g_thinning) + g_thinning*g_lengths[i%g_NumStrandVariables];//between 0 and 1
			int maxCV = floor(maxLength*numSegments)+ 2;//can do 1 here with sd=1.2;

			//create the random CVs
			for(int j=2;j<numCVs;j++)
			{   
				float sd;

				if(randomChoice > 0.95)//make some very stray hair
					sd = 1.2f;
				else if(randomChoice > 0.8)
					sd = 0.8f;
				else //make some lesser stray hair that are more deviant near the ends
				{
					if(maxLength>((numCVs-1)/numCVs) && j==numCVs-1)    
						sd = 100.0f;
					else if(j>=maxCV)
						sd = 4.0f;
					else 
						sd = 0.12f;
				}			 

				BoxMullerTransform(x,y);
				x *= sd * sdScale;
				y *= sd * sdScale; 
				CVs[j] = D3DXVECTOR2(x,y);
			}

			//create the points
			for(int s=0;s<numSegments;s++)
			{
				for(float u=0;u<1.0;u+=uStep)
				{  
					float4 basis;
					vectorMatrixMultiply(&basis,basisMatrix,float4(u * u * u, u * u, u, 1));  
					float2 position = float2(0,0);
					for (int c = 0; c < 4; ++c) 
						position += basis[c] * CVs[s+c];
					if( index-lastIndex < (int)g_TessellatedMasterStrandLengthMax )
						randStrandDeviations[index++] = position;
				}
			}
		}
		else
		{
			for(unsigned int j=0;j<g_TessellatedMasterStrandLengthMax;j++)
				randStrandDeviations[index++] = float2(0,0);
		}


		//sizes
		size =  minSize;
		if( randomChoice > 0.9)
			size -= variance;
		else if( randomChoice > threshold)
			size += (random()-0.5)*variance;
		g_Strand_Sizes[i] = size;



		lastIndex = index;
	}

	//random deviations---------------------------------------------------------------------------
	ID3D11Buffer* randNumbersVB;
	initialData.pSysMem = randStrandDeviations;
	HRESULT hr;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumStrandVariables*g_TessellatedMasterStrandLengthMax * sizeof(float2);
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &randNumbersVB));
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_R32G32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumStrandVariables*g_TessellatedMasterStrandLengthMax;
	SAFE_RELEASE(g_StrandDeviationsRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(randNumbersVB, &desc, &g_StrandDeviationsRV));
	g_Effect->GetVariableByName("g_strandDeviations")->AsShaderResource()->SetResource(g_StrandDeviationsRV);
	SAFE_RELEASE(randNumbersVB);
	delete[] randStrandDeviations;

	//sizes-----------------------------------------------------------------------------------------

	initialData.pSysMem = g_Strand_Sizes;
	bufferDesc.ByteWidth = g_NumStrandVariables * sizeof(float);
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	SAFE_RELEASE(g_StrandSizes);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_StrandSizes));
	desc.Format = DXGI_FORMAT_R32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumStrandVariables;
	SAFE_RELEASE(g_StrandSizesRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_StrandSizes, &desc, &g_StrandSizesRV));
	g_Effect->GetVariableByName("g_StrandSizes")->AsShaderResource()->SetResource(g_StrandSizesRV);
	//delete [] sizes;

	delete[] CVs;

	return S_OK;
}



HRESULT CreateCurlDeviations(ID3D11Device* pd3dDevice)
{
	//curls---------------------------------------------------------------------------------------
	HRESULT hr;

	int numCVs = 13;
	float sd = 1.5;
    if(g_useShortHair)
	{
		numCVs = 10;
		sd = 1.2f;
	}
	int numSegments = numCVs - 3;
	int numVerticesPerSegment = ceil(g_TessellatedMasterStrandLengthMax/float(numSegments));
	float uStep = 1.0/numVerticesPerSegment;
	D3DXVECTOR2* CVs = new D3DXVECTOR2[numCVs];
	CVs[0] = D3DXVECTOR2(0,0);
	CVs[1] = D3DXVECTOR2(0,0);
	CVs[2] = D3DXVECTOR2(0,0);
	CVs[3] = D3DXVECTOR2(0,0);
	float x,y;
	int index = 0;
	int lastIndex = 0;

	D3DXMATRIX basisMatrix = D3DXMATRIX
		(
		-1/6.0f,  3/6.0f, -3/6.0f,  1/6.0f,
		3/6.0f, -6/6.0f,      0,  4/6.0f,
		-3/6.0f,  3/6.0f,  3/6.0f,  1/6.0f,
		1/6.0f,      0,       0,      0
		);


	float2* curlDeviations = new float2[g_NumMasterStrands*g_TessellatedMasterStrandLengthMax];
	for (int i = 0; i < g_NumMasterStrands; i++) 
	{
		//create the random CVs
		for(int j=4;j<numCVs;j++)
		{   
			BoxMullerTransform(x,y);
			x *= sd;
			y *= sd; 
			CVs[j] = D3DXVECTOR2(x,y);
		}

		//create the points
		for(int s=0;s<numSegments;s++)
		{
			for(float u=0;u<1.0;u+=uStep)
			{  
				float4 basis;
				vectorMatrixMultiply(&basis,basisMatrix,float4(u * u * u, u * u, u, 1));  
				float2 position = float2(0,0);
				for (int c = 0; c < 4; ++c) 
					position += basis[c] * CVs[s+c];
				if( index-lastIndex < (int)g_TessellatedMasterStrandLengthMax )
					curlDeviations[index++] = position;
			}
		}

		lastIndex = index;
	}

	ID3D11Buffer* curlsVB;
	D3D11_SUBRESOURCE_DATA initialData;
	initialData.pSysMem = curlDeviations;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumMasterStrands*g_TessellatedMasterStrandLengthMax * sizeof(float2);
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &curlsVB));
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_R32G32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumMasterStrands*g_TessellatedMasterStrandLengthMax;
	SAFE_RELEASE(g_CurlDeviationsRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(curlsVB, &desc, &g_CurlDeviationsRV));
	g_Effect->GetVariableByName("g_curlDeviations")->AsShaderResource()->SetResource(g_CurlDeviationsRV);
	SAFE_RELEASE(curlsVB);
	delete[] curlDeviations;
	delete[] CVs;

	return S_OK;
}

HRESULT CreateStrandLengths(ID3D11Device* pd3dDevice)
{
	D3D11_SUBRESOURCE_DATA initialData;
	g_lengths = new float[g_NumStrandVariables];

	float minLength = 0.2f;

	for (int i = 0; i < g_NumStrandVariables; ++i) 
		g_lengths[i] = random()*(1-minLength) + minLength;

	ID3D11Buffer* StrandLengths;
	initialData.pSysMem = g_lengths;
	HRESULT hr;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumStrandVariables * sizeof(float);
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &StrandLengths));
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_R32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumStrandVariables;
	SAFE_RELEASE(g_StrandLengthsRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(StrandLengths, &desc, &g_StrandLengthsRV));
	g_Effect->GetVariableByName("g_StrandLengths")->AsShaderResource()->SetResource(g_StrandLengthsRV);
    SAFE_RELEASE(StrandLengths);

	return S_OK;
}

HRESULT CreateStrandBarycentricCoordinates(ID3D11Device* pd3dDevice)
{
	D3D11_SUBRESOURCE_DATA initialData;
	if(g_BarycentricCoordinates)
		delete[] g_BarycentricCoordinates;
	g_BarycentricCoordinates = new D3DXVECTOR2[g_NumStrandVariables];
	float coord1 = 1;
	float coord2 = 0;
	for (int c = 0; c < g_NumStrandVariables; ++c) 
	{
		coord1 = random();
		coord2 = random();
		if(coord1 + coord2 > 1)
		{
			coord1 = 1.0 - coord1;
			coord2 = 1.0 - coord2;
		}


		g_BarycentricCoordinates[c] = D3DXVECTOR2(coord1, coord2);
	}
	initialData.pSysMem = g_BarycentricCoordinates;
	HRESULT hr;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumStrandVariables * sizeof(D3DXVECTOR2);
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	SAFE_RELEASE(g_StrandCoordinates);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_StrandCoordinates));
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_R32G32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumStrandVariables;
	SAFE_RELEASE(g_StrandCoordinatesRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_StrandCoordinates, &desc, &g_StrandCoordinatesRV));
	g_Effect->GetVariableByName("g_StrandCoordinates")->AsShaderResource()->SetResource(g_StrandCoordinatesRV);
	return S_OK;
}



HRESULT CreateTangentJitter(ID3D11Device* pd3dDevice)
{
	D3D11_SUBRESOURCE_DATA initialData;
	if(g_tangentJitter) delete[] g_tangentJitter;
	g_tangentJitter = new D3DXVECTOR2[g_NumStrandVariables];
	float val1 = 0;
	float val2 = 0;
	for (int c = 0; c < g_NumStrandVariables; ++c) 
	{
		val1 = random()-0.5;
		val2 = random()-0.5;
		g_tangentJitter[c] = D3DXVECTOR2(val1, val2);
	}
	ID3D11Buffer* TangentJitterVB;
	initialData.pSysMem = g_tangentJitter;
	HRESULT hr;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumStrandVariables * sizeof(D3DXVECTOR2);
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &TangentJitterVB));
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_R32G32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumStrandVariables;
	SAFE_RELEASE(g_TangentJitterRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(TangentJitterVB, &desc, &g_TangentJitterRV));
	g_Effect->GetVariableByName("g_TangentJitter")->AsShaderResource()->SetResource(g_TangentJitterRV);
	//delete [] jitter;
	SAFE_RELEASE(TangentJitterVB);
	return S_OK;
}


HRESULT CreateStrandToClumpIndexMap(ID3D11Device* pd3dDevice)
{
	HRESULT hr = S_OK;

	//map the texture for reading
	char filename[MAX_PATH];
	WCHAR wfilename[MAX_PATH];
	WCHAR fullPath[MAX_PATH];

	sprintf_s(filename,100,"%s\\%s",g_loadDirectory,"clumpMapBarycentric.raw");
	mbstowcs_s(NULL, wfilename, MAX_PATH, filename, MAX_PATH);
	V_RETURN(NVUTFindDXSDKMediaFileCch( fullPath, MAX_PATH, wfilename));
	wcstombs_s(NULL,filename,MAX_PATH,fullPath,MAX_PATH);

	FILE * inTexture;
	errno_t error;
	if((error = fopen_s(&inTexture,filename,"r")) != 0)
	{
        MessageBoxA(NULL,"Could not find file clumpMapBarycentric.raw","Error",MB_OK);
		return S_FALSE;
	}

#define textureSize 256
	float texture[textureSize][textureSize];
	unsigned char * buffer;
	buffer = (unsigned char*) malloc (sizeof(unsigned char)*textureSize*textureSize);
	fread (buffer,sizeof(unsigned char),textureSize*textureSize,inTexture);

	int ind=0;

	//read the texture
	for(int y=0;y<textureSize;y++)
		for(int x=0;x<textureSize;x++)
			texture[y][x] = float(buffer[ind++])/255.0;

	fclose(inTexture);

	//for each masterStrand sample the texture, and decide on how many strands go in the clump, save this info
	int* numStrandsCumulativeTri = new int[g_NumMasterStrands];
	g_totalSStrands = 0;
	int minSStrands = 1 << 30;
	int maxSStrands = 0;
	int avgSStrands = 0;
	for(int i=0;i<g_NumMasterStrands;i++)
	{
		D3DXVECTOR2 Texcoord = g_MasterAttributes[i].texcoord;
		int num = texture[int(Texcoord.y*256)][int(Texcoord.x*256)]*g_NumSStrandsPerWisp;
		numStrandsCumulativeTri[i] = num;
		minSStrands = min(num, minSStrands);
		maxSStrands = max(num, maxSStrands);
		avgSStrands += num;
		if(i>0)
			numStrandsCumulativeTri[i] += numStrandsCumulativeTri[i-1];		
	}
	avgSStrands /= g_NumMasterStrands;
	g_MaxSStrandsPerControlHair = maxSStrands;
	g_totalSStrands = numStrandsCumulativeTri[g_NumMasterStrands-1];

	//upload map to buffer
	{
	ID3D11Buffer* SStrandsPerMasterStrandCumulative;
	D3D11_SUBRESOURCE_DATA initialData;
	initialData.pSysMem = numStrandsCumulativeTri;
	initialData.SysMemPitch = 0;
	initialData.SysMemSlicePitch = 0;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumMasterStrands * sizeof(unsigned); 
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &SStrandsPerMasterStrandCumulative));
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	ZeroMemory( &desc, sizeof(desc) );
	desc.Format = DXGI_FORMAT_R32_SINT; 
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumMasterStrands;
	SAFE_RELEASE(g_SStrandsPerMasterStrandCumulative);
	V_RETURN(pd3dDevice->CreateShaderResourceView(SStrandsPerMasterStrandCumulative, &desc, &g_SStrandsPerMasterStrandCumulative));
	g_Effect->GetVariableByName("g_SStrandsPerMasterStrandCumulative")->AsShaderResource()->SetResource(g_SStrandsPerMasterStrandCumulative);
	SAFE_RELEASE(SStrandsPerMasterStrandCumulative);
	}

	//make a map where each strand is mapped to its masterStrand number
	float* Map=new float[g_totalSStrands];
	int currentTriIndex=0;
	int previosCNumber = 0;
	for(int i=0;i<g_totalSStrands;i++)
	{
		while(i>=numStrandsCumulativeTri[currentTriIndex])
		{
			previosCNumber = numStrandsCumulativeTri[currentTriIndex];
			currentTriIndex++;
		}
		Map[i] = currentTriIndex;

		Map[i] += (i-previosCNumber)/1000.0;
	}


	//upload map to buffer
	ID3D11Buffer* SstrandsIndexVB;
	D3D11_SUBRESOURCE_DATA initialData;
	initialData.pSysMem = Map;
	initialData.SysMemPitch = 0;
	initialData.SysMemSlicePitch = 0;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_totalSStrands * sizeof(float); 
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &SstrandsIndexVB));
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	ZeroMemory( &desc, sizeof(desc) );
	desc.Format = DXGI_FORMAT_R32_FLOAT; 
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_totalSStrands;
	SAFE_RELEASE(g_SstrandsIndexRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(SstrandsIndexVB, &desc, &g_SstrandsIndexRV));
	g_Effect->GetVariableByName("g_sStrandIndices")->AsShaderResource()->SetResource(g_SstrandsIndexRV);
	SAFE_RELEASE(SstrandsIndexVB);

	free (buffer);
	delete[] Map;
	delete[] numStrandsCumulativeTri;

	return hr;
}

HRESULT CreateStrandToBarycentricIndexMap(ID3D11Device* pd3dDevice)
{
	HRESULT hr = S_OK;

	//map the texture for reading
	char filename[MAX_PATH];
	WCHAR wfilename[MAX_PATH];
	TCHAR fullPath[MAX_PATH];

	sprintf_s(filename,100,"%s\\%s",g_loadDirectory,"densityMapBarycentric.raw");
	mbstowcs_s(NULL, wfilename, MAX_PATH, filename, MAX_PATH);
	V_RETURN(NVUTFindDXSDKMediaFileCch( fullPath, MAX_PATH, wfilename));
	wcstombs_s(NULL,filename,MAX_PATH,fullPath,MAX_PATH);

	FILE * inTexture;
	errno_t error;
	if((error = fopen_s(&inTexture,filename,"r")) != 0)
	{
        MessageBoxA(NULL,"Could not find file densityMapBarycentric.raw","Error",MB_OK);
		return S_FALSE;
	}

#define textureSize 256
	float texture[textureSize][textureSize];
	unsigned char * buffer;
	buffer = (unsigned char*) malloc (sizeof(unsigned char)*textureSize*textureSize);
	fread (buffer,sizeof(unsigned char),textureSize*textureSize,inTexture);

	int ind=0;

	//read the texture
	for(int y=0;y<textureSize;y++)
		for(int x=0;x<textureSize;x++)
			texture[y][x] = float(buffer[ind++])/255.0;

	fclose(inTexture);

	//for each triangle average its 3 texcoords, sample the texture, and decide on how many strands go in the triangle, save this info
	int* numStrandsCumulativeTri = new int[g_NumWisps];
	g_totalMStrands = 0;
	g_MaxMStrandsPerWisp = 0;
	for(int i=0;i<g_NumWisps;i++)
	{
		D3DXVECTOR3 indices = g_masterCVIndices[i];
		D3DXVECTOR2 averageTexcoord= D3DXVECTOR2(0,0);
		averageTexcoord += 0.3333f*g_MasterAttributes[(int)indices.x].texcoord;
		averageTexcoord += 0.3333f*g_MasterAttributes[(int)indices.y].texcoord;
		averageTexcoord += 0.3333f*g_MasterAttributes[(int)indices.z].texcoord;

		int num = texture[int(averageTexcoord.y*256)][int(averageTexcoord.x*256)]*g_NumMStrandsPerWisp;
		numStrandsCumulativeTri[i] = num;
		g_MaxMStrandsPerWisp = max((unsigned)num, g_MaxMStrandsPerWisp);
		if(i>0)
			numStrandsCumulativeTri[i] += numStrandsCumulativeTri[i-1];		
	}
	g_totalMStrands = numStrandsCumulativeTri[g_NumWisps-1];

	//upload map to buffer
	{
	ID3D11Buffer* MStrandsPerWispCumulative;
	D3D11_SUBRESOURCE_DATA initialData;
	initialData.pSysMem = numStrandsCumulativeTri;
	initialData.SysMemPitch = 0;
	initialData.SysMemSlicePitch = 0;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumWisps * sizeof(unsigned); 
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &MStrandsPerWispCumulative));
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	ZeroMemory( &desc, sizeof(desc) );
	desc.Format = DXGI_FORMAT_R32_SINT; 
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumWisps;
	SAFE_RELEASE(g_MStrandsPerWispCumulative);
	V_RETURN(pd3dDevice->CreateShaderResourceView(MStrandsPerWispCumulative, &desc, &g_MStrandsPerWispCumulative));
	g_Effect->GetVariableByName("g_MStrandsPerWispCumulative")->AsShaderResource()->SetResource(g_MStrandsPerWispCumulative);
	SAFE_RELEASE(MStrandsPerWispCumulative);
	}

	//make a map where each strand is mapped to its triangle number
	float* Map=new float[g_totalMStrands];
	int currentTriIndex=0;
	int previosCNumber = 0;
	for(int i=0;i<g_totalMStrands;i++)
	{
		while(i>=numStrandsCumulativeTri[currentTriIndex])
		{
			previosCNumber = numStrandsCumulativeTri[currentTriIndex];
			currentTriIndex++;
		}
		Map[i] = currentTriIndex;

		Map[i] += (i-previosCNumber)/1000.0;
	}


	//upload map to buffer
	ID3D11Buffer* MstrandsIndexVB;
	D3D11_SUBRESOURCE_DATA initialData;
	initialData.pSysMem = Map;
	initialData.SysMemPitch = 0;
	initialData.SysMemSlicePitch = 0;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_totalMStrands * sizeof(float); 
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &MstrandsIndexVB));
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	ZeroMemory( &desc, sizeof(desc) );
	desc.Format = DXGI_FORMAT_R32_FLOAT; 
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_totalMStrands;
	SAFE_RELEASE(g_MstrandsIndexRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(MstrandsIndexVB, &desc, &g_MstrandsIndexRV));
	g_Effect->GetVariableByName("g_mStrandIndices")->AsShaderResource()->SetResource(g_MstrandsIndexRV);
	SAFE_RELEASE(MstrandsIndexVB);

	free (buffer);
	delete[] Map;
	delete[] numStrandsCumulativeTri;

	return hr;
}

HRESULT CreateRandomCircularCoordinates(ID3D11Device* pd3dDevice)
{
	if(g_clumpCoordinates)
		delete[] g_clumpCoordinates;
	g_clumpCoordinates = new D3DXVECTOR2[g_NumStrandVariables];
	//create two coordinates that lie inside the unit circle
	int c = 0;
	float coord1 = 0;
	float coord2 = 0;
	g_clumpCoordinates[c++] = D3DXVECTOR2(coord1, coord2);
	while(c<g_NumStrandVariables)
	{
		//distributed with a gaussian distribution
		BoxMullerTransform(coord1,coord2);
		float sd = 0.25;
		coord1 *= sd;
		coord2 *= sd;


		if( sqrt(sqr(coord1) + sqr(coord2)) < 1 )
			g_clumpCoordinates[c++] = D3DXVECTOR2(coord1, coord2);	
	}

	D3D11_SUBRESOURCE_DATA initialData;
	initialData.pSysMem = g_clumpCoordinates;
	HRESULT hr;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumStrandVariables * sizeof(D3DXVECTOR2);
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	ID3D11Buffer* strandCircularCoordinates;
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &strandCircularCoordinates));
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_R32G32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumStrandVariables;
	SAFE_RELEASE(g_StrandCircularCoordinatesRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(strandCircularCoordinates, &desc, &g_StrandCircularCoordinatesRV));
	g_Effect->GetVariableByName("g_StrandCircularCoordinates")->AsShaderResource()->SetResource(g_StrandCircularCoordinatesRV);
	SAFE_RELEASE(strandCircularCoordinates);
	return S_OK;
}


HRESULT CreateStrandAttributes(ID3D11Device* pd3dDevice)
{
	D3D11_SUBRESOURCE_DATA initialData;
	initialData.pSysMem = g_MasterAttributes;
	HRESULT hr;
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = g_NumMasterStrands * sizeof(Attributes);
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	SAFE_RELEASE(g_MasterAttributesBuffer);
	V_RETURN(pd3dDevice->CreateBuffer(&bufferDesc, &initialData, &g_MasterAttributesBuffer));
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	desc.Format = DXGI_FORMAT_R32G32_FLOAT;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.ElementOffset = 0;
	desc.Buffer.ElementWidth = g_NumMasterStrands;
	SAFE_RELEASE(g_StrandAttributesRV);
	V_RETURN(pd3dDevice->CreateShaderResourceView(g_MasterAttributesBuffer, &desc, &g_StrandAttributesRV));
	g_Effect->GetVariableByName("g_Attributes")->AsShaderResource()->SetResource(g_StrandAttributesRV);
	return S_OK;
}

//--------------------------------------------------------------------------------------
// Create input layouts 
//--------------------------------------------------------------------------------------
HRESULT CreateInputLayouts(ID3D11Device* pd3dDevice)
{
	HRESULT hr;
	V_RETURN(CreateMasterStrandIL(pd3dDevice));
	V_RETURN(CreateMasterStrandTesselatedInputIL(pd3dDevice));
	return S_OK;
}

HRESULT CreateMasterStrandIL(ID3D11Device* pd3dDevice)
{
	HRESULT hr;

	const D3D11_INPUT_ELEMENT_DESC elements2[] =
	{
		{ "POSITION",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "POSITION",  1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	D3DX11_PASS_DESC passDesc2;    
	V_RETURN(g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("addForcesAndIntegrate")->GetDesc(&passDesc2));
	V_RETURN(pd3dDevice->CreateInputLayout(elements2, sizeof(elements2) / sizeof(D3D11_INPUT_ELEMENT_DESC), passDesc2.pIAInputSignature, passDesc2.IAInputSignatureSize, &g_MasterStrandILAddForces));


	const D3D11_INPUT_ELEMENT_DESC elements[] =
	{
		{ "Position", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	D3DX11_PASS_DESC passDesc;    
	V_RETURN(g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("SatisfyCollisionConstraints")->GetDesc(&passDesc));
	V_RETURN(pd3dDevice->CreateInputLayout(elements, sizeof(elements) / sizeof(D3D11_INPUT_ELEMENT_DESC), passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &g_MasterStrandIL));


	const D3D11_INPUT_ELEMENT_DESC elements3[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "LENGTH",   0, DXGI_FORMAT_R32_FLOAT,          1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	V_RETURN(g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("SatisfySpringConstraints")->GetDesc(&passDesc));
	V_RETURN(pd3dDevice->CreateInputLayout(elements3, sizeof(elements3) / sizeof(D3D11_INPUT_ELEMENT_DESC), passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &g_MasterStrandSpringConstraintsIL));

	const D3D11_INPUT_ELEMENT_DESC elements4[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "LENGTH",   0, DXGI_FORMAT_R32G32_FLOAT,       1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	V_RETURN(g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("SatisfyAngularSpringConstraints")->GetDesc(&passDesc));
	V_RETURN(pd3dDevice->CreateInputLayout(elements4, sizeof(elements4) / sizeof(D3D11_INPUT_ELEMENT_DESC), passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &g_MasterStrandAngularSpringConstraintsIL));

	const D3D11_INPUT_ELEMENT_DESC elements6[] =
	{
		{ "X_AXIS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "Y_AXIS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "Z_AXIS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },

	};
	V_RETURN(g_Effect->GetTechniqueByName("SimulateHair")->GetPassByName("PropagateCoordinateFrame")->GetDesc(&passDesc));
	V_RETURN(pd3dDevice->CreateInputLayout(elements6, sizeof(elements6) / sizeof(D3D11_INPUT_ELEMENT_DESC), passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &g_CoordinateFrameIL));

	D3D11_INPUT_ELEMENT_DESC elements7[] = 
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,       0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT,       0,12, D3D11_INPUT_PER_VERTEX_DATA, 0 }, 
	};
	V_RETURN(g_Effect->GetTechniqueByName("DrawTexture")->GetPassByName("drawCollisionsTexture")->GetDesc(&passDesc));
	V_RETURN(pd3dDevice->CreateInputLayout(elements7, sizeof(elements7) / sizeof(D3D11_INPUT_ELEMENT_DESC), passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &g_screenQuadIL));


	return S_OK;
}

HRESULT CreateMasterStrandTesselatedInputIL(ID3D11Device* pd3dDevice)
{
	HRESULT hr;

	const D3D11_INPUT_ELEMENT_DESC elements[] =
	{
		{ "controlVertexIndex", 0, DXGI_FORMAT_R32_UINT , 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "u"                 , 0, DXGI_FORMAT_R32_FLOAT, 0, 4, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	D3DX11_PASS_DESC passDesc;    
	V_RETURN(g_Effect->GetTechniqueByName("RenderHair")->GetPassByName("Tessellate")->GetDesc(&passDesc));
	V_RETURN(pd3dDevice->CreateInputLayout(elements, sizeof(elements) / sizeof(D3D11_INPUT_ELEMENT_DESC), passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &g_MasterStrandTesselatedInputIL));

	return S_OK;
}
//--------------------------------------------------------------------------------------
// Release resources 
//--------------------------------------------------------------------------------------
void ReleaseResources()
{
	if(g_TessellatedMasterStrandVertexOffsets)
	{
		delete[] g_TessellatedMasterStrandVertexOffsets;
		g_TessellatedMasterStrandVertexOffsets = NULL;
	}
	if(g_MasterStrandControlVertices)
	{
		delete[] g_MasterStrandControlVertices;
		g_MasterStrandControlVertices = NULL;
	}
	if(g_MasterStrandControlVertexOffsets)
	{
		delete[] g_MasterStrandControlVertexOffsets;
		g_MasterStrandControlVertexOffsets = NULL;
	}
	if(g_MasterStrandControlVertices)
	{
		delete[] g_MasterStrandControlVertices;
		g_MasterStrandControlVertices = NULL;
	}
	if(g_coordinateFrames)
	{
		delete[] g_coordinateFrames;
		g_coordinateFrames = NULL;
	}
	if(g_masterCVIndices)
	{
    	delete[] g_masterCVIndices;
		g_masterCVIndices = NULL;
	}
	if(g_clumpCoordinates)
	{
		delete[] g_clumpCoordinates;
		g_clumpCoordinates = NULL;
	}
	if(g_masterStrandLengthToRoot)
	{
		delete [] g_masterStrandLengthToRoot;
		g_masterStrandLengthToRoot = NULL;
	}
	if(g_masterStrandLengths)
	{
		delete [] g_masterStrandLengths;
		g_masterStrandLengths = NULL;
	}
	if(g_Strand_Sizes)
	{
		delete [] g_Strand_Sizes;
		g_Strand_Sizes = NULL;
	}
	if(g_tangentJitter)
	{
		delete [] g_tangentJitter;
		g_tangentJitter = NULL; 
	}
	if(g_BarycentricCoordinates)
	{
		delete [] g_BarycentricCoordinates;
		g_BarycentricCoordinates = NULL; 
	}
	delete [] g_lengths;
	delete [] g_MasterAttributes;

	//input layouts
	SAFE_RELEASE(g_MasterStrandIL);
	SAFE_RELEASE(g_CoordinateFrameIL);
	SAFE_RELEASE(g_MasterStrandILAddForces);
	SAFE_RELEASE(g_MasterStrandSpringConstraintsIL);
	SAFE_RELEASE(g_MasterStrandAngularSpringConstraintsIL);
	SAFE_RELEASE(g_MasterStrandTesselatedInputIL);
	SAFE_RELEASE(g_screenQuadIL);

	//index buffers
	SAFE_RELEASE(g_MasterStrandSimulation1IB);
	SAFE_RELEASE(g_MasterStrandSimulation2IB);
	SAFE_RELEASE(g_MasterStrandSimulationAngular1IB);
	SAFE_RELEASE(g_MasterStrandSimulationAngular2IB);

	//vertex buffers
	SAFE_RELEASE(g_MasterStrandVB[0]);
	SAFE_RELEASE(g_MasterStrandVB[1]);
	SAFE_RELEASE(g_ForceVB[0]);
	SAFE_RELEASE(g_ForceVB[1]);
	SAFE_RELEASE(g_TessellatedMasterStrandVB);
	SAFE_RELEASE(g_TessellatedMasterStrandVBInitial);
	SAFE_RELEASE(g_MasterStrandLinearSpringLengthsVB);
	SAFE_RELEASE(g_MasterStrandAngularSpringConstraintsVB);
	SAFE_RELEASE(g_TessellatedWispVB);
	SAFE_RELEASE(g_MasterStrandVBOldTemp);
	SAFE_RELEASE(g_MasterStrandVBOld);

    //compute shader variables
	SAFE_RELEASE(g_MasterStrandUAB);
	SAFE_RELEASE(g_MasterStrandPrevUAB);
    SAFE_RELEASE(g_MasterStrandUAB_SRV);
    SAFE_RELEASE(g_MasterStrandUAB_UAV);
    SAFE_RELEASE(g_MasterStrandPrevUAB_UAV);
	SAFE_RELEASE(g_pCSSimulateParticles);
	SAFE_RELEASE(g_MasterStrandLinearSpringLengthsSRV);
    SAFE_RELEASE(g_MasterStrandSimulationIndicesBuffer);
    SAFE_RELEASE(g_MasterStrandSimulationIndicesSRV);
	SAFE_RELEASE(g_pcbCSPerFrame);
	SAFE_RELEASE(g_pcbCSTransforms);
    SAFE_RELEASE(g_pcbCSRootXform);
    SAFE_RELEASE(g_CoordinateFrameUAB_UAV);
    SAFE_RELEASE(g_CoordinateFrameUAB_SRV);
    SAFE_RELEASE(g_CoordinateFrameUAB);
    SAFE_RELEASE(g_AngularStiffnessUAB_SRV);
	SAFE_RELEASE(g_MasterStrandAngularSpringConstraintsSRV);

	SAFE_RELEASE(g_StrandSizes);
	SAFE_RELEASE(g_StrandCoordinates);
	SAFE_RELEASE(g_MasterAttributesBuffer);
	SAFE_RELEASE(g_masterLengths);
	SAFE_RELEASE(g_masterLengthsUntessellated);
	SAFE_RELEASE(g_rootsIndices);
	SAFE_RELEASE(g_rootsIndicesUntessellated);
	SAFE_RELEASE(g_rootsIndicesMasterStrand);
	SAFE_RELEASE(g_UnTessellatedWispVB);
	SAFE_RELEASE(g_CoordinateFrameVB[0]);
	SAFE_RELEASE(g_CoordinateFrameVB[1]);
	SAFE_RELEASE(g_TessellatedCoordinateFrameVB);
	SAFE_RELEASE(g_TessellatedMasterStrandLengthToRootVB);
	SAFE_RELEASE(g_TessellatedTangentsVB);
	SAFE_RELEASE(g_collisionScreenQuadBuffer);
	SAFE_RELEASE(g_ScreenQuadBuffer);
	SAFE_RELEASE(g_SuperSampleScreenQuadBuffer);
	SAFE_RELEASE(g_BarycentricInterpolatedPositionsWidthsVB);
	SAFE_RELEASE(g_BarycentricInterpolatedIdAlphaTexVB);
	SAFE_RELEASE(g_BarycentricInterpolatedTangentVB);
	SAFE_RELEASE(g_ClumpInterpolatedPositionsWidthsVB);
	SAFE_RELEASE(g_ClumpInterpolatedIdAlphaTexVB);
	SAFE_RELEASE(g_ClumpInterpolatedTangentVB);

	//shaders resource views
	SAFE_RELEASE(g_TessellatedMasterStrandRV);
	SAFE_RELEASE(g_MasterStrandRV[0]);
	SAFE_RELEASE(g_MasterStrandRV[1]);
	SAFE_RELEASE(g_ForceRV[0]);
	SAFE_RELEASE(g_ForceRV[1]);
	SAFE_RELEASE(g_CoordinateFrameRV[0]);
	SAFE_RELEASE(g_CoordinateFrameRV[1]);
	SAFE_RELEASE(g_StrandSizesRV);
	SAFE_RELEASE(g_StrandCoordinatesRV);
	SAFE_RELEASE(g_StrandCircularCoordinatesRV);
	SAFE_RELEASE(g_StrandAttributesRV);
	SAFE_RELEASE(g_RootsIndicesRV);
	SAFE_RELEASE(g_RootsIndicesRVUntessellated);
	SAFE_RELEASE(g_MasterLengthsRV);
	SAFE_RELEASE(g_rootsIndicesMasterStrandRV);
	SAFE_RELEASE(g_MasterLengthsRVUntessellated);
	SAFE_RELEASE(g_TessellatedMasterStrandRootIndexRV);
	SAFE_RELEASE(g_TessellatedCoordinateFrameRV);
	SAFE_RELEASE(g_TessellatedMasterStrandLengthToRootRV);
	SAFE_RELEASE(g_StrandLengthsRV);
	SAFE_RELEASE(g_TessellatedTangentsRV);
	SAFE_RELEASE(g_StrandColorsRV);
	SAFE_RELEASE(g_StrandDeviationsRV);
	SAFE_RELEASE(g_CurlDeviationsRV);
	SAFE_RELEASE(g_OriginalMasterStrandRV);
	SAFE_RELEASE(g_CollisionsRV );
	SAFE_RELEASE(g_originalVectorsRV);
	SAFE_RELEASE(g_MasterStrandLocalVertexNumberRV);
	SAFE_RELEASE(hairTextureRV);
	SAFE_RELEASE(g_TangentJitterRV);
	SAFE_RELEASE(g_MasterStrandLengthsRV);
	SAFE_RELEASE(g_MasterStrandLengthToRootRV);
	SAFE_RELEASE(g_pSceneColorBufferShaderResourceView);
	SAFE_RELEASE(g_BarycentricInterpolatedPositionsWidthsRV);
	SAFE_RELEASE(g_BarycentricInterpolatedIdAlphaTexRV);
	SAFE_RELEASE(g_BarycentricInterpolatedTangentRV);
	SAFE_RELEASE(g_ClumpInterpolatedPositionsWidthsRV);
	SAFE_RELEASE(g_ClumpInterpolatedIdAlphaTexRV);
	SAFE_RELEASE(g_ClumpInterpolatedTangentRV);
	SAFE_RELEASE(g_MstrandsIndexRV);
	SAFE_RELEASE(g_SstrandsIndexRV);
	SAFE_RELEASE(g_SStrandsPerMasterStrandCumulative);
	SAFE_RELEASE(g_MStrandsPerWispCumulative);
	SAFE_RELEASE(g_HairBaseRV[0]);
	SAFE_RELEASE(g_HairBaseRV[1]);
	SAFE_RELEASE(g_HairBaseRV[2]);

	//render target views
	SAFE_RELEASE(g_CollisionsRTV );
	SAFE_RELEASE(g_pSceneRTV2X);
	SAFE_RELEASE(g_pSceneDSV2X);

	//textures
	SAFE_RELEASE(g_pSceneDepthBuffer2X);
	SAFE_RELEASE(g_pSceneColorBuffer2X);
	SAFE_RELEASE(g_pSceneColorBuffer2XMS);

}



//--------------------------------------------------------------------------------------
// Helper functions
//--------------------------------------------------------------------------------------

//load a single texture and associated view
HRESULT loadTextureFromFile(LPCWSTR file,LPCSTR shaderTextureName, ID3D11Device* pd3dDevice)
{

	HRESULT hr;

	D3DX11_IMAGE_LOAD_INFO texLoadInfo;
	texLoadInfo.MipLevels = 8;
	texLoadInfo.MipFilter = D3DX10_FILTER_TRIANGLE;
	texLoadInfo.Filter = D3DX10_FILTER_TRIANGLE;
	ID3D11Resource *pRes = NULL;

	WCHAR str[MAX_PATH];
	V_RETURN(NVUTFindDXSDKMediaFileCch(str, MAX_PATH, file));

	V_RETURN( D3DX11CreateTextureFromFile(pd3dDevice, str, &texLoadInfo, NULL, &pRes, &hr ) );
	if( pRes )
	{
		ID3D11Texture2D* texture;
		ID3D11ShaderResourceView* textureRview;
		ID3DX11EffectShaderResourceVariable* textureRVar;

		pRes->QueryInterface( __uuidof( ID3D11Texture2D ), (LPVOID*)&texture );
		D3D11_TEXTURE2D_DESC desc;
		texture->GetDesc( &desc );
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		ZeroMemory( &SRVDesc, sizeof(SRVDesc) );
		SRVDesc.Format = desc.Format;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MostDetailedMip = 0;
		SRVDesc.Texture2D.MipLevels = desc.MipLevels;
		V_RETURN (pd3dDevice->CreateShaderResourceView( texture, &SRVDesc, &textureRview));
		textureRVar = g_Effect->GetVariableByName( shaderTextureName )->AsShaderResource();
		V_RETURN(textureRVar->SetResource( textureRview ));

		SAFE_RELEASE( texture );
		SAFE_RELEASE( textureRview );
	}

	SAFE_RELEASE( pRes );

	return S_OK;
}

//load a single texture and associated view
HRESULT loadTextureFromFile(LPCWSTR file,ID3D11ShaderResourceView** textureRview, ID3D11Device* pd3dDevice)
{

	HRESULT hr;

	D3DX11_IMAGE_LOAD_INFO texLoadInfo;
	texLoadInfo.MipLevels = 8;
	texLoadInfo.MipFilter = D3DX10_FILTER_TRIANGLE;
	texLoadInfo.Filter = D3DX10_FILTER_TRIANGLE;
	ID3D11Resource *pRes = NULL;

	WCHAR str[MAX_PATH];
	V_RETURN(NVUTFindDXSDKMediaFileCch(str, MAX_PATH, file));

	V_RETURN( D3DX11CreateTextureFromFile(pd3dDevice, str, &texLoadInfo, NULL, &pRes, &hr ) );
	if( pRes )
	{
		ID3D11Texture2D* texture;
		
		pRes->QueryInterface( __uuidof( ID3D11Texture2D ), (LPVOID*)&texture );
		D3D11_TEXTURE2D_DESC desc;
		texture->GetDesc( &desc );
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		ZeroMemory( &SRVDesc, sizeof(SRVDesc) );
		SRVDesc.Format = desc.Format;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MostDetailedMip = 0;
		SRVDesc.Texture2D.MipLevels = desc.MipLevels;
		V_RETURN (pd3dDevice->CreateShaderResourceView( texture, &SRVDesc, textureRview));
		SAFE_RELEASE( texture );
	}

	SAFE_RELEASE( pRes );

	return S_OK;
}


float vecLength(HairVertex v1,HairVertex v2)
{
	return sqrt( sqr(v1.Position.x-v2.Position.x) + sqr(v1.Position.y-v2.Position.y) + sqr(v1.Position.z-v2.Position.z) );
}


void vectorMatrixMultiply(D3DXVECTOR3* vecOut,const D3DXMATRIX matrix,const D3DXVECTOR3 vecIn)
{
	vecOut->x = vecIn.x*matrix(0,0) + vecIn.y*matrix(1,0) + vecIn.z*matrix(2,0) + matrix(3,0);
	vecOut->y  = vecIn.x*matrix(0,1)+ vecIn.y*matrix(1,1) + vecIn.z*matrix(2,1) + matrix(3,1);
	vecOut->z = vecIn.x*matrix(0,2) + vecIn.y*matrix(1,2) + vecIn.z*matrix(2,2) + matrix(3,2);
}
void vectorMatrixMultiply(D3DXVECTOR4* vecOut,const D3DXMATRIX matrix,const D3DXVECTOR4 vecIn)
{
	vecOut->x = vecIn.x*matrix(0,0) + vecIn.y*matrix(0,1) + vecIn.z*matrix(0,2) + vecIn.w*matrix(0,3);
	vecOut->y = vecIn.x*matrix(1,0) + vecIn.y*matrix(1,1) + vecIn.z*matrix(1,2) + vecIn.w*matrix(1,3);
	vecOut->z = vecIn.x*matrix(2,0) + vecIn.y*matrix(2,1) + vecIn.z*matrix(2,2) + vecIn.w*matrix(2,3);
	vecOut->w = vecIn.x*matrix(3,0) + vecIn.y*matrix(3,1) + vecIn.z*matrix(3,2) + vecIn.w*matrix(3,3);

}


//note: this method is only valid for when the axis is perpendicular to the prevVec
void rotateVector(const float3& rotationAxis,float theta,const float3& prevVec, float3& newVec)
{
	float3 axisDifference;
	D3DXVec3Subtract(&axisDifference,&rotationAxis,&prevVec);
	if( D3DXVec3Length(&axisDifference)<SMALL_NUMBER || theta < SMALL_NUMBER )
	{
		newVec = prevVec;
		return;
	}

	float c = cos(theta);
	float s = sin(theta);
	float t = 1 - c;
	float x = rotationAxis.x;
	float y = rotationAxis.y;
	float z = rotationAxis.z;

	D3DXMATRIX rotationMatrix( t*x*x + c,   t*x*y - s*z, t*x*z + s*y, 0,
		t*x*y + s*z, t*y*y + c,   t*y*z - s*x, 0,
		t*x*z - s*y, t*y*z + s*x, t*z*z + c  , 0,
		0          , 0          , 0          , 1);

	vectorMatrixMultiply(&newVec,rotationMatrix,prevVec);

}


HRESULT CreateCollisionScreenQuadVB(ID3D11Device* pd3dDevice)
{	
	HRESULT hr;

	GRID_TEXTURE_DISPLAY_STRUCT *renderQuad(NULL);
	renderQuad = new GRID_TEXTURE_DISPLAY_STRUCT[ 6 ];

	//fill render quad
	GRID_TEXTURE_DISPLAY_STRUCT tempVertex1;
	GRID_TEXTURE_DISPLAY_STRUCT tempVertex2;
	GRID_TEXTURE_DISPLAY_STRUCT tempVertex3;
	GRID_TEXTURE_DISPLAY_STRUCT tempVertex4;

	float w = float(MAX_INTERPOLATED_HAIR_PER_WISP);
	float h = float(g_NumWisps);

	float left   = -1.0f + 2.0f/w;
	float right  =  1.0f - 2.0f/w;
	float top    =  1.0f - 2.0f/h;
	float bottom = -1.0f + 2.0f/h;

	tempVertex1.Pos = D3DXVECTOR3( left     , top       , 0.0f      );
	tempVertex1.Tex = D3DXVECTOR3( 0.0f     , 0.0f      , 0.0f      );

	tempVertex2.Pos = D3DXVECTOR3( right    , top       , 0.0f      );
	tempVertex2.Tex = D3DXVECTOR3( 1.0f     , 0.0f      , 0.0f      );

	tempVertex3.Pos = D3DXVECTOR3( right    , bottom    , 0.0f      );
	tempVertex3.Tex = D3DXVECTOR3( 1.0f     , 1.0f      , 0.0f      );

	tempVertex4.Pos = D3DXVECTOR3( left     , bottom    , 0.0f      );
	tempVertex4.Tex = D3DXVECTOR3( 0.0f     , 1.0       , 0.0f      );

	int index = 0;
	renderQuad[index++] = tempVertex1;
	renderQuad[index++] = tempVertex2;
	renderQuad[index++] = tempVertex3;
	renderQuad[index++] = tempVertex1;
	renderQuad[index++] = tempVertex3;
	renderQuad[index++] = tempVertex4;

	D3D11_BUFFER_DESC bd;
	bd.ByteWidth = sizeof(GRID_TEXTURE_DISPLAY_STRUCT)*6;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory( &InitData, sizeof(D3D11_SUBRESOURCE_DATA) );
	InitData.pSysMem = renderQuad;
	InitData.SysMemPitch = 0;
	InitData.SysMemSlicePitch = 0;
	SAFE_RELEASE(g_collisionScreenQuadBuffer);
	V_RETURN( pd3dDevice->CreateBuffer( &bd, &InitData,&g_collisionScreenQuadBuffer  ) ); 	

	delete [] renderQuad;
	renderQuad = NULL;
	return S_OK;
}

//this is the size of the back buffer
HRESULT CreateScreenQuadVB(ID3D11Device* pd3dDevice)
{	
	HRESULT hr;

	SAFE_RELEASE(g_ScreenQuadBuffer);

	GRID_TEXTURE_DISPLAY_STRUCT *renderQuad(NULL);
	renderQuad = new GRID_TEXTURE_DISPLAY_STRUCT[ 6 ];

	//fill render quad
	GRID_TEXTURE_DISPLAY_STRUCT tempVertex1;
	GRID_TEXTURE_DISPLAY_STRUCT tempVertex2;
	GRID_TEXTURE_DISPLAY_STRUCT tempVertex3;
	GRID_TEXTURE_DISPLAY_STRUCT tempVertex4;

	float left   = -1.0f;
	float right  =  1.0f;
	float top    =  1.0f;
	float bottom = -1.0f;

	tempVertex1.Pos = D3DXVECTOR3( left     , top       , 0.0f      );
	tempVertex1.Tex = D3DXVECTOR3( 0.0f     , 0.0f      , 0.0f      );

	tempVertex2.Pos = D3DXVECTOR3( right    , top       , 0.0f      );
	tempVertex2.Tex = D3DXVECTOR3( 1.0f     , 0.0f      , 0.0f      );

	tempVertex3.Pos = D3DXVECTOR3( right    , bottom    , 0.0f      );
	tempVertex3.Tex = D3DXVECTOR3( 1.0f     , 1.0f      , 0.0f      );

	tempVertex4.Pos = D3DXVECTOR3( left     , bottom    , 0.0f      );
	tempVertex4.Tex = D3DXVECTOR3( 0.0f     , 1.0       , 0.0f      );

	int index = 0;
	renderQuad[index++] = tempVertex1;
	renderQuad[index++] = tempVertex2;
	renderQuad[index++] = tempVertex3;
	renderQuad[index++] = tempVertex1;
	renderQuad[index++] = tempVertex3;
	renderQuad[index++] = tempVertex4;

	D3D11_BUFFER_DESC bd;
	bd.ByteWidth = sizeof(GRID_TEXTURE_DISPLAY_STRUCT)*6;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory( &InitData, sizeof(D3D11_SUBRESOURCE_DATA) );
	InitData.pSysMem = renderQuad;
	InitData.SysMemPitch = 0;
	InitData.SysMemSlicePitch = 0;
	V_RETURN( pd3dDevice->CreateBuffer( &bd, &InitData,&g_ScreenQuadBuffer  ) ); 	

	delete [] renderQuad;
	renderQuad = NULL;
	return S_OK;
}


//this is the size of the super sample buffer
HRESULT CreateSuperSampleScreenQuadVB(ID3D11Device* pd3dDevice)
{	
	HRESULT hr;

	SAFE_RELEASE(g_SuperSampleScreenQuadBuffer);

	GRID_TEXTURE_DISPLAY_STRUCT *renderQuad(NULL);
	renderQuad = new GRID_TEXTURE_DISPLAY_STRUCT[ 6 ];

	//fill render quad
	GRID_TEXTURE_DISPLAY_STRUCT tempVertex1;
	GRID_TEXTURE_DISPLAY_STRUCT tempVertex2;
	GRID_TEXTURE_DISPLAY_STRUCT tempVertex3;
	GRID_TEXTURE_DISPLAY_STRUCT tempVertex4;

	float left   = -1.0f;
	float right  =  1.0f;
	float top    =  1.0f;
	float bottom = -1.0f;

	tempVertex1.Pos = D3DXVECTOR3( left     , top       , 0.0f      );
	tempVertex1.Tex = D3DXVECTOR3( 0.0f     , 0.0f      , 0.0f      );

	tempVertex2.Pos = D3DXVECTOR3( right    , top       , 0.0f      );
	tempVertex2.Tex = D3DXVECTOR3( 1.0f     , 0.0f      , 0.0f      );

	tempVertex3.Pos = D3DXVECTOR3( right    , bottom    , 0.0f      );
	tempVertex3.Tex = D3DXVECTOR3( 1.0f     , 1.0f      , 0.0f      );

	tempVertex4.Pos = D3DXVECTOR3( left     , bottom    , 0.0f      );
	tempVertex4.Tex = D3DXVECTOR3( 0.0f     , 1.0       , 0.0f      );

	int index = 0;
	renderQuad[index++] = tempVertex1;
	renderQuad[index++] = tempVertex2;
	renderQuad[index++] = tempVertex3;
	renderQuad[index++] = tempVertex1;
	renderQuad[index++] = tempVertex3;
	renderQuad[index++] = tempVertex4;

	D3D11_BUFFER_DESC bd;
	bd.ByteWidth = sizeof(GRID_TEXTURE_DISPLAY_STRUCT)*6;
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory( &InitData, sizeof(D3D11_SUBRESOURCE_DATA) );
	InitData.pSysMem = renderQuad;
	InitData.SysMemPitch = 0;
	InitData.SysMemSlicePitch = 0;
	V_RETURN( pd3dDevice->CreateBuffer( &bd, &InitData,&g_SuperSampleScreenQuadBuffer  ) ); 	

	delete [] renderQuad;
	renderQuad = NULL;
	return S_OK;
}



void RenderScreenQuad(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext, ID3D11Buffer* buffer)
{
	UINT stride[1] = { sizeof(GRID_TEXTURE_DISPLAY_STRUCT) };
	UINT offset[1] = { 0 };

	pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
	pd3dContext->IASetInputLayout( g_screenQuadIL );
	pd3dContext->IASetVertexBuffers( 0, 1,  &buffer, stride, offset );
	pd3dContext->Draw( 6, 0 ); 
}



float3 TrasformFromWorldToLocalCF(int index,float3 oldVector)
{
	float3 vX = float3( g_coordinateFrames[ index*12 + 0] , g_coordinateFrames[ index*12 + 1] , g_coordinateFrames[ index*12 + 2] );
	float3 vY = float3( g_coordinateFrames[ index*12 + 4] , g_coordinateFrames[ index*12 + 5] , g_coordinateFrames[ index*12 + 6] );
	float3 vZ = float3( g_coordinateFrames[ index*12 + 8] , g_coordinateFrames[ index*12 + 9] , g_coordinateFrames[ index*12 + 10] );

	float3 worldX = float3(1,0,0);
	float3 worldY = float3(0,1,0);
	float3 worldZ = float3(0,0,1);


	D3DXMATRIX WorldToV = D3DXMATRIX
		(
		D3DXVec3Dot(&vX,&worldX), D3DXVec3Dot(&vY,&worldX), D3DXVec3Dot(&vZ,&worldX), 0,
		D3DXVec3Dot(&vX,&worldY), D3DXVec3Dot(&vY,&worldY), D3DXVec3Dot(&vZ,&worldY), 0, 
		D3DXVec3Dot(&vX,&worldZ), D3DXVec3Dot(&vY,&worldZ), D3DXVec3Dot(&vZ,&worldZ), 0,
		0,                        0,                        0,                        1
		);

	//transform from old to new coordinate frame
	float3 retVector;
	D3DXVec3TransformCoord(&retVector,&oldVector,&WorldToV);
	return retVector;

}


//--------------------------------------------------------------------------------------
// LoadTextureArray loads a texture array and associated view from a series
// of textures on disk.
//--------------------------------------------------------------------------------------
HRESULT LoadTextureArray( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext, char* sTexturePrefix, int iNumTextures, ID3D11Texture2D** ppTex2D, ID3D11ShaderResourceView** ppSRV,int offset)
{
	HRESULT hr = S_OK;
	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory( &desc, sizeof(D3D11_TEXTURE2D_DESC) );

	WCHAR szTextureName[MAX_PATH];
	WCHAR str[MAX_PATH];
	for(int i=0; i<iNumTextures; i++)
	{
		wsprintf(szTextureName, L"%S%.2d.dds", sTexturePrefix, i+offset); 
		V_RETURN( NVUTFindDXSDKMediaFileCch( str, MAX_PATH, szTextureName ) );

		ID3D11Resource *pRes = NULL;
		D3DX11_IMAGE_LOAD_INFO loadInfo;
		ZeroMemory( &loadInfo, sizeof( D3DX11_IMAGE_LOAD_INFO ) );
		loadInfo.Width = D3DX_FROM_FILE;
		loadInfo.Height  = D3DX_FROM_FILE;
		loadInfo.Depth  = D3DX_FROM_FILE;
		loadInfo.FirstMipLevel = 0;
		loadInfo.MipLevels = 14;
		loadInfo.Usage = D3D11_USAGE_STAGING;
		loadInfo.BindFlags = 0;
		loadInfo.CpuAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
		loadInfo.MiscFlags = 0;
		loadInfo.Format = DXGI_FORMAT_R8G8B8A8_UNORM; 
		loadInfo.Filter = D3DX10_FILTER_TRIANGLE;
		loadInfo.MipFilter = D3DX10_FILTER_TRIANGLE;

		V_RETURN(D3DX11CreateTextureFromFile( pd3dDevice, str, &loadInfo, NULL, &pRes, &hr ));
		if( pRes )
		{
			ID3D11Texture2D* pTemp;
			pRes->QueryInterface( __uuidof( ID3D11Texture2D ), (LPVOID*)&pTemp );
			pTemp->GetDesc( &desc );

			D3D11_MAPPED_SUBRESOURCE mappedTex2D;
			if(loadInfo.Format != desc.Format)   
				return false;

			if(!(*ppTex2D))
			{
				desc.Usage = D3D11_USAGE_DEFAULT;
				desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				desc.CPUAccessFlags = 0;
				desc.ArraySize = iNumTextures;
				V_RETURN(pd3dDevice->CreateTexture2D( &desc, NULL, ppTex2D));
			}

			for(UINT iMip=0; iMip < desc.MipLevels; iMip++)
			{
				pd3dContext->Map(pTemp, iMip, D3D11_MAP_READ, 0, &mappedTex2D );

				pd3dContext->UpdateSubresource( (*ppTex2D), 
					D3D11CalcSubresource( iMip, i, desc.MipLevels ),
					NULL,
					mappedTex2D.pData,
					mappedTex2D.RowPitch,
					0 );

				pd3dContext->Unmap(pTemp, iMip);
			}

			SAFE_RELEASE( pRes );
			SAFE_RELEASE( pTemp );
		}
		else
		{
			return false;
		}
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	ZeroMemory( &SRVDesc, sizeof(SRVDesc) );
	SRVDesc.Format = desc.Format;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	SRVDesc.Texture2DArray.MipLevels = desc.MipLevels;
	SRVDesc.Texture2DArray.ArraySize = iNumTextures;
	V_RETURN(pd3dDevice->CreateShaderResourceView( *ppTex2D, &SRVDesc, ppSRV ));

	return hr;
}

//--------------------------------------------------------------------------------------

//functions for creating and rendering the plane

struct PlaneVertex
{
    D3DXVECTOR3 Pos;
    D3DXVECTOR3 Normal; 
};


HRESULT CreatePlane( ID3D11Device* pd3dDevice, float radius, float height )
{
    HRESULT hr;
    PlaneVertex vertices[] =
    {
        { D3DXVECTOR3( -radius, height,  radius ), D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) },
        { D3DXVECTOR3(  radius, height,  radius ), D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) },
        { D3DXVECTOR3(  radius, height, -radius ), D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) },
        { D3DXVECTOR3( -radius, height, -radius ), D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) },
    };
    D3D11_BUFFER_DESC bd;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( PlaneVertex ) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = vertices;
    hr = pd3dDevice->CreateBuffer( &bd, &InitData, &g_pPlaneVertexBuffer );
    if( FAILED(hr) )
        return hr;

    DWORD indices[] =
    {
        0,1,2,
        0,2,3,
    };

    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof( DWORD ) * 6;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    InitData.pSysMem = indices;
    hr = pd3dDevice->CreateBuffer( &bd, &InitData, &g_pPlaneIndexBuffer );
    if( FAILED(hr) )
        return hr;

    g_pTechniqueRenderPlane = g_Effect->GetTechniqueByName("RenderPlane");

    // Create the input layout
    const D3D11_INPUT_ELEMENT_DESC layoutPlane[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    D3DX11_PASS_DESC PassDesc;
    UINT numElements = sizeof(layoutPlane)/sizeof(layoutPlane[0]);
    g_pTechniqueRenderPlane->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN( pd3dDevice->CreateInputLayout( layoutPlane, numElements, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &g_pPlaneVertexLayout ) );

    return S_OK;
}

void DrawPlane(ID3D11DeviceContext *pd3dContext)
{
	g_HairShadows.SetHairShadowTexture();

    if(g_MeshWithShadows && g_renderShadows)
        g_pTechniqueRenderPlane->GetPassByName("RenderPlaneWithShadows")->Apply(0, pd3dContext);
    else
        g_pTechniqueRenderPlane->GetPassByName("RenderPlaneWithoutShadows")->Apply(0, pd3dContext);

    UINT stride = sizeof( PlaneVertex );
    UINT offset = 0;
    pd3dContext->IASetVertexBuffers( 0, 1, &g_pPlaneVertexBuffer, &stride, &offset );
    pd3dContext->IASetIndexBuffer( g_pPlaneIndexBuffer, DXGI_FORMAT_R32_UINT, 0 );
    pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    pd3dContext->IASetInputLayout( g_pPlaneVertexLayout );
    pd3dContext->DrawIndexed( 6, 0, 0 );

	g_HairShadows.UnSetHairShadowTexture();

    if(g_MeshWithShadows && g_renderShadows)
        g_pTechniqueRenderPlane->GetPassByName("RenderPlaneWithShadows")->Apply(0, pd3dContext);
    else
        g_pTechniqueRenderPlane->GetPassByName("RenderPlaneWithoutShadows")->Apply(0, pd3dContext);
}

