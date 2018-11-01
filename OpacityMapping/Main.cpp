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

#include "Camera.h"
#include "ParticleSystem.h"
#include "LightSource.h"
#include "Floor.h"
#include "Util.h"
#include "LineDraw.h"
#include "LowResOffscreen.h"
#include "OpaqueObject.h"
#include "SharedDefines.h"

#include "DXUTgui.h"
#include "DXUTmisc.h"
#include "DXUTSettingsDlg.h"
#include "SDKmisc.h"

#include "D3DX11Effect.h"

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CCustomCamera               g_Camera;                // A model viewing camera
CDXUTDialogResourceManager  g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg             g_SettingsDlg;           // Device settings dialog
CDXUTTextHelper*            g_pTxtHelper = NULL;
CDXUTDialog                 g_HUD;                   // dialog for standard controls
CDXUTDialog                 g_SampleUI;              // dialog for sample specific controls

// Direct3D resources
ID3DX11Effect*              g_pEffect = NULL;
ID3D11Texture2D*            g_pDepthStencilTexture = NULL;
ID3D11DepthStencilView*     g_pDepthStencilTextureDSV = NULL;
ID3D11ShaderResourceView*   g_pDepthStencilTextureSRV = NULL;
ID3D11RenderTargetView*     g_p16FRenderTextureRTV = NULL;
ID3D11ShaderResourceView*   g_p16FRenderTextureSRV = NULL;
ID3D11Texture2D*            g_p16FRenderTexture = NULL;
ID3D11Texture2D*            g_p16FResolveTexture = NULL;

// Tracks the AA mode that the global effect was compiled with
MSAAMode					g_EffectAAMode = NoAA;

// Effect hooks
ID3DX11EffectMatrixVariable* g_mWorldToView_EffectVar = NULL;
ID3DX11EffectMatrixVariable* g_mViewToProj_EffectVar = NULL;
ID3DX11EffectVectorVariable* g_EyePoint_EffectVar = NULL;

// Sub-systems
CLowResOffscreen g_LowResOffscreen(1);
CParticleSystem g_ParticleSystem(512);
CLightSources g_LightSources(512, 2, 128);
CFloor g_Floor(0.2f, 0.005f, 20);

const D3DXVECTOR3 kOpaqueObjectAnchorPoint(0.f,3.f,0.f);
const D3DXVECTOR3 kOpaqueObjectStartPoint(0.3f,0.5f,0.3f);
const double kOpaqueObjectPhase = 0.9576355;
COpaqueObject g_OpaqueObject(kOpaqueObjectAnchorPoint, kOpaqueObjectStartPoint, kOpaqueObjectPhase);

// State/feature flags
BOOL g_SaveState = FALSE;
BOOL g_ShowBounds = FALSE;
BOOL g_ShowOpacityTargets = FALSE;
BOOL g_InflateBounds = FALSE;
BOOL g_SkipMainRender = FALSE;
BOOL g_SkipOpacityRender = FALSE;
BOOL g_MeteredOverdraw = FALSE;
BOOL g_ShowHelp = FALSE;
BOOL g_ShowUI = TRUE;
BOOL g_CycleOpacityShadowIntensity = TRUE;
BOOL g_ShowOpacityShadowCycledOffWarning = FALSE;

FLOAT g_BackbufferAspectRatio = 0.f;
FLOAT g_LocalBoundsAlpha = 1.f;
FLOAT g_LightBoundsAlpha = 1.f;

enum FillReductionMethod {
    FRM_None = 0,
    FRM_LowResOffscreen = 1,
	FRM_LowResOffscreen_Soft = 2,
};
INT g_FillReductionMethod = FRM_LowResOffscreen_Soft;

// Simulation time/pausing
double g_SimulationTime = 0.f;
BOOL g_SimulationPaused = FALSE;

//--------------------------------------------------------------------------------------
// Constants
//--------------------------------------------------------------------------------------
const DXGI_FORMAT kMainSceneBufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

const FLOAT kZNear = 0.1f;
const FLOAT kZFar = 100.f;

const D3DXMATRIX InitialLight0Transform = D3DXMATRIX(   -0.66646081f,  0.73595816f, -0.11914623f, 0.00000000f,
                                                         0.37681967f,  0.19462481f, -0.90560943f, 0.00000000f,
                                                        -0.64330155f, -0.64844966f, -0.40703326f, 0.00000000f,
                                                         0.00000000f,  0.00000000f,  0.00000000f, 0.99998331f
                                                        );

const D3DXMATRIX InitialLight1Transform = D3DXMATRIX(    0.91075832f, -0.18810505f,  0.36760876f, 0.00000000f,
                                                        -0.32734728f, -0.87154430f,  0.36504090f, 0.00000000f,
                                                         0.25172073f, -0.45279962f, -0.85534209f, 0.00000000f,
                                                         0.00000000f,  0.00000000f,  0.00000000f, 0.99983329f
                                                        );

const D3DXMATRIX InitialLight2Transform = D3DXMATRIX(   1.f, 0.f, 0.f, 0.f,
                                                        0.f, 0.f, 1.f, 0.f,
                                                        0.f,-1.f, 0.f, 0.f,
                                                        0.f, 0.f, 0.f, 1.f
                                                        );

const D3DXMATRIX InitialLight3Transform = D3DXMATRIX(   -0.033414662f, 0.85001773f, 0.52569354f, 0.00000000f,
                                                        -0.93943179f, -0.20622925f, 0.27374801f, 0.00000000f,
                                                         0.34110418f, -0.48470643f, 0.80542433f, 0.00000000f,
                                                         0.00000000f,  0.00000000f, 0.00000000f, 0.99948430f
                                                        );

