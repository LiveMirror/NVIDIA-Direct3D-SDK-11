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

//--------------------------------------------------------------------------------------
// This program performs a parallel simulation of an "N-body" system of stars.  That is,
// it simulates the evolution over time of the positions and velocities of a number of
// massive bodies, such as stars, that all influence each other via gravitational
// attraction.  Because all bodies interact with all others, there are O(N^2) interactions
// to compute at each time step (each frame).  This is a lot of computation, but it 
// is highly parallel and is thus amenable to GPU implementation using DirectCompute
// shaders.

#include "nBodyCS.h"

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------

CModelViewerCamera            g_Camera;                // A model viewing camera

CDXUTDialogResourceManager    g_DialogResourceManager; // manager for shared resources of dialogs
CD3DSettingsDlg               g_SettingsDlg;           // Device settings dialog
CDXUTDialog                   g_HUD;                   // dialog for standard controls
CDXUTDialog                   g_SampleUI;              // dialog for sample-specific controls

bool                          g_bShowHelp          = false;  
bool                          g_bPaused            = false;

float                         g_pointSize          = DEFAULT_POINT_SIZE;
float                         g_fFrameTime         = 0.0f;
int                           g_iNumBodies         = 1;

float                         g_clusterScale       = 1.54f;
float                         g_velocityScale      = 8.0f;

CDXUTTextHelper*              g_pTxtHelper         = NULL;

BodyData                      g_BodyData;

NBodySystemCS*                g_nBodySystem        = NULL;  

UINT g_iSimSizes[] = {1024, 4096, 8192, 14336, 16384, 28672, 30720, 32768, 57344, 61440, 65536 };

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------

// D3D11 callbacks
bool    CALLBACK IsD3D11DeviceAcceptable  ( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo, DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext );
HRESULT CALLBACK OnD3D11CreateDevice      ( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
HRESULT CALLBACK OnD3D11ResizedSwapChain  ( ID3D11Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext );
void    CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext );
void    CALLBACK OnD3D11DestroyDevice     ( void* pUserContext );
void    CALLBACK OnD3D11FrameRender       ( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime, float fElapsedTime, void* pUserContext );

// System callbacks
LRESULT CALLBACK MsgProc                  ( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext );
void    CALLBACK KeyboardProc             ( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext );
void    CALLBACK OnGUIEvent               ( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext );
void    CALLBACK OnFrameMove              ( double fTime, float fElapsedTime, void* pUserContext );
bool    CALLBACK ModifyDeviceSettings     ( DXUTDeviceSettings* pDeviceSettings, void* pUserContext );

// Support functions
void    InitApp();
void    RenderText10( double fTime );
void    initializeBodies(unsigned int numBodies);

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR, int )
{
    HRESULT hr;
    V_RETURN(DXUTSetMediaSearchPath(L"..\\Source\\nBodyCS"));
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Set the callback functions. These functions allow DXUT to notify
    // the application about device changes, user input, and windows messages.  The 
    // callbacks are optional so you need only set callbacks for events you're interested 
    // in. However, if you don't handle the device reset/lost callbacks then the sample 
    // framework won't be able to reset your device since the application must first 
    // release all device resources before resetting.  Likewise, if you don't handle the 
    // device created/destroyed callbacks then DXUT won't be able to 
    // recreate your device resources.

    // Direct3D10 callbacks
    DXUTSetCallbackD3D11DeviceAcceptable  ( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated     ( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized  ( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed   ( OnD3D11DestroyDevice );
    DXUTSetCallbackD3D11FrameRender       ( OnD3D11FrameRender );

    DXUTSetCallbackMsgProc       ( MsgProc );
    DXUTSetCallbackKeyboard      ( KeyboardProc );
    DXUTSetCallbackFrameMove     ( OnFrameMove );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );

    // Show the cursor and clip it when in full screen
    DXUTSetCursorSettings( true, true );

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLine(), &argc);
    g_iNumBodies = 30720;
    if (argc > 1)
    {
        g_iNumBodies = _wtoi(argv[1]);
    }

    InitApp();

    // Initialize DXUT and create the desired Win32 window and Direct3D 
    // device for the application. Calling each of these functions is optional, but they
    // allow you to set several options which control the behavior of the framework.
    DXUTInit        ( true, true ); // Parse the command line, handle the default hotkeys, and show msgboxes
    DXUTCreateWindow( L"Compute Shader N-Body Simulation" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_10_0, true, 800, 600 );

    // Pass control to DXUT for handling the message pump and 
    // dispatching render calls. DXUT will call your FrameMove 
    // and FrameRender callback when there is idle time between handling window messages.
    DXUTMainLoop();

    // Perform any application-level cleanup here. Direct3D device resources are released within the
    // appropriate callback functions and therefore don't require any cleanup code here.

    return DXUTGetExitCode();
}

//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    // Initialize dialogs
    g_SettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init        ( &g_DialogResourceManager );
    g_SampleUI.Init   ( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); 
	g_SampleUI.SetCallback( OnGUIEvent );

	int iY = 10; 
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen" ,   35, iY, 125, 22 );
    g_HUD.AddButton( IDC_TOGGLEREF,        L"Toggle REF (F3)"    ,   35, iY += 24, 125, 22, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE,     L"Change device (F2)" ,   35, iY += 24, 125, 22, VK_F2 );
	g_HUD.AddButton( IDC_RESET,            L"Reset (R)",             35, iY += 48, 125, 22, 'R' );
	g_HUD.AddButton( IDC_TOGGLEUPDATES,    L"Pause (P)",             35, iY += 24, 125, 22, 'P' );

	iY = 0;
	g_SampleUI.AddStatic  ( IDC_POINTSIZELABEL, L"Point Size ",              35, iY += 48, 125, 22 );
    g_SampleUI.AddSlider  ( IDC_POINTSIZE,                                   35, iY += 24, 125, 22, -SCALE_SLIDER_MAX, SCALE_SLIDER_MAX, 0 );

    CDXUTComboBox *combobox;
    g_SampleUI.AddStatic  ( IDC_NUMBODIESLABEL, L"# Bodies",                 35, iY += 48, 125, 22 );
    g_SampleUI.AddComboBox( IDC_NUMBODIES,                                   35, iY += 24, 125, 22, 0, false, &combobox);
    combobox->AddItem(L"1024",  0);
    combobox->AddItem(L"4096",  0);
    combobox->AddItem(L"8192",  0);
    combobox->AddItem(L"14336", 0);
    combobox->AddItem(L"16384", 0);
    combobox->AddItem(L"28672", 0);
    combobox->AddItem(L"30720", 0);
    combobox->AddItem(L"32768", 0);
    combobox->AddItem(L"57344", 0);
    combobox->AddItem(L"61440", 0);
    combobox->AddItem(L"65536", 0);
    combobox->SetSelectedByIndex(6);
  
	g_nBodySystem = new NBodySystemCS();
	
	g_Camera.SetEnablePositionMovement(true);
}

