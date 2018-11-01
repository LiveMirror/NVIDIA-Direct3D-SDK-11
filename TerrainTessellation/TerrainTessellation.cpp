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
#include "DXUTgui.h"
#include "DXUTmisc.h"
#include "DXUTShapes.h"
#include "DXUTCamera.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"
#include "d3dx11effect.h"
#include "Stars.h"
#include "TileRing.h"
#include "skybox11.h"

#pragma warning(disable:4995) // avoid warning for "...was marked as #pragma deprecated"
#pragma warning(disable:4996) // unsafe stdio nonsense

#undef min
#undef max
#include <stdio.h>
#include <utility>
#include <fstream>
#include <sstream>
#include <strsafe.h>

CFirstPersonCamera g_MasterCamera, g_DebugCamera;
CFirstPersonCamera* g_pControlledCamera = &g_MasterCamera;
bool g_syncCameras = true, g_DebugCameraHasBeenUsed = false;

CDXUTDialogResourceManager g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg g_SettingsDlg; // Device settings dialog
CDXUTDialog g_HUD; // dialog for standard controls
CDXUTDialog g_SampleUI; // dialog for sample specific controls
CDXUTTextHelper* g_pTextHelper = NULL;
bool g_ShowHelp = false;

CSkybox11 g_Skybox;
bool g_CheckForCracks = false;		// Set to true for more obvious cracks when debugging.

const int N_QUERIES = 5;
ID3D11Device* g_pD3D11Device = NULL;
ID3D11Query* g_PipelineQueries[N_QUERIES];
ID3D11Query* g_FreePipelineQueries[N_QUERIES];
UINT64 g_PrimitivesRendered = 0;
	
ID3DX11Effect* g_pTerrainEffect = NULL;
ID3DX11Effect* g_pDeformEffect = NULL;
ID3D11Buffer* g_TileTriStripIB = NULL;
ID3D11Buffer* g_TileQuadListIB = NULL;

// Shader variables and matrices
ID3DX11EffectVectorVariable* g_pEyePosVar = NULL;
ID3DX11EffectVectorVariable* g_pViewDirVar = NULL;
ID3DX11EffectVectorVariable* g_pFractalOctavesTVar = NULL;
ID3DX11EffectVectorVariable* g_pFractalOctavesDVar = NULL;
ID3DX11EffectVectorVariable* g_pUVOffsetTVar = NULL;
ID3DX11EffectVectorVariable* g_pUVOffsetDVar = NULL;
ID3DX11EffectScalarVariable* g_pTriSizeVar = NULL;
ID3DX11EffectScalarVariable* g_pTileSizeVar = NULL;
ID3DX11EffectScalarVariable* g_DebugShowPatchesVar = NULL;
ID3DX11EffectScalarVariable* g_DebugShowTilesVar = NULL;
ID3DX11EffectVectorVariable* g_pViewportVar = NULL;
ID3DX11EffectScalarVariable* g_WireAlphaVar = NULL;
ID3DX11EffectScalarVariable* g_WireWidthVar = NULL;
ID3DX11EffectScalarVariable* g_DetailNoiseVar = NULL;
ID3DX11EffectVectorVariable* g_DetailUVVar = NULL;
ID3DX11EffectScalarVariable* g_SampleSpacingVar = NULL;

ID3DX11EffectShaderResourceVariable* g_HeightMapVar = NULL;
ID3DX11EffectShaderResourceVariable* g_GradientMapVar = NULL;
ID3DX11EffectShaderResourceVariable* g_InputTexVar = NULL;
D3D11_VIEWPORT g_BackBufferVP;

ID3DX11EffectTechnique* g_pTesselationTechnique = NULL;
ID3DX11EffectTechnique* g_pInitializationTechnique = NULL;
ID3DX11EffectTechnique* g_pGradientTechnique = NULL;

ID3D11ShaderResourceView* g_pHeightMapSRV = NULL;
ID3D11RenderTargetView*   g_pHeightMapRTV = NULL;
ID3D11ShaderResourceView* g_pGradientMapSRV = NULL;
ID3D11RenderTargetView*   g_pGradientMapRTV = NULL;
float invNonlinearCameraSpeed(float);

const int MAX_OCTAVES = 15;
const int g_DefaultRidgeOctaves = 3;			// This many ridge octaves is good for the moon - rather jagged.
const int g_DefaultfBmOctaves = 3;
const int g_DefaultTexTwistOctaves = 1;
const int g_DefaultDetailNoiseScale = 20;
int g_RidgeOctaves     = g_DefaultRidgeOctaves;
int g_fBmOctaves       = g_DefaultfBmOctaves;
int g_TexTwistOctaves  = g_DefaultTexTwistOctaves;
int g_DetailNoiseScale = g_DefaultDetailNoiseScale;
float g_CameraSpeed = invNonlinearCameraSpeed(100);
int g_PatchCount = 0;
bool g_HwTessellation = true;
int g_tessellatedTriWidth = 6;	// pixels on a triangle edge

const float CLIP_NEAR = 1, CLIP_FAR = 20000;
D3DXVECTOR2 g_ScreenSize(1920,1200);

const int COARSE_HEIGHT_MAP_SIZE = 1024;
const float WORLD_SCALE = 400;
const int VTX_PER_TILE_EDGE = 9;				// overlap => -2
const int TRI_STRIP_INDEX_COUNT = (VTX_PER_TILE_EDGE-1) * (2 * VTX_PER_TILE_EDGE + 2);
const int QUAD_LIST_INDEX_COUNT = (VTX_PER_TILE_EDGE-1) * (VTX_PER_TILE_EDGE-1) * 4;
const int MAX_RINGS = 10;
int g_nRings = 0;

TileRing* g_pTileRings[MAX_RINGS];
float SNAP_GRID_SIZE = 0;

bool g_ResetTerrain = false;

const WCHAR* g_pHelpText = 
	L"\n"
	L"Move: WASD + mouse\n"
	L"0-9: load saved viewpoints.\n"
	L"Alt+0-9: save a viewpoint.\n"
	L"'h' hide the entire GUI (for screenshots)\n"
	L"\n"
	L"There are two independent viewpoints for center of projection and LOD.\n"
	L"They can be manipulated independently for debug viewing.\n"
	L"Saved viewpoint 0 is loaded at start-up.\n"
;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_STATIC                  -1
#define IDC_TOGGLEFULLSCREEN        1
#define IDC_TOGGLEREF               2
#define IDC_CHANGEDEVICE            3
#define IDC_WIREFRAME               4
#define IDC_RIDGE_OCTAVES           6
#define IDC_RIDGE_STATIC            7
#define IDC_CAMERASPEED             8
#define IDC_CAMERASPEED_STATIC      9
#define IDC_HW_TESSELLATION         10
#define IDC_FBM_OCTAVES             12
#define IDC_FBM_STATIC              13
#define IDC_SYNC_CAMERAS            16
#define IDC_CONTROL_BOTH            17
#define IDC_CONTROL_EYE             18
#define IDC_CONTROL_LOD             19
#define IDC_CAMERA_STATIC           20
#define IDC_TRI_SIZE_STATIC         21
#define IDC_TRI_SIZE                22
#define IDC_DEBUG_SHOW_PATCHES      24
#define IDC_WIREFRAME_ALPHA         25
#define IDC_TWIST_OCTAVES           26
#define IDC_TWIST_STATIC            27
#define IDC_DEBUG_SHOW_TILES        28
#define IDC_DETAIL_OCTAVES          29
#define IDC_DETAIL_STATIC           30

