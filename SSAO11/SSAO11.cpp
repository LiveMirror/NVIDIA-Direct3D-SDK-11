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

#include <DXUT.h>
#include <DXUTgui.h>
#include <DXUTmisc.h>
#include <DXUTCamera.h>
#include <DXUTSettingsDlg.h>
#include <SDKmisc.h>

#include "NVSDK_D3D11_SSAO/NVSSAO/NVSDK_D3D11_SSAO.h"
#include "Scene3D.h"
#include "SceneBinFile.h"

//--------------------------------------------------------------------------------------
// Scene description
//--------------------------------------------------------------------------------------

#define FOVY (40.0f * D3DX_PI / 180.0f)
#define ZNEAR 0.01f
#define ZFAR 500.0f

// Captures from the NVIDIA Medusa demo (Linear depth and back-buffer colors)
BinFileDescriptor g_BinFileDesc[] = 
{
    { L"Medusa 1",   L"..\\Media\\SSAO11\\Medusa_Warrior.bin" },   // 720p
    { L"Medusa 2",   L"..\\Media\\SSAO11\\Medusa_Snake.bin" },     // 720p
    { L"Medusa 3",   L"..\\Media\\SSAO11\\Medusa_Monster.bin" },   // 1080p
};

MeshDescriptor g_MeshDesc[] =
{
    {L"AT-AT",     L"..\\Media\\AT-AT.sdkmesh",   SCENE_USE_GROUND_PLANE, SCENE_NO_SHADING,  SCENE_USE_ORBITAL_CAMERA, 0.05f },
    {L"Leaves",    L"..\\Media\\leaves.sdkmesh",  SCENE_NO_GROUND_PLANE,  SCENE_USE_SHADING, SCENE_USE_ORBITAL_CAMERA, 0.05f },
    {L"Sibenik",   L"..\\Media\\sibenik.sdkmesh", SCENE_NO_GROUND_PLANE,  SCENE_NO_SHADING,  SCENE_USE_FIRST_PERSON_CAMERA, 0.005f },
};

#define NUM_BIN_FILES   (sizeof(g_BinFileDesc)  / sizeof(g_BinFileDesc[0]))
#define NUM_MESHES      (sizeof(g_MeshDesc)     / sizeof(g_MeshDesc[0]))

//--------------------------------------------------------------------------------------
// MSAA settings
//--------------------------------------------------------------------------------------

struct MSAADescriptor
{
    WCHAR name[32];
    int samples;
    int quality;
};

MSAADescriptor g_MSAADesc[] =
{
    {L"1x MSAA",   1, 0},
    {L"2x MSAA",   2, 0},
    {L"4x MSAA",   4, 0},
    {L"8x MSAA",   8, 0},
    {L"",          0, 0},
};

//--------------------------------------------------------------------------------------
// GUI constants
//--------------------------------------------------------------------------------------

#define MAX_RADIUS_MULT 4.0f
#define MAX_ANGLE_BIAS 60

enum
{
    IDC_TOGGLEFULLSCREEN = 1,
    IDC_TOGGLEREF,
    IDC_CHANGEDEVICE,
    IDC_CHANGESCENE,
    IDC_CHANGEMSAA,
    IDC_SHOW_COLORS,
    IDC_SHOW_AO,
    IDC_RADIUS_STATIC,
    IDC_RADIUS_SLIDER,
    IDC_STEP_SIZE_STATIC,
    IDC_STEP_SIZE_SLIDER,
    IDC_ANGLE_BIAS_STATIC,
    IDC_ANGLE_BIAS_SLIDER,
    IDC_EXPONENT_STATIC,
    IDC_EXPONENT_SLIDER,
    IDC_BLUR_SIZE_STATIC,
    IDC_BLUR_SIZE_SLIDER,
    IDC_BLUR_SHARPNESS_STATIC,
    IDC_BLUR_SHARPNESS_SLIDER,
    IDC_HALF_RES_AO_MODE,
    IDC_FULL_RES_AO_MODE,
    IDC_SHADER_TYPE_PS,
    IDC_SHADER_TYPE_CS,
};

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------

CFirstPersonCamera            g_FirstPersonCamera;      // A first-person camera
CModelViewerCamera            g_OrbitalCamera;          // An orbital viewing camera
CDXUTDialogResourceManager    g_DialogResourceManager;  // manager for shared resources of dialogs
CD3DSettingsDlg               g_SettingsDlg;            // Device settings dialog
CDXUTDialog                   g_HUD;                    // dialog for standard controls
CDXUTTextHelper*              g_pTxtHelper = NULL;
bool                          g_DrawUI = true;
bool                          g_ShowAO = true;
bool                          g_ShowColors = true;
NVSDK_D3D11_AOParameters      g_AOParams;

