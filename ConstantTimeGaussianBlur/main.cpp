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
#include "DXUTsettingsdlg.h"
#include "SDKmisc.h"
#include "d3dx11effect.h"

#include "skybox11.h"

#include <D3DX11tex.h>
#include <D3DX11.h>
#include <D3DX11core.h>
#include <D3DX11async.h>
#include <assert.h>


CDXUTDialogResourceManager  g_DialogResourceManager;    // Manager for shared resources of dialogs
CModelViewerCamera          g_Camera;                   // A model viewing camera
CD3DSettingsDlg             g_D3DSettingsDlg;           // Device settings dialog
CDXUTDialog                 g_HUD;                      // Dialog for standard controls
CDXUTDialog                 g_SampleUI;                 // Dialog for sample specific controls
CSkybox11                   g_Skybox;
CDXUTTextHelper*            g_pTxtHelper = NULL;

// Render target texture for scene
ID3D11Texture2D*            g_pTex_Scene = NULL;
ID3D11RenderTargetView*     g_pRTV_Scene = NULL;
ID3D11ShaderResourceView*   g_pSRV_Scene = NULL;

// RW texture as intermediate and output buffer
ID3D11Texture2D*            g_pTex_Output = NULL;
ID3D11UnorderedAccessView*  g_pUAV_Output = NULL;
ID3D11ShaderResourceView*   g_pSRV_Output = NULL;

// Default render targets in the swap chain
ID3D11RenderTargetView*		g_pRTV_Default = NULL;
ID3D11DepthStencilView*		g_pDSV_Default = NULL;
D3D11_VIEWPORT				g_VP_Default[D3D11_VIEWPORT_AND_SCISSORRECT_MAX_INDEX];

// D3DX11 Effects
ID3DX11Effect*				g_pFX_GaussianCol = NULL;
ID3DX11Effect*				g_pFX_GaussianRow = NULL;
ID3DX11Effect*				g_pFX_Render = NULL;

// Background cubemap
ID3D11ShaderResourceView*	g_pSRV_Background = NULL;

// Stuff used for drawing the "full screen quad"
struct SCREEN_VERTEX
{
    D3DXVECTOR4 pos;
    D3DXVECTOR2 tex;
};
ID3D11Buffer*               g_pScreenQuadVB = NULL;
ID3D11InputLayout*          g_pQuadLayout = NULL;

// UI elements
CDXUTStatic*                g_pStaticRadius = NULL;
CDXUTStatic*                g_pStaticIteration = NULL;
CDXUTComboBox*				g_pColorModeCombo = NULL;
CDXUTSlider*                g_pRadiusSlider = NULL;
CDXUTSlider*                g_pIterationSlider = NULL;


//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN    1
#define IDC_TOGGLEREF           2
#define IDC_CHANGEDEVICE        3
#define IDC_COLORMODE			4
#define IDC_RADIUSSLIDER		5
#define IDC_ITERATIONSLIDER		6


//--------------------------------------------------------------------------------------
// Default parameter values
//--------------------------------------------------------------------------------------
UINT	g_NumApproxPasses = 3;
UINT	g_MaxApproxPasses = 8;
float	g_FilterRadius = 30;

BOOL	g_bColorBlur = FALSE;
BOOL	g_bUseD3DRefMode = FALSE;

#define SHADER_NOT_READY		0
#define SHADER_RECOMPILE		1
#define SHADER_READY			2
UINT	g_ShaderCompileStatus = SHADER_NOT_READY;

// The number of per group threads is fixed to 128 for mono mode, 256 for color mode.
// Do not change this value.
UINT	g_ThreadsPerGroup = 128;


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
HRESULT CreateResources(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc);
void ReleaseResources();

float CalculateBoxFilterWidth(float radius, int pass)
{
	// Calculate standard deviation according to cutoff width

	// We use sigma*3 as the width of filtering window
	float sigma = radius / 3.0f;

	// The width of the repeating box filter
	float box_width = sqrt(sigma * sigma * 12.0f / pass + 1);

	return box_width;
}

//--------------------------------------------------------------------------------------
// Find and compile the specified shader
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    HRESULT hr = S_OK;

    // find the file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

    DWORD dwShaderFlags = D3D10_SHADER_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3D10_SHADER_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3D10_SHADER_DEBUG;
#endif

    ID3DBlob* pErrorBlob;
    hr = D3DX11CompileFromFile( str, NULL, NULL, szEntryPoint, szShaderModel, 
        dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL );
    if( FAILED(hr) )
    {
        if( pErrorBlob != NULL )
            OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        SAFE_RELEASE( pErrorBlob );
        return hr;
    }
    SAFE_RELEASE( pErrorBlob );

    return S_OK;
}

HRESULT LoadEffectFromFile( ID3D11Device* pd3dDevice, WCHAR* szFileName, ID3DX11Effect** ppEffect )
{
    HRESULT hr = S_OK;

    // find the file
    WCHAR str[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, szFileName ) );

	// Compile the effect file
	ID3DBlob* pBlobFX = NULL;
    ID3DBlob* pErrorBlob = NULL;
	hr = D3DX11CompileFromFile(str, NULL, NULL, NULL, "fx_5_0", NULL, NULL, NULL, &pBlobFX, &pErrorBlob, NULL);
    if (FAILED(hr))
    {
        if(pErrorBlob != NULL)
            OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
        SAFE_RELEASE(pErrorBlob);
        return hr;
    }

	// Create the effect
	hr = D3DX11CreateEffectFromMemory(pBlobFX->GetBufferPointer(), pBlobFX->GetBufferSize(), 0, pd3dDevice, ppEffect);
    if( FAILED(hr) )
    {
		OutputDebugString( TEXT("Failed to load effect file.") );
        return hr;
    }

	SAFE_RELEASE(pBlobFX);
	SAFE_RELEASE(pErrorBlob);

    return S_OK;
}