HRESULT loadTextureFromFile(LPCWSTR file, LPCSTR shaderTextureName, ID3D11Device* pd3dDevice, ID3DX11Effect* pEffect)
{

    HRESULT hr;

    D3DX11_IMAGE_LOAD_INFO texLoadInfo;
    texLoadInfo.MipLevels = 20;
    texLoadInfo.MipFilter = D3DX11_FILTER_TRIANGLE;
    texLoadInfo.Filter = D3DX11_FILTER_TRIANGLE;
    // texLoadInfo.Format = DXGI_FORMAT_R16G16B16A16_UNORM;
    ID3D11Resource *pRes = NULL;

    WCHAR str[MAX_PATH];
    V_RETURN(DXUTFindDXSDKMediaFileCch(str, MAX_PATH, file));

    V_RETURN(D3DX11CreateTextureFromFile(pd3dDevice, str, &texLoadInfo, NULL, &pRes, NULL));
    if (pRes)
    {
        ID3D11Texture2D* texture;

        pRes->QueryInterface(__uuidof(ID3D11Texture2D), (LPVOID*)&texture);
        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);
        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
        ZeroMemory(&SRVDesc, sizeof(SRVDesc));
        SRVDesc.Format = desc.Format;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Texture2D.MostDetailedMip = 0;
        SRVDesc.Texture2D.MipLevels = desc.MipLevels;

        ID3D11ShaderResourceView* textureRview;
        V_RETURN (pd3dDevice->CreateShaderResourceView(texture, &SRVDesc, &textureRview));
        ID3DX11EffectShaderResourceVariable* textureRVar = pEffect->GetVariableByName(shaderTextureName)->AsShaderResource();
        textureRVar->SetResource(textureRview);

        SAFE_RELEASE(texture);
        SAFE_RELEASE(textureRview);
     }

    SAFE_RELEASE(pRes);
    return S_OK;
}

HRESULT LoadSkyboxFromFile(LPCWSTR file, ID3D11Device* pd3dDevice)
{

    HRESULT hr;

    WCHAR str[MAX_PATH];
    V_RETURN(DXUTFindDXSDKMediaFileCch(str, MAX_PATH, file));

    D3DX11_IMAGE_INFO SrcInfo;
    hr = D3DX11GetImageInfoFromFile(str, NULL, &SrcInfo, NULL);

    D3DX11_IMAGE_LOAD_INFO texLoadInfo;
    texLoadInfo.Width          = SrcInfo.Width;
    texLoadInfo.Height         = SrcInfo.Height;
    texLoadInfo.Depth          = SrcInfo.Depth;
    texLoadInfo.FirstMipLevel  = 0;
    texLoadInfo.MipLevels      = SrcInfo.MipLevels;
    texLoadInfo.Usage          = D3D11_USAGE_DEFAULT;
    texLoadInfo.BindFlags      = D3D11_BIND_SHADER_RESOURCE;
    texLoadInfo.CpuAccessFlags = 0;
    texLoadInfo.MiscFlags      = SrcInfo.MiscFlags;
    texLoadInfo.Format         = SrcInfo.Format;
    texLoadInfo.Filter         = D3DX11_FILTER_TRIANGLE;
    texLoadInfo.MipFilter      = D3DX11_FILTER_TRIANGLE;
    texLoadInfo.pSrcInfo       = &SrcInfo;
    ID3D11Resource *pRes = NULL;

    V_RETURN(D3DX11CreateTextureFromFile(pd3dDevice, str, &texLoadInfo, NULL, &pRes, NULL));
    if (pRes)
    {
        ID3D11Texture2D* texture;

        pRes->QueryInterface(__uuidof(ID3D11Texture2D), (LPVOID*)&texture);
        D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
        ZeroMemory(&SRVDesc, sizeof(SRVDesc));
        SRVDesc.Format = texLoadInfo.Format;
        SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        SRVDesc.Texture2D.MostDetailedMip = 0;
        SRVDesc.Texture2D.MipLevels = texLoadInfo.MipLevels;

        ID3D11ShaderResourceView* textureRview;
        V_RETURN (pd3dDevice->CreateShaderResourceView(texture, &SRVDesc, &textureRview));
        
		g_Skybox.OnD3D11CreateDevice(pd3dDevice, 1, texture, textureRview);
        
		// Sky box class holds references.
        //SAFE_RELEASE(texture);
        //SAFE_RELEASE(textureRview);
     }

    SAFE_RELEASE(pRes);
    return S_OK;
}

float nonlinearCameraSpeed(float f)
{
	// Make the slider control logarithmic.
	return pow(10.0f, f/100.0f);	// f [1,400]
}

float invNonlinearCameraSpeed(float f)
{
	// Make the slider control logarithmic.
	return 100.0f * (logf(f) / logf(10.0f));
}

void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext)
{
    switch (nControlID)
    {
    case IDC_TOGGLEFULLSCREEN: DXUTToggleFullScreen(); break;
    case IDC_TOGGLEREF:        DXUTToggleREF(); break;
    case IDC_CHANGEDEVICE:     g_SettingsDlg.SetActive(!g_SettingsDlg.IsActive()); break;
	case IDC_SYNC_CAMERAS:	   g_DebugCamera = g_MasterCamera; break;

	case IDC_CONTROL_BOTH:
		{
			CDXUTRadioButton* pRadioButton = (CDXUTRadioButton*) pControl;
			if (pRadioButton->GetChecked())
			{
				g_pControlledCamera = &g_MasterCamera;
				g_syncCameras = true;
			}
			break;
		}
	case IDC_CONTROL_EYE:
		{
			CDXUTRadioButton* pRadioButton = (CDXUTRadioButton*) pControl;
			if (pRadioButton->GetChecked())
			{
				g_pControlledCamera = &g_DebugCamera;
				g_syncCameras = false;
				g_DebugCameraHasBeenUsed = true;
			}
			break;
		}
	case IDC_CONTROL_LOD:
		{
			CDXUTRadioButton* pRadioButton = (CDXUTRadioButton*) pControl;
			if (pRadioButton->GetChecked())
			{
				g_pControlledCamera = &g_MasterCamera;
				g_syncCameras = false;
			}
			break;
		}
    
    case IDC_HW_TESSELLATION:
        {
            CDXUTCheckBox* pCheckBox = (CDXUTCheckBox*) pControl;
            g_HwTessellation = pCheckBox->GetChecked();
        }
        break;
        
	case IDC_WIREFRAME:
        {
            CDXUTCheckBox* pCheckBox = (CDXUTCheckBox*) pControl;
            const bool isOn = pCheckBox->GetChecked();
			CDXUTSlider* pWireAlpha = g_SampleUI.GetSlider(IDC_WIREFRAME_ALPHA);
			assert(pWireAlpha != NULL);
			pWireAlpha->SetEnabled(isOn);
        }
        break;
        
	case IDC_RIDGE_OCTAVES:
        {
            CDXUTSlider* pSlider = (CDXUTSlider*) pControl;
            g_RidgeOctaves = pSlider->GetValue();

            WCHAR sz[200];
            StringCchPrintf(sz, 200, L"Ridge octaves: %d", g_RidgeOctaves);
            g_SampleUI.GetStatic(IDC_RIDGE_STATIC)->SetText(sz);
            
            g_ResetTerrain = true;
        }
        break;
    
    case IDC_FBM_OCTAVES:
        {
            CDXUTSlider* pSlider = (CDXUTSlider*) pControl;
            g_fBmOctaves = pSlider->GetValue();

            WCHAR sz[200];
            StringCchPrintf(sz, 200, L"fBm octaves: %d", g_fBmOctaves);
            g_SampleUI.GetStatic(IDC_FBM_STATIC)->SetText(sz);
            
            g_ResetTerrain = true;
        }
        break;
    
    case IDC_TWIST_OCTAVES:
        {
            CDXUTSlider* pSlider = (CDXUTSlider*) pControl;
            g_TexTwistOctaves = pSlider->GetValue();

            WCHAR sz[200];
            StringCchPrintf(sz, 200, L"Twist octaves: %d", g_TexTwistOctaves);
            g_SampleUI.GetStatic(IDC_TWIST_STATIC)->SetText(sz);
            
            g_ResetTerrain = true;
        }
        break;
    
    case IDC_DETAIL_OCTAVES:
        {
            CDXUTSlider* pSlider = (CDXUTSlider*) pControl;
            g_DetailNoiseScale = pSlider->GetValue();

            WCHAR sz[200];
            StringCchPrintf(sz, 200, L"Detail noise scale: %.2f", 0.01f * (float)g_DetailNoiseScale);
            g_SampleUI.GetStatic(IDC_DETAIL_STATIC)->SetText(sz);
        }
        break;
    
    case IDC_TRI_SIZE:
        {
            CDXUTSlider* pSlider = (CDXUTSlider*) pControl;
            g_tessellatedTriWidth = pSlider->GetValue();

            WCHAR sz[200];
            StringCchPrintf(sz, 200, L"Target tri width: %d", g_tessellatedTriWidth);
            g_SampleUI.GetStatic(IDC_TRI_SIZE_STATIC)->SetText(sz);
        }
        break;
    
    case IDC_CAMERASPEED:
        {
            CDXUTSlider* pSlider = (CDXUTSlider*) pControl;
            g_CameraSpeed = (float)pSlider->GetValue();

            WCHAR sz[200];
            StringCchPrintf(sz, 200, L"Movement speed: %.2f", nonlinearCameraSpeed(g_CameraSpeed));
            g_SampleUI.GetStatic(IDC_CAMERASPEED_STATIC)->SetText(sz);
        }
        break;
    }
}