D3D11_VIEWPORT                g_Viewport;
UINT                          g_BackBufferWidth;
UINT                          g_BackBufferHeight;
SceneRenderer                 g_pSceneRenderer;

ID3D11Texture2D*              g_ColorTexture;
ID3D11RenderTargetView*       g_ColorRTV;
ID3D11ShaderResourceView*     g_ColorSRV;
ID3D11Texture2D*              g_NonMSAAColorTexture;
ID3D11RenderTargetView*       g_NonMSAAColorRTV;
ID3D11ShaderResourceView*     g_NonMSAAColorSRV;
ID3D11Texture2D*              g_DepthStencilTexture;
ID3D11DepthStencilView*       g_DepthStencilView;

SceneMesh g_SceneMeshes[NUM_MESHES];
SceneBinFile g_SceneBinFiles[NUM_BIN_FILES];

struct Scene
{
    Scene()
        : pMesh(NULL)
        , pBinFile(NULL)
    {
    }
    SceneMesh *pMesh;
    SceneBinFile *pBinFile;
};
Scene g_Scenes[NUM_MESHES+NUM_BIN_FILES];
bool g_UseOrbitalCamera = true;

int g_CurrentSceneId = 0;
int g_MSAACurrentSettings = 0;

//--------------------------------------------------------------------------------------
void RenderText(const NVSDK_D3D11_RenderTimesForAO &AORenderTimes, UINT AllocatedVideoMemoryBytes)
{
    g_pTxtHelper->Begin();

    g_pTxtHelper->SetInsertionPos(5, 5);
    g_pTxtHelper->SetForegroundColor(D3DXCOLOR(0.0f, 1.0f, 0.0f, 1.0f));
    g_pTxtHelper->DrawTextLine(DXUTGetFrameStats(true));
    g_pTxtHelper->DrawTextLine(DXUTGetDeviceStats());

    g_pTxtHelper->DrawFormattedTextLine(L"GPU Times (ms): Total: %0.1f (Z: %0.2f, AO: %0.2f, BlurX: %0.2f, BlurY: %0.2f, Comp: %0.2f)",
        AORenderTimes.TotalTimeMS,
        AORenderTimes.ZTimeMS,
        AORenderTimes.AOTimeMS,
        AORenderTimes.BlurXTimeMS,
        AORenderTimes.BlurYTimeMS,
        AORenderTimes.CompositeTimeMS);
    g_pTxtHelper->DrawFormattedTextLine(L"Allocated Video Memory: %d MB\n", AllocatedVideoMemoryBytes / (1024*1024));

    g_pTxtHelper->End();
}

//--------------------------------------------------------------------------------------
void InitAOParams(NVSDK_D3D11_AOParameters &AOParams)
{
    AOParams.radius = 1.0f;
    AOParams.stepSize = 4;
    AOParams.angleBias = 10.0f;
    AOParams.strength = 1.0f;
    AOParams.powerExponent = 1.0f;
    AOParams.shaderType = NVSDK_HBAO_4x8_CS;
    AOParams.resolutionMode = NVSDK_FULL_RES_AO;
    AOParams.blurRadius = 16;
    AOParams.blurSharpness = 8.0f;
    AOParams.useBlending = true;
}

//--------------------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------------------
void ReleaseRenderTargets()
{
    SAFE_RELEASE(g_ColorTexture);
    SAFE_RELEASE(g_ColorRTV);
    SAFE_RELEASE(g_ColorSRV);

    SAFE_RELEASE(g_NonMSAAColorTexture);
    SAFE_RELEASE(g_NonMSAAColorRTV);
    SAFE_RELEASE(g_NonMSAAColorSRV);
}

//--------------------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------------------
void CreateRenderTargets(ID3D11Device* pd3dDevice)
{
    ReleaseRenderTargets();

    D3D11_TEXTURE2D_DESC texDesc;
    texDesc.Width              = g_BackBufferWidth;
    texDesc.Height             = g_BackBufferHeight;
    texDesc.ArraySize          = 1;
    texDesc.MiscFlags          = 0;
    texDesc.MipLevels          = 1;
    texDesc.BindFlags          = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    texDesc.Usage              = D3D11_USAGE_DEFAULT;
    texDesc.CPUAccessFlags     = NULL;

    // Allocate MSAA color buffer
    texDesc.SampleDesc.Count   = g_MSAADesc[g_MSAACurrentSettings].samples;
    texDesc.SampleDesc.Quality = g_MSAADesc[g_MSAACurrentSettings].quality;
    texDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;

    pd3dDevice->CreateTexture2D(&texDesc, NULL, &g_ColorTexture);
    pd3dDevice->CreateRenderTargetView(g_ColorTexture, NULL, &g_ColorRTV);
    pd3dDevice->CreateShaderResourceView(g_ColorTexture, NULL, &g_ColorSRV);

    // Allocate non-MSAA color buffer
    texDesc.SampleDesc.Count   = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;

    pd3dDevice->CreateTexture2D(&texDesc, NULL, &g_NonMSAAColorTexture);
    pd3dDevice->CreateRenderTargetView(g_NonMSAAColorTexture, NULL, &g_NonMSAAColorRTV);
    pd3dDevice->CreateShaderResourceView(g_NonMSAAColorTexture, NULL, &g_NonMSAAColorSRV);
}

