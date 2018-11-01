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

#ifndef COMMON_H
#define COMMON_H

#include <DXUT.h>
#include <SDKMisc.h>
#include <d3dx11effect.h>
#include "dxut.h" 
#include "strsafe.h"

#define STRSAFE_NO_DEPRECATE

#include <d3dx11effect.h>
#include "math.h"
#include "assert.h"
#include "dxut.h" 

// Some common globals shared across
extern int                                 g_Width;
extern int                                 g_Height;

#ifndef SAFE_ACQUIRE
#define SAFE_ACQUIRE(dst, p)      { if(dst) { SAFE_RELEASE(dst); } if (p) { (p)->AddRef(); } dst = (p); }
#endif


#define __TSTR_LENGTH 1024
static TCHAR __tstr[__TSTR_LENGTH];


#define V_GET_TECHNIQUE( pEffect, pEffectPathTstr, pEffTech, pTechniqueNameStr ) pEffTech = pEffect->GetTechniqueByName(pTechniqueNameStr); \
    if( !pEffTech || !pEffTech->IsValid() ) \
    {\
        StringCbPrintf(__tstr, __TSTR_LENGTH, TEXT("Failed Getting Technique: \"%hs\" in effect file %s."), pTechniqueNameStr, pEffectPathTstr) ;\
        MessageBox(NULL, __tstr, TEXT("Direct3D 10 Effect Init Error - Bad Technique"), MB_ABORTRETRYIGNORE|MB_ICONWARNING);\
        hr = E_FAIL;\
    }

#define V_GET_TECHNIQUE_RET( pEffect, pEffectPathTstr, pEffTech, pTechniqueNameStr ) V_GET_TECHNIQUE(pEffect, pEffectPathTstr, pEffTech, pTechniqueNameStr )\
    if( FAILED(hr) )\
        return hr;


#define V_GET_VARIABLE_RET( pEffect, pEffectPathTstr, pEffVar, pVarNameStr, asVariable ) V_GET_VARIABLE(pEffect, pEffectPathTstr, pEffVar, pVarNameStr, asVariable )\
    if( FAILED(hr) )\
        return hr;


#define V_GET_VARIABLE( pEffect, pEffectPathTstr, pEffVar, pVarNameStr, asVariable ) pEffVar = pEffect->GetVariableByName(pVarNameStr)->asVariable(); \
    if( !pEffVar || !pEffVar->IsValid() ) \
    {\
        StringCbPrintf(__tstr, __TSTR_LENGTH, TEXT("Failed Getting Variable \"%hs\" in effect file %s."), pVarNameStr, pEffectPathTstr);\
        MessageBox(NULL, __tstr, TEXT("Direct3D 10 Effect Init Error - Bad Variable"), MB_ABORTRETRYIGNORE|MB_ICONWARNING);\
        hr = E_FAIL;\
    }

#define V_GET_VARIABLE( pEffect, pEffectPathTstr, pEffVar, pVarNameStr, asVariable ) pEffVar = pEffect->GetVariableByName(pVarNameStr)->asVariable(); \
    if( !pEffVar || !pEffVar->IsValid() ) \
    {\
        StringCbPrintf(__tstr, __TSTR_LENGTH, TEXT("Failed Getting Variable \"%hs\" in effect file %s."), pVarNameStr, pEffectPathTstr);\
        MessageBox(NULL, __tstr, TEXT("Direct3D 10 Effect Init Error - Bad Variable"), MB_ABORTRETRYIGNORE|MB_ICONWARNING);\
        hr = E_FAIL;\
    }

#define V_GET_VARIABLE_RET( pEffect, pEffectPathTstr, pEffVar, pVarNameStr, asVariable ) V_GET_VARIABLE(pEffect, pEffectPathTstr, pEffVar, pVarNameStr, asVariable )\
    if( FAILED(hr) )\
        return hr;

#define V_GET_VARIABLE_RETBOOL( pEffect, pEffectPathTstr, pEffVar, pVarNameStr, asVariable ) V_GET_VARIABLE(pEffect, pEffectPathTstr, pEffVar, pVarNameStr, asVariable )\
    if( FAILED(hr) )\
        return false;


HRESULT WINAPI NVUTFindDXSDKMediaFileCch( WCHAR* strDestPath, int cchDest, LPCWSTR strFilename );
HRESULT WINAPI NVUTFindDXSDKMediaFileCchT( LPTSTR strDestPath, int cchDest, LPCTSTR strFilename );

HRESULT MyCreateEffectFromFile(TCHAR *fName, DWORD dwShaderFlags, ID3D11Device *pDevice, ID3DX11Effect **pEffect);
HRESULT MyCreateEffectFromCompiledFile(TCHAR *fName, DWORD dwShaderFlags, ID3D11Device *pDevice, ID3DX11Effect **pEffect);
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3D10Blob** ppBlobOut );
HRESULT LoadComputeShader(ID3D11Device* pd3dDevice, WCHAR *szFilename, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3D11ComputeShader **ppComputeShader);

inline void ComputeRowColsForFlat3DTexture( int depth, int *outCols, int *outRows )
{
    // Compute # of rows and cols for a "flat 3D-texture" configuration
    // (in this configuration all the slices in the volume are spread in a single 2D texture)
    int rows =(int)floorf(sqrtf((float)depth));
    int cols = rows;
    while( rows * cols < depth ) {
        cols++;
    }
    assert( rows*cols >= depth );
    
    *outCols = cols;
    *outRows = rows;
}

#endif