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

#pragma warning (disable: 4995)
#define NOMINMAX

#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "SDKmesh.h"
#include "MultiViewSoftShadows.h"

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
CDXUTDialogResourceManager  g_DialogResourceManager;    // manager for shared resources of dialogs
CModelViewerCamera          g_Camera;                   // A model viewing camera
CModelViewerCamera          g_LCamera;                   // A model viewing camera
CD3DSettingsDlg             g_D3DSettingsDlg;           // Device settings dialog
CDXUTDialog                 g_HUD;                      // dialog for standard controls
CDXUTTextHelper*            g_pTxtHelper = NULL;

Scene g_Scene;
float g_LightRadiusWorld = 0.5f;
UINT g_NumMeshes = 2;
bool g_ShowHelp = false;
bool g_ShowUI = true;

#define ZNEAR 0.1f
#define ZFAR 100.0f

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
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dContext, double fTime,
                                 float fElapsedTime, void* pUserContext );
void InitUI();

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
enum {
    IDC_TOGGLEFULLSCREEN = 1,
    IDC_TOGGLEREF,
    IDC_CHANGEDEVICE,
    IDC_CHANGETECHNIQUE,
    IDC_LIGHT_SIZE_STATIC,
    IDC_LIGHT_SIZE_SLIDER,
    IDC_NUM_MESHES_STATIC,
    IDC_NUM_MESHES_SLIDER,
    IDC_TOGGLE_VISDEPTH,
};

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
        case 'U':
            g_ShowUI = !g_ShowUI;
            break;
        case VK_ESCAPE:
           PostQuitMessage( 0 );
           break;
        }
    }
}

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Disable gamma correction on this sample
    DXUTSetIsInGammaCorrectMode( false );

    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackFrameMove( OnFrameMove );
    
    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    // IMPORTANT: set SDK media search path to include source directory of this sample, when started from .\Bin
    HRESULT hr;
    V_RETURN(DXUTSetMediaSearchPath(L"..\\Source\\MultiViewSoftShadows"));

    InitUI();

    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"Multi-View Soft Shadows" );
    DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, 1280, 720 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}

//--------------------------------------------------------------------------------------
// Initialize the GUI
//--------------------------------------------------------------------------------------
void InitUI()
{
    WCHAR str[100];
    int iY = 20;

    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent );
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 23 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 23, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 23, VK_F2 );

    iY += 24;
    StringCchPrintf( str, 100, L"Light Size: %.2f", g_LightRadiusWorld);
    g_HUD.AddStatic( IDC_LIGHT_SIZE_STATIC, str, 35, iY += 24, 125, 22 );
    g_HUD.AddSlider( IDC_LIGHT_SIZE_SLIDER, 50, iY += 24, 100, 22, 0, 100, int(g_LightRadiusWorld*100));

    StringCchPrintf( str, 100, L"Num Meshes: %d", g_NumMeshes);
    g_HUD.AddStatic( IDC_NUM_MESHES_STATIC, str, 35, iY += 24, 125, 22 );
    g_HUD.AddSlider( IDC_NUM_MESHES_SLIDER, 50, iY += 24, 100, 22, 1, MAX_NUM_MESHES, g_NumMeshes);

    iY += 10;
    g_HUD.AddCheckBox( IDC_TOGGLE_VISDEPTH, L"Visualize Depth", 20, iY += 24, 100, 22, false, 'V' );
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
    
    // Disable vsync
    pDeviceSettings->d3d11.SyncInterval = 0;

    // For the first device created if it is a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if (s_bFirstTime)
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF ) ||
           ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
            pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
            DXUTDisplaySwitchingToREFWarning( pDeviceSettings->ver );
        }
        // Also, prefer 4xAA, first time
        if (DXUT_D3D11_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d11.DriverType != D3D_DRIVER_TYPE_REFERENCE )
        {
            pDeviceSettings->d3d11.sd.SampleDesc.Count = 4;
            pDeviceSettings->d3d11.sd.SampleDesc.Quality = 0;
        }
    }

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
    // Update the camera's position based on user input 
    g_Camera.FrameMove(fElapsedTime);
    g_LCamera.FrameMove(fElapsedTime);
    g_Scene.SetLightCamera(g_LCamera);
    g_Scene.SetMainCamera(g_Camera);
}

