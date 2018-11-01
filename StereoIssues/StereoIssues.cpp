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


// WinMain is the entry point to this sample and is located at the bottom of this file.

// Disable complaints about definitions of pragma deprecated stuff in some windows headers.
#pragma warning(disable: 4995)

#include <DXUT.h>
#include <SDKmisc.h>

#include <vector>

// Scenes
#include "Scene.h"
#include "StanfordBunnyScene.h"

// Include nvstereo, but only for Dx10.
#define NO_STEREO_D3D9
#include "nvstereo.h"

// Text rendering
ID3DX10Font* gFont10 = NULL;
ID3DX10Sprite* gSprite10 = NULL;
CDXUTTextHelper* gTxtHelper = NULL;

// our global vector of all our scenes.
// Make this global because it's easier to work with DXUT this way
const int cNoActiveScene = -1;
std::vector<Scene*> gScenes;
int gActiveScene = cNoActiveScene;

// A little UI stuff
CDXUTDialogResourceManager gDialogResourceManager;
CDXUTDialog gUI;

// Our parameters for the stereo parameters texture.
nv::stereo::ParamTextureManagerD3D10* gStereoTexMgr = NULL;
ID3D10Texture2D* gStereoParamTexture = NULL;
ID3D10ShaderResourceView* gStereoParamTextureRV = NULL;

// Support going back and forth between fullscreen and windowed mode
#define IDC_TOGGLEFULLSCREEN 1

// ------------------------------------------------------------------------------------------------
void ClearRenderTargets(ID3D10Device* d3d10)
{
    // Clearing every frame is a good practice for SLI. By clearing surfaces, you indicate to the 
    // driver that information from the previous frame is not needed by the other GPU(s) involved
    // in rendering.
    float ClearColor[4] = { 0.0, 0.0, 0.5, 0.0 };

    ID3D10RenderTargetView* pRTV = DXUTGetD3D10RenderTargetView();
    d3d10->ClearRenderTargetView(pRTV, ClearColor);

    ID3D10DepthStencilView* pDSV = DXUTGetD3D10DepthStencilView();
    d3d10->ClearDepthStencilView(pDSV, D3D10_CLEAR_DEPTH, 1.0, 0);
}

// ------------------------------------------------------------------------------------------------
void RenderWorld(ID3D10Device* d3d10, double fTime, float fElapsedTime)
{
    // Give the active scene a shot at rendering the world.
    gScenes[gActiveScene]->Render(d3d10);
}

// ------------------------------------------------------------------------------------------------
void RenderText()
{
    // Render common text, then give the active scene a chance to display text information
    // about itself.
    gTxtHelper->Begin();
    gTxtHelper->SetInsertionPos(8, 8);
    gTxtHelper->SetForegroundColor(D3DXCOLOR(1.0f, 1.0f, 0.0f, 1.0f));
    gTxtHelper->DrawTextLine(DXUTGetFrameStats(true));
    gTxtHelper->DrawTextLine(DXUTGetDeviceStats());
    
    bool stereoActive = gStereoTexMgr->IsStereoActive();

    if (stereoActive) {
        gTxtHelper->DrawTextLine(L"Stereo Active");
    } else {
        gTxtHelper->SetForegroundColor(D3DXCOLOR(1.0f, 0.2f, 0.2f, 1.0f));
        gTxtHelper->DrawTextLine(L"Stereo is currently disabled--you should see no differences between techniques.");
        gTxtHelper->SetForegroundColor(D3DXCOLOR(1.0f, 0.5f, 0.0f, 1.0f));
    }

    gScenes[gActiveScene]->RenderText(gTxtHelper, stereoActive);
    gTxtHelper->End();
}

// ------------------------------------------------------------------------------------------------
void RenderCursor(ID3D10Device* d3d10)
{
    // Cursor rendering is handled by the active scene.
    // Cursors need a bit of stereo love, so the scenes handle that.
    gScenes[gActiveScene]->RenderCursor(d3d10);
}

// ------------------------------------------------------------------------------------------------
void RenderHUD(ID3D10Device* d3d10, double fTime, float fElapsedTime)
{
    RenderText();

    gUI.OnRender( fElapsedTime );

    // Render the cursor last for the obvious reason that we want it to appear above everything 
    // else
    RenderCursor(d3d10);
}

// ------------------------------------------------------------------------------------------------
HRESULT CreateStereoParamTextureAndView(ID3D10Device* d3d10)
{
    // This function creates a texture that is suitable to be stereoized by the driver.
    // Note that the parameters primarily come from nvstereo.h
    using nv::stereo::ParamTextureManagerD3D10;

    HRESULT hr = D3D_OK;

	D3D10_TEXTURE2D_DESC desc;
	desc.Width = ParamTextureManagerD3D10::Parms::StereoTexWidth;
	desc.Height = ParamTextureManagerD3D10::Parms::StereoTexHeight;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = ParamTextureManagerD3D10::Parms::StereoTexFormat;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D10_USAGE_DYNAMIC;
	desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
	desc.MiscFlags = 0;
	V_RETURN(d3d10->CreateTexture2D(&desc, NULL, &gStereoParamTexture));

    // Since we need to bind the texture to a shader input, we also need a resource view.
	D3D10_SHADER_RESOURCE_VIEW_DESC descRV;
	descRV.Format = desc.Format;
	descRV.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
	descRV.Texture2D.MipLevels = 1;
	descRV.Texture2D.MostDetailedMip = 0;
	descRV.Texture2DArray.MostDetailedMip = 0;
	descRV.Texture2DArray.MipLevels = 1;
	descRV.Texture2DArray.FirstArraySlice = 0;
	descRV.Texture2DArray.ArraySize = desc.ArraySize;
	V_RETURN(d3d10->CreateShaderResourceView(gStereoParamTexture, &descRV, &gStereoParamTextureRV));

    return hr;
}

