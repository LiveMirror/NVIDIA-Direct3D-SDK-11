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

#ifndef GLOBALS_H
#define GLOBALS_H

#include "DXUTcamera.h"

extern ID3D11VertexShader*					g_pSkinningVS;
extern ID3D11VertexShader*					g_pPlaneVS;
extern ID3D11VertexShader*					g_pInputMeshVS;
extern ID3D11VertexShader*					g_pShadowmapVS;
extern ID3D11HullShader*					g_pSubDToBezierHS;
extern ID3D11HullShader*					g_pSubDToGregoryHS;
extern ID3D11HullShader*					g_pSubDToBezierHS_Distance;
extern ID3D11HullShader*					g_pSubDToGregoryHS_Distance ;
extern ID3D11DomainShader*					g_pSurfaceEvalDS;
extern ID3D11DomainShader*					g_pGregoryEvalDS;
extern ID3D11PixelShader*					g_pPhongShadingPS;
extern ID3D11PixelShader*					g_pPhongShadingPS_I ;
extern ID3D11PixelShader*					g_pFlatShadingPS;
extern ID3D11PixelShader*					g_pFlatShadingPS_I;
extern ID3D11PixelShader*					g_pSolidColorPS;
extern ID3D11PixelShader*					g_pShadowmapPS;
extern ID3D11SamplerState*                  g_pSamplePoint;
extern ID3D11SamplerState*                  g_pSampleLinear;
extern ID3D11ShaderResourceView*            pHeightMapTextureSRV;    
extern ID3D11ShaderResourceView*            pNormalMapTextureSRV; 
extern ID3D11ShaderResourceView*            pTexRenderRV11; 
extern ID3D11VertexShader*					g_pStaticObjectVS;
extern ID3D11PixelShader*					g_pStaticObjectPS;
extern ID3D11HullShader*					g_pStaticObjectHS;
extern ID3D11HullShader*					g_pStaticObjectHS_Distance;
extern ID3D11HullShader*					g_pStaticObjectGregoryHS;
extern ID3D11HullShader*					g_pStaticObjectGregoryHS_Distance;
extern ID3D11DomainShader*					g_pStaticObjectDS;
extern ID3D11DomainShader*					g_pStaticObjectGregoryDS;

extern bool                                 g_bFlatShading;  
extern int                                  g_iAdaptive; 
extern UINT								    g_iBindPerMesh;
extern UINT								    g_iBindPerFrame;
extern UINT								    g_iBindPerDraw;
extern UINT								    g_iBindPerPatch;
extern bool                                 g_bDrawGregoryACC;                  
extern ID3D11InputLayout*                   g_pConstructionLayout;
extern ID3D11InputLayout*                   g_pVertexLayout;
extern ID3D11Buffer*						g_pcbPerFrame;
extern ID3D11Buffer*						g_pcbPerDraw;

extern CModelViewerCamera                   g_Camera;
extern CModelViewerCamera                   g_Light;
struct CB_PER_DRAW_CONSTANTS;





#endif // GLOBALS_H