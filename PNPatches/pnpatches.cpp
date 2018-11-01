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

#include "pnpatches.h"
#include <vector>
#include "mymesh.h"
#include "wsnormalmap.h"
#include "ProgressBar.h"

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------
float g_ViewRange               = 10000.0f;
float g_ViewRangeMin            = 0.1f;
float g_ViewRangeMax            = 1000.0f;
float g_BackBufferWidth         = 1280.0f;
float g_BackBufferHeight        = 720.0f;
float g_SuperSampleScale        = 1.0f;

// Statistics
int g_CurrentFrame              = 0;

// Control variables
bool g_RenderHUD                = true;
bool g_RenderDetailed           = false;
bool bUpdateCamera              = true;

CDXUTTextHelper* g_pTxtHelper   = NULL;
CD3DSettingsDlg g_D3DSettingsDlg;
CDXUTDialogResourceManager g_DialogResourceManager;
CDXUTDialog g_HUD;
CFirstPersonCamera g_Camera;

MyMesh *g_pDetailedMesh     = NULL;
MyMesh *g_pCoarseMesh       = NULL;
MyMesh *g_pAnimatedMesh     = NULL;

// Mesh definition
UINT g_MeshVerticesNum      = 0;
UINT g_MeshQuadIndicesNum   = 0;
UINT g_MeshTrgIndicesNum    = 0;
UINT g_MeshMemorySize       = 0;

// SLI
bool    g_NVAPIInitialized  = false;
int     g_NumAFRGroups      = 1;

//--------------------------------------------------------------------------------------
// UI control IDs
//--------------------------------------------------------------------------------------
#define IDC_TOGGLEFULLSCREEN            1
#define IDC_TOGGLEREF                   2
#define IDC_CHANGEDEVICE                3
#define IDC_UPDATECAMERA                4
#define IDC_RENDERWIREFRAME             5
#define IDC_TESSELLATIONFACTOR_STATIC   6
#define IDC_TESSELLATIONFACTOR          7
#define IDC_RENDERNORMALS               9
#define IDC_USE_NORMAL_MAP             10
#define IDC_DRAW_TEX_COORDS            11
#define IDC_GEOMETRY_STATIC            12
#define IDC_GEOMETRY_TYPE              14
#define IDC_DRAW_CHECKER_BOARD         15
#define IDC_RELOAD_EFFECT              16
#define IDC_DRAW_DISPLACEMENT_MAP      17
#define IDC_FIX_THE_SEAMS              18
#define IDC_ENABLE_ANIMATION           19

//--------------------------------------------------------------------------------------
// Extern declarations 
//--------------------------------------------------------------------------------------
void InitApp();
void RenderText();

enum { METHOD_NONE = 0, METHOD_COARSE = 1, METHOD_TESSELLATED = 2, METHOD_DETAILED = 3 };
static int s_iGeometryTypes[] = { METHOD_NONE, METHOD_COARSE, METHOD_TESSELLATED, METHOD_DETAILED };
static int s_iCurGeometryType = METHOD_TESSELLATED;
static void SetGeometryType(int iType)
{
    if (iType == METHOD_DETAILED)
    {
        if (g_pDetailedMesh == NULL)
        {
            if (MessageBox(NULL, L"It may take couple of minutes to load the detailed model. Are you sure?", L"Warning", MB_YESNO) != IDYES)
            {
                g_HUD.GetComboBox(IDC_GEOMETRY_TYPE)->SetSelectedByIndex(s_iCurGeometryType);
                return;
            }
            else
            {
                g_pDetailedMesh = new MyMesh();
                g_pDetailedMesh->LoadMeshFromFile(L"Tessellation/body_kriso_zbrush_fine.OBJ");

                g_pDetailedMesh->SetDisplacementMap(g_pCoarseMesh->g_pDisplacementMapSRV);
                g_pDetailedMesh->SetNormalMap(g_pCoarseMesh->g_pNormalMapSRV);
            }
        }
        g_RenderDetailed = true;
    }
    else
    {
        g_RenderDetailed = false;
    }
    s_iCurGeometryType = iType;
    g_HUD.GetCheckBox(IDC_ENABLE_ANIMATION)->SetChecked(s_iCurGeometryType != METHOD_DETAILED && MyMesh::g_bEnableAnimation);
    g_HUD.GetCheckBox(IDC_ENABLE_ANIMATION)->SetEnabled(s_iCurGeometryType != METHOD_DETAILED);
    g_HUD.GetSlider(IDC_TESSELLATIONFACTOR)->SetEnabled(s_iCurGeometryType == METHOD_TESSELLATED);
    g_HUD.GetComboBox(IDC_GEOMETRY_TYPE)->SetSelectedByIndex(s_iCurGeometryType);
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
    static bool isFirstTime = true;

    pDeviceSettings->d3d11.sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    if (isFirstTime)
    {
        pDeviceSettings->d3d11.sd.SampleDesc.Count = 4;
        pDeviceSettings->d3d11.sd.SampleDesc.Quality = 0;
        pDeviceSettings->d3d11.SyncInterval = 0;
        isFirstTime = false;
    }

    pDeviceSettings->d3d11.AutoCreateDepthStencil = true;

    return true;
}

