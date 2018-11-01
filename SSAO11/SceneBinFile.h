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

#include <DXUT.h>

#ifndef MAX_PATH_STR
#define MAX_PATH_STR 512
#endif

struct BinFileDescriptor
{
    LPWSTR Name;
    LPWSTR Path;
};

typedef struct
{
    UINT Width;
    UINT Height;
    float FovyRad;
    float ZNear;
    float ZFar;
} DumpHeader;

typedef struct
{
    DXGI_FORMAT DX10Format;
    UINT FormatBytes;
} RenderTargetHeader;

class SceneBinFile
{
public:
    SceneBinFile()
        : m_ImportedLinZTexture(NULL)
        , m_ImportedLinZSRV(NULL)
        , m_ImportedColorTexture(NULL)
        , m_ImportedColorSRV(NULL)
        , m_InputFp(NULL)
    {
    }

    HRESULT OnCreateDevice(BinFileDescriptor &SceneDesc)
    {
        WCHAR Filepath[MAX_PATH_STR+1];
        DXUTFindDXSDKMediaFileCch(Filepath, MAX_PATH_STR, SceneDesc.Path);

        _wfopen_s(&m_InputFp, Filepath, L"rb");
        if (!m_InputFp)
        {
            return S_FALSE;
        }

        LoadHeader(m_InputFp);
        m_FrameWidth = m_DumpHeader.Width;
        m_FrameHeight = m_DumpHeader.Height;
        return S_OK;
    }

    void CreateBuffers(ID3D11Device* pd3dDevice)
    {
        rewind(m_InputFp);
        LoadHeader(m_InputFp);
        LoadLinearDepths(m_InputFp, pd3dDevice);
        LoadColors(m_InputFp, pd3dDevice);
    }

    float GetSceneScale()
    {
        return min(m_DumpHeader.ZNear, m_DumpHeader.ZFar);
    }

    UINT GetWidth()
    {
        return m_DumpHeader.Width;
    }

    UINT GetHeight()
    {
        return m_DumpHeader.Height;
    }

    ID3D11Texture2D *GetLinearDepthTexture()
    {
        return m_ImportedLinZTexture;
    }

    ID3D11ShaderResourceView *GetColorSRV()
    {
        return m_ImportedColorSRV;
    }

    void OnResizedSwapChain(ID3D11Device* pd3dDevice, UINT FrameWidth, UINT FrameHeight)
    {
        m_FrameWidth = FrameWidth;
        m_FrameHeight = FrameHeight;
        CreateBuffers(pd3dDevice);
    }

    void OnDestroyDevice()
    {
        SAFE_RELEASE(m_ImportedLinZTexture);
        SAFE_RELEASE(m_ImportedLinZSRV);
        SAFE_RELEASE(m_ImportedColorTexture);
        SAFE_RELEASE(m_ImportedColorSRV);

        if (m_InputFp)
        {
            fclose(m_InputFp);
        }
    }

protected:
    DumpHeader                    m_DumpHeader;
    ID3D11Texture2D               *m_ImportedLinZTexture;
    ID3D11ShaderResourceView      *m_ImportedLinZSRV;
    ID3D11Texture2D               *m_ImportedColorTexture;
    ID3D11ShaderResourceView      *m_ImportedColorSRV;
    int                           m_FrameWidth;
    int                           m_FrameHeight;
    FILE                          *m_InputFp;

    void LoadHeader(FILE *fp)
    {
        UINT FormatVersion;
        fread(&FormatVersion, sizeof(FormatVersion), 1, fp);
        fread(&m_DumpHeader, sizeof(m_DumpHeader), 1, fp);
    }