//--------------------------------------------------------------------------------------
// UI control IDs + helpers
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN                       1
#define IDC_TOGGLEREF                              2
#define IDC_CHANGEDEVICE                           3
#define IDC_TOGGLEBOUNDS                           4
#define IDC_TOGGLESHOWOPACITYTARGETS               5
#define IDC_OPACITYMULTIPLIER_LABEL                6
#define IDC_OPACITYMULTIPLIER_SLIDER               7
#define IDC_TOGGLEINFLATEBOUNDS                    8
#define IDC_OPACITY_SIZE_LABEL                     9
#define IDC_OPACITY_SIZE_SLIDER                    10
#define IDC_LIGHTINGMODE                           11
#define IDC_TOGGLEWIREFRAME                        12
#define IDC_FILLREDUCTIONMETHOD                    13
#define IDC_TOGGLEMETEREDOVERDRAW                  19
#define IDC_LOWRES_UPSCALE_DEPTH_THRESHOLD_LABEL   20
#define IDC_LOWRES_UPSCALE_DEPTH_THRESHOLD_SLIDER  21
#define IDC_TOGGLESIMULATION					   22
#define IDC_SOFT_PARTICLE_FADE_DISTANCE_LABEL      23
#define IDC_SOFT_PARTICLE_FADE_DISTANCE_SLIDER     24
#define IDC_TOGGLE_CYCLEOPACITYSHADOWSTRENGTH      25

void UpdateOpacitySizeLabel()
{
    WCHAR buffer[256];
    swprintf_s(buffer, 256, L"Opacity map size: %d", g_LightSources.GetOpacityMapMipLevelWH());
    g_SampleUI.GetStatic(IDC_OPACITY_SIZE_LABEL)->SetText(buffer);
}

void UpdateFillReductionControls()
{
	const bool bLowResEnabled = (FRM_LowResOffscreen == g_FillReductionMethod || FRM_LowResOffscreen_Soft == g_FillReductionMethod);
    g_SampleUI.GetControl(IDC_LOWRES_UPSCALE_DEPTH_THRESHOLD_SLIDER)->SetEnabled(bLowResEnabled);
    g_SampleUI.GetControl(IDC_LOWRES_UPSCALE_DEPTH_THRESHOLD_SLIDER)->SetVisible(bLowResEnabled);
    g_SampleUI.GetControl(IDC_LOWRES_UPSCALE_DEPTH_THRESHOLD_LABEL)->SetVisible(bLowResEnabled);

	const bool bSoftEnabled = (FRM_LowResOffscreen_Soft == g_FillReductionMethod);
    g_SampleUI.GetControl(IDC_SOFT_PARTICLE_FADE_DISTANCE_SLIDER)->SetEnabled(bSoftEnabled);
    g_SampleUI.GetControl(IDC_SOFT_PARTICLE_FADE_DISTANCE_SLIDER)->SetVisible(bSoftEnabled);
    g_SampleUI.GetControl(IDC_SOFT_PARTICLE_FADE_DISTANCE_LABEL)->SetVisible(bSoftEnabled);
}

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );

bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void CALLBACK OnD3D11DestroyDevice( void* pUserContext );
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext );

void InitApp(LPWSTR lpCmdLine);
void RenderText();

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    HRESULT hr;
    V_RETURN(DXUTSetMediaSearchPath(L"..\\Source\\OpacityMapping"));
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Set DXUT callbacks
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );

    InitApp(lpCmdLine);
    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true );
    DXUTCreateWindow( L"OpacityMapping" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_10_0, true, 1024, 768 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}