HRESULT CompileGaussianFilterEffects(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
    HRESULT hr = S_OK;

	// The compute shaders for column and row filtering are created from the same
	// effect file, separated by different macro definitions.

    // Find the file
    WCHAR str[MAX_PATH];
	if (g_bColorBlur)
	{
		V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, TEXT("gaussian_color_cs.fx") ) );
	}
	else
	{
		V_RETURN( DXUTFindDXSDKMediaFileCch( str, MAX_PATH, TEXT("gaussian_mono_cs.fx") ) );
	}

	D3D10_SHADER_MACRO defines[8];

	// Software emulation mode requires different shader code
	char str_ref_mode[8];
	sprintf_s(str_ref_mode, "%u", g_bUseD3DRefMode ? 1 : 0);
	defines[0].Name = "USE_D3D_REF_MODE";
	defines[0].Definition = str_ref_mode;

	// Input image height
	UINT num_rows = pBackBufferSurfaceDesc->Height;
	char str_num_rows[8];
	sprintf_s(str_num_rows, "%u", num_rows);
	defines[1].Name = "NUM_IMAGE_ROWS";
	defines[1].Definition = str_num_rows;

	// Input image width
	UINT num_cols = pBackBufferSurfaceDesc->Width;
	char str_num_cols[8];
	sprintf_s(str_num_cols, "%u", num_cols);
	defines[2].Name = "NUM_IMAGE_COLS";
	defines[2].Definition = str_num_cols;


	// ------------------------ Column filtering shader -----------------------

	// Row scan or column scan
	defines[3].Name = "SCAN_COL_PASS";
	defines[3].Definition = "1";

	// Allocate shared memory according to the size of input image
	char str_data_length[8];
	sprintf_s(str_data_length, "%u", max(pBackBufferSurfaceDesc->Height, g_ThreadsPerGroup * 2));
	defines[4].Name = "SCAN_SMEM_SIZE";
	defines[4].Definition = str_data_length;

	// Number of texels per thread handling
	UINT texels_per_thread = (pBackBufferSurfaceDesc->Height + g_ThreadsPerGroup - 1) / g_ThreadsPerGroup;
	char str_texels_per_thread[8];
	sprintf_s(str_texels_per_thread, "%u", texels_per_thread);
	defines[5].Name = "TEXELS_PER_THREAD";
	defines[5].Definition = str_texels_per_thread;

	char str_threads_per_group[8];
	sprintf_s(str_threads_per_group, "%u", g_ThreadsPerGroup);
	defines[6].Name = "THREADS_PER_GROUP";
	defines[6].Definition = str_threads_per_group;

	defines[7].Name = NULL;
	defines[7].Definition = NULL;

	// Compile the effect file
	ID3DBlob* pBlobFX = NULL;
    ID3DBlob* pErrorBlob = NULL;
	hr = D3DX11CompileFromFile(str, defines, NULL, NULL, "fx_5_0", NULL, NULL, NULL, &pBlobFX, &pErrorBlob, NULL);
    if (FAILED(hr))
    {
        if(pErrorBlob != NULL)
            OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
        SAFE_RELEASE(pErrorBlob);
        return hr;
    }

	// Create the effect for column Gaussian filtering
	V_RETURN(D3DX11CreateEffectFromMemory(pBlobFX->GetBufferPointer(), pBlobFX->GetBufferSize(), 0, pd3dDevice, &g_pFX_GaussianCol));

	SAFE_RELEASE(pBlobFX);
	SAFE_RELEASE(pErrorBlob);


	// ------------------------- Row filtering shader -------------------------

	defines[3].Name = "SCAN_COL_PASS";
	defines[3].Definition = "0";

	// Allocate shared memory according to the size of input image
	sprintf_s(str_data_length, "%u", max(pBackBufferSurfaceDesc->Width, g_ThreadsPerGroup * 2));
	defines[4].Name = "SCAN_SMEM_SIZE";
	defines[4].Definition = str_data_length;

	// Number of texels per thread handling
	texels_per_thread = (pBackBufferSurfaceDesc->Width + g_ThreadsPerGroup - 1) / g_ThreadsPerGroup;
	sprintf_s(str_texels_per_thread, "%u", texels_per_thread);
	defines[5].Name = "TEXELS_PER_THREAD";
	defines[5].Definition = str_texels_per_thread;

	hr = D3DX11CompileFromFile(str, defines, NULL, NULL, "fx_5_0", NULL, NULL, NULL, &pBlobFX, &pErrorBlob, NULL);
    if (FAILED(hr))
    {
        if(pErrorBlob != NULL)
            OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());
        SAFE_RELEASE(pErrorBlob);
        return hr;
    }

	// Create the effect for row Gaussian filtering
	V_RETURN(D3DX11CreateEffectFromMemory(pBlobFX->GetBufferPointer(), pBlobFX->GetBufferSize(), 0, pd3dDevice, &g_pFX_GaussianRow));

	SAFE_RELEASE(pBlobFX);
	SAFE_RELEASE(pErrorBlob);


	return hr;
}

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    HRESULT hr;
    V_RETURN(DXUTSetMediaSearchPath(L"..\\Source\\ConstantTimeGaussianBlur"));
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // Disable gamma correction on this sample
    DXUTSetIsInGammaCorrectMode( false );

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
    
    //DXUTInit( true, true, L"-forceref" ); // Force create a ref device so that feature level D3D_FEATURE_LEVEL_11_0 is guaranteed
    DXUTInit( true, true );                 // Use this line instead to try to create a hardware device

	// Parse window size from command line
	int width = 1024;
	int height = 768;
	if (lpCmdLine != NULL)
		swscanf_s(lpCmdLine, L"%i %i", &width, &height);

    DXUTSetCursorSettings( true, true );    // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"Constant Time Gaussian Blur" );
    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, width, height );
    DXUTMainLoop();                         // Enter into the DXUT render loop

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

    g_HUD.SetCallback( OnGUIEvent ); int iY = 30;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 23 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 30, 170, 23, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 30, 170, 23, VK_F2 );

	iY = 0;
	g_SampleUI.AddStatic( 0, TEXT("Color Mode:"), 0, iY, 170, 23, false, NULL );
	g_SampleUI.AddComboBox( IDC_COLORMODE, 0, iY += 28, 200, 30, 1, false, &g_pColorModeCombo );
    g_pColorModeCombo->AddItem( TEXT("Grayscale"), (void*)0 );
    g_pColorModeCombo->AddItem( TEXT("Color"), (void*)1 );

	TCHAR text[64];
	swprintf_s(text, TEXT("Filter Radius: %u"), (UINT)g_FilterRadius);
    g_SampleUI.AddStatic( 0, text, 0, iY += 45, 170, 23, false, &g_pStaticRadius );
    g_SampleUI.AddSlider( IDC_RADIUSSLIDER, 0, iY += 28, 250, 23, 1, 400, (int)g_FilterRadius, false, &g_pRadiusSlider );

	swprintf_s(text, TEXT("Iteration: %u"), (UINT)g_NumApproxPasses);
    g_SampleUI.AddStatic( 0, text, 0, iY += 45, 170, 23, false, &g_pStaticIteration );
    g_SampleUI.AddSlider( IDC_ITERATIONSLIDER, 0, iY += 28, 250, 23, 1, g_MaxApproxPasses, (int)g_NumApproxPasses, false, &g_pIterationSlider );

    g_SampleUI.SetCallback( OnGUIEvent ); 
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
    if( s_bFirstTime )
    {
        s_bFirstTime = false;
        if( ( DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF ) ||
            ( DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
            pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE ) )
        {
			g_bUseD3DRefMode = TRUE;
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
	HRESULT hr = S_OK;

	// Compile the shaders if haven't.
	if (g_ShaderCompileStatus == SHADER_RECOMPILE)
	{
		SAFE_RELEASE(g_pFX_GaussianCol);
		SAFE_RELEASE(g_pFX_GaussianRow);

		V(CompileGaussianFilterEffects(DXUTGetD3D11Device(), DXUTGetDXGIBackBufferSurfaceDesc()));
		g_ShaderCompileStatus = SHADER_READY;
	}

    // Update the camera's position based on user input 
    g_Camera.FrameMove( fElapsedTime );
}

void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    g_pTxtHelper->End();
}

void RenderWaitingText()
{
    g_pTxtHelper->Begin();
	g_pTxtHelper->SetInsertionPos( DXUTGetDXGIBackBufferSurfaceDesc()->Width / 2 - 120, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( TEXT("Compiling shaders. Please wait...") );

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
    *pbNoFurtherProcessing = g_SampleUI.MsgProc( hWnd, uMsg, wParam, lParam );
    if( *pbNoFurtherProcessing )
        return 0;

    // Pass all windows messages to camera so it can respond to user input
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
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
            g_D3DSettingsDlg.SetActive( !g_D3DSettingsDlg.IsActive() ); break;

        case IDC_COLORMODE:
			{
				UINT mode = (UINT)g_SampleUI.GetComboBox(IDC_COLORMODE)->GetSelectedData();

				g_bColorBlur = (mode == 0) ? FALSE : TRUE;
				g_ThreadsPerGroup = (mode == 0) ? 128 : 256;

				// Re-create resources and shaders
				ReleaseResources();
				CreateResources(DXUTGetD3D11Device(), DXUTGetDXGIBackBufferSurfaceDesc());
				g_ShaderCompileStatus = SHADER_NOT_READY;
			}
            break;

        case IDC_RADIUSSLIDER:
			{
				int value = g_pRadiusSlider->GetValue();
				TCHAR text[64];
				swprintf_s(text, TEXT("Filter Radius: %u"), value);
				g_pStaticRadius->SetText(text);

				g_FilterRadius = (float)value;
			}
            break;

        case IDC_ITERATIONSLIDER:
			{
				int value = g_pIterationSlider->GetValue();
				TCHAR text[64];
				swprintf_s(text, TEXT("Iteration: %u"), value);
				g_pStaticIteration->SetText(text);

				g_NumApproxPasses = (UINT)value;
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
    // reject any device which doesn't support CS50
    if ( DeviceInfo->MaxLevel != D3D_FEATURE_LEVEL_11_0 )
        return false;

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

    static bool bFirstOnCreateDevice = true;

    // Warn the user that in order to support CS50, a non-hardware device has been created, continue or quit?
    if ( DXUTGetDeviceSettings().d3d11.DriverType != D3D_DRIVER_TYPE_HARDWARE && bFirstOnCreateDevice )
    {
        if ( MessageBox( 0, L"CS50 capability is missing. "\
                            L"In order to continue, a non-hardware device has been created, "\
                            L"it will be very slow, continue?", L"Warning", MB_ICONEXCLAMATION | MB_YESNO ) != IDYES )
            return E_FAIL;
    }

    bFirstOnCreateDevice = false;

	// UI elements
    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

	// Load background cubemap
    WCHAR strPath[MAX_PATH];
    V_RETURN( DXUTFindDXSDKMediaFileCch( strPath, MAX_PATH, L"Media\\GaussianBlur\\NYC_dusk.dds" ) );

    ID3D11Texture2D* pCubeTexture = NULL;
    ID3D11ShaderResourceView* pCubeRV = NULL;
    UINT SupportCaps = 0;

    pd3dDevice->CheckFormatSupport( DXGI_FORMAT_R16G16B16A16_FLOAT, &SupportCaps );
    if( SupportCaps & D3D11_FORMAT_SUPPORT_TEXTURECUBE &&
        SupportCaps & D3D11_FORMAT_SUPPORT_RENDER_TARGET &&
        SupportCaps & D3D11_FORMAT_SUPPORT_TEXTURE2D )
    {
        D3DX11_IMAGE_LOAD_INFO LoadInfo;
        LoadInfo.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        V_RETURN( D3DX11CreateShaderResourceViewFromFile( pd3dDevice, strPath, &LoadInfo, NULL, &pCubeRV, NULL ) );
        pCubeRV->GetResource( ( ID3D11Resource** )&pCubeTexture );

        V_RETURN( g_Skybox.OnD3D11CreateDevice( pd3dDevice, 50, pCubeTexture, pCubeRV ) );
    } else
        return E_FAIL;

	// Load effect
	V_RETURN(LoadEffectFromFile(pd3dDevice, TEXT("gaussian_vs_ps.fx"), &g_pFX_Render));

	// Layout for full screen quad rendering
	const D3D11_INPUT_ELEMENT_DESC quad_layout[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
 	D3DX11_PASS_DESC pass_desc;
	g_pFX_Render->GetTechniqueByName("Tech_ShowImage")->GetPassByName("Pass_ShowMonoImage")->GetDesc(&pass_desc);
	V_RETURN(pd3dDevice->CreateInputLayout(quad_layout, 2, pass_desc.pIAInputSignature, pass_desc.IAInputSignatureSize, &g_pQuadLayout));

	// Vertex buffer for full screen quad rendering
    SCREEN_VERTEX svQuad[4];
    svQuad[0].pos = D3DXVECTOR4( -1.0f, 1.0f, 0.5f, 1.0f );
    svQuad[0].tex = D3DXVECTOR2( 0.0f, 0.0f );
    svQuad[1].pos = D3DXVECTOR4( 1.0f, 1.0f, 0.5f, 1.0f );
    svQuad[1].tex = D3DXVECTOR2( 1.0f, 0.0f );
    svQuad[2].pos = D3DXVECTOR4( -1.0f, -1.0f, 0.5f, 1.0f );
    svQuad[2].tex = D3DXVECTOR2( 0.0f, 1.0f );
    svQuad[3].pos = D3DXVECTOR4( 1.0f, -1.0f, 0.5f, 1.0f );
    svQuad[3].tex = D3DXVECTOR2( 1.0f, 1.0f );

    D3D11_BUFFER_DESC vbdesc =
    {
        4 * sizeof( SCREEN_VERTEX ),
        D3D11_USAGE_IMMUTABLE,
        D3D11_BIND_VERTEX_BUFFER,
        0,
        0
    };
    D3D11_SUBRESOURCE_DATA InitData;
    InitData.pSysMem = svQuad;
    InitData.SysMemPitch = 0;
    InitData.SysMemSlicePitch = 0;
    V_RETURN( pd3dDevice->CreateBuffer( &vbdesc, &InitData, &g_pScreenQuadVB ) );

    // Setup the camera   
    D3DXVECTOR3 vecEye( 0.0f, -10.5f, -3.0f );
    D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );
    g_Camera.SetViewParams( &vecEye, &vecAt );
	g_Camera.SetWorldQuat(D3DXQUATERNION(-0.46037379f, 0.12569726f, 0.13149534f, 0.86888695f));

    return S_OK;
}

HRESULT CreateResources(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc)
{
    HRESULT hr;

	// Render target texture for scene rendering
    D3D11_TEXTURE2D_DESC tex_desc;
    D3D11_RENDER_TARGET_VIEW_DESC rtv_desc;
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
	D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc;

	ZeroMemory(&tex_desc, sizeof(D3D11_TEXTURE2D_DESC));
	ZeroMemory(&rtv_desc, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
	ZeroMemory(&srv_desc, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
	ZeroMemory(&uav_desc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));

	tex_desc.ArraySize = 1;
    tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    tex_desc.Usage = D3D11_USAGE_DEFAULT;
	tex_desc.Format = g_bColorBlur ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R32_FLOAT;
    tex_desc.Width = pBackBufferSurfaceDesc->Width;
    tex_desc.Height = pBackBufferSurfaceDesc->Height;
    tex_desc.MipLevels = 1;
    tex_desc.SampleDesc.Count = 1;
    V_RETURN(pd3dDevice->CreateTexture2D(&tex_desc, NULL, &g_pTex_Scene));

    rtv_desc.Format = tex_desc.Format;
    rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtv_desc.Texture2D.MipSlice = 0;
    V_RETURN(pd3dDevice->CreateRenderTargetView(g_pTex_Scene, &rtv_desc, &g_pRTV_Scene));

    srv_desc.Format = tex_desc.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;
    srv_desc.Texture2D.MostDetailedMip = 0;
    V_RETURN(pd3dDevice->CreateShaderResourceView(g_pTex_Scene, &srv_desc, &g_pSRV_Scene));

	// RW texture for output
    tex_desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	tex_desc.Format = g_bColorBlur ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R32_FLOAT;
    tex_desc.SampleDesc.Count = 1;
    V_RETURN(pd3dDevice->CreateTexture2D(&tex_desc, NULL, &g_pTex_Output));

	uav_desc.Format = tex_desc.Format;
	uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uav_desc.Texture2D.MipSlice = 0;
	V_RETURN(pd3dDevice->CreateUnorderedAccessView(g_pTex_Output, &uav_desc, &g_pUAV_Output));

    srv_desc.Format = tex_desc.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;
    srv_desc.Texture2D.MostDetailedMip = 0;
    V_RETURN(pd3dDevice->CreateShaderResourceView(g_pTex_Output, &srv_desc, &g_pSRV_Output));

	return S_OK;
}

HRESULT CALLBACK OnD3D11ResizedSwapChain( ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext )
{
    HRESULT hr;

	// UI elements
    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11ResizedSwapChain( pd3dDevice, pBackBufferSurfaceDesc ) );

	// Background cubemap
    g_Skybox.OnD3D11ResizedSwapChain( pBackBufferSurfaceDesc );

	// Create render targets, shader resource views and unordered access views
	CreateResources(pd3dDevice, pBackBufferSurfaceDesc);

	// Request to recompile the shaders
	g_ShaderCompileStatus = SHADER_NOT_READY;

	// Save render target and viewport
	ID3D11DeviceContext* pd3dImmediateContext = NULL;
	pd3dDevice->GetImmediateContext(&pd3dImmediateContext);
	pd3dImmediateContext->OMGetRenderTargets(1, &g_pRTV_Default, &g_pDSV_Default);
    UINT nViewPorts = 1;
    pd3dImmediateContext->RSGetViewports(&nViewPorts, g_VP_Default);
	SAFE_RELEASE(pd3dImmediateContext);

    // Setup the camera's projection parameters
    float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams( D3DX_PI / 4, fAspectRatio, 0.1f, 5000.0f );
    g_Camera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    g_HUD.SetSize( 170, 170 );
    g_SampleUI.SetLocation( pBackBufferSurfaceDesc->Width - 260, pBackBufferSurfaceDesc->Height - 240 );
    g_SampleUI.SetSize( 240, 110 );

    return S_OK;
}

void DrawFullScreenQuad(ID3D11DeviceContext* pd3dImmediateContext, UINT ScreenWidth, UINT ScreenHeight)
{
    // Setup the viewport to match the backbuffer
    D3D11_VIEWPORT vp;
    vp.Width = (float)ScreenWidth;
    vp.Height = (float)ScreenHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0.0f;
    vp.TopLeftY = 0.0f;
    pd3dImmediateContext->RSSetViewports(1, &vp);

    UINT strides = sizeof(SCREEN_VERTEX);
    UINT offsets = 0;
    ID3D11Buffer* pBuffers[1] = {g_pScreenQuadVB};

    pd3dImmediateContext->IASetInputLayout(g_pQuadLayout);
    pd3dImmediateContext->IASetVertexBuffers(0, 1, pBuffers, &strides, &offsets);
    pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    pd3dImmediateContext->Draw( 4, 0 );
}

void RestoreDefaultStates(ID3D11DeviceContext* pd3dImmediateContext)
{
	pd3dImmediateContext->ClearState();

	// Render target
	ID3D11RenderTargetView* rtv_array[1] = {g_pRTV_Default};
	pd3dImmediateContext->OMSetRenderTargets(1, rtv_array, g_pDSV_Default);

	// Viewport
	pd3dImmediateContext->RSSetViewports(1, g_VP_Default);
}

void ApplyGaussianFilter(ID3D11DeviceContext* pd3dImmediateContext)
{
    D3D11_TEXTURE2D_DESC tex_desc;
    g_pTex_Scene->GetDesc(&tex_desc);

	float box_width = CalculateBoxFilterWidth(g_FilterRadius, g_NumApproxPasses);
	float half_box_width = box_width * 0.5f;
	float frac_half_box_width = (half_box_width + 0.5f) - (int)(half_box_width + 0.5f);
	float inv_frac_half_box_width = 1.0f - frac_half_box_width;
	float rcp_box_width = 1.0f / box_width;


	// Step 1. Vertical passes: Each thread group handles a colomn in the image

	ID3DX11EffectTechnique* pTech = g_pFX_GaussianCol->GetTechniqueByName("Tech_GaussianFilter");

	// Input texture
	g_pFX_GaussianCol->GetVariableByName("g_texInput")->AsShaderResource()->SetResource(g_pSRV_Scene);
	// Output texture
	g_pFX_GaussianCol->GetVariableByName("g_rwtOutput")->AsUnorderedAccessView()->SetUnorderedAccessView(g_pUAV_Output);

	g_pFX_GaussianCol->GetVariableByName("g_NumApproxPasses")->AsScalar()->SetInt(g_NumApproxPasses - 1);
	g_pFX_GaussianCol->GetVariableByName("g_HalfBoxFilterWidth")->AsScalar()->SetFloat(half_box_width);
	g_pFX_GaussianCol->GetVariableByName("g_FracHalfBoxFilterWidth")->AsScalar()->SetFloat(frac_half_box_width);
	g_pFX_GaussianCol->GetVariableByName("g_InvFracHalfBoxFilterWidth")->AsScalar()->SetFloat(inv_frac_half_box_width);
	g_pFX_GaussianCol->GetVariableByName("g_RcpBoxFilterWidth")->AsScalar()->SetFloat(rcp_box_width);

	// Select pass
	ID3DX11EffectPass* pPass = g_bColorBlur ? pTech->GetPassByName("Pass_GaussianColor") : pTech->GetPassByName("Pass_GaussianMono");

	pPass->Apply(0, pd3dImmediateContext);
	pd3dImmediateContext->Dispatch(tex_desc.Width, 1, 1);

	// Unbound CS resource and output
	ID3D11ShaderResourceView* srv_array[] = {NULL, NULL, NULL, NULL};
	pd3dImmediateContext->CSSetShaderResources(0, 4, srv_array);
	ID3D11UnorderedAccessView* uav_array[] = {NULL, NULL, NULL, NULL};
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 4, uav_array, NULL);


	// Step 2. Horizontal passes: Each thread group handles a row in the image

	pTech = g_pFX_GaussianRow->GetTechniqueByName("Tech_GaussianFilter");

	// Input texture
	g_pFX_GaussianRow->GetVariableByName("g_texInput")->AsShaderResource()->SetResource(g_pSRV_Scene);
	// Output texture
	g_pFX_GaussianRow->GetVariableByName("g_rwtOutput")->AsUnorderedAccessView()->SetUnorderedAccessView(g_pUAV_Output);

	g_pFX_GaussianRow->GetVariableByName("g_NumApproxPasses")->AsScalar()->SetInt(g_NumApproxPasses - 1);
	g_pFX_GaussianRow->GetVariableByName("g_HalfBoxFilterWidth")->AsScalar()->SetFloat(half_box_width);
	g_pFX_GaussianRow->GetVariableByName("g_FracHalfBoxFilterWidth")->AsScalar()->SetFloat(frac_half_box_width);
	g_pFX_GaussianRow->GetVariableByName("g_InvFracHalfBoxFilterWidth")->AsScalar()->SetFloat(inv_frac_half_box_width);
	g_pFX_GaussianRow->GetVariableByName("g_RcpBoxFilterWidth")->AsScalar()->SetFloat(rcp_box_width);

	// Select pass
	pPass = g_bColorBlur ? pTech->GetPassByName("Pass_GaussianColor") : pTech->GetPassByName("Pass_GaussianMono");

	pPass->Apply(0, pd3dImmediateContext);
	pd3dImmediateContext->Dispatch(tex_desc.Height, 1, 1);

	// Unbound CS resource and output
	pd3dImmediateContext->CSSetShaderResources(0, 4, srv_array);
	pd3dImmediateContext->CSSetUnorderedAccessViews(0, 4, uav_array, NULL);
}


void ShowImage(ID3D11DeviceContext* pd3dImmediateContext)
{
	// Output to default render target
	ID3D11RenderTargetView* rtv_array[1] = {g_pRTV_Default};
    pd3dImmediateContext->OMSetRenderTargets(1, rtv_array, g_pDSV_Default);
	pd3dImmediateContext->RSSetViewports(1, g_VP_Default);

	ID3DX11EffectTechnique* pTech = g_pFX_Render->GetTechniqueByName("Tech_ShowImage");
	ID3DX11EffectPass* pPass = g_bColorBlur ? pTech->GetPassByName("Pass_ShowColorImage") : pTech->GetPassByName("Pass_ShowMonoImage");

	if (g_bColorBlur)
		g_pFX_Render->GetVariableByName("g_texColorInput")->AsShaderResource()->SetResource(g_pSRV_Output);
	else
		g_pFX_Render->GetVariableByName("g_texMonoInput")->AsShaderResource()->SetResource(g_pSRV_Output);

	UINT strides = sizeof(SCREEN_VERTEX);
    UINT offsets = 0;
    ID3D11Buffer* pBuffers[1] = {g_pScreenQuadVB};

    pd3dImmediateContext->IASetInputLayout(g_pQuadLayout);
    pd3dImmediateContext->IASetVertexBuffers(0, 1, pBuffers, &strides, &offsets);
    pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	pPass->Apply(0, pd3dImmediateContext);
    pd3dImmediateContext->Draw( 4, 0 );
}

void CALLBACK OnD3D11FrameRender(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext)
{
	// If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

	// Clear render targets
	float ClearColor[4] = {0, 0, 0, 0};
	pd3dImmediateContext->ClearRenderTargetView(g_pRTV_Default, ClearColor);
	pd3dImmediateContext->ClearDepthStencilView(g_pDSV_Default, D3D11_CLEAR_DEPTH, 1.0, 0);

	// Request to recompile the shader if it hasn't been compiled.
	if (g_ShaderCompileStatus == SHADER_NOT_READY)
	{
		RenderWaitingText();
		g_ShaderCompileStatus = SHADER_RECOMPILE;
		return;
	}

	// Set FP16x4 texture as the output RT
	ID3D11RenderTargetView* rtv_array[1];
	rtv_array[0] = g_pRTV_Scene;
	pd3dImmediateContext->OMSetRenderTargets(1, rtv_array, g_pDSV_Default);
	pd3dImmediateContext->ClearRenderTargetView(g_pRTV_Scene, ClearColor);

	// Get the projection & view matrix from the camera class
	D3DXMATRIX mWorld = *g_Camera.GetWorldMatrix();
	D3DXMATRIX mView = *g_Camera.GetViewMatrix();
	D3DXMATRIX mProj = *g_Camera.GetProjMatrix();
	D3DXMATRIX mWorldViewProjection = mWorld * mView * mProj;

	// Render background cubemap
	g_Skybox.D3D11Render(&mWorldViewProjection, g_bColorBlur, pd3dImmediateContext);

	// Resore default render target so that g_pRTV_Downscale can be unbound.
	rtv_array[0] = g_pRTV_Default;
	pd3dImmediateContext->OMSetRenderTargets(1, rtv_array, g_pDSV_Default);

	// Perform Gaussian filtering with repeated box filters
	ApplyGaussianFilter(pd3dImmediateContext);

	// Display the filtering result
	ShowImage(pd3dImmediateContext);

	// The D3D states must be restored at the end of frame. Otherwise the runtime
	// will complain unreleased resource due to D3DX11Effect.
	RestoreDefaultStates(pd3dImmediateContext);

    g_HUD.OnRender(fElapsedTime);
    g_SampleUI.OnRender(fElapsedTime);
    RenderText();
}

void ReleaseResources()
{
	SAFE_RELEASE(g_pTex_Scene);
	SAFE_RELEASE(g_pRTV_Scene);
	SAFE_RELEASE(g_pSRV_Scene);
	SAFE_RELEASE(g_pTex_Output);
	SAFE_RELEASE(g_pUAV_Output);
	SAFE_RELEASE(g_pSRV_Output);

	// The compute shader for Gaussian filtering is re-compiled every time the
	// size of input image changed.
	SAFE_RELEASE(g_pFX_GaussianCol);
	SAFE_RELEASE(g_pFX_GaussianRow);
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

    g_Skybox.OnD3D11DestroyDevice();

	ReleaseResources();

	SAFE_RELEASE(g_pFX_Render);
	SAFE_RELEASE(g_pSRV_Background);
    SAFE_RELEASE(g_pScreenQuadVB);
    SAFE_RELEASE(g_pQuadLayout);
	SAFE_RELEASE(g_pRTV_Default);
	SAFE_RELEASE(g_pDSV_Default);
}

void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();

    g_Skybox.OnD3D11ReleasingSwapChain();

	ReleaseResources();

	SAFE_RELEASE(g_pSRV_Background);
	SAFE_RELEASE(g_pRTV_Default);
	SAFE_RELEASE(g_pDSV_Default);
}
