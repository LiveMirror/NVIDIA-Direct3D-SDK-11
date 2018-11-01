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
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"
#include "resource.h"
#include "BzrFile_stencil.h"
#include "GPUPatchUtils.h"
#include "DynamicMesh.h" 
#include "StaticMesh.h"  
#include "typedef.h"
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>

using namespace std;

//--------------------------------------------------------------------------------------
// Macros
//--------------------------------------------------------------------------------------
#define MAX_DIVS 32
#define DEF_DIVS 8
#define DEPTH_RES 1024
#define GRIDLENGTH 20
#define GRIDWIDTH 20
#define FRAMENUM 100


// Control variables
int                                 g_iSubdivs = DEF_DIVS;                         // Startup subdivisions per edge
bool                                g_bFlatShading = false;  
bool                                g_bDrawWires = false;                          // Draw the mesh with wireframe overlay
bool                                g_bDrawGregoryACC = false;                     // Show gregory patches
bool                                g_bAnimation = false;                          // Render animation sequence
int                                 g_iAdaptive = 0; 
int                                 g_lightIntensity = 30; 
int                                 g_iAnimationSpeed = 5; 
static int                          g_frame = 0;

// Cmd line params
typedef struct _CmdLineParams
{
    D3D_DRIVER_TYPE DriverType;
    unsigned int uWidth;
    unsigned int uHeight;
    bool bCapture;
    WCHAR strCaptureFilename[256];
    int iExitFrame;
    bool bRenderHUD;
}CmdLineParams;
static CmdLineParams g_CmdLineParams;

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
// DXUT resources
CDXUTDialogResourceManager          g_DialogResourceManager;                      // manager for shared resources of dialogs
CModelViewerCamera                  g_Camera;                                     // A model viewing camera
CModelViewerCamera                  g_Light;                   
CD3DSettingsDlg                     g_D3DSettingsDlg;                             // Device settings dialog
CDXUTDialog                         g_HUD;                                        // manages the 3D   
CDXUTDialog                         g_SampleUI;                                   // dialog for sample specific controls

UINT								g_iBindPerPatch = 0;
UINT								g_iBindPerFrame = 1;
UINT								g_iBindPerDraw = 2;
UINT								g_iBindPerMesh = 3;

// Geometry
ID3D11InputLayout*                  g_pConstructionLayout                         = NULL;
ID3D11InputLayout*                  g_pVertexLayout                               = NULL;
ID3D11Buffer*                       pPlaneIndexBuffer                             = NULL;
ID3D11Buffer*                       pPlaneVertexBuffer                            = NULL;
IMesh*                              dm1                                           = NULL;
IMesh*                              sm1                                           = NULL;

// Textures used in the shaders
ID3D11ShaderResourceView*           pHeightMapTextureSRV                          = NULL;  
ID3D11ShaderResourceView*           pNormalMapTextureSRV                          = NULL; 
ID3D11ShaderResourceView*           pFloorHeightTextureSRV                        = NULL; 
ID3D11Texture2D*                    pTexRender11                                  = NULL;       
ID3D11ShaderResourceView*           pTexRenderRV11                                = NULL;  
ID3D11DepthStencilView*             pTexRenderDS11                                = NULL;
ID3D11ShaderResourceView*           pFloorTextureSRV                              = NULL;

// Resources
CDXUTTextHelper*                    g_pTxtHelper                                  = NULL;

// Shaders
ID3D11VertexShader*					g_pSkinningVS                                 = NULL;
ID3D11VertexShader*					g_pPlaneVS                                    = NULL;
ID3D11VertexShader*					g_pInputMeshVS                                = NULL;
ID3D11VertexShader*					g_pShadowmapVS                                = NULL;
ID3D11HullShader*					g_pSubDToBezierHS                             = NULL;
ID3D11HullShader*					g_pSubDToGregoryHS                            = NULL;
ID3D11HullShader*					g_pStaticObjectGregoryHS                      = NULL;
ID3D11HullShader*					g_pStaticObjectGregoryHS_Distance             = NULL;
ID3D11HullShader*					g_pSubDToBezierHS_Distance                    = NULL;
ID3D11HullShader*					g_pSubDToGregoryHS_Distance                   = NULL;
ID3D11HullShader*					g_pPlaneHS                                    = NULL;
ID3D11HullShader*					g_pPlaneHS_Distance                           = NULL;
ID3D11DomainShader*					g_pSurfaceEvalDS                              = NULL;
ID3D11DomainShader*					g_pGregoryEvalDS                              = NULL;
ID3D11PixelShader*					g_pPhongShadingPS                             = NULL;
ID3D11PixelShader*					g_pPhongShadingPS_I                           = NULL;
ID3D11PixelShader*					g_pFlatShadingPS                              = NULL;
ID3D11PixelShader*					g_pFlatShadingPS_I                            = NULL;
ID3D11PixelShader*					g_pSolidColorPS                               = NULL;
ID3D11PixelShader*					g_pShadowmapPS                                = NULL;
ID3D11PixelShader*					g_pPlanePS                                    = NULL;
ID3D11PixelShader*					g_pInputMeshPS                                = NULL;
ID3D11VertexShader*					g_pStaticObjectVS                             = NULL;
ID3D11HullShader*					g_pStaticObjectHS                             = NULL;
ID3D11HullShader*					g_pStaticObjectHS_Distance                    = NULL;
ID3D11DomainShader*					g_pStaticObjectDS                             = NULL;
ID3D11DomainShader*					g_pStaticObjectGregoryDS                      = NULL;
ID3D11DomainShader*					g_pPlaneDS                                    = NULL;


// States
ID3D11RasterizerState*              g_pRasterizerStateSolid                       = NULL;
ID3D11RasterizerState*              g_pRasterizerStateWireframe                   = NULL;
ID3D11BlendState*                   g_pBlendStateNoBlend                          = NULL;
// Samplers
ID3D11SamplerState*                 g_pSamplePoint                                = NULL;
ID3D11SamplerState*                 g_pSampleLinear                               = NULL;

