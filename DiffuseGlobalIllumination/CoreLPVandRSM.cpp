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

#pragma warning(disable : 4995)
#include "Lighting.h"



void invokeHierarchyBasedPropagation(ID3D11DeviceContext* pd3dContext, bool useSingleLPV, int numHierarchyLevels, int numPropagationStepsLPV, int PropLevel,
                                     LPV_Hierarchy* GV, LPV_Hierarchy* GVColor, LPV_RGB_Hierarchy* LPVAccumulate, LPV_RGB_Hierarchy* LPVPropagate)
{
    float ClearColor2[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    if(!useSingleLPV)
    {
        //downsample the geometry volume at the beginning since it does not change
        //here we are downsampling from level 0 to level 1
        for(int i=0; i<numHierarchyLevels-1;i++)
        {
            GV->Downsample(pd3dContext,i,DOWNSAMPLE_MAX);
            GVColor->Downsample(pd3dContext,i,DOWNSAMPLE_MAX);
        }

        PropSpecs propAmounts[MAX_LEVELS];

        if(numHierarchyLevels == 2)
        {
            int individualSteps = numPropagationStepsLPV/3;
            propAmounts[0] = PropSpecs(individualSteps , individualSteps+ (numPropagationStepsLPV-individualSteps*3));
            propAmounts[1] = PropSpecs(individualSteps, 0);
        }
        else if(numHierarchyLevels == 3)
        {
            propAmounts[0] = PropSpecs(numPropagationStepsLPV/5,numPropagationStepsLPV/5);
            propAmounts[1] = PropSpecs(numPropagationStepsLPV/5,numPropagationStepsLPV/5);
            propAmounts[2] = PropSpecs(numPropagationStepsLPV/5,0);        
        }
        else
        {
            for(int i=0;i<(numHierarchyLevels-1);i++)
                propAmounts[i] = PropSpecs(numPropagationStepsLPV/(numHierarchyLevels*2-1),numPropagationStepsLPV/(numHierarchyLevels*2-1));
            propAmounts[numHierarchyLevels-1] = PropSpecs(numPropagationStepsLPV/(numHierarchyLevels*2-1),0);
        }


        propagateLightHierarchy(pd3dContext, LPVAccumulate, LPVPropagate, GV, GVColor, 0, propAmounts, numHierarchyLevels);
            
    }
    else
    {
        //downsample all the buffers, since we initialized on the finest level (since it looks like i have a bug if I initialize the lower level directly)
        
        for(int level=0; level<PropLevel; level++)
        {
            LPVPropagate->clearRenderTargetView(pd3dContext,ClearColor2,true, level+1);
            LPVPropagate->clearRenderTargetView(pd3dContext,ClearColor2,false, level+1);
            LPVAccumulate->clearRenderTargetView(pd3dContext,ClearColor2,true, level+1);
            LPVAccumulate->clearRenderTargetView(pd3dContext,ClearColor2,false, level+1);

            LPVPropagate->Downsample(pd3dContext,level,DOWNSAMPLE_AVERAGE);
            LPVAccumulate->Downsample(pd3dContext,level,DOWNSAMPLE_AVERAGE);
            GV->Downsample(pd3dContext,level,DOWNSAMPLE_MAX);
            GVColor->Downsample(pd3dContext,level,DOWNSAMPLE_MAX);
        }

        if(g_usePSPropagation)
        {
            pd3dContext->PSSetConstantBuffers( 0, 1, &g_pcbLPVinitVS );
            pd3dContext->PSSetConstantBuffers( 2, 1, &g_pcbLPVpropagateGather );
            pd3dContext->PSSetConstantBuffers( 6, 1, &g_pcbLPVpropagateGather2 );        
        }
        else
        {
            pd3dContext->CSSetConstantBuffers( 0, 1, &g_pcbLPVinitVS );
            pd3dContext->CSSetConstantBuffers( 2, 1, &g_pcbLPVpropagateGather );
            pd3dContext->CSSetConstantBuffers( 6, 1, &g_pcbLPVpropagateGather2 );
        }

        for(int i=0; i< numPropagationStepsLPV; i++)
            propagateLPV(pd3dContext, i, LPVPropagate->m_collection[PropLevel], LPVAccumulate->m_collection[PropLevel], GV->m_collection[PropLevel], GVColor->m_collection[PropLevel] );
    }
}

//initialize the GV ( geometry volume) with the RSM data
void initializeGV(ID3D11DeviceContext* pd3dContext, SimpleRT* GV, SimpleRT* GVColor, SimpleRT* RSMAlbedoRT, SimpleRT* RSMNormalRT, DepthRT* shadowMapDS,
                  float fovy, float aspectRatio, float nearPlane, float farPlane, bool useDirectional, const D3DXMATRIX* projectionMatrix, D3DXMATRIX viewMatrix)
{
    HRESULT hr;

    float BlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    pd3dContext->OMSetBlendState(g_pMaxBlendBS, BlendFactor, 0xffffffff);
    pd3dContext->OMSetDepthStencilState(g_depthStencilStateDisableDepth, 0);

    assert(GV->getNumRTs()==1); // this code cannot deal with more than one texture in each RenderTarget
    ID3D11RenderTargetView* GVRTV = GV->get_pRTV(0);
    ID3D11RenderTargetView* GVColorRTV = GVColor->get_pRTV(0);
    
    D3DXMATRIX viewToLPVMatrix;
    viewToLPVMatrix = GV->getViewToLPVMatrixGV(viewMatrix);

    //set the constant buffer
    float tanFovyHalf = tan(fovy/2.0f);
    float tanFovxHalf = tan(fovy/2.0f)*aspectRatio;

    D3D11_MAPPED_SUBRESOURCE MappedResource;
    pd3dContext->Map( g_reconPos, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    RECONSTRUCT_POS* pcbTempR = ( RECONSTRUCT_POS* )MappedResource.pData;
    pcbTempR->farClip = farPlane;
    pd3dContext->Unmap( g_reconPos, 0 );
    pd3dContext->VSSetConstantBuffers( 6, 1, &g_reconPos );
    pd3dContext->PSSetConstantBuffers( 6, 1, &g_reconPos );

    D3DXMATRIX mProjInv;
    D3DXMatrixInverse(&mProjInv, NULL, projectionMatrix);
    V(pd3dContext->Map( g_pcbInvProjMatrix, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    CB_INVPROJ_MATRIX* pInvProjMat = ( CB_INVPROJ_MATRIX* )MappedResource.pData;
    D3DXMatrixTranspose( &pInvProjMat->m_InverseProjMatrix, &mProjInv );
    pd3dContext->Unmap( g_pcbInvProjMatrix, 0 );
    pd3dContext->VSSetConstantBuffers( 7, 1, &g_pcbInvProjMatrix );

    g_LPVViewport.Width  = (float)GV->getWidth3D();
    g_LPVViewport.Height = (float)GV->getHeight3D();

    //clear and bind the GVs as render targets
    //bind all the GVs as MRT output
    ID3D11RenderTargetView* pRTVGV[2] = { GVRTV, GVColorRTV };
    pd3dContext->OMSetRenderTargets( 2, pRTVGV, NULL );
    pd3dContext->RSSetViewports(1, &g_LPVViewport);

    //bind the RSMs to be read as textures
    ID3D11ShaderResourceView* ppSRVsRSMs2[3] = {RSMAlbedoRT->get_pSRV(0), RSMNormalRT->get_pSRV(0), shadowMapDS->get_pSRV(0)};
    pd3dContext->VSSetShaderResources( 0, 3, ppSRVsRSMs2);

    //set the shaders
    if(g_useBilinearInit && g_bilinearInitGVenabled)
        pd3dContext->VSSetShader( g_pVSInitializeLPV_Bilinear, NULL, 0 );    
    else    
        pd3dContext->VSSetShader( g_pVSInitializeLPV, NULL, 0 );
    if(g_useBilinearInit && g_bilinearInitGVenabled)
        pd3dContext->GSSetShader( g_pGSInitializeLPV_Bilinear, NULL, 0 );
    else
        pd3dContext->GSSetShader( g_pGSInitializeLPV, NULL, 0 );
    pd3dContext->PSSetShader( g_pPSInitializeGV, NULL, 0 );

    //set the constants
    pd3dContext->Map( g_pcbLPVinitVS, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    CB_LPV_INITIALIZE* pcbLPVinitVS = ( CB_LPV_INITIALIZE* )MappedResource.pData;
    pcbLPVinitVS->RSMWidth = RSMNormalRT->getWidth2D();
    pcbLPVinitVS->RSMHeight = RSMNormalRT->getHeight2D();
    pd3dContext->Unmap( g_pcbLPVinitVS, 0 );
    pd3dContext->VSSetConstantBuffers( 0, 1, &g_pcbLPVinitVS );


    //initialize the constant buffer which changes based on the chosen LPV level
    pd3dContext->Map( g_pcbLPVinitialize_LPVDims, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    CB_LPV_INITIALIZE3* pcbLPVinitVS3 = ( CB_LPV_INITIALIZE3* )MappedResource.pData;
    pcbLPVinitVS3->g_numCols = GV->getNumCols();    //the number of columns in the flattened 2D LPV
    pcbLPVinitVS3->g_numRows = GV->getNumRows();    //the number of columns in the flattened 2D LPV
    pcbLPVinitVS3->LPV2DWidth =  GV->getWidth2D();  //the total width of the flattened 2D LPV
    pcbLPVinitVS3->LPV2DHeight = GV->getHeight2D(); //the total height of the flattened 2D LPV
    pcbLPVinitVS3->LPV3DWidth = GV->getWidth3D();   //the width of the LPV in 3D
    pcbLPVinitVS3->LPV3DHeight = GV->getHeight3D(); //the height of the LPV in 3D
    pcbLPVinitVS3->LPV3DDepth = GV->getDepth3D();   //the depth of the LPV in 3D
    pcbLPVinitVS3->fluxWeight = 4 * tanFovxHalf * tanFovyHalf / (RSM_RES * RSM_RES);
    pcbLPVinitVS3->fluxWeight *= 30.0f; //arbitrary scale
    pcbLPVinitVS3->useFluxWeight = useDirectional? 0 : 1;
    pd3dContext->Unmap( g_pcbLPVinitialize_LPVDims, 0 );

    pd3dContext->VSSetConstantBuffers( 5, 1, &g_pcbLPVinitialize_LPVDims );
    pd3dContext->GSSetConstantBuffers( 5, 1, &g_pcbLPVinitialize_LPVDims );


    pd3dContext->Map( g_pcbLPVinitVS2, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    CB_LPV_INITIALIZE2* pcbLPVinitVS2 = ( CB_LPV_INITIALIZE2* )MappedResource.pData;
    D3DXMatrixTranspose( &pcbLPVinitVS2->g_ViewToLPV, &viewToLPVMatrix);

    D3DXVec3TransformNormal(&pcbLPVinitVS2->lightDirGridSpace, &D3DXVECTOR3(0, 0, 1), &viewToLPVMatrix);
    D3DXVec3Normalize(&pcbLPVinitVS2->lightDirGridSpace, &pcbLPVinitVS2->lightDirGridSpace);

    pcbLPVinitVS2->displacement = g_VPLDisplacement;
    pd3dContext->Unmap( g_pcbLPVinitVS2, 0 );
    pd3dContext->VSSetConstantBuffers( 1, 1, &g_pcbLPVinitVS2 );

    int numPoints = RSMAlbedoRT->getWidth2D()*RSMAlbedoRT->getHeight2D();
    if(g_useBilinearInit && g_bilinearInitGVenabled)
        numPoints *= 8;
    unsigned int stride = 0;
    unsigned int offset = 0;
    ID3D11Buffer* buffer[] = { NULL };
    pd3dContext->IASetVertexBuffers(0, 1, buffer, &stride, &offset);
    pd3dContext->IASetInputLayout(NULL);
    pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    pd3dContext->Draw(numPoints,0);
    pd3dContext->IASetInputLayout( g_pMeshLayout );

    ID3D11RenderTargetView* pRTVsNULL3[3] = { NULL, NULL, NULL };
    pd3dContext->OMSetRenderTargets( 3, pRTVsNULL3, NULL );
    ID3D11ShaderResourceView* ppSRVsNULL3[3] = { NULL, NULL, NULL };
    pd3dContext->VSSetShaderResources( 0, 3, ppSRVsNULL3);
    ID3D11GeometryShader* NULLGS = NULL;
    pd3dContext->GSSetShader( NULLGS, NULL, 0 );

    pd3dContext->OMSetBlendState(g_pNoBlendBS, BlendFactor, 0xffffffff);
    pd3dContext->OMSetDepthStencilState(g_normalDepthStencilState, 0);  
}

void initializeLPV(ID3D11DeviceContext* pd3dContext, SimpleRT_RGB* LPVAccumulate, SimpleRT_RGB* LPVPropagate, 
                   D3DXMATRIX viewMatrix, D3DXMATRIX mProjectionMatrix, SimpleRT* RSMColorRT, SimpleRT* RSMNormalRT, DepthRT* shadowMapDS,
                   float fovy, float aspectRatio, float nearPlane, float farPlane, bool useDirectional)
{
    HRESULT hr;

    float BlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    pd3dContext->OMSetBlendState(g_pMaxBlendBS, BlendFactor, 0xffffffff);
    pd3dContext->OMSetDepthStencilState(g_depthStencilStateDisableDepth, 0);

    ID3D11RenderTargetView* pRTVsNULL3[3] = { NULL, NULL, NULL };

    D3DXMATRIX inverseViewToLPVMatrix, viewToLPVMatrix;
    LPVPropagate->getLPVLightViewMatrices(viewMatrix, &viewToLPVMatrix, &inverseViewToLPVMatrix);

    //set the constant buffer
    float tanFovyHalf = tan(fovy/2.0f);
    float tanFovxHalf = tan(fovy/2.0f)*aspectRatio;

    D3D11_MAPPED_SUBRESOURCE MappedResource;
    pd3dContext->Map( g_reconPos, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    RECONSTRUCT_POS* pcbTempR = ( RECONSTRUCT_POS* )MappedResource.pData;
    pcbTempR->farClip = farPlane;
    pd3dContext->Unmap( g_reconPos, 0 );
    pd3dContext->VSSetConstantBuffers( 6, 1, &g_reconPos );
    pd3dContext->PSSetConstantBuffers( 6, 1, &g_reconPos );

    D3DXMATRIX mProjInv;
    D3DXMatrixInverse(&mProjInv, NULL, &mProjectionMatrix);
    V(pd3dContext->Map( g_pcbInvProjMatrix, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource ) );
    CB_INVPROJ_MATRIX* pInvProjMat = ( CB_INVPROJ_MATRIX* )MappedResource.pData;
    D3DXMatrixTranspose( &pInvProjMat->m_InverseProjMatrix, &mProjInv );
    pd3dContext->Unmap( g_pcbInvProjMatrix, 0 );
    pd3dContext->VSSetConstantBuffers( 7, 1, &g_pcbInvProjMatrix );

    g_LPVViewport.Width  = (float)LPVPropagate->getWidth3D();
    g_LPVViewport.Height = (float)LPVPropagate->getHeight3D();

    unsigned int stride = 0;
    unsigned int offset = 0;
    ID3D11Buffer* buffer[] = { NULL };
    ID3D11ShaderResourceView* ppSRVsRSMs[3] = { RSMColorRT->get_pSRV(0), RSMNormalRT->get_pSRV(0), shadowMapDS->get_pSRV(0)};

    CB_LPV_INITIALIZE2* pcbLPVinitVS2; 
    int numPoints = RSMColorRT->getWidth2D()*RSMColorRT->getHeight2D();
    if(g_useBilinearInit)
        numPoints *= 8;
    ID3D11GeometryShader* NULLGS = NULL;

    //initialize the LPV with the RSM from the direct light source
    ID3D11RenderTargetView* pRTVsLPV[3] = { LPVPropagate->getRed()->get_pRTV(0), LPVPropagate->getGreen()->get_pRTV(0), LPVPropagate->getBlue()->get_pRTV(0) };
    pd3dContext->OMSetRenderTargets( 3, pRTVsLPV, NULL );
    pd3dContext->RSSetViewports(1, &g_LPVViewport);

    //bind the RSMs to be read as textures
    pd3dContext->VSSetShaderResources( 0, 3, ppSRVsRSMs);

    //set the shaders
    if(g_useBilinearInit)
        pd3dContext->VSSetShader( g_pVSInitializeLPV_Bilinear, NULL, 0 );
    else
        pd3dContext->VSSetShader( g_pVSInitializeLPV, NULL, 0 );
    if(g_useBilinearInit)
        pd3dContext->GSSetShader( g_pGSInitializeLPV_Bilinear, NULL, 0 );
    else
        pd3dContext->GSSetShader( g_pGSInitializeLPV, NULL, 0 );
    pd3dContext->PSSetShader( g_pPSInitializeLPV, NULL, 0 );

    //set the constants
    pd3dContext->Map( g_pcbLPVinitVS, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    CB_LPV_INITIALIZE* pcbLPVinitVS = ( CB_LPV_INITIALIZE* )MappedResource.pData;
    pcbLPVinitVS->RSMWidth = RSMNormalRT->getWidth2D();
    pcbLPVinitVS->RSMHeight = RSMNormalRT->getHeight2D();
    pcbLPVinitVS->lightStrength = g_directLightStrength;
    pd3dContext->Unmap( g_pcbLPVinitVS, 0 );
    pd3dContext->VSSetConstantBuffers( 0, 1, &g_pcbLPVinitVS );
    pd3dContext->PSSetConstantBuffers( 0, 1, &g_pcbLPVinitVS );

    pd3dContext->Map( g_pcbLPVinitialize_LPVDims, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    CB_LPV_INITIALIZE3* pcbLPVinitVS3 = ( CB_LPV_INITIALIZE3* )MappedResource.pData;
    pcbLPVinitVS3->g_numCols = LPVPropagate->getNumCols();    //the number of columns in the flattened 2D LPV
    pcbLPVinitVS3->g_numRows = LPVPropagate->getNumRows();    //the number of columns in the flattened 2D LPV
    pcbLPVinitVS3->LPV2DWidth =  LPVPropagate->getWidth2D();  //the total width of the flattened 2D LPV
    pcbLPVinitVS3->LPV2DHeight = LPVPropagate->getHeight2D(); //the total height of the flattened 2D LPV
    pcbLPVinitVS3->LPV3DWidth = LPVPropagate->getWidth3D();   //the width of the LPV in 3D
    pcbLPVinitVS3->LPV3DHeight = LPVPropagate->getHeight3D(); //the height of the LPV in 3D
    pcbLPVinitVS3->LPV3DDepth = LPVPropagate->getDepth3D();   //the depth of the LPV in 3D
    pcbLPVinitVS3->fluxWeight = 4 * tanFovxHalf * tanFovyHalf / (RSM_RES * RSM_RES);
    pcbLPVinitVS3->fluxWeight *= 30.0f; //arbitrary scale
    pcbLPVinitVS3->useFluxWeight = useDirectional? 0 : 1;
    pd3dContext->Unmap( g_pcbLPVinitialize_LPVDims, 0 );
    pd3dContext->VSSetConstantBuffers( 5, 1, &g_pcbLPVinitialize_LPVDims );
    pd3dContext->GSSetConstantBuffers( 5, 1, &g_pcbLPVinitialize_LPVDims );

    pd3dContext->Map( g_pcbLPVinitVS2, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    pcbLPVinitVS2 = ( CB_LPV_INITIALIZE2* )MappedResource.pData;
    D3DXMatrixTranspose( &pcbLPVinitVS2->g_ViewToLPV, &viewToLPVMatrix);
    D3DXMatrixTranspose( &pcbLPVinitVS2->g_LPVtoView, &inverseViewToLPVMatrix);

    D3DXVec3TransformNormal(&pcbLPVinitVS2->lightDirGridSpace, &D3DXVECTOR3(0, 0, 1), &viewToLPVMatrix);
    D3DXVec3Normalize(&pcbLPVinitVS2->lightDirGridSpace, &pcbLPVinitVS2->lightDirGridSpace);

    pcbLPVinitVS2->displacement = g_VPLDisplacement;
    pd3dContext->Unmap( g_pcbLPVinitVS2, 0 );
    pd3dContext->VSSetConstantBuffers( 1, 1, &g_pcbLPVinitVS2 );

    //render as many points as pixels in the RSM
    //each point will VTFs to determine its position in the grid, its flux and its normal
    //(to determine the point's position in the grid we first have to unproject the point to world space, then transform it to LPV space)
    //the point will then set its output position to write to the correct part of the output 2D texture
    //in the pixel shader the point will write out the SPH coefficients of the clamped cosine lobe
    pd3dContext->IASetVertexBuffers(0, 1, buffer, &stride, &offset);
    pd3dContext->IASetInputLayout(NULL);
    pd3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    pd3dContext->Draw(numPoints,0);
    pd3dContext->IASetInputLayout( g_pMeshLayout );

    pd3dContext->OMSetRenderTargets( 3, pRTVsNULL3, NULL );
    ID3D11ShaderResourceView* ppSRVsNULL3[3] = { NULL, NULL, NULL };
    pd3dContext->VSSetShaderResources( 0, 3, ppSRVsNULL3);
    pd3dContext->GSSetShader( NULLGS, NULL, 0 );

    pd3dContext->OMSetBlendState(g_pNoBlendBS, BlendFactor, 0xffffffff);
    pd3dContext->OMSetDepthStencilState(g_normalDepthStencilState, 0);
}



void renderRSM(ID3D11DeviceContext* pd3dContext, bool depthPeel, SimpleRT* pRSMColorRT, SimpleRT* pRSMAlbedoRT, SimpleRT* pRSMNormalRT, DepthRT* pShadowMapDS,DepthRT* pShadowTex,
               const D3DXMATRIX* projectionMatrix, const D3DXMATRIX* viewMatrix, int numMeshes, RenderMesh** meshes, D3D11_VIEWPORT shadowViewport, D3DXVECTOR3 lightPos, const float lightRadius, float depthBiasFromGUI, bool bUseSM)
{

    float BlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    pd3dContext->OMSetBlendState(g_pNoBlendBS, BlendFactor, 0xffffffff);
    pd3dContext->OMSetDepthStencilState(g_normalDepthStencilState, 0);
    pd3dContext->RSSetState(g_pRasterizerStateMainRender);


    D3D11_MAPPED_SUBRESOURCE MappedResource;
    float ClearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    pd3dContext->ClearRenderTargetView( pRSMColorRT->get_pRTV(0), ClearColor );
    pd3dContext->ClearRenderTargetView( pRSMAlbedoRT->get_pRTV(0), ClearColor );    
    pd3dContext->ClearRenderTargetView( pRSMNormalRT->get_pRTV(0), ClearColor );
    ID3D11RenderTargetView* pRTVs[3] = { pRSMColorRT->get_pRTV(0), pRSMNormalRT->get_pRTV(0), pRSMAlbedoRT->get_pRTV(0) };
    
    if(depthPeel)
    {
        ID3D11ShaderResourceView* ppSRV[1] = { pShadowTex->get_pSRV(0) };
        pd3dContext->PSSetShaderResources( 6, 1, ppSRV );

        ID3D11SamplerState *states[1] = { g_pDepthPeelingTexSampler };
        pd3dContext->PSSetSamplers( 2, 1, states );
    }

    pd3dContext->ClearDepthStencilView( *pShadowMapDS, D3D11_CLEAR_DEPTH, 1.0, 0 );

    pd3dContext->OMSetRenderTargets( 3, pRTVs, *pShadowMapDS );
    pd3dContext->RSSetViewports(1, &shadowViewport);

    D3DXMATRIX inverseProjectionMatrix;
    D3DXMatrixInverse( &inverseProjectionMatrix, NULL, projectionMatrix);

    if(depthPeel)
    {
        pd3dContext->VSSetShader( g_pVSRSMDepthPeeling, NULL, 0 );
        pd3dContext->PSSetShader( g_pPSRSMDepthPeel, NULL, 0 );
    }
    else
    {
        pd3dContext->VSSetShader( g_pVSRSM, NULL, 0 );
        pd3dContext->PSSetShader( g_pPSRSM, NULL, 0 );
    }

    //bind the sampler
    ID3D11SamplerState *states[1] = { g_pLinearSampler };
    pd3dContext->PSSetSamplers( 0, 1, states );
    ID3D11SamplerState *stateAniso[1] = { g_pAnisoSampler };
    pd3dContext->PSSetSamplers( 3, 1, stateAniso );


    //render the meshes
    for(int i=0; i<numMeshes; i++)
    {
        RenderMesh* mesh = meshes[i];

        //set the light matrices
        D3DXMATRIX WVMatrix, WVMatrixIT, WVPMatrix, ViewProjClip2Tex;
        mesh->createMatrices( *projectionMatrix, *viewMatrix, &WVMatrix, &WVMatrixIT, &WVPMatrix, &ViewProjClip2Tex );
        UpdateSceneCB( pd3dContext, lightPos, lightRadius, depthBiasFromGUI, bUseSM, &(WVPMatrix), &(WVMatrixIT), &(mesh->m_WMatrix), &(ViewProjClip2Tex) );

        pd3dContext->Map( g_pcbMeshRenderOptions, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        CB_MESH_RENDER_OPTIONS* pMeshRenderOptions = ( CB_MESH_RENDER_OPTIONS* )MappedResource.pData;
        if(g_useTextureForRSMs)
            pMeshRenderOptions->useTexture = mesh->m_UseTexture;
        else 
            pMeshRenderOptions->useTexture = false;
        pd3dContext->Unmap( g_pcbMeshRenderOptions, 0 );
        pd3dContext->PSSetConstantBuffers( 6, 1, &g_pcbMeshRenderOptions );

        if(g_subsetToRender==-1)
            mesh->m_Mesh.RenderBounded( pd3dContext, D3DXVECTOR3(0,0,0), D3DXVECTOR3(100000,100000,100000), 0 );
        else
            mesh->m_Mesh.RenderSubsetBounded(0,g_subsetToRender, pd3dContext, D3DXVECTOR3(0,0,0), D3DXVECTOR3(10000,10000,10000), false, 0 );
    }


    ID3D11RenderTargetView* pRTVsNULL3[3] = { NULL, NULL, NULL };
    pd3dContext->OMSetRenderTargets( 3, pRTVsNULL3, NULL );

    ID3D11ShaderResourceView* ppSRVNULL[1] = { NULL };
    pd3dContext->PSSetShaderResources( 6, 1, ppSRVNULL );
}

void invokeCascadeBasedPropagation(ID3D11DeviceContext* pd3dContext, bool useSingleLPV, int propLevel, LPV_RGB_Cascade* LPVAccumulate, LPV_RGB_Cascade* LPVPropagate, LPV_Cascade* GV, LPV_Cascade* GVColor, int num_iterations)
{
    if(g_usePSPropagation)
    {
        pd3dContext->PSSetConstantBuffers( 2, 1, &g_pcbLPVpropagateGather );
        pd3dContext->PSSetConstantBuffers( 6, 1, &g_pcbLPVpropagateGather2 );    
    }
    else
    {
        pd3dContext->CSSetConstantBuffers( 2, 1, &g_pcbLPVpropagateGather );
        pd3dContext->CSSetConstantBuffers( 6, 1, &g_pcbLPVpropagateGather2 );
    }

    if(useSingleLPV)
    {
        for(int iteration=0; iteration<num_iterations; iteration++)
            propagateLPV(pd3dContext,iteration, LPVPropagate->m_collection[propLevel], LPVAccumulate->m_collection[propLevel], GV->m_collection[propLevel], GVColor->m_collection[propLevel]);
        return;
    }

    for(int level=0; level<LPVPropagate->getNumLevels(); level++)
        for(int iteration=0; iteration<num_iterations; iteration++)
            propagateLPV(pd3dContext,iteration, LPVPropagate->m_collection[level], LPVAccumulate->m_collection[level], GV->m_collection[level], GVColor->m_collection[level]);
}

void propagateLightHierarchy(ID3D11DeviceContext* pd3dContext, LPV_RGB_Hierarchy* LPVAccumulate, LPV_RGB_Hierarchy* LPVPropagate, LPV_Hierarchy* GV, LPV_Hierarchy* GVColor, int level, PropSpecs propAmounts[MAX_LEVELS], int numLevels)
{
    //set shader constants
    if(g_usePSPropagation)
    {
        pd3dContext->PSSetConstantBuffers( 2, 1, &g_pcbLPVpropagateGather );
        pd3dContext->PSSetConstantBuffers( 6, 1, &g_pcbLPVpropagateGather2 );
    }
    else
    {
        pd3dContext->CSSetConstantBuffers( 2, 1, &g_pcbLPVpropagateGather );
        pd3dContext->CSSetConstantBuffers( 6, 1, &g_pcbLPVpropagateGather2 );
    }

    //propagate at the current level
    for(int i=0; i< propAmounts[level].m_down ; i++)
        propagateLPV(pd3dContext, i, LPVPropagate->m_collection[level], LPVAccumulate->m_collection[level], GV->m_collection[level], GVColor->m_collection[level] );

    //recursively call propagate at the lower level
    if(level<(numLevels-1))
    {
        float ClearColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};

        //clear the coarser level
        //dont need to clear the propagation buffers, because the front buffer will be completely overwritten by the downsample function,
        //and the backbuffer will be completely overwritten by the propagate function
        LPVAccumulate->clearRenderTargetView(pd3dContext,ClearColor,true,level+1);
        LPVAccumulate->clearRenderTargetView(pd3dContext,ClearColor,false,level+1);

        //dowsample the  current level wavefront to the coarser level wavefront. the front buffer is downsampled to the front buffer
        LPVPropagate->Downsample(pd3dContext,level,DOWNSAMPLE_AVERAGE);

        //propagate at the coarser level
        propagateLightHierarchy(pd3dContext, LPVAccumulate, LPVPropagate, GV, GVColor, level+1, propAmounts, numLevels);

        //upsample the coarser level wavefront LPV to the finer level
        LPVPropagate->Upsample(pd3dContext, level+1, UPSAMPLE_BILINEAR, SAMPLE_REPLACE);
        //upsample and accumulate the coarser level accumulated LPV to the finer level
        LPVAccumulate->Upsample(pd3dContext, level+1,UPSAMPLE_BILINEAR, SAMPLE_ACCUMULATE);
    }

    //and then propagate once more at the current level
    if(g_usePSPropagation)
    {
        pd3dContext->PSSetConstantBuffers( 2, 1, &g_pcbLPVpropagateGather );
        pd3dContext->PSSetConstantBuffers( 6, 1, &g_pcbLPVpropagateGather2 );
    }
    else
    {
        pd3dContext->CSSetConstantBuffers( 2, 1, &g_pcbLPVpropagateGather );
        pd3dContext->CSSetConstantBuffers( 6, 1, &g_pcbLPVpropagateGather2 );
    }

    for(int i=0; i< propAmounts[level].m_up ; i++)
        propagateLPV(pd3dContext, i, LPVPropagate->m_collection[level], LPVAccumulate->m_collection[level], GV->m_collection[level], GVColor->m_collection[level] );

}

//shader to do propagation and accumulation of light for one iteration
void propagateLPV_PixelShaderPath(ID3D11DeviceContext* pd3dContext, int iteration, SimpleRT_RGB* LPVPropagate, SimpleRT_RGB* LPVAccumulate, SimpleRT* GV, SimpleRT* GVColor )
{
    ID3D11GeometryShader* NULLGS = NULL;
    D3D11_MAPPED_SUBRESOURCE MappedResource;

    //swap buffers so that the data that we just wrote goes into the back buffer. we will read from the backbuffer and write to the frontbuffer
    LPVPropagate->swapBuffers();
    //LPVAccumulate->swapBuffers(); dont need to swap accumulate buffers because we are just going to be additively blending to the front buffer

    //set the shaders
    pd3dContext->VSSetShader( g_pVSPropagateLPV, NULL, 0 );
    pd3dContext->GSSetShader( g_pGSPropagateLPV, NULL, 0 );
    if(g_useOcclusion || g_useMultipleBounces)
        pd3dContext->PSSetShader( g_pPSPropagateLPV, NULL, 0 );
    else
        pd3dContext->PSSetShader( g_pPSPropagateLPVSimple, NULL, 0 );


    //set the LPV backbuffers as textures to read from
    ID3D11ShaderResourceView* ppSRVsLPV[5] = { LPVPropagate->getRedBack()->get_pSRV(0), LPVPropagate->getGreenBack()->get_pSRV(0), LPVPropagate->getBlueBack()->get_pSRV(0),
                                               GV->get_pSRV(0), GVColor->get_pSRV(0)};
    pd3dContext->PSSetShaderResources( 3, 5, ppSRVsLPV);

    //update constant buffer
    pd3dContext->Map( g_pcbLPVpropagate, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    CB_LPV_PROPAGATE* pcbLPVinitVS3 = ( CB_LPV_PROPAGATE* )MappedResource.pData;
    pcbLPVinitVS3->g_numCols = LPVAccumulate->getNumCols();    //the number of columns in the flattened 2D LPV
    pcbLPVinitVS3->g_numRows = LPVAccumulate->getNumRows();    //the number of columns in the flattened 2D LPV
    pcbLPVinitVS3->LPV2DWidth =  LPVAccumulate->getWidth2D();  //the total width of the flattened 2D LPV
    pcbLPVinitVS3->LPV2DHeight = LPVAccumulate->getHeight2D(); //the total height of the flattened 2D LPV
    pcbLPVinitVS3->LPV3DWidth = LPVAccumulate->getWidth3D();   //the width of the LPV in 3D
    pcbLPVinitVS3->LPV3DHeight = LPVAccumulate->getHeight3D(); //the height of the LPV in 3D
    pcbLPVinitVS3->LPV3DDepth = LPVAccumulate->getDepth3D();   //the depth of the LPV in 3D
    pcbLPVinitVS3->b_accumulate = false;
    pd3dContext->Unmap( g_pcbLPVpropagate, 0 );
    pd3dContext->PSSetConstantBuffers( 5, 1, &g_pcbLPVpropagate ); //have to update this at each iteration

    if(iteration<2)
    {
        pd3dContext->Map( g_pcbGV, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        CB_GV* pcbGV = ( CB_GV* )MappedResource.pData;

        if(iteration<1 || !g_useOcclusion) pcbGV->useGVOcclusion = 0;
        else pcbGV->useGVOcclusion = 1;
        
        pcbGV->useMultipleBounces = g_useMultipleBounces;
        pcbGV->fluxAmplifier = g_fluxAmplifier;
        pcbGV->reflectedLightAmplifier = g_reflectedLightAmplifier;
        pcbGV->occlusionAmplifier = g_occlusionAmplifier;

        //in the first step we can optionally copy the propagation LPV data into the accumulation LPV
        if(iteration<1) pcbGV->copyPropToAccum = 1; else pcbGV->copyPropToAccum = 0;

        pd3dContext->Unmap( g_pcbGV, 0 );
        pd3dContext->PSSetConstantBuffers( 3, 1, &g_pcbGV );
    }

    g_LPVViewport.Width  = (float)LPVPropagate->getWidth3D();
    g_LPVViewport.Height = (float)LPVPropagate->getHeight3D();

    //set the MRTs that we are going to be writing to
    ID3D11RenderTargetView* pRTVLPV[6] = { LPVPropagate->getRedFront()->get_pRTV(0), LPVPropagate->getGreenFront()->get_pRTV(0) , LPVPropagate->getBlueFront()->get_pRTV(0)
                                         ,LPVAccumulate->getRedFront()->get_pRTV(0), LPVAccumulate->getGreenFront()->get_pRTV(0) , LPVAccumulate->getBlueFront()->get_pRTV(0)};
    pd3dContext->OMSetRenderTargets( 6, pRTVLPV, NULL );
    pd3dContext->RSSetViewports(1, &g_LPVViewport);

    //do the actual rendering
    float BlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    pd3dContext->OMSetBlendState(g_pNoBlend1AddBlend2BS, BlendFactor, 0xffffffff);
    UINT stride = sizeof( Tex3PosVertex );
    UINT offset = 0;
    pd3dContext->IASetInputLayout( g_pPos3Tex3IL );
    pd3dContext->IASetVertexBuffers( 0, 1, &(LPVAccumulate->m_pRenderToLPV_VB), &stride, &offset );
    pd3dContext->IASetIndexBuffer(NULL,DXGI_FORMAT_R32_UINT,0); 
    pd3dContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    pd3dContext->Draw( 6*LPVAccumulate->getDepth3D(), 0 ); 

    pd3dContext->GSSetShader( NULLGS, NULL, 0 );
    ID3D11RenderTargetView* pRTVsNULL6[6] = { NULL, NULL, NULL, NULL, NULL, NULL };
    pd3dContext->OMSetRenderTargets( 6, pRTVsNULL6, NULL );
    ID3D11ShaderResourceView* ppSRVsNULL8[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    pd3dContext->PSSetShaderResources( 3, 8, ppSRVsNULL8);

}

void propagateLPV(ID3D11DeviceContext* pd3dContext, int iteration, SimpleRT_RGB* LPVPropagate, SimpleRT_RGB* LPVAccumulate, SimpleRT* GV, SimpleRT* GVColor )
{
    if(g_usePSPropagation)
        return propagateLPV_PixelShaderPath(pd3dContext, iteration, LPVPropagate, LPVAccumulate, GV, GVColor ); 

    bool bAccumulateSeparately = false;
    if(LPVAccumulate->getRedFront()->getNumChannels()==1 && LPVAccumulate->getRedFront()->getNumRTs()==4) bAccumulateSeparately = true;

    D3D11_MAPPED_SUBRESOURCE MappedResource;

    //swap buffers so that the data that we just wrote goes into the back buffer. we will read from the backbuffer and write to the frontbuffer
    LPVPropagate->swapBuffers();

    //clear the front buffer because we are going to be replacing its contents completely with the propagated wavefront from backbuffer
    //we are not doing a clear here since it is faster for my app to just update all the voxels in the shader
    //float ClearColorLPV[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    //LPVPropagate->clearRenderTargetView(pd3dContext, ClearColorLPV, true);

    //make sure that the propagate LPV is actually a float4, since the other paths are not implemented (and were not needed since we dont need to read and write to LPVPropagate)
    assert(LPVPropagate->getRedFront()->getNumRTs()==1);
    assert(GV->getNumRTs()==1);
    assert(GVColor->getNumRTs()==1);


    UINT initCounts = 0;

    //set the shader
    if(g_useOcclusion || g_useMultipleBounces)
        pd3dContext->CSSetShader( g_pCSPropagateLPV, NULL, 0 );
    else 
        pd3dContext->CSSetShader( g_pCSPropagateLPVSimple, NULL, 0 );        

    //set the unordered views that we are going to be writing to
    ID3D11UnorderedAccessView* ppUAV[3] = { LPVPropagate->getRedFront()->get_pUAV(0), LPVPropagate->getGreenFront()->get_pUAV(0) , LPVPropagate->getBlueFront()->get_pUAV(0)};
    pd3dContext->CSSetUnorderedAccessViews( 0, 3, ppUAV, &initCounts );

    //set the LPV backbuffers as textures to read from
    ID3D11ShaderResourceView* ppSRVsLPV[5] = { LPVPropagate->getRedBack()->get_pSRV(0), LPVPropagate->getGreenBack()->get_pSRV(0), LPVPropagate->getBlueBack()->get_pSRV(0),
                                               GV->get_pSRV(0), GVColor->get_pSRV(0)};
    pd3dContext->CSSetShaderResources( 3, 5, ppSRVsLPV);

    if(!bAccumulateSeparately)
    {
        LPVAccumulate->swapBuffers();

        //set the textures and UAVs for the accumulation buffer
        ID3D11UnorderedAccessView* ppUAVAccum[3] = { LPVAccumulate->getRedFront()->get_pUAV(0), LPVAccumulate->getGreenFront()->get_pUAV(0) , LPVAccumulate->getBlueFront()->get_pUAV(0)};
        pd3dContext->CSSetUnorderedAccessViews( 3, 3, ppUAVAccum, &initCounts );

        //set the LPV backbuffers as textures to read from
        ID3D11ShaderResourceView* ppSRVsLPVAccum[3] = { LPVAccumulate->getRedBack()->get_pSRV(0), LPVAccumulate->getGreenBack()->get_pSRV(0), LPVAccumulate->getBlueBack()->get_pSRV(0)};
        pd3dContext->CSSetShaderResources( 8, 3, ppSRVsLPVAccum);    
    }

    //update constant buffer
    pd3dContext->Map( g_pcbLPVpropagate, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
    CB_LPV_PROPAGATE* pcbLPVinitVS3 = ( CB_LPV_PROPAGATE* )MappedResource.pData;
    pcbLPVinitVS3->g_numCols = LPVAccumulate->getNumCols();    //the number of columns in the flattened 2D LPV
    pcbLPVinitVS3->g_numRows = LPVAccumulate->getNumRows();    //the number of columns in the flattened 2D LPV
    pcbLPVinitVS3->LPV2DWidth =  LPVAccumulate->getWidth2D();  //the total width of the flattened 2D LPV
    pcbLPVinitVS3->LPV2DHeight = LPVAccumulate->getHeight2D(); //the total height of the flattened 2D LPV
    pcbLPVinitVS3->LPV3DWidth = LPVAccumulate->getWidth3D();   //the width of the LPV in 3D
    pcbLPVinitVS3->LPV3DHeight = LPVAccumulate->getHeight3D(); //the height of the LPV in 3D
    pcbLPVinitVS3->LPV3DDepth = LPVAccumulate->getDepth3D();   //the depth of the LPV in 3D
    pcbLPVinitVS3->b_accumulate = !bAccumulateSeparately;
    pd3dContext->Unmap( g_pcbLPVpropagate, 0 );
    pd3dContext->CSSetConstantBuffers( 5, 1, &g_pcbLPVpropagate ); //have to update this at each iteration

    if(iteration<2)
    {
        pd3dContext->Map( g_pcbGV, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource );
        CB_GV* pcbGV = ( CB_GV* )MappedResource.pData;

        if(iteration<1 || !g_useOcclusion) pcbGV->useGVOcclusion = 0;
        else pcbGV->useGVOcclusion = 1;
        
        pcbGV->useMultipleBounces = g_useMultipleBounces;
        pcbGV->fluxAmplifier = g_fluxAmplifier;
        pcbGV->reflectedLightAmplifier = g_reflectedLightAmplifier;
        pcbGV->occlusionAmplifier = g_occlusionAmplifier;

        //in the first step we can optionally copy the propagation LPV data into the accumulation LPV
        if(iteration<1) pcbGV->copyPropToAccum = 1; else pcbGV->copyPropToAccum = 0;

        pd3dContext->Unmap( g_pcbGV, 0 );
        pd3dContext->CSSetConstantBuffers( 3, 1, &g_pcbGV );
    }

    pd3dContext->Dispatch( LPVPropagate->getWidth3D()/X_BLOCK_SIZE,  LPVPropagate->getHeight3D()/Y_BLOCK_SIZE,  LPVPropagate->getDepth3D()/Z_BLOCK_SIZE );

    ID3D11UnorderedAccessView* ppUAVssNULL3[3] = { NULL, NULL, NULL};
    pd3dContext->CSSetUnorderedAccessViews( 0, 3, ppUAVssNULL3, &initCounts );
    ID3D11ShaderResourceView* ppSRVsNULL5[5] = { NULL, NULL, NULL, NULL, NULL };
    pd3dContext->CSSetShaderResources( 3, 5, ppSRVsNULL5);


    if(!bAccumulateSeparately)
    {
        //unset the textures and UAVs for the accumulation buffer
        ID3D11UnorderedAccessView* ppUAVAccumNULL[3] = {NULL, NULL, NULL};
        pd3dContext->CSSetUnorderedAccessViews( 3, 3, ppUAVAccumNULL, &initCounts );

        //set the LPV backbuffers as textures to read from
        ID3D11ShaderResourceView* ppSRVsLPVAccumNULL[3] = { NULL, NULL, NULL};
        pd3dContext->CSSetShaderResources( 8, 3, ppSRVsLPVAccumNULL);    
    }
    else
    {
        //after propagation we want to accumulate the propagated results into a separate accumulation buffer
        //note: this code is very specific to either having 3 float4 RGB textures, or 12 float textures
        //if you need more generality you will need to add it to the calculation of batches and accumulateLPV
        int numBatches = 1;
        if(LPVAccumulate->getRedFront()->getNumChannels()==1 && LPVAccumulate->getRedFront()->getNumRTs()==4) numBatches = 2;

        for(int batch=0; batch<numBatches; batch++)
            accumulateLPVs(pd3dContext, batch, LPVPropagate, LPVAccumulate );
    }
}

void accumulateLPVs(ID3D11DeviceContext* pd3dContext,int batch, SimpleRT_RGB* LPVPropagate, SimpleRT_RGB* LPVAccumulate )
{
    UINT initCounts = 0;

    if(LPVAccumulate->getRedFront()->getNumChannels()>1 && LPVAccumulate->getRedFront()->getNumRTs()==1)
    {
        pd3dContext->CSSetShader( g_pCSAccumulateLPV, NULL, 0 );

        LPVAccumulate->swapBuffers();

        ID3D11UnorderedAccessView* ppUAV[3] = { LPVAccumulate->getRedFront()->get_pUAV(0), LPVAccumulate->getGreenFront()->get_pUAV(0) , LPVAccumulate->getBlueFront()->get_pUAV(0)};
        pd3dContext->CSSetUnorderedAccessViews( 0, 3, ppUAV, &initCounts );

        //set the LPVs as textures to read from
        ID3D11ShaderResourceView* ppSRVsLPV[6] = { LPVPropagate->getRedFront()->get_pSRV(0), LPVPropagate->getGreenFront()->get_pSRV(0), LPVPropagate->getBlueFront()->get_pSRV(0),
                                                   LPVAccumulate->getRedBack()->get_pSRV(0), LPVAccumulate->getGreenBack()->get_pSRV(0) , LPVAccumulate->getBlueBack()->get_pSRV(0)};
        pd3dContext->CSSetShaderResources( 0, 6, ppSRVsLPV);
    }
    else if(LPVAccumulate->getRedFront()->getNumChannels()==1 && LPVAccumulate->getRedFront()->getNumRTs()==4)
    {
        //if we have single channel textures then we can read and write to them at the same time, but we can only bind 8 at a time
        if(batch == 0)
        {
            pd3dContext->CSSetShader( g_pCSAccumulateLPV_singleFloats_8, NULL, 0 );
            ID3D11UnorderedAccessView* ppUAV[8] ={ LPVAccumulate->getRed()->get_pUAV(0), LPVAccumulate->getRed()->get_pUAV(1), LPVAccumulate->getRed()->get_pUAV(2), LPVAccumulate->getRed()->get_pUAV(3), 
                                                   LPVAccumulate->getGreen()->get_pUAV(0), LPVAccumulate->getGreen()->get_pUAV(1), LPVAccumulate->getGreen()->get_pUAV(2), LPVAccumulate->getGreen()->get_pUAV(3)};
            pd3dContext->CSSetUnorderedAccessViews( 0, 8, ppUAV, &initCounts );

            //set the LPV backbuffers as textures to read from
            ID3D11ShaderResourceView* ppSRVsLPV[2] = { LPVPropagate->getRedFront()->get_pSRV(0), LPVPropagate->getGreenFront()->get_pSRV(0)};
            pd3dContext->CSSetShaderResources( 0, 2, ppSRVsLPV);
        }
        else if(batch == 1)
        {
            pd3dContext->CSSetShader( g_pCSAccumulateLPV_singleFloats_4, NULL, 0 );
            ID3D11UnorderedAccessView* ppUAV[4] ={ LPVAccumulate->getBlue()->get_pUAV(0), LPVAccumulate->getBlue()->get_pUAV(1), LPVAccumulate->getBlue()->get_pUAV(2), LPVAccumulate->getBlue()->get_pUAV(3)}; 
            pd3dContext->CSSetUnorderedAccessViews( 0, 4, ppUAV, &initCounts );

            //set the LPV backbuffers as textures to read from
            ID3D11ShaderResourceView* ppSRVsLPV[1] = {LPVPropagate->getBlueFront()->get_pSRV(0)};
            pd3dContext->CSSetShaderResources( 0, 1, ppSRVsLPV);

        }

    }
    else
        assert(0); //this path is not implemented and either we are here by mistake, or we have to implement this path because it is really needed

    pd3dContext->CSSetConstantBuffers( 0, 1, &g_pcbLPVpropagate ); //have to update this at each iteration
    pd3dContext->Dispatch( LPVPropagate->getWidth3D()/X_BLOCK_SIZE,  LPVPropagate->getHeight3D()/Y_BLOCK_SIZE,  LPVPropagate->getDepth3D()/Z_BLOCK_SIZE );

    ID3D11UnorderedAccessView* ppUAVsNULL8[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    pd3dContext->CSSetUnorderedAccessViews( 0, 8, ppUAVsNULL8, &initCounts );
    ID3D11ShaderResourceView* ppSRVsNULL6[6] = { NULL, NULL, NULL, NULL, NULL, NULL};
    pd3dContext->CSSetShaderResources( 0, 6, ppSRVsNULL6);
}