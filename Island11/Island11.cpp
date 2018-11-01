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
#include "SDKMesh.h"
#include "d3dx11effect.h"

#include "Island11.h"

#include "terrain.h"

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------

float g_BackBufferWidth         = 1280.0f;
float g_BackBufferHeight        = 720.0f;
float g_LightPosition[3]        = {-10000.0f,6500.0f,10000.0f};

// Statistics
int g_CurrentFrame              = 0;
float g_FrameTime				= 0;
float g_TotalTime				= 0;

// Control variables
bool g_RenderWireframe          = false;
bool g_QueryStats               = false;
bool g_RenderHUD                = true;
bool g_UseDynamicLOD			= true;
bool g_RenderCaustics			= true;
bool g_FrustumCullInHS			= true;
bool g_CycleViewPoints			= false;


float g_DynamicTessellationFactor = 50.0f;
float g_StaticTessellationFactor = 12.0f;


D3DXVECTOR3 g_EyePoints[6]=  {D3DXVECTOR3(365.0f,  3.0f, 166.0f),
						D3DXVECTOR3(478.0f, 15.0f, 248.0f),
						D3DXVECTOR3(430.0f,  3.0f, 249.0f),
						D3DXVECTOR3(513.0f, 10.0f, 277.0f),
						D3DXVECTOR3(303.0f,  3.0f, 459.0f),
						D3DXVECTOR3(20.0f,  12.0f, 477.0f),

						};

D3DXVECTOR3 g_LookAtPoints[6]={D3DXVECTOR3(330.0f,-11.0f, 259.0f),
						  D3DXVECTOR3(388.0f,-16.0f, 278.0f),
						  D3DXVECTOR3(357.0f,-59.0f, 278.0f),
						  D3DXVECTOR3(438.0f,-12.0f, 289.0f),
						  D3DXVECTOR3(209.0f,-20.0f, 432.0f),
						  D3DXVECTOR3(90.0f,  -7.0f, 408.0f),
						  };


int g_NumViews=6;

D3DXVECTOR3 g_EyePoint = g_EyePoints[0];
D3DXVECTOR3 g_LookAtPoint = g_LookAtPoints[0];
D3DXVECTOR3 g_EyePointspeed = D3DXVECTOR3(0,0,0);
D3DXVECTOR3 g_LookAtPointspeed = D3DXVECTOR3(0,0,0);

CDXUTStatic*		g_pDynTessString=NULL;
CDXUTSlider*		g_pDynTessSlider=NULL;
CDXUTStatic*		g_pStatTessString=NULL;
CDXUTSlider*		g_pStatTessSlider=NULL;
D3D11_QUERY_DATA_PIPELINE_STATISTICS g_PipelineQueryData;

ID3D11Device*       g_pd3dDevice    = NULL;
ID3DX11Effect*      g_pEffect       = NULL;
CD3DSettingsDlg     g_D3DSettingsDlg;
CDXUTDialogResourceManager g_DialogResourceManager;
CDXUTDialog         g_HUD;
CDXUTTextHelper*    g_pTxtHelper    = NULL;
CFirstPersonCamera  g_Camera;
CTerrain			g_Terrain;
ID3D11Query*        g_pPipelineQuery= NULL;




//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN            1
#define IDC_TOGGLEREF                   2
#define IDC_CHANGEDEVICE                3
#define IDC_RENDERWIREFRAME             5
#define IDS_STATTESSFACTOR              6
#define IDS_DYNTESSFACTOR               7
#define IDC_QUERYSTATS					8
#define IDC_DYNAMICLOD					9
#define IDC_CAUSTICS					10
#define IDC_HSCULLING					11
#define IDC_CYCLEVIEWPOINTS				12

#define IDST_TLABEL                     999



//--------------------------------------------------------------------------------------
// Extern declarations 
//--------------------------------------------------------------------------------------
void InitApp();
void RenderText();