// constant buffers
ID3D11Buffer*						g_pcbPerFrame                                 = NULL;
ID3D11Buffer*						g_pcbPerDraw                                  = NULL;
   

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN      1
#define IDC_TOGGLEREF             3
#define IDC_CHANGEDEVICE          4
#define IDC_PATCH_SUBDIVS         5
#define IDC_PATCH_SUBDIVS_STATIC  6
#define IDC_BUMP_HEIGHT	          7
#define IDC_BUMP_HEIGHT_STATIC    8
#define IDC_TOGGLE_LINES          9
#define IDC_TOGGLE_FLAT           12
#define IDC_DIST_ATTEN            14
#define IDC_TOGGLE_ANIMATION      15
#define IDC_TOGGLE_INPUTMESH      18
#define IDC_TOGGLE_GREGORY        19
#define IDC_ADAPTIVE              20
#define IDC_COMBOBOX_ADAPTIVE     21
#define IDC_LIGHTINTENSITY_STATIC 22
#define IDC_LIGHTINTENSITY	      23

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext );
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext );
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );

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
void InitApp();
void RenderText();
HRESULT CreateConstantBuffers( ID3D11Device* pd3dDevice );
bool FileExists( WCHAR* pFileName );
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, 
                               LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D10_SHADER_MACRO* pDefines );
HRESULT initResourcesForAnimatedObjects(ID3D11Device* pd3dDevice);
HRESULT initResourcesForStaticObjects(ID3D11Device* pd3dDevice);
void FillGrid_Indexed_WithTangentSpace( ID3D11Device* pd3dDevice, DWORD dwWidth, DWORD dwLength, 
                                        float fGridSizeX, float fGridSizeZ, 
                                        ID3D11Buffer** lplpVB, ID3D11Buffer** lplpIB );


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    HRESULT hr;
    V_RETURN(DXUTSetMediaSearchPath(L"..\\Source\\GregoryACC11"));
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device (either D3D10 or D3D11) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set DXUT callbacks
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackFrameMove( OnFrameMove );

    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    InitApp();
    //DXUTInit( true, true, L"-forceref" ); // Parse the command line, show msgboxes on error, and an extra cmd line param to force REF for now
	DXUTInit( true, true );
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"GregoryACC11" );
	DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, g_CmdLineParams.uWidth, g_CmdLineParams.uHeight );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{

    // Initialize dialogs
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );
	g_SampleUI.GetFont( 0 );


    g_HUD.SetCallback( OnGUIEvent ); int iY = 20;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 22, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 22, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent ); iY = 5;

    WCHAR sz[100];
    iY -= 80;
	int iSX = 100;
	swprintf_s( sz, L"Static TFs: %d", g_iSubdivs );
    g_SampleUI.AddStatic( IDC_PATCH_SUBDIVS_STATIC, sz, 20, iY += 26, iSX, 22 );
    g_SampleUI.AddSlider( IDC_PATCH_SUBDIVS, 50, iY += 24, iSX, 22, 1, MAX_DIVS - 1, g_iSubdivs );
	iY += 10;
	swprintf_s( sz, L"Adjust spot light: %d", g_lightIntensity );
	g_SampleUI.AddStatic( IDC_LIGHTINTENSITY_STATIC, sz, 20, iY += 26, iSX, 22 );
	g_SampleUI.AddSlider( IDC_LIGHTINTENSITY, 50, iY += 24, iSX, 22, 5, MAX_DIVS - 1, g_lightIntensity );
	iY += 30;
	g_SampleUI.AddStatic( IDC_ADAPTIVE, L"Adaptive TFs:", 0, iY, 55, 24 );
	CDXUTComboBox *pCombo;
	g_SampleUI.AddComboBox( IDC_COMBOBOX_ADAPTIVE, 0, iY += 25, iSX+50, 24, 0, true, &pCombo );
    if( pCombo )
    {
        pCombo->SetDropHeight( 45 );
		pCombo->AddItem( L"Turn off", NULL );
        pCombo->AddItem( L"Turn on", NULL );
        pCombo->SetSelectedByIndex( 0 );
    }

    g_SampleUI.AddCheckBox( IDC_TOGGLE_ANIMATION, L"Animation", 20, iY += 26, iSX, 22, g_bAnimation );

    g_SampleUI.AddCheckBox( IDC_TOGGLE_LINES, L"Toggle Wires", 20, iY += 26, iSX, 22, g_bDrawWires );

	g_SampleUI.AddCheckBox( IDC_TOGGLE_GREGORY, L"Show Gregory Patches", 20, iY += 26, iSX, 22, g_bDrawGregoryACC );

    g_SampleUI.AddCheckBox( IDC_TOGGLE_FLAT, L"Flat Shading", 20, iY += 26, iSX, 22, g_bFlatShading );


	D3DXVECTOR3 vecEye( -100.0f, 100.0f, 100.0f );
	D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );
	g_Camera.SetViewParams( &vecEye, &vecAt );
    D3DXVECTOR3 vecLight( -20.0f, 240.0f, 0.0f );
	g_Light.SetViewParams( &vecLight, &vecAt );

}


//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{

	assert( pDeviceSettings->ver == DXUT_D3D11_DEVICE );

    // For the first device created if its a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
	if( s_bFirstTime )
    {
        s_bFirstTime = false;

        // Set driver type based on cmd line / program default
        pDeviceSettings->d3d11.DriverType = g_CmdLineParams.DriverType;

        // Disable vsync
        pDeviceSettings->d3d11.SyncInterval = 0;

        if( ( DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF ) ||
            ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
            pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }
    }

    return true;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );
}