//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp(LPWSTR lpCmdLine)
{
    // Parse the command-line
    int nNumArgs;
    WCHAR** pstrArgList = CommandLineToArgvW( lpCmdLine, &nNumArgs );
    for( int iArg = 0; iArg < nNumArgs; iArg++ )
    {
        WCHAR* strCmdLine = pstrArgList[iArg];

        // Handle flag args
        if( *strCmdLine == L'/' || *strCmdLine == L'-' )
        {
            ++strCmdLine;

            if(0 == _wcsicmp(L"frozen",strCmdLine))
            {
                g_SimulationPaused = TRUE;
				g_Camera.DisableCameraOrbit();
				g_CycleOpacityShadowIntensity = FALSE;
            }
        }
    }

	// Init camera
    D3DXVECTOR3 vecEye( 0.0f, 0.5f, -1.3f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, -0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
    g_Camera.SetButtonMasks(MOUSE_RIGHT_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON);

    g_ParticleSystem.InitPlumeSim();
    g_ParticleSystem.SetMeteredOverdraw(g_MeteredOverdraw);

    g_LightSources.InitLight(0, InitialLight0Transform, D3DXVECTOR4(0.65f, 0.4f, 0.2f, 1.f));
    g_LightSources.InitLight(1, InitialLight1Transform, D3DXVECTOR4(0.25f, 0.25f, 0.75f, 1.f));
    g_LightSources.InitLight(2, InitialLight2Transform, D3DXVECTOR4(0.05f, 0.05f, 0.08f, 1.f));
    //g_LightSources.InitLight(3, InitialLight3Transform, D3DXVECTOR4(0.05f, 0.02f, 0.01f, 1.f));

    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 10;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 23 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 23, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 23, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent );

    int iX = 0; iY = 0;

	// Simulation pause
	g_SampleUI.AddStatic(0, L"Simulation", iX, iY += 26, 170, 23);

    iX += 20;
    g_SampleUI.AddCheckBox( IDC_TOGGLESIMULATION, L"On/off", iX, iY += 26, 170, 23, !g_SimulationPaused );

	// Tessellation controls
	iX = 0; iY += 30;
    g_SampleUI.AddStatic(0, L"Lighting Method", iX, iY += 26, 170, 23);

    iX += 20;
	CDXUTComboBox* pLightingModeCombo = NULL;
	g_SampleUI.AddComboBox( IDC_LIGHTINGMODE,iX, iY += 26, 170, 26, 0, false, &pLightingModeCombo );
    pLightingModeCombo->AddItem( L"Vertex", NULL );
    pLightingModeCombo->AddItem( L"Tessellated", NULL );
	pLightingModeCombo->AddItem( L"Pixel", NULL );
    pLightingModeCombo->SetSelectedByIndex(g_ParticleSystem.GetLightingMode());
    pLightingModeCombo->SetDropHeight(48);

    g_SampleUI.AddCheckBox( IDC_TOGGLEWIREFRAME, L"Wireframe", iX, iY += 26, 170, 23, TRUE == g_ParticleSystem.GetWireframe() );

    // Fill reduction mapping controls
    iX = 0; iY += 30;
    g_SampleUI.AddStatic(0, L"Fill Reduction", iX, iY += 26, 170, 23);

    iX += 20;

    CDXUTComboBox* pFillReductionMethodCombo = NULL;
    g_SampleUI.AddComboBox( IDC_FILLREDUCTIONMETHOD,iX, iY += 26, 170, 26, 0, false, &pFillReductionMethodCombo );
    pFillReductionMethodCombo->AddItem( L"None", NULL );
    pFillReductionMethodCombo->AddItem( L"Lo-res", NULL );
	pFillReductionMethodCombo->AddItem( L"Lo-res (soft)", NULL );
    pFillReductionMethodCombo->SetSelectedByIndex(g_FillReductionMethod);
    pFillReductionMethodCombo->SetDropHeight(32);

    g_SampleUI.AddCheckBox( IDC_TOGGLEMETEREDOVERDRAW, L"Show full-res overdraw", iX, iY += 26, 170, 23, TRUE == g_MeteredOverdraw);

	UINT loResY = iY;
	g_SampleUI.AddStatic( IDC_LOWRES_UPSCALE_DEPTH_THRESHOLD_LABEL, L"Upscale depth threshold", iX, loResY += 26, 170, 23 );
    g_SampleUI.AddSlider( IDC_LOWRES_UPSCALE_DEPTH_THRESHOLD_SLIDER, iX, loResY += 18, 170, 23, 1, 100, INT(100.f * (1.f + log10f(g_LowResOffscreen.GetUpsampleFilterDepthThreshold())/6.f)) );

	g_SampleUI.AddStatic( IDC_SOFT_PARTICLE_FADE_DISTANCE_LABEL, L"Soft fade distance", iX, loResY += 26, 170, 23 );
    g_SampleUI.AddSlider( IDC_SOFT_PARTICLE_FADE_DISTANCE_SLIDER, iX, loResY += 18, 170, 23, 1, 100, INT(g_ParticleSystem.GetSoftParticleFadeDistance() * 100.f) );

	iY = loResY;

    UpdateFillReductionControls();

    // Add opacity-mapping controls
    iX = 0; iY += 30;
    g_SampleUI.AddStatic(0, L"Opacity Shadows", iX, iY += 26, 170, 23);

    iX += 20;
    g_SampleUI.AddCheckBox( IDC_TOGGLEBOUNDS, L"Show bounds", iX, iY += 26, 170, 23, TRUE == g_ShowBounds );
    g_SampleUI.AddCheckBox( IDC_TOGGLEINFLATEBOUNDS, L"Inflate bounds", iX, iY += 26, 170, 23, TRUE == g_InflateBounds );
    g_SampleUI.AddCheckBox( IDC_TOGGLESHOWOPACITYTARGETS, L"Show render targets", iX, iY += 26, 170, 23, TRUE == g_ShowOpacityTargets );
	g_SampleUI.AddCheckBox( IDC_TOGGLE_CYCLEOPACITYSHADOWSTRENGTH, L"Cycle shadow intensity", iX, iY += 26, 170, 23, TRUE == g_CycleOpacityShadowIntensity );

    iY += 12;
    g_SampleUI.AddStatic( IDC_OPACITYMULTIPLIER_LABEL, L"Opacity multiplier", iX, iY += 26, 170, 23 );
    g_SampleUI.AddSlider( IDC_OPACITYMULTIPLIER_SLIDER, iX, iY += 18, 170, 23, 0, 100, int(g_LightSources.GetOpacityMultiplier() * 100.f) );

    iY += 12;
    g_SampleUI.AddStatic( IDC_OPACITY_SIZE_LABEL, L"Opacity map size", iX, iY += 26, 170, 23 );
    g_SampleUI.AddSlider( IDC_OPACITY_SIZE_SLIDER, iX, iY += 18, 170, 23, 0, 8, 8-g_LightSources.GetOpacityMapMipLevel() );
    UpdateOpacitySizeLabel();
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text. This function uses the ID3DXFont interface for 
// efficient text rendering.
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 5, 5 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

	UINT nBackBufferHeight = DXUTGetDXGIBackBufferSurfaceDesc()->Height;
	UINT nBackBufferWidth = DXUTGetDXGIBackBufferSurfaceDesc()->Width;
    if (g_ShowHelp)
    {
        
        g_pTxtHelper->SetInsertionPos( 5, nBackBufferHeight-105 );
        g_pTxtHelper->DrawTextLine( L"Controls:" );

        g_pTxtHelper->SetInsertionPos( 20, nBackBufferHeight-85 );
        g_pTxtHelper->DrawTextLine( L"Rotate light:  Right mouse button\n"
									L"Zoom camera:  Mouse wheel scroll\n"
									L"Rotate camera: Left mouse button");

        g_pTxtHelper->SetInsertionPos( 300, nBackBufferHeight-85 );
        g_pTxtHelper->DrawTextLine(	L"Toggle opacity mapping: O\n"
									L"Toggle particles: R\n"
									L"Toggle camera orbit: C\n" );

        g_pTxtHelper->SetInsertionPos( 530, nBackBufferHeight-85 );
        g_pTxtHelper->DrawTextLine(	L"Cycle light control: L\n"
									L"Hide help: F1\n" 
									L"Quit: ESC\n" );
    }
    else
    {
        g_pTxtHelper->DrawTextLine( L"Press F1 for help" );
    }

	if(g_ShowOpacityShadowCycledOffWarning || g_SkipOpacityRender)
	{
		g_pTxtHelper->SetInsertionPos( nBackBufferWidth/2 - 90, nBackBufferHeight - 120 );
		g_pTxtHelper->DrawTextLine(L"!!!Opacity Shadows Off!!!");
	}

    g_pTxtHelper->End();
}

