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

#include <tchar.h>
#include <AtlBase.h>
#include <Strsafe.h>
#include "Common.h"



HRESULT WINAPI NVUTFindDXSDKMediaFileCch( WCHAR* strDestPath, int cchDest, LPCWSTR strFilename )
{
    HRESULT hr = DXUTFindDXSDKMediaFileCch( strDestPath, cchDest, strFilename );

    if (FAILED(hr))
    {
        WCHAR strFullError[MAX_PATH] = L"The file ";
        StringCchCat( strFullError, MAX_PATH, strFilename );
        StringCchCat( strFullError, MAX_PATH, L" was not found in the search path.\nCannot continue - exiting." );

        MessageBoxW(NULL, strFullError, L"Media file not found", MB_OK);
        exit(0);
    }

    return hr;
}

HRESULT WINAPI NVUTFindDXSDKMediaFileCchT( LPTSTR strDestPath, int cchDest, LPCTSTR strFilename )
{
    HRESULT hr(S_OK);
    USES_CONVERSION;

    CT2W wstrFileName(strFilename);
    LPWSTR wstrDestPath = new WCHAR[cchDest];
    
    hr = NVUTFindDXSDKMediaFileCch(wstrDestPath, cchDest, wstrFileName);
    
    if(!FAILED(hr))
    {
        LPTSTR tstrDestPath = W2T(wstrDestPath);
        _tcsncpy_s(strDestPath, cchDest, tstrDestPath, cchDest);
    }

    delete[] wstrDestPath;

    return hr;
}


#include <d3dx9shader.h>
HRESULT MyCreateEffectFromFile(TCHAR *fName, DWORD dwShaderFlags, ID3D11Device *pDevice, ID3DX11Effect **pEffect)
{
    char fNameChar[1024];
	CharToOem(fName, fNameChar);
	ID3DXBuffer *pShader;
	HRESULT hr;
	ID3DXBuffer *pErrors;
	if (FAILED(D3DXCompileShaderFromFileA(fNameChar, NULL, NULL, NULL, "fx_5_0", 0, &pShader, &pErrors, NULL)))
	{
		MessageBoxA(NULL, (char *)pErrors->GetBufferPointer(), "Error", MB_OK);
		return E_FAIL;
	}
    V_RETURN(D3DX11CreateEffectFromMemory(pShader->GetBufferPointer(), pShader->GetBufferSize(), dwShaderFlags, pDevice, pEffect));

	SAFE_RELEASE(pErrors);
	SAFE_RELEASE(pShader);

    return S_OK;
}


HRESULT MyCreateEffectFromCompiledFile(TCHAR *fName, DWORD dwShaderFlags, ID3D11Device *pDevice, ID3DX11Effect **pEffect)
{
	HRESULT hr;
	
	char filename[MAX_PATH];
	wcstombs_s(NULL,filename,MAX_PATH,fName,MAX_PATH);

	FILE * pFile;
	errno_t error;
	if((error = fopen_s(&pFile,filename,"rb")) != 0)
	{
        MessageBoxA(NULL,"Could not find compiled effect file","Error",MB_OK);
		return S_FALSE;
	}

    // obtain file size:
    fseek (pFile , 0 , SEEK_END);
    int lSize = ftell (pFile);
    rewind (pFile);

    // allocate memory to contain the whole file:
    char* buffer = (char*) malloc (sizeof(char)*lSize);
    if (buffer == NULL) return S_FALSE; 

    // copy the file into the buffer:
    size_t result = fread (buffer,sizeof(char),lSize,pFile);
    if ((int)result != lSize) return S_FALSE;

    //create the effect
    V_RETURN(D3DX11CreateEffectFromMemory((void*) buffer,  result*sizeof(char) , dwShaderFlags, pDevice, pEffect));

    fclose (pFile);

    free (buffer);

    return S_OK;
}


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


HRESULT LoadComputeShader(ID3D11Device* pd3dDevice, WCHAR *szFilename, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3D11ComputeShader **ppComputeShader)
{
	HRESULT hr;
	ID3DBlob* pBlob = NULL;
	V_RETURN( CompileShaderFromFile( szFilename, szEntryPoint, "cs_5_0", &pBlob ) );
	V_RETURN( pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, ppComputeShader ) );
	SAFE_RELEASE( pBlob );
	return hr;
}
