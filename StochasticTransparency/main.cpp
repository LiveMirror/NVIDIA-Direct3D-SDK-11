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
#include "DualDepthPeeling11.h"
#include "StochasticTransparency11.h"
#include "PlainAlphaBlending.h"
#include <strsafe.h>

typedef struct
{
    WCHAR* pName;
    BaseTechnique *pEngine;
} TechniqueUI;

enum
{
    STOCHASTIC_TRANSPARENCY,
    DUAL_DEPTH_PEELING,
    PLAIN_ALPHA_BLENDING,
    NUM_TECHNIQUES
};

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------

CDXUTDialogResourceManager  g_DialogResourceManager;    // manager for shared resources of dialogs
CModelViewerCamera          g_Camera;                   // A model viewing camera
CD3DSettingsDlg             g_D3DSettingsDlg;           // Device settings dialog
CDXUTDialog                 g_HUD;                      // dialog for standard controls
CDXUTDialog                 g_SampleUI;                 // dialog for sample specific controls
CDXUTTextHelper*            g_pTxtHelper = NULL;
bool                        g_ShowUI = true;
D3DXVECTOR3                 g_ModelOffset;

TechniqueUI                 g_Techniques[NUM_TECHNIQUES];
StochasticTransparency      *g_pStochasticTransparency = NULL;
DualDepthPeeling            *g_pDualDepthPeeling = NULL;
PlainAlphaBlending          *g_pPlainAlphaBlending = NULL;
BaseTechnique               *g_pCurrentEngine = NULL;

UINT                        BaseTechnique::m_NumGeomPasses;
float                       BaseTechnique::m_Alpha;
CDXUTSDKMesh                Scene::m_Mesh;

//--------------------------------------------------------------------------------------
// Defines
//--------------------------------------------------------------------------------------

#define IMAGE_WIDTH 1024
#define IMAGE_HEIGHT 1024

#define ZNEAR 0.1f
#define ZFAR 100.0f

#define MAX_NUM_PEELING_PASSES 8
#define NUM_PEELING_PASSES 4

#define MAX_NUM_STOCHASTIC_PASSES 8
#define NUM_STOCHASTIC_PASSES 1

#define AUTO_ROTATION_RATE 0.05f
#define WORLD_OFFSET 0.01f

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
enum {
    IDC_TOGGLEFULLSCREEN = 1,
    IDC_TOGGLEREF,
    IDC_CHANGEDEVICE,
    IDC_USE_STOCHASTIC_TRANSPARENCY,
    IDC_USE_DUAL_DEPTH_PEELING,
    IDC_USE_PLAIN_ALPHA_BLENDING,
    IDC_NUM_PEELING_PASSES_STATIC,
    IDC_NUM_PEELING_PASSES_SLIDER,
    IDC_NUM_STOCHASTIC_PASSES_STATIC,
    IDC_NUM_STOCHASTIC_PASSES_SLIDER,
    IDC_ALPHA_STATIC,
    IDC_ALPHA_SLIDER,
    IDC_AUTO_ROTATE
};

//--------------------------------------------------------------------------------------
// Forward declarations 
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings* pDeviceSettings, void* pUserContext);
void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext);
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                         void* pUserContext);
void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext);

bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                      DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext);
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext);
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext);
void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext);
void CALLBACK OnD3D11DestroyDevice(void* pUserContext);
void CALLBACK OnD3D11FrameRender(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext);

void InitGUI();
void RenderText();

//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard(UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext)
{
    if(bKeyDown)
    {
        switch(nChar)
        {
        case VK_ESCAPE:
            PostQuitMessage(0);
            break;
        case 'U':
            g_ShowUI = !g_ShowUI;
            break;
        case 'A':
            g_ModelOffset[0] -= WORLD_OFFSET;
            break;
        case 'D':
            g_ModelOffset[0] += WORLD_OFFSET;
            break;
        case 'W':
            g_ModelOffset[1] += WORLD_OFFSET;
            break;
        case 'S':
            g_ModelOffset[1] -= WORLD_OFFSET;
            break;
        case 'Q':
            g_ModelOffset[2] -= WORLD_OFFSET;
            break;
        case 'E':
            g_ModelOffset[2] += WORLD_OFFSET;
            break;
        }
    }
}