HRESULT CompileShaderFromFile(WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut)
{
    HRESULT hr = S_OK;

    // This DXUT helper finds the media's path
    WCHAR str[MAX_PATH];
    V_RETURN(DXUTFindDXSDKMediaFileCch(str, MAX_PATH, szFileName));

    // Compile the shader
    ID3D10Blob* pErrorBlob = NULL;
    hr = D3DX11CompileFromFile( str, NULL, NULL, szEntryPoint, szShaderModel, D3D10_SHADER_ENABLE_STRICTNESS, NULL, NULL, ppBlobOut, &pErrorBlob, NULL );
    if( FAILED(hr) )
    {
        if( pErrorBlob )
            OutputDebugStringA((char*)pErrorBlob->GetBufferPointer());

        SAFE_RELEASE(pErrorBlob);
        SAFE_RELEASE(*ppBlobOut);
    }

    return hr;
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
bool CALLBACK ModifyDeviceSettings( DXUTDeviceSettings* pDeviceSettings, void* pUserContext )
{
    pDeviceSettings->d3d11.sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	// MSAA should be applied not to the back buffer but to offscreen buffer
	// Nuking MSAA to 4x if it's >4x, to simplify manual depth resolve pass in fx file
	g_Terrain.MultiSampleCount = min(4,pDeviceSettings->d3d11.sd.SampleDesc.Count);
	g_Terrain.MultiSampleQuality = 0;
	pDeviceSettings->d3d11.sd.SampleDesc.Count = 1;
	pDeviceSettings->d3d11.sd.SampleDesc.Quality = 0;

	return true;
}


//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                      void* pUserContext )
{
    HRESULT hr;

    static bool isFirstTime = true;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pd3dDevice, pd3dImmediateContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pd3dDevice ) );
    
    WCHAR path[MAX_PATH];
    ID3DXBuffer* pShader = NULL;
	if( DXUTFindDXSDKMediaFileCch(path, MAX_PATH, L"Island11.fx") != S_OK )
	{
		V_RETURN(DXUTFindDXSDKMediaFileCch(path, MAX_PATH, L"Source\\Island11_Water_Tessellation\\Island11.fx"));
	}
    D3DXCompileShaderFromFile(path, NULL, NULL, NULL, "fx_5_0", 0, &pShader, NULL, NULL);
    V_RETURN(D3DX11CreateEffectFromMemory(pShader->GetBufferPointer(), pShader->GetBufferSize(), 0, pd3dDevice, &g_pEffect));
	SAFE_RELEASE(pShader);

	// Initialize terrain 
	g_Terrain.Initialize(pd3dDevice,g_pEffect);
	g_Terrain.LoadTextures();
	g_Terrain.BackbufferWidth=g_BackBufferWidth;
	g_Terrain.BackbufferHeight=g_BackBufferHeight;
	g_Terrain.ReCreateBuffers();

    // Set initial camera view
    if(isFirstTime)
    {
        g_Camera.SetViewParams(&g_EyePoints[0], &g_LookAtPoints[0]);
        g_Camera.SetScalers(0.005f, 50.0f);
    }

	// Create queries
    D3D11_QUERY_DESC queryDesc;
    queryDesc.Query = D3D11_QUERY_PIPELINE_STATISTICS;
    queryDesc.MiscFlags = 0;
    pd3dDevice->CreateQuery(&queryDesc, &g_pPipelineQuery);

	// Helper stuff
	g_pTxtHelper = new CDXUTTextHelper( pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15 );

    isFirstTime = false;

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

    float aspectRatio = (float)pBackBufferSurfaceDesc->Width / (float)pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams(camera_fov * D3DX_PI / 360.0f, aspectRatio, scene_z_near, scene_z_far);

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 300, 0 );
    g_HUD.SetSize( 300, 300 );

    g_BackBufferWidth = (float)pBackBufferSurfaceDesc->Width;
    g_BackBufferHeight = (float)pBackBufferSurfaceDesc->Height;
	g_Terrain.BackbufferWidth = g_BackBufferWidth;
	g_Terrain.BackbufferHeight = g_BackBufferHeight;
	g_Terrain.ReCreateBuffers();

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    if(!g_CycleViewPoints)
	{
		g_Camera.FrameMove(fElapsedTime);
	}
}