// ------------------------------------------------------------------------------------------------
bool CALLBACK IsD3D10DeviceAcceptable(UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType,
                                      DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext)
{
    // Only accept hardware devices
    return  DeviceType == D3D10_DRIVER_TYPE_HARDWARE;
}

// ------------------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10CreateDevice(ID3D10Device* d3d10, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext) 
{
    HRESULT hr = D3D_OK;
    V_RETURN(gDialogResourceManager.OnD3D10CreateDevice(d3d10));


    V_RETURN(D3DX10CreateFont(d3d10, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                L"Verdana", &gFont10));
    V_RETURN(D3DX10CreateSprite(d3d10, 512, &gSprite10));
    gTxtHelper = new CDXUTTextHelper(NULL, NULL, gFont10, gSprite10, 15);

    // Create our stereo parameter texture
    V_RETURN(CreateStereoParamTextureAndView(d3d10));

    // Initialize the stereo texture manager. Note that the StereoTextureManager was created
    // before the device. This is important, because NvAPI_Stereo_CreateConfigurationProfileRegistryKey
    // must be called BEFORE device creation.
    gStereoTexMgr->Init(d3d10);

    // Give each of our scenes a chance to load their resources.
    for (std::vector<Scene*>::iterator it = gScenes.begin(); it != gScenes.end(); ++it) {
        (*it)->SetStereoParamRV(gStereoParamTextureRV);
        V_RETURN((*it)->LoadResources(d3d10));
    }

    return hr;
}

// ------------------------------------------------------------------------------------------------
void CALLBACK OnD3D10DestroyDevice(void* pUserContext) 
{
    // Clean everything up.
    gDialogResourceManager.OnD3D10DestroyDevice();

    for (std::vector<Scene*>::iterator it = gScenes.begin(); it != gScenes.end(); ++it) {
        (*it)->UnloadResources();
    }

    SAFE_RELEASE(gStereoParamTextureRV);
    SAFE_RELEASE(gStereoParamTexture);

    SAFE_DELETE(gStereoTexMgr);
    SAFE_RELEASE(gFont10);
    SAFE_RELEASE(gSprite10);
    SAFE_DELETE(gTxtHelper);
}

// ------------------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10ResizedSwapChain(ID3D10Device* d3d10, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext) 
{
    HRESULT hr = D3D_OK;
    V_RETURN(gDialogResourceManager.OnD3D10ResizedSwapChain(d3d10, pBackBufferSurfaceDesc));

    // All scenes need a chance to handle the resize.
    for (std::vector<Scene*>::iterator it = gScenes.begin(); it != gScenes.end(); ++it) {
        (*it)->OnD3D10ResizedSwapChain(d3d10, pSwapChain, pBackBufferSurfaceDesc);
    }

    gUI.SetLocation( pBackBufferSurfaceDesc->Width - 170, 0 );
    gUI.SetSize( 170, 170 );

    return hr;
}

// ------------------------------------------------------------------------------------------------
void CALLBACK Render(ID3D10Device* d3d10, double fTime, float fElapsedTime, void* pUserContext) 
{
    // First, we update our stereo texture
    gStereoTexMgr->UpdateStereoTexture(d3d10, gStereoParamTexture, false);
    // Then, we tell the scene what the current convergence depth is. Cursor rendering
    // uses this information to cause the cursor to draw at screen depth if the cursor 
    // isn't currently over an object in the scene.
    gScenes[gActiveScene]->SetConvergenceDepth(gStereoTexMgr->GetConvergenceDepth());

    // Then, clear our render targets, render the world and finish with the hud.
    ClearRenderTargets(d3d10);
    RenderWorld(d3d10, fTime, fElapsedTime);
    RenderHUD(d3d10, fTime, fElapsedTime);
}

// ------------------------------------------------------------------------------------------------
void CALLBACK OnD3D10ReleasingSwapChain(void* pUserContext) 
{ 
    gDialogResourceManager.OnD3D10ReleasingSwapChain();
}

