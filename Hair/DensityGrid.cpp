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

#pragma warning(disable:4995) // avoid warning for "...was marked as #pragma deprecated"
#pragma warning(disable:4244) // avoid warning for "conversion from 'float' to 'int'"
#pragma warning(disable:4201) //avoid warning from mmsystem.h "warning C4201: nonstandard extension used : nameless struct/union"

#include "Hair.h"
#include "DensityGrid.h"
#include "Common.h"
#include <Strsafe.h>

HairGrid::HairGrid( ID3D11Device* pd3dDevice, ID3D11DeviceContext *pd3dContext ):renderQuadBuffer(NULL),renderQuadBufferForGrid(NULL),m_pEffect(NULL), m_cols(0), m_rows(0),
m_fluidGridWidth(0),m_fluidGridHeight(0),m_fluidGridDepth(0)
{
    m_pD3DDevice = NULL;
    m_pD3DContext = NULL;

    SAFE_ACQUIRE(m_pD3DDevice, pd3dDevice);
    SAFE_ACQUIRE(m_pD3DContext, pd3dContext);

	//formats
    RenderTargetFormats [RENDER_TARGET_DENSITY]         = DXGI_FORMAT_R16G16B16A16_FLOAT;
    RenderTargetFormats [RENDER_TARGET_DENSITY_DEMUX]      = DXGI_FORMAT_R16_FLOAT;
    RenderTargetFormats [RENDER_TARGET_DENSITY_BLUR_TEMP]  = DXGI_FORMAT_R16_FLOAT;
    RenderTargetFormats [RENDER_TARGET_DENSITY_BLUR]       = DXGI_FORMAT_R16_FLOAT;
    RenderTargetFormats [RENDER_TARGET_VOXELIZED_OBSTACLES] = DXGI_FORMAT_R16_FLOAT;

    //textures
	pDensity2DTextureArray = NULL;
    pDensityDemux2DTextureArray = NULL;
    pVoxelizedObstacle2DTextureArray = NULL;
    pDensityBlurTemp2DTextureArray = NULL;
    pDensityBlur2DTextureArray = NULL;
	m_pFluidObstacleTexture3D = NULL;

	m_pRenderTargetViewFluidObstacles = NULL;
    
	//render target views
	memset(pDensityRenderTargetViews, 0, sizeof(pDensityRenderTargetViews));
	memset(pDensityDemuxRenderTargetViews, 0, sizeof(pDensityDemuxRenderTargetViews));
	memset(pVoxelizedObstacleRenderTargetViews, 0, sizeof(pVoxelizedObstacleRenderTargetViews));
	memset(pDensityBlurTempRenderTargetViews, 0, sizeof(pDensityBlurTempRenderTargetViews));
	memset(pDensityBlurRenderTargetViews, 0, sizeof(pDensityBlurRenderTargetViews));
    
	//shader resource views
 	memset(pRenderTargetShaderViews, 0, sizeof(pRenderTargetShaderViews));
}

HRESULT HairGrid::ReloadShader(ID3DX11Effect* pEffect,TCHAR pEffectPath[MAX_PATH])
{
	SAFE_RELEASE(m_pEffect);
	m_pEffect = NULL;
	return LoadShaders(pEffect,pEffectPath);
}