//--------------------------------------------------------------------------------------
// Render text for the UI
//--------------------------------------------------------------------------------------
void RenderText(double ShadowMapRenderTimeMS, double EyeRenderTimeMs)
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 0.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    WCHAR str[100];
    StringCchPrintf(str, 100, L"Render Times: Shadow Maps: %0.1f ms, Shading: %0.1f ms", ShadowMapRenderTimeMS, EyeRenderTimeMs);
    g_pTxtHelper->DrawTextLine( str );

    const UINT ShadowMapSizeMB = SHADOW_MAP_ARRAY_SIZE*SHADOW_MAP_RES*SHADOW_MAP_RES*4 / (1024*1024);
    StringCchPrintf(str, 100, L"Shadow Maps: %dx%dx%d (%d MB total)", SHADOW_MAP_ARRAY_SIZE, SHADOW_MAP_RES, SHADOW_MAP_RES, ShadowMapSizeMB);
    g_pTxtHelper->DrawTextLine( str );

    const UINT NumTrisPerShadowMap = g_Scene.GetNumShadowCastingTriangles();
    StringCchPrintf(str, 100, L"Num Triangles / Shadow Map: %d (%dk total)", NumTrisPerShadowMap, NumTrisPerShadowMap*SHADOW_MAP_ARRAY_SIZE/1000);
    g_pTxtHelper->DrawTextLine( str );

    if (g_ShowHelp)
    {
        UINT nBackBufferHeight = DXUTGetDXGIBackBufferSurfaceDesc()->Height;
        g_pTxtHelper->SetInsertionPos( 2, nBackBufferHeight-15*6 );
        g_pTxtHelper->DrawTextLine( L"Controls:" );

        g_pTxtHelper->SetInsertionPos( 20, nBackBufferHeight-15*5 );
        g_pTxtHelper->DrawTextLine( L"Rotate light:  Left mouse button\n"
            L"Zoom camera:  Mouse wheel scroll\n"
            L"Rotate camera: Right mouse button");

        g_pTxtHelper->SetInsertionPos( 300, nBackBufferHeight-15*5 );
        g_pTxtHelper->DrawTextLine( L"Hide help: F1\n" 
            L"Quit: ESC\n" );
    }
    else
    {
        g_pTxtHelper->DrawTextLine( L"Press F1 for help" );
    }

    g_pTxtHelper->End();
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

    static unsigned MouseMask = 0;
    if (uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK)
    {
        MouseMask |= 1;
    }
    if (uMsg == WM_LBUTTONUP)
    {
        MouseMask &= ~1;
    }

    // Pass all windows messages to camera so it can respond to user input
    // Pass all remaining windows messages to camera so it can respond to user input
    if (MouseMask & 1) // left button pressed
        g_LCamera.HandleMessages( hWnd, uMsg, wParam, lParam );
    else
        g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    WCHAR str[100];

    switch( nControlID )
    {
    case IDC_TOGGLEFULLSCREEN:
        DXUTToggleFullScreen();
        break;
    case IDC_TOGGLEREF:
        DXUTToggleREF();
        break;
    case IDC_CHANGEDEVICE:
        g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() );
        break;
    case IDC_LIGHT_SIZE_SLIDER:
        g_LightRadiusWorld = (float) g_HUD.GetSlider( IDC_LIGHT_SIZE_SLIDER )->GetValue() / 100;
        StringCchPrintf( str, 100, L"Light Size: %.2f", g_LightRadiusWorld );
        g_HUD.GetStatic( IDC_LIGHT_SIZE_STATIC )->SetText( str );
        break;
    case IDC_NUM_MESHES_SLIDER:
        g_NumMeshes = g_HUD.GetSlider( IDC_NUM_MESHES_SLIDER )->GetValue();
        StringCchPrintf( str, 100, L"Num Meshes: %d", g_NumMeshes );
        g_HUD.GetStatic( IDC_NUM_MESHES_STATIC )->SetText( str );
        break;
    }
}

bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                      DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext )
{
    return true;
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

    ID3D11DeviceContext* pd3dContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dContext, &g_DialogResourceManager, 15 );

    g_Scene.CreateResources(pd3dDevice);

    D3DXVECTOR3 EyePos = D3DXVECTOR3(-0.644995f, 0.614183f, -0.660632f);
    D3DXVECTOR3 Lookat = D3DXVECTOR3(0,0,0);
    g_Camera.SetViewParams(&EyePos, &Lookat);
    g_Camera.SetRadius(1.7f, 0.001f, 4.0f);

    D3DXVECTOR3 LightPos = D3DXVECTOR3(3.57088f, 6.989f, -5.19698f);
    D3DXVECTOR3 LightLookat = D3DXVECTOR3(0,0,-0.4f);
    g_LCamera.SetViewParams(&LightPos, &LightLookat);

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Called whenever the swap chain is resized.  
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

    // Setup the camera's projection parameters    
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    float fFovyRad = D3DX_PI / 4;
    g_Camera.SetProjParams( fFovyRad, fAspectRatio, ZNEAR, ZFAR );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_LCamera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height);

    UINT HudWidth = 200;
    UINT HudHeight = 200;
    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - HudWidth, 0 );
    g_HUD.SetSize( HudWidth, HudHeight );

    g_Scene.SetScreenResolution(pd3dDevice, fFovyRad, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height);

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Callback function that renders the frame.  This function sets up the rendering 
// matrices and renders the scene and UI.
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if (g_D3DSettingsDlg.IsActive())
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    // Store off main render target and depth/stencil
    ID3D11RenderTargetView* pMainRTV = NULL;
    ID3D11DepthStencilView* pMainDSV = NULL;
    pd3dImmediateContext->OMGetRenderTargets(1, &pMainRTV, &pMainDSV);

    SceneParameters args;
    args.LightRadiusWorld = g_LightRadiusWorld;
    args.NumCharacters = g_NumMeshes;

    // Render scene with soft shadows
    double ShadowMapRenderTimeMs, EyeRenderTimeMs;
    g_Scene.Render(pd3dImmediateContext, pMainRTV, pMainDSV, args, ShadowMapRenderTimeMs, EyeRenderTimeMs);

    // Optionally, visualize the last-rendered shadow map
    const bool ShowShadowMap = g_HUD.GetCheckBox( IDC_TOGGLE_VISDEPTH )->GetChecked();
    if (ShowShadowMap) g_Scene.VisualizeShadowMap(pd3dImmediateContext, pMainRTV);

    // Restore initial render target and depth/stencil
    pd3dImmediateContext->OMSetRenderTargets(1, &pMainRTV, pMainDSV);
    SAFE_RELEASE(pMainRTV);
    SAFE_RELEASE(pMainDSV);

    if (g_ShowUI)
    {
        DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"HUD / Stats");
        g_HUD.OnRender(fElapsedTime);
        RenderText(ShadowMapRenderTimeMs, EyeRenderTimeMs);
        DXUT_EndPerfEvent();
    }
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

    g_Scene.ReleaseResources();
}

//--------------------------------------------------------------------------------------
// Release the swap chain
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}