//--------------------------------------------------------------------------------------
// Determine if the device is acceptable
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable( const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, 
									   const CD3D11EnumDeviceInfo *DeviceInfo, DXGI_FORMAT BackBufferFormat, 
									   bool bWindowed, void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has been 
// created, which will happen during application initialization and windowed/full screen 
// toggles. This is the best location to create D3DPOOL_MANAGED resources since these 
// resources need to be reloaded whenever the device is destroyed. Resources created  
// here should be released in the OnD3D10DestroyDevice callback. 
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC *pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

	D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS options;
	hr = pd3dDevice->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &options, sizeof(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS));
	if( !options.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x )
	{
		MessageBox(NULL, L"Compute Shaders are not supported on your hardware", L"Unsupported HW", MB_OK);
		return E_FAIL;
	}

	ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_SettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
   
	// Initialize the font
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    V_RETURN( CDXUTDirectionWidget::StaticOnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );

	D3DXVECTOR3 vecEye(0.0f, 0.0f, -50.0f);
    D3DXVECTOR3 vecAt (0.0f, 0.0f,  0.0f);
    g_Camera.SetViewParams( &vecEye, &vecAt );

	// Init the Body system
	g_nBodySystem->onCreateDevice(pd3dDevice, pd3dImmediateContext);
	initializeBodies( g_iNumBodies );
	g_nBodySystem->resetBodies( g_BodyData ); 
	g_nBodySystem->setPointSize   ( g_pointSize );

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Main render function
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime, float fElapsedTime, void* pUserContext )
{
    //float ClearColor[4] = { 0.176f, 0.196f, 0.667f, 0.0f };

	float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	// Clear the main render target
    pd3dImmediateContext->ClearRenderTargetView( DXUTGetD3D11RenderTargetView(), ClearColor );

	// Clear the depth stencil
    pd3dImmediateContext->ClearDepthStencilView( DXUTGetD3D11DepthStencilView(), D3D10_CLEAR_DEPTH, 1.0, 0 );

    // If the settings dialog is being shown, then
    // render it instead of rendering the app's scene
    if( g_SettingsDlg.IsActive() )
    {
        g_SettingsDlg.OnRender( fElapsedTime );
        return;
    }

	// Update bodies
	if (!g_bPaused)
    {
        g_nBodySystem->updateBodies(fElapsedTime);
    }

	// Render bodies
	g_nBodySystem->renderBodies( g_Camera.GetWorldMatrix() , g_Camera.GetViewMatrix() , g_Camera.GetProjMatrix() );
	
	// This seems broken in this latest DXUT version
	g_HUD.OnRender( fElapsedTime ); 
	g_SampleUI.OnRender( fElapsedTime );
	
	RenderText10( fTime );
}


//--------------------------------------------------------------------------------------
// Handle resize
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

    // SwapChain has changed and may have new attributes such as size. Application
    // should create resources dependent on SwapChain, such as 2D textures that
    // need to have the same size as the back buffer.
    //
    DXUTTRACE( L"SwapChain Reset called\n" );

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_SettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

	// Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams ( D3DX_PI/4, fAspectRatio, 0.1f, 1000.0f );
    g_Camera.SetWindow     ( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );
	
    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width-170, 0 );
    g_HUD.SetSize    ( 170, 170 );

	g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width-170, 180 );
    g_SampleUI.SetSize( 170, 300 );

	g_nBodySystem->onResizedSwapChain(pd3dDevice, pBackBufferSurfaceDesc);

	return S_OK;
}

//--------------------------------------------------------------------------------------
// Handle release
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    DXUTTRACE( L"SwapChain Lost called\n" );

    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// been destroyed, which generally happens as a result of application termination or 
// windowed/full screen toggles. Resources created in the OnD3D10CreateDevice callback 
// should be released here, which generally includes all D3DPOOL_MANAGED resources. 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
	g_nBodySystem->onDestroyDevice();

    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_SettingsDlg.OnD3D11DestroyDevice();
	DXUTGetGlobalResourceCache().OnDestroyDevice();
    
	CDXUTDirectionWidget::StaticOnD3D11DestroyDevice();

	SAFE_DELETE( g_pTxtHelper );

}

//--------------------------------------------------------------------------------------
// This callback function is called immediately before a device is created to allow the 
// application to modify the device settings. The supplied pDeviceSettings parameter 
// contains the settings that the framework has selected for the new device, and the 
// application can make any desired changes directly to this structure.  Note however that 
// DXUT will not correct invalid device settings so care must be taken 
// to return valid device settings, otherwise IDirect3D9::CreateDevice() will fail.  
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    
    return true;
}

//--------------------------------------------------------------------------------------
// This callback function will be called once at the beginning of every frame. This is the
// best location for your application to handle updates to the scene, but is not 
// intended to contain actual rendering calls, which should instead be placed in the 
// OnFrameRender callback.  
//--------------------------------------------------------------------------------------
void    CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    static int tickCounter = 0;
	
	g_Camera.FrameMove(fElapsedTime);
    
	// Update the global time
	g_fFrameTime = fElapsedTime; 
}

//--------------------------------------------------------------------------------------
// Before handling window messages, DXUT passes incoming windows 
// messages to the application through this callback function. If the application sets 
// *pbNoFurtherProcessing to TRUE, then DXUT will not process this message.
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing, void* pUserContext )
{
    // Always allow dialog resource manager calls to handle global messages
    // so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

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

	// Pass all windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
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
            case VK_F1: g_bShowHelp = !g_bShowHelp; break;
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
        case IDC_TOGGLEFULLSCREEN:   DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:          DXUTToggleREF(); break;
        case IDC_CHANGEDEVICE:       g_SettingsDlg.SetActive( !g_SettingsDlg.IsActive() ); break;
    
        case IDC_TOGGLEUPDATES:    g_bPaused = !g_bPaused; break;
        case IDC_RESET:            initializeBodies( g_iNumBodies );
								   g_nBodySystem->resetBodies( g_BodyData ); 
								   break;
   
		case IDC_POINTSIZE:
			{
				float sliderValue = (float)static_cast<CDXUTSlider*>(g_SampleUI.GetControl(IDC_POINTSIZE))->GetValue();
				
				if(sliderValue > 0){
					g_pointSize = DEFAULT_POINT_SIZE * MAX_SCALING_FACTOR * sliderValue / SCALE_SLIDER_MAX;
				}else{
					if(sliderValue < 0 ){
						g_pointSize = DEFAULT_POINT_SIZE / (MAX_SCALING_FACTOR * fabs(sliderValue) / SCALE_SLIDER_MAX);
					}
				}
				 
				g_nBodySystem->setPointSize( g_pointSize );
			}
			break;
        case IDC_NUMBODIES:
            {
                UINT index = (UINT)static_cast<CDXUTComboBox*>(g_SampleUI.GetControl(IDC_NUMBODIES))->GetSelectedIndex();
                g_iNumBodies = g_iSimSizes[index];
                initializeBodies( g_iNumBodies );
                g_nBodySystem->resetBodies( g_BodyData ); 
            }
            break;
    }

}

