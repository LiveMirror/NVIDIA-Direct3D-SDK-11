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

// This dample demonstrates the use of approximating Catmull-clark subdivision 
// surfaces with gregory patches. 

#include "DXUT.h"
#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTsettingsDlg.h"
#include "SDKmisc.h"


#define MAXTESSFACTOR 64


typedef enum _TESSELLATION_MODE
{
    Integer             = 0,
	Pow2                = 1,
    FractionalOdd       = 2,
	FractionalEven      = 3,
}TESSELLATION_MODE;
TESSELLATION_MODE                                             g_tessellationMode = Integer;

typedef enum _REDUCTION_MODE
{
    Max                 = 0,
	Min                 = 1,
    Avg                 = 2,
}REDUCTION_MODE;
REDUCTION_MODE                                                g_reductionMode = Max;

struct CB
{  
    D3DXMATRIX f4x4WorldViewProjection; // World * View * Projection matrix  
    float fTessFactors[4];              // Tessellation factors 
	float fModes[4];                    // Tessellation pattern modes
};

struct Vertex
{
	D3DXVECTOR3 m_Position;
};
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

CModelViewerCamera                                            g_Camera;                  // A model viewing camera for each mesh scene
UINT                                                          g_iCBBind = 0;
static unsigned int                                           g_uQuadTessFactorL = 5;
static unsigned int 										  g_uQuadTessFactorR = 5;
static unsigned int 										  g_uQuadTessFactorT = 5;
static unsigned int 										  g_uQuadTessFactorB = 5;
static float                                                  g_scale=1.0f;

CDXUTTextHelper*                                              g_pTxtHelper = NULL;
ID3D11Buffer*           									  pQuadIndexBuffer = NULL;
ID3D11Buffer*           									  pTriIndexBuffer = NULL;
ID3D11Buffer*           									  pVertexBuffer = NULL;
ID3D11Buffer*           									  pTriVertexBuffer = NULL;
ID3D11Buffer*                                                 g_pcb = NULL;   
// Shaders
ID3D11VertexShader*         								  g_pSceneWithTessellationVS = NULL;
ID3D11HullShader*           								  g_pTrianglesHS[4];
ID3D11DomainShader*         								  g_pTrianglesDS = NULL;
ID3D11HullShader*           								  g_pQuadsHS[4];
ID3D11DomainShader*         								  g_pQuadsDS = NULL;
ID3D11PixelShader*          								  g_pSolidColorPS = NULL;

ID3D11InputLayout*                                            g_pSceneVertexLayout = NULL;
ID3D11RasterizerState*                                        g_pRasterizerStateWireframe = NULL;

CDXUTDialogResourceManager                                    g_DialogResourceManager;    // manager for shared resources of dialogs
CD3DSettingsDlg                                               g_D3DSettingsDlg;           // Device settings dialog
CDXUTDialog                                                   g_HUD;                      // manages the 3D   
CDXUTDialog                                                   g_SampleUI;                 // dialog for sample specific controls

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN        		1
#define IDC_TOGGLEREF               		3
#define IDC_CHANGEDEVICE            		4
#define IDC_STATIC_MESH             		5
#define IDC_COMBOBOX_MESH           		6
#define IDC_CHECKBOX_TEXTURED       		8
#define IDC_STATIC_TESS_FACTORL             9
#define IDC_SLIDER_TESS_FACTORL      		10
#define IDC_STATIC_TESS_FACTORR      		11
#define IDC_SLIDER_TESS_FACTORR      		12
#define IDC_STATIC_TESS_FACTORT      		13
#define IDC_SLIDER_TESS_FACTORT      		14
#define IDC_STATIC_TESS_FACTORB      		15
#define IDC_SLIDER_TESS_FACTORB      		16
#define IDC_STATIC_REDUCTION_MODE           17
#define IDC_COMBOBOX_REDUCTION_MODE         18
#define IDC_STATIC_SCALE      				19
#define IDC_SLIDER_SCALE      				20

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
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, 
                               LPCSTR szShaderModel, ID3DBlob** ppBlobOut, D3D10_SHADER_MACRO* pDefines );
