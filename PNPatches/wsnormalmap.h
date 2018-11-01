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

#include <vector>
#include "mymath/mymath.h"
#include "d3dx11effect.h"

#define RETURN_ON_ERROR(x)    { hr = (x); if( FAILED(hr) ) { assert(false); return; } }

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct MyMesh;

class WSNormalMap
{
    static bool s_bInited;

    static ID3DX11Effect*       m_pEffect;
    static ID3DX11Effect*       m_pEffectCS;
    static ID3DX11EffectPass*   m_pDefaultPass;

    static ID3D11Device*        m_pDevice;
    static ID3D11DeviceContext* m_pContext;

public:
    WSNormalMap(int2 dimensions, DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT);
    static void Init(ID3D11Device *pDevice, ID3D11DeviceContext *pContext);
    static bool Initialized();
    static void Destroy();
    static void DisableTessellation();

    int2                m_MapDimensions;
    DXGI_FORMAT         m_Format;
    
    void GetMapsTiled(MyMesh *pMesh, ID3D11Texture2D **ppNormalMapTexture, ID3D11Texture2D **ppPositionMapTexture, int2 tile_origin, int tile_size, int ss_scale=1, int border=0);
    void GenerateNormalMapAndPositionMap_GPU(MyMesh *pMesh, ID3D11ShaderResourceView **ppNormalMapSRV, ID3D11ShaderResourceView **ppPositionMapSRV);
    void FixupNormals_GPU(MyMesh *pMesh, ID3D11ShaderResourceView *pDisplacedPositionsSRV);
    void TransformNormalMapToTangent_GPU(MyMesh *pMesh);
    void TransformNormalMapTileToTangent_GPU(MyMesh *pMesh, ID3D11Texture2D **ppNormalMapTexture, ID3D11Texture2D **ppNormalMapTextureTS, int2 tile_origin, int tile_size, int ss_scale=1, int border=0);
    void DilateNormalMap_GPU(MyMesh *pMesh, int borderSize = 1);
    
    void DilateNormalMap(ID3D11Texture2D **ppNormalMapTexture, ID3D11Texture2D **ppDilatedNormalMapTexture, int borderSize = 1);
    void DilateDisplacementMap(ID3D11Texture2D **ppDisplacementMapTexture, ID3D11Texture2D **ppDilatedDisplacementMapTexture, int borderSize = 1);

    void DownsampleNormalMap(ID3D11Texture2D **ppNormalMapTexture, ID3D11Texture2D **ppDownsampledNormalMapTexture, int ss_scale = 1);
    void DownsampleDisplacementMap(ID3D11Texture2D **ppDisplacementMapTexture, ID3D11Texture2D **ppDownsampledDisplacementMapTexture, int ss_scale = 1);
};