//--------------------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------------------
void ReleaseDepthBuffer()
{
    SAFE_RELEASE(g_DepthStencilTexture);
    SAFE_RELEASE(g_DepthStencilView);
}

//--------------------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------------------
void CreateDepthBuffer(ID3D11Device* pd3dDevice)
{
    ReleaseDepthBuffer();

    // Create a hardware-depth texture that can be fetched from a shader.
    // To do so, use a TYPELESS format and set the D3D11_BIND_SHADER_RESOURCE flag.
    // D3D10.0 did not allow creating such a depth texture with SampleCount > 1.
    // This is now possible since D3D10.1.
    D3D11_TEXTURE2D_DESC texDesc;
    texDesc.ArraySize          = 1;
    texDesc.BindFlags          = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    texDesc.CPUAccessFlags     = NULL;
    texDesc.Width              = g_BackBufferWidth;
    texDesc.Height             = g_BackBufferHeight;
    texDesc.MipLevels          = 1;
    texDesc.MiscFlags          = NULL;
    texDesc.SampleDesc.Count   = g_MSAADesc[g_MSAACurrentSettings].samples;
    texDesc.SampleDesc.Quality = g_MSAADesc[g_MSAACurrentSettings].quality;
    texDesc.Usage              = D3D11_USAGE_DEFAULT;
    texDesc.Format             = DXGI_FORMAT_R24G8_TYPELESS;
    pd3dDevice->CreateTexture2D(&texDesc, NULL, &g_DepthStencilTexture);

    // Create a depth-stencil view
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    if (g_MSAADesc[g_MSAACurrentSettings].samples > 1)
    {
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
    }
    else
    {
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    }
    dsvDesc.Texture2D.MipSlice = 0;
    dsvDesc.Flags = 0; // new in D3D11
    pd3dDevice->CreateDepthStencilView(g_DepthStencilTexture, &dsvDesc, &g_DepthStencilView);
}

//--------------------------------------------------------------------------------------
// 
//--------------------------------------------------------------------------------------
void ResizeScreenSizedBuffers(ID3D11Device* pd3dDevice)
{
    CreateRenderTargets(pd3dDevice);
    CreateDepthBuffer(pd3dDevice);
}

//--------------------------------------------------------------------------------------
// This callback function is called immediately before a device is created to allow the 
// application to modify the device settings. The supplied pDeviceSettings parameter 
// contains the settings that the framework has selected for the new device, and the 
// application can make any desired changes directly to this structure.  Note however that 
// DXUT will not correct invalid device settings so care must be taken 
// to return valid device settings, otherwise CreateDevice() will fail.  
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings, void* pUserContext)
{
    assert(pDeviceSettings->ver == DXUT_D3D11_DEVICE);
    
    // Disable vsync
    pDeviceSettings->d3d11.SyncInterval = 0;

    // Do not allocate any depth buffer in DXUT
    pDeviceSettings->d3d11.AutoCreateDepthStencil = false;

    // For the first device created if it is a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if (s_bFirstTime)
    {
        s_bFirstTime = false;
        if ((DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF) ||
            (DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
             pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE))
        {
            DXUTDisplaySwitchingToREFWarning(pDeviceSettings->ver);
        }
    }

    return true;
}