void CreateABuffer( ID3D11Device* pd3dDevice, UINT bindFlags, UINT width, UINT cpuAccessFlag, D3D11_USAGE usage, ID3D11Buffer* &pBuffer, void* pData);

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    HRESULT hr;
    V_RETURN(DXUTSetMediaSearchPath(L"..\\Source\\ShowTessellationPatterns"));
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

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
	DXUTInit( true, true );
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"DX11 Tessellation Pattern" );
	DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, 1200, 900 );
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}


//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{

    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );
    g_SampleUI.Init( &g_DialogResourceManager );
    g_SampleUI.GetFont( 0 );

    g_HUD.SetCallback( OnGUIEvent ); 
    int iY = 30;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 23 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 23, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 23, VK_F2 );

    g_SampleUI.SetCallback( OnGUIEvent );
    iY = -40;
	g_SampleUI.AddStatic( IDC_STATIC_MESH, L"Tessellation Mode:", 0, iY, 55, 24 );
    CDXUTComboBox *pCombo;
    g_SampleUI.AddComboBox( IDC_COMBOBOX_MESH, 0, iY += 25, 140, 24, 0, true, &pCombo );
    if( pCombo )
    {
        pCombo->SetDropHeight( 45 );
        pCombo->AddItem( L"Integer", NULL );
        pCombo->AddItem( L"Pow2", NULL );
		pCombo->AddItem( L"Fractional Odd", NULL );
		pCombo->AddItem( L"Fractional Even", NULL );
        pCombo->SetSelectedByIndex( 0 );
    }
	iY+=20;
	g_SampleUI.AddStatic( IDC_STATIC_REDUCTION_MODE, L"Reduction Mode:", 0, iY, 55, 24 );
    g_SampleUI.AddComboBox( IDC_COMBOBOX_REDUCTION_MODE, 0, iY += 25, 140, 24, 0, true, &pCombo );
    if( pCombo )
    {
        pCombo->SetDropHeight( 45 );
        pCombo->AddItem( L"Max", NULL );
        pCombo->AddItem( L"Min", NULL );
		pCombo->AddItem( L"Average", NULL );
        pCombo->SetSelectedByIndex( 0 );
    }
    WCHAR szTemp[256];
	int verticalGap = 15;
    swprintf_s( szTemp, L"TF Left : %d", g_uQuadTessFactorL );
    g_SampleUI.AddStatic( IDC_STATIC_TESS_FACTORL, szTemp, 5, iY += verticalGap, 108, 24 );
	g_SampleUI.AddSlider( IDC_SLIDER_TESS_FACTORL, 0, iY += verticalGap, 140, 24, 1, MAXTESSFACTOR-1, g_uQuadTessFactorL );
    swprintf_s( szTemp, L"TF Right : %d", g_uQuadTessFactorR );
    g_SampleUI.AddStatic( IDC_STATIC_TESS_FACTORR, szTemp, 5, iY += verticalGap, 108, 24 );
	g_SampleUI.AddSlider( IDC_SLIDER_TESS_FACTORR, 0, iY += verticalGap, 140, 24, 1, MAXTESSFACTOR-1, g_uQuadTessFactorR );
	swprintf_s( szTemp, L"TF Bottom : %d", g_uQuadTessFactorB );
    g_SampleUI.AddStatic( IDC_STATIC_TESS_FACTORB, szTemp, 5, iY += verticalGap, 108, 24 );
	g_SampleUI.AddSlider( IDC_SLIDER_TESS_FACTORB, 0, iY += verticalGap, 140, 24, 1, MAXTESSFACTOR-1, g_uQuadTessFactorB );
	swprintf_s( szTemp, L"TF Top : %d", g_uQuadTessFactorT );
    g_SampleUI.AddStatic( IDC_STATIC_TESS_FACTORT, szTemp, 5, iY += verticalGap, 108, 24 );
	g_SampleUI.AddSlider( IDC_SLIDER_TESS_FACTORT, 0, iY += verticalGap, 140, 24, 1, MAXTESSFACTOR-1, g_uQuadTessFactorT );
	swprintf_s( szTemp, L"Scale : %.4f", g_scale );
    g_SampleUI.AddStatic( IDC_STATIC_SCALE, szTemp, 5, iY += verticalGap, 108, 24 );
	g_SampleUI.AddSlider( IDC_SLIDER_SCALE, 0, iY += verticalGap, 140, 24, 0, 1000, ( int )( 1000.0f * g_scale ) );
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

    // Pass all windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{

}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
    WCHAR szTemp[256];
    
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

        case IDC_SLIDER_TESS_FACTORL:
			g_uQuadTessFactorL = (unsigned int)((CDXUTSlider*)pControl)->GetValue();
            swprintf_s( szTemp, L"TF Left : %d", g_uQuadTessFactorL );
            g_SampleUI.GetStatic( IDC_STATIC_TESS_FACTORL )->SetText( szTemp );
            break;
        case IDC_SLIDER_TESS_FACTORR:
			g_uQuadTessFactorR = (unsigned int)((CDXUTSlider*)pControl)->GetValue();
            swprintf_s( szTemp, L"TF Right : %d", g_uQuadTessFactorR );
            g_SampleUI.GetStatic( IDC_STATIC_TESS_FACTORR )->SetText( szTemp );
            break;
	    case IDC_SLIDER_TESS_FACTORT:
			g_uQuadTessFactorT = (unsigned int)((CDXUTSlider*)pControl)->GetValue();
            swprintf_s( szTemp, L"TF Top : %d", g_uQuadTessFactorT );
            g_SampleUI.GetStatic( IDC_STATIC_TESS_FACTORT )->SetText( szTemp );
            break;
        case IDC_SLIDER_TESS_FACTORB:
			g_uQuadTessFactorB = (unsigned int)((CDXUTSlider*)pControl)->GetValue();
            swprintf_s( szTemp, L"TF Bottom : %d", g_uQuadTessFactorB );
            g_SampleUI.GetStatic( IDC_STATIC_TESS_FACTORB )->SetText( szTemp );
            break;
        case IDC_COMBOBOX_MESH:
            g_tessellationMode = (TESSELLATION_MODE)((CDXUTComboBox*)pControl)->GetSelectedIndex();
            break;
	    case IDC_COMBOBOX_REDUCTION_MODE:
            g_reductionMode = (REDUCTION_MODE)((CDXUTComboBox*)pControl)->GetSelectedIndex();
            break;
		case IDC_SLIDER_SCALE:
            g_scale = ( float )g_SampleUI.GetSlider( IDC_SLIDER_SCALE )->GetValue() / 1000.0f;

            WCHAR sz[100];
            swprintf_s( sz, L"Scale: %.4f", g_scale );
            g_SampleUI.GetStatic( IDC_STATIC_SCALE )->SetText( sz );
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
// Use this until D3DX11 comes online and we get some compilation helpers
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    HRESULT hr;

    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

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
    g_CmdLineParams.uWidth = 2400;
    g_CmdLineParams.uHeight = 1800;
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

  // Create the shaders
	ID3DBlob* pBlob = NULL;
    V_RETURN( CompileShaderFromFile( L"TessPattern.hlsl", "VS_RenderSceneWithTessellation", "vs_4_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreateVertexShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pSceneWithTessellationVS ) );

    // Define our scene vertex data layout
    const D3D11_INPUT_ELEMENT_DESC SceneLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    V_RETURN( pd3dDevice->CreateInputLayout( SceneLayout, ARRAYSIZE( SceneLayout ), pBlob->GetBufferPointer(),
                                                pBlob->GetBufferSize(), &g_pSceneVertexLayout ) );
    SAFE_RELEASE( pBlob );
    

	// Quads HS
    V_RETURN( CompileShaderFromFile( L"TessPattern.hlsl", "HS_Quads_fractionaleven", "hs_5_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pQuadsHS[FractionalEven] ) );
    V_RETURN( CompileShaderFromFile( L"TessPattern.hlsl", "HS_Quads_fractionalodd", "hs_5_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pQuadsHS[FractionalOdd] ) );
	V_RETURN( CompileShaderFromFile( L"TessPattern.hlsl", "HS_Quads_integer", "hs_5_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pQuadsHS[Integer] ) );
	V_RETURN( CompileShaderFromFile( L"TessPattern.hlsl", "HS_Quads_pow2", "hs_5_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pQuadsHS[Pow2] ) );
	// Triangles HS
    V_RETURN( CompileShaderFromFile( L"TessPattern.hlsl", "HS_Triangles_fractionaleven", "hs_5_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pTrianglesHS[FractionalEven] ) );
    V_RETURN( CompileShaderFromFile( L"TessPattern.hlsl", "HS_Triangles_fractionalodd", "hs_5_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pTrianglesHS[FractionalOdd] ) );
	V_RETURN( CompileShaderFromFile( L"TessPattern.hlsl", "HS_Triangles_integer", "hs_5_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pTrianglesHS[Integer] ) );
	V_RETURN( CompileShaderFromFile( L"TessPattern.hlsl", "HS_Triangles_pow2", "hs_5_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreateHullShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pTrianglesHS[Pow2] ) );
    
    // Quads DS
    V_RETURN( CompileShaderFromFile( L"TessPattern.hlsl", "DS_Quads", "ds_5_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreateDomainShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pQuadsDS ) );
    // Triangles DS
    V_RETURN( CompileShaderFromFile( L"TessPattern.hlsl", "DS_Triangles", "ds_5_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreateDomainShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pTrianglesDS ) );

    V_RETURN( CompileShaderFromFile( L"TessPattern.hlsl", "PS_SolidColor", "ps_4_0", &pBlob, NULL ) ); 
    V_RETURN( pd3dDevice->CreatePixelShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pSolidColorPS ) );   

    SAFE_RELEASE( pBlob );

	float width=20.0f; float offset=24.0f;
	Vertex *  pVerts = new Vertex[4];
	pVerts[0].m_Position = D3DXVECTOR3(-width+offset,-width,0.0f); 
	pVerts[1].m_Position = D3DXVECTOR3(width+offset,-width,0.0f); 
	pVerts[2].m_Position = D3DXVECTOR3(width+offset,width,0.0f); 
	pVerts[3].m_Position = D3DXVECTOR3(-width+offset,width,0.0f); 
    
	CreateABuffer(pd3dDevice, D3D11_BIND_VERTEX_BUFFER,  4*sizeof(Vertex), 
		0, D3D11_USAGE_DEFAULT,  pVertexBuffer, (void *)& pVerts[0]);
	delete [] pVerts;

	const unsigned int indicesQuad[4] = {0,1,2,3};
	CreateABuffer(pd3dDevice, D3D11_BIND_INDEX_BUFFER, 4* 1 *sizeof(unsigned int), 0, D3D11_USAGE_DEFAULT,  pQuadIndexBuffer, (void *)&indicesQuad[0]);
		
	pVerts = new Vertex[3];
    pVerts[0].m_Position = D3DXVECTOR3(-width-offset,-width,0.0f); 
	pVerts[1].m_Position = D3DXVECTOR3(width-offset,-width,0.0f); 
	pVerts[2].m_Position = D3DXVECTOR3(-offset,width/1.5f,0.0f); 

	CreateABuffer(pd3dDevice, D3D11_BIND_VERTEX_BUFFER,  3*sizeof(Vertex), 
		0, D3D11_USAGE_DEFAULT,  pTriVertexBuffer, (void *)& pVerts[0]);
	delete [] pVerts;

	const unsigned int indicesTri[3] = {0,1,2};
	CreateABuffer(pd3dDevice, D3D11_BIND_INDEX_BUFFER, 3* 1 *sizeof(unsigned int), 0, D3D11_USAGE_DEFAULT,  pTriIndexBuffer, (void *)&indicesTri[0]);

	D3DXVECTOR3 vecEye( 0.0f, 0.0f, -100.0f );
	D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );
	g_Camera.SetViewParams( &vecEye, &vecAt );
	// Setup the mesh params for adaptive tessellation
	   
	// Setup constant buffer
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;    
    Desc.ByteWidth = sizeof( CB );
    V_RETURN( pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcb ) );

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

    // Create solid and wireframe rasterizer state objects
    D3D11_RASTERIZER_DESC RasterDesc;
    ZeroMemory( &RasterDesc, sizeof(D3D11_RASTERIZER_DESC) );
    RasterDesc.CullMode = D3D11_CULL_NONE;  //be able to view back face
    RasterDesc.FrontCounterClockwise = FALSE;
    RasterDesc.DepthBias = 0;
    RasterDesc.DepthBiasClamp = 0.0f;
    RasterDesc.SlopeScaledDepthBias = 0.0f;
    RasterDesc.DepthClipEnable = TRUE;
    RasterDesc.ScissorEnable = FALSE;
    RasterDesc.MultisampleEnable = FALSE;
    RasterDesc.AntialiasedLineEnable = FALSE;
    RasterDesc.FillMode = D3D11_FILL_WIREFRAME;
    V_RETURN( pd3dDevice->CreateRasterizerState( &RasterDesc, &g_pRasterizerStateWireframe ) );

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

    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 5000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
    g_Camera.SetButtonMasks( MOUSE_MIDDLE_BUTTON, MOUSE_WHEEL, MOUSE_LEFT_BUTTON );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, pBackBufferSurfaceDesc->Height - 220 );
    g_SampleUI.SetSize( 150, 110 );

    return S_OK;
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

   // Get the projection & view matrix from the camera class
    D3DXMATRIXA16 mWorld;
    D3DXMATRIXA16 mView;
    D3DXMATRIXA16 mProj;
    D3DXMATRIXA16 mWorldViewProjection;
    D3DXMATRIXA16 mWorldView;
    mWorld = *g_Camera.GetWorldMatrix();
    mView = *g_Camera.GetViewMatrix();
    mProj = *g_Camera.GetProjMatrix();
    mWorldViewProjection = mWorld * mView * mProj;
    mWorldView = mWorld * mView;

    // Setup the constant buffer for the scene vertex shader
    D3D11_MAPPED_SUBRESOURCE MappedResource;
    pd3dImmediateContext->Map( g_pcb, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    CB* pCB = ( CB* )MappedResource.pData;
    D3DXMatrixTranspose( &pCB->f4x4WorldViewProjection, &mWorldViewProjection );
    pCB->fTessFactors[0] = (float)g_uQuadTessFactorL;
    pCB->fTessFactors[1] = (float)g_uQuadTessFactorB;
    pCB->fTessFactors[2] = (float)g_uQuadTessFactorR;
    pCB->fTessFactors[3] = (float)g_uQuadTessFactorT;
	pCB->fModes[0] = (float)g_reductionMode;
	pCB->fModes[1] = (float)g_scale;
	pCB->fModes[2] = 0.0f;
	pCB->fModes[3] = 0.0f;
    pd3dImmediateContext->Unmap( g_pcb, 0 );


    // Clear the render target & depth stencil
    float ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    pd3dImmediateContext->ClearRenderTargetView( DXUTGetD3D11RenderTargetView(), ClearColor );
    pd3dImmediateContext->ClearDepthStencilView( DXUTGetD3D11DepthStencilView(), D3D11_CLEAR_DEPTH, 1.0, 0 );   

    pd3dImmediateContext->RSSetState( g_pRasterizerStateWireframe );
	pd3dImmediateContext->HSSetConstantBuffers( g_iCBBind, 1, &g_pcb );
	pd3dImmediateContext->DSSetConstantBuffers( g_iCBBind, 1, &g_pcb );

	// Set the input layout
	pd3dImmediateContext->IASetInputLayout( g_pSceneVertexLayout );
	UINT Stride = sizeof( Vertex );
	UINT Offset = 0;
	

	// render the quad
	pd3dImmediateContext->IASetVertexBuffers( 0, 1, &pVertexBuffer, &Stride, &Offset );
	pd3dImmediateContext->IASetIndexBuffer( pQuadIndexBuffer, DXGI_FORMAT_R32_UINT, 0 ); 
	pd3dImmediateContext->VSSetShader( g_pSceneWithTessellationVS, NULL, 0 );
	pd3dImmediateContext->HSSetShader( g_pQuadsHS[g_tessellationMode], NULL, 0 );
	pd3dImmediateContext->DSSetShader( g_pQuadsDS, NULL, 0 );
	pd3dImmediateContext->GSSetShader( NULL, NULL, 0 );
	pd3dImmediateContext->PSSetShader( g_pSolidColorPS, NULL, 0 );
	pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST );
	pd3dImmediateContext->DrawIndexed( 4, 0, 0 );

	// render the triangle
	pd3dImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST );
	pd3dImmediateContext->IASetVertexBuffers( 0, 1, &pTriVertexBuffer, &Stride, &Offset );
	pd3dImmediateContext->IASetIndexBuffer( pTriIndexBuffer, DXGI_FORMAT_R32_UINT, 0 ); 
	pd3dImmediateContext->HSSetShader( g_pTrianglesHS[g_tessellationMode], NULL, 0 );
	pd3dImmediateContext->DSSetShader( g_pTrianglesDS, NULL, 0 );
	pd3dImmediateContext->DrawIndexed( 3, 0, 0 );
    
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

}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D10CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    //CDXUTDirectionWidget::StaticOnD3D11DestroyDevice();

    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE( g_pTxtHelper );
    SAFE_RELEASE( g_pSceneWithTessellationVS );
    SAFE_RELEASE( g_pTrianglesDS );

	for (int i=0; i<4; i++)
	{
		SAFE_RELEASE( g_pQuadsHS[i] );
		SAFE_RELEASE( g_pTrianglesHS[i] );
	}
    SAFE_RELEASE( g_pQuadsDS );
	SAFE_RELEASE( g_pSolidColorPS );

    SAFE_RELEASE( g_pSceneVertexLayout );
    SAFE_RELEASE( g_pRasterizerStateWireframe );
	SAFE_RELEASE( pQuadIndexBuffer );
	SAFE_RELEASE( pTriIndexBuffer );
	SAFE_RELEASE( pVertexBuffer );
	SAFE_RELEASE( pTriVertexBuffer );
	SAFE_RELEASE( g_pcb );

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
void CreateABuffer( ID3D11Device* pd3dDevice, UINT bindFlags, UINT width, UINT cpuAccessFlag, 
                      D3D11_USAGE usage, ID3D11Buffer* &pBuffer, void* pData)
{
    HRESULT hr = S_OK;

    D3D11_BUFFER_DESC vbDesc    = {0};
    vbDesc.BindFlags            = bindFlags; // Type of resource
    vbDesc.ByteWidth            = width;       // size in bytes
    vbDesc.CPUAccessFlags       = cpuAccessFlag;     // D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ
    vbDesc.Usage                = usage;    // D3D11_USAGE_DEFAULT, D3D11_USAGE_IMMUTABLE, D3D11_USAGE_DYNAMIC, D3D11_USAGE_STAGING

    D3D11_SUBRESOURCE_DATA vbData = {0};
    vbData.pSysMem = pData;

    hr = pd3dDevice->CreateBuffer( &vbDesc, &vbData, &pBuffer );
    assert( SUCCEEDED( hr ) );
}