static std::wstring CameraFileName(int saveNo)
{
	std::wostringstream sstr;
	sstr << "TerrainTessellation\\Camera" << saveNo << ".txt" << std::ends;
	
	// We are possibly creating a file that doesn't exist yet.  So it's not an error if it's not found.
    WCHAR mediaPath[MAX_PATH];
	DXUTFindDXSDKMediaFileCch(mediaPath, MAX_PATH, sstr.str().c_str());
	return mediaPath;
}

static void SaveCamera(int saveNo, const CBaseCamera& cam)
{
	std::ofstream ostr(CameraFileName(saveNo).c_str());

	const D3DXVECTOR3& eye = *cam.GetEyePt();
	const D3DXVECTOR3& at  = *cam.GetLookAtPt();
	ostr << eye.x << " " << eye.y << " " << eye.z << " " << std::endl;
	ostr << at.x  << " " << at.y  << " " << at.z  << " " << std::endl;
}

static void LoadCamera(int saveNo, CBaseCamera& cam)
{
	std::ifstream istr(CameraFileName(saveNo).c_str());

	if (istr.good())
	{
		D3DXVECTOR3 eye;
		D3DXVECTOR3 at;
		istr >> eye.x >> eye.y >> eye.z;
		istr >> at.x  >> at.y  >> at.z;

		cam.SetViewParams(&eye, &at);
	}
}

static void InitCameras()
{
	D3DXVECTOR3 vEye(786.1f,  -86.5f,  62.2f);
	D3DXVECTOR3  vAt(786.3f, -130.0f, 244.1f);

	// Default to the above hardcoded vector only in case loading from file fails.
    g_MasterCamera.SetViewParams(&vEye, &vAt);

	// But load the zeroth file if it exists.
	LoadCamera(0, g_MasterCamera);

    g_MasterCamera.SetEnablePositionMovement(true);

	// The two cameras start out the same.
	g_DebugCamera = g_MasterCamera;
}

void InitApp()
{
	InitCameras();
	ReadStars();

    g_SettingsDlg.Init(&g_DialogResourceManager);
    g_HUD.Init(&g_DialogResourceManager);
    g_SampleUI.Init(&g_DialogResourceManager);
    g_SampleUI.SetCallback(OnGUIEvent);

    g_HUD.SetCallback(OnGUIEvent); int iY = 10;
    g_HUD.AddButton(IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 15, iY,       145, 22);
    g_HUD.AddButton(IDC_TOGGLEREF,        L"Toggle REF (F3)",    15, iY += 24, 145, 22, VK_F3);
    g_HUD.AddButton(IDC_CHANGEDEVICE,     L"Change device (F2)", 15, iY += 24, 145, 22, VK_F2);
    
    iY += 24;

	const int group = 0;
	g_SampleUI.AddStatic(IDC_CAMERA_STATIC, L"Camera controls:",      0, iY += 24, 200, 22);
    g_SampleUI.AddRadioButton(IDC_CONTROL_BOTH, group, L"Eye & LOD", 25, iY += 24, 100, 16, true);
    g_SampleUI.AddRadioButton(IDC_CONTROL_EYE,  group, L"Eye only",  25, iY += 24, 100, 16, false);
    g_SampleUI.AddRadioButton(IDC_CONTROL_LOD,  group, L"LOD only",  25, iY += 24, 100, 16, false);
	g_SampleUI.AddButton(IDC_SYNC_CAMERAS, L"Sync LOD -> Eye",       25, iY += 24, 145, 22);
    
    WCHAR sz[200];
    StringCchPrintf(sz, 200, L"Speed: %.2f", nonlinearCameraSpeed(g_CameraSpeed)); 
    g_SampleUI.AddStatic(IDC_CAMERASPEED_STATIC, sz, 25, iY += 30, 200, 22);
    g_SampleUI.AddSlider(IDC_CAMERASPEED,            25, iY += 18, 145, 22, 1, 400, (int)g_CameraSpeed);

    iY += 24;

	// These two on a line together.
	const bool isWireOn = false;
	const int initAlpha = 70;
	CDXUTSlider* pWireAlpha = NULL;
    g_SampleUI.AddCheckBox(IDC_WIREFRAME, L"Wireframe",   0, iY, 70, 22, isWireOn);
    g_SampleUI.AddSlider(IDC_WIREFRAME_ALPHA,           110, iY, 70, 22, 0, 400, initAlpha, false, &pWireAlpha);
	pWireAlpha->SetEnabled(isWireOn);
	iY += 24;

    g_SampleUI.AddCheckBox(IDC_HW_TESSELLATION,    L"Hardware Tessellation",  0, iY += 24, 200, 22, g_HwTessellation);
	g_SampleUI.AddCheckBox(IDC_DEBUG_SHOW_PATCHES, L"Debug Show Patches",     0, iY += 24, 200, 22, false);
	g_SampleUI.AddCheckBox(IDC_DEBUG_SHOW_TILES,   L"Debug Show Tiles",       0, iY += 24, 200, 22, false);
    
    StringCchPrintf(sz, 200, L"Ridge octaves: %d", g_RidgeOctaves); 
    g_SampleUI.AddStatic(IDC_RIDGE_STATIC,    sz, 0, iY += 30, 200, 22);
    g_SampleUI.AddSlider(IDC_RIDGE_OCTAVES,       0, iY += 18, 191, 22, 0, MAX_OCTAVES, g_RidgeOctaves);
    
    StringCchPrintf(sz, 200, L"fBm octaves: %d", g_fBmOctaves); 
    g_SampleUI.AddStatic(IDC_FBM_STATIC,      sz, 0, iY += 30, 200, 22);
    g_SampleUI.AddSlider(IDC_FBM_OCTAVES,         0, iY += 18, 191, 22, 0, MAX_OCTAVES, g_fBmOctaves);
    
    StringCchPrintf(sz, 200, L"Twist octaves: %d", g_TexTwistOctaves); 
    g_SampleUI.AddStatic(IDC_TWIST_STATIC,   sz, 0, iY += 30, 200, 22);
    g_SampleUI.AddSlider(IDC_TWIST_OCTAVES,      0, iY += 18, 191, 22, 0, MAX_OCTAVES, g_TexTwistOctaves);
    
    StringCchPrintf(sz, 200, L"Detail noise scale: %.2f", 0.01f * (float)g_DetailNoiseScale);
	g_SampleUI.AddStatic(IDC_DETAIL_STATIC,   sz, 0, iY += 30, 200, 22);
    g_SampleUI.AddSlider(IDC_DETAIL_OCTAVES,      0, iY += 18, 191, 22, 0, 200, g_DetailNoiseScale);
    
    StringCchPrintf(sz, 200, L"Target tri width: %d", g_tessellatedTriWidth); 
    g_SampleUI.AddStatic(IDC_TRI_SIZE_STATIC, sz, 0, iY += 30, 200, 22);
    g_SampleUI.AddSlider(IDC_TRI_SIZE,            0, iY += 18, 191, 22, 1, 50, g_tessellatedTriWidth);
}

void RenderText()
{
	if (g_pTextHelper)
	{
		g_pTextHelper->Begin();
		if (g_HUD.GetVisible())
		{
			g_pTextHelper->SetInsertionPos(5, 5);
			g_pTextHelper->SetForegroundColor(D3DXCOLOR(1.0f, 1.0f, 0.0f, 1.0f));
			g_pTextHelper->DrawTextLine(DXUTGetFrameStats(true));
			g_pTextHelper->DrawTextLine(DXUTGetDeviceStats());
			g_pTextHelper->DrawFormattedTextLine(L"Primitives rasterized: %d", (int) g_PrimitivesRendered);
			const D3DXVECTOR3& eye = *g_pControlledCamera->GetEyePt();
			g_pTextHelper->DrawFormattedTextLine(L"Camera at (%.1f, %.1f, %.1f)", eye.x, eye.y, eye.z);
			g_pTextHelper->DrawTextLine(L"Press F1 for help");

			if (g_ShowHelp)
				g_pTextHelper->DrawTextLine(g_pHelpText);
			{
			}
		}
		g_pTextHelper->End();
	}
}

//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}

//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D11 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings, void* pUserContext)
{
	pDeviceSettings->d3d11.sd.SampleDesc.Count = 8;		// AA
	pDeviceSettings->d3d11.sd.SampleDesc.Quality = 16;	// CSAA

	pDeviceSettings->d3d11.SyncInterval = 0;	// turn off vsync
#ifdef _DEBUG
	pDeviceSettings->d3d11.CreateFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    return true;
}

static void CreateTileTriangleIB(ID3D11Device* pd3dDevice)
{
    D3D11_SUBRESOURCE_DATA initData;
    ZeroMemory(&initData, sizeof(D3D11_SUBRESOURCE_DATA));
    
    int index = 0;
    unsigned long* pIndices = new unsigned long[TRI_STRIP_INDEX_COUNT];

	// Using the same patch-corner vertices as for h/w tessellaiton, tessellate them into 2 tris each.
	// Create the usual zig-zag pattern of stripped triangles for a regular gridded terrain tile.
    for (int y = 0; y < VTX_PER_TILE_EDGE-1; ++y)
    {
		const int rowStart = y * VTX_PER_TILE_EDGE;

        for (int x = 0; x < VTX_PER_TILE_EDGE; ++x)
        {
            pIndices[index++] = rowStart + x;
            pIndices[index++] = rowStart + x + VTX_PER_TILE_EDGE;
        }

		// Repeat the last one on this row and the first on the next to join strips with degenerates.
        pIndices[index] = pIndices[index-1];
		++index;
        pIndices[index++] = rowStart + VTX_PER_TILE_EDGE;
    }
	assert(index == TRI_STRIP_INDEX_COUNT);
    
    initData.pSysMem = pIndices;
    D3D11_BUFFER_DESC iBufferDesc =
    {
        sizeof(unsigned long) * TRI_STRIP_INDEX_COUNT,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_INDEX_BUFFER,
        0,
        0
    };

    HRESULT hr;
    V(pd3dDevice->CreateBuffer(&iBufferDesc, &initData, &g_TileTriStripIB));
    delete[] pIndices;
}

static void CreateTileQuadListIB(ID3D11Device* pd3dDevice)
{
    D3D11_SUBRESOURCE_DATA initData;
    ZeroMemory(&initData, sizeof(D3D11_SUBRESOURCE_DATA));
    
    int index = 0;
    unsigned long* pIndices = new unsigned long[QUAD_LIST_INDEX_COUNT];

	// The IB describes one tile of NxN patches.
	// Four vertices per quad, with VTX_PER_TILE_EDGE-1 quads per tile edge.
    for (int y = 0; y < VTX_PER_TILE_EDGE-1; ++y)
    {
		const int rowStart = y * VTX_PER_TILE_EDGE;

        for (int x = 0; x < VTX_PER_TILE_EDGE-1; ++x)
        {
            pIndices[index++] = rowStart + x;
            pIndices[index++] = rowStart + x + VTX_PER_TILE_EDGE;
            pIndices[index++] = rowStart + x + VTX_PER_TILE_EDGE + 1;
            pIndices[index++] = rowStart + x + 1;
        }
    }
	assert(index == QUAD_LIST_INDEX_COUNT);
    
    initData.pSysMem = pIndices;
    D3D11_BUFFER_DESC iBufferDesc =
    {
        sizeof(unsigned long) * QUAD_LIST_INDEX_COUNT,
        D3D11_USAGE_DEFAULT,
        D3D11_BIND_INDEX_BUFFER,
        0,
        0
    };

    HRESULT hr;
    V(pd3dDevice->CreateBuffer(&iBufferDesc, &initData, &g_TileQuadListIB));
    delete[] pIndices;
}

static void CreateQueries(ID3D11Device* pd3dDevice)
{
	D3D11_QUERY_DESC desc;
	desc.MiscFlags = 0;
	desc.Query = D3D11_QUERY_PIPELINE_STATISTICS;

	for (int i=0; i!=N_QUERIES; ++i)
	{
		g_PipelineQueries[i] = NULL;
		pd3dDevice->CreateQuery(&desc, &g_PipelineQueries[i]);
		g_FreePipelineQueries[i] = g_PipelineQueries[i];			// All start on free list.
	}
}

static HRESULT CreateTerrainEffect(ID3D11Device* pd3dDevice)
{
    HRESULT hr;

    WCHAR path[MAX_PATH];
    ID3DXBuffer* pShader = NULL;
    if (FAILED (hr = DXUTFindDXSDKMediaFileCch(path, MAX_PATH, L"TerrainTessellation.fx"))) {
        MessageBox (NULL, L"TerrainTessellation.fx cannot be found.", L"Error", MB_OK);
        return hr;
    }

	const DWORD flags = 0;
    ID3DXBuffer* pErrors = NULL;
    if (D3DXCompileShaderFromFile(path, NULL, NULL, NULL, "fx_5_0", flags, &pShader, &pErrors, NULL) != S_OK)
    {
		const char* pTxt = (char *)pErrors->GetBufferPointer();
        MessageBoxA(NULL, pTxt, "Compilation errors", MB_OK);
        return hr;
    }

    if (D3DX11CreateEffectFromMemory(pShader->GetBufferPointer(), pShader->GetBufferSize(), 0, pd3dDevice, &g_pTerrainEffect) != S_OK)
    {
        MessageBoxA(NULL, "Failed to create terrain effect", "Effect load error", MB_OK);
        return hr;
    }

    // Obtain techniques
    g_pTesselationTechnique = g_pTerrainEffect->GetTechniqueByName("TesselationTechnique");
    
    // Obtain miscellaneous variables
	g_HeightMapVar = g_pTerrainEffect->GetVariableByName("g_CoarseHeightMap")->AsShaderResource();
	g_GradientMapVar = g_pTerrainEffect->GetVariableByName("g_CoarseGradientMap")->AsShaderResource();

    g_pEyePosVar  = g_pTerrainEffect->GetVariableByName("g_EyePos")->AsVector();
    g_pViewDirVar = g_pTerrainEffect->GetVariableByName("g_ViewDir")->AsVector();
	g_pFractalOctavesTVar = g_pTerrainEffect->GetVariableByName("g_FractalOctaves")->AsVector();
	g_pUVOffsetTVar = g_pTerrainEffect->GetVariableByName("g_TextureWorldOffset")->AsVector();
    g_pViewportVar = g_pTerrainEffect->GetVariableByName( "Viewport" )->AsVector();
	g_pTriSizeVar = g_pTerrainEffect->GetVariableByName("g_tessellatedTriWidth")->AsScalar();
	g_pTileSizeVar = g_pTerrainEffect->GetVariableByName("g_tileSize")->AsScalar();
	g_DebugShowPatchesVar = g_pTerrainEffect->GetVariableByName("g_DebugShowPatches")->AsScalar();
	g_DebugShowTilesVar = g_pTerrainEffect->GetVariableByName("g_DebugShowTiles")->AsScalar();
	g_WireAlphaVar = g_pTerrainEffect->GetVariableByName("g_WireAlpha")->AsScalar();
	g_WireWidthVar = g_pTerrainEffect->GetVariableByName("g_WireWidth")->AsScalar();
	g_DetailNoiseVar = g_pTerrainEffect->GetVariableByName("g_DetailNoiseScale")->AsScalar();
	g_DetailUVVar = g_pTerrainEffect->GetVariableByName("g_DetailUVScale")->AsVector();
	g_SampleSpacingVar = g_pTerrainEffect->GetVariableByName("g_CoarseSampleSpacing")->AsScalar();
    
	V_RETURN(loadTextureFromFile(L"TerrainTessellation/LunarSurface1.dds",     "g_TerrainColourTexture1", pd3dDevice, g_pTerrainEffect));
	V_RETURN(loadTextureFromFile(L"TerrainTessellation/LunarMicroDetail1.dds", "g_TerrainColourTexture2", pd3dDevice, g_pTerrainEffect));
	V_RETURN(loadTextureFromFile(L"GaussianNoise256.jpg", "g_NoiseTexture",   pd3dDevice, g_pTerrainEffect));
	V_RETURN(loadTextureFromFile(L"fBm5Octaves.dds",      "g_DetailNoiseTexture",     pd3dDevice, g_pTerrainEffect));
	V_RETURN(loadTextureFromFile(L"fBm5OctavesGrad.dds",  "g_DetailNoiseGradTexture", pd3dDevice, g_pTerrainEffect));
    
    return S_OK;
}