//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
                                  double fTime, float fElapsedTime, void* pUserContext )
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }


	g_TotalTime+=fElapsedTime;
	g_FrameTime=fElapsedTime;

	float viewpoints_slide_speed_factor=0.01f;
	float viewpoints_slide_speed_damp=0.97f;
	float scaled_time=(1.0f+g_TotalTime/25.0f);
	int viewpoint_index = (int)(scaled_time)%6;
	D3DXVECTOR3 predicted_camera_position;
	float dh;
	
	if(g_CycleViewPoints)
	{
		g_EyePointspeed+=(g_EyePoints[viewpoint_index]-g_EyePoint)*viewpoints_slide_speed_factor;
		g_LookAtPointspeed+=(g_LookAtPoints[viewpoint_index]-g_LookAtPoint)*viewpoints_slide_speed_factor;

		predicted_camera_position=(g_EyePoint+g_EyePointspeed*fElapsedTime*15.0f)/terrain_geometry_scale;

		dh=predicted_camera_position.y-g_Terrain.height[((int)predicted_camera_position.x)%terrain_gridpoints]
													   [((int)(terrain_gridpoints-predicted_camera_position.z))%terrain_gridpoints]-3.0f;
		if(dh<0)
		{
			g_EyePointspeed.y-=dh;
		}
		
		dh=predicted_camera_position.y-3.0f;
		if(dh<0)
		{
			g_EyePointspeed.y-=dh;
		}

		g_EyePointspeed*=viewpoints_slide_speed_damp;
		g_LookAtPointspeed*=viewpoints_slide_speed_damp;

		g_EyePoint+=g_EyePointspeed*fElapsedTime;
		g_LookAtPoint+=g_LookAtPointspeed*fElapsedTime;


		if(g_EyePoint.x<0)g_EyePoint.x=0;
		if(g_EyePoint.y<0)g_EyePoint.y=0;
		if(g_EyePoint.z<0)g_EyePoint.z=0;

		if(g_EyePoint.x>terrain_gridpoints*terrain_geometry_scale)g_EyePoint.x=terrain_gridpoints*terrain_geometry_scale;
		if(g_EyePoint.y>terrain_gridpoints*terrain_geometry_scale)g_EyePoint.y=terrain_gridpoints*terrain_geometry_scale;
		if(g_EyePoint.z>terrain_gridpoints*terrain_geometry_scale)g_EyePoint.z=terrain_gridpoints*terrain_geometry_scale;

		g_Camera.SetViewParams(&g_EyePoint, &g_LookAtPoint);
		g_Camera.FrameMove(fElapsedTime);
	}
	else
	{
		g_EyePointspeed=D3DXVECTOR3(0,0,0);
		g_LookAtPointspeed=D3DXVECTOR3(0,0,0);
	}

    D3D11_VIEWPORT defaultViewport;
    UINT viewPortsNum = 1;
    pd3dImmediateContext->RSGetViewports(&viewPortsNum, &defaultViewport);
	

	D3DXVECTOR2 WaterTexcoordShift(g_TotalTime*1.5f,g_TotalTime*0.75f);
	D3DXVECTOR2 ScreenSizeInv(1.0f/(g_BackBufferWidth*main_buffer_size_multiplier),1.0f/(g_BackBufferHeight*main_buffer_size_multiplier));

	g_pEffect->GetVariableByName("g_ZNear")->AsScalar()->SetFloat(scene_z_near);
	g_pEffect->GetVariableByName("g_ZFar")->AsScalar()->SetFloat(scene_z_far);
	g_pEffect->GetVariableByName("g_LightPosition")->AsVector()->SetFloatVector(g_LightPosition);
	g_pEffect->GetVariableByName("g_WaterBumpTexcoordShift")->AsVector()->SetFloatVector(WaterTexcoordShift);
	g_pEffect->GetVariableByName("g_ScreenSizeInv")->AsVector()->SetFloatVector(ScreenSizeInv);

	g_pEffect->GetVariableByName("g_DynamicTessFactor")->AsScalar()->SetFloat(g_DynamicTessellationFactor);
	g_pEffect->GetVariableByName("g_StaticTessFactor")->AsScalar()->SetFloat(g_StaticTessellationFactor);
	g_pEffect->GetVariableByName("g_UseDynamicLOD")->AsScalar()->SetFloat(g_UseDynamicLOD?1.0f:0.0f);
	g_pEffect->GetVariableByName("g_RenderCaustics")->AsScalar()->SetFloat(g_RenderCaustics?1.0f:0.0f);
	g_pEffect->GetVariableByName("g_FrustumCullInHS")->AsScalar()->SetFloat(g_FrustumCullInHS?1.0f:0.0f);


	if(g_QueryStats)
	{
		pd3dImmediateContext->Begin(g_pPipelineQuery);
	}

	g_Terrain.Render(&g_Camera);

	if(g_QueryStats)
	{
	    pd3dImmediateContext->End(g_pPipelineQuery);
		while(S_OK != pd3dImmediateContext->GetData(g_pPipelineQuery, &g_PipelineQueryData, sizeof(g_PipelineQueryData), 0));
	}
	
    DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"HUD / Stats");
    g_HUD.OnRender(fElapsedTime);
    RenderText();
    DXUT_EndPerfEvent();

    g_CurrentFrame++;
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain( void* pUserContext )
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}


//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11CreateDevice 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice( void* pUserContext )
{
	g_Terrain.DeInitialize();
	g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE(g_pTxtHelper);
    SAFE_RELEASE(g_pEffect);
    SAFE_RELEASE(g_pPipelineQuery);
}


//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                          bool* pbNoFurtherProcessing, void* pUserContext )
{
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
    g_Camera.HandleMessages( hWnd, uMsg, wParam, lParam );

    return 0;
}


//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent( UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext )
{
	WCHAR str[128];
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
         case IDC_RENDERWIREFRAME:
             g_RenderWireframe = !g_RenderWireframe;
             break;

         case IDS_DYNTESSFACTOR:
             g_DynamicTessellationFactor = (float)g_pDynTessSlider->GetValue();

			 wsprintf(str,L"Dynamic tessellation LOD: %d",(long)(g_DynamicTessellationFactor));
			 g_pDynTessString->SetText(str);
             break;

         case IDS_STATTESSFACTOR:
             g_StaticTessellationFactor = (float)g_pStatTessSlider->GetValue();
			 wsprintf(str,L"Static tessellation factor: %d",(long)(g_StaticTessellationFactor));
			 g_pStatTessString->SetText(str);
             break;

         case IDC_QUERYSTATS:
             g_QueryStats = !g_QueryStats;
             break;

         case IDC_DYNAMICLOD:
             g_UseDynamicLOD = !g_UseDynamicLOD;
             break;

         case IDC_CAUSTICS:
             g_RenderCaustics = !g_RenderCaustics;
			 break;

         case IDC_HSCULLING:
			 g_FrustumCullInHS = !g_FrustumCullInHS;
			 break;

         case IDC_CYCLEVIEWPOINTS:
			 g_CycleViewPoints = !g_CycleViewPoints;
             break;
	}
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if(nChar == VK_SHIFT)
    {
        if(bKeyDown)
            g_Camera.SetScalers(0.005f, 500.0f);
        else
            g_Camera.SetScalers(0.005f, 50.0f);
    }
	if(!g_CycleViewPoints)
	{
		if(nChar=='1')
		{
			g_Camera.SetViewParams(&g_EyePoints[0],&g_LookAtPoints[0]); 
		}
		if(nChar=='2')
		{
			g_Camera.SetViewParams(&g_EyePoints[1],&g_LookAtPoints[1]); 
		}
		if(nChar=='3')
		{
			g_Camera.SetViewParams(&g_EyePoints[2],&g_LookAtPoints[2]); 
		}
		if(nChar=='4')
		{
			g_Camera.SetViewParams(&g_EyePoints[3],&g_LookAtPoints[3]); 
		}
		if(nChar=='5')
		{
			g_Camera.SetViewParams(&g_EyePoints[4],&g_LookAtPoints[4]); 
		}
		if(nChar=='6')
		{
			g_Camera.SetViewParams(&g_EyePoints[5],&g_LookAtPoints[5]); 
		}
	}
}


//--------------------------------------------------------------------------------------
// Handle mouse button presses
//--------------------------------------------------------------------------------------
void CALLBACK OnMouse( bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown,
                       bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta,
                       int xPos, int yPos, void* pUserContext )
{
}


//--------------------------------------------------------------------------------------
// Call if device was removed.  Return true to find a new device, false to quit
//--------------------------------------------------------------------------------------
bool CALLBACK OnDeviceRemoved( void* pUserContext )
{
    return true;
}


//--------------------------------------------------------------------------------------
// Initialize everything and go into a render loop
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    HRESULT hr;
    V_RETURN(DXUTSetMediaSearchPath(L"..\\Source\\Island11"));
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    // DXUT will create and use the best device (either D3D9 or D3D11) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set general DXUT callbacks
    DXUTSetCallbackFrameMove( OnFrameMove );
    DXUTSetCallbackKeyboard( OnKeyboard );
    DXUTSetCallbackMouse( OnMouse );
    DXUTSetCallbackMsgProc( MsgProc );
    DXUTSetCallbackDeviceChanging( ModifyDeviceSettings );
    DXUTSetCallbackDeviceRemoved( OnDeviceRemoved );

    // Set the D3D11 DXUT callbacks. Remove these sets if the app doesn't need to support D3D11
    DXUTSetCallbackD3D11DeviceAcceptable( IsD3D11DeviceAcceptable );
    DXUTSetCallbackD3D11DeviceCreated( OnD3D11CreateDevice );
    DXUTSetCallbackD3D11SwapChainResized( OnD3D11ResizedSwapChain );
    DXUTSetCallbackD3D11FrameRender( OnD3D11FrameRender );
    DXUTSetCallbackD3D11SwapChainReleasing( OnD3D11ReleasingSwapChain );
    DXUTSetCallbackD3D11DeviceDestroyed( OnD3D11DestroyDevice );

    InitApp();

    DXUTInit( true, true, NULL );		 // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"Island11" );
    DXUTGetD3D11Enumeration(true, true);

    DXUTCreateDevice( D3D_FEATURE_LEVEL_11_0, true, (int)g_BackBufferWidth, (int)g_BackBufferHeight );
    DXUTMainLoop();

    // Perform any application-level cleanup here

    return DXUTGetExitCode();
}

//--------------------------------------------------------------------------------------
// Initialize the app 
//--------------------------------------------------------------------------------------
void InitApp()
{
    g_D3DSettingsDlg.Init( &g_DialogResourceManager );
    g_HUD.Init( &g_DialogResourceManager );

    g_HUD.SetCallback( OnGUIEvent ); int iY = 20;
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle Full Screen", 0, iY, 170, 23 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 23, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change Device (F2)", 0, iY += 26, 170, 23, VK_F2 );

    CDXUTCheckBox* pCheckBox = NULL;
    
    iY += 24;
    g_HUD.AddCheckBox(IDC_RENDERWIREFRAME, L"Render Wire(f)rame", 0, iY+=24, 140, 24, g_RenderWireframe, 'F', false, &pCheckBox);
	g_HUD.AddCheckBox(IDC_QUERYSTATS, L"Query (P)ipeline Statistics", 0, iY+=24, 140, 24, false, 'P', false, &pCheckBox);
	iY += 24;
    iY += 24;
	g_HUD.AddStatic(IDST_TLABEL,L"Dynamic Tessellation LOD: 50",0,iY+=24,140,24,true,&g_pDynTessString);
	g_HUD.AddSlider(IDS_DYNTESSFACTOR,0,iY+=24,200,24,1,100,50,true,&g_pDynTessSlider);
	g_HUD.AddStatic(IDST_TLABEL,L"Static Tessellation Factor: 12",0,iY+=24,140,24,true,&g_pStatTessString);
	g_HUD.AddSlider(IDS_STATTESSFACTOR,0,iY+=24,200,24,1,63,12,true,&g_pStatTessSlider);
	iY += 24;
 	g_HUD.AddCheckBox(IDC_DYNAMICLOD, L"Use Dynamic Tessellation (L)OD", 0, iY+=24, 140, 24, true, 'L', false, &pCheckBox);
	g_HUD.AddCheckBox(IDC_HSCULLING, L"Use Frustum Cull in (H)S", 0, iY+=24, 140, 24, true, 'H', false, &pCheckBox);
	g_HUD.AddCheckBox(IDC_CAUSTICS, L"Render Refraction (C)austics", 0, iY+=24, 140, 24, true, 'C', false, &pCheckBox);
    iY += 24;
	g_HUD.AddCheckBox(IDC_CYCLEVIEWPOINTS, L"Auto Cycle (V)iews (1,2,3,4,5,6)", 0, iY+=24, 140, 24, false, 'V', false, &pCheckBox);
}