//--------------------------------------------------------------------------------------
// Rejects any D3D11 devices that aren't acceptable to the app by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                       DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
}

//--------------------------------------------------------------------------------------
// Called right before creating a device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if(DXUT_D3D11_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }

        // Also, prefer 4xAA, first time
        if(DXUT_D3D11_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d11.DriverType != D3D_DRIVER_TYPE_REFERENCE )
        {
            pDeviceSettings->d3d11.sd.SampleDesc.Count = 4;
            pDeviceSettings->d3d11.sd.SampleDesc.Quality = 0;
        }
    }

    // We create our own depth-stencil
    pDeviceSettings->d3d11.AutoCreateDepthStencil = FALSE;

    return true;
}

//--------------------------------------------------------------------------------------
// Load (or reload) to the global effect
//--------------------------------------------------------------------------------------
HRESULT LoadEffect(ID3D11Device* pd3dDevice)
{
	HRESULT hr;

	SAFE_RELEASE(g_pEffect);

	// Set up the AA-mode pre-processor macro
    D3DXMACRO macros[] = {
        { "NUM_AA_SAMPLES", GetStringFromAAMode(g_EffectAAMode) },
        { NULL, NULL }
    };

    // Read the D3DX effect file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"Main.fx" ) );
    ID3DXBuffer* pEffectBuffer = NULL;
    V_RETURN(D3DXCompileShaderFromFile(str, macros, NULL, NULL, "fx_5_0", 0, &pEffectBuffer, NULL, NULL));
    V_RETURN(D3DX11CreateEffectFromMemory(pEffectBuffer->GetBufferPointer(), pEffectBuffer->GetBufferSize(), 0, pd3dDevice, &g_pEffect));
    SAFE_RELEASE(pEffectBuffer)

    // Retrieve global effect params
    g_mWorldToView_EffectVar = g_pEffect->GetVariableByName("g_mWorldToView")->AsMatrix();
    g_mViewToProj_EffectVar = g_pEffect->GetVariableByName("g_mViewToProj")->AsMatrix();
    g_EyePoint_EffectVar = g_pEffect->GetVariableByName("g_EyePoint")->AsVector();

    // Init sub-systems
    V_RETURN( CScreenAlignedQuad::Instance().BindToEffect(g_pEffect) );
    V_RETURN( g_ParticleSystem.BindToEffect(pd3dDevice, g_pEffect) );
    V_RETURN( g_LightSources.BindToEffect(pd3dDevice, g_pEffect) );
    V_RETURN( g_Floor.BindToEffect(pd3dDevice, g_pEffect) );
    V_RETURN( g_LowResOffscreen.BindToEffect(g_pEffect) );
    V_RETURN( CLineDraw::Instance().BindToEffect(pd3dDevice, g_pEffect) );
    V_RETURN( g_OpaqueObject.BindToEffect(pd3dDevice, g_pEffect) );

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
    HRESULT hr;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

    if(pd3dDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0)
    {
		g_ParticleSystem.SetLightingMode(CParticleSystem::LM_Tessellated);
		g_SampleUI.GetComboBox(IDC_LIGHTINGMODE)->SetSelectedByIndex(CParticleSystem::LM_Tessellated);
        g_SampleUI.SetControlEnabled( IDC_LIGHTINGMODE, TRUE );
    }
    else
    {
        g_ParticleSystem.SetLightingMode(CParticleSystem::LM_Pixel);
        g_SampleUI.GetComboBox(IDC_LIGHTINGMODE)->SetSelectedByIndex(CParticleSystem::LM_Pixel);
        g_SampleUI.SetControlEnabled( IDC_LIGHTINGMODE, FALSE );
    }

    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_SettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );

    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    // Init sub-systems
    CScreenAlignedQuad::Instance().OnD3D11CreateDevice(pd3dDevice);
    g_ParticleSystem.OnD3D11CreateDevice(pd3dDevice);
    g_LightSources.OnD3D11CreateDevice(pd3dDevice);
    CDrawUP::Instance().OnD3D11CreateDevice(pd3dDevice);
    g_Floor.OnD3D11CreateDevice(pd3dDevice);
    g_LowResOffscreen.OnD3D11CreateDevice(pd3dDevice);
    CLineDraw::Instance().OnD3D11CreateDevice(pd3dDevice);
    g_OpaqueObject.OnD3D11CreateDevice(pd3dDevice);

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Camera management helper
//--------------------------------------------------------------------------------------
void UpdateCameraProj()
{
    g_Camera.SetProjParams( D3DX_PI / 4, g_BackbufferAspectRatio, kZNear, kZFar );
}

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();

    {
        // Create depth-stencil
        D3D11_TEXTURE2D_DESC descDepth;
        descDepth.Width = pBackBufferSurfaceDesc->Width;
        descDepth.Height = pBackBufferSurfaceDesc->Height;
        descDepth.MipLevels = 1;
        descDepth.ArraySize = 1;
        descDepth.Format = DXGI_FORMAT_R24G8_TYPELESS;
        descDepth.SampleDesc.Count = pBackBufferSurfaceDesc->SampleDesc.Count;
        descDepth.SampleDesc.Quality = pBackBufferSurfaceDesc->SampleDesc.Quality;
        descDepth.Usage = D3D11_USAGE_DEFAULT;
        descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        descDepth.CPUAccessFlags = 0;
        descDepth.MiscFlags = 0;
        V_RETURN(pd3dDevice->CreateTexture2D( &descDepth, NULL, &g_pDepthStencilTexture ));

        // Create the depth stencil view
        D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
        descDSV.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        descDSV.Flags = 0;
        if( descDepth.SampleDesc.Count > 1 )
            descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
        else
            descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        descDSV.Texture2D.MipSlice = 0;
        V_RETURN(pd3dDevice->CreateDepthStencilView( g_pDepthStencilTexture, &descDSV, &g_pDepthStencilTextureDSV ));

        // Also create a shader resource view for the depth-stencil
        D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
        descSRV.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        if( descDepth.SampleDesc.Count > 1 )
            descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
        else
        {
            descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            descSRV.Texture2D.MostDetailedMip = 0;
            descSRV.Texture2D.MipLevels = descDepth.MipLevels;
        }

        V_RETURN(pd3dDevice->CreateShaderResourceView( g_pDepthStencilTexture, &descSRV, &g_pDepthStencilTextureSRV ));
    }

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_SettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    g_BackbufferAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    UpdateCameraProj();
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    // Lay out UI
    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 220, pBackBufferSurfaceDesc->Height - 700 );
    g_SampleUI.SetSize( 220, 700 );

    // Set up 4x16F buffers for rendering main scene
    {
        D3D11_TEXTURE2D_DESC texDesc;
        texDesc.Width = pBackBufferSurfaceDesc->Width;
        texDesc.Height = pBackBufferSurfaceDesc->Height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = kMainSceneBufferFormat;
        texDesc.SampleDesc = pBackBufferSurfaceDesc->SampleDesc;
        texDesc.Usage = D3D11_USAGE_DEFAULT;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        texDesc.CPUAccessFlags = 0;
        texDesc.MiscFlags = 0;

        V_RETURN( pd3dDevice->CreateTexture2D(&texDesc, NULL, &g_p16FRenderTexture) );

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
        rtvDesc.Format = texDesc.Format;
        rtvDesc.ViewDimension = texDesc.SampleDesc.Count > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;

        V_RETURN( pd3dDevice->CreateRenderTargetView(g_p16FRenderTexture, &rtvDesc, &g_p16FRenderTextureRTV) );

        // If multi-sampling is enabled, we will need a separate non-AA resource to resolve to
        if(texDesc.SampleDesc.Count > 1)
        {
            D3D11_TEXTURE2D_DESC resolveDesc = texDesc;
            resolveDesc.SampleDesc.Count = 1;
            resolveDesc.SampleDesc.Quality = 0;

            V_RETURN( pd3dDevice->CreateTexture2D(&resolveDesc, NULL, &g_p16FResolveTexture) );
        }
        else
        {
            g_p16FResolveTexture = g_p16FRenderTexture;
            g_p16FResolveTexture->AddRef();
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC rvDesc;
        rvDesc.Format = rtvDesc.Format;
        rvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        rvDesc.Texture2D.MostDetailedMip = 0;
        rvDesc.Texture2D.MipLevels = texDesc.MipLevels;

        V_RETURN( pd3dDevice->CreateShaderResourceView(g_p16FResolveTexture, &rvDesc, &g_p16FRenderTextureSRV) );
    }

	// Get the new AA mode, and figure out if it has changed
	MSAAMode newMSAAMode = NoAA;
	V_RETURN(GetAAModeFromSampleDesc(pBackBufferSurfaceDesc->SampleDesc, newMSAAMode));
	if(NULL != g_pEffect && newMSAAMode != g_EffectAAMode)
	{
		// Release the effect (this will force a reload below)
		SAFE_RELEASE(g_pEffect);
	}
	g_EffectAAMode = newMSAAMode;

	// Load (or reload, if necessary) the global effect
	if(NULL == g_pEffect)
	{
		V_RETURN( LoadEffect(pd3dDevice) );
	}

    // Leave the device in a predictable state
    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    pd3dImmediateContext->OMSetRenderTargets( 1, &pRTV, g_pDepthStencilTextureDSV );

    g_LowResOffscreen.OnD3D11ResizedSwapChain(pd3dDevice, pSwapChain, pBackBufferSurfaceDesc, g_pDepthStencilTextureSRV);

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Shows/hides opacity shadows over time to show benefit of opacity mapping effect
//--------------------------------------------------------------------------------------
void UpdateOpacityShadowStrength(double fTime, float fElapsedTime)
{
	if(!g_CycleOpacityShadowIntensity)
	{
		g_LightSources.SetOpacityShadowStrength(1.f);
		g_ShowOpacityShadowCycledOffWarning = FALSE;
		return;
	}
		
	const double fCycleDuration = 10.f;
	const double fTransitionFraction = 0.05f;

	double fDummy;
	const double fCycleParam = modf(fTime/fCycleDuration, &fDummy);
	float oss;
	if(fCycleParam < (0.5f - fTransitionFraction))
	{
		oss = 1.f;
	}
	else if(fCycleParam < 0.5f)
	{
		oss = FLOAT(1. - (fCycleParam - (0.5 - fTransitionFraction))/fTransitionFraction);
	}
	else if(fCycleParam < (1.f - fTransitionFraction))
	{
		oss = 0.f;
	}
	else
	{
		oss = FLOAT((fCycleParam - (1. - fTransitionFraction))/fTransitionFraction);
	}

	g_ShowOpacityShadowCycledOffWarning = oss < 0.5f;
	g_LightSources.SetOpacityShadowStrength(oss);
}

//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
	float fElapsedSimulationTime = g_SimulationPaused ? 0.f : fElapsedTime;
	g_SimulationTime += fElapsedSimulationTime;

    g_ParticleSystem.PlumeSimTick(g_SimulationTime);

    // Camera update can optionally pan to centre on plume
    const D3DXVECTOR3 ParticlesCentre = 0.5f * (g_ParticleSystem.GetBoundsMin() + g_ParticleSystem.GetBoundsMax());
    g_Camera.FrameMove( fTime, fElapsedTime, ParticlesCentre );

    g_ParticleSystem.SortByDepth(*g_Camera.GetViewMatrix());

    // Save the state, if necessary
    if(g_SaveState)
    {
        g_ParticleSystem.SaveParticleState(fTime);
        g_Camera.SaveState(fTime);
        g_SaveState = FALSE;
    }

    // We drive the light transform from the camera's world matrix
    g_LightSources.UpdateControlledLight(*g_Camera.GetWorldMatrix());

    // Update the swing of the opaque object
    g_OpaqueObject.OnFrameMove(g_SimulationTime, fElapsedSimulationTime);

	// Update opacity shadow strength
	UpdateOpacityShadowStrength(g_SimulationTime, fElapsedSimulationTime);
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext )
{
    HRESULT hr;

    D3DXMATRIXA16 mWorldToView;
    D3DXMATRIXA16 mViewToWorld;
    D3DXMATRIXA16 mViewToProj;
    D3DXMATRIXA16 mProjToView;

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Reset the dynamic UP buffer
    CDrawUP::Instance().OnBeginFrame();

    // Update the D3D11 resources from the particle simulation
    g_ParticleSystem.UpdateD3D11Resources(pd3dImmediateContext);

    // Preserve the original RT
    ID3D11RenderTargetView* pOriginalRT = NULL;
    pd3dImmediateContext->OMGetRenderTargets(1, &pOriginalRT, NULL);

    D3D11_VIEWPORT mainSceneViewport;
    UINT NumViewports = 1;
    pd3dImmediateContext->RSGetViewports(&NumViewports, &mainSceneViewport);

    // We allow the bounds to be artificially inflated so that we can
    // see the effect of BV discontinuities more clearly, especially
    // when paused
    D3DXVECTOR3 ParticlesMin = g_ParticleSystem.GetBoundsMin();
    D3DXVECTOR3 ParticlesMax = g_ParticleSystem.GetBoundsMax();
    if(g_InflateBounds)
    {
        const FLOAT InlfationMultiplier = 1.1f;
        const D3DXVECTOR3 ParticlesMinToMax = ParticlesMax - ParticlesMin;
        const D3DXVECTOR3 ParticlesDelta = ParticlesMinToMax * (InlfationMultiplier - 1.f);
        ParticlesMin -= ParticlesDelta;
        ParticlesMax += ParticlesDelta;
    }

    // Get the projection & view matrix from the camera class
    mViewToProj = *g_Camera.GetProjMatrix();
    mWorldToView = *g_Camera.GetViewMatrix();
    D3DXMatrixInverse(&mViewToWorld, NULL, &mWorldToView);
    D3DXMatrixInverse(&mProjToView, NULL, &mViewToProj);
    const D3DXVECTOR3 eyePt = D3DXVECTOR3(mViewToWorld._41, mViewToWorld._42, mViewToWorld._43);

    // Clear the render target and the zbuffer 
    const D3DXVECTOR4 black(0.f,0.f,0.f,1.f);
    pd3dImmediateContext->ClearRenderTargetView(g_p16FRenderTextureRTV, (FLOAT*)&black);
    pd3dImmediateContext->ClearDepthStencilView(g_pDepthStencilTextureDSV, D3D11_CLEAR_DEPTH, 1.f, 0);

    // Render to opacity maps
    for(UINT light = 0; light != g_LightSources.GetNumLights(); ++light)
    {
        g_LightSources.BeginRenderToOpacityMaps(pd3dImmediateContext, light, ParticlesMin, ParticlesMax, mViewToWorld);

        if(g_ParticleSystem.GetNumActiveParticles() && !g_SkipOpacityRender)
        {
            V( g_ParticleSystem.DrawToOpacity(pd3dImmediateContext) );
        }

        g_LightSources.EndRenderToOpacityMaps(pd3dImmediateContext, light);
    }

    // Render opaque object to shadow maps
    D3DXVECTOR3 OpaqueMin;
    D3DXVECTOR3 OpaqueMax;
    g_OpaqueObject.GetBounds(OpaqueMin, OpaqueMax);
    for(UINT light = 0; light != g_LightSources.GetNumLights(); ++light)
    {
        g_LightSources.BeginRenderToShadowMap(pd3dImmediateContext, light, OpaqueMin, OpaqueMax, mViewToWorld);
        g_OpaqueObject.DrawToShadowMap(pd3dImmediateContext);
        g_LightSources.EndRenderToShadowMap(pd3dImmediateContext, light);
    }

    // Update camera variables for main scene rendering
    V( g_mWorldToView_EffectVar->SetMatrix((FLOAT*)&mWorldToView) );
    V( g_mViewToProj_EffectVar->SetMatrix((FLOAT*)&mViewToProj) );
    V( g_EyePoint_EffectVar->SetFloatVector((FLOAT*)&eyePt) );

    // Update light-related variables for main scene rendering
    g_LightSources.SetEffectParametersForSceneRendering(mViewToWorld);

    // Set up to render main scene
    pd3dImmediateContext->OMSetRenderTargets(1, &g_p16FRenderTextureRTV, g_pDepthStencilTextureDSV);
    pd3dImmediateContext->RSSetViewports(1, &mainSceneViewport);

    // Render floor + opaque object
    g_Floor.Draw(pd3dImmediateContext, g_MeteredOverdraw);
    g_OpaqueObject.DrawToScene(pd3dImmediateContext, g_MeteredOverdraw);

    if(g_ParticleSystem.GetNumActiveParticles())
    {
        if(g_ShowBounds)
        {
            const D3DXVECTOR4 RedLineColor(1,0,0,g_LocalBoundsAlpha);
            const D3DXVECTOR4 YellowLineColor(1,1,0,g_LightBoundsAlpha);

            // Render world bounds
            D3DXMATRIXA16 mIdentity;
            D3DXMatrixIdentity(&mIdentity);
            V( CLineDraw::Instance().DrawBounds(pd3dImmediateContext, mIdentity, D3DXVECTOR4(ParticlesMin,0.f), D3DXVECTOR4(ParticlesMax,0.f), RedLineColor) );

            // Render light bounds
            const D3DXVECTOR4 MinClip(-1.f,-1.f, 0.f, 1.f);
            const D3DXVECTOR4 MaxClip( 1.f, 1.f, 1.f, 1.f);
            V( CLineDraw::Instance().DrawBounds(pd3dImmediateContext, g_LightSources.GetControlledLightOpacityShadowClipToWorld(), MinClip, MaxClip, YellowLineColor) );
            V( CLineDraw::Instance().DrawBoundsArrows(pd3dImmediateContext, g_LightSources.GetControlledLightOpacityShadowClipToWorld(), MinClip, MaxClip, YellowLineColor) );
        }

        if(!g_SkipMainRender)
        {
			switch(g_FillReductionMethod)
			{
			case FRM_LowResOffscreen:
			case FRM_LowResOffscreen_Soft:
				{
					// Switch to read-only depth-stencil for low-res particle rendering (allows us to use depth-stencil as SRV)
					pd3dImmediateContext->OMSetRenderTargets(1, &g_p16FRenderTextureRTV, g_pDepthStencilTextureDSV);

					g_LowResOffscreen.BeginRenderToLowResOffscreen(pd3dImmediateContext);

					switch(g_FillReductionMethod)
					{
					case FRM_LowResOffscreen_Soft:
						V( g_ParticleSystem.DrawSoftToSceneLowRes(	pd3dImmediateContext,
																	g_LowResOffscreen.GetReducedDepthStencilSRV(),
																	g_Camera.GetNearClip(), g_Camera.GetFarClip()));
						break;
					default:
						V( g_ParticleSystem.DrawToSceneLowRes(pd3dImmediateContext) );
						break;
					}

					g_LowResOffscreen.EndRenderToLowResOffscreen(pd3dImmediateContext, g_Camera.GetNearClip(), g_Camera.GetFarClip(), g_MeteredOverdraw);
				}
				break;
			default:
				{
					// Render (remaining) particles
					V( g_ParticleSystem.DrawToScene(pd3dImmediateContext) );
				}
				break;
			}
        }

        // Ensure render targets are un-set
        ID3D11ShaderResourceView* NullResources[32];
        ZeroMemory(NullResources, sizeof(NullResources));
        pd3dImmediateContext->VSSetShaderResources(0, 32, NullResources);
        pd3dImmediateContext->DSSetShaderResources(0, 32, NullResources);
        pd3dImmediateContext->HSSetShaderResources(0, 32, NullResources);
        pd3dImmediateContext->GSSetShaderResources(0, 32, NullResources);
        pd3dImmediateContext->PSSetShaderResources(0, 32, NullResources);

        // Show opacity targets if necessary
        const UINT dstWH = UINT(min(mainSceneViewport.Width/8.f,mainSceneViewport.Height/8.f));
        UINT dstLeft = 0;
        if(g_ShowOpacityTargets)
        {
            const UINT strideH = dstWH + 2;

            RECT dstRect;
            dstRect.bottom = UINT(mainSceneViewport.Height);
            dstRect.top = UINT(dstRect.bottom - dstWH);
            dstRect.left = dstLeft;
            dstRect.right = UINT(dstRect.left + dstWH);

            V( CScreenAlignedQuad::Instance().DrawQuadTranslucent(pd3dImmediateContext, g_LightSources.GetControlledLightOpacityMap(3), g_LightSources.GetOpacityMapMipLevel(), &dstRect) );

            dstRect.bottom -= strideH;
            dstRect.top -= strideH;
            V( CScreenAlignedQuad::Instance().DrawQuadTranslucent(pd3dImmediateContext, g_LightSources.GetControlledLightOpacityMap(2), g_LightSources.GetOpacityMapMipLevel(), &dstRect) );

            dstRect.bottom -= strideH;
            dstRect.top -= strideH;
            V( CScreenAlignedQuad::Instance().DrawQuadTranslucent(pd3dImmediateContext, g_LightSources.GetControlledLightOpacityMap(1), g_LightSources.GetOpacityMapMipLevel(), &dstRect) );

            dstRect.bottom -= strideH;
            dstRect.top -= strideH;
            V( CScreenAlignedQuad::Instance().DrawQuadTranslucent(pd3dImmediateContext, g_LightSources.GetControlledLightOpacityMap(0), g_LightSources.GetOpacityMapMipLevel(), &dstRect) );

            dstLeft = dstRect.right + 20;
        }
    }

    // Resolve AA, if necessary
    if(g_p16FResolveTexture != g_p16FRenderTexture)
    {
        pd3dImmediateContext->ResolveSubresource(g_p16FResolveTexture, 0, g_p16FRenderTexture, 0, kMainSceneBufferFormat);
    }

    // Restore main RT & copy results across
    pd3dImmediateContext->OMSetRenderTargets(1, &pOriginalRT, g_pDepthStencilTextureDSV);
    pd3dImmediateContext->RSSetViewports(1, &mainSceneViewport);
	V( CScreenAlignedQuad::Instance().DrawQuad(pd3dImmediateContext, g_p16FRenderTextureSRV, 0, NULL, NULL) );

	if (g_ShowUI)
    {
		DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" ); // These events are to help PIX identify what the code is doing
		RenderText();
		V( g_HUD.OnRender( fElapsedTime ) );
		V( g_SampleUI.OnRender( fElapsedTime ) );
		DXUT_EndPerfEvent();
	}

    SAFE_RELEASE(pOriginalRT);
}