//--------------------------------------------------------------------------------------
// Reject any D3D11 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                      DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext)
{
    return true;
}

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
    HRESULT hr;

    DXUTTRACE(L"OnD3D11CreateDevice called\n");

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext(); // does not addref
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice(pd3dDevice, pd3dImmediateContext) );
    V_RETURN( g_SettingsDlg.OnD3D11CreateDevice(pd3dDevice) );
    g_pTxtHelper = new CDXUTTextHelper(pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15);

    // Setup orbital camera
    D3DXVECTOR3 vecEye(0.0f, 2.0f, 0.0f);
    D3DXVECTOR3 vecAt (0.0f, 0.0f, 0.0f);
    g_OrbitalCamera.SetViewParams(&vecEye, &vecAt);
    g_OrbitalCamera.SetRadius(1.5f, 0.01f);

    // Setup orbital camera
    D3DXVECTOR3 sibenikVecEye(0.025372846f, -0.059336402f, -0.057159711f);
    D3DXVECTOR3 sibenikVecAt (0.68971950f, -0.61976534f ,0.43737334f);
    g_FirstPersonCamera.SetViewParams(&sibenikVecEye, &sibenikVecAt);
    g_FirstPersonCamera.SetEnablePositionMovement(1);
    g_FirstPersonCamera.SetScalers(0.001f, 0.1f);

    // Load Scene3D.fx
    g_pSceneRenderer.OnCreateDevice(pd3dDevice);

    // Load the scene bin files
    int SceneId = 0;
    for (int i = 0; i < NUM_BIN_FILES; ++i)
    {
        if (FAILED(g_SceneBinFiles[i].OnCreateDevice(g_BinFileDesc[i])))
        {
            MessageBox(NULL, L"Unable to load data file", L"ERROR", MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
            PostQuitMessage(0);
            return S_FALSE;
        }
        g_Scenes[SceneId++].pBinFile = &g_SceneBinFiles[i];
    }

    // Load the sdkmesh data files
    for (int i = 0; i < NUM_MESHES; ++i)
    {
        if (FAILED(g_SceneMeshes[i].OnCreateDevice(pd3dDevice, g_MeshDesc[i])))
        {
            MessageBox(NULL, L"Unable to create mesh", L"ERROR", MB_OK|MB_SETFOREGROUND|MB_TOPMOST);
            PostQuitMessage(0);
            return S_FALSE;
        }
        g_Scenes[SceneId++].pMesh = &g_SceneMeshes[i];
    }

    return S_OK;
}

//--------------------------------------------------------------------------------------
// SwapChain has changed and may have new attributes such as size.
// Create any D3D11 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
    HRESULT hr;

    DXUTTRACE(L"OnD3D11ResizedSwapChain called\n");

    V_RETURN( g_DialogResourceManager.OnD3D11ResizedSwapChain(pd3dDevice, pBackBufferSurfaceDesc) );
    V_RETURN( g_SettingsDlg.OnD3D11ResizedSwapChain(pd3dDevice, pBackBufferSurfaceDesc) );

    g_BackBufferWidth   = pBackBufferSurfaceDesc->Width;
    g_BackBufferHeight  = pBackBufferSurfaceDesc->Height;

    // Create a viewport to match the screen size
    g_Viewport.TopLeftX = 0.0f;
    g_Viewport.TopLeftY = 0.0f;
    g_Viewport.MinDepth = 0.0f;
    g_Viewport.MaxDepth = 1.0f;
    g_Viewport.Width    = (FLOAT)g_BackBufferWidth;
    g_Viewport.Height   = (FLOAT)g_BackBufferHeight;

    // Setup the camera's projection parameters
    float AspectRatio  = (float)g_BackBufferWidth / (float)g_BackBufferHeight;
    g_OrbitalCamera.SetProjParams (FOVY, AspectRatio, ZNEAR, ZFAR);
    g_OrbitalCamera.SetWindow     (g_BackBufferWidth, g_BackBufferHeight);
    g_OrbitalCamera.SetButtonMasks(MOUSE_LEFT_BUTTON, MOUSE_WHEEL, 0);

    g_FirstPersonCamera.SetProjParams (FOVY, AspectRatio, ZNEAR, ZFAR);
    g_FirstPersonCamera.SetRotateButtons( 1, 1, 1 );

    UINT HudWidth = 256;
    float HudOpacity = 0.32f;
    g_HUD.SetLocation(g_BackBufferWidth - HudWidth, 0);
    g_HUD.SetSize    (HudWidth, g_BackBufferHeight);
    g_HUD.SetBackgroundColors(D3DCOLOR_RGBA(0,0,0,int(HudOpacity*255)));

    // Allocate our own screen-sized buffers, as the SwapChain only contains a non-MSAA color buffer.
    ResizeScreenSizedBuffers(pd3dDevice);

    // Resize the bin-file data
    for (int i = 0; i < NUM_BIN_FILES; ++i)
    {
        g_SceneBinFiles[i].OnResizedSwapChain(pd3dDevice, g_BackBufferWidth, g_BackBufferHeight);
    }

    return hr;
}

//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext)
{
    if (g_UseOrbitalCamera)
    {
        g_OrbitalCamera.FrameMove(fElapsedTime);
    }
    else
    {
        g_FirstPersonCamera.FrameMove(fElapsedTime);
    }
}

//--------------------------------------------------------------------------------------
// Callback function that renders the frame.  This function sets up the rendering 
// matrices and renders the scene and UI.
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime, 
                                 float fElapsedTime, void* pUserContext)
{
    static NVSDK_D3D11_RenderTimesForAO RenderTimes;
    static UINT32 NumBytes;

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if (g_SettingsDlg.IsActive())
    {
        g_SettingsDlg.OnRender(fElapsedTime);
        return;
    }

    // Reallocate the render targets and depth buffer if the MSAA mode has been changed in the GUI.
    int SelectedId = g_HUD.GetComboBox(IDC_CHANGEMSAA)->GetSelectedIndex();
    if (g_MSAACurrentSettings != SelectedId)
    {
        g_MSAACurrentSettings = SelectedId;
        ResizeScreenSizedBuffers(pd3dDevice);
    }

    // The step-size parameter has no effect on the pixel-shader mode
    const bool IsComputeShaderMode = (g_AOParams.shaderType == NVSDK_HBAO_4x8_CS);
    g_HUD.GetStatic(IDC_STEP_SIZE_STATIC)->SetVisible(IsComputeShaderMode);
    g_HUD.GetSlider(IDC_STEP_SIZE_SLIDER)->SetVisible(IsComputeShaderMode);

    SceneMesh *pMesh = g_Scenes[g_CurrentSceneId].pMesh;
    SceneBinFile *pBinFile = g_Scenes[g_CurrentSceneId].pBinFile;

    // The MSAA settings have no effect if the mesh is not rendered
    g_HUD.GetComboBox(IDC_CHANGEMSAA)->SetVisible(pMesh != NULL);

    pd3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pd3dImmediateContext->RSSetViewports(1, &g_Viewport);

    //--------------------------------------------------------------------------------------
    // Render colors and depths
    //--------------------------------------------------------------------------------------
    if (pMesh)
    {
        // Clear render target and depth buffer
        float BgColor[4] = { 1.0f, 1.0f, 1.0f };
        pd3dImmediateContext->ClearRenderTargetView(g_ColorRTV, BgColor);
        pd3dImmediateContext->ClearDepthStencilView(g_DepthStencilView, D3D11_CLEAR_DEPTH, 1.0, 0);

        // Render color and depth with the Scene3D class
        pd3dImmediateContext->OMSetRenderTargets(1, &g_ColorRTV, g_DepthStencilView);
        
        if (g_UseOrbitalCamera)
        {
            g_pSceneRenderer.OnFrameRender(g_OrbitalCamera.GetWorldMatrix(),
                                           g_OrbitalCamera.GetViewMatrix(),
                                           g_OrbitalCamera.GetProjMatrix(),
                                           pMesh);
        }
        else
        {
            D3DXMATRIX IdentityMatrix;
            D3DXMatrixIdentity(&IdentityMatrix);
            g_pSceneRenderer.OnFrameRender(&IdentityMatrix,
                                           g_FirstPersonCamera.GetViewMatrix(),
                                           g_FirstPersonCamera.GetProjMatrix(),
                                           pMesh);
        }

        if (g_MSAADesc[g_MSAACurrentSettings].samples > 1)
        {
            pd3dImmediateContext->ResolveSubresource(g_NonMSAAColorTexture, 0, g_ColorTexture, 0, DXGI_FORMAT_R8G8B8A8_UNORM);
        }
        else
        {
            pd3dImmediateContext->CopyResource(g_NonMSAAColorTexture, g_ColorTexture);
        }
    }

    //--------------------------------------------------------------------------------------
    // Render the SSAO to the backbuffer
    //--------------------------------------------------------------------------------------
    ID3D11RenderTargetView* pBackBufferRTV = DXUTGetD3D11RenderTargetView(); // does not addref
    pd3dImmediateContext->OMSetRenderTargets(1, &pBackBufferRTV, NULL);

    if (g_ShowColors)
    {
        // if "Show Colors" is enabled in the GUI, copy the non-MSAA colors to the back buffer
        if (pBinFile)
        {
            g_pSceneRenderer.CopyColors(pBinFile->GetColorSRV());
        }
        else
        {
            g_pSceneRenderer.CopyColors(g_NonMSAAColorSRV);
        }
    }

    if (!g_ShowColors && !g_ShowAO)
    {
        // If both "Show Colors" and "Show AO" are disabled in the GUI
        float BgColor[4] = { 0.0f, 0.0f, 0.0f };
        pd3dImmediateContext->ClearRenderTargetView(pBackBufferRTV, BgColor);
    }

    if (g_ShowAO)
    {
        NVSDK_Status status;
        if (pMesh)
        {
            status = NVSDK_D3D11_SetHardwareDepthsForAO(pd3dDevice, g_DepthStencilTexture, ZNEAR, ZFAR, g_Viewport.MinDepth, g_Viewport.MaxDepth, pMesh->GetSceneScale());
            assert(status == NVSDK_OK);
        }
        else
        {
            status = NVSDK_D3D11_SetViewSpaceDepthsForAO(pd3dDevice, pBinFile->GetLinearDepthTexture(), pBinFile->GetSceneScale());
            assert(status == NVSDK_OK);
        }

        g_AOParams.useBlending = g_ShowColors;
        status = NVSDK_D3D11_SetAOParameters(&g_AOParams);
        assert(status == NVSDK_OK);

        status = NVSDK_D3D11_RenderAO(pd3dDevice, pd3dImmediateContext, FOVY, pBackBufferRTV, NULL, &RenderTimes);
        assert(status == NVSDK_OK);

        status = NVSDK_D3D11_GetAllocatedVideoMemoryBytes(&NumBytes);
        assert(status == NVSDK_OK);
    }

    //--------------------------------------------------------------------------------------
    // Render the GUI
    //--------------------------------------------------------------------------------------
    if (g_DrawUI)
    {
        RenderText(RenderTimes, NumBytes);
        g_HUD.OnRender(fElapsedTime); 
    }
}