static HRESULT CreateAmplifiedHeights(ID3D11Device* pd3dDevice)
{
    D3D11_TEXTURE2D_DESC desc;
	desc.Width              = COARSE_HEIGHT_MAP_SIZE;
    desc.Height             = COARSE_HEIGHT_MAP_SIZE;
    desc.MipLevels          = 1;
    desc.ArraySize          = 1;
	desc.Format             = DXGI_FORMAT_R32_FLOAT;
    desc.SampleDesc.Count   = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage              = D3D11_USAGE_DEFAULT;
    desc.BindFlags          = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags     = 0;
    desc.MiscFlags          = 0;

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory(&SRVDesc, sizeof(SRVDesc));
	SRVDesc.Format                    = desc.Format;
	SRVDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels       = 1;
	SRVDesc.Texture2D.MostDetailedMip = 0;

	D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
	RTVDesc.Format             = desc.Format;
	RTVDesc.ViewDimension      = D3D11_RTV_DIMENSION_TEXTURE2D;
	RTVDesc.Texture2D.MipSlice = 0;

	// No initial data here - it's initialized by deformation.
	ID3D11Texture2D* pTex = NULL;
    HRESULT hr = pd3dDevice->CreateTexture2D(&desc, NULL, &pTex);

	if (SUCCEEDED(hr))
	{
		HRESULT hr = S_OK;
        V_RETURN(pd3dDevice->CreateShaderResourceView(pTex, &SRVDesc, &g_pHeightMapSRV));
		V_RETURN(pd3dDevice->CreateRenderTargetView(pTex,   &RTVDesc, &g_pHeightMapRTV));

        SAFE_RELEASE(pTex);
	}

	desc.Format = DXGI_FORMAT_R16G16_FLOAT;

	// No initial data here - it's initialized by deformation.
    hr = pd3dDevice->CreateTexture2D(&desc, NULL, &pTex);

	if (SUCCEEDED(hr))
	{
		HRESULT hr = S_OK;
		SRVDesc.Format = RTVDesc.Format = desc.Format;
        V_RETURN(pd3dDevice->CreateShaderResourceView(pTex, &SRVDesc, &g_pGradientMapSRV));
		V_RETURN(pd3dDevice->CreateRenderTargetView(pTex,   &RTVDesc, &g_pGradientMapRTV));

        SAFE_RELEASE(pTex);
	}

	return hr;
}

static HRESULT CreateDeformEffect(ID3D11Device* pd3dDevice)
{
    HRESULT hr;

    WCHAR path[MAX_PATH];
    ID3DXBuffer* pShader = NULL;
    if (FAILED (hr = DXUTFindDXSDKMediaFileCch(path, MAX_PATH, L"Deformation.fx"))) {
        MessageBox (NULL, L"Deformation.fx cannot be found.", L"Error", MB_OK);
        return hr;
    }

    ID3DXBuffer* pErrors = NULL;
    if (D3DXCompileShaderFromFile(path, NULL, NULL, NULL, "fx_5_0", 0, &pShader, &pErrors, NULL) != S_OK)
    {
        MessageBoxA(NULL, (char *)pErrors->GetBufferPointer(), "Compilation errors", MB_OK);
        return hr;
    }

    if (D3DX11CreateEffectFromMemory(pShader->GetBufferPointer(), pShader->GetBufferSize(), 0, pd3dDevice, &g_pDeformEffect) != S_OK)
    {
        MessageBoxA(NULL, "Failed to create deformation effect", "Effect load error", MB_OK);
        return hr;
    }

    // Obtain techniques
	g_pInitializationTechnique = g_pDeformEffect->GetTechniqueByName("InitializationTechnique");
	g_pGradientTechnique       = g_pDeformEffect->GetTechniqueByName("GradientTechnique");
    
    // Obtain miscellaneous variables
	g_pFractalOctavesDVar = g_pDeformEffect->GetVariableByName("g_FractalOctaves")->AsVector();
	g_pUVOffsetDVar = g_pDeformEffect->GetVariableByName("g_TextureWorldOffset")->AsVector();
	g_InputTexVar = g_pDeformEffect->GetVariableByName("g_InputTexture")->AsShaderResource();

	V_RETURN(loadTextureFromFile(L"GaussianNoise256.jpg", "g_NoiseTexture", pd3dDevice, g_pDeformEffect));
    
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
    HRESULT hr;

	g_pD3D11Device = pd3dDevice;
	CreateAmplifiedHeights(pd3dDevice);

    ID3D11DeviceContext* pContext = DXUTGetD3D11DeviceContext();
	V_RETURN(g_DialogResourceManager.OnD3D11CreateDevice(pd3dDevice, pContext));
    V_RETURN(g_SettingsDlg.OnD3D11CreateDevice(pd3dDevice));
    
    g_pTextHelper = new CDXUTTextHelper(pd3dDevice, pContext, &g_DialogResourceManager, 15);

	CreateTerrainEffect(pd3dDevice);
	CreateDeformEffect(pd3dDevice);

	// This array defines the outer width of each successive ring.
	int widths[] = { 0, 16, 16, 16, 16 };
	g_nRings = sizeof(widths) / sizeof(widths[0]) - 1;		// widths[0] doesn't define a ring hence -1
	assert(g_nRings <= MAX_RINGS);

	float tileWidth = 0.125f;
	for (int i=0; i!=g_nRings && i!=MAX_RINGS; ++i)
	{
		g_pTileRings[i] = new TileRing(pd3dDevice, widths[i]/2, widths[i+1], tileWidth);
		tileWidth *= 2.0f;
	}

	// This is a whole fraction of the max tessellation, i.e., 64/N.  The intent is that 
	// the height field scrolls through the terrain mesh in multiples of the polygon spacing.
	// So polygon vertices mostly stay fixed relative to the displacement map and this reduces
	// scintillation.  Without snapping, it scintillates badly.  Additionally, we make the
	// snap size equal to one patch width, purely to stop the patches dancing around like crazy.
	// The non-debug rendering works fine either way, but crazy flickering of the debug patches 
	// makes understanding much harder.
	const int PATCHES_PER_TILE_EDGE = VTX_PER_TILE_EDGE-1;
	SNAP_GRID_SIZE = WORLD_SCALE * g_pTileRings[g_nRings-1]->tileSize() / PATCHES_PER_TILE_EDGE;

	TileRing::CreateInputLayout(pd3dDevice, g_pTesselationTechnique);
	CreateTileTriangleIB(pd3dDevice);
	CreateTileQuadListIB(pd3dDevice);
	CreateQueries(pd3dDevice);

	LoadSkyboxFromFile(L"TerrainTessellation/MilkyWayCube.dds", pd3dDevice);
	ReleaseStars();
	CreateStars(pd3dDevice);
    
    return S_OK;
}

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
    HRESULT hr;

	ID3DX11EffectVectorVariable* pScreenSize = g_pTerrainEffect->GetVariableByName("g_screenSize")->AsVector();
	if (pScreenSize)
	{
		D3DXVECTOR2 v((float) pBackBufferSurfaceDesc->Width, (float) pBackBufferSurfaceDesc->Height);
		pScreenSize->SetFloatVector(v);
	}
    
    g_Skybox.OnD3D11ResizedSwapChain(pBackBufferSurfaceDesc);
    
    V_RETURN(g_DialogResourceManager.OnD3D11ResizedSwapChain(pd3dDevice, pBackBufferSurfaceDesc));
    V_RETURN(g_SettingsDlg.OnD3D11ResizedSwapChain(pd3dDevice, pBackBufferSurfaceDesc));

    g_HUD.SetLocation(pBackBufferSurfaceDesc->Width - 170, 0);
    g_HUD.SetSize(170, 170);
    g_SampleUI.SetLocation(pBackBufferSurfaceDesc->Width - 200, 0);
    g_SampleUI.SetSize(200, 300);

    ID3D11DeviceContext* pContext = DXUTGetD3D11DeviceContext();
    unsigned int n = 1;
    pContext->RSGetViewports(&n, &g_BackBufferVP);

	// Setup the camera's projection parameters
	g_ScreenSize = D3DXVECTOR2((float) pBackBufferSurfaceDesc->Width, (float) pBackBufferSurfaceDesc->Height);
    float aspectRatio = g_ScreenSize.x / g_ScreenSize.y;
    g_DebugCamera.SetProjParams(D3DX_PI / 3, aspectRatio, CLIP_NEAR, CLIP_FAR);
    g_MasterCamera.SetProjParams(D3DX_PI / 3, aspectRatio, CLIP_NEAR, CLIP_FAR);

	g_ResetTerrain = true;		// Reset once to init.

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext)
{
	const D3DXVECTOR3 beforePos = *g_pControlledCamera->GetEyePt();
	g_pControlledCamera->SetScalers(0.01f, nonlinearCameraSpeed(g_CameraSpeed));
    g_pControlledCamera->FrameMove(fElapsedTime);

	const bool moved = (beforePos != *g_pControlledCamera->GetEyePt()) != 0;
	g_ResetTerrain = g_ResetTerrain || moved;

	// The debug camera tracks the master until the debug camera is first used separately.
	if (!g_DebugCameraHasBeenUsed)
		g_DebugCamera = g_MasterCamera;
}