//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    g_pTxtHelper->End();
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
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.MsgProc( hWnd, uMsg, wParam, lParam );
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
	static int iCaptureNumber = 0;
    #define VK_C (67)
    #define VK_G (71)

    if( bKeyDown )
    {
        switch( nChar )
        {
            case VK_C:    
                swprintf_s( g_CmdLineParams.strCaptureFilename, L"FrameCapture%d.bmp", iCaptureNumber );
                iCaptureNumber++;
                break;

            case VK_G:
                g_CmdLineParams.bRenderHUD = !g_CmdLineParams.bRenderHUD;
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
        // Standard DXUT controls
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); 
			break;
        case IDC_TOGGLEREF:
            DXUTToggleREF(); 
			break;
        case IDC_CHANGEDEVICE:
			g_D3DSettingsDlg.IsActive();
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive()); 
			break;
        // Custom app controls
        case IDC_PATCH_SUBDIVS:
        {
            g_iSubdivs = g_SampleUI.GetSlider( IDC_PATCH_SUBDIVS )->GetValue();

            WCHAR sz[100];
            swprintf_s( sz, L"Static TF: %d", g_iSubdivs );
            g_SampleUI.GetStatic( IDC_PATCH_SUBDIVS_STATIC )->SetText( sz );
        }
            break;
		case IDC_LIGHTINTENSITY:
		{
				g_lightIntensity = g_SampleUI.GetSlider( IDC_LIGHTINTENSITY )->GetValue();

				WCHAR sz[100];
				swprintf_s( sz, L"Spotlight Intensity: %d", g_lightIntensity );
				g_SampleUI.GetStatic( IDC_LIGHTINTENSITY_STATIC )->SetText( sz );
		}
			break;
	    case IDC_COMBOBOX_ADAPTIVE:
        {
			g_iAdaptive = (ADAPTIVETESSELLATION_METRIC)((CDXUTComboBox*)pControl)->GetSelectedIndex();
			g_SampleUI.GetSlider(IDC_PATCH_SUBDIVS)->SetEnabled(!g_iAdaptive);
	    }
            break;
        case IDC_TOGGLE_LINES:
        {
            g_bDrawWires = g_SampleUI.GetCheckBox( IDC_TOGGLE_LINES )->GetChecked();
			g_SampleUI.GetCheckBox( IDC_TOGGLE_GREGORY)->SetEnabled(!g_bDrawWires);
			g_SampleUI.GetCheckBox( IDC_TOGGLE_FLAT)->SetEnabled(!g_bDrawWires);
        }
            break;
	    case IDC_TOGGLE_FLAT:
        {
            g_bFlatShading = g_SampleUI.GetCheckBox( IDC_TOGGLE_FLAT )->GetChecked();
        }
            break;
	    case IDC_TOGGLE_GREGORY:
        {
            g_bDrawGregoryACC = g_SampleUI.GetCheckBox( IDC_TOGGLE_GREGORY )->GetChecked();
        }
            break;
	    case IDC_TOGGLE_ANIMATION:
        {
            g_bAnimation = g_SampleUI.GetCheckBox( IDC_TOGGLE_ANIMATION )->GetChecked();
        }
            break;
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
HRESULT CreateConstantBuffers( ID3D11Device* pd3dDevice )
{
    HRESULT hr = S_OK;

	D3D11_BUFFER_DESC Desc;
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.MiscFlags = 0;

    Desc.ByteWidth = sizeof( CB_PER_FRAME_CONSTANTS );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbPerFrame ) );

	Desc.ByteWidth = sizeof( CB_PER_DRAW_CONSTANTS );
	V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbPerDraw ) );

	return hr;
}

//--------------------------------------------------------------------------------------
// Use this until D3DX11 comes online and we get some compilation helpers
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    HRESULT hr;

    WCHAR str[1024];

    V_RETURN( DXUTFindDXSDKMediaFileCch( str, 1024, szFileName ) );

    ID3DBlob* pErrorBlob = NULL;
    hr = D3DX11CompileFromFile( str, NULL, NULL, szEntryPoint, szShaderModel, D3D10_SHADER_ENABLE_STRICTNESS, NULL, NULL, ppBlobOut, &pErrorBlob, NULL );
    if ( FAILED(hr) )
    {
        if ( pErrorBlob )
            OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );

        SAFE_RELEASE( pErrorBlob );


        return hr;
    }    

    SAFE_RELEASE( pErrorBlob );

    return hr;
}