    void LoadLinearDepths(FILE *fp, ID3D11Device* pd3dDevice)
    {
        HRESULT hr;

        int inWidth = m_DumpHeader.Width;
        int inHeight = m_DumpHeader.Height;
        int outWidth = m_FrameWidth;
        int outHeight = m_FrameHeight;

        RenderTargetHeader rt_header;
        fread(&rt_header, sizeof(rt_header), 1, fp);

        float *inBuffer = new float[inWidth*inHeight];
        fread(inBuffer, inWidth * inHeight * rt_header.FormatBytes, 1, fp);

        D3D11_TEXTURE2D_DESC tex_desc;
        tex_desc.Width            = outWidth;
        tex_desc.Height           = outHeight;
        tex_desc.MipLevels        = tex_desc.ArraySize = 1;
        tex_desc.Format           = rt_header.FormatBytes == 4 ? DXGI_FORMAT_R32_FLOAT : DXGI_FORMAT_R16_FLOAT;
        tex_desc.SampleDesc.Count = 1;
        tex_desc.SampleDesc.Quality = 0;
        tex_desc.Usage            = D3D11_USAGE_IMMUTABLE;
        tex_desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
        tex_desc.CPUAccessFlags   = 0;
        tex_desc.MiscFlags        = 0;

        // Assuming fp32 depths
        float *outBuffer = new float[outWidth*outHeight];
        for (int x = 0; x < outWidth; ++x)
        {
            for (int y = 0; y < outHeight; ++y)
            {
                int x1 = x * inWidth / outWidth;
                int y1 = y * inHeight / outHeight;
                outBuffer[y * outWidth + x] = inBuffer[y1 * inWidth + x1];
            }
        }

        D3D11_SUBRESOURCE_DATA sr_desc;
        sr_desc.pSysMem          = outBuffer;
        sr_desc.SysMemPitch      = outWidth*rt_header.FormatBytes;
        sr_desc.SysMemSlicePitch = 0;

        SAFE_RELEASE(m_ImportedLinZTexture);
        V(pd3dDevice->CreateTexture2D(&tex_desc, &sr_desc, &m_ImportedLinZTexture));
        delete[] outBuffer;
        delete[] inBuffer;

        SAFE_RELEASE(m_ImportedLinZSRV);
        V(pd3dDevice->CreateShaderResourceView(m_ImportedLinZTexture, NULL, &m_ImportedLinZSRV));
    }

    void LoadColors(FILE *fp, ID3D11Device* pd3dDevice)
    {
        HRESULT hr;

        int inWidth = m_DumpHeader.Width;
        int inHeight = m_DumpHeader.Height;
        int outWidth = m_FrameWidth;
        int outHeight = m_FrameHeight;

        RenderTargetHeader rt_header;
        size_t n = fread(&rt_header, sizeof(rt_header), 1, fp);
        if (n != 1) return;

        BYTE *inBuffer = new BYTE[inWidth*inHeight*rt_header.FormatBytes];
        fread(inBuffer, inWidth * inHeight * rt_header.FormatBytes, 1, fp);

        D3D11_TEXTURE2D_DESC tex_desc;
        tex_desc.Width            = outWidth;
        tex_desc.Height           = outHeight;
        tex_desc.MipLevels        = tex_desc.ArraySize = 1;
        tex_desc.SampleDesc.Count = 1;
        tex_desc.SampleDesc.Quality = 0;
        tex_desc.Usage            = D3D11_USAGE_IMMUTABLE;
        tex_desc.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
        tex_desc.CPUAccessFlags   = 0;
        tex_desc.MiscFlags        = 0;
        tex_desc.Format           = rt_header.DX10Format;

        // Assuming RGBA8 colors
        UINT *inBuffer8 = (UINT *)inBuffer;
        UINT *outBuffer = new UINT[outWidth*outHeight];
        for (int x = 0; x < outWidth; ++x)
        {
            for (int y = 0; y < outHeight; ++y)
            {
                int x1 = x * inWidth / outWidth;
                int y1 = y * inHeight / outHeight;
                outBuffer[y * outWidth + x] = inBuffer8[y1 * inWidth + x1];
            }
        }

        D3D11_SUBRESOURCE_DATA sr_desc;
        sr_desc.pSysMem          = outBuffer;
        sr_desc.SysMemPitch      = outWidth*rt_header.FormatBytes;
        sr_desc.SysMemSlicePitch = 0;

        SAFE_RELEASE(m_ImportedColorTexture);
        V(pd3dDevice->CreateTexture2D(&tex_desc, &sr_desc, &m_ImportedColorTexture));
        delete[] inBuffer;
        delete[] outBuffer;

        SAFE_RELEASE(m_ImportedColorSRV);
        V(pd3dDevice->CreateShaderResourceView(m_ImportedColorTexture, NULL, &m_ImportedColorSRV));
    }
};