//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext )
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass messages to settings dialog if its active
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all remaining windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}

//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if(bKeyDown)
    {
        switch(nChar)
        {
        case VK_F1:
            g_ShowHelp = !g_ShowHelp;
            break;
        case 'o':
        case 'O':
            g_SkipOpacityRender = !g_SkipOpacityRender;
            break;
        case 'r':
        case 'R':
            g_SkipMainRender = !g_SkipMainRender;
            break;
        case 'l':
        case 'L':
            g_LightSources.CycleControlledLight(*g_Camera.GetWorldMatrix());
            break;
		case 'u':
        case 'U':
            g_ShowUI = !g_ShowUI;
            break;
		case 'c':
		case 'C':
			g_Camera.ToggleCameraOrbit();
			break;
        }
    }
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:
            g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;
        case IDC_LIGHTINGMODE:
			g_ParticleSystem.SetLightingMode((CParticleSystem::LightingMode)g_SampleUI.GetComboBox(IDC_LIGHTINGMODE)->GetSelectedIndex()); break;
        case IDC_TOGGLEWIREFRAME:
            g_ParticleSystem.SetWireframe(!g_ParticleSystem.GetWireframe()); break;
        case IDC_TOGGLEBOUNDS:
            g_ShowBounds = !g_ShowBounds; break;
        case IDC_TOGGLEINFLATEBOUNDS:
            g_InflateBounds = !g_InflateBounds; break;
        case IDC_TOGGLESHOWOPACITYTARGETS:
            g_ShowOpacityTargets = !g_ShowOpacityTargets; break;
		case IDC_TOGGLE_CYCLEOPACITYSHADOWSTRENGTH:
			g_CycleOpacityShadowIntensity = !g_CycleOpacityShadowIntensity; break;
        case IDC_OPACITYMULTIPLIER_SLIDER:
            g_LightSources.SetOpacityMultiplier(FLOAT(g_SampleUI.GetSlider(IDC_OPACITYMULTIPLIER_SLIDER)->GetValue())/100.f);
            break;
        case IDC_OPACITY_SIZE_SLIDER:
            g_LightSources.SetOpacityMapMipLevel(DXUTGetD3D11Device(), 8-g_SampleUI.GetSlider(IDC_OPACITY_SIZE_SLIDER)->GetValue());
            UpdateOpacitySizeLabel();
            break;
        case IDC_FILLREDUCTIONMETHOD:
            g_FillReductionMethod = g_SampleUI.GetComboBox(IDC_FILLREDUCTIONMETHOD)->GetSelectedIndex();
            UpdateFillReductionControls();
            break;
        case IDC_TOGGLEMETEREDOVERDRAW:
            g_MeteredOverdraw = !g_MeteredOverdraw;
            g_ParticleSystem.SetMeteredOverdraw(g_MeteredOverdraw);
            break;
		case IDC_LOWRES_UPSCALE_DEPTH_THRESHOLD_SLIDER:
			g_LowResOffscreen.SetUpsampleFilterDepthThreshold(powf(10.f,6.f * (0.01f * FLOAT(g_SampleUI.GetSlider(IDC_LOWRES_UPSCALE_DEPTH_THRESHOLD_SLIDER)->GetValue()) - 1.f)));
			break;

		case IDC_SOFT_PARTICLE_FADE_DISTANCE_SLIDER:
			g_ParticleSystem.SetSoftParticleFadeDistance(0.01f * FLOAT(g_SampleUI.GetSlider(IDC_SOFT_PARTICLE_FADE_DISTANCE_SLIDER)->GetValue()));
			break;


		case IDC_TOGGLESIMULATION:
			g_SimulationPaused = !g_SimulationPaused;
			break;
    }
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();

    SAFE_RELEASE( g_p16FRenderTextureRTV );
    SAFE_RELEASE( g_p16FRenderTexture );
    SAFE_RELEASE( g_p16FRenderTextureSRV);
    SAFE_RELEASE( g_p16FResolveTexture);

    g_LowResOffscreen.OnD3D11ReleasingSwapChain();

    SAFE_RELEASE(g_pDepthStencilTexture);
    SAFE_RELEASE(g_pDepthStencilTextureDSV);
    SAFE_RELEASE(g_pDepthStencilTextureSRV);
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    SAFE_DELETE(g_pTxtHelper);

    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_SettingsDlg.OnD3D11DestroyDevice();

    CScreenAlignedQuad::Instance().OnD3D11DestroyDevice();
    g_ParticleSystem.OnD3D11DestroyDevice();
    g_LightSources.OnD3D11DestroyDevice();
    CDrawUP::Instance().OnD3D11DestroyDevice();
    g_Floor.OnD3D11DestroyDevice();
    g_LowResOffscreen.OnD3D11DestroyDevice();
    CLineDraw::Instance().OnD3D11DestroyDevice();
    g_OpaqueObject.OnD3D11DestroyDevice();

    SAFE_RELEASE( g_pEffect );
}