static void FullScreenPass(ID3D11DeviceContext* pContext, ID3DX11EffectVectorVariable* minVar, ID3DX11EffectVectorVariable* maxVar, ID3DX11EffectTechnique* pTech)
{
	// All of clip space:
	D3DXVECTOR3 areaMin(-1, -1, 0), areaMax(1, 1, 0);

	minVar->SetFloatVector(areaMin);
	maxVar->SetFloatVector(areaMax);
	pTech->GetPassByIndex(0)->Apply(0, pContext);
	pContext->Draw(4, 0);
}

static void InitializeHeights(ID3D11DeviceContext* pContext)
{
	ID3DX11EffectVectorVariable* deformMinVar = g_pDeformEffect->GetVariableByName("g_DeformMin")->AsVector();
	ID3DX11EffectVectorVariable* deformMaxVar = g_pDeformEffect->GetVariableByName("g_DeformMax")->AsVector();

	// This viewport is the wrong size for the texture.  But I've tweaked the noise frequencies etc to match.
	// So keep it like this for now.  TBD: tidy up.
	static const D3D11_VIEWPORT vp1 = { 0,0, (float) COARSE_HEIGHT_MAP_SIZE, (float) COARSE_HEIGHT_MAP_SIZE, 0.0f, 1.0f };
	pContext->RSSetViewports(1, &vp1);
	pContext->IASetInputLayout(NULL);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->OMSetRenderTargets(1, &g_pHeightMapRTV, NULL);

	FullScreenPass(pContext, deformMinVar, deformMaxVar, g_pInitializationTechnique);

	static const D3D11_VIEWPORT vp2 = { 0,0, (float) COARSE_HEIGHT_MAP_SIZE, (float) COARSE_HEIGHT_MAP_SIZE, 0.0f, 1.0f };
	pContext->RSSetViewports(1, &vp2);
	g_InputTexVar->SetResource(g_pHeightMapSRV);
	pContext->OMSetRenderTargets(1, &g_pGradientMapRTV, NULL);
	FullScreenPass(pContext, deformMinVar, deformMaxVar, g_pGradientTechnique);

	ID3D11RenderTargetView* pNULLRT = {NULL};
    pContext->OMSetRenderTargets(1, &(pNULLRT), NULL);
}

static void SetUVOffset(ID3DX11EffectVectorVariable* pVar)
{
	D3DXVECTOR3 eye = *g_MasterCamera.GetEyePt();
	eye.y = 0;
	if (SNAP_GRID_SIZE > 0)
	{
		eye.x = floorf(eye.x / SNAP_GRID_SIZE) * SNAP_GRID_SIZE;
		eye.z = floorf(eye.z / SNAP_GRID_SIZE) * SNAP_GRID_SIZE;
	}
	eye /= WORLD_SCALE;
	eye.z *= -1;
	pVar->SetFloatVector(eye);
}

void DeformInitTerrain(ID3D11DeviceContext* pContext)
{
	// Reset this so that it's not simultaneously resource and RT.
	g_HeightMapVar->SetResource(NULL);
	g_GradientMapVar->SetResource(NULL);
	g_pTesselationTechnique->GetPassByName("ShadedTriStrip")->Apply(0, pContext);

	int octaves[3] = { g_RidgeOctaves, g_fBmOctaves, g_TexTwistOctaves };
	g_pFractalOctavesDVar->SetIntVector(octaves);

	SetUVOffset(g_pUVOffsetDVar);
	InitializeHeights(pContext);
}

void SetViewport(ID3D11DeviceContext* pContext, const D3D11_VIEWPORT& vp)
{
	// Solid wireframe needs the vp in a GS constant.  Set both.
    pContext->RSSetViewports(1, &vp);
    
    float viewportf[4];
    viewportf[0] = (float) vp.Width;
    viewportf[1] = (float) vp.Height;
	viewportf[2] = (float) vp.TopLeftX;
	viewportf[3] = (float) vp.TopLeftY;
    g_pViewportVar->SetFloatVector(viewportf);
}