//--------------------------------------------------------------------------------------
// Create any D3D10 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;
	    
	g_CmdLineParams.DriverType = D3D_DRIVER_TYPE_HARDWARE;
    g_CmdLineParams.uWidth = 1024;
    g_CmdLineParams.uHeight = 768;
    g_CmdLineParams.bCapture = false;
    swprintf_s( g_CmdLineParams.strCaptureFilename, L"FrameCapture.bmp" );
    g_CmdLineParams.iExitFrame = -1;
    g_CmdLineParams.bRenderHUD = true;

    static bool bFirstOnCreateDevice = true;

    // Warn the user that in order to support DX11, a non-hardware device has been created, continue or quit?
    if ( DXUTGetDeviceSettings().d3d11.DriverType != D3D_DRIVER_TYPE_HARDWARE && bFirstOnCreateDevice )
    {
        if ( MessageBox( 0, L"No Direct3D 11 hardware detected. "\
                            L"In order to continue, a non-hardware device has been created, "\
                            L"it will be very slow, continue?", L"Warning", MB_ICONEXCLAMATION | MB_YESNO ) != IDYES )
            return E_FAIL;
    }
    
    bFirstOnCreateDevice = false;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    // Create shaders
    ID3DBlob* pBlob = NULL;
    D3D10_SHADER_MACRO ShaderMacros[2];
	ShaderMacros[0].Definition = "1";
	ShaderMacros[1].Definition = "1";

	WCHAR * shaderFileName = L"GregoryAccTessellation11\\GregoryACC11_stencil.hlsl";
	V_RETURN( CompileShaderFromFile( shaderFileName, "SkinningVS",	   "vs_5_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pSkinningVS ) );
	const D3D11_INPUT_ELEMENT_DESC ConstructionVertexFormatElements2[] = 
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};
	hr= pd3dDevice->CreateInputLayout( ConstructionVertexFormatElements2, ARRAYSIZE( ConstructionVertexFormatElements2 ), pBlob->GetBufferPointer(),
		pBlob->GetBufferSize(), &g_pConstructionLayout ) ;

	V_RETURN( CompileShaderFromFile( shaderFileName, "PlaneVS",	   "vs_5_0", &pBlob ) );
	V_RETURN( pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pPlaneVS ) );

    ShaderMacros[0].Name = "USE_DISTANCE_METRIC";
    ShaderMacros[1].Name = NULL;

	V_RETURN( CompileShaderFromFile( shaderFileName, "PlaneHS", "hs_5_0", &pBlob ) );
	V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pPlaneHS ) );
	V_RETURN( CompileShaderFromFile( shaderFileName, "PlaneHS", "hs_5_0", &pBlob, ShaderMacros ) );
	V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pPlaneHS_Distance ) );

    V_RETURN( CompileShaderFromFile( shaderFileName, "SubDToBezierHS", "hs_5_0", &pBlob) ); 
	V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pSubDToBezierHS ) );
    V_RETURN( CompileShaderFromFile( shaderFileName, "SubDToGregoryHS", "hs_5_0", &pBlob ) );
	V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pSubDToGregoryHS ) );
    V_RETURN( CompileShaderFromFile( shaderFileName, "SubDToBezierHS", "hs_5_0", &pBlob, ShaderMacros ) ); 
	V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pSubDToBezierHS_Distance ) );
    V_RETURN( CompileShaderFromFile( shaderFileName, "SubDToGregoryHS", "hs_5_0", &pBlob, ShaderMacros ) );
	V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pSubDToGregoryHS_Distance ) );

    V_RETURN( CompileShaderFromFile( shaderFileName, "staticObjectHS",	   "hs_5_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pStaticObjectHS ) );
	V_RETURN( CompileShaderFromFile( shaderFileName, "staticObjectGregoryHS",	   "hs_5_0", &pBlob ) );
	V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pStaticObjectGregoryHS ) );
    V_RETURN( CompileShaderFromFile( shaderFileName, "staticObjectHS",	   "hs_5_0", &pBlob, ShaderMacros ) );
    V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pStaticObjectHS_Distance ) );
	V_RETURN( CompileShaderFromFile( shaderFileName, "staticObjectGregoryHS",	   "hs_5_0", &pBlob, ShaderMacros ) );
	V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pStaticObjectGregoryHS_Distance ) );


	V_RETURN( CompileShaderFromFile( shaderFileName, "SurfaceEvalDS",   "ds_5_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreateDomainShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pSurfaceEvalDS ) );
    V_RETURN( CompileShaderFromFile( shaderFileName, "GregoryEvalDS",   "ds_5_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreateDomainShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pGregoryEvalDS ) );
    V_RETURN( CompileShaderFromFile( shaderFileName, "PlaneDS",   "ds_5_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreateDomainShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pPlaneDS ) );

	V_RETURN( CompileShaderFromFile( shaderFileName, "PhongShadingPS",   "ps_5_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pPhongShadingPS ) );
	V_RETURN( CompileShaderFromFile( shaderFileName, "PlanePS",	   "ps_5_0", &pBlob ) );
	V_RETURN( pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pPlanePS ) );
	V_RETURN( CompileShaderFromFile( shaderFileName, "SolidColorPS",   "ps_5_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pSolidColorPS ) );
	V_RETURN( CompileShaderFromFile( shaderFileName, "PlaneVS",	   "vs_5_0", &pBlob ) );
	V_RETURN( CompileShaderFromFile( shaderFileName, "ShadowMapVertexShader",	   "vs_5_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pShadowmapVS ) );
	V_RETURN( CompileShaderFromFile( shaderFileName, "ShadowMapPixelShader",	   "ps_5_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pShadowmapPS ) );
	V_RETURN( CompileShaderFromFile( shaderFileName, "InputMeshVertexShader",	   "vs_5_0", &pBlob ) );
	V_RETURN( pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pInputMeshVS ) );
	V_RETURN( CompileShaderFromFile( shaderFileName, "InputMeshPixelShader",	   "ps_5_0", &pBlob ) );
	V_RETURN( pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pInputMeshPS ) );
	V_RETURN( CompileShaderFromFile( shaderFileName, "PhongShadingPS_I",   "ps_5_0", &pBlob ) );
	V_RETURN( pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pPhongShadingPS_I ) );
	V_RETURN( CompileShaderFromFile( shaderFileName, "FlatShadingPS",   "ps_5_0", &pBlob ) );
	V_RETURN( pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pFlatShadingPS ) );
	V_RETURN( CompileShaderFromFile( shaderFileName, "FlatShadingPS_I",   "ps_5_0", &pBlob ) );
	V_RETURN( pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pFlatShadingPS_I ) );

	V_RETURN( CompileShaderFromFile( shaderFileName, "staticObjectVS",	   "vs_5_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pStaticObjectVS ) );

	V_RETURN( CompileShaderFromFile( shaderFileName, "staticObjectDS",	   "ds_5_0", &pBlob ) );
    V_RETURN( pd3dDevice->CreateDomainShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pStaticObjectDS ) );
	V_RETURN( CompileShaderFromFile( shaderFileName, "staticObjectGregoryDS",	   "ds_5_0", &pBlob ) );
	V_RETURN( pd3dDevice->CreateDomainShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pStaticObjectGregoryDS ) );
    

	SAFE_RELEASE( pBlob );
    
    // Create constant buffers
    V_RETURN( CreateConstantBuffers( pd3dDevice ) );


	initResourcesForAnimatedObjects(pd3dDevice);

	initResourcesForStaticObjects(pd3dDevice);

	// create vertex and index buffer for a plane
	unsigned int length =GRIDLENGTH; unsigned int width =GRIDWIDTH;
	float fGridSizeZ,fGridSizeX;
	fGridSizeZ=fGridSizeX= 300.0f;
	int   nNumVertex = ( width + 1 ) * ( length + 1 );
    int   nNumIndex = 3 * 2 * width * length;
    float   fStepX = fGridSizeX / width;
    float   fStepZ = fGridSizeZ / length;
	Vertex *  pVerts = new Vertex[nNumVertex];
	int v=0;
	for ( unsigned int i=0; i<=length; ++i )
    {
        for ( unsigned int j=0; j<=width; ++j )
        {
            pVerts[v].m_Position = float3(-fGridSizeX/2.0f + j*fStepX, -17.0f, fGridSizeZ/2.0f - i*fStepZ);
            pVerts[v].m_Texcoord = float2 (0.0f + ( (float)j / width  ), 1- 0.0f - ( (float)i / length ));
            pVerts[v].m_Bitangent = float3(0.0f, 0.0f, -1.0f);
			pVerts[v].m_Tangent = float3(1.0f, 0.0f, 0.0f);

            v++;
        }
    }
	assert (v==nNumVertex);

	CreateABuffer(pd3dDevice, D3D11_BIND_VERTEX_BUFFER,  nNumVertex*sizeof(Vertex), 
		0, D3D11_USAGE_DEFAULT,  pPlaneVertexBuffer, (void *)& pVerts[0]);

	unsigned int * pIndex = new unsigned int [nNumIndex];
    // Fill index buffer
	v=0;
    for ( unsigned int i=0; i<length; ++i )
    {
        for ( unsigned int j=0; j<width; ++j )
        {
            pIndex[v++] = (  i    * (width+1) + j     );
            pIndex[v++] = (  i    * (width+1) + j + 1 );
            pIndex[v++] = ( (i+1) * (width+1) + j     );

            pIndex[v++] = ( (i+1) * (width+1) + j     );
            pIndex[v++] = (  i    * (width+1) + j + 1 );
            pIndex[v++] = ( (i+1) * (width+1) + j + 1 );

        }
    }

	CreateABuffer(pd3dDevice, D3D11_BIND_INDEX_BUFFER, nNumIndex*sizeof(unsigned int), 0, D3D11_USAGE_DEFAULT,  
		pPlaneIndexBuffer, (void *)&pIndex[0]);
	delete [] pIndex;
		
	//HRESULT hr = S_OK;
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"GregoryAccTessellation11\\monsterfrog-d.bmp" ));	
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &pHeightMapTextureSRV, NULL ));
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"GregoryAccTessellation11\\monsterfrog-n.bmp" ));	
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &pNormalMapTextureSRV, NULL ));
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"GregoryAccTessellation11\\stones.bmp" ));	
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &pFloorTextureSRV, NULL ));
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"GregoryAccTessellation11\\stones_NM_height.dds" ));	
    V_RETURN( D3DX11CreateShaderResourceViewFromFile( pd3dDevice, str, NULL, NULL, &pFloorHeightTextureSRV, NULL ));
    
	D3D11_SAMPLER_DESC SamDescShad = 
        {
            D3D11_FILTER_ANISOTROPIC,// D3D11_FILTER Filter;
            D3D11_TEXTURE_ADDRESS_CLAMP, //D3D11_TEXTURE_ADDRESS_MODE AddressU;
            D3D11_TEXTURE_ADDRESS_CLAMP, //D3D11_TEXTURE_ADDRESS_MODE AddressV;
            D3D11_TEXTURE_ADDRESS_BORDER, //D3D11_TEXTURE_ADDRESS_MODE AddressW;
            0,//FLOAT MipLODBias;
            16,//UINT MaxAnisotropy;
            D3D11_COMPARISON_NEVER , //D3D11_COMPARISON_FUNC ComparisonFunc;
            0.0,0.0,0.0,0.0,//FLOAT BorderColor[ 4 ];
            0,//FLOAT MinLOD;
            0//FLOAT MaxLOD;   
        };
        SamDescShad.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        SamDescShad.MaxAnisotropy = 2;
		V_RETURN( pd3dDevice->CreateSamplerState( &SamDescShad, &g_pSamplePoint ) );
    // Linear
	D3D11_SAMPLER_DESC SamDesc = {};
    SamDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    SamDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	SamDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    V_RETURN( pd3dDevice->CreateSamplerState( &SamDesc, &g_pSampleLinear ) );

    // Create solid and wireframe rasterizer state objects
    D3D11_RASTERIZER_DESC RasterDesc;
    ZeroMemory( &RasterDesc, sizeof(D3D11_RASTERIZER_DESC) );
    RasterDesc.FillMode = D3D11_FILL_SOLID;
    RasterDesc.CullMode = D3D11_CULL_NONE;  //be able to view back face
    //RasterDesc.DepthClipEnable = TRUE;

    //RasterDesc.CullMode = D3D11_CULL_BACK;
    RasterDesc.FrontCounterClockwise = FALSE;
    RasterDesc.DepthBias = 0;
    RasterDesc.DepthBiasClamp = 0.0f;
    RasterDesc.SlopeScaledDepthBias = 0.0f;
    RasterDesc.DepthClipEnable = TRUE;
    RasterDesc.ScissorEnable = FALSE;
    RasterDesc.MultisampleEnable = FALSE;
    RasterDesc.AntialiasedLineEnable = FALSE;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterDesc, &g_pRasterizerStateSolid ) );

    RasterDesc.FillMode = D3D11_FILL_WIREFRAME;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterDesc, &g_pRasterizerStateWireframe ) );

	// Create a blend state to disable alpha blending
    D3D11_BLEND_DESC BlendState;
    ZeroMemory(&BlendState, sizeof(D3D11_BLEND_DESC));
    BlendState.IndependentBlendEnable =                 FALSE;
    BlendState.RenderTarget[0].BlendEnable =            FALSE;
    BlendState.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = pd3dDevice->CreateBlendState( &BlendState, &g_pBlendStateNoBlend );


    return S_OK;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                          const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.05f, 4000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );
    g_Light.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.05f, 4000.0f );
    g_Light.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 300 );
    g_SampleUI.SetSize( 170, 300 );

	D3D11_TEXTURE2D_DESC Desc;
	ZeroMemory( &Desc, sizeof( D3D11_TEXTURE2D_DESC ) );
	Desc.ArraySize = 1;
	Desc.BindFlags = D3D11_BIND_DEPTH_STENCIL  | D3D11_BIND_SHADER_RESOURCE;
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.Format = DXGI_FORMAT_R32_TYPELESS;
	Desc.Width = pBackBufferSurfaceDesc->Width;
	Desc.Height = pBackBufferSurfaceDesc->Height;
	Desc.MipLevels = 1;
	Desc.SampleDesc.Count = 1;
	V_RETURN( pd3dDevice->CreateTexture2D( &Desc, NULL, &pTexRender11 ) );


	// Create the depth stencil view for the shadow map
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvd;
    dsvd.Flags = 0;
    dsvd.Format = DXGI_FORMAT_D32_FLOAT;
    dsvd.Texture2D.MipSlice = 0;
    dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    V_RETURN( pd3dDevice->CreateDepthStencilView( pTexRender11, &dsvd, &pTexRenderDS11 ) );

    // Create the resource view for the shadow map
    D3D11_SHADER_RESOURCE_VIEW_DESC DescRV;
    DescRV.Format = DXGI_FORMAT_R32_FLOAT;
    DescRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    DescRV.Texture2D.MipLevels = 1;
    DescRV.Texture2D.MostDetailedMip = 0;
    V_RETURN( pd3dDevice->CreateShaderResourceView( pTexRender11, &DescRV, &pTexRenderRV11 ) );

    return S_OK;
}