// ------------------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings, void* pUserContext) 
{
    if (pDeviceSettings->ver == DXUT_D3D10_DEVICE) {
#if _DEBUG
        // Set debug if debugging. In debug, run aliased mode because 
        // otherwise DXUT deletes a target while bound, causing many, many
        // spew messages.
        pDeviceSettings->d3d10.CreateFlags |= D3D10_CREATE_DEVICE_DEBUG;
        pDeviceSettings->d3d10.sd.SampleDesc.Count = 1;
#else
        // I like AA, so turn it on here.
        pDeviceSettings->d3d10.sd.SampleDesc.Count = 4;
#endif
        pDeviceSettings->d3d10.sd.SampleDesc.Quality = 0;
    }

    return true;
}

// ------------------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                          void* pUserContext)
{
    // WM_MOUSEMOVE needs to make it to the active scene to draw the cursor in the right place.
    *pbNoFurtherProcessing = gDialogResourceManager.MsgProc(hWnd, uMsg, wParam, lParam);
    if( *pbNoFurtherProcessing && uMsg != WM_MOUSEMOVE)
        return 0;

    // Again, make sure that WM_MOUSEMOVE makes it to the active scene, otherwise the mouse will 
    // freeze when it's over our GUI elements, making it significantly less useful.
    *pbNoFurtherProcessing = gUI.MsgProc(hWnd, uMsg, wParam, lParam);
    if( *pbNoFurtherProcessing && uMsg != WM_MOUSEMOVE)
        return 0;

    // Pass all remaining windows messages to camera so it can respond to user input
    if (gActiveScene != cNoActiveScene) {
        return gScenes[gActiveScene]->HandleMessages(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}

// ------------------------------------------------------------------------------------------------
void CALLBACK Update(double fTime, float fElapsedTime, void* pUserContext) 
{ 
    // Only the active scene gets the update
    gScenes[gActiveScene]->Update(fTime, fElapsedTime);
}

// ------------------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext)
{
    switch( nControlID )
    {
        case IDC_TOGGLEFULLSCREEN:
            DXUTToggleFullScreen(); 
            break;
        default:
            // All other messages need to be given to all scenes, in case they need to handle
            // them.
            for (std::vector<Scene*>::iterator it = gScenes.begin(); it != gScenes.end(); ++it) {
                (*it)->OnGUIEvent(nEvent, nControlID, pControl, pUserContext);
            }
    } 
}

// ------------------------------------------------------------------------------------------------
void InitUI()
{
    gUI.Init(&gDialogResourceManager);

    int y = 10;

    gUI.SetCallback(OnGUIEvent); 
    gUI.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, y, 125, 22 );
    y += 24;

    int startId = IDC_TOGGLEFULLSCREEN + 1;
    for (std::vector<Scene*>::iterator it = gScenes.begin(); it != gScenes.end(); ++it) {
        (*it)->InitUI(gUI, 35, y, startId);
    }
}

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
INT WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, INT)
{    
    HRESULT hr;
    V_RETURN(DXUTSetMediaSearchPath(L"..\\Source\\StereoIssues"));
    // Set Direct3D 10 callbacks. This sample won't work with Dx9, so don't bother setting any of 
    // those.
    DXUTSetCallbackD3D10DeviceAcceptable(IsD3D10DeviceAcceptable);
    DXUTSetCallbackD3D10DeviceCreated(OnD3D10CreateDevice);
    DXUTSetCallbackD3D10SwapChainResized(OnD3D10ResizedSwapChain);
    DXUTSetCallbackD3D10FrameRender(Render);
    DXUTSetCallbackD3D10SwapChainReleasing(OnD3D10ReleasingSwapChain);
    DXUTSetCallbackD3D10DeviceDestroyed(OnD3D10DestroyDevice);
    
    // Set the generic callback functions
    DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);
    DXUTSetCallbackMsgProc(MsgProc);
    DXUTSetCallbackFrameMove(Update);

    // We'll deal with our own cursor in fullscreen mode.
    DXUTSetCursorSettings(false, false);

    // Needs to be called before any other NVAPI functions, so before device creation is fine.
    NvAPI_Initialize();

    // ParamTextureManager must be created before the device to give our settings-loading code a chance to fire.
    gStereoTexMgr = new nv::stereo::ParamTextureManagerD3D10;

    gScenes.push_back(new StanfordBunnyScene);
    gActiveScene = 0;
    
    InitUI();

    // Initialize DXUT and create the desired Win32 window and Direct3D device for the application    
    DXUTInit(true, true); 

    // Stereo is a Vista/Win7 feature for Dx9 and Dx10. One of the restrictions is that it only works
    // in fullscreen mode, but it's useful to debug and place breakpoints in windowed mode.
    // In any event, there is a button in the sample to toggle to fullscreen if needed, this only
    // controls the starting state of the app.
    bool windowed = false;
#ifdef _DEBUG
    windowed = true;
#endif

    DXUTCreateWindow(L"Stereo Issues");
    DXUTCreateDevice(windowed);

    // Start the render loop
    DXUTMainLoop();
    int retval = DXUTGetExitCode();

    // Clean everything up
    DXUTShutdown(retval);

    // We no longer have an active scene.
    gActiveScene = cNoActiveScene;

    // No memory leaks!
    for (std::vector<Scene*>::iterator it = gScenes.begin(); it != gScenes.end(); ++it) {
        SAFE_DELETE(*it);
    }
    
    return retval;
}