//--------------------------------------------------------------------------------------
// Render text for the UI
//--------------------------------------------------------------------------------------
void RenderText()
{
	WCHAR info[256];
	WCHAR number_string[256];
	WCHAR number_string_with_spaces[256];
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 0.0f, 0.0f, 0.0f, 1.0f ) );
	DXUTDeviceSettings device_settings = DXUTGetDeviceSettings();
	int fps_multiplied_by_100 = (int)(DXUTGetFPS()*100.0f);

	wsprintf(info,L"%d.%02d FPS, VSync %s, %dx%d",fps_multiplied_by_100 / 100,fps_multiplied_by_100 % 100,(device_settings.d3d11.SyncInterval==0?L"OFF":L"ON"),DXUTGetWindowWidth(),DXUTGetWindowHeight());
	g_pTxtHelper->DrawTextLine( info );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );


	if(g_QueryStats)
	{
		g_pTxtHelper->DrawTextLine(L"Pipeline Stats:");

		wsprintf(number_string, L"%d", (UINT)g_PipelineQueryData.IAPrimitives);
		number_string_with_spaces[0]=NULL;
		for(int i=0;i<(int)wcslen(number_string);i++)
		{
			if((((int)wcslen(number_string)-i)%3==0)&&(i!=0)) wcsncat_s(&number_string_with_spaces[0],256,L",",1);
			wcsncat_s(&number_string_with_spaces[0],256,&number_string[i],1);
		}
		wsprintf(info, L"Input Primitives              : %s", number_string_with_spaces);
		g_pTxtHelper->DrawTextLine(info);


		wsprintf(number_string, L"%d", (UINT)g_PipelineQueryData.CInvocations);
		number_string_with_spaces[0]=NULL;
		for(int i=0;i<(int)wcslen(number_string);i++)
		{
			if((((int)wcslen(number_string)-i)%3==0)&&(i!=0)) wcsncat_s(&number_string_with_spaces[0],256,L",",1);
			wcsncat_s(&number_string_with_spaces[0],256,&number_string[i],1);
		}
		wsprintf(info, L"Primitives created            : %s", number_string_with_spaces);
		g_pTxtHelper->DrawTextLine(info);


		wsprintf(number_string, L"%d", (UINT)((float)g_PipelineQueryData.CInvocations*(1.0f/g_FrameTime)/1000000.0f));
		number_string_with_spaces[0]=NULL;
		for(int i=0;i<(int)wcslen(number_string);i++)
		{
			if((((int)wcslen(number_string)-i)%3==0)&&(i!=0)) wcsncat_s(&number_string_with_spaces[0],256,L",",1);
			wcsncat_s(&number_string_with_spaces[0],256,&number_string[i],1);
		}
		wsprintf(info, L"Primitives created / sec      : %sM", number_string_with_spaces);
		g_pTxtHelper->DrawTextLine(info);

		wsprintf(number_string, L"%d", (UINT)g_PipelineQueryData.CPrimitives);
		number_string_with_spaces[0]=NULL;
		for(int i=0;i<(int)wcslen(number_string);i++)
		{
			if((((int)wcslen(number_string)-i)%3==0)&&(i!=0)) wcsncat_s(&number_string_with_spaces[0],256,L",",1);
			wcsncat_s(&number_string_with_spaces[0],256,&number_string[i],1);
		}
		wsprintf(info, L"Primitives passed clipping    : %s", number_string_with_spaces);
		g_pTxtHelper->DrawTextLine(info);
		
		wsprintf(number_string, L"%d", (UINT)(g_PipelineQueryData.CInvocations/g_PipelineQueryData.IAPrimitives));
		number_string_with_spaces[0]=NULL;
		for(int i=0;i<(int)wcslen(number_string);i++)
		{
			if((((int)wcslen(number_string)-i)%3==0)&&(i!=0)) wcsncat_s(&number_string_with_spaces[0],256,L",",1);
			wcsncat_s(&number_string_with_spaces[0],256,&number_string[i],1);
		}
		wsprintf(info, L"Average expansion ratio       : %s", number_string_with_spaces);
		g_pTxtHelper->DrawTextLine(info);
	}
    g_pTxtHelper->End();
}