//--------------------------------------------------------------------------------------
// Create any D3D11 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
static ID3D11Query *s_pQuery = NULL;
static bool s_bQueryEmpty = true;
static D3D11_QUERY_DATA_PIPELINE_STATISTICS s_GPUProbeStats;

HRESULT CALLBACK OnD3D11CreateDevice( ID3D11Device* pDevice, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc,
                                     void* pUserContext )
{
    HRESULT hr;

    InitializeProgressBar();

    D3D11_QUERY_DESC Desc;
    Desc.Query = D3D11_QUERY_PIPELINE_STATISTICS;
    Desc.MiscFlags = 0;
    pDevice->CreateQuery(&Desc, &s_pQuery);

    static bool isFirstTime = true;

    ID3D11DeviceContext* pContext = DXUTGetD3D11DeviceContext();
    WSNormalMap::Init(pDevice, pContext);
    MyMesh::Init(pDevice);
    V_RETURN( g_DialogResourceManager.OnD3D11CreateDevice( pDevice, pContext ) );
    V_RETURN( g_D3DSettingsDlg.OnD3D11CreateDevice( pDevice ) );
    g_pTxtHelper = new CDXUTTextHelper( pDevice, pContext, &g_DialogResourceManager, 15 );

    ID3D11ShaderResourceView* pDisplacementTextureSRV = NULL;
    ID3D11ShaderResourceView* pNormalTextureSRV = NULL;

    // Try to load displacement and normal maps from cache
    {
        D3DX11_IMAGE_INFO imageInfo;
        ID3D11Texture2D* pDisplacementTexture = NULL;
        ID3D11Texture2D* pNormalTexture = NULL;
        WCHAR path[1024];

        if (DXUTFindDXSDKMediaFileCch(path, MAX_PATH, L"PNPatches/cache/displacement_map.dds") == S_OK)
        {
            if (D3DX11GetImageInfoFromFile(path, NULL, &imageInfo, NULL) == S_OK)
            {
                D3DX11_IMAGE_LOAD_INFO imageLoadInfo;
                memset(&imageLoadInfo, 0, sizeof(imageLoadInfo));
                imageLoadInfo.Width = imageInfo.Width;
                imageLoadInfo.Height = imageInfo.Height;
                imageLoadInfo.MipLevels = 1;
                imageLoadInfo.Format = imageInfo.Format;
                imageLoadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                V_RETURN(D3DX11CreateTextureFromFile(pDevice, path, &imageLoadInfo, NULL, (ID3D11Resource**)&pDisplacementTexture, NULL));

                D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
                memset(&descSRV, 0, sizeof(descSRV));
                descSRV.Format = imageInfo.Format;
                descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                descSRV.Texture2D.MipLevels = 1;
                pDevice->CreateShaderResourceView(pDisplacementTexture, &descSRV, &pDisplacementTextureSRV);

                pDisplacementTexture->Release();
            }
        }

        if (DXUTFindDXSDKMediaFileCch(path, MAX_PATH, L"PNPatches/cache/normal_map.dds") == S_OK)
        {
            if (D3DX11GetImageInfoFromFile(path, NULL, &imageInfo, NULL) == S_OK)
            {
                D3DX11_IMAGE_LOAD_INFO imageLoadInfo;
                memset(&imageLoadInfo, 0, sizeof(imageLoadInfo));
                imageLoadInfo.Width = imageInfo.Width;
                imageLoadInfo.Height = imageInfo.Height;
                imageLoadInfo.MipLevels = 1;
                imageLoadInfo.Format = imageInfo.Format;
                imageLoadInfo.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                V_RETURN(D3DX11CreateTextureFromFile(pDevice, path, &imageLoadInfo, NULL, (ID3D11Resource**)&pNormalTexture, NULL));

                D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
                memset(&descSRV, 0, sizeof(descSRV));
                descSRV.Format = imageInfo.Format;
                descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                descSRV.Texture2D.MipLevels = 1;
                pDevice->CreateShaderResourceView(pNormalTexture, &descSRV, &pNormalTextureSRV);

                pNormalTexture->Release();
            }
        }
    }

    if (!(pDisplacementTextureSRV && pNormalTextureSRV))
    {
        MessageBox(NULL, L"Could not read displacement and normal maps from PNPatches/cache folder", L"Warning", MB_OK);
        exit(0);
    }

    g_pCoarseMesh = new MyMesh();
    g_pCoarseMesh->LoadMeshFromFile(L"Tessellation/body_kriso_zbrush_coarse.OBJ");

    g_pCoarseMesh->SetDisplacementMap(pDisplacementTextureSRV);
    g_pCoarseMesh->SetNormalMap(pNormalTextureSRV);

    SAFE_RELEASE(pDisplacementTextureSRV);
    SAFE_RELEASE(pNormalTextureSRV);

    // Load animated model
    g_pAnimatedMesh = new MyMesh();
    g_pAnimatedMesh->LoadMeshFromFile(L"Tessellation/monster.x");

    // Assign displacement and normal maps
    g_pAnimatedMesh->SetNormalMap(g_pCoarseMesh->g_pNormalMapSRV);
    float relativeScale = g_pAnimatedMesh->GetRelativeScale(g_pCoarseMesh);
    g_pAnimatedMesh->SetDisplacementMap(g_pCoarseMesh->g_pDisplacementMapSRV, relativeScale);

    // Remove progress bar
    SetProgress(-1);

    // Set initial camera view
    if (isFirstTime)
    {
        D3DXVECTOR3 eyePt = D3DXVECTOR3(-38.051765f, 152.686584f, -184.241455f);
        D3DXVECTOR3 lookAtPt = D3DXVECTOR3(-37.558346f, 152.850357f, -183.387238f);
        g_Camera.SetViewParams(&eyePt, &lookAtPt);
        g_Camera.SetScalers(0.005f, 10.0f);
    }

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
    g_Camera.SetProjParams(D3DX_PI / 4.0f, aspectRatio, g_ViewRangeMin, g_ViewRangeMax);

    g_HUD.SetLocation( pBackBufferSurfaceDesc->Width - 200, 0 );
    g_HUD.SetSize( 200, 170 );

    g_BackBufferWidth = (float)pBackBufferSurfaceDesc->Width;
    g_BackBufferHeight = (float)pBackBufferSurfaceDesc->Height;

    // our base resolution is 1280x720. we compute tessfactor scale in respect to base
    MyMesh::g_TessellationFactorResScale = sqrt(((double)g_BackBufferWidth * g_BackBufferHeight) / ((double)1280 * 720));

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove( double fTime, float fElapsedTime, void* pUserContext )
{
    g_Camera.FrameMove(fElapsedTime);
}


//--------------------------------------------------------------------------------------
// Render text for the UI
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos( 2, 0 );
    g_pTxtHelper->SetForegroundColor( D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ) );
    g_pTxtHelper->DrawTextLine( DXUTGetFrameStats( DXUTIsVsyncEnabled() ) );
    g_pTxtHelper->DrawTextLine( DXUTGetDeviceStats() );

    g_pTxtHelper->DrawFormattedTextLine(L"'W','S','A','D' & mouse - to navigate", s_GPUProbeStats.CPrimitives);
    g_pTxtHelper->DrawFormattedTextLine(L"Shift - accelerate navigation", s_GPUProbeStats.CPrimitives);

#if 0
    WCHAR info[256];
    g_pTxtHelper->DrawTextLine(L"");
    wsprintf(info, L"Quads: %d", g_MeshQuadIndicesNum / 4);
    g_pTxtHelper->DrawTextLine(info);
    g_pTxtHelper->DrawFormattedTextLine(L"Triangles: %d", g_MeshTrgIndicesNum / 3);
    wsprintf(info, L"Memory Usage: %d KB", g_MeshMemorySize / 1024);
    g_pTxtHelper->DrawTextLine(info);
#endif

    g_pTxtHelper->DrawFormattedTextLine(L"Total triangles: %I64d", s_GPUProbeStats.CInvocations);
    g_pTxtHelper->DrawFormattedTextLine(L"Unculled triangles: %I64d", s_GPUProbeStats.CPrimitives);

    g_pTxtHelper->End();
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D11 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D11FrameRender( ID3D11Device* pd3dDevice, ID3D11DeviceContext* pd3dImmediateContext,
                                 double fTime, float fElapsedTime, void* pUserContext )
{
    D3D11_QUERY_DATA_PIPELINE_STATISTICS GPUProbeStats;
    if (!s_bQueryEmpty && pd3dImmediateContext->GetData(s_pQuery, &GPUProbeStats, sizeof(GPUProbeStats), 0) == S_OK)
    {
        memcpy(&s_GPUProbeStats, &GPUProbeStats, sizeof(GPUProbeStats));
        s_bQueryEmpty = true;
    }
    if (s_bQueryEmpty)
    {
        pd3dImmediateContext->Begin(s_pQuery);
    }

    // If the settings dialog is being shown, then render it instead of rendering the app's scene
    if( g_D3DSettingsDlg.IsActive() )
    {
        g_D3DSettingsDlg.OnRender( fElapsedTime );
        return;
    }

    const float clearColor[4] = {0.25f, 0.25f, 0.25f, 0.0f};

    ID3D11RenderTargetView *pDefaultRTV = DXUTGetD3D11RenderTargetView();
    ID3D11DepthStencilView *pDefaultDSV = DXUTGetD3D11DepthStencilView();

    D3D11_VIEWPORT defaultViewport;
    UINT viewPortsNum = 1;
    pd3dImmediateContext->RSGetViewports(&viewPortsNum, &defaultViewport);

    D3DXMATRIX mView = *g_Camera.GetViewMatrix();
    D3DXMATRIX mProj = *g_Camera.GetProjMatrix();
    D3DXMATRIX mViewProj = mView * mProj;
    D3DXMATRIX mViewProjInv;
    D3DXMatrixInverse(&mViewProjInv, NULL, &mViewProj);
    D3DXVECTOR3 cameraPosition = *g_Camera.GetEyePt();
    D3DXVECTOR3 viewRight(mView.m[0][0], mView.m[1][0], mView.m[2][0]);
    D3DXVECTOR3 viewUp(mView.m[0][1], mView.m[1][1], mView.m[2][1]);
    D3DXVECTOR3 viewForward(mView.m[0][2], mView.m[1][2], mView.m[2][2]);

    pd3dImmediateContext->ClearRenderTargetView(pDefaultRTV, clearColor);
    pd3dImmediateContext->ClearDepthStencilView(pDefaultDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

    if (MyMesh::g_bEnableAnimation && !g_RenderDetailed)
    {
        if (g_pAnimatedMesh)
        {
            g_pAnimatedMesh->m_TessellationFactor = s_iCurGeometryType == METHOD_TESSELLATED ? MyMesh::g_TessellationFactor : 1;
            g_pAnimatedMesh->RenderAnimated(pd3dImmediateContext, mView, mProj, cameraPosition, fTime);
        }
    }
    else
    {
        if (g_RenderDetailed && g_pDetailedMesh)
        {
            g_pDetailedMesh->Render(pd3dImmediateContext, mView, mProj, cameraPosition);
        }
        else if (s_iCurGeometryType != METHOD_NONE && g_pCoarseMesh)
        {
            g_pCoarseMesh->m_TessellationFactor = s_iCurGeometryType == METHOD_TESSELLATED ? MyMesh::g_TessellationFactor : 1;
            g_pCoarseMesh->Render(pd3dImmediateContext, mView, mProj, cameraPosition);
        }
    }


    if (s_bQueryEmpty)
    {
        pd3dImmediateContext->End(s_pQuery);
        s_bQueryEmpty = false;
    }

    if (g_RenderHUD)
    {
        DXUT_BeginPerfEvent(DXUT_PERFEVENTCOLOR, L"HUD / Stats");
        g_HUD.OnRender(fElapsedTime);
        RenderText();
        DXUT_EndPerfEvent();
    }

    if (g_CurrentFrame++ == 0) // bring window to front
    {
        SetForegroundWindow(DXUTGetHWND());
    }
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
    SAFE_RELEASE(s_pQuery);
    g_DialogResourceManager.OnD3D11DestroyDevice();
    g_D3DSettingsDlg.OnD3D11DestroyDevice();
    DXUTGetGlobalResourceCache().OnDestroyDevice();

    WSNormalMap::Destroy();
    MyMesh::Destroy();

    SAFE_DELETE(g_pTxtHelper);

    SAFE_DELETE(g_pDetailedMesh);
    SAFE_DELETE(g_pCoarseMesh);
    SAFE_DELETE(g_pAnimatedMesh);
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
    WCHAR str[MAX_PATH];
    CDXUTRadioButton* pRadioButton = NULL;
    bool bModeChanged = false;

    if (g_RenderHUD == false) return;

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
        MyMesh::g_RenderWireframe = !MyMesh::g_RenderWireframe;
        break;
    case IDC_TESSELLATIONFACTOR:
        MyMesh::g_TessellationFactor = (float)g_HUD.GetSlider(IDC_TESSELLATIONFACTOR)->GetValue();
        wsprintf(str, L"Tessellation Level: %d", (int)MyMesh::g_TessellationFactor);
        g_HUD.GetStatic(IDC_TESSELLATIONFACTOR_STATIC)->SetText(str);
        break;
    case IDC_RENDERNORMALS:
        MyMesh:: g_RenderNormals = !MyMesh::g_RenderNormals;
        break;
    case IDC_FIX_THE_SEAMS:
        MyMesh::g_bFixTheSeams = !MyMesh::g_bFixTheSeams;
        MyMesh::LoadEffect();
        break;
    case IDC_ENABLE_ANIMATION:
        MyMesh::g_bEnableAnimation = !MyMesh::g_bEnableAnimation;
        break;
    case IDC_USE_NORMAL_MAP:
        pRadioButton = g_HUD.GetRadioButton(IDC_USE_NORMAL_MAP);
        bModeChanged = MyMesh::SetRenderMode(RENDER_NORMAL_MAP);
        break;
    case IDC_DRAW_DISPLACEMENT_MAP:
        pRadioButton = g_HUD.GetRadioButton(IDC_DRAW_DISPLACEMENT_MAP);
        bModeChanged = MyMesh::SetRenderMode(RENDER_DISPLACEMENT_MAP);
        break;
    case IDC_DRAW_TEX_COORDS:
        pRadioButton = g_HUD.GetRadioButton(IDC_DRAW_TEX_COORDS);
        bModeChanged = MyMesh::SetRenderMode(RENDER_TEXTURE_COORDINATES);
        break;
    case IDC_DRAW_CHECKER_BOARD:
        pRadioButton = g_HUD.GetRadioButton(IDC_DRAW_CHECKER_BOARD);
        bModeChanged = MyMesh::SetRenderMode(RENDER_CHECKER_BOARD);
        break;
    case IDC_UPDATECAMERA:
        MyMesh::g_bUpdateCamera = !MyMesh::g_bUpdateCamera;
        break;
    case IDC_GEOMETRY_TYPE:
        if (nEvent == EVENT_COMBOBOX_SELECTION_CHANGED)
        {
            SetGeometryType(g_HUD.GetComboBox(IDC_GEOMETRY_TYPE)->GetSelectedIndex());
        }
        break;
    case IDC_RELOAD_EFFECT:
        MyMesh::LoadEffect();
        break;
    }

    if (pRadioButton)
    {
        if (!bModeChanged)
        {
            pRadioButton->SetChecked(false);
            MyMesh::SetRenderMode(RENDER_DEFAULT);
        }
    }
}


//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard( UINT nChar, bool bKeyDown, bool bAltDown, void* pUserContext )
{
    if (nChar == VK_SHIFT)
    {
        if(bKeyDown)
            g_Camera.SetScalers(0.005f, 100.0f);
        else
            g_Camera.SetScalers(0.005f, 10.0f);
    }

    if (!bKeyDown) return;

    if (nChar == VK_TAB)
    {
        g_RenderHUD = !g_RenderHUD;
        ShowCursor(g_RenderHUD);
    }

    if (bAltDown && nChar == '0')
    {
        static int entry = 0;
        FILE *file = NULL;
        fopen_s(&file, "camera_positions.log", "a");
        D3DXVECTOR3 eyePt = *g_Camera.GetEyePt();
        D3DXVECTOR3 lookAtPt = *g_Camera.GetLookAtPt();
        fprintf(file, "View #%d\n", entry);
        fprintf(file, "D3DXVECTOR3 eyePt = D3DXVECTOR3(%ff, %ff, %ff);\n", eyePt.x, eyePt.y, eyePt.z);
        fprintf(file, "D3DXVECTOR3 lookAtPt = D3DXVECTOR3(%ff, %ff, %ff);\n", lookAtPt.x, lookAtPt.y, lookAtPt.z);
        fclose(file);
        entry++;
    }

    // Predefined views
    if(nChar == '1') {
        D3DXVECTOR3 eyePt = D3DXVECTOR3(175.735550f, 71.120201f, -33.687935f);
        D3DXVECTOR3 lookAtPt = D3DXVECTOR3(174.939163f, 71.495262f, -34.162342f);
        g_Camera.SetViewParams(&eyePt, &lookAtPt);
    }
    else if(nChar == '2') {
        D3DXVECTOR3 eyePt = D3DXVECTOR3(-38.051765f, 152.686584f, -184.241455f);
        D3DXVECTOR3 lookAtPt = D3DXVECTOR3(-37.558346f, 152.850357f, -183.387238f);
        g_Camera.SetViewParams(&eyePt, &lookAtPt);
    }
    else if(nChar == '3') {
        D3DXVECTOR3 eyePt = D3DXVECTOR3(-195.621094f, 125.658028f, -272.873077f);
        D3DXVECTOR3 lookAtPt = D3DXVECTOR3(-194.979004f, 125.597534f, -272.108856f);
        g_Camera.SetViewParams(&eyePt, &lookAtPt);
    }
#if 0
    else if(nChar == '4') {
        D3DXVECTOR3 eyePt = D3DXVECTOR3(1.667134f, 0.977631f, 3.018520f);
        D3DXVECTOR3 lookAtPt = D3DXVECTOR3(1.102306f, 0.902180f, 2.196769f);
        g_Camera.SetViewParams(&eyePt, &lookAtPt);
    }
    else if(nChar == '5') {
        D3DXVECTOR3 eyePt = D3DXVECTOR3(-1.632872f, 0.977779f, 3.507845f);
        D3DXVECTOR3 lookAtPt = D3DXVECTOR3(-1.310902f, 0.972256f, 2.561111f);
        g_Camera.SetViewParams(&eyePt, &lookAtPt);
    }
#endif
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
    V_RETURN(DXUTSetMediaSearchPath(L"..\\Source\\PNPatches"));
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

    DXUTInit( true, true, NULL ); // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings( true, true ); // Show the cursor and clip it when in full screen
    DXUTCreateWindow( L"PN-Patches" );
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
    g_HUD.AddButton( IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 0, iY, 170, 23 );
    g_HUD.AddButton( IDC_TOGGLEREF, L"Toggle REF (F3)", 0, iY += 26, 170, 23, VK_F3 );
    g_HUD.AddButton( IDC_CHANGEDEVICE, L"Change device (F2)", 0, iY += 26, 170, 23, VK_F2 );

    WCHAR str[MAX_PATH];
    CDXUTCheckBox* pCheckBox = NULL;

    iY += 24;
    wsprintf(str, L"Tessellation Level: %d", (int)MyMesh::g_TessellationFactor);
    g_HUD.AddStatic(IDC_TESSELLATIONFACTOR_STATIC, str, 0, iY+=20, 140, 24);
    g_HUD.AddSlider(IDC_TESSELLATIONFACTOR, 0, iY+=24, 140, 24, 1, 512, (int)MyMesh::g_TessellationFactor);

    iY += 40;
    g_HUD.AddStatic(IDC_GEOMETRY_STATIC, L"Geometry to render:", 0, iY, 140, 24);
    iY += 24;
    g_HUD.AddComboBox(IDC_GEOMETRY_TYPE, 0, iY, 200, 30, 0, true);
    g_HUD.GetComboBox(IDC_GEOMETRY_TYPE)->AddItem(L"None", &s_iGeometryTypes[METHOD_NONE]);
    g_HUD.GetComboBox(IDC_GEOMETRY_TYPE)->AddItem(L"Coarse model", &s_iGeometryTypes[METHOD_COARSE]);
    g_HUD.GetComboBox(IDC_GEOMETRY_TYPE)->AddItem(L"Tessellated coarse", &s_iGeometryTypes[METHOD_TESSELLATED]);
    g_HUD.GetComboBox(IDC_GEOMETRY_TYPE)->AddItem(L"Detailed model", &s_iGeometryTypes[METHOD_DETAILED]);

    g_HUD.GetComboBox(IDC_GEOMETRY_TYPE)->SetSelectedByIndex(s_iCurGeometryType);
    g_HUD.EnableKeyboardInput(true);
    iY += 50;

    g_HUD.AddCheckBox(IDC_UPDATECAMERA, L"Update culling", 0, iY+=24, 140, 24, MyMesh::g_bUpdateCamera, 0, false, &pCheckBox);

    iY += 24;
    g_HUD.AddCheckBox(IDC_RENDERWIREFRAME, L"Show Wire(f)rame", 0, iY+=24, 140, 24, MyMesh::g_RenderWireframe, 'F');
    g_HUD.AddCheckBox(IDC_RENDERNORMALS, L"Show Normals", 0, iY+=24, 140, 24, MyMesh::g_RenderNormals, 0);
    g_HUD.AddCheckBox(IDC_FIX_THE_SEAMS, L"Fix Displacement Seams", 0, iY+=24, 140, 24, MyMesh::g_bFixTheSeams, 0);

    g_HUD.AddCheckBox(IDC_ENABLE_ANIMATION, L"Animation", 0, iY+=48, 140, 24, MyMesh::g_bEnableAnimation, 0);

    iY += 24;
    g_HUD.AddRadioButton(IDC_USE_NORMAL_MAP, 0, L"Draw (N)ormal Map", 0, iY+=24, 140, 24, false, 'N');
    g_HUD.AddRadioButton(IDC_DRAW_TEX_COORDS, 0, L"Draw (T)exture Coord", 0, iY+=24, 140, 24, false, 'T');
    g_HUD.AddRadioButton(IDC_DRAW_DISPLACEMENT_MAP, 0, L"Draw Displacement Map", 0, iY+=24, 140, 24, false);
    g_HUD.AddRadioButton(IDC_DRAW_CHECKER_BOARD, 0, L"Draw (C)hecker Board", 0, iY+=24, 140, 24, false, 'C');

    iY += 24;
    g_HUD.AddButton(IDC_RELOAD_EFFECT, L"Reload Effect", 0, iY+=24, 170, 24);
}