void SetMatrices(const CFirstPersonCamera* pCam, const D3DXMATRIX& mProj)
{
    const D3DXMATRIX mView = *pCam->GetViewMatrix();

	// Set matrices
	D3DXMATRIX mWorld, mScale, mTrans;
	D3DXMatrixScaling(&mScale, WORLD_SCALE, WORLD_SCALE, WORLD_SCALE);

	// We keep the eye centered in the middle of the tile rings.  The height map scrolls in the horizontal 
	// plane instead.
	const D3DXVECTOR3 eye = *g_MasterCamera.GetEyePt();
	float snappedX = eye.x, snappedZ = eye.z;
	if (SNAP_GRID_SIZE > 0)
	{
		snappedX = floorf(snappedX / SNAP_GRID_SIZE) * SNAP_GRID_SIZE;
		snappedZ = floorf(snappedZ / SNAP_GRID_SIZE) * SNAP_GRID_SIZE;
	}
	const float dx = eye.x - snappedX;
	const float dz = eye.z - snappedZ;
	snappedX = eye.x - 2*dx;				// Why the 2x?  I'm confused.  But it works.
	snappedZ = eye.z - 2*dz;
	D3DXMatrixTranslation(&mTrans, snappedX, 0, snappedZ);
	D3DXMatrixMultiply(&mWorld, &mScale, &mTrans);
    
	ID3DX11EffectMatrixVariable* pmWorldViewProj    = g_pTerrainEffect->GetVariableByName("g_WorldViewProj")->AsMatrix();
    ID3DX11EffectMatrixVariable* pmProj             = g_pTerrainEffect->GetVariableByName("g_Proj")->AsMatrix();
    ID3DX11EffectMatrixVariable* pmWorldViewProjLOD = g_pTerrainEffect->GetVariableByName("g_WorldViewProjLOD")->AsMatrix();
    ID3DX11EffectMatrixVariable* pmWorldViewLOD     = g_pTerrainEffect->GetVariableByName("g_WorldViewLOD")->AsMatrix();
	assert(pmProj->IsValid());
	assert(pmWorldViewProj->IsValid());
	assert(pmWorldViewProjLOD->IsValid());
	assert(pmWorldViewLOD->IsValid());

    D3DXMATRIX mWorldView = mWorld * mView;
    D3DXMATRIX mWorldViewProj = mWorldView * mProj;
    pmWorldViewProj->SetMatrix((float*) &mWorldViewProj);
    pmProj->SetMatrix((float*) &mProj);

	// For LOD calculations, we always use the master camera's view matrix.
    D3DXMATRIX mWorldViewLOD = mWorld * *g_MasterCamera.GetViewMatrix();;
    D3DXMATRIX mWorldViewProjLOD = mWorldViewLOD * mProj;
    pmWorldViewProjLOD->SetMatrix((float*) &mWorldViewProjLOD);
	pmWorldViewLOD->SetMatrix((float*) &mWorldViewLOD);

	// Due to the snapping tricks, the centre of projection moves by a small amount in the range ([0,2*dx],[0,2*dz])
	// relative to the terrain.  For frustum culling, we need this eye position.
	D3DXVECTOR3 cullingEye = eye;
	cullingEye.x -= snappedX;
	cullingEye.z -= snappedZ;
    g_pEyePosVar->SetFloatVector(cullingEye);
	g_pViewDirVar->SetFloatVector((float*) g_MasterCamera.GetWorldAhead());
}

void RenderTerrain(ID3D11DeviceContext* pContext, const D3DXMATRIX& mProj, const D3D11_VIEWPORT& vp, const char* passOverride=NULL)
{
	const CFirstPersonCamera* pCam = (g_syncCameras)? g_pControlledCamera: &g_DebugCamera;
	SetMatrices(pCam, mProj);

	if (g_HwTessellation)
	{
		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
		pContext->IASetIndexBuffer(g_TileQuadListIB, DXGI_FORMAT_R32_UINT, 0);
	}
	else
	{
		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		pContext->IASetIndexBuffer(g_TileTriStripIB, DXGI_FORMAT_R32_UINT, 0);
	}

	const char* passName = "ShadedTriStrip";

	const bool wire = g_SampleUI.GetCheckBox(IDC_WIREFRAME)->GetChecked();
	if (wire)
		passName = "Wireframe";
	if (g_HwTessellation)
	{
		if (wire)
			passName = "HwTessellatedWireframe";
		else
			passName = "HwTessellated";
	}
	if (passOverride != NULL)
		passName = passOverride;

	ID3DX11EffectPass* pPass = g_pTesselationTechnique->GetPassByName(passName);
	if (!pPass)
		return;		// Shouldn't happen unless the FX file is broken (like wrong pass name).

	SetViewport(pContext, vp);

	for (int i=0; i!=g_nRings; ++i)
	{
		const TileRing* pRing = g_pTileRings[i];
		pRing->SetRenderingState(pContext);

		g_HeightMapVar->SetResource(g_pHeightMapSRV);
		g_GradientMapVar->SetResource(g_pGradientMapSRV);
		g_pTileSizeVar->SetFloat(pRing->tileSize());
	
		// Need to apply the pass after setting its vars.
		pPass->Apply(0, pContext);

		// Instancing is used: one tiles is one instance and the index buffer describes all the 
		// NxN patches within one tile.
		const int nIndices = (g_HwTessellation)? QUAD_LIST_INDEX_COUNT: TRI_STRIP_INDEX_COUNT;
		pContext->DrawIndexedInstanced(nIndices, pRing->nTiles(), 0, 0, 0);
	}
}

ID3D11Query* FindFreeQuery()
{
	for (int i=0; i!=N_QUERIES; ++i)
	{
		if (g_FreePipelineQueries[i])
		{
			ID3D11Query* pResult = g_FreePipelineQueries[i];
			g_FreePipelineQueries[i] = NULL;
			return pResult;
		}
	}
	
	return NULL;
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void RenderToBackBuffer(ID3D11DeviceContext* pContext, double fTime, float fElapsedTime, void* pUserContext)
{
    ID3D11RenderTargetView* pBackBufferRTV = DXUTGetD3D11RenderTargetView();
    ID3D11DepthStencilView* pBackBufferDSV = DXUTGetD3D11DepthStencilView();

    // Clear render target and the depth stencil 
    float ClearColor[4] = { 1.0f, 0.0f, 1.0f, 1.0f };					// Purple to better spot terrain cracks (disable sky cube).
    //float ClearColor[4] = { 0.465f, 0.725f, 0.0f, 1.0f };				// NV green for colour-consistent illustrations.
    pContext->ClearRenderTargetView(pBackBufferRTV, ClearColor);
    pContext->ClearDepthStencilView(pBackBufferDSV, D3D11_CLEAR_DEPTH, 1.0, 0);

    pContext->OMSetRenderTargets(1, &pBackBufferRTV, pBackBufferDSV);

	// Something's wrong in the shader and the tri size is out by a factor of 2.  Why?!?
	g_pTriSizeVar->SetInt(2 * g_tessellatedTriWidth);

	const bool debugDrawPatches = g_SampleUI.GetCheckBox(IDC_DEBUG_SHOW_PATCHES)->GetChecked();
	g_DebugShowPatchesVar->SetBool(debugDrawPatches);

	const bool debugDrawTiles = g_SampleUI.GetCheckBox(IDC_DEBUG_SHOW_TILES)->GetChecked();
	g_DebugShowTilesVar->SetBool(debugDrawTiles);

	const float wireAlpha = 0.01f * (float) g_SampleUI.GetSlider(IDC_WIREFRAME_ALPHA)->GetValue();

	// Below 1.0, we fade the lines out with blending; above 1, we increase line thickness.
	if (wireAlpha < 1)
	{
		g_WireAlphaVar->SetFloat(wireAlpha);
		g_WireWidthVar->SetFloat(1);
	}
	else
	{
		g_WireAlphaVar->SetFloat(1);
		g_WireWidthVar->SetFloat(wireAlpha);
	}

	g_DetailNoiseVar->SetFloat(0.001f * (float)g_DetailNoiseScale);
	g_SampleSpacingVar->SetFloat(WORLD_SCALE * g_pTileRings[g_nRings-1]->outerWidth() / (float) COARSE_HEIGHT_MAP_SIZE);

	// If the settings dialog is being shown, then render it instead of rendering the app's scene
    if (g_SettingsDlg.IsActive())
    {
        g_SettingsDlg.OnRender(fElapsedTime);
        return;
    }
    
	const CFirstPersonCamera* pCam = (g_syncCameras)? g_pControlledCamera: &g_DebugCamera;
    const D3DXMATRIX mProj = *pCam->GetProjMatrix();
    const D3DXMATRIX mView = *pCam->GetViewMatrix();

	SetViewport(pContext, g_BackBufferVP);

	D3DXMATRIX mViewCopy = mView;
	mViewCopy._41 = mViewCopy._42 = mViewCopy._43 = 0;
	D3DXMATRIX mWVP = StarWorldMatrix() * mViewCopy * mProj;
	if (!g_CheckForCracks)
		g_Skybox.D3D11Render(&mWVP, pContext);

	RenderStars(pContext, mViewCopy, mProj, g_ScreenSize);

	int vec[3] = { g_RidgeOctaves, g_fBmOctaves, g_TexTwistOctaves };
	g_pFractalOctavesTVar->SetIntVector(vec);

	// I'm still trying to figure out if the detail scale can be derived from any combo of ridge + twist.
	// I don't think this works well (nor does ridge+twist+fBm).  By contrast the relationship with fBm is
	// straightforward.  The -4 is a fudge factor that accounts for the frequency of the coarsest ocatve
	// in the pre-rendered detail map.
	const float DETAIL_UV_SCALE = powf(2.0f, std::max(g_RidgeOctaves, g_TexTwistOctaves) + g_fBmOctaves - 4.0f);
	g_DetailUVVar->SetFloatVector(D3DXVECTOR2(DETAIL_UV_SCALE, 1.0f/DETAIL_UV_SCALE));

	SetUVOffset(g_pUVOffsetTVar);

	ID3D11Query* pFreeQuery = FindFreeQuery();
	if (pFreeQuery)
		pContext->Begin(pFreeQuery);

	RenderTerrain(pContext, mProj, g_BackBufferVP);

	if (pFreeQuery)
		pContext->End(pFreeQuery);

	for (int i=0; i!=N_QUERIES; ++i)
	{
		if (!g_FreePipelineQueries[i] && g_PipelineQueries[i])	// in use & exists
		{
			D3D11_QUERY_DATA_PIPELINE_STATISTICS stats;
			if (S_OK == pContext->GetData(g_PipelineQueries[i], &stats, sizeof(stats), D3D11_ASYNC_GETDATA_DONOTFLUSH))
			{
				g_PrimitivesRendered = stats.CInvocations;
				g_FreePipelineQueries[i] = g_PipelineQueries[i];	// Put back on free list.
			}
		}
	}

    // Render GUI
    DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"HUD / Stats");
    RenderText();
    g_HUD.OnRender(fElapsedTime); 
    g_SampleUI.OnRender(fElapsedTime);
    DXUT_EndPerfEvent();

    ID3D11RenderTargetView* pNULLRT = {NULL};
    pContext->OMSetRenderTargets(1, &(pNULLRT), NULL);
}

void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pContext,double fTime, float fElapsedTime, void* pUserContext)
{
	if (g_ResetTerrain)
	{
		g_ResetTerrain = false;
		DeformInitTerrain(pContext);
	}

	RenderToBackBuffer(pContext, fTime, fElapsedTime, pUserContext);
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext)
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
    
    g_Skybox.OnD3D11ReleasingSwapChain();

    ID3D11RenderTargetView* pNullView = { NULL };
    DXUTGetD3D11DeviceContext()->OMSetRenderTargets(1, &pNullView, NULL);
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice(void* pUserContext)
{
	g_pD3D11Device = NULL;
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_SettingsDlg.OnD3D11DestroyDevice();
    
	g_Skybox.OnD3D11DestroyDevice();
    ReleaseStars();
	TileRing::ReleaseInputLayout();

	for (int i=0; i!=g_nRings; ++i)
	{
		delete g_pTileRings[i];
		g_pTileRings[i] = NULL;
	}

    SAFE_RELEASE(g_pTerrainEffect);
    SAFE_RELEASE(g_pDeformEffect);

    SAFE_RELEASE(g_pHeightMapSRV);
    SAFE_RELEASE(g_pHeightMapRTV);
    SAFE_RELEASE(g_pGradientMapSRV);
    SAFE_RELEASE(g_pGradientMapRTV);

	SAFE_RELEASE(g_TileTriStripIB);
    SAFE_RELEASE(g_TileQuadListIB);

	for (int i=0; i!=N_QUERIES; ++i)
		SAFE_RELEASE(g_PipelineQueries[i]);
    
    SAFE_DELETE(g_pTextHelper);
}

//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, 
                         bool* pbNoFurtherProcessing, void* pUserContext)
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc(hWnd, uMsg, wParam, lParam);
    if (*pbNoFurtherProcessing)
        return 0;

    // Pass messages to settings dialog if its active
    if (g_SettingsDlg.IsActive())
    {
        g_SettingsDlg.MsgProc(hWnd, uMsg, wParam, lParam);
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc(hWnd, uMsg, wParam, lParam);
    if (*pbNoFurtherProcessing)
        return 0;

    *pbNoFurtherProcessing = g_SampleUI.MsgProc(hWnd, uMsg, wParam, lParam);
    if (*pbNoFurtherProcessing)
        return 0;

    // Pass all remaining windows messages to camera so it can respond to user input
    g_pControlledCamera->HandleMessages(hWnd, uMsg, wParam, lParam);

    return 0;
}

//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard(UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext)
{
	if (bKeyDown)
	{
		switch (nChar)
		{
			case VK_F1:
				g_ShowHelp = !g_ShowHelp;
				break;
			case 'h':
			case 'H':
				// For screenshots.
				g_SampleUI.SetVisible(!g_SampleUI.GetVisible());
				g_HUD.SetVisible(!g_HUD.GetVisible());
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				if (bAltDown)
					SaveCamera(nChar - '0', *g_pControlledCamera);
				else
					LoadCamera(nChar - '0', *g_pControlledCamera);
				break;
		}
	}
}

//--------------------------------------------------------------------------------------
// Handle mouse button presses
//--------------------------------------------------------------------------------------
void CALLBACK OnMouse(bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown, 
                      bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta, 
                      int xPos, int yPos, void* pUserContext)
{
}


//--------------------------------------------------------------------------------------
// Call if device was removed.  Return true to find a new device, false to quit
//--------------------------------------------------------------------------------------
bool CALLBACK OnDeviceRemoved(void* pUserContext)
{
    return true;
}

//--------------------------------------------------------------------------------------
// Initialize everything and go into a render loop
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    HRESULT hr;
    V_RETURN(DXUTSetMediaSearchPath(L"..\\Source\\TerrainTessellation"));
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    // Set general DXUT callbacks
    DXUTSetCallbackFrameMove(OnFrameMove);
    DXUTSetCallbackKeyboard(OnKeyboard);
    DXUTSetCallbackMouse(OnMouse);
    DXUTSetCallbackMsgProc(MsgProc);
    DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);
    DXUTSetCallbackDeviceRemoved(OnDeviceRemoved);

    // Set the D3D11 DXUT callbacks. Remove these sets if the app doesn't need to support D3D11
    DXUTSetCallbackD3D11DeviceAcceptable(IsD3D11DeviceAcceptable);
    DXUTSetCallbackD3D11DeviceCreated(OnD3D11CreateDevice);
    DXUTSetCallbackD3D11SwapChainResized(OnD3D11ResizedSwapChain);
    DXUTSetCallbackD3D11FrameRender(OnD3D11FrameRender);
    DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11ReleasingSwapChain);
    DXUTSetCallbackD3D11DeviceDestroyed(OnD3D11DestroyDevice);
    
    InitApp();

    DXUTInit(true, true, NULL); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings(true, true); // Show the cursor and clip it when in full screen
    DXUTCreateWindow(L"TerrainTessellation");
    DXUTGetD3D11Enumeration(true, true);
    DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, 1920, 1200);
    DXUTMainLoop(); // Enter into the DXUT render loop

    // Perform any application-level cleanup here

    return DXUTGetExitCode();
}
