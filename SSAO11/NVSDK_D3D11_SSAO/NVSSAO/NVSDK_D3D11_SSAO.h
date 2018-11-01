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

#pragma once
#pragma pack(push,8) // Make sure we have consistent structure packings

#include <d3d11.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__d3d11_h__) && defined(__cplusplus)

typedef enum
{
    NVSDK_OK                                    =  0,
    NVSDK_ERROR                                 = -1,
    NVSDK_INVALID_ARGUMENT                      = -5,
} NVSDK_Status;

#ifndef _WIN32
#define __cdecl
#endif

#define NVSDK_INTERFACE extern NVSDK_Status __cdecl

// NVSDK_HALF_RES_AO renders the AO with a half-res viewport, taking as input half-res linear depths (faster).
// NVSDK_FULL_RES_AO renders the AO with a full-res viewport, taking as input full-res linear depths (higher quality).
typedef enum
{
    NVSDK_HALF_RES_AO,
    NVSDK_FULL_RES_AO
} NVSDK_D3D11_AOResolutionMode;

// NVSDK_HBAO_4x8_CS renders HBAO using compute shaders, 4 directions and 8 steps per direction (faster).
// NVSDK_HBAO_8x6_PS renders HBAO using a pixel-shader, 8 directions, 6 steps per direction and a 4x4 random texture (reference).
typedef enum
{
    NVSDK_HBAO_4x8_CS,
    NVSDK_HBAO_8x6_PS
} NVSDK_D3D11_AOShaderType;

typedef struct
{
    float                           radius;                         // default = 1.0                    // the actual eye-space AO radius is set to radius*SceneScale
    UINT32                          stepSize;                       // default = 4                      // step size in the AO pass // 1~16
    float                           angleBias;                      // default = 10 degrees             // to hide low-tessellation artifacts // 0.0~30.0
    float                           strength;                       // default = 1.0                    // scale factor for the AO, the greater the darker
    float                           powerExponent;                  // default = 1.0                    // the final AO output is pow(AO, powerExponent)
    NVSDK_D3D11_AOShaderType        shaderType;                     // default = HBAO_4x8_CS            // to compare HBAO_4x8_CS with the original HBAO algorithm
    NVSDK_D3D11_AOResolutionMode    resolutionMode;                 // default = FULL_RES_AO            // resolution mode for the AO pass
    UINT32                          blurRadius;                     // default = 16 pixels              // radius of the cross bilateral filter // 0~16
    float                           blurSharpness;                  // default = 8.0                    // the higher, the more the blur preserves edges // 0.0~16.0
    BOOL                            useBlending;                    // default = true                   // to multiply the AO over the destination render target
} NVSDK_D3D11_AOParameters;

// To save video memory, you can pass your own render targets to the library.
// These have to be half or full resolution, non-multisample.
// Setting a pointer to NULL will make the library allocate the associated render target internally.
typedef struct
{
    ID3D11Texture2D *pFullResLinDepthBuffer;    // full resolution, R32_FLOAT, R16_FLOAT or R16G16_FLOAT
    ID3D11Texture2D *pHalfResLinDepthBuffer;    // half resolution, R32_FLOAT, R16_FLOAT or R16G16_FLOAT
} NVSDK_D3D11_RenderTargetsForAO;

// To profile the GPU time spent in NVSDK_D3D11_RenderAO.
typedef struct
{
    float ZTimeMS;
    float AOTimeMS;
    float BlurXTimeMS;
    float BlurYTimeMS;
    float CompositeTimeMS;
    float TotalTimeMS;
} NVSDK_D3D11_RenderTimesForAO;

//---------------------------------------------------------------------------------------------------
// Sets the input hardware (non-linear) depths to be used by NVSDK_D3D10_RenderAO.
// If pFullResNonLinearDepthTexture is multisample, it is resolved internally when calling RenderAO.
// The texture format for pFullResNonLinearDepthTexture is required to be R24G8_TYPELESS.
// ZNear and ZFar are the parameters of the perspective camera associated with the input depths.
// [MinZ,MaxZ] is the viewport depth range associated with the input depths, typically [0.0f,1.0f].
// Unless explicitely specified, SceneScale = min(ZNear, ZFar) is used as a scale factor for the AO radius.
//---------------------------------------------------------------------------------------------------
NVSDK_INTERFACE NVSDK_D3D11_SetHardwareDepthsForAO(ID3D11Device *pDevice,
                                                   ID3D11Texture2D *pFullResNonLinearDepthTexture,
                                                   float ZNear,
                                                   float ZFar,
                                                   float MinZ,
                                                   float MaxZ,
                                                   float SceneScale=0.0f);

//---------------------------------------------------------------------------------------------------
// Sets the input eye-space depths to be used by NVSDK_D3D11_RenderAO.
// pFullResLinearDepthTexture is required to be R32_FLOAT, R16_FLOAT or R16G16_FLOAT, non-multisample.
// SceneScale is used as a scale factor for the AO radius.
//---------------------------------------------------------------------------------------------------
NVSDK_INTERFACE NVSDK_D3D11_SetViewSpaceDepthsForAO(ID3D11Device *pDevice,
                                                    ID3D11Texture2D *pFullResLinearDepthTexture,
                                                    float SceneScale);

//---------------------------------------------------------------------------------------------------
// Renders screen space ambient occlusion into pOutputRT.
// Assuming that NVSDK_D3D11_SetHardwareDepthsForAO or NVSDK_D3D11_SetViewSpaceDepthsForAO has been called previously.
// FovyRad = 2*atan((h/2)/ZNear), where h = screen height in eye space.
// Allocates internal resources the first time it is called.
// pCustomRTs should be NULL, unless you want to re-use existing render targets.
// pRenderTimes should be NULL, unless you want to profile the GPU time, which may have a performance hit.
//---------------------------------------------------------------------------------------------------
NVSDK_INTERFACE NVSDK_D3D11_RenderAO(ID3D11Device *pDevice,
                                     ID3D11DeviceContext* pDeviceContext,
                                     float FovyRad,
                                     ID3D11RenderTargetView *pOutputRT,
                                     NVSDK_D3D11_RenderTargetsForAO *pCustomRTs=NULL,
                                     NVSDK_D3D11_RenderTimesForAO *pRenderTimes=NULL);

//---------------------------------------------------------------------------------------------------
// [Optional] Overrides the default ambient occlusion parameters.
//---------------------------------------------------------------------------------------------------
NVSDK_INTERFACE NVSDK_D3D11_SetAOParameters(const NVSDK_D3D11_AOParameters *pParams);

//---------------------------------------------------------------------------------------------------
// [Optional] Returns the amount of video memory allocated by the library, in bytes.
//---------------------------------------------------------------------------------------------------
NVSDK_INTERFACE NVSDK_D3D11_GetAllocatedVideoMemoryBytes(UINT32 *pNumBytes);

//---------------------------------------------------------------------------------------------------
// Releases the internal resources.
//---------------------------------------------------------------------------------------------------
NVSDK_INTERFACE NVSDK_D3D11_ReleaseAOResources();

#endif //defined(__d3d11_h__) && defined(__cplusplus)

#ifdef __cplusplus
}; //extern "C" {
#endif

#pragma pack(pop)
