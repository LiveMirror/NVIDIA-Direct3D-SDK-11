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

#ifndef SIMPLERT_H
#define SIMPLERT_H

#include "Defines.h"

struct Tex3PosVertex
{
    D3DXVECTOR3 Pos;
    D3DXVECTOR3 Tex;
    Tex3PosVertex(D3DXVECTOR3 p, D3DXVECTOR3 t) {Pos=p; Tex=t;}
    Tex3PosVertex() {Pos=D3DXVECTOR3(0.0f,0.0f,0.0f); Tex=D3DXVECTOR3(0.0f,0.0f, 0.0f);}
};

//static int g_numResources = 0;
static int g_numResourcesRT = 0;
static int g_numResources2D = 0;
static int g_numResources3D = 0;
static int g_numResourcesRTV = 0;
static int g_numResourcesUAV = 0;

struct ShaderCompileParams
{
    int blockSize[2];
    int numThreads;
    int direction;
    bool smem_opt;
};
static ShaderCompileParams defaultParams = { { 0, 0 }, 0, 0, false };

HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut, ShaderCompileParams &params = defaultParams);
void ComputeRowColsForFlat3DTexture( int depth, int& outCols, int& outRows );

class SimpleRT;

class RenderTarget
{
private:
protected:
    int m_numRTs;
public:
    int m_width3D;
    int m_height3D;
    int m_depth3D;
    int getNumRTs() { return m_numRTs; }
    ID3D11ShaderResourceView**    m_pSRVs;
    RenderTarget() : m_pSRVs(NULL), m_width3D(0), m_height3D(0), m_depth3D(0), m_numRTs(0) {    }
    void create(int numRTs) 
    { 
        g_numResourcesRT += numRTs;

        m_numRTs = numRTs; 
        m_pSRVs = new ID3D11ShaderResourceView*[m_numRTs]; 
        for(int i=0; i<numRTs; i++)
            m_pSRVs[i] = NULL;
    }

    virtual bool is2DTexture() { return true; } //is this always true?

    ID3D11ShaderResourceView** get_ppSRV(int rt)
    {
        return (m_numRTs > rt) ? &m_pSRVs[rt]  : NULL;
    }
    ID3D11ShaderResourceView* get_pSRV(int rt)
    {
        return (m_numRTs > rt) ? m_pSRVs[rt]  : NULL;
    }

    ~RenderTarget()
    {
        for(int i=0; i<m_numRTs; i++)
            SAFE_RELEASE( m_pSRVs[i] );
        if(m_pSRVs)    
            delete[] m_pSRVs;
        m_pSRVs = NULL;
    }
};

enum  DOWNSAMPLE_TYPE
{
    DOWNSAMPLE_MAX = 0,
    DOWNSAMPLE_MIN = 1,
    DOWNSAMPLE_AVERAGE = 2,
};

enum UPSAMPLE_TYPE
{
    UPSAMPLE_DUPLICATE = 0,
    UPSAMPLE_BILINEAR = 1,
};

enum SAMPLE_TYPE
{
    SAMPLE_REPLACE = 0,
    SAMPLE_ACCUMULATE = 1,
};


class BasicLPVTransforms
{
    private:
    //lpv transform matrices
    D3DXMATRIX m_worldToLPVBB; //matrix to transform a unit cube centered at world center to the world region occupied by the LPV
    D3DXMATRIX m_worldToLPVNormTex; //matrix to transform from world to LPV normalized texture space ( which is 0 to 1 in all 3 dimensions) 
    D3DXMATRIX m_worldToLPVNormTexRender; //matrix to transform from world to LPV normalized texture space without spatial snapping( real cascade position )
    
    D3DXMATRIX m_inverseLPVXform;
    D3DXMATRIX m_objTranslate;
    D3DXVECTOR3 m_cellSize;

    public:

    D3DXMATRIX getWorldToLPVNormTex() {    return m_worldToLPVNormTex; }
    D3DXMATRIX getWorldToLPVNormTexRender() {    return m_worldToLPVNormTexRender; }
    D3DXMATRIX getWorldToLPVBB() {    return m_worldToLPVBB; }
    D3DXVECTOR3 getCellSize() { return m_cellSize; }
    float getCellSizeVolume() { return m_cellSize.x*m_cellSize.y*m_cellSize.z; }


    void getLPVLightViewMatrices(D3DXMATRIX ViewMatrix, D3DXMATRIX* viewToLPVMatrix, D3DXMATRIX* inverseViewToLPV)
    {
        D3DXMATRIX inverseViewMatrix;

        //matrix to transform from the light view to the LPV space
        D3DXMatrixInverse( &inverseViewMatrix, NULL, &ViewMatrix );
        D3DXMatrixMultiply( viewToLPVMatrix,&inverseViewMatrix, &m_worldToLPVNormTex);
        D3DXMatrixInverse( inverseViewToLPV, NULL, viewToLPVMatrix );
    }

    D3DXMATRIX getViewToLPVMatrixGV(D3DXMATRIX ViewMatrix)
    {
        D3DXMATRIX viewToLPVMatrix, inverseViewMatrix;

        //matrix to transform from the light view to the LPV space
        D3DXMatrixInverse( &inverseViewMatrix, NULL, &ViewMatrix );
        D3DXMatrixMultiply( &viewToLPVMatrix,&inverseViewMatrix, &m_worldToLPVNormTex);

        //matrix to transform from the light view to the GV space
        //center the grid and then shift the GV half a cell so that GV cell centers line up with LPV cell vertices
        D3DXMATRIX ViewToLPVMatrixGV;
        D3DXMatrixMultiply(&ViewToLPVMatrixGV, &viewToLPVMatrix, &m_objTranslate);
        return ViewToLPVMatrixGV;
    }


    void setLPVTransformsRotatedAndOffset(float LPVscale, D3DXVECTOR3 LPVtranslate, D3DXMATRIX cameraViewMatrix, D3DXVECTOR3 viewVector, int width3D, int height3D, int depth3D)
    {
        D3DXMATRIX scale;
        
        /*
        // animating the view vector over time for debugging
        static float angle = 0;
        viewVector = D3DXVECTOR3(sin(angle),0, -cos(angle));
        D3DXVec3Normalize(&viewVector,&viewVector);
        angle += 0.0001745; //0.01 degrees
        */
        

        D3DXMatrixScaling(&scale,LPVscale,LPVscale,LPVscale);

        //scale the LPV by the total amount it needs to be scaled
        m_worldToLPVBB = scale;

        //construct a translation matrix to translate the LPV along the view vector (so that 80% of the LPV is infront of the camera)
        float offsetScale = 0.8f;
        D3DXVECTOR3 originalOffset = D3DXVECTOR3(offsetScale*0.5f*viewVector.x, offsetScale*0.5f*viewVector.y, offsetScale*0.5f*viewVector.z);
        D3DXVECTOR4 transformedOffset;
        D3DXVec3Transform(&transformedOffset,&originalOffset,&m_worldToLPVBB);

        //the total translation is the translation amount for the grid center + the translation amount for shifting the grid along the view vector
        //in addition, we also SNAP the translation to be a multiple of the grid cell size (we only translate in full cell sizes to avoid flickering)
        m_cellSize = D3DXVECTOR3((float)LPVscale/width3D, (float)LPVscale/height3D, (float)LPVscale/depth3D ); 
        D3DXMATRIXA16 modifiedTranslate;
        
        D3DXMatrixTranslation(&modifiedTranslate, floorf(( LPVtranslate.x+transformedOffset.x)/m_cellSize.x)*m_cellSize.x, 
                                                  floorf(( LPVtranslate.y+transformedOffset.y)/m_cellSize.y)*m_cellSize.y,
                                                  floorf(( LPVtranslate.z+transformedOffset.z)/m_cellSize.z)*m_cellSize.z );

        //further transform the LPV by applying the offset calculated and then translating it to the final position
        m_worldToLPVBB = m_worldToLPVBB * modifiedTranslate;

        D3DXMATRIXA16 translate;
        D3DXMatrixInverse( &m_inverseLPVXform, NULL, &m_worldToLPVBB );
        D3DXMatrixTranslation(&translate, 0.5f, 0.5f, 0.5f);
        D3DXMatrixMultiply(&m_worldToLPVNormTex, &m_inverseLPVXform, &translate);

        {
            D3DXMatrixTranslation(&modifiedTranslate,    ( LPVtranslate.x+transformedOffset.x) - floorf(( LPVtranslate.x+transformedOffset.x)/m_cellSize.x)*m_cellSize.x, 
                                                        ( LPVtranslate.y+transformedOffset.y) - floorf(( LPVtranslate.y+transformedOffset.y)/m_cellSize.y)*m_cellSize.y,
                                                        ( LPVtranslate.z+transformedOffset.z) - floorf(( LPVtranslate.z+transformedOffset.z)/m_cellSize.z)*m_cellSize.z );
                                                        
            //further transform the LPV by applying the offset calculated and then translating it to the final position
            D3DXMATRIXA16 worldToLPVBB = m_worldToLPVBB * modifiedTranslate;
            D3DXMATRIXA16 inverseLPVXform;
            D3DXMatrixInverse( &inverseLPVXform, NULL, &worldToLPVBB );
            D3DXMatrixTranslation(&translate, 0.5f, 0.5f, 0.5f);
            D3DXMatrixMultiply(&m_worldToLPVNormTexRender, &inverseLPVXform, &translate);
        }


        D3DXMatrixTranslation(&m_objTranslate,0.5f/width3D,0.5f/height3D,0.5f/depth3D);
    };

};

class SimpleRT : public RenderTarget, public BasicLPVTransforms
{
private:
    int m_width2D;
    int m_height2D;

    bool m_use2DTex;
    int m_numCols;
    int m_numRows;

    int m_numChannels;
    ID3D11Texture2D**             m_pTex2Ds;
    ID3D11Texture3D**             m_pTex3Ds;
    ID3D11RenderTargetView**     m_pRTVs;
    ID3D11UnorderedAccessView**  m_pUAVs;

    void createResources(int numRTs)
    {
        g_numResources2D += numRTs;
        g_numResources3D += numRTs;
        g_numResourcesRTV += numRTs;
        g_numResourcesUAV += numRTs;

        RenderTarget::create(numRTs);
        m_pTex2Ds = new ID3D11Texture2D*[numRTs];
        m_pTex3Ds = new ID3D11Texture3D*[numRTs];
        m_pRTVs =   new ID3D11RenderTargetView*[numRTs];
        m_pUAVs =   new ID3D11UnorderedAccessView*[numRTs];
        for(int i=0; i<numRTs; i++)
        {
            m_pTex2Ds[i] = NULL;
            m_pTex3Ds[i] = NULL;
            m_pRTVs[i] = NULL;
            m_pUAVs[i] = NULL;
        }
    }

    void setNumChannels(DXGI_FORMAT format)
    {
        if(format==DXGI_FORMAT_R32G32B32A32_TYPELESS || format==DXGI_FORMAT_R32G32B32A32_FLOAT  || format==DXGI_FORMAT_R32G32B32A32_UINT || format==DXGI_FORMAT_R32G32B32A32_SINT || 
           format==DXGI_FORMAT_R16G16B16A16_TYPELESS || format==DXGI_FORMAT_R16G16B16A16_FLOAT || format==DXGI_FORMAT_R16G16B16A16_UNORM || format==DXGI_FORMAT_R16G16B16A16_UINT || format==DXGI_FORMAT_R16G16B16A16_SNORM || format==DXGI_FORMAT_R16G16B16A16_SINT ||
           format==DXGI_FORMAT_R8G8B8A8_TYPELESS || format==DXGI_FORMAT_R8G8B8A8_UNORM || format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || format==DXGI_FORMAT_R8G8B8A8_UINT || format==DXGI_FORMAT_R8G8B8A8_SNORM || format==DXGI_FORMAT_R8G8B8A8_SINT)
           m_numChannels = 4;
        else if(format==DXGI_FORMAT_R32G32B32_TYPELESS || format==DXGI_FORMAT_R32G32B32_FLOAT || format==DXGI_FORMAT_R32G32B32_UINT || format==DXGI_FORMAT_R32G32B32_SINT)
            m_numChannels = 3;
        else if(format==DXGI_FORMAT_R32G32_TYPELESS || format==DXGI_FORMAT_R32G32_FLOAT || format==DXGI_FORMAT_R32G32_UINT || format==DXGI_FORMAT_R32G32_SINT ||
                format==DXGI_FORMAT_R16G16_TYPELESS || format==DXGI_FORMAT_R16G16_FLOAT || format==DXGI_FORMAT_R16G16_UNORM || format==DXGI_FORMAT_R16G16_UINT || format==DXGI_FORMAT_R16G16_SNORM || format==DXGI_FORMAT_R16G16_SINT ||
                format==DXGI_FORMAT_R8G8_TYPELESS || format==DXGI_FORMAT_R8G8_UNORM || format==DXGI_FORMAT_R8G8_UINT || format==DXGI_FORMAT_R8G8_SNORM || format==DXGI_FORMAT_R8G8_SINT)
            m_numChannels = 2;
        else if(format==DXGI_FORMAT_R32_TYPELESS || format==DXGI_FORMAT_D32_FLOAT || format==DXGI_FORMAT_R32_FLOAT || format==DXGI_FORMAT_R32_UINT || format==DXGI_FORMAT_R32_SINT ||
                format==DXGI_FORMAT_R16_TYPELESS || format==DXGI_FORMAT_R16_FLOAT || format==DXGI_FORMAT_D16_UNORM || format==DXGI_FORMAT_R16_UNORM || format==DXGI_FORMAT_R16_UINT || format==DXGI_FORMAT_R16_SNORM || format==DXGI_FORMAT_R16_SINT ||
                format==DXGI_FORMAT_R8_TYPELESS || format==DXGI_FORMAT_R8_UNORM || format==DXGI_FORMAT_R8_UINT || format==DXGI_FORMAT_R8_SNORM || format==DXGI_FORMAT_R8_SINT || format==DXGI_FORMAT_A8_UNORM)
            m_numChannels = 1;
        else
            m_numChannels = -1; //didnt recognize the format, please add the format to the list above

    }

public:

    SimpleRT() : RenderTarget(), m_pTex2Ds( NULL ), m_pTex3Ds( NULL ), m_pRTVs( NULL ), m_pUAVs( NULL ), m_width2D(0), m_height2D(0), m_use2DTex(true), m_numCols(0), m_numRows(0), m_numChannels(0) { }

    int getWidth2D() {return m_width2D;}
    int getHeight2D() {return m_height2D;}

    int getWidth3D() {return m_width3D;}
    int getHeight3D() {return m_height3D;}
    int getDepth3D() {return m_depth3D;}

    int getNumCols() {return m_numCols;}
    int getNumRows() {return m_numRows;}

    int getNumChannels() { return m_numChannels; }

    bool is2DTexture() { return m_use2DTex; }

    HRESULT Create2D( ID3D11Device* pd3dDevice, int width2D, int height2D, int width3D, int height3D, int depth3D, DXGI_FORMAT format, bool uav = false, int numRTs = 1 )
    {
        HRESULT hr;

        m_use2DTex = true;

        createResources(numRTs);
        setNumChannels(format);

        m_width2D = width2D;
        m_height2D = height2D;
        m_width3D = width3D;
        m_height3D = height3D;
        m_depth3D = depth3D;
        ComputeRowColsForFlat3DTexture( m_depth3D, m_numCols, m_numRows );

        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory( &desc, sizeof( D3D11_TEXTURE2D_DESC ) );
        desc.ArraySize = 1;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        if (uav) desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.Format = format;
        desc.Width = width2D;
        desc.Height = height2D;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        for(int i=0; i<m_numRTs; i++)
            V_RETURN( pd3dDevice->CreateTexture2D( &desc, NULL, &m_pTex2Ds[i] ) );

        // create the shader resource view
        D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
        descSRV.Format = desc.Format;
        descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        descSRV.Texture2D.MipLevels = 1;
        descSRV.Texture2D.MostDetailedMip = 0;
        for(int i=0; i<m_numRTs; i++)
            V_RETURN( pd3dDevice->CreateShaderResourceView( m_pTex2Ds[i], &descSRV, &m_pSRVs[i] ) );

        // create the render target view
        D3D11_RENDER_TARGET_VIEW_DESC descRT;
        descRT.Format = desc.Format;
        descRT.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        descRT.Texture2D.MipSlice = 0;
        for(int i=0; i<m_numRTs; i++)
            V_RETURN( pd3dDevice->CreateRenderTargetView( m_pTex2Ds[i], &descRT, &m_pRTVs[i] ) );

        if (uav)
        {
            // create the unordered access view
            D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
            descUAV.Format = desc.Format;
            descUAV.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
            descUAV.Texture2D.MipSlice = 0;
            for(int i=0; i<m_numRTs; i++)
                V_RETURN( pd3dDevice->CreateUnorderedAccessView( m_pTex2Ds[i], &descUAV, &m_pUAVs[i] ) );
        }
       
        return S_OK;
    }

    HRESULT Create2DArray( ID3D11Device* pd3dDevice, int width3D, int height3D, int depth3D, DXGI_FORMAT format, bool uav = false, int numRTs = 1  )
    {

        HRESULT hr;

        m_use2DTex = false;

        createResources(numRTs);
        setNumChannels(format);

        m_width3D = width3D;
        m_height3D = height3D;
        m_depth3D = depth3D;
        ComputeRowColsForFlat3DTexture( m_depth3D, m_numCols, m_numRows );
        m_width2D = m_numCols*width3D;
        m_height2D = m_numRows*height3D;

        //create the texture
        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory( &desc, sizeof( D3D11_TEXTURE2D_DESC ) );
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        if (uav) desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.Format = format;
        desc.Width = width3D;
        desc.Height = height3D;
        desc.ArraySize = depth3D;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        for(int i=0; i<m_numRTs; i++)
            V_RETURN( pd3dDevice->CreateTexture2D( &desc, NULL, &m_pTex2Ds[i] ) );

        // create the shader resource view
        D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
        descSRV.Format = desc.Format;
        descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        descSRV.Texture2DArray.ArraySize = depth3D;
        descSRV.Texture2DArray.FirstArraySlice = 0;
        descSRV.Texture2DArray.MipLevels = 1;
        descSRV.Texture2DArray.MostDetailedMip = 0;
        for(int i=0; i<m_numRTs; i++)
            V_RETURN( pd3dDevice->CreateShaderResourceView( m_pTex2Ds[i], &descSRV, &m_pSRVs[i] ) );

        // create the render target view
        D3D11_RENDER_TARGET_VIEW_DESC descRT;
        descRT.Format = desc.Format;
        descRT.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
        descRT.Texture2DArray.ArraySize = depth3D;
        descRT.Texture2DArray.MipSlice = 0;
        descRT.Texture2DArray.FirstArraySlice = 0;
        for(int i=0; i<m_numRTs; i++)
            V_RETURN( pd3dDevice->CreateRenderTargetView( m_pTex2Ds[i], &descRT, &m_pRTVs[i] ) );

        if (uav)
        {
            // create the unordered access view
            D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
            descUAV.Format = desc.Format;
            descUAV.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
            descUAV.Texture2DArray.ArraySize = depth3D;
            descUAV.Texture2DArray.FirstArraySlice = 0;
            descUAV.Texture2DArray.MipSlice = 0;
            for(int i=0; i<m_numRTs; i++)
                V_RETURN( pd3dDevice->CreateUnorderedAccessView( m_pTex2Ds[i], &descUAV, &m_pUAVs[i] ) );
        }

        return S_OK;
    }

    HRESULT Create3D( ID3D11Device* pd3dDevice, int width3D, int height3D, int depth3D, DXGI_FORMAT format, bool uav = false, int numRTs = 1  )
    {
        HRESULT hr;

        m_use2DTex = false;

        createResources(numRTs);
        setNumChannels(format);

        m_width3D = width3D;
        m_height3D = height3D;
        m_depth3D = depth3D;
        ComputeRowColsForFlat3DTexture( m_depth3D, m_numCols, m_numRows );
        m_width2D = m_numCols*width3D;
        m_height2D = m_numRows*height3D;


        D3D11_TEXTURE3D_DESC desc;
        ZeroMemory( &desc, sizeof( D3D11_TEXTURE3D_DESC ) );
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        if (uav) desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.Format = format;
        desc.Width = width3D;
        desc.Height = height3D;
        desc.Depth = depth3D;
        desc.MipLevels = 1;
        for(int i=0; i<m_numRTs; i++)
            V_RETURN( pd3dDevice->CreateTexture3D( &desc, NULL, &m_pTex3Ds[i] ) );

        // create the shader resource view
        D3D11_SHADER_RESOURCE_VIEW_DESC descSRV;
        descSRV.Format = desc.Format;
        descSRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
        descSRV.Texture3D.MipLevels = 1;
        descSRV.Texture3D.MostDetailedMip = 0;
        for(int i=0; i<m_numRTs; i++)
            V_RETURN( pd3dDevice->CreateShaderResourceView( m_pTex3Ds[i], &descSRV, &m_pSRVs[i] ) );

        // create the render target view
        D3D11_RENDER_TARGET_VIEW_DESC descRT;
        descRT.Format = desc.Format;
        descRT.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
        descRT.Texture3D.MipSlice = 0;
        descRT.Texture3D.FirstWSlice = 0;
        descRT.Texture3D.WSize = depth3D;
        for(int i=0; i<m_numRTs; i++)
            V_RETURN( pd3dDevice->CreateRenderTargetView( m_pTex3Ds[i], &descRT, &m_pRTVs[i] ) );

        if (uav)
        {
            // create the unordered access view
            D3D11_UNORDERED_ACCESS_VIEW_DESC descUAV;
            descUAV.Format = desc.Format;
            descUAV.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
            descUAV.Texture3D.MipSlice = 0;
            descUAV.Texture3D.FirstWSlice = 0;
            descUAV.Texture3D.WSize = depth3D;
            for(int i=0; i<m_numRTs; i++)
                V_RETURN( pd3dDevice->CreateUnorderedAccessView( m_pTex3Ds[i], &descUAV, &m_pUAVs[i] ) );
        }
       
        return S_OK;
    }

    void Release()
    {
        for(int i=0; i<m_numRTs; i++)
        {
            SAFE_RELEASE( m_pTex2Ds[i] );
            SAFE_RELEASE( m_pTex3Ds[i] );
            SAFE_RELEASE( m_pRTVs[i] );
            SAFE_RELEASE( m_pUAVs[i] );        
        }

        if(m_pTex2Ds)
            delete[] m_pTex2Ds;
        if(m_pTex3Ds)
            delete[] m_pTex3Ds;
        if(m_pRTVs)
            delete[] m_pRTVs;
        if(m_pUAVs)
            delete[] m_pUAVs;

        m_pTex2Ds = NULL;
        m_pTex3Ds = NULL;
        m_pRTVs = NULL;
        m_pUAVs = NULL;

    }

    ~SimpleRT()
    {
        Release();
    }

    ID3D11RenderTargetView* get_pRTV(int i)
    {
        return (m_numRTs > i) ? m_pRTVs[i]  : NULL;
    }

    ID3D11UnorderedAccessView* get_pUAV(int i)
    {
        return (m_numRTs > i) ? m_pUAVs[i]  : NULL;
    }



};

class SimpleRT_RGB : public BasicLPVTransforms
{
    SimpleRT* Red[2];
    SimpleRT* Green[2];
    SimpleRT* Blue[2];
    unsigned int m_currentBuffer;
    bool m_doubleBuffer;

    HRESULT CreateRT(SimpleRT* rt, ID3D11Device* pd3dDevice, int width2D, int height2D, int width3D, int height3D, int depth3D, DXGI_FORMAT format, bool uav, bool use3DTex, bool use2DTexArray, int numRTs = 1)
    {
        if(use3DTex)
            return (rt->Create3D( pd3dDevice, width3D, height3D, depth3D, format, uav, numRTs));
        else if(use2DTexArray)
            return (rt->Create2DArray( pd3dDevice, width3D, height3D, depth3D, format, uav, numRTs));
        else
            return (rt->Create2D( pd3dDevice, width2D, height2D, width3D, height3D, depth3D, format, uav, numRTs));
    }

    int getBackbufferIndex() 
    {    
        if(m_doubleBuffer) return 1-m_currentBuffer;
        else return m_currentBuffer;
    }

public:

    ID3D11Buffer* m_pRenderToLPV_VB;

    SimpleRT_RGB() {        
        Red[0] = Red[1] = NULL;
        Green[0] = Green[1] = NULL;
        Blue[0] = Blue[1] = NULL;
        m_currentBuffer = 0;
        m_doubleBuffer = false;
        m_pRenderToLPV_VB = NULL;
    };
    HRESULT Create( ID3D11Device* pd3dDevice, int width2D, int height2D, int width3D, int height3D, int depth3D, DXGI_FORMAT format, bool uav , bool doublebuffer, bool use3DTex, bool use2DTexArray, int numRTs = 1)
    {
        HRESULT hr = S_OK;

        Red[0] = new SimpleRT();
        Green[0] = new SimpleRT();
        Blue[0] = new SimpleRT();

        V_RETURN(CreateRT(Red[0], pd3dDevice, width2D, height2D, width3D, height3D, depth3D, format, uav, use3DTex, use2DTexArray, numRTs));
        V_RETURN(CreateRT(Green[0], pd3dDevice, width2D, height2D, width3D, height3D, depth3D, format, uav, use3DTex, use2DTexArray, numRTs));
        V_RETURN(CreateRT(Blue[0], pd3dDevice, width2D, height2D, width3D, height3D, depth3D, format, uav, use3DTex, use2DTexArray, numRTs));

        m_doubleBuffer = doublebuffer;
        if(m_doubleBuffer)
        {
            Red[1] = new SimpleRT();
            Green[1] = new SimpleRT();
            Blue[1] = new SimpleRT();
            V_RETURN(CreateRT(Red[1], pd3dDevice, width2D, height2D, width3D, height3D, depth3D, format, uav, use3DTex, use2DTexArray, numRTs));
            V_RETURN(CreateRT(Green[1], pd3dDevice,width2D, height2D, width3D, height3D, depth3D, format, uav, use3DTex, use2DTexArray, numRTs));
            V_RETURN(CreateRT(Blue[1], pd3dDevice, width2D, height2D, width3D, height3D, depth3D, format, uav, use3DTex, use2DTexArray, numRTs));        
        }

        //create the Vertex Buffer to use for rendering to this grid
        int numVertices = 6 * depth3D;
        Tex3PosVertex* verticesLPV = new Tex3PosVertex[numVertices];

        for(int d=0;d<depth3D;d++)
        {
            verticesLPV[d*6] =    Tex3PosVertex(D3DXVECTOR3( -1.0f, -1.0f, 0.0f ), D3DXVECTOR3( 0.0f, 1.0f, (float)d) );
            verticesLPV[d*6+1] =  Tex3PosVertex(D3DXVECTOR3( -1.0f, 1.0f,  0.0f ), D3DXVECTOR3( 0.0f, 0.0f, (float)d) );
            verticesLPV[d*6+2] =  Tex3PosVertex(D3DXVECTOR3( 1.0f, -1.0f,  0.0f ), D3DXVECTOR3( 1.0f, 1.0f, (float)d) );
        
            verticesLPV[d*6+3] =  Tex3PosVertex(D3DXVECTOR3( 1.0f, -1.0f,  0.0f ), D3DXVECTOR3( 1.0f, 1.0f, (float)d) );
            verticesLPV[d*6+4] =  Tex3PosVertex(D3DXVECTOR3( -1.0f, 1.0f,  0.0f ), D3DXVECTOR3( 0.0f, 0.0f, (float)d) );
            verticesLPV[d*6+5] =  Tex3PosVertex(D3DXVECTOR3( 1.0f, 1.0f,   0.0f ), D3DXVECTOR3( 1.0f, 0.0f, (float)d) );
        }

        D3D11_BUFFER_DESC bd;
        D3D11_SUBRESOURCE_DATA InitData;
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof( Tex3PosVertex ) * numVertices;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = 0;
        bd.MiscFlags = 0;
        InitData.pSysMem = verticesLPV;
        V_RETURN(  pd3dDevice->CreateBuffer( &bd, &InitData, &m_pRenderToLPV_VB ) );
        delete verticesLPV;
        

        return hr;
    };

    void clearRenderTargetView(ID3D11DeviceContext* pd3dContext, const float clearColor[], bool front)
    {
        int Buffer = m_currentBuffer;
        if(!front)
        {
            if(!m_doubleBuffer) return;
            Buffer = getBackbufferIndex();
        }
        for(int i=0;i<Red[Buffer]->getNumRTs();i++)
            pd3dContext->ClearRenderTargetView( Red[Buffer]->get_pRTV(i), clearColor );    
        for(int i=0;i<Green[Buffer]->getNumRTs();i++)
            pd3dContext->ClearRenderTargetView( Green[Buffer]->get_pRTV(i), clearColor );
        for(int i=0;i<Blue[Buffer]->getNumRTs();i++)
            pd3dContext->ClearRenderTargetView( Blue[Buffer]->get_pRTV(i), clearColor );    
    }

    SimpleRT* getRed(bool front=true) { int Buffer = front? m_currentBuffer : getBackbufferIndex(); return Red[Buffer]; }
    SimpleRT* getBlue(bool front=true) { int Buffer = front? m_currentBuffer : getBackbufferIndex();  return Blue[Buffer]; }
    SimpleRT* getGreen(bool front=true) { int Buffer = front? m_currentBuffer : getBackbufferIndex();  return Green[Buffer]; }

    SimpleRT* getRedFront() { return Red[m_currentBuffer]; }
    SimpleRT* getBlueFront() { return Blue[m_currentBuffer]; }
    SimpleRT* getGreenFront() { return Green[m_currentBuffer]; }

    SimpleRT* getRedBack() { return Red[getBackbufferIndex()]; }
    SimpleRT* getBlueBack() { return Blue[getBackbufferIndex()]; }
    SimpleRT* getGreenBack() { return Green[getBackbufferIndex()]; }

    int getNumChannels() { return Red[m_currentBuffer]->getNumChannels(); }

    int getNumRTs() {return Red[m_currentBuffer]->getNumRTs(); }

    void swapBuffers() {if(m_doubleBuffer) m_currentBuffer = 1 - m_currentBuffer; }

    unsigned int getCurrentBuffer() {return m_currentBuffer; }

    int getWidth3D() {return  Red[0]->getWidth3D(); }
    int getHeight3D() {return Red[0]->getHeight3D(); }
    int getDepth3D() {return  Red[0]->getDepth3D(); }

    int getWidth2D() {return  Red[0]->getWidth2D(); }
    int getHeight2D() {return  Red[0]->getHeight2D(); }

    int getNumCols() {return  Red[0]->getNumCols(); }
    int getNumRows() {return  Red[0]->getNumRows(); }


    ~SimpleRT_RGB()
    {
        SAFE_DELETE(Red[0]);
        SAFE_DELETE(Green[0]);
        SAFE_DELETE(Blue[0]);
        SAFE_DELETE(Red[1]);
        SAFE_DELETE(Green[1]);
        SAFE_DELETE(Blue[1]);
        SAFE_RELEASE( m_pRenderToLPV_VB );
    }
};

struct CB_HIERARCHY
{
    int finerLevelWidth;        
    int finerLevelHeight;  
    int finerLevelDepth;  
    int g_numColsFiner;  

    int g_numRowsFiner; 
    int g_numColsCoarser;   
    int g_numRowsCoarser;   
    int corserLevelWidth; 

    int corserLevelHeight; 
    int corserLevelDepth;
    int padding1;
    int padding2;
};

class Hierarchy
{
private:

    ID3D11ComputeShader* m_pCSDownsampleMax;
    ID3D11ComputeShader* m_pCSDownsampleMin;
    ID3D11ComputeShader* m_pCSDownsampleAverage;
    ID3D11ComputeShader* m_pCSDownsampleAverageInplace;

    ID3D11ComputeShader* m_pCSUpsample;
    ID3D11ComputeShader* m_pCSUpsampleBilinear;
    ID3D11ComputeShader* m_pCSUpsampleBilinearAccumulate;
    ID3D11ComputeShader* m_pCSUpsampleBilinearAccumulateInplace;

    ID3D11Buffer* m_pcb;

public:


    Hierarchy(ID3D11Device* pd3dDevice)
    {
        HRESULT hr;
        ID3DBlob* pBlob = NULL;

        m_pCSDownsampleMax = NULL;
        V( CompileShaderFromFile( L"Hierarchy.hlsl", "DownsampleMax", "cs_5_0", &pBlob ) );
        V( pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pCSDownsampleMax ) );
        SAFE_RELEASE( pBlob );

        m_pCSDownsampleMin = NULL;
        V( CompileShaderFromFile( L"Hierarchy.hlsl", "DownsampleMin", "cs_5_0", &pBlob ) );
        V( pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pCSDownsampleMin ) );
        SAFE_RELEASE( pBlob );
        
        m_pCSDownsampleAverage = NULL;
        V( CompileShaderFromFile( L"Hierarchy.hlsl", "DownsampleAverage", "cs_5_0", &pBlob ) );
        V( pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pCSDownsampleAverage ) );
        SAFE_RELEASE( pBlob );

        m_pCSDownsampleAverageInplace = NULL;
        V( CompileShaderFromFile( L"Hierarchy.hlsl", "DownsampleAverageInplace", "cs_5_0", &pBlob ) );
        V( pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pCSDownsampleAverageInplace ) );
        SAFE_RELEASE( pBlob );

        m_pCSUpsample = NULL;
        V( CompileShaderFromFile( L"Hierarchy.hlsl", "Upsample", "cs_5_0", &pBlob ) );
        V( pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pCSUpsample ) );
        SAFE_RELEASE( pBlob );

        m_pCSUpsampleBilinear = NULL;
        V( CompileShaderFromFile( L"Hierarchy.hlsl", "UpsampleBilinear", "cs_5_0", &pBlob ) );
        V( pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pCSUpsampleBilinear ) );
        SAFE_RELEASE( pBlob );

        m_pCSUpsampleBilinearAccumulate = NULL;
        V( CompileShaderFromFile( L"Hierarchy.hlsl", "UpsampleBilinearAccumulate", "cs_5_0", &pBlob ) );
        V( pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pCSUpsampleBilinearAccumulate ) );
        SAFE_RELEASE( pBlob );


        m_pCSUpsampleBilinearAccumulateInplace = NULL;
        V( CompileShaderFromFile( L"Hierarchy.hlsl", "UpsampleBilinearAccumulateInplace", "cs_5_0", &pBlob ) );
        V( pd3dDevice->CreateComputeShader( pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &m_pCSUpsampleBilinearAccumulateInplace ) );
        SAFE_RELEASE( pBlob );
        


        // setup constant buffer
        D3D11_BUFFER_DESC Desc;
        Desc.Usage = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        Desc.MiscFlags = 0;
        Desc.ByteWidth = sizeof( CB_HIERARCHY );
        V( pd3dDevice->CreateBuffer( &Desc, NULL, &m_pcb ) );

    }

    //go from a finer (higherRes) to a coarser (lowerRes) grid by taking the max of a 2x2x2 (in 3D) or 2x2 (in 2D) region.
    virtual void Downsample(ID3D11DeviceContext* pd3dContext, SimpleRT* finer, SimpleRT* coarser, DOWNSAMPLE_TYPE op, int RTindex0, int RTindex1 = -1, int RTindex2 = -1, int RTindex3 = -1)
    {
        float ClearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        for(int i=0;i<coarser->getNumRTs();i++)
            pd3dContext->ClearRenderTargetView( coarser->get_pRTV(i), ClearColor );    

        assert( (RTindex1 == -1) || op==DOWNSAMPLE_AVERAGE ); //only have mutiple RT code written for the downsample average option. If you need more oprions you have to include them here 

        if(op==DOWNSAMPLE_MAX)
            pd3dContext->CSSetShader( m_pCSDownsampleMax, NULL, 0 );
        else if(op==DOWNSAMPLE_MIN)
            pd3dContext->CSSetShader( m_pCSDownsampleMin, NULL, 0 );
        else if(op==DOWNSAMPLE_AVERAGE)
        {
            if(RTindex1 == -1)
                pd3dContext->CSSetShader( m_pCSDownsampleAverage, NULL, 0 );
            else
                pd3dContext->CSSetShader( m_pCSDownsampleAverageInplace, NULL, 0 );
        }

        //set the unordered views that we are going to be writing to
        //set the finer RT as a texture to read from
        UINT initCounts = 0;

        if(RTindex1 == -1)
        {
            ID3D11UnorderedAccessView* ppUAV[1] = { coarser->get_pUAV(RTindex0) };
            pd3dContext->CSSetUnorderedAccessViews( 0, 1, ppUAV, &initCounts );
            ID3D11ShaderResourceView* ppSRV[1] = { finer->get_pSRV(RTindex0) };
            pd3dContext->CSSetShaderResources( 0, 1, ppSRV);
        }
        else
        {
            ID3D11UnorderedAccessView* ppUAV[4] = { coarser->get_pUAV(RTindex0), coarser->get_pUAV(RTindex1), coarser->get_pUAV(RTindex2), coarser->get_pUAV(RTindex3) };
            pd3dContext->CSSetUnorderedAccessViews( 0, 4, ppUAV, &initCounts );
            ID3D11ShaderResourceView* ppSRV[4] = { finer->get_pSRV(RTindex0), finer->get_pSRV(RTindex1), finer->get_pSRV(RTindex2), finer->get_pSRV(RTindex3) };
            pd3dContext->CSSetShaderResources( 0, 4, ppSRV);    
        }

        //set the constant buffer
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        pd3dContext->Map( m_pcb, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        CB_HIERARCHY* pcb = ( CB_HIERARCHY* )MappedResource.pData;
        pcb->finerLevelWidth = finer->getWidth3D();        
        pcb->finerLevelHeight = finer->getHeight3D();  
        pcb->finerLevelDepth = finer->getDepth3D();  
        pcb->corserLevelWidth = coarser->getWidth3D(); 
        pcb->corserLevelHeight = coarser->getHeight3D(); 
        pcb->corserLevelDepth = coarser->getDepth3D();
        pcb->g_numColsFiner = finer->getNumCols();  
        pcb->g_numRowsFiner = finer->getNumRows(); 
        pcb->g_numColsCoarser = coarser->getNumCols();  
        pcb->g_numRowsCoarser = coarser->getNumRows();    

        pd3dContext->Unmap( m_pcb, 0 );
        pd3dContext->CSSetConstantBuffers( 0, 1, &m_pcb );

        pd3dContext->Dispatch( g_LPVWIDTH/X_BLOCK_SIZE, g_LPVHEIGHT/Y_BLOCK_SIZE, g_LPVDEPTH/Z_BLOCK_SIZE );


        if(RTindex1 == -1)
        {
            ID3D11UnorderedAccessView* ppUAVssNULL1[1] = { NULL };
            pd3dContext->CSSetUnorderedAccessViews( 0, 1, ppUAVssNULL1, &initCounts );
            ID3D11ShaderResourceView* ppSRVsNULL1[1] = { NULL };
            pd3dContext->CSSetShaderResources( 0, 1, ppSRVsNULL1);
        }
        else
        {
            ID3D11UnorderedAccessView* ppUAVssNULL4[4] = { NULL, NULL, NULL, NULL };
            pd3dContext->CSSetUnorderedAccessViews( 0, 4, ppUAVssNULL4, &initCounts );
            ID3D11ShaderResourceView* ppSRVsNULL4[4] = { NULL, NULL, NULL, NULL };
            pd3dContext->CSSetShaderResources( 0, 4, ppSRVsNULL4);
        }

    }

    //go from a coarser (lowerRes) to a finer (higherRes) grid by copying data from one cell to a 2x2x2 (in 3D) or 2x2 (in 2D) region.
    virtual void Upsample(ID3D11DeviceContext* pd3dContext, SimpleRT* finer, SimpleRT* coarser, UPSAMPLE_TYPE op, SAMPLE_TYPE sampleType, ID3D11ShaderResourceView* srvAccumulate, int RTindex = 0)
    {
        float ClearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        for(int i=0;i<finer->getNumRTs();i++)
            pd3dContext->ClearRenderTargetView( finer->get_pRTV(i), ClearColor );    

        if(op==UPSAMPLE_DUPLICATE && sampleType == SAMPLE_REPLACE)
            pd3dContext->CSSetShader( m_pCSUpsample, NULL, 0 );
        else if(op==UPSAMPLE_BILINEAR && sampleType == SAMPLE_REPLACE)
        pd3dContext->CSSetShader( m_pCSUpsampleBilinear, NULL, 0 );
        else if(op==UPSAMPLE_BILINEAR && sampleType == SAMPLE_ACCUMULATE && srvAccumulate )
        pd3dContext->CSSetShader( m_pCSUpsampleBilinearAccumulate, NULL, 0 );

        //set the unordered views that we are going to be writing to
        UINT initCounts = 0;
        ID3D11UnorderedAccessView* ppUAV[1] = { finer->get_pUAV(RTindex) };
        pd3dContext->CSSetUnorderedAccessViews( 0, 1, ppUAV, &initCounts );

        //set the coarser RT as a texture to read from
        if( sampleType == SAMPLE_REPLACE)
        {
            ID3D11ShaderResourceView* ppSRV[1] = { coarser->get_pSRV(RTindex) };
            pd3dContext->CSSetShaderResources( 0, 1, ppSRV);
        }
        else if(sampleType == SAMPLE_ACCUMULATE)
        {
            ID3D11ShaderResourceView* ppSRV[2] = {coarser->get_pSRV(RTindex), srvAccumulate };
            pd3dContext->CSSetShaderResources( 0, 2, ppSRV);
        }

        //set the constant buffer
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        pd3dContext->Map( m_pcb, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        CB_HIERARCHY* pcb = ( CB_HIERARCHY* )MappedResource.pData;
        pcb->finerLevelWidth = finer->getWidth3D();        
        pcb->finerLevelHeight = finer->getHeight3D();  
        pcb->finerLevelDepth = finer->getDepth3D();  
        pcb->corserLevelWidth = coarser->getWidth3D(); 
        pcb->corserLevelHeight = coarser->getHeight3D(); 
        pcb->corserLevelDepth = coarser->getDepth3D();
        pcb->g_numColsFiner = finer->getNumCols();  
        pcb->g_numRowsFiner = finer->getNumRows(); 
        pcb->g_numColsCoarser = coarser->getNumCols();  
        pcb->g_numRowsCoarser = coarser->getNumRows();    

        pd3dContext->Unmap( m_pcb, 0 );
        pd3dContext->CSSetConstantBuffers( 0, 1, &m_pcb );

        pd3dContext->Dispatch( g_LPVWIDTH/X_BLOCK_SIZE, g_LPVHEIGHT/Y_BLOCK_SIZE, g_LPVDEPTH/Z_BLOCK_SIZE );

        ID3D11UnorderedAccessView* ppUAVssNULL1[1] = { NULL };
        pd3dContext->CSSetUnorderedAccessViews( 0, 1, ppUAVssNULL1, &initCounts );
        ID3D11ShaderResourceView* ppSRVsNULL2[2] = { NULL, NULL };
        pd3dContext->CSSetShaderResources( 0, 2, ppSRVsNULL2);

    }

    //go from a coarser (lowerRes) to a finer (higherRes) grid by accumulating data from one cell to a 2x2x2 (in 3D) or 2x2 (in 2D) region.
    //This version is specialized for 4 float textures, if you want more you have to change this function and also UpsampleBilinearAccumulateInplace in Hierarchy.hlsl
    virtual void UpsampleAccumulateInplace4(ID3D11DeviceContext* pd3dContext, 
                                            SimpleRT* finer, int finerIndex0, int finerIndex1, int finerIndex2, int finerIndex3, 
                                            SimpleRT* coarser, int coarserIndex0, int coarserIndex1, int coarserIndex2, int coarserIndex3,  
                                            UPSAMPLE_TYPE op, SAMPLE_TYPE sampleType)
    {
        assert(  sampleType == SAMPLE_ACCUMULATE && op==UPSAMPLE_BILINEAR );

        pd3dContext->CSSetShader( m_pCSUpsampleBilinearAccumulateInplace, NULL, 0 );

        //set the unordered views that we are going to be writing to
        UINT initCounts = 0;
        ID3D11UnorderedAccessView* ppUAV[4] = { finer->get_pUAV(finerIndex0), finer->get_pUAV(finerIndex1), finer->get_pUAV(finerIndex2), finer->get_pUAV(finerIndex3) };
        pd3dContext->CSSetUnorderedAccessViews( 1, 4, ppUAV, &initCounts );

        //set the coarser RT as a texture to read from
        ID3D11ShaderResourceView* ppSRV[4] = { coarser->get_pSRV(coarserIndex0), coarser->get_pSRV(coarserIndex1), coarser->get_pSRV(coarserIndex2), coarser->get_pSRV(coarserIndex3) };
        pd3dContext->CSSetShaderResources( 2, 4, ppSRV);

        //set the constant buffer
        D3D11_MAPPED_SUBRESOURCE MappedResource;
        pd3dContext->Map( m_pcb, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        CB_HIERARCHY* pcb = ( CB_HIERARCHY* )MappedResource.pData;
        pcb->finerLevelWidth = finer->getWidth3D();        
        pcb->finerLevelHeight = finer->getHeight3D();  
        pcb->finerLevelDepth = finer->getDepth3D();  
        pcb->corserLevelWidth = coarser->getWidth3D(); 
        pcb->corserLevelHeight = coarser->getHeight3D(); 
        pcb->corserLevelDepth = coarser->getDepth3D();
        pcb->g_numColsFiner = finer->getNumCols();  
        pcb->g_numRowsFiner = finer->getNumRows(); 
        pcb->g_numColsCoarser = coarser->getNumCols();  
        pcb->g_numRowsCoarser = coarser->getNumRows();    

        pd3dContext->Unmap( m_pcb, 0 );
        pd3dContext->CSSetConstantBuffers( 0, 1, &m_pcb );

        pd3dContext->Dispatch( g_LPVWIDTH/X_BLOCK_SIZE, g_LPVHEIGHT/Y_BLOCK_SIZE, g_LPVDEPTH/Z_BLOCK_SIZE );

        ID3D11UnorderedAccessView* ppUAVssNULL4[4] = { NULL, NULL, NULL, NULL };
        pd3dContext->CSSetUnorderedAccessViews( 0, 4, ppUAVssNULL4, &initCounts );
        ID3D11ShaderResourceView* ppSRVsNULL4[4] = { NULL, NULL, NULL, NULL };
        pd3dContext->CSSetShaderResources( 0, 4, ppSRVsNULL4);

    }


    virtual ~Hierarchy()
    {
        SAFE_RELEASE(m_pCSDownsampleMax);
        SAFE_RELEASE(m_pCSDownsampleMin);
        SAFE_RELEASE(m_pCSDownsampleAverage);
        SAFE_RELEASE(m_pCSUpsample);
        SAFE_RELEASE(m_pCSDownsampleAverageInplace);
        SAFE_RELEASE(m_pCSUpsampleBilinear);
        SAFE_RELEASE(m_pCSUpsampleBilinearAccumulate);
        SAFE_RELEASE(m_pCSUpsampleBilinearAccumulateInplace);
        SAFE_RELEASE(m_pcb);
    }
};

class RTCollection
{
public:
    SimpleRT** m_collection;
    int m_levels;


    RTCollection()
    {
        m_collection = NULL;
        m_levels = 0;
    }
    virtual ~RTCollection()
    {
        for(int i=0;i<m_levels;i++)
            SAFE_DELETE(m_collection[i]);
        delete[] m_collection;
    }

    virtual HRESULT Create2D( int& levels, ID3D11Device* pd3dDevice, int width2D, int height2D, int width3D, int height3D, int depth3D, DXGI_FORMAT format, bool uav = false )
    {
        return Create2D( levels, pd3dDevice, width2D, height2D, width3D, height3D, depth3D, format, uav);
    }

    virtual HRESULT Create3D( int& levels, ID3D11Device* pd3dDevice, int width, int height, int depth, DXGI_FORMAT format, bool uav = false )
    {    
         return Create3D( levels, pd3dDevice, width, height, depth, format, uav);
    }

    virtual HRESULT Create2DArray( int& levels, ID3D11Device* pd3dDevice, int width, int height, int depth, DXGI_FORMAT format, bool uav = false )
    {
         return Create2DArray( levels, pd3dDevice, width, height, depth, format, uav);    
    }

    virtual void setLPVTransformsRotatedAndOffset(float LPVscale, D3DXVECTOR3 LPVtranslate, D3DXMATRIX cameraViewMatrix, D3DXVECTOR3 viewVector)
    {
        setLPVTransformsRotatedAndOffset(LPVscale, LPVtranslate, cameraViewMatrix, viewVector);
    }
    
/*
    ID3D11Texture2D* get2DTexture(int level, int rt = 0)
    {
        return m_collection[level]->get_pTex2D(rt);
    }

    ID3D11Texture3D* get3DTexture(int level, int rt = 0)
    {
        return m_collection[level]->get_pTex3D(rt);
    }*/

    ID3D11ShaderResourceView* getShaderResourceView(int level, int rt = 0)
    {
        return m_collection[level]->get_pSRV(rt);
    }

    ID3D11ShaderResourceView** getShaderResourceViewpp(int level, int rt = 0)
    {
        return m_collection[level]->get_ppSRV(rt);
    }

    ID3D11RenderTargetView* getRenderTargetView(int level, int rt = 0)
    {
        return m_collection[level]->get_pRTV(rt);
    }

    RenderTarget* getRenderTarget(int level) {return m_collection[level]; }

    int getNumLevels() {return m_levels; };
};

class LPV_Hierarchy : public Hierarchy, public RTCollection
{
public:

    LPV_Hierarchy(ID3D11Device* pd3dDevice) : Hierarchy(pd3dDevice){        
        m_collection = NULL;
        m_levels = 0;
    };

    int getWidth2D(int level) {return m_collection[level]->getWidth2D();}
    int getHeight2D(int level) {return m_collection[level]->getHeight2D();}

    //all the LPVs in the hierarchy have the same transforms, since they are colocated
    virtual void setLPVTransformsRotatedAndOffset(float LPVscale, D3DXVECTOR3 LPVtranslate, D3DXMATRIX cameraViewMatrix, D3DXVECTOR3 viewVector)
    {
        for(int i=0; i<m_levels; i++)
                m_collection[i]->setLPVTransformsRotatedAndOffset(LPVscale, LPVtranslate, cameraViewMatrix, viewVector, m_collection[i]->getWidth3D(), m_collection[i]->getHeight3D(), m_collection[i]->getDepth3D());
    }

    virtual HRESULT Create2D( int& levels, ID3D11Device* pd3dDevice, int width2D, int height2D, int width3D, int height3D, int depth3D, DXGI_FORMAT format, bool uav = false )
    {
        HRESULT hr = S_OK;
        m_collection = new SimpleRT*[levels];
        for(int j=0;j<levels;j++) m_collection[j] = NULL;
        int i=0;
        float div=1.0f;
        for(;i<levels;i++)
        {
            //note: this is assuming that we are only using this hierarchy to for 2D textures encoding 3D textures. should have a bools saying if that is the case or not. for the moment this is true

            m_collection[i] = new SimpleRT();
            int newWidth3D = (int)(width3D/div);
            int newHeight3D = (int)(height3D/div);
            int newDepth3D = (int)(depth3D/div);
            if(newWidth3D<4 || newHeight3D<4 || newDepth3D<4 )
                break;
            int cols, rows;
            ComputeRowColsForFlat3DTexture( newDepth3D, cols, rows );
            int newWidth2D = cols*newWidth3D;
            int newHeight2D = rows*newHeight3D;

            m_collection[i]->Create2D( pd3dDevice, newWidth2D, newHeight2D, newWidth3D, newHeight3D, newDepth3D, format, uav );
            div *= 2.0f;
        }
        m_levels = i;
        levels = m_levels;
        return hr;
    }

    virtual HRESULT Create2DArray( int& levels, ID3D11Device* pd3dDevice, int width, int height, int depth, DXGI_FORMAT format, bool uav = false )
    {
        HRESULT hr = S_OK;
        m_collection = new SimpleRT*[levels];
        int i=0;
        float div=1.0f;
        for(;i<levels;i++)
        {
            m_collection[i] = new SimpleRT();
            int newWidth = (int)(width/div);
            int newHeight = (int)(height/div);
            int newDepth = (int)(depth/div);
            if(newWidth<4 || newHeight<4 || newDepth<4 )
                break;
            m_collection[i]->Create2DArray( pd3dDevice, newWidth, newHeight, newDepth, format, uav );
            div *= 2.0f;
        }
        m_levels = i;
        levels = m_levels;
        return hr;
    }

    virtual HRESULT Create3D( int& levels, ID3D11Device* pd3dDevice, int width, int height, int depth, DXGI_FORMAT format, bool uav = false )
    {
        HRESULT hr = S_OK;
        m_collection = new SimpleRT*[levels];
        int i=0;
        float div=1.0f;
        for(;i<levels;i++)
        {
            m_collection[i] = new SimpleRT();
            int newWidth = (int)(width/div);
            int newHeight = (int)(height/div);
            int newDepth = (int)(depth/div);
            if(newWidth<4 || newHeight<4 || newDepth<4 )
                break;
            m_collection[i]->Create3D( pd3dDevice, newWidth, newHeight, newDepth, format, uav );
            div *= 2.0f;
        }
        m_levels = i;
        levels = m_levels;
        return hr;
    }

    //downsample level finerLevel to level finerLevel+1
    void Downsample(ID3D11DeviceContext* pd3dContext, int finerLevel,DOWNSAMPLE_TYPE op)
    {
        int coarserLevel = finerLevel+1;
        if(finerLevel<0 || coarserLevel>=m_levels) return;
        if(m_collection[finerLevel]->getNumRTs()==1)
            Hierarchy::Downsample(pd3dContext,m_collection[finerLevel], m_collection[coarserLevel], op, 0);
        else if(m_collection[finerLevel]->getNumRTs()==4)
            Hierarchy::Downsample(pd3dContext,m_collection[finerLevel], m_collection[coarserLevel], op, 0, 1, 2, 3);
        else
            assert(false); // this path is not implemented yet!
    }

    //upsample level coarserLevel to level coarserLevel-1
    void Upsample(ID3D11DeviceContext* pd3dContext, int coarserLevel, UPSAMPLE_TYPE upsampleOp, SAMPLE_TYPE sampleType)
    {
        int finerLevel = coarserLevel-1;
        if(finerLevel<0 || coarserLevel>=m_levels) return;
        if(sampleType == SAMPLE_ACCUMULATE)
        {
            //this option is not implemented since we dont have double buffering in SimpleRT!
            assert(false);

        }
        else
            Hierarchy::Upsample(pd3dContext, m_collection[finerLevel], m_collection[coarserLevel], upsampleOp, sampleType, NULL);
    }


    ~LPV_Hierarchy()
    {
    }

};


class RTCollection_RGB
{
public:
    SimpleRT_RGB** m_collection;
    int m_levels;

    RTCollection_RGB()
    {
        m_collection = NULL;
        m_levels = 0;
    }
    virtual ~RTCollection_RGB()
    {
        for(int i=0;i<m_levels;i++)
            SAFE_DELETE(m_collection[i]);
        delete[] m_collection;
    }

    virtual HRESULT Create(int& levels, ID3D11Device* pd3dDevice, int width2D, int height2D, int width3D, int height3D, int depth3D, DXGI_FORMAT format, bool uav, bool doublebuffer, bool use3DTex, bool use2DTexArray, int numRTs = 1 )
    {
        return Create(levels, pd3dDevice, width2D, height2D, width3D, height3D, depth3D, format, uav, doublebuffer, use3DTex, use2DTexArray, numRTs);
    }

    void clearRenderTargetView(ID3D11DeviceContext* pd3dContext, const float clearColor[], bool front, int level)
    {
        m_collection[level]->clearRenderTargetView(pd3dContext,clearColor,front);
    }

    virtual void setLPVTransformsRotatedAndOffset(float LPVscale, D3DXVECTOR3 LPVtranslate, D3DXMATRIX cameraViewMatrix, D3DXVECTOR3 viewVector)
    {
        setLPVTransformsRotatedAndOffset(LPVscale, LPVtranslate, cameraViewMatrix, viewVector);
    }
    

    SimpleRT* getRed(int level, bool front=true) { return m_collection[level]->getRed(front); }
    SimpleRT* getBlue(int level, bool front=true) { return m_collection[level]->getBlue(front); }
    SimpleRT* getGreen(int level, bool front=true) { return m_collection[level]->getGreen(front); }

    SimpleRT* getRedFront(int level) { return m_collection[level]->getRedFront(); }
    SimpleRT* getBlueFront(int level) { return m_collection[level]->getBlueFront(); }
    SimpleRT* getGreenFront(int level) { return m_collection[level]->getGreenFront(); }

    SimpleRT* getRedBack(int level) { return m_collection[level]->getRedBack(); }
    SimpleRT* getBlueBack(int level) { return m_collection[level]->getBlueBack();  }
    SimpleRT* getGreenBack(int level) { return m_collection[level]->getGreenBack(); }

    void swapBuffers(int level) { m_collection[level]->swapBuffers();  }

    unsigned int getCurrentBuffer(int level) { return m_collection[level]->getCurrentBuffer(); }

    int getNumLevels() {return m_levels; };

    int getWidth3D(int level) {return  m_collection[level]->getWidth3D(); }
    int getHeight3D(int level) {return  m_collection[level]->getHeight3D(); }
    int getDepth3D(int level) {return  m_collection[level]->getDepth3D(); }

    int getWidth2D(int level) {return  m_collection[level]->getWidth2D(); }
    int getHeight2D(int level) {return  m_collection[level]->getHeight2D(); }

    int getNumCols(int level) {return  m_collection[level]->getNumCols(); }
    int getNumRows(int level) {return  m_collection[level]->getNumRows(); }



};

class Cascade
{
public:

    float m_cascadeScale;
    D3DXVECTOR3 m_cascadeTranslate;

    Cascade(float cascadeScale, D3DXVECTOR3 cascadeTranslate) { m_cascadeScale = cascadeScale; m_cascadeTranslate = cascadeTranslate; };

    float getCascadeScale(int level){ return level==0?1:level*m_cascadeScale; }

    D3DXVECTOR3 getCascadeTranslate(int level){ return m_cascadeTranslate*(float)level; }

    void setCascadeScale(float scale){ m_cascadeScale = scale; }

    void setCascadeTranslate(D3DXVECTOR3 translate){ m_cascadeTranslate = translate; }

    ~Cascade() {};
};

class LPV_RGB_Cascade : public RTCollection_RGB, public Cascade
{

    public:

    LPV_RGB_Cascade(ID3D11Device* pd3dDevice, float cascadeScale, D3DXVECTOR3 cascadeTranslate) : RTCollection_RGB(), Cascade(cascadeScale, cascadeTranslate) {     };
    virtual ~LPV_RGB_Cascade() {};

    virtual HRESULT Create(int& levels, ID3D11Device* pd3dDevice, int width2D, int height2D, int width3D, int height3D, int depth3D, DXGI_FORMAT format, bool uav, bool doublebuffer, bool use3DTex, bool use2DTexArray, int numRTs )
    {
        HRESULT hr = S_OK;

        m_collection = new SimpleRT_RGB*[levels];
        for(int j=0;j<levels;j++) m_collection[j] = NULL;

        m_levels = levels;
        for(int level=0; level<m_levels; level++)
        {
            m_collection[level] = new SimpleRT_RGB();
            V_RETURN(m_collection[level]->Create( pd3dDevice, width2D, height2D, width3D, height3D, depth3D, format, uav, doublebuffer, use3DTex, use2DTexArray, numRTs ));
        }

        return hr;
    }

    virtual void setLPVTransformsRotatedAndOffset(float LPVscale, D3DXVECTOR3 LPVtranslate, D3DXMATRIX cameraViewMatrix, D3DXVECTOR3 viewVector)
    {
        for(int level=0; level<m_levels; level++)
            m_collection[level]->setLPVTransformsRotatedAndOffset(LPVscale*getCascadeScale(level), LPVtranslate+getCascadeTranslate(level), cameraViewMatrix, viewVector, m_collection[level]->getWidth3D(), m_collection[level]->getHeight3D(), m_collection[level]->getDepth3D());
    }    

};

class LPV_Cascade : public RTCollection, public Cascade
{
    public:

    LPV_Cascade(ID3D11Device* pd3dDevice, float cascadeScale, D3DXVECTOR3 cascadeTranslate) : RTCollection(), Cascade(cascadeScale, cascadeTranslate) { };
    virtual ~LPV_Cascade() {};

    virtual HRESULT Create2D( int& levels, ID3D11Device* pd3dDevice, int width2D, int height2D, int width3D, int height3D, int depth3D, DXGI_FORMAT format, bool uav = false )
    {
        HRESULT hr = S_OK;
        m_levels = levels;

        m_collection = new SimpleRT*[levels];

        for(int level=0; level<m_levels; level++)
        {
            m_collection[level] = new SimpleRT();
            V_RETURN(m_collection[level]->Create2D( pd3dDevice, width2D, height2D, width3D, height3D, depth3D, format, uav));
        }

        return hr;
    }

    virtual HRESULT Create3D( int& levels, ID3D11Device* pd3dDevice, int width, int height, int depth, DXGI_FORMAT format, bool uav = false )
    {    
        HRESULT hr = S_OK;
        m_levels = levels;

        m_collection = new SimpleRT*[levels];

        for(int level=0; level<m_levels; level++)
        {
            m_collection[level] = new SimpleRT();
            V_RETURN(m_collection[level]->Create3D( pd3dDevice, width, height, depth, format, uav));
        }

        return hr;
    }

    virtual HRESULT Create2DArray( int& levels, ID3D11Device* pd3dDevice, int width, int height, int depth, DXGI_FORMAT format, bool uav = false )
    {    
        HRESULT hr = S_OK;
        m_levels = levels;

        m_collection = new SimpleRT*[levels];

        for(int level=0; level<m_levels; level++)
        {
            m_collection[level] = new SimpleRT();
            V_RETURN(m_collection[level]->Create2DArray( pd3dDevice, width, height, depth, format, uav));
        }

        return hr;
    }

    virtual void setLPVTransformsRotatedAndOffset(float LPVscale, D3DXVECTOR3 LPVtranslate, D3DXMATRIX cameraViewMatrix, D3DXVECTOR3 viewVector)
    {
        for(int level=0; level<m_levels; level++)
            m_collection[level]->setLPVTransformsRotatedAndOffset(LPVscale*getCascadeScale(level), LPVtranslate+getCascadeTranslate(level), cameraViewMatrix, viewVector, m_collection[level]->getWidth3D(), m_collection[level]->getHeight3D(), m_collection[level]->getDepth3D());
    }

};


class LPV_RGB_Hierarchy : public Hierarchy , public RTCollection_RGB
{
private:

public:

    //all the LPVs in the hierarchy have the same transforms, since they are colocated
    virtual void setLPVTransformsRotatedAndOffset(float LPVscale, D3DXVECTOR3 LPVtranslate, D3DXMATRIX cameraViewMatrix, D3DXVECTOR3 viewVector)
    {
        for(int level=0; level<m_levels; level++)
                m_collection[level]->setLPVTransformsRotatedAndOffset(LPVscale, LPVtranslate, cameraViewMatrix, viewVector, m_collection[level]->getWidth3D(), m_collection[level]->getHeight3D(), m_collection[level]->getDepth3D());
    }


    LPV_RGB_Hierarchy(ID3D11Device* pd3dDevice) : Hierarchy(pd3dDevice), RTCollection_RGB() {};

    virtual HRESULT Create(int& levels, ID3D11Device* pd3dDevice, int width2D, int height2D, int width3D, int height3D, int depth3D, DXGI_FORMAT format, bool uav, bool doublebuffer, bool use3DTex, bool use2DTexArray, int numRTs = 1 )
    {
        HRESULT hr = S_OK;
        m_collection = new SimpleRT_RGB*[levels];
        for(int j=0;j<levels;j++) m_collection[j] = NULL;

        int i=0;
        float div=1.0f;
        for(;i<levels;i++)
        {
            m_collection[i] = new SimpleRT_RGB();
            
            int newWidth3D = (int)(width3D/div);
            int newHeight3D = (int)(height3D/div);
            int newDepth3D = (int)(depth3D/div);
            if(newWidth3D<4 || newHeight3D<4 || (newDepth3D<4 && use3DTex) )
                break;
            int cols, rows;
            ComputeRowColsForFlat3DTexture( newDepth3D, cols, rows );
            int newWidth2D = cols*newWidth3D;
            int newHeight2D = rows*newHeight3D;

            m_collection[i]->Create( pd3dDevice, newWidth2D, newHeight2D, newWidth3D, newHeight3D, newDepth3D, format, uav, doublebuffer, use3DTex, use2DTexArray, numRTs );
            div *= 2.0f;
        }
        m_levels = i;
        levels = m_levels;
        return hr;
    }
    
    //downsample level finerLevel to level finerLevel+1
    void Downsample(ID3D11DeviceContext* pd3dContext, int finerLevel, DOWNSAMPLE_TYPE op)
    {
        int coarserLevel = finerLevel+1;
        if(finerLevel<0 || coarserLevel>=m_levels) return;
        if(m_collection[finerLevel]->getNumRTs()==1)
        {
            Hierarchy::Downsample(pd3dContext, m_collection[finerLevel]->getRed(), m_collection[coarserLevel]->getRed(), op, 0);
            Hierarchy::Downsample(pd3dContext, m_collection[finerLevel]->getGreen(), m_collection[coarserLevel]->getGreen(), op, 0);
            Hierarchy::Downsample(pd3dContext, m_collection[finerLevel]->getBlue(), m_collection[coarserLevel]->getBlue(), op, 0);
        }
        else if(m_collection[finerLevel]->getNumRTs()==4)
        {
            Hierarchy::Downsample(pd3dContext, m_collection[finerLevel]->getRed(), m_collection[coarserLevel]->getRed(), op, 0, 1, 2, 3);
            Hierarchy::Downsample(pd3dContext, m_collection[finerLevel]->getGreen(), m_collection[coarserLevel]->getGreen(), op, 0, 1, 2, 3);
            Hierarchy::Downsample(pd3dContext, m_collection[finerLevel]->getBlue(), m_collection[coarserLevel]->getBlue(), op, 0, 1, 2, 3);    
        }
        else
            assert(false); //this path is not implemented

    }

    //upsample level coarserLevel to level coarserLevel-1
    void Upsample(ID3D11DeviceContext* pd3dContext, int coarserLevel, UPSAMPLE_TYPE upsampleOp, SAMPLE_TYPE sampleType)
    {
        int finerLevel = coarserLevel-1;
        if(finerLevel<0 || coarserLevel>=m_levels) return;

        if(sampleType == SAMPLE_ACCUMULATE)
        {
            //note! for the case where we have multiple buffers we want to make one call to a specialized version of upsample, instead of iterating as below!

            //we want to upsample the coarse level and add it to the finer level. this means we have to swap buffers on the finer level (cant read and write to a float4 UAV)
            if(m_collection[finerLevel]->getNumChannels() > 1)
            {
                m_collection[finerLevel]->swapBuffers();
                for(int rt=0; rt<m_collection[finerLevel]->getNumRTs(); rt++)
                {
                    Hierarchy::Upsample(pd3dContext, m_collection[finerLevel]->getRed(), m_collection[coarserLevel]->getRed(), upsampleOp, sampleType, m_collection[finerLevel]->getRedBack()->get_pSRV(rt), rt);
                    Hierarchy::Upsample(pd3dContext, m_collection[finerLevel]->getGreen(), m_collection[coarserLevel]->getGreen(), upsampleOp, sampleType, m_collection[finerLevel]->getGreenBack()->get_pSRV(rt), rt);
                    Hierarchy::Upsample(pd3dContext, m_collection[finerLevel]->getBlue(), m_collection[coarserLevel]->getBlue(), upsampleOp, sampleType,  m_collection[finerLevel]->getBlueBack()->get_pSRV(rt), rt);
                }
            }
            else
            {
                assert(m_collection[finerLevel]->getNumRTs()==4); //only 4 rendertargets version is implemented
                //in this version we will be doing inplace updates
                Hierarchy::UpsampleAccumulateInplace4(pd3dContext, m_collection[finerLevel]->getRed(),0,1,2,3, m_collection[coarserLevel]->getRed(), 0,1,2,3, upsampleOp, sampleType);
                Hierarchy::UpsampleAccumulateInplace4(pd3dContext, m_collection[finerLevel]->getGreen(),0,1,2,3, m_collection[coarserLevel]->getGreen(),0,1,2,3, upsampleOp, sampleType);
                Hierarchy::UpsampleAccumulateInplace4(pd3dContext, m_collection[finerLevel]->getBlue(),0,1,2,3, m_collection[coarserLevel]->getBlue(),0,1,2,3, upsampleOp, sampleType);            
            }
        }
        else
        {
            Hierarchy::Upsample(pd3dContext, m_collection[finerLevel]->getRed(), m_collection[coarserLevel]->getRed(), upsampleOp, sampleType, NULL);
            Hierarchy::Upsample(pd3dContext, m_collection[finerLevel]->getGreen(), m_collection[coarserLevel]->getGreen(), upsampleOp, sampleType, NULL);
            Hierarchy::Upsample(pd3dContext, m_collection[finerLevel]->getBlue(), m_collection[coarserLevel]->getBlue(), upsampleOp, sampleType, NULL);
        }
    }


    virtual ~LPV_RGB_Hierarchy()
    {
    }
};


//assuming that a DepthRT will not want to have more than a single Render target / texture (vs number of them as in SimpleRT)
class DepthRT : public RenderTarget
{
public:

    ID3D11Texture2D* pTexture;
    ID3D11DepthStencilView*    pDSV;


    DepthRT( ID3D11Device* pd3dDevice, D3D11_TEXTURE2D_DESC* pTexDesc ): RenderTarget()
    {
        HRESULT hr;
        V( pd3dDevice->CreateTexture2D( pTexDesc, NULL, &pTexture ) );

        RenderTarget::create(1);

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        V( pd3dDevice->CreateShaderResourceView( pTexture, &srvDesc, get_ppSRV(0) ) );

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
        dsvDesc.Texture2D.MipSlice = 0;
        dsvDesc.Flags = 0;
        V( pd3dDevice->CreateDepthStencilView( pTexture, &dsvDesc, &pDSV ) );
    }

    ~DepthRT()
    {
        SAFE_RELEASE( pTexture );
        SAFE_RELEASE( pDSV );
    }

    operator ID3D11Texture2D*()
    {
        return pTexture;
    }

    operator ID3D11DepthStencilView*()
    {
        return pDSV;
    }

    operator ID3D11ShaderResourceView*()
    {
        return m_pSRVs[0];
    }
};

#endif