//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    HRESULT hr;
    V_RETURN(DXUTSetMediaSearchPath(L"..\\Source\\StochasticTransparency"));
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    // Disable gamma correction on this sample
    DXUTSetIsInGammaCorrectMode(false);

    DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);
    DXUTSetCallbackMsgProc(MsgProc);
    DXUTSetCallbackKeyboard(OnKeyboard);
    DXUTSetCallbackFrameMove(OnFrameMove);
    
    DXUTSetCallbackD3D11DeviceAcceptable(IsD3D11DeviceAcceptable);
    DXUTSetCallbackD3D11DeviceCreated(OnD3D11CreateDevice);
    DXUTSetCallbackD3D11SwapChainResized(OnD3D11ResizedSwapChain);
    DXUTSetCallbackD3D11FrameRender(OnD3D11FrameRender);
    DXUTSetCallbackD3D11SwapChainReleasing(OnD3D11ReleasingSwapChain);
    DXUTSetCallbackD3D11DeviceDestroyed(OnD3D11DestroyDevice);

    InitGUI();

    DXUTInit(true, true, NULL); // Parse the command line, show msgboxes on error
    DXUTSetCursorSettings(true, true); // Show the cursor and clip it when in full screen
    DXUTCreateWindow(L"Stochastic Transparency");
    DXUTCreateDevice(D3D_FEATURE_LEVEL_11_0, true, IMAGE_WIDTH, IMAGE_HEIGHT);
    DXUTMainLoop(); // Enter into the DXUT render loop

    return DXUTGetExitCode();
}

//--------------------------------------------------------------------------------------
void InitGUI()
{
    g_D3DSettingsDlg.Init(&g_DialogResourceManager);
    g_HUD.Init(&g_DialogResourceManager);
    g_HUD.SetCallback(OnGUIEvent);
    
    int iY = 20;
    g_HUD.AddButton(IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 23);
    g_HUD.AddButton(IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 23, VK_F3);
    g_HUD.AddButton(IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 23, VK_F2);

    g_SampleUI.Init(&g_DialogResourceManager);
    g_SampleUI.SetCallback(OnGUIEvent); 

    iY = 0;
    g_SampleUI.AddRadioButton(IDC_USE_STOCHASTIC_TRANSPARENCY,     0, L"Stochastic Transparency" , 10, iY += 24, 125, 22, true);
    g_SampleUI.AddRadioButton(IDC_USE_DUAL_DEPTH_PEELING,          0, L"Dual Depth Peeling" , 10, iY += 24, 125, 22, false);
    g_SampleUI.AddRadioButton(IDC_USE_PLAIN_ALPHA_BLENDING,        0, L"Plain Alpha Blending" , 10, iY += 24, 125, 22, false);

    iY += 40;
    g_SampleUI.AddStatic(IDC_NUM_PEELING_PASSES_STATIC, L"", 30, iY, 125, 22);
    g_SampleUI.AddStatic(IDC_NUM_STOCHASTIC_PASSES_STATIC, L"", 30, iY, 125, 22);

    iY += 24;
    g_SampleUI.AddSlider(IDC_NUM_PEELING_PASSES_SLIDER, 50, iY, 100, 22, 1, MAX_NUM_PEELING_PASSES, NUM_PEELING_PASSES);
    g_SampleUI.AddSlider(IDC_NUM_STOCHASTIC_PASSES_SLIDER, 50, iY, 100, 22, 1, MAX_NUM_STOCHASTIC_PASSES, NUM_STOCHASTIC_PASSES);

    g_SampleUI.AddStatic(IDC_ALPHA_STATIC, L"", 30, iY += 24, 125, 22);
    g_SampleUI.AddSlider(IDC_ALPHA_SLIDER, 50, iY += 24, 100, 22, 0, 100, 60);

    g_SampleUI.AddCheckBox(IDC_AUTO_ROTATE, L"Auto Rotate", 35, iY += 26, 125, 22, false);
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

    // For the first device created if it is a REF device, optionally display a warning dialog box
    static bool s_bFirstTime = true;
    if(s_bFirstTime)
    {
        s_bFirstTime = false;
        if((DXUT_D3D9_DEVICE == pDeviceSettings->ver && pDeviceSettings->d3d9.DeviceType == D3DDEVTYPE_REF) ||
           (DXUT_D3D11_DEVICE == pDeviceSettings->ver &&
            pDeviceSettings->d3d11.DriverType == D3D_DRIVER_TYPE_REFERENCE))
        {
            DXUTDisplaySwitchingToREFWarning(pDeviceSettings->ver);
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
void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void* pUserContext)
{
    // Update the camera's position based on user input 
    g_Camera.FrameMove(fElapsedTime);
}

//--------------------------------------------------------------------------------------
// Render text for the UI
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos(2, 0);
    g_pTxtHelper->SetForegroundColor(D3DXCOLOR(0.0f, 0.0f, 0.0f, 1.0f));
    g_pTxtHelper->DrawTextLine(DXUTGetFrameStats(DXUTIsVsyncEnabled()));
    g_pTxtHelper->DrawTextLine(DXUTGetDeviceStats());

    g_pTxtHelper->End();
}

//--------------------------------------------------------------------------------------
// Before handling window messages, DXUT passes incoming windows 
// messages to the application through this callback function. If the application sets 
// *pbNoFurtherProcessing to TRUE, then DXUT will not process this message.
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, bool* pbNoFurtherProcessing,
                         void* pUserContext)
{
    // Pass messages to dialog resource manager calls so GUI state is updated correctly
    *pbNoFurtherProcessing = g_DialogResourceManager.MsgProc(hWnd, uMsg, wParam, lParam);
    if(*pbNoFurtherProcessing)
        return 0;

    // Pass messages to settings dialog if its active
    if(g_D3DSettingsDlg.IsActive())
    {
        g_D3DSettingsDlg.MsgProc(hWnd, uMsg, wParam, lParam);
        return 0;
    }

    // Give the dialogs a chance to handle the message first
    *pbNoFurtherProcessing = g_HUD.MsgProc(hWnd, uMsg, wParam, lParam);
    if(*pbNoFurtherProcessing)
        return 0;
    *pbNoFurtherProcessing = g_SampleUI.MsgProc(hWnd, uMsg, wParam, lParam);
    if(*pbNoFurtherProcessing)
        return 0;

    // Pass all windows messages to camera so it can respond to user input
    g_Camera.HandleMessages(hWnd, uMsg, wParam, lParam);

    return 0;
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext)
{
    switch(nControlID)
    {
        case IDC_TOGGLEFULLSCREEN:
        {
            DXUTToggleFullScreen();
            break;
        }
        case IDC_TOGGLEREF:
        {
            DXUTToggleREF();
            break;
        }
        case IDC_CHANGEDEVICE:
        {
            g_D3DSettingsDlg.SetActive(!g_D3DSettingsDlg.IsActive());
            break;
        }
        case IDC_USE_STOCHASTIC_TRANSPARENCY:
        {
            g_pCurrentEngine = g_Techniques[STOCHASTIC_TRANSPARENCY].pEngine;
            break;
        }
        case IDC_USE_DUAL_DEPTH_PEELING:
        {
            g_pCurrentEngine = g_Techniques[DUAL_DEPTH_PEELING].pEngine;
            break;
        }
        case IDC_USE_PLAIN_ALPHA_BLENDING:
        {
            g_pCurrentEngine = g_Techniques[PLAIN_ALPHA_BLENDING].pEngine;
            break;
        }
    }
}

bool CALLBACK IsD3D11DeviceAcceptable(const CD3D11EnumAdapterInfo *AdapterInfo, UINT Output, const CD3D11EnumDeviceInfo *DeviceInfo,
                                      DXGI_FORMAT BackBufferFormat, bool bWindowed, void* pUserContext)
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
HRESULT CALLBACK OnD3D11CreateDevice(ID3D11Device* pd3dDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext)
{
    HRESULT hr;

    ID3D11DeviceContext* pd3dImmediateContext = DXUTGetD3D11DeviceContext();
    V_RETURN(g_DialogResourceManager.OnD3D11CreateDevice(pd3dDevice, pd3dImmediateContext));
    V_RETURN(g_D3DSettingsDlg.OnD3D11CreateDevice(pd3dDevice));
    g_pTxtHelper = new CDXUTTextHelper(pd3dDevice, pd3dImmediateContext, &g_DialogResourceManager, 15);

    g_Camera.SetRadius(1.5f, 0.1f);
    Scene::CreateMesh(pd3dDevice);

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Called whenever the swap chain is resized.  
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D11ResizedSwapChain(ID3D11Device* pd3dDevice, IDXGISwapChain* pSwapChain,
                                         const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc, void* pUserContext)
{
    HRESULT hr;

    V_RETURN(g_DialogResourceManager.OnD3D11ResizedSwapChain(pd3dDevice, pBackBufferSurfaceDesc));
    V_RETURN(g_D3DSettingsDlg.OnD3D11ResizedSwapChain(pd3dDevice, pBackBufferSurfaceDesc));

    // Setup the camera's projection parameters    
    float fAspectRatio = pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams(D3DX_PI / 4, fAspectRatio, ZNEAR, ZFAR);
    g_Camera.SetWindow(pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height);

    g_HUD.SetLocation(pBackBufferSurfaceDesc->Width - 170, 0);
    g_HUD.SetSize(170, 170);

    const UINT Width = 256;
    const UINT Height = 256;
    g_SampleUI.SetLocation(pBackBufferSurfaceDesc->Width - Width, 150);
    g_SampleUI.SetSize(Width, Height);
    g_SampleUI.SetBackgroundColors(D3DCOLOR_RGBA(116,183,27,255));

    SAFE_DELETE(g_pDualDepthPeeling);
    g_pDualDepthPeeling = new DualDepthPeeling(pd3dDevice, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height);
    g_Techniques[DUAL_DEPTH_PEELING].pEngine = g_pDualDepthPeeling;

    SAFE_DELETE(g_pStochasticTransparency);
    g_pStochasticTransparency = new StochasticTransparency(pd3dDevice, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height);
    g_Techniques[STOCHASTIC_TRANSPARENCY].pEngine = g_pStochasticTransparency;

    SAFE_DELETE(g_pPlainAlphaBlending);
    g_pPlainAlphaBlending = new PlainAlphaBlending(pd3dDevice, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height);
    g_Techniques[PLAIN_ALPHA_BLENDING].pEngine = g_pPlainAlphaBlending;

    g_pCurrentEngine = g_Techniques[0].pEngine;

    return S_OK;
}

//--------------------------------------------------------------------------------------
void UpdateUI()
{
    float alpha = (float)g_SampleUI.GetSlider(IDC_ALPHA_SLIDER)->GetValue() * 0.01f;
    BaseTechnique::SetAlpha(alpha);

    UINT NumPeelingPasses = g_SampleUI.GetSlider(IDC_NUM_PEELING_PASSES_SLIDER)->GetValue();
    g_pDualDepthPeeling->SetNumGeometryPasses(NumPeelingPasses);

    UINT NumStochasticPasses = g_SampleUI.GetSlider(IDC_NUM_STOCHASTIC_PASSES_SLIDER)->GetValue();
    g_pStochasticTransparency->SetNumPasses(NumStochasticPasses);

    bool IsDepthPeelingEnabled = (g_pCurrentEngine == g_pDualDepthPeeling);
    g_SampleUI.GetStatic(IDC_NUM_PEELING_PASSES_STATIC)->SetVisible(IsDepthPeelingEnabled);
    g_SampleUI.GetSlider(IDC_NUM_PEELING_PASSES_SLIDER)->SetVisible(IsDepthPeelingEnabled);
    g_SampleUI.GetStatic(IDC_NUM_STOCHASTIC_PASSES_STATIC)->SetVisible(!IsDepthPeelingEnabled);
    g_SampleUI.GetSlider(IDC_NUM_STOCHASTIC_PASSES_SLIDER)->SetVisible(!IsDepthPeelingEnabled);

    WCHAR sz[100];
    StringCchPrintf(sz, 100, L"Num geometry passes: %d", BaseTechnique::GetNumGeometryPasses());
    g_SampleUI.GetStatic(IDC_NUM_PEELING_PASSES_STATIC)->SetText(sz);
    g_SampleUI.GetStatic(IDC_NUM_STOCHASTIC_PASSES_STATIC)->SetText(sz);

    StringCchPrintf(sz, 100, L"Alpha: %.2f", BaseTechnique::GetAlpha());
    g_SampleUI.GetStatic(IDC_ALPHA_STATIC)->SetText(sz);
}

//--------------------------------------------------------------------------------------
D3DXMATRIX OffsetWorldMatrix()
{
    D3DXMATRIX TranslationMatrix;
    D3DXMatrixTranslation( &TranslationMatrix, g_ModelOffset[0], g_ModelOffset[1], g_ModelOffset[2]);
    D3DXMATRIX WorldMatrix = TranslationMatrix * (*g_Camera.GetWorldMatrix());

    static float s_Angle = 0.0f;
    if (g_SampleUI.GetCheckBox(IDC_AUTO_ROTATE)->GetChecked())
    {
        s_Angle += AUTO_ROTATION_RATE / DXUTGetFPS();
        if (s_Angle > D3DX_PI*2.0f) s_Angle = 0.0;
    }

    D3DXMATRIX mWorld;
    D3DXMATRIX mRotation;
    D3DXMatrixRotationY(&mRotation, s_Angle + D3DX_PI);
    D3DXMatrixMultiply(&mWorld, &mRotation, &WorldMatrix);
    return mWorld;
}

//--------------------------------------------------------------------------------------
void UpdateMatrices()
{
    D3DXMATRIX mProj = *g_Camera.GetProjMatrix();
    D3DXMATRIX mView = *g_Camera.GetViewMatrix();
    D3DXMATRIX mWorld = OffsetWorldMatrix();

    D3DXMATRIX mViewI, mViewIT, mViewProj, mViewProjI;
    D3DXMATRIX mWorldI, mWorldIT;
    D3DXMATRIX mWorldView, mWorldViewI, mWorldViewIT;
    D3DXMATRIX mWorldViewProj, mWorldViewProjI;
    D3DXMatrixInverse(&mViewI, NULL, &mView);
    D3DXMatrixTranspose(&mViewIT, &mViewI);
    D3DXMatrixMultiply(&mViewProj, &mView, &mProj);
    D3DXMatrixInverse(&mViewProjI, NULL, &mViewProj);
    D3DXMatrixInverse(&mWorldI, NULL, &mWorld);
    D3DXMatrixTranspose(&mWorldIT, &mWorldI);
    D3DXMatrixMultiply(&mWorldView, &mWorld, &mView);
    D3DXMatrixInverse(&mWorldViewI, NULL, &mWorldView);
    D3DXMatrixTranspose(&mWorldViewIT, &mWorldViewI);
    D3DXMatrixMultiply(&mWorldViewProj, &mWorldView, &mProj);
    D3DXMatrixInverse(&mWorldViewProjI, NULL, &mWorldViewProj);

    for (int i = 0; i < NUM_TECHNIQUES; ++i)
    {
        g_Techniques[i].pEngine->UpdateMatrices(mWorldViewProj, mWorldViewIT);
    }
}

//--------------------------------------------------------------------------------------
// Callback function that renders the frame.  This function sets up the rendering 
// matrices and renders the scene and UI.
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender(ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext, double fTime,
                                 float fElapsedTime, void* pUserContext)
{
    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if(g_D3DSettingsDlg.IsActive())
    {
        g_D3DSettingsDlg.OnRender(fElapsedTime);
        return;
    }

    UpdateUI();
    UpdateMatrices();

    // Store off original render target and depth/stencil
    ID3D11RenderTargetView* pOrigRTV = NULL;
    ID3D11DepthStencilView* pOrigDSV = NULL;
    pd3dImmediateContext->OMGetRenderTargets(1, &pOrigRTV, &pOrigDSV);

    BaseTechnique::ResetNumGeometryPasses();
    g_pCurrentEngine->Render(pd3dImmediateContext, pOrigRTV);

    // Restore original render targets
    pd3dImmediateContext->OMSetRenderTargets(1, &pOrigRTV, pOrigDSV);
    SAFE_RELEASE(pOrigRTV);
    SAFE_RELEASE(pOrigDSV);

    // Reset SRVs to avoid D3D runtime warnings
    ID3D11ShaderResourceView* pNullSRVs[2] = { NULL, NULL };
    pd3dImmediateContext->PSSetShaderResources(0, 2, pNullSRVs);

    if (g_ShowUI)
    {
        DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"HUD / Stats");
        g_HUD.OnRender(fElapsedTime);
        g_SampleUI.OnRender(fElapsedTime);
        RenderText();
        DXUT_EndPerfEvent();
    }
}

//--------------------------------------------------------------------------------------
// This callback function will be called immediately after the Direct3D device has 
// been destroyed, which generally happens as a result of application termination or 
// windowed/full screen toggles. Resources created in the OnD3D11CreateDevice callback 
// should be released here, which generally includes all D3DPOOL_MANAGED resources. 
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11DestroyDevice(void* pUserContext)
{
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();
    SAFE_DELETE(g_pTxtHelper);

    SAFE_DELETE(g_pStochasticTransparency);
    SAFE_DELETE(g_pDualDepthPeeling);
    SAFE_DELETE(g_pPlainAlphaBlending);
    Scene::ReleaseMesh();
}

//--------------------------------------------------------------------------------------
// Release the swap chain
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11ReleasingSwapChain(void* pUserContext)
{
    g_DialogResourceManager.OnD3D11ReleasingSwapChain();
}