void adjustWorldPosition(D3DXMATRIX & mWorld)
{
	D3DXMATRIX mSpinX, mSpinZ, mSpinY, mTranslate;
	D3DXMatrixRotationY( &mSpinY, -D3DX_PI*0.05 );
	D3DXMatrixMultiply( &mWorld, &mWorld, &mSpinY );
	D3DXMatrixRotationX( &mSpinX, -D3DX_PI*0.02 );
	D3DXMatrixMultiply( &mWorld, &mWorld, &mSpinX );

}
void FillGrid_Indexed_WithTangentSpace( ID3D11Device* pd3dDevice, DWORD dwWidth, DWORD dwLength, 
                                        float fGridSizeX, float fGridSizeZ, 
                                        ID3D11Buffer** lplpVB, ID3D11Buffer** lplpIB )
{
    HRESULT hr;
    DWORD   nNumVertex = ( dwWidth + 1 ) * ( dwLength + 1 );
    DWORD   nNumIndex = 3 * 2 * dwWidth * dwLength;
    float   fStepX = fGridSizeX / dwWidth;
    float   fStepZ = fGridSizeZ / dwLength;
    
    // Allocate memory for buffer of vertices in system memory
    Vertex*    pVertexBuffer = new Vertex[nNumVertex];
    Vertex*    pVertex = &pVertexBuffer[0];

    // Fill vertex buffer (normal is always (0,1,0) )
    for ( DWORD i=0; i<=dwLength; ++i )
    {
        for ( DWORD j=0; j<=dwWidth; ++j )
        {
            pVertex->m_Position = float3(-fGridSizeX/2.0f + j*fStepX, 0.0f, fGridSizeZ/2.0f - i*fStepZ);
            pVertex->m_Texcoord = float2 (0.0f + ( (float)j / dwWidth  ), 0.0f + ( (float)i / dwLength ));
            pVertex->m_Bitangent = float3(0.0f, 0.0f, -1.0f);
			pVertex->m_Tangent = float3(1.0f, 0.0f, 0.0f);
            pVertex++;
        }
    }
    
    // Allocate memory for buffer of indices in system memory
    WORD*    pIndexBuffer = new WORD [nNumIndex];
    WORD*    pIndex = &pIndexBuffer[0];

    // Fill index buffer
    for ( DWORD i=0; i<dwLength; ++i )
    {
        for ( DWORD j=0; j<dwWidth; ++j )
        {
            *pIndex++ = (WORD)(  i    * (dwWidth+1) + j     );
            *pIndex++ = (WORD)(  i    * (dwWidth+1) + j + 1 );
            *pIndex++ = (WORD)( (i+1) * (dwWidth+1) + j     );

            *pIndex++ = (WORD)( (i+1) * (dwWidth+1) + j     );
            *pIndex++ = (WORD)(  i    * (dwWidth+1) + j + 1 );
            *pIndex++ = (WORD)( (i+1) * (dwWidth+1) + j + 1 );
        }
    }

#ifdef GRID_OPTIMIZE_INDICES
    GridOptimizeIndices(pIndexBuffer, nNumIndex, nNumVertex);
#endif

#ifdef GRID_OPTIMIZE_VERTICES
    GridOptimizeVertices(pIndexBuffer, pVertexBuffer, sizeof(TANGENTSPACEVERTEX), 
                         nNumIndex, nNumVertex);
#endif


    // Actual VB creation

    // Set initial data info
    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = pVertexBuffer;

    // Fill DX11 vertex buffer description
    D3D11_BUFFER_DESC    bd;
    bd.Usage =           D3D11_USAGE_DEFAULT;
    bd.ByteWidth =       sizeof( Vertex ) * nNumVertex;
    bd.BindFlags =       D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags =  0;
    bd.MiscFlags =       0;

    // Create DX11 vertex buffer specifying initial data
    hr = pd3dDevice->CreateBuffer( &bd, &InitData, lplpVB );
    if( FAILED( hr ) )
    {
        OutputDebugString( L"FillGrid_Indexed_WithTangentSpace: Failed to create vertex buffer.\n" );
        return;
    }

    // Actual IB creation

    // Set initial data info
    InitData.pSysMem = pIndexBuffer;

    // Fill DX11 vertex buffer description
    bd.ByteWidth = sizeof( WORD ) * nNumIndex;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    
    // Create DX11 index buffer specifying initial data
    hr = pd3dDevice->CreateBuffer( &bd, &InitData, lplpIB );
    if( FAILED( hr ) )
    {
        OutputDebugString( L"FillGrid_Indexed_WithTangentSpace: Failed to create index buffer.\n" );
        return;
    }

    // Release host memory vertex buffer
    delete [] pIndexBuffer;

    // Release host memory vertex buffer
    delete [] pVertexBuffer;
}

void RenderPlaneMesh( ID3D11DeviceContext* pd3dDeviceContext, UINT32 mode  )
{
	D3DXMATRIX mWorld = *g_Camera.GetWorldMatrix();
	adjustWorldPosition(mWorld);

	D3DXMATRIX mView = *g_Camera.GetViewMatrix();
	D3DXMATRIX mProj = *g_Camera.GetProjMatrix();
	D3DXMATRIX mWorldView = mWorld * mView;
	D3DXMATRIX mViewProjection = mWorldView * mProj;
	D3DXMATRIX mLightView = *g_Light.GetViewMatrix();
	D3DXMATRIX mLightProj = *g_Light.GetProjMatrix();

	D3D11_MAPPED_SUBRESOURCE MappedResource;
	pd3dDeviceContext->Map( g_pcbPerDraw, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
	CB_PER_DRAW_CONSTANTS* pData = ( CB_PER_DRAW_CONSTANTS* )MappedResource.pData;
	pData = ( CB_PER_DRAW_CONSTANTS* )MappedResource.pData;
	D3DXMatrixTranspose( &pData->world, &mWorld );
	D3DXMatrixTranspose( &pData->worldView, &mWorldView );
	D3DXMatrixTranspose( &pData->viewProjection, &mViewProjection );
	mViewProjection = mLightView * mLightProj;
	D3DXMatrixTranspose( &pData->mLightViewProjection, &mViewProjection );
	pd3dDeviceContext->Unmap( g_pcbPerDraw, 0 );

	ID3D11PixelShader* pPixelShader;
	ID3D11HullShader* pHullShader;
	pPixelShader = g_pPlanePS;
	if (mode==2) pPixelShader = g_pSolidColorPS;
	pHullShader = g_pPlaneHS;
	if (g_iAdaptive == 1) 
		pHullShader = g_pPlaneHS_Distance;

    // Bind all of the CBs
	pd3dDeviceContext->VSSetConstantBuffers( g_iBindPerDraw, 1, &g_pcbPerDraw );
	pd3dDeviceContext->HSSetConstantBuffers( g_iBindPerDraw, 1, &g_pcbPerDraw );
	pd3dDeviceContext->DSSetConstantBuffers( g_iBindPerDraw, 1, &g_pcbPerDraw );
	pd3dDeviceContext->PSSetConstantBuffers( g_iBindPerDraw, 1, &g_pcbPerDraw );
	// Set the shaders
    pd3dDeviceContext->VSSetShader( g_pPlaneVS, NULL, 0 );
	pd3dDeviceContext->HSSetShader( pHullShader, NULL, 0 );
	pd3dDeviceContext->DSSetShader( g_pPlaneDS, NULL, 0 );
	pd3dDeviceContext->PSSetShader( pPixelShader, NULL, 0 );
	pd3dDeviceContext->HSSetShaderResources( 7, 1, &pFloorHeightTextureSRV );
	pd3dDeviceContext->DSSetShaderResources( 7, 1, &pFloorHeightTextureSRV );
	pd3dDeviceContext->PSSetShaderResources( 7, 1, &pFloorHeightTextureSRV );
    pd3dDeviceContext->PSSetShaderResources( 3, 1, &pTexRenderRV11 );
	pd3dDeviceContext->PSSetShaderResources( 4, 1, &pFloorTextureSRV );
    ID3D11SamplerState* ppSamplerStates[2] = { g_pSamplePoint, g_pSampleLinear};
	pd3dDeviceContext->DSSetSamplers( 0, 2, ppSamplerStates );
    pd3dDeviceContext->PSSetSamplers( 0, 2, ppSamplerStates );

	pd3dDeviceContext->IASetInputLayout( g_pConstructionLayout );
    pd3dDeviceContext->IASetIndexBuffer( pPlaneIndexBuffer, DXGI_FORMAT_R32_UINT, 0 ); 
    UINT Stride = sizeof( Vertex);
    UINT Offset = 0;
    pd3dDeviceContext->IASetVertexBuffers( 0, 1, &pPlaneVertexBuffer, &Stride, &Offset );
    pd3dDeviceContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST );

	pd3dDeviceContext->DrawIndexed( 6*GRIDLENGTH*GRIDWIDTH, 0, 0 );
	
	ID3D11ShaderResourceView* pNull = NULL;
	pd3dDeviceContext->PSSetShaderResources( 3, 1, &pNull );
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                  float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

	int frame = (g_frame/g_iAnimationSpeed) % FRAMENUM;// == 0 ? 0 : (g_frame/g_iAnimationSpeed)  % FRAMENUM; 
	if (g_bAnimation) g_frame++;

	D3DXVECTOR3 vEyePt = *g_Camera.GetEyePt();
	D3DXVECTOR3 vLightPt = *g_Light.GetEyePt();
	D3DXVECTOR3 vLightDir = *g_Light.GetLookAtPt() - vLightPt;
	D3DXVECTOR3 vEyeDir = *g_Camera.GetLookAtPt() - vEyePt;
	D3DXVec3Normalize( &vEyeDir, &vEyeDir );
	D3DXVec3Normalize( &vLightDir, &vLightDir );

    // Update per-frame variables
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    pd3dImmediateContext->Map( g_pcbPerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    CB_PER_FRAME_CONSTANTS* pData = ( CB_PER_FRAME_CONSTANTS* )MappedResource.pData;
    pData = ( CB_PER_FRAME_CONSTANTS* )MappedResource.pData;
	pData->vEyeDir = D3DXVECTOR4(vEyePt,0.0f);
	pData->vLightPos = D3DXVECTOR4(-20,240,0,0);
	pData->vLightDir = D3DXVECTOR4(0,1,0,0);
	pData->vRandom = D3DXVECTOR4(0,0,0, (float)g_lightIntensity);
	pData->fLightAngle = 0.95;
	pData->fTessellationFactor = (float)g_iSubdivs;
    pData->fDisplacementHeight = 3.0f;
    pd3dImmediateContext->Unmap( g_pcbPerFrame, 0 );

	pd3dImmediateContext->HSSetConstantBuffers( g_iBindPerFrame, 1, &g_pcbPerFrame );
	pd3dImmediateContext->VSSetConstantBuffers( g_iBindPerFrame, 1, &g_pcbPerFrame );
	pd3dImmediateContext->DSSetConstantBuffers( g_iBindPerFrame, 1, &g_pcbPerFrame );
	pd3dImmediateContext->PSSetConstantBuffers( g_iBindPerFrame, 1, &g_pcbPerFrame );

    const FLOAT rgbaBackground[4] =  { 0.0f, 0.0f, 0.0f, 0.0f };
	
    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    ID3D11DepthStencilView* pDSV = DXUTGetD3D11DepthStencilView();
    pd3dImmediateContext->ClearRenderTargetView( pRTV, rgbaBackground );
    pd3dImmediateContext->ClearDepthStencilView( pTexRenderDS11, D3D11_CLEAR_DEPTH, 1.0, 0 );


	//
	// Shadow Pass
	//
	ID3D11RenderTargetView* pNullRT = NULL;
	pd3dImmediateContext->OMSetRenderTargets( 1, &pNullRT, pTexRenderDS11 );	
	pd3dImmediateContext->OMSetBlendState( g_pBlendStateNoBlend, 0, 0xffffffff );
    pd3dImmediateContext->RSSetState( g_pRasterizerStateSolid );
    dm1->GenerateShadowMap( pd3dImmediateContext, frame);
	sm1->GenerateShadowMap( pd3dImmediateContext, 0);

	//
	// Main pass
	//
	pd3dImmediateContext->ClearRenderTargetView( pRTV, rgbaBackground );
    pd3dImmediateContext->ClearDepthStencilView( pDSV, D3D11_CLEAR_DEPTH, 1.0, 0 );
    pd3dImmediateContext->OMSetRenderTargets( 1, &pRTV, pDSV );


    pd3dImmediateContext->RSSetState( g_pRasterizerStateSolid );

	pd3dImmediateContext->RSSetState( g_bDrawWires ? g_pRasterizerStateWireframe : g_pRasterizerStateSolid );
	int wireframeChoice = 1;
	if( g_bDrawWires ) wireframeChoice = 2;
	RenderPlaneMesh( pd3dImmediateContext, wireframeChoice);

	dm1->Render(pd3dImmediateContext, wireframeChoice, frame);
	sm1->Render(pd3dImmediateContext, wireframeChoice, 0);

    // Render the HUD
	DXUT_BeginPerfEvent( DXUT_PERFEVENTCOLOR, L"HUD / Stats" );
	g_HUD.OnRender( fElapsedTime );
	g_SampleUI.OnRender( fElapsedTime );
	RenderText();
	DXUT_EndPerfEvent();

}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D10ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{

	g_DialogResourceManager.OnD3D11ReleasingSwapChain();

	SAFE_RELEASE( pTexRender11 );
	SAFE_RELEASE( pTexRenderDS11 );
	SAFE_RELEASE( pTexRenderRV11 );

}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D10CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();

    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );

    //release shaders
    SAFE_RELEASE( g_pSkinningVS );
    SAFE_RELEASE( g_pSubDToBezierHS );
    SAFE_RELEASE( g_pSubDToGregoryHS );
	SAFE_RELEASE( g_pSubDToBezierHS_Distance );
    SAFE_RELEASE( g_pSubDToGregoryHS_Distance );
    SAFE_RELEASE( g_pSurfaceEvalDS );
    SAFE_RELEASE( g_pGregoryEvalDS );
    SAFE_RELEASE( g_pPhongShadingPS );
	SAFE_RELEASE( g_pPhongShadingPS_I );
	SAFE_RELEASE( g_pFlatShadingPS );
	SAFE_RELEASE( g_pFlatShadingPS_I );
    SAFE_RELEASE( g_pSolidColorPS );
    SAFE_RELEASE( g_pPlaneVS );
	SAFE_RELEASE( g_pPlaneHS );
	SAFE_RELEASE( g_pPlaneHS_Distance );
	SAFE_RELEASE( g_pPlaneDS );
    SAFE_RELEASE( g_pPlanePS );
	SAFE_RELEASE( g_pInputMeshVS );
    SAFE_RELEASE( g_pInputMeshPS );
    SAFE_RELEASE( g_pShadowmapVS );
    SAFE_RELEASE( g_pShadowmapPS );
	SAFE_RELEASE( g_pStaticObjectVS);
	SAFE_RELEASE( g_pStaticObjectHS);
	SAFE_RELEASE( g_pStaticObjectHS_Distance);
	SAFE_RELEASE( g_pStaticObjectGregoryHS);
	SAFE_RELEASE( g_pStaticObjectGregoryHS_Distance);
	SAFE_RELEASE( g_pStaticObjectDS);
	SAFE_RELEASE( g_pStaticObjectGregoryDS);

    //release rasterizer states and samplers
    SAFE_RELEASE( g_pRasterizerStateSolid );
    SAFE_RELEASE( g_pRasterizerStateWireframe );
    SAFE_RELEASE( g_pSamplePoint );
    SAFE_RELEASE( g_pSampleLinear );
	SAFE_RELEASE( g_pBlendStateNoBlend );

    SAFE_RELEASE( g_pcbPerFrame );
	SAFE_RELEASE( g_pcbPerDraw );

	SAFE_RELEASE( pPlaneVertexBuffer ); 
	SAFE_RELEASE( pPlaneIndexBuffer ); 


    //release inputlayout
    SAFE_RELEASE( g_pConstructionLayout ); 
	SAFE_RELEASE( g_pVertexLayout ); 

    //release 2D Textures and their views
    SAFE_RELEASE( pHeightMapTextureSRV );
	SAFE_RELEASE( pNormalMapTextureSRV );
	SAFE_RELEASE( pFloorHeightTextureSRV );
	SAFE_RELEASE( pFloorTextureSRV );
    SAFE_RELEASE( pTexRenderDS11 );
    SAFE_RELEASE( pTexRender11 );
    SAFE_RELEASE( pTexRenderRV11 );

	// release geometry objects
	SAFE_DELETE(dm1);
	SAFE_DELETE(sm1);

}