//--------------------------------------------------------------------------------------
// Release D3D11 resources created in OnD3D11ResizedSwapChain 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext)
{
    DXUTTRACE(L"OnD3D11ReleasingSwapChain called\n");

    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}

//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// been destroyed, which generally happens as a result of application termination or 
// windowed/full screen toggles.
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice(void* pUserContext)
{
    DXUTTRACE(L"OnD3D11DestroyDevice called\n");

    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_SettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE(g_pTxtHelper);

    g_pSceneRenderer.OnDestroyDevice();

    for (int i = 0; i < NUM_BIN_FILES; ++i)
    {
        g_SceneBinFiles[i].OnDestroyDevice();
    }

    for (int i = 0; i < NUM_MESHES; i++)
    {
        g_SceneMeshes[i].OnDestroyDevice();
    }

    ReleaseRenderTargets();
    ReleaseDepthBuffer();

    NVSDK_D3D11_ReleaseAOResources();
}

//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, 
                          bool* pbNoFurtherProcessing, void* pUserContext)
{
    // Always allow dialog resource manager calls to handle global messages
    // so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc(hWnd, uMsg, wParam, lParam);
    if (*pbNoFurtherProcessing)
    {
        return 0;
    }

    if (g_SettingsDlg.IsActive())
    {
        g_SettingsDlg.MsgProc(hWnd, uMsg, wParam, lParam);
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc(hWnd, uMsg, wParam, lParam);
    if (*pbNoFurtherProcessing)
    {
        return 0;
    }

    // Pass all windows messages to camera so it can respond to user input
    if (g_UseOrbitalCamera)
    {
        g_OrbitalCamera.HandleMessages(hWnd, uMsg, wParam, lParam);
    }
    else
    {
        g_FirstPersonCamera.HandleMessages(hWnd, uMsg, wParam, lParam);
    }

    return 0;
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext)
{
    switch (nControlID)
    {
        case IDC_TOGGLEFULLSCREEN:   DXUTToggleFullScreen(); break;
        case IDC_TOGGLEREF:          DXUTToggleREF();        break;
           
        case IDC_CHANGEDEVICE:
        {
            g_SettingsDlg.SetActive(!g_SettingsDlg.IsActive()); 
            break;
        }
        case IDC_CHANGESCENE:
        {
            CDXUTComboBox* pComboBox = (CDXUTComboBox*) pControl;
            g_CurrentSceneId = pComboBox->GetSelectedIndex();
            SceneMesh *pSceneMesh = g_Scenes[g_CurrentSceneId].pMesh;
            g_UseOrbitalCamera = pSceneMesh && pSceneMesh->UseOrbitalCamera();
            break;
        }
        case IDC_SHOW_COLORS:
        {
            g_ShowColors = g_HUD.GetCheckBox(IDC_SHOW_COLORS)->GetChecked();
            break;
        }
        case IDC_SHOW_AO:
        {
            g_ShowAO = g_HUD.GetCheckBox(IDC_SHOW_AO)->GetChecked();
            break;
        }
        case IDC_RADIUS_SLIDER: 
        {
            g_AOParams.radius = (float) g_HUD.GetSlider(IDC_RADIUS_SLIDER)->GetValue() * MAX_RADIUS_MULT / 100.0f;

            WCHAR sz[100];
            StringCchPrintf(sz, 100, L"Radius multiplier: %0.2f", g_AOParams.radius); 
            g_HUD.GetStatic(IDC_RADIUS_STATIC)->SetText(sz);

            break;
        }
        case IDC_ANGLE_BIAS_SLIDER: 
        {
            g_AOParams.angleBias = (float) g_HUD.GetSlider(IDC_ANGLE_BIAS_SLIDER)->GetValue();

            WCHAR sz[100];
            StringCchPrintf(sz, 100, L"Angle bias: %0.0f", g_AOParams.angleBias); 
            g_HUD.GetStatic(IDC_ANGLE_BIAS_STATIC)->SetText(sz);

            break;
        }
        case IDC_EXPONENT_SLIDER: 
        {
            g_AOParams.powerExponent = (float)g_HUD.GetSlider(IDC_EXPONENT_SLIDER)->GetValue() / 100.0f;

            WCHAR sz[100];
            StringCchPrintf(sz, 100, L"Power exponent: %0.2f", g_AOParams.powerExponent);
            g_HUD.GetStatic(IDC_EXPONENT_STATIC)->SetText(sz);

            break;
        }
        case IDC_STEP_SIZE_SLIDER:
        {
            g_AOParams.stepSize = g_HUD.GetSlider(IDC_STEP_SIZE_SLIDER)->GetValue();

            WCHAR sz[100];
            StringCchPrintf(sz, 100, L"Step size: %d", g_AOParams.stepSize);
            g_HUD.GetStatic(IDC_STEP_SIZE_STATIC)->SetText(sz);

            break;
        }
        case IDC_BLUR_SIZE_SLIDER:
        {
            g_AOParams.blurRadius = g_HUD.GetSlider(IDC_BLUR_SIZE_SLIDER)->GetValue();

            WCHAR sz[100];
            StringCchPrintf(sz, 100, L"Blur width: %d", 2*g_AOParams.blurRadius+1);
            g_HUD.GetStatic(IDC_BLUR_SIZE_STATIC)->SetText(sz);

            break;
        }
        case IDC_BLUR_SHARPNESS_SLIDER: 
        {
            g_AOParams.blurSharpness = (float)g_HUD.GetSlider(IDC_BLUR_SHARPNESS_SLIDER)->GetValue();

            WCHAR sz[100];
            StringCchPrintf(sz, 100, L"Blur sharpness: %0.0f", g_AOParams.blurSharpness); 
            g_HUD.GetStatic(IDC_BLUR_SHARPNESS_STATIC)->SetText(sz);

            break;
        }
        case IDC_HALF_RES_AO_MODE:
        {
            g_AOParams.resolutionMode = NVSDK_HALF_RES_AO;
            break;
        }
        case IDC_FULL_RES_AO_MODE:
        {
            g_AOParams.resolutionMode = NVSDK_FULL_RES_AO;
            break;
        }
        case IDC_SHADER_TYPE_PS:
        {
            g_AOParams.shaderType = NVSDK_HBAO_8x6_PS;
            break;
        }
        case IDC_SHADER_TYPE_CS:
        {
            g_AOParams.shaderType = NVSDK_HBAO_4x8_CS;
            break;
        }
    }
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
            case 'U':
                g_DrawUI = !g_DrawUI;
                break;
            case 'R':
                g_pSceneRenderer.RecompileShader();
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
//
//--------------------------------------------------------------------------------------
void InitGUI()
{
    // Initialize dialogs
    g_SettingsDlg.Init(&g_DialogResourceManager);
    g_HUD.Init        (&g_DialogResourceManager);

    g_HUD.SetCallback(OnGUIEvent);
    
    int iY = 10; 
    g_HUD.AddButton  (IDC_TOGGLEFULLSCREEN,   L"Toggle full screen" , 35, iY, 160, 22);
    g_HUD.AddButton  (IDC_TOGGLEREF,          L"Toggle REF (F3)"    , 35, iY += 24, 160, 22, VK_F3);
    g_HUD.AddButton  (IDC_CHANGEDEVICE,       L"Change device (F2)" , 35, iY += 24, 160, 22, VK_F2);
    iY += 20;

    g_HUD.AddCheckBox(IDC_SHOW_AO,     L"Show AO",     35, iY += 26, 125, 22, g_ShowAO);
    g_HUD.AddCheckBox(IDC_SHOW_COLORS, L"Show Colors", 35, iY += 26, 125, 22, g_ShowColors);
    iY += 20;

    CDXUTComboBox *pComboBox;
    g_HUD.AddComboBox(IDC_CHANGESCENE,    35, iY += 24, 125, 22, 'M', false, &pComboBox);
    for (int i = 0; i < NUM_BIN_FILES; i++)
    {
        pComboBox->AddItem(g_BinFileDesc[i].Name, NULL);
    }
    for (int i = 0; i < NUM_MESHES; i++)
    {
        pComboBox->AddItem(g_MeshDesc[i].Name, NULL);
    }

    g_HUD.AddComboBox(IDC_CHANGEMSAA,    35, iY += 24, 125, 22, 'N', false, &pComboBox);
    for (int i = 0; g_MSAADesc[i].samples != 0; i++)
    {
        pComboBox->AddItem(g_MSAADesc[i].name, (void*)i);
    }

    WCHAR sz[100];
    int dy = 20;
    StringCchPrintf(sz, 100, L"Radius multiplier: %0.2f", g_AOParams.radius); 
    g_HUD.AddStatic(IDC_RADIUS_STATIC, sz, 35, iY += dy + 20, 125, 22);
    g_HUD.AddSlider(IDC_RADIUS_SLIDER, 50, iY += dy, 100, 22, 0, 100, int(g_AOParams.radius / MAX_RADIUS_MULT * 100));

    StringCchPrintf(sz, 100, L"Angle bias: %0.0f", g_AOParams.angleBias); 
    g_HUD.AddStatic(IDC_ANGLE_BIAS_STATIC, sz, 35, iY += dy, 125, 22);
    g_HUD.AddSlider(IDC_ANGLE_BIAS_SLIDER, 50, iY += dy, 100, 22, 0, MAX_ANGLE_BIAS, (int)g_AOParams.angleBias);

    StringCchPrintf(sz, 100, L"Power exponent: %0.2f", g_AOParams.powerExponent); 
    g_HUD.AddStatic(IDC_EXPONENT_STATIC, sz, 35, iY += dy, 125, 22);
    g_HUD.AddSlider(IDC_EXPONENT_SLIDER, 50, iY += dy, 100, 22, 0, 200, (int)(100.0f*g_AOParams.powerExponent));

    StringCchPrintf(sz, 100, L"Step size: %d", g_AOParams.stepSize);
    g_HUD.AddStatic(IDC_STEP_SIZE_STATIC, sz, 35, iY += dy, 125, 22);
    g_HUD.AddSlider(IDC_STEP_SIZE_SLIDER, 50, iY += dy, 100, 22, 1, 8, g_AOParams.stepSize);

    StringCchPrintf(sz, 100, L"Blur width: %d", 2*g_AOParams.blurRadius+1);
    g_HUD.AddStatic(IDC_BLUR_SIZE_STATIC, sz, 35, iY += dy, 125, 22);
    g_HUD.AddSlider(IDC_BLUR_SIZE_SLIDER, 50, iY += dy, 100, 22, 0, 16, g_AOParams.blurRadius);

    StringCchPrintf(sz, 100, L"Blur sharpness: %0.0f", g_AOParams.blurSharpness); 
    g_HUD.AddStatic(IDC_BLUR_SHARPNESS_STATIC, sz, 35, iY += dy, 125, 22);
    g_HUD.AddSlider(IDC_BLUR_SHARPNESS_SLIDER, 50, iY += dy, 100, 22, 0, 16, (int)g_AOParams.blurSharpness);

    iY += 10;
    g_HUD.AddRadioButton( IDC_SHADER_TYPE_PS, 1, L"Pixel-shader HBAO",    35, iY += 24, 125, 22, (g_AOParams.shaderType == NVSDK_HBAO_8x6_PS) );
    g_HUD.AddRadioButton( IDC_SHADER_TYPE_CS, 1, L"Compute-shader HBAO",  35, iY += 24, 125, 22, (g_AOParams.shaderType == NVSDK_HBAO_4x8_CS) );

    iY += 10;
    g_HUD.AddRadioButton(IDC_HALF_RES_AO_MODE, 0, L"Half-resolution AO" , 35, iY += 24, 125, 22, (g_AOParams.resolutionMode == NVSDK_HALF_RES_AO));
    g_HUD.AddRadioButton(IDC_FULL_RES_AO_MODE, 0, L"Full-resolution AO" , 35, iY += 24, 125, 22, (g_AOParams.resolutionMode == NVSDK_FULL_RES_AO));
}

//--------------------------------------------------------------------------------------
// Initialize everything and go into a render loop
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    HRESULT hr;
    V_RETURN(DXUTSetMediaSearchPath(L"..\\Source\\SSAO11"));
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    // DXUT will create and use the best device (either D3D9 or D3D11) 
    // that is available on the system depending on which D3D callbacks are set below

    // Set general DXUT callbacks
    DXUTSetCallbackFrameMove(OnFrameMove);
    DXUTSetCallbackKeyboard(OnKeyboard);
    DXUTSetCallbackMouse(OnMouse);
    DXUTSetCallbackMsgProc(MsgProc);
    DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);
    DXUTSetCallbackDeviceRemoved(OnDeviceRemoved);

    // Set the D3D11 DXUT callbacks
    DXUTSetCallbackD3D11DeviceAcceptable  (IsD3D11DeviceAcceptable);
    DXUTSetCallbackD3D11DeviceCreated     (OnD3D11CreateDevice);
    DXUTSetCallbackD3D11SwapChainResized  (OnD3D11ResizedSwapChain);
    DXUTSetCallbackD3D11FrameRender       (OnD3D11FrameRender);
    DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11ReleasingSwapChain);
    DXUTSetCallbackD3D11DeviceDestroyed   (OnD3D11DestroyDevice);

    // Perform any application-level initialization here
    InitAOParams(g_AOParams);
    InitGUI();

    DXUTInit(true, true, NULL); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings(true, true); // Show the cursor and clip it when in full screen
    DXUTSetIsInGammaCorrectMode(false); // Do not use a SRGB back buffer for this sample
    DXUTCreateWindow(L"NVIDIA DX11 SSAO");
    DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, false, 1280, 720);

    DXUTMainLoop(); // Enter into the DXUT render loop

    // Perform any application-level cleanup here

    return DXUTGetExitCode();
}