//--------------------------------------------------------------------------------------
// Helper function to compute performance statistics for n-body system
//--------------------------------------------------------------------------------------
void computePerfStats(double &interactionsPerSecond, double &gflops, 
                      float seconds, int iterations)
{
    const int flopsPerInteraction = 20;
    interactionsPerSecond = (float)g_iNumBodies * (float)g_iNumBodies;
    interactionsPerSecond *= 1e-9 * iterations / seconds;
    gflops = interactionsPerSecond * (float)flopsPerInteraction;
}


//--------------------------------------------------------------------------------------
// Render Text 
//--------------------------------------------------------------------------------------
void RenderText10( double fTime )
{
 	// Output statistics
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 5, 5 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats() );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );
    
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
    double GFLOPs, interactionsPerSecond;
    computePerfStats(interactionsPerSecond, GFLOPs, g_fFrameTime, 1);
    g_pTxtHelper->DrawFormattedTextLine( L"fps: %0.2f | Time: %0.4f (ms) | GFLOP/s: %0.2f | Bodies: %d", 1.0/g_fFrameTime, g_fFrameTime*1000.0, GFLOPs, g_iNumBodies);
    
    /*char str[256];
    sprintf_s(str, "fps: %0.2f | Time: %0.4f (ms) | GFLOP/s: %0.2f | Bodies: %d", 1.0/g_fFrameTime, g_fFrameTime*1000.0, GFLOPs, g_iNumBodies);
    OutputDebugStringA(str);*/

    // Draw help
	const DXGI_SURFACE_DESC* pd3dsdBackBuffer = DXUTGetDXGIBackBufferSurfaceDesc();
	    
	if( g_bShowHelp )
    {
        g_pTxtHelper->SetInsertionPos( 10, pd3dsdBackBuffer->Height-15*6 );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 0.75f, 0.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"Controls (F1 to hide):" );

        g_pTxtHelper->SetInsertionPos( 40, pd3dsdBackBuffer->Height-15*5 );
        g_pTxtHelper->DrawTextLine( L"Quit: ESC" );
    }
    else
    {
        g_pTxtHelper->SetInsertionPos( 10, pd3dsdBackBuffer->Height-15*2 );
        g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f ) );
        g_pTxtHelper->DrawTextLine( L"Press F1 for help" );
    }
    g_pTxtHelper->End();
}

//--------------------------------------------------------------------------------------
// Initialize bodies
//--------------------------------------------------------------------------------------
void initializeBodies(unsigned int numBodies)
{
    if( g_BodyData.nBodies == numBodies )
    {
        return;
    }

    // Free previous data
    delete [] g_BodyData.position;
    delete [] g_BodyData.velocity;

    // Allocate new data
    g_BodyData.position = new float[numBodies * 3];
    g_BodyData.velocity = new float[numBodies * 3];

    g_BodyData.nBodies = numBodies;

    float scale = g_clusterScale;
    float vscale = scale * g_velocityScale;
    float inner = 2.5f * scale;
    float outer = 4.0f * scale;

    int p = 0, v=0;
    unsigned int i = 0;
    while (i < numBodies)
    {
        float x, y, z;
        x = rand() / (float) RAND_MAX * 2 - 1;
        y = rand() / (float) RAND_MAX * 2 - 1;
        z = rand() / (float) RAND_MAX * 2 - 1;

        D3DXVECTOR3 point(x, y, z);
        float len = D3DXVec3Length(&point);
        D3DXVec3Normalize(&point, &point);
        if (len > 1)
            continue;

        g_BodyData.position[p++] = point.x * (inner + (outer - inner) * rand() / (float) RAND_MAX);
        g_BodyData.position[p++] = point.y * (inner + (outer - inner) * rand() / (float) RAND_MAX);
        g_BodyData.position[p++] = point.z * (inner + (outer - inner) * rand() / (float) RAND_MAX);
        //g_BodyData.position[p++] = 1.0f;

        x = 0.0f; // * (rand() / (float) RAND_MAX * 2 - 1);
        y = 0.0f; // * (rand() / (float) RAND_MAX * 2 - 1);
        z = 1.0f; // * (rand() / (float) RAND_MAX * 2 - 1);
        D3DXVECTOR3 axis(x, y, z);
        D3DXVec3Normalize(&axis, &axis);

        if (1 - D3DXVec3Dot(&point, &axis) < 1e-6)
        {
            axis.x = point.y;
            axis.y = point.x;
            D3DXVec3Normalize(&axis, &axis);
        }
        //if (point.y < 0) axis = scalevec(axis, -1);
        D3DXVECTOR3 vv(g_BodyData.position[3*i], g_BodyData.position[3*i+1], g_BodyData.position[3*i+2]);
        D3DXVec3Cross(&vv, &vv, &axis);
        g_BodyData.velocity[v++] = vv.x * vscale;
        g_BodyData.velocity[v++] = vv.y * vscale;
        g_BodyData.velocity[v++] = vv.z * vscale;
        //g_BodyData.velocity[v++] = 1.0f;

        i++;
    }
}