HRESULT HairGrid::Initialize( int width, int height, int depth, ID3DX11Effect* pEffect,TCHAR pEffectPath[MAX_PATH] )
{
    HRESULT hr;
 
	m_totalDepth = depth;

    m_Width = m_dim[0] = width;
	m_Height = m_dim[1] = height;
	m_Depth = m_dim[2] = (depth/4.0 + 0.5);

    m_rows =(int)floorf(sqrtf((float)m_totalDepth));
    m_cols = m_rows;
    while( m_rows * m_cols < m_totalDepth ) 
        m_cols++;
    assert( m_rows*m_cols >= m_totalDepth );   
    
    assert( m_totalDepth < MAX_RTS_TOTAL );

    V_RETURN(LoadShaders(pEffect,pEffectPath));


    //Create the textures ------------------------------------------------------------------------------------------------- 

    //create the density texture array
	//each texture has 4 channels so we need m_totalDepth/4 textures to store
	D3D11_TEXTURE2D_DESC desc;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.CPUAccessFlags = 0; 
    desc.MipLevels = 1;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Width =  m_Width;
    desc.Height = m_Height;
	desc.SampleDesc.Count   = 1;
	desc.SampleDesc.Quality = 0;


	//create the demux and blur textures
	desc.ArraySize = m_totalDepth;
	desc.Format = RenderTargetFormats[RENDER_TARGET_DENSITY_DEMUX];
	V_RETURN( m_pD3DDevice->CreateTexture2D(&desc,NULL,&pDensityDemux2DTextureArray)); 

	desc.ArraySize = m_totalDepth;
	desc.Format = RenderTargetFormats[RENDER_TARGET_VOXELIZED_OBSTACLES];
	V_RETURN( m_pD3DDevice->CreateTexture2D(&desc,NULL,&pVoxelizedObstacle2DTextureArray)); 

	desc.ArraySize = m_totalDepth;
	desc.Format = RenderTargetFormats[RENDER_TARGET_DENSITY_BLUR_TEMP];
	V_RETURN( m_pD3DDevice->CreateTexture2D(&desc,NULL,&pDensityBlurTemp2DTextureArray)); 
	
	desc.ArraySize = m_totalDepth;
	desc.Format = RenderTargetFormats[RENDER_TARGET_DENSITY_BLUR];
	V_RETURN( m_pD3DDevice->CreateTexture2D(&desc,NULL,&pDensityBlur2DTextureArray)); 

	desc.Format = RenderTargetFormats[RENDER_TARGET_DENSITY];
    desc.ArraySize = m_Depth;
	V_RETURN( m_pD3DDevice->CreateTexture2D(&desc,NULL,&pDensity2DTextureArray));
	
    

	//create the renderTargetViews -------------------------------------------------------------------------------------------------
    D3D11_RENDER_TARGET_VIEW_DESC DescRT;
    DescRT.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	DescRT.Texture2DArray.MipSlice = 0;	
	DescRT.Texture2DArray.ArraySize = 1;

	DescRT.Format = RenderTargetFormats[RENDER_TARGET_DENSITY_DEMUX];
	for(int i=0;i<m_totalDepth;i++)
	{
	    DescRT.Texture2DArray.FirstArraySlice = i;
        V_RETURN( m_pD3DDevice->CreateRenderTargetView( pDensityDemux2DTextureArray, &DescRT, &pDensityDemuxRenderTargetViews[i]) );
		V_RETURN( m_pD3DDevice->CreateRenderTargetView( pVoxelizedObstacle2DTextureArray, &DescRT, &pVoxelizedObstacleRenderTargetViews[i]) );
        V_RETURN( m_pD3DDevice->CreateRenderTargetView( pDensityBlurTemp2DTextureArray, &DescRT, &pDensityBlurTempRenderTargetViews[i]) );
        V_RETURN( m_pD3DDevice->CreateRenderTargetView( pDensityBlur2DTextureArray, &DescRT, &pDensityBlurRenderTargetViews[i]) );
	}

    DescRT.Format = RenderTargetFormats[RENDER_TARGET_DENSITY];
	for(int i=0;i<m_Depth;i++)
	{
	    DescRT.Texture2DArray.FirstArraySlice = i;
        V_RETURN( m_pD3DDevice->CreateRenderTargetView( pDensity2DTextureArray, &DescRT, &pDensityRenderTargetViews[i]) );
	}




    //-------------------------------------------------------------------------------------------------------------------------------
    // Create the "shader resource view" (SRView) and "shader resource variable" (SRVar) for the given texture 
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
    ZeroMemory( &SRVDesc, sizeof(SRVDesc) );
    SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
	SRVDesc.Texture2DArray.FirstArraySlice = 0;
	SRVDesc.Texture2DArray.MipLevels = 1;
    SRVDesc.Texture2DArray.MostDetailedMip = 0;

	SRVDesc.Texture2DArray.ArraySize = m_totalDepth;
    SRVDesc.Format = RenderTargetFormats[RENDER_TARGET_DENSITY_DEMUX];
    V_RETURN(CreateRTTextureAsShaderResource(RENDER_TARGET_DENSITY_DEMUX,"Texture_density_Demux",m_pEffect,&SRVDesc));
    
	SRVDesc.Texture2DArray.ArraySize = m_totalDepth;
    SRVDesc.Format = RenderTargetFormats[RENDER_TARGET_VOXELIZED_OBSTACLES];
    V_RETURN(CreateRTTextureAsShaderResource(RENDER_TARGET_VOXELIZED_OBSTACLES,"Texture_Voxelized_Obstacles",m_pEffect,&SRVDesc));

	SRVDesc.Texture2DArray.ArraySize = m_totalDepth;
	SRVDesc.Format = RenderTargetFormats[RENDER_TARGET_DENSITY_BLUR_TEMP];
    V_RETURN(CreateRTTextureAsShaderResource(RENDER_TARGET_DENSITY_BLUR_TEMP,"Texture_density_Blur_Temp",m_pEffect,&SRVDesc));
    
	SRVDesc.Texture2DArray.ArraySize = m_totalDepth;
	SRVDesc.Format = RenderTargetFormats[RENDER_TARGET_DENSITY_BLUR];
    V_RETURN(CreateRTTextureAsShaderResource(RENDER_TARGET_DENSITY_BLUR,"Texture_density_Blur",m_pEffect,&SRVDesc));
	
	SRVDesc.Texture2DArray.ArraySize = m_Depth;
	SRVDesc.Format = RenderTargetFormats[RENDER_TARGET_DENSITY];
    V_RETURN(CreateRTTextureAsShaderResource(RENDER_TARGET_DENSITY,"Texture_density",m_pEffect,&SRVDesc));


    Reset();

    //make the vertex buffers--------------------------------------------------------------------------------------------------------

	// Create layout
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = 
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,       0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT,       0,12, D3D11_INPUT_PER_VERTEX_DATA, 0 }, 
    };
    UINT numElements = sizeof(layoutDesc)/sizeof(layoutDesc[0]);
    D3DX11_PASS_DESC PassDesc;
    TechniqueDrawTexture->GetPassByIndex( 0 )->GetDesc( &PassDesc );
    V_RETURN(m_pD3DDevice->CreateInputLayout( layoutDesc, numElements, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &quad_layout ));

	
	// Vertex buffer for "m_totalDepth" number quads to draw all the slices of the 3D-texture as a flat 3D-texture to the screen
    // (used to draw all the individual slices at once to the screen buffer)
	GRID_TEXTURE_DISPLAY_STRUCT *renderQuad(NULL);
    #define VERTICES_PER_SLICE 6
    m_numVerticesRenderQuad = VERTICES_PER_SLICE * m_totalDepth;
    renderQuad = new GRID_TEXTURE_DISPLAY_STRUCT[ m_numVerticesRenderQuad ];
    int index = 0;
    for(int z=0; z<m_totalDepth; z++)
        InitScreenSlice(&renderQuad,z,index);
    V_RETURN(CreateVertexBuffer(sizeof(GRID_TEXTURE_DISPLAY_STRUCT)*m_numVerticesRenderQuad,D3D11_BIND_VERTEX_BUFFER, &renderQuadBuffer, renderQuad, m_numVerticesRenderQuad));
    delete [] renderQuad;
    renderQuad = NULL;

    //Vertex buffer for rendering to a 2D texture array
    m_numVerticesRenderQuadsForGrid = VERTICES_PER_SLICE;
    renderQuad = new GRID_TEXTURE_DISPLAY_STRUCT[ m_numVerticesRenderQuadsForGrid ];
    InitGridSlice(&renderQuad);
    V_RETURN(CreateVertexBuffer(sizeof(GRID_TEXTURE_DISPLAY_STRUCT)*m_numVerticesRenderQuadsForGrid,D3D11_BIND_VERTEX_BUFFER, &renderQuadBufferForGrid, renderQuad, m_numVerticesRenderQuadsForGrid));
    delete [] renderQuad;
    renderQuad = NULL;


    return S_OK;
}

HairGrid::~HairGrid( void )
{

    SAFE_RELEASE(m_pD3DDevice);
	SAFE_RELEASE(m_pD3DContext);
    SAFE_RELEASE(m_pEffect);

	SAFE_RELEASE(pDensity2DTextureArray);
    SAFE_RELEASE(pDensityDemux2DTextureArray);
    SAFE_RELEASE(pVoxelizedObstacle2DTextureArray);
    SAFE_RELEASE(pDensityBlurTemp2DTextureArray);
	SAFE_RELEASE(pDensityBlur2DTextureArray);
	
	for(int i=0;i<MAX_RT_GROUPS*8;i++)
       SAFE_RELEASE(pDensityRenderTargetViews[i]);
    
	for(int i=0;i<MAX_RTS_TOTAL;i++)
	{
	    SAFE_RELEASE(pDensityDemuxRenderTargetViews[i]);
	    SAFE_RELEASE(pVoxelizedObstacleRenderTargetViews[i]);		
		SAFE_RELEASE(pDensityBlurTempRenderTargetViews[i]);
		SAFE_RELEASE(pDensityBlurRenderTargetViews[i]);
	}

    for(int i=0;i<NUM_RENDER_TARGET_TYPES;i++)
		SAFE_RELEASE(pRenderTargetShaderViews[i]);

	SAFE_RELEASE(renderQuadBuffer);
	SAFE_RELEASE(renderQuadBufferForGrid);
    SAFE_RELEASE(quad_layout);

	SAFE_RELEASE(m_pRenderTargetViewFluidObstacles);
	SAFE_RELEASE(m_pFluidObstacleTexture3D);
    m_pFluidObstacleTexture3D = NULL;
}


void HairGrid::Reset( void )
{
    float color[4] = {0, 0, 0, 0 };

	for(int i=0;i<m_Depth;i++)
        m_pD3DContext->ClearRenderTargetView( pDensityRenderTargetViews[i], color );
	
	for(int i=0;i<m_totalDepth;i++)
	{
		m_pD3DContext->ClearRenderTargetView( pDensityDemuxRenderTargetViews[i], color );
		m_pD3DContext->ClearRenderTargetView( pVoxelizedObstacleRenderTargetViews[i], color );
		m_pD3DContext->ClearRenderTargetView( pDensityBlurTempRenderTargetViews[i], color );
		m_pD3DContext->ClearRenderTargetView( pDensityBlurRenderTargetViews[i], color );
	}
}



HRESULT HairGrid::LoadShaders(ID3DX11Effect* pEffect,TCHAR pEffectPath[MAX_PATH])
{
    HRESULT hr = S_OK;
    SAFE_ACQUIRE(m_pEffect,pEffect);

    V_GET_TECHNIQUE_RET( m_pEffect, pEffectPath, TechniqueDrawTexture,"DrawTexture");
    V_GET_TECHNIQUE_RET( m_pEffect, pEffectPath, TechniqueDrawTextureDemuxed,"DrawTextureDemuxed");
    V_GET_TECHNIQUE_RET( m_pEffect, pEffectPath, TechniqueDemux,"TextureDemux");
    V_GET_TECHNIQUE_RET( m_pEffect, pEffectPath, TechniqueVoxelizeObstacles,"VoxelizeObstacles");
    V_GET_TECHNIQUE_RET( m_pEffect, pEffectPath, TechniqueDemuxTo3DFluidObstacles,"DemuxTo3DFluidObstacles");

    V_GET_VARIABLE_RET( m_pEffect, pEffectPath, m_TextureWidthVar, "textureWidth", AsScalar );
    V_GET_VARIABLE_RET( m_pEffect, pEffectPath, m_TextureHeightVar, "textureHeight", AsScalar );
	V_GET_VARIABLE_RET( m_pEffect, pEffectPath, m_TextureDepthVar, "textureDepth", AsScalar );
    V_GET_VARIABLE_RET( m_pEffect, pEffectPath, m_RowWidth, "rowWidth", AsScalar );
    V_GET_VARIABLE_RET( m_pEffect, pEffectPath, m_ColWidth, "colWidth", AsScalar );
    V_GET_VARIABLE_RET( m_pEffect, pEffectPath, m_textureIndexShaderVariable, "textureIndex", AsScalar );
    V_GET_VARIABLE_RET( m_pEffect, pEffectPath, m_GridZShaderVariable, "gridZIndex", AsScalar );
    V_GET_VARIABLE_RET( m_pEffect, pEffectPath, m_BlurDirectionShaderVariable, "blurDirection", AsVector );
    V_GET_VARIABLE_RET( m_pEffect, pEffectPath, m_TextureToBlurShaderShaderVariable, "Texure_to_blur", AsShaderResource );

	V_RETURN(m_TextureWidthVar->SetFloat( float(m_Width)));
    V_RETURN(m_TextureHeightVar->SetFloat(float(m_Height)));
    V_RETURN(m_TextureDepthVar->SetFloat(float(m_totalDepth)));
    V_RETURN(m_RowWidth->SetInt( g_Width/m_cols ));
	V_RETURN(m_ColWidth->SetInt( g_Height/m_rows ));

	m_pEffect->GetVariableByName("fluidTextureWidth")->AsScalar()->SetInt(m_fluidGridWidth);
	m_pEffect->GetVariableByName("fluidTextureHeight")->AsScalar()->SetInt(m_fluidGridHeight);
	m_pEffect->GetVariableByName("fluidTextureDepth")->AsScalar()->SetInt(m_fluidGridDepth);

	pShaderResourceVariables[RENDER_TARGET_DENSITY_DEMUX] = m_pEffect->GetVariableByName( "Texture_density_Demux" )->AsShaderResource();
	pShaderResourceVariables[RENDER_TARGET_VOXELIZED_OBSTACLES] = m_pEffect->GetVariableByName( "Texture_Voxelized_Obstacles" )->AsShaderResource();
	pShaderResourceVariables[RENDER_TARGET_DENSITY_BLUR_TEMP] = m_pEffect->GetVariableByName( "Texture_density_Blur_Temp" )->AsShaderResource();
	pShaderResourceVariables[RENDER_TARGET_DENSITY_BLUR] = m_pEffect->GetVariableByName( "Texture_density_Blur" )->AsShaderResource();
	pShaderResourceVariables[RENDER_TARGET_DENSITY] = m_pEffect->GetVariableByName( "Texture_density" )->AsShaderResource();

    for(int rtIndex =0; rtIndex<int(NUM_RENDER_TARGET_TYPES);rtIndex++)
	{
    	assert(pShaderResourceVariables[rtIndex]->IsValid());
        V_RETURN(pShaderResourceVariables[rtIndex]->SetResource( pRenderTargetShaderViews[rtIndex] ));
	}
	return hr;
}


HRESULT HairGrid::CreateRTTextureAsShaderResource(RENDER_TARGET rtIndex, LPCSTR shaderTextureName,ID3DX11Effect* pEffect,D3D11_SHADER_RESOURCE_VIEW_DESC *SRVDesc )
{
    HRESULT hr;
 
	// Create the "shader resource view" (SRView) and "shader resource variable" (SRVar) for the given texture 
    SAFE_RELEASE(pRenderTargetShaderViews[rtIndex]);
	if( rtIndex == RENDER_TARGET_DENSITY )
	{	 V_RETURN(m_pD3DDevice->CreateShaderResourceView( pDensity2DTextureArray,SRVDesc, &pRenderTargetShaderViews[rtIndex])); }
	else if( rtIndex == RENDER_TARGET_DENSITY_DEMUX) 
	{    V_RETURN(m_pD3DDevice->CreateShaderResourceView( pDensityDemux2DTextureArray,SRVDesc, &pRenderTargetShaderViews[rtIndex])); }
	else if( rtIndex == RENDER_TARGET_VOXELIZED_OBSTACLES) 
	{    V_RETURN(m_pD3DDevice->CreateShaderResourceView( pVoxelizedObstacle2DTextureArray,SRVDesc, &pRenderTargetShaderViews[rtIndex])); }
	else if(rtIndex == RENDER_TARGET_DENSITY_BLUR_TEMP)
	{    V_RETURN(m_pD3DDevice->CreateShaderResourceView( pDensityBlurTemp2DTextureArray,SRVDesc, &pRenderTargetShaderViews[rtIndex])); }
	else if(rtIndex == RENDER_TARGET_DENSITY_BLUR)
	{    V_RETURN(m_pD3DDevice->CreateShaderResourceView( pDensityBlur2DTextureArray,SRVDesc, &pRenderTargetShaderViews[rtIndex])); }
	else
		return S_FALSE;
	
	pShaderResourceVariables[rtIndex] = pEffect->GetVariableByName( shaderTextureName )->AsShaderResource();
    assert(pShaderResourceVariables[rtIndex]->IsValid());
    
    return S_OK;
}



//assuming that we are going to be rendering parallel to the z direction 
HRESULT HairGrid::AddHairDensity(D3DXMATRIX& objToVolumeXForm, HRESULT (*DrawHair)(D3DXMATRIX&,ID3D11Device*,ID3D11DeviceContext*,float,float,int))
{
    HRESULT hr = S_OK;

    float color[4] = {0, 0, 0, 0 };

    D3DXMATRIX proj;
    D3DXMATRIX worldViewProj;

	//render to 8 render targets at once (use 8 MRT)

    //draw all the hair
	for(int z=0;z<m_Depth;z+=8) 
	{
		//create, clear and set the viewport
		for(int i=z;i<z+8;i++)
		{
		    if(i>=m_Depth)
                break;
			
       	    m_pD3DContext->ClearRenderTargetView( pDensityRenderTargetViews[i], color );
    	}

		int numSlices;
		if(z<m_Depth-8)
			numSlices = 8;
		else
			numSlices = m_Depth-z;
          

		ID3D11RenderTargetView **pCnDRTs = new ID3D11RenderTargetView *[numSlices];
		for(int i=0;i<numSlices;i++)
            pCnDRTs[i] = pDensityRenderTargetViews[i+z];   
	
        m_pD3DContext->OMSetRenderTargets( numSlices, pCnDRTs, NULL ); 

		//set the viewport
        D3D11_VIEWPORT viewport = { 0, 0, m_Width, m_Height, 0.0f, 1.0f };
        m_pD3DContext->RSSetViewports(1, &viewport);

	    //set the projection matrix with the correct near, far plane
        D3DXMatrixOrthoOffCenterLH(&proj, -0.5, 0.5, -0.5, 0.5, ((float)z)/m_Depth - 0.5f, (float)(z+8)/m_Depth - 0.5f );
        D3DXMatrixMultiply(&worldViewProj, &objToVolumeXForm, &proj);

		DrawHair(worldViewProj, m_pD3DDevice, m_pD3DContext, 0.0f, 1.0/32.0, numSlices); //32 = 8 RTs * 4 slices per RT 

		delete[] pCnDRTs;

	}

    m_pD3DContext->OMSetRenderTargets(0, NULL, NULL);

	return hr;
}


HRESULT HairGrid::demultiplexTexture()
{
    HRESULT hr = S_OK;

	UINT stride[1] = { sizeof(GRID_TEXTURE_DISPLAY_STRUCT) };
    UINT offset[1] = { 0 };

	pShaderResourceVariables[RENDER_TARGET_DENSITY]->SetResource( pRenderTargetShaderViews[RENDER_TARGET_DENSITY] );
	pShaderResourceVariables[RENDER_TARGET_VOXELIZED_OBSTACLES]->SetResource( pRenderTargetShaderViews[RENDER_TARGET_VOXELIZED_OBSTACLES] );

    //set the viewport
    D3D11_VIEWPORT viewport = { 0, 0, m_Width, m_Height, 0.0f, 1.0f };
    m_pD3DContext->RSSetViewports(1, &viewport);

    float color[4] = {0, 0, 0, 0 };

    for(int i=0;i<m_totalDepth;i++)
	{   
   	    m_pD3DContext->ClearRenderTargetView( pDensityDemuxRenderTargetViews[i], color );
		m_pD3DContext->OMSetRenderTargets( 1, &(pDensityDemuxRenderTargetViews[i]), NULL ); 
    	m_GridZShaderVariable->SetInt(i);
        V_RETURN(TechniqueDemux->GetPassByIndex(0)->Apply(0, m_pD3DContext));

		//render the quad
		m_pD3DContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        m_pD3DContext->IASetInputLayout( quad_layout );
        m_pD3DContext->IASetVertexBuffers( 0, 1,  &renderQuadBufferForGrid, stride, offset );
        m_pD3DContext->Draw( m_numVerticesRenderQuadsForGrid, 0 ); 
		
	}
	
	pShaderResourceVariables[RENDER_TARGET_VOXELIZED_OBSTACLES]->SetResource( NULL );
	pShaderResourceVariables[RENDER_TARGET_DENSITY]->SetResource( NULL );
	m_pD3DContext->OMSetRenderTargets(0, NULL, NULL);
    TechniqueDemux->GetPassByIndex(0)->Apply(0, m_pD3DContext);

	return hr;
}

HRESULT HairGrid::voxelizeObstacles()
{
    HRESULT hr = S_OK;

	UINT stride[1] = { sizeof(GRID_TEXTURE_DISPLAY_STRUCT) };
    UINT offset[1] = { 0 };

    //set the viewport
    D3D11_VIEWPORT viewport = { 0, 0, m_Width, m_Height, 0.0f, 1.0f };
    m_pD3DContext->RSSetViewports(1, &viewport);

    float color[4] = {0, 0, 0, 0 };

    for(int i=0;i<m_totalDepth;i++)
	{   
   	    m_pD3DContext->ClearRenderTargetView( pVoxelizedObstacleRenderTargetViews[i], color );
		m_pD3DContext->OMSetRenderTargets( 1, &(pVoxelizedObstacleRenderTargetViews[i]), NULL ); 
    	m_GridZShaderVariable->SetInt(i);
        V_RETURN(TechniqueVoxelizeObstacles->GetPassByIndex(0)->Apply(0, m_pD3DContext));

		//render the quad
		m_pD3DContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
        m_pD3DContext->IASetInputLayout( quad_layout );
        m_pD3DContext->IASetVertexBuffers( 0, 1,  &renderQuadBufferForGrid, stride, offset );
        m_pD3DContext->Draw( m_numVerticesRenderQuadsForGrid, 0 ); 
		
	}
	
	m_pD3DContext->OMSetRenderTargets(0, NULL, NULL);
    TechniqueVoxelizeObstacles->GetPassByIndex(0)->Apply(0, m_pD3DContext);

	return hr;
}


HRESULT HairGrid::setDemuxTexture()
{
    HRESULT hr = S_OK;
	pShaderResourceVariables[RENDER_TARGET_DENSITY_DEMUX]->SetResource( pRenderTargetShaderViews[RENDER_TARGET_DENSITY_DEMUX] );
	return hr;
}
HRESULT HairGrid::setObstacleVoxelizedTexture()
{
    HRESULT hr = S_OK;
	pShaderResourceVariables[RENDER_TARGET_VOXELIZED_OBSTACLES]->SetResource( pRenderTargetShaderViews[RENDER_TARGET_VOXELIZED_OBSTACLES] );
	return hr;
}
HRESULT HairGrid::setBlurTexture()
{
    HRESULT hr = S_OK;
	pShaderResourceVariables[RENDER_TARGET_DENSITY_BLUR]->SetResource( pRenderTargetShaderViews[RENDER_TARGET_DENSITY_BLUR] );
	return hr;
}
HRESULT HairGrid::unsetDemuxTexture()
{
    HRESULT hr = S_OK;
	pShaderResourceVariables[RENDER_TARGET_DENSITY_DEMUX]->SetResource( NULL );
	return hr;
}
HRESULT HairGrid::unsetObstacleVoxelizedTexture()
{
    HRESULT hr = S_OK;
	pShaderResourceVariables[RENDER_TARGET_VOXELIZED_OBSTACLES]->SetResource( NULL );
	return hr;
}
HRESULT HairGrid::unsetBlurTexture()
{
    HRESULT hr = S_OK;
	pShaderResourceVariables[RENDER_TARGET_DENSITY_BLUR]->SetResource( NULL );
	return hr;
}



HRESULT HairGrid::demultiplexTextureTo3DFluidObstacles()
{
    HRESULT hr = S_OK;

    // All drawing will take place to a viewport with the dimensions of a 3D texture slice
    D3D11_VIEWPORT rtViewport;
    rtViewport.TopLeftX = 0;
    rtViewport.TopLeftY = 0;
    rtViewport.MinDepth = 0;
    rtViewport.MaxDepth = 1;
    rtViewport.Width =  m_fluidGridWidth;  
    rtViewport.Height = m_fluidGridHeight; 
    m_pD3DContext->RSSetViewports(1,&rtViewport);

    float clearValue[4] = {1, 1, 1, 1 };
    m_pD3DContext->ClearRenderTargetView( m_pRenderTargetViewFluidObstacles, clearValue );


    pShaderResourceVariables[RENDER_TARGET_DENSITY]->SetResource( pRenderTargetShaderViews[RENDER_TARGET_DENSITY] );
    V_RETURN(TechniqueDemuxTo3DFluidObstacles->GetPassByIndex(0)->Apply(0, m_pD3DContext));
    m_pD3DContext->OMSetRenderTargets( 1, &m_pRenderTargetViewFluidObstacles , NULL ); 
    g_fluid->drawGridSlices();

	
	m_pD3DContext->OMSetRenderTargets(0, NULL, NULL);

	return hr;
}


HRESULT HairGrid::DisplayTexture(RENDER_TARGET rtIndex)
{
    HRESULT hr = S_OK;
    D3D11_VIEWPORT rtViewport;
    rtViewport.TopLeftX = 0;
    rtViewport.TopLeftY = 0;
    rtViewport.MinDepth = 0;
    rtViewport.MaxDepth = 1;
    rtViewport.Width = g_Width;
    rtViewport.Height = g_Height;
    m_pD3DContext->RSSetViewports(1,&rtViewport);

    ID3D11RenderTargetView* pRTV = DXUTGetD3D11RenderTargetView();
    m_pD3DContext->OMSetRenderTargets( 1, &pRTV , NULL ); 

    //bind texture as shader resource
    V_RETURN(pShaderResourceVariables[rtIndex]->SetResource( pRenderTargetShaderViews[rtIndex] ));

	//set the correct texture index for the shader
	m_textureIndexShaderVariable->SetInt(rtIndex);

	//set the correct technique
	if(rtIndex==RENDER_TARGET_DENSITY)
	{     V_RETURN(TechniqueDrawTexture->GetPassByIndex(0)->Apply(0, m_pD3DContext));}
	else
	{	 V_RETURN(TechniqueDrawTextureDemuxed->GetPassByIndex(0)->Apply(0, m_pD3DContext));}

	DrawSlicesToScreen();

    // Unset resources and apply technique (so that the resource is actually unbound)
    pShaderResourceVariables[rtIndex]->SetResource(NULL);
	if(rtIndex==RENDER_TARGET_DENSITY)
	{     V_RETURN(TechniqueDrawTexture->GetPassByIndex(0)->Apply(0, m_pD3DContext));}
	else
	{	 V_RETURN(TechniqueDrawTextureDemuxed->GetPassByIndex(0)->Apply(0, m_pD3DContext));}
 
    return hr; 
}

void HairGrid::DrawSlicesToScreen( void )
{
    UINT stride[1] = { sizeof(GRID_TEXTURE_DISPLAY_STRUCT) };
    UINT offset[1] = { 0 };

    m_pD3DContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    m_pD3DContext->IASetInputLayout( quad_layout );
    m_pD3DContext->IASetVertexBuffers( 0, 1,  &renderQuadBuffer, stride, offset );
    m_pD3DContext->Draw( m_numVerticesRenderQuad, 0 ); 
}

void HairGrid::InitScreenSlice(GRID_TEXTURE_DISPLAY_STRUCT** vertices, int z, int& index )
{
    GRID_TEXTURE_DISPLAY_STRUCT tempVertex1;
    GRID_TEXTURE_DISPLAY_STRUCT tempVertex2;
    GRID_TEXTURE_DISPLAY_STRUCT tempVertex3;
    GRID_TEXTURE_DISPLAY_STRUCT tempVertex4;

    // compute the offset (px, py) in the "flat 3D-texture" space for the slice with given 'z' coordinate
    int column      = z % m_cols;
    int row         = (int)floorf((float)(z/m_cols));
    int px = column * m_dim[0];
    int py = row    * m_dim[1];

    float w = float(m_dim[0]);
    float h = float(m_dim[1]);

    float Width  = float(m_cols * m_dim[0]);
    float Height = float(m_rows * m_dim[1]);

    float left   = -1.0f + (px*2.0f/Width);
    float right  = -1.0f + ((px+w)*2.0f/Width);
    float top    =  1.0f - ((py+h)*2.0f/Height);
    float bottom =  1.0f - py*2.0f/Height;

    tempVertex1.Pos   = D3DXVECTOR3( left   , bottom    , 0.0f      );
    tempVertex1.Tex   = D3DXVECTOR3( 0      , 0         , float(z)  );

    tempVertex2.Pos   = D3DXVECTOR3( right  , bottom    , 0.0f      );
    tempVertex2.Tex   = D3DXVECTOR3( w      ,  0        , float(z)  );

    tempVertex3.Pos   = D3DXVECTOR3( right  , top       , 0.0f      );
    tempVertex3.Tex   = D3DXVECTOR3( w      ,  h        , float(z)  );

    tempVertex4.Pos   = D3DXVECTOR3( left   , top       , 0.0f      );
    tempVertex4.Tex   = D3DXVECTOR3( 0      ,  h        , float(z)  );

    (*vertices)[index++] = tempVertex1;
    (*vertices)[index++] = tempVertex2;
    (*vertices)[index++] = tempVertex3;
    (*vertices)[index++] = tempVertex1;
    (*vertices)[index++] = tempVertex3;
    (*vertices)[index++] = tempVertex4;
}


void HairGrid::InitGridSlice(GRID_TEXTURE_DISPLAY_STRUCT** vertices )
{
    GRID_TEXTURE_DISPLAY_STRUCT tempVertex1;
    GRID_TEXTURE_DISPLAY_STRUCT tempVertex2;
    GRID_TEXTURE_DISPLAY_STRUCT tempVertex3;
    GRID_TEXTURE_DISPLAY_STRUCT tempVertex4;

    float w = float(m_dim[0]);
    float h = float(m_dim[1]);

    float left   = -1.0f;
    float right  =  1.0f;
    float top    = -1.0f;
    float bottom =  1.0f;

    tempVertex1.Pos   = D3DXVECTOR3( left   , bottom    , 0.0f      );
    tempVertex1.Tex   = D3DXVECTOR3( 0      , 0         , 0         );

    tempVertex2.Pos   = D3DXVECTOR3( right  , bottom    , 0.0f      );
    tempVertex2.Tex   = D3DXVECTOR3( w      ,  0        , 0         );

    tempVertex3.Pos   = D3DXVECTOR3( right  , top       , 0.0f      );
    tempVertex3.Tex   = D3DXVECTOR3( w      ,  h        , 0         );

    tempVertex4.Pos   = D3DXVECTOR3( left   , top       , 0.0f      );
    tempVertex4.Tex   = D3DXVECTOR3( 0      ,  h        , 0         );

	 int index = 0;
    (*vertices)[index++] = tempVertex1;
    (*vertices)[index++] = tempVertex2;
    (*vertices)[index++] = tempVertex3;
    (*vertices)[index++] = tempVertex1;
    (*vertices)[index++] = tempVertex3;
    (*vertices)[index++] = tempVertex4;
}


HRESULT HairGrid::CreateVertexBuffer( int ByteWidth, UINT bindFlags, ID3D11Buffer** vertexBuffer,
                                 GRID_TEXTURE_DISPLAY_STRUCT* vertices,int numVertices)
{
    HRESULT hr;

    D3D11_BUFFER_DESC bd;
    bd.ByteWidth = ByteWidth;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(D3D11_SUBRESOURCE_DATA) );
    InitData.pSysMem = vertices;
    InitData.SysMemPitch = ByteWidth/numVertices;

    V_RETURN( m_pD3DDevice->CreateBuffer( &bd, &InitData,vertexBuffer  ) );

    return S_OK;
}


HRESULT HairGrid::setFluidObstacleTexture( ID3D11Texture3D* texture, DXGI_FORMAT format, int texWidth, int texHeight, int texDepth )
{
    HRESULT hr = S_OK;

	SAFE_RELEASE(m_pFluidObstacleTexture3D);
	m_pFluidObstacleTexture3D=NULL;

	//acquire the texture
    SAFE_ACQUIRE(m_pFluidObstacleTexture3D, texture);
    
	//create the render target view
    D3D11_RENDER_TARGET_VIEW_DESC DescRT;
    ZeroMemory( &DescRT, sizeof(DescRT) );
    DescRT.Format = format;
    DescRT.ViewDimension =  D3D11_RTV_DIMENSION_TEXTURE3D;
    DescRT.Texture3D.FirstWSlice = 0;
    DescRT.Texture3D.MipSlice = 0;
    DescRT.Texture3D.WSize = texDepth;
    V_RETURN( m_pD3DDevice->CreateRenderTargetView( m_pFluidObstacleTexture3D, &DescRT, &m_pRenderTargetViewFluidObstacles) );

    m_fluidGridWidth = texWidth;
    m_fluidGridHeight = texHeight;
	m_fluidGridDepth = texDepth;

	m_pEffect->GetVariableByName("fluidTextureWidth")->AsScalar()->SetInt(texWidth);
	m_pEffect->GetVariableByName("fluidTextureHeight")->AsScalar()->SetInt(texHeight);
	m_pEffect->GetVariableByName("fluidTextureDepth")->AsScalar()->SetInt(texDepth);

    return hr;
}