//--------------------------------------------------------------------------------------
// Helper function to check for file existance
//--------------------------------------------------------------------------------------
void CreateShaderResourceViewFromFile(ID3D11Device* pd3dDevice, WCHAR* pFileName, ID3D11ShaderResourceView** srv )
{
	if( FileExists( pFileName ) )
    {
		HRESULT hr;
        hr = D3DX11CreateShaderResourceViewFromFile( pd3dDevice, pFileName, NULL, NULL, srv, NULL );
		assert( D3D_OK == hr );
    }
}

bool FileExists( WCHAR* pFileName )
{
    DWORD fileAttr;    
    fileAttr = GetFileAttributes(pFileName);    
    if (0xFFFFFFFF == fileAttr)        
        return false;    
    return true;
}

//--------------------------------------------------------------------------------------
// Helper function to compile an hlsl shader from file, 
// its binary compiled code is returned
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, 
                               LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D10_SHADER_MACRO* pDefines )
{
    HRESULT hr = S_OK;

    // find the file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

    // open the file
    HANDLE hFile = CreateFile( str, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_FLAG_SEQUENTIAL_SCAN, NULL );
    if( INVALID_HANDLE_VALUE == hFile )
        return E_FAIL;

    // Get the file size
    LARGE_INTEGER FileSize;
    GetFileSizeEx( hFile, &FileSize );

    // create enough space for the file data
    BYTE* pFileData = new BYTE[ FileSize.LowPart ];
    if( !pFileData )
        return E_OUTOFMEMORY;

    // read the data in
    DWORD BytesRead;
    if( !ReadFile( hFile, pFileData, FileSize.LowPart, &BytesRead, NULL ) )
        return E_FAIL; 

    CloseHandle( hFile );

    // Compile the shader
    char pFilePathName[MAX_PATH];        
    WideCharToMultiByte(CP_ACP, 0, str, -1, pFilePathName, MAX_PATH, NULL, NULL);
    ID3DBlob* pErrorBlob;
    hr = D3DCompile( pFileData, FileSize.LowPart, pFilePathName, pDefines, NULL, szEntryPoint, szShaderModel, D3D10_SHADER_ENABLE_STRICTNESS, 0, ppBlobOut, &pErrorBlob );

    delete []pFileData;

    if( FAILED(hr) )
    {
        OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        return hr;
    }
    SAFE_RELEASE( pErrorBlob );

    return S_OK;
}


HRESULT initResourcesForAnimatedObjects(ID3D11Device* pd3dDevice)
{
	HRESULT hr = S_OK;
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"GregoryAccTessellation11\\monsterfrog_mapped_10.bzr" ));	

	SAFE_DELETE(dm1);
	dm1 = new DynamicMesh( str );	
	dm1->Init();

	return S_OK;
}
HRESULT initResourcesForStaticObjects(ID3D11Device* pd3dDevice)
{
	HRESULT hr = S_OK;
	WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, L"GregoryAccTessellation11\\Tree.bzr" ));
	SAFE_DELETE(sm1);
	sm1 = new StaticMesh( str );	
	sm1->Init();

	
	return S_OK;
}

