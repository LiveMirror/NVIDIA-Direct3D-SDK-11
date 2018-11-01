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


#define MAX_D3D11_VERTEX_STREAMS D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT


D3D11_PRIMITIVE_TOPOLOGY GetPrimitiveType11_( SDKMESH_PRIMITIVE_TYPE PrimType )
{
    D3D11_PRIMITIVE_TOPOLOGY retType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    switch( PrimType )
    {
        case PT_TRIANGLE_LIST:
            retType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
        case PT_TRIANGLE_STRIP:
            retType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            break;
        case PT_LINE_LIST:
            retType = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
            break;
        case PT_LINE_STRIP:
            retType = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
            break;
        case PT_POINT_LIST:
            retType = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
            break;
        case PT_TRIANGLE_LIST_ADJ:
            retType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
            break;
        case PT_TRIANGLE_STRIP_ADJ:
            retType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
            break;
        case PT_LINE_LIST_ADJ:
            retType = D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
            break;
        case PT_LINE_STRIP_ADJ:
            retType = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
            break;
    };

    return retType;
}

using namespace std;

void Mesh::initializeDefaultNormalmaps(ID3D11Device* pd3dDevice, string mapName)
{

    char strPath[MAX_PATH];

    SDKMESH_MATERIAL* pMaterials = m_pMaterialArray;
    UINT numMaterials = m_pMeshHeader->NumMaterials;
    SDKMESH_CALLBACKS11* pLoaderCallbacks = NULL;

   
    for( UINT m = 0; m < numMaterials; m++ )
    {
        if( pMaterials[m].pNormalRV11==NULL || pMaterials[m].pNormalRV11==( ID3D11ShaderResourceView* )ERROR_RESOURCE_VALUE)
        {
            //pMaterials[m].NormalTexture[0] = *mapName.c_str();

            sprintf_s( strPath, MAX_PATH, "%s%s", m_strPath, mapName.c_str() );
            if( FAILED( DXUTGetGlobalResourceCache().CreateTextureFromFile( pd3dDevice, DXUTGetD3D11DeviceContext(),
                                                                            strPath,
                                                                            &pMaterials[m].pNormalRV11 ) ) )
            pMaterials[m].pNormalRV11 = ( ID3D11ShaderResourceView* )ERROR_RESOURCE_VALUE;            
        }
    }
}

void Mesh::initializeAlphaMaskTextures()
{
    m_numMaterials = m_pMeshHeader->NumMaterials;
    pAlphaMaskRV11s = new ID3D11ShaderResourceView*[m_numMaterials];
    for(unsigned int i=0;i<m_numMaterials; i++)
         pAlphaMaskRV11s[i] = NULL;
}

//try to load a normal map that has the same name prefix as the diffuse map 
void Mesh::LoadNormalmaps( ID3D11Device* pd3dDevice, string stringToRemove, string stringToAdd )
{

    char strPath[MAX_PATH];

    SDKMESH_MATERIAL* pMaterials = m_pMaterialArray;
    UINT numMaterials = m_pMeshHeader->NumMaterials;
    SDKMESH_CALLBACKS11* pLoaderCallbacks = NULL;

    {
        for( UINT m = 0; m < numMaterials; m++ )
        {
            // load textures
            if( pMaterials[m].DiffuseTexture[0] != 0 && (pMaterials[m].pNormalRV11==NULL || IsErrorResource(pMaterials[m].pNormalRV11)) )
            {
                string diffuseString = std::string(&pMaterials[m].DiffuseTexture[0],260);
                unsigned int diffStringSize = unsigned int(diffuseString.size());

                size_t found = -1;
                found=diffuseString.rfind(stringToRemove);
                if (found!=string::npos && found!=-1)
                {
                    diffuseString.replace (found,stringToRemove.size(),stringToAdd);

                    //pMaterials[m].NormalTexture[0] = *diffuseString.c_str();

                    sprintf_s( strPath, MAX_PATH, "%s%s", m_strPath, diffuseString.c_str() );
                    if( FAILED( DXUTGetGlobalResourceCache().CreateTextureFromFile( pd3dDevice, DXUTGetD3D11DeviceContext(),
                                                                                strPath,
                                                                                &pMaterials[m].pNormalRV11 ) ) )
                    pMaterials[m].pNormalRV11 = ( ID3D11ShaderResourceView* )ERROR_RESOURCE_VALUE;
                }
                
            }
        }
    }
}

//try to load an alpha map that has the same name prefix as the diffuse map 
void Mesh::LoadAlphaMasks( ID3D11Device* pd3dDevice, string stringToRemove, string stringToAdd )
{

    char strPath[MAX_PATH];

    SDKMESH_MATERIAL* pMaterials = m_pMaterialArray;
    UINT numMaterials = m_pMeshHeader->NumMaterials;
    SDKMESH_CALLBACKS11* pLoaderCallbacks = NULL;

    {
        for( UINT m = 0; m < m_numMaterials; m++ )
        {
            // load textures
            if( pMaterials[m].DiffuseTexture[0] != 0 && (pAlphaMaskRV11s[m] == NULL || IsErrorResource( pAlphaMaskRV11s[m]) ) )
            {
                string diffuseString = std::string(&pMaterials[m].DiffuseTexture[0],260);
                unsigned int diffStringSize = unsigned int(diffuseString.size());

                size_t found = -1;
                found=diffuseString.rfind(stringToRemove);
                if (found!=string::npos && found!=-1)
                {
                    diffuseString.replace (found,stringToRemove.size(),stringToAdd);

                    sprintf_s( strPath, MAX_PATH, "%s%s", m_strPath, diffuseString.c_str() );
                    if( FAILED( DXUTGetGlobalResourceCache().CreateTextureFromFile( pd3dDevice, DXUTGetD3D11DeviceContext(),
                                                                                strPath,
                                                                                &pAlphaMaskRV11s[m] ) ) )
                    pAlphaMaskRV11s[m] = ( ID3D11ShaderResourceView* )ERROR_RESOURCE_VALUE;
                }
                
            }
        }
    }
}


void Mesh::RenderBounded( ID3D11DeviceContext* pd3dDeviceContext, D3DXVECTOR3 minExtentsSize, D3DXVECTOR3 maxExtentsSize, 
                        UINT iDiffuseSlot,
                        UINT iNormalSlot,
                        UINT iSpecularSlot,
                        UINT iAlphaSlot,
                        ALPHA_RENDER_STATE alphaState)
{
    RenderFrameSubsetBounded( 0, false, pd3dDeviceContext, minExtentsSize, maxExtentsSize, iDiffuseSlot, iNormalSlot, iSpecularSlot, iAlphaSlot, alphaState );
}


void Mesh::RenderFrameSubsetBounded( UINT iFrame,
                                     bool bAdjacent,
                                     ID3D11DeviceContext* pd3dDeviceContext,
                                     D3DXVECTOR3 minExtentsSize, D3DXVECTOR3 maxExtentsSize, 
                                     UINT iDiffuseSlot,
                                     UINT iNormalSlot,
                                     UINT iSpecularSlot,
                                     UINT iAlphaSlot,
                                     ALPHA_RENDER_STATE alphaState)
{
    if( !m_pStaticMeshData || !m_pFrameArray )
        return;

    if( m_pFrameArray[iFrame].Mesh != INVALID_MESH )
    {
        RenderMeshSubsetBounded( m_pFrameArray[iFrame].Mesh,
                                 bAdjacent,
                                 pd3dDeviceContext,
                                 minExtentsSize, maxExtentsSize,
                                 iDiffuseSlot,
                                 iNormalSlot,
                                 iSpecularSlot,
                                 iAlphaSlot,
                                 alphaState);
    }

    // Render our children
    if( m_pFrameArray[iFrame].ChildFrame != INVALID_FRAME )
        RenderFrameSubsetBounded( m_pFrameArray[iFrame].ChildFrame, bAdjacent, pd3dDeviceContext, minExtentsSize, maxExtentsSize, iDiffuseSlot, 
                     iNormalSlot, iSpecularSlot, iAlphaSlot, alphaState );

    // Render our siblings
    if( m_pFrameArray[iFrame].SiblingFrame != INVALID_FRAME )
        RenderFrameSubsetBounded( m_pFrameArray[iFrame].SiblingFrame, bAdjacent, pd3dDeviceContext, minExtentsSize, maxExtentsSize,  iDiffuseSlot, 
                     iNormalSlot, iSpecularSlot, iAlphaSlot, alphaState );
}

void Mesh::RenderMeshSubsetBounded( UINT iMesh,
                                    bool bAdjacent,
                                    ID3D11DeviceContext* pd3dDeviceContext,
                                    D3DXVECTOR3 minExtentsSize, D3DXVECTOR3 maxExtentsSize, 
                                    UINT iDiffuseSlot,
                                    UINT iNormalSlot,
                                    UINT iSpecularSlot,
                                    UINT iAlphaSlot,
                                    ALPHA_RENDER_STATE alphaState)
{
    if( 0 < GetOutstandingBufferResources() )
        return;

    SDKMESH_MESH* pMesh = &m_pMeshArray[iMesh];

    UINT Strides[MAX_D3D11_VERTEX_STREAMS];
    UINT Offsets[MAX_D3D11_VERTEX_STREAMS];
    ID3D11Buffer* pVB[MAX_D3D11_VERTEX_STREAMS];

    if( pMesh->NumVertexBuffers > MAX_D3D11_VERTEX_STREAMS )
        return;

    for( UINT64 i = 0; i < pMesh->NumVertexBuffers; i++ )
    {
        pVB[i] = m_pVertexBufferArray[ pMesh->VertexBuffers[i] ].pVB11;
        Strides[i] = ( UINT )m_pVertexBufferArray[ pMesh->VertexBuffers[i] ].StrideBytes;
        Offsets[i] = 0;
    }

    SDKMESH_INDEX_BUFFER_HEADER* pIndexBufferArray;
    if( bAdjacent )
        pIndexBufferArray = m_pAdjacencyIndexBufferArray;
    else
        pIndexBufferArray = m_pIndexBufferArray;

    ID3D11Buffer* pIB = pIndexBufferArray[ pMesh->IndexBuffer ].pIB11;
    DXGI_FORMAT ibFormat = DXGI_FORMAT_R16_UINT;
    switch( pIndexBufferArray[ pMesh->IndexBuffer ].IndexType )
    {
    case IT_16BIT:
        ibFormat = DXGI_FORMAT_R16_UINT;
        break;
    case IT_32BIT:
        ibFormat = DXGI_FORMAT_R32_UINT;
        break;
    };

    pd3dDeviceContext->IASetVertexBuffers( 0, pMesh->NumVertexBuffers, pVB, Strides, Offsets );
    pd3dDeviceContext->IASetIndexBuffer( pIB, ibFormat, 0 );

    SDKMESH_SUBSET* pSubset = NULL;
    SDKMESH_MATERIAL* pMat = NULL;
    D3D11_PRIMITIVE_TOPOLOGY PrimType;

    for( UINT subset = 0; subset < pMesh->NumSubsets; subset++ )
    {
        pSubset = &m_pSubsetArray[ pMesh->pSubsets[subset] ];

        if( m_BoundingBoxExtentsSubsets[iMesh][subset].x>=minExtentsSize.x && m_BoundingBoxExtentsSubsets[iMesh][subset].x<maxExtentsSize.x &&
            m_BoundingBoxExtentsSubsets[iMesh][subset].y>=minExtentsSize.y && m_BoundingBoxExtentsSubsets[iMesh][subset].y<maxExtentsSize.y &&
            m_BoundingBoxExtentsSubsets[iMesh][subset].z>=minExtentsSize.z && m_BoundingBoxExtentsSubsets[iMesh][subset].z<maxExtentsSize.z )
        if(  alphaState==ALL_ALPHA || 
            (alphaState==NO_ALPHA && ( pSubset->MaterialID>=m_numMaterials || (IsErrorResource( pAlphaMaskRV11s[pSubset->MaterialID])) || pAlphaMaskRV11s[pSubset->MaterialID]==NULL) ) || //if we want no alpha then either the texture should not exist, or it should be invalid or it should be null
            (alphaState==WITH_ALPHA && pSubset->MaterialID<m_numMaterials && !IsErrorResource( pAlphaMaskRV11s[pSubset->MaterialID] ) && pAlphaMaskRV11s[pSubset->MaterialID]!=NULL )
          )
        {

            PrimType = GetPrimitiveType11_( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            if( bAdjacent )
            {
                switch( PrimType )
                {
                case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
                    PrimType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
                    break;
                case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
                    PrimType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
                    break;
                case D3D11_PRIMITIVE_TOPOLOGY_LINELIST:
                    PrimType = D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
                    break;
                case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:
                    PrimType = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
                    break;
                }
            }

            pd3dDeviceContext->IASetPrimitiveTopology( PrimType );

            pMat = &m_pMaterialArray[ pSubset->MaterialID ];
            if( iDiffuseSlot != INVALID_SAMPLER_SLOT && !IsErrorResource( pMat->pDiffuseRV11 ) )
                pd3dDeviceContext->PSSetShaderResources( iDiffuseSlot, 1, &pMat->pDiffuseRV11 );
            if( iNormalSlot != INVALID_SAMPLER_SLOT && !IsErrorResource( pMat->pNormalRV11 ) )
                pd3dDeviceContext->PSSetShaderResources( iNormalSlot, 1, &pMat->pNormalRV11 );
            if( iSpecularSlot != INVALID_SAMPLER_SLOT && !IsErrorResource( pMat->pSpecularRV11 ) )
                pd3dDeviceContext->PSSetShaderResources( iSpecularSlot, 1, &pMat->pSpecularRV11 );

            if(alphaState==ALL_ALPHA || alphaState==WITH_ALPHA )
            if( iAlphaSlot != INVALID_SAMPLER_SLOT && pSubset->MaterialID<m_numMaterials && !IsErrorResource( pAlphaMaskRV11s[pSubset->MaterialID] ) )
                pd3dDeviceContext->PSSetShaderResources( iAlphaSlot, 1, & pAlphaMaskRV11s[pSubset->MaterialID] );

            UINT IndexCount = ( UINT )pSubset->IndexCount;
            UINT IndexStart = ( UINT )pSubset->IndexStart;
            UINT VertexStart = ( UINT )pSubset->VertexStart;
            if( bAdjacent )
            {
                IndexCount *= 2;
                IndexStart *= 2;
            }

            pd3dDeviceContext->DrawIndexed( IndexCount, IndexStart, VertexStart );
        }
    }
}


void Mesh::RenderSubsetBounded( UINT iMesh,
                                     UINT subset,
                                     ID3D11DeviceContext* pd3dDeviceContext,
                                     D3DXVECTOR3 minExtentsSize, D3DXVECTOR3 maxExtentsSize, 
                                     bool bAdjacent,                            
                                     UINT iDiffuseSlot,
                                     UINT iNormalSlot,
                                     UINT iSpecularSlot,
                                     UINT iAlphaSlot,
                                     ALPHA_RENDER_STATE alphaState)
{
    if( 0 < GetOutstandingBufferResources() )
        return;

    if((int)subset>=m_numSubsets[iMesh]) return;

    SDKMESH_MESH* pMesh = &m_pMeshArray[iMesh];

    UINT Strides[MAX_D3D11_VERTEX_STREAMS];
    UINT Offsets[MAX_D3D11_VERTEX_STREAMS];
    ID3D11Buffer* pVB[MAX_D3D11_VERTEX_STREAMS];

    if( pMesh->NumVertexBuffers > MAX_D3D11_VERTEX_STREAMS )
        return;

    for( UINT64 i = 0; i < pMesh->NumVertexBuffers; i++ )
    {
        pVB[i] = m_pVertexBufferArray[ pMesh->VertexBuffers[i] ].pVB11;
        Strides[i] = ( UINT )m_pVertexBufferArray[ pMesh->VertexBuffers[i] ].StrideBytes;
        Offsets[i] = 0;
    }

    SDKMESH_INDEX_BUFFER_HEADER* pIndexBufferArray;
    if( bAdjacent )
        pIndexBufferArray = m_pAdjacencyIndexBufferArray;
    else
        pIndexBufferArray = m_pIndexBufferArray;

    ID3D11Buffer* pIB = pIndexBufferArray[ pMesh->IndexBuffer ].pIB11;
    DXGI_FORMAT ibFormat = DXGI_FORMAT_R16_UINT;
    switch( pIndexBufferArray[ pMesh->IndexBuffer ].IndexType )
    {
    case IT_16BIT:
        ibFormat = DXGI_FORMAT_R16_UINT;
        break;
    case IT_32BIT:
        ibFormat = DXGI_FORMAT_R32_UINT;
        break;
    };

    pd3dDeviceContext->IASetVertexBuffers( 0, pMesh->NumVertexBuffers, pVB, Strides, Offsets );
    pd3dDeviceContext->IASetIndexBuffer( pIB, ibFormat, 0 );

    SDKMESH_SUBSET* pSubset = NULL;
    SDKMESH_MATERIAL* pMat = NULL;
    D3D11_PRIMITIVE_TOPOLOGY PrimType;

    pSubset = &m_pSubsetArray[ pMesh->pSubsets[subset] ];

    
    if( m_BoundingBoxExtentsSubsets[iMesh][subset].x>=minExtentsSize.x && m_BoundingBoxExtentsSubsets[iMesh][subset].x<maxExtentsSize.x &&
        m_BoundingBoxExtentsSubsets[iMesh][subset].y>=minExtentsSize.y && m_BoundingBoxExtentsSubsets[iMesh][subset].y<maxExtentsSize.y &&
        m_BoundingBoxExtentsSubsets[iMesh][subset].z>=minExtentsSize.z && m_BoundingBoxExtentsSubsets[iMesh][subset].z<maxExtentsSize.z )
    {

        PrimType = GetPrimitiveType11_( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
        if( bAdjacent )
        {
            switch( PrimType )
            {
            case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
                PrimType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
                break;
            case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
                PrimType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
                break;
            case D3D11_PRIMITIVE_TOPOLOGY_LINELIST:
                PrimType = D3D11_PRIMITIVE_TOPOLOGY_LINELIST_ADJ;
                break;
            case D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP:
                PrimType = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ;
                break;
            }
        }

        pd3dDeviceContext->IASetPrimitiveTopology( PrimType );

        pMat = &m_pMaterialArray[ pSubset->MaterialID ];
        if( iDiffuseSlot != INVALID_SAMPLER_SLOT && !IsErrorResource( pMat->pDiffuseRV11 ) )
            pd3dDeviceContext->PSSetShaderResources( iDiffuseSlot, 1, &pMat->pDiffuseRV11 );
        if( iNormalSlot != INVALID_SAMPLER_SLOT && !IsErrorResource( pMat->pNormalRV11 ) )
            pd3dDeviceContext->PSSetShaderResources( iNormalSlot, 1, &pMat->pNormalRV11 );
        if( iSpecularSlot != INVALID_SAMPLER_SLOT && !IsErrorResource( pMat->pSpecularRV11 ) )
            pd3dDeviceContext->PSSetShaderResources( iSpecularSlot, 1, &pMat->pSpecularRV11 );

        UINT IndexCount = ( UINT )pSubset->IndexCount;
        UINT IndexStart = ( UINT )pSubset->IndexStart;
        UINT VertexStart = ( UINT )pSubset->VertexStart;
        if( bAdjacent )
        {
            IndexCount *= 2;
            IndexStart *= 2;
        }

        pd3dDeviceContext->DrawIndexed( IndexCount, IndexStart, VertexStart );
    }
}



void Mesh::ComputeSubmeshBoundingVolumes()
{
    SDKMESH_SUBSET* pSubset = NULL;
    D3D11_PRIMITIVE_TOPOLOGY PrimType;
    D3DXVECTOR3 lower; 
    D3DXVECTOR3 upper; 
    
    m_numMeshes = m_pMeshHeader->NumMeshes;
    m_numSubsets = new int[m_numMeshes];
    
    SDKMESH_MESH* currentMesh = &m_pMeshArray[0];
    int tris = 0;

    m_BoundingBoxCenterSubsets = new D3DXVECTOR3*[m_pMeshHeader->NumMeshes];
    m_BoundingBoxExtentsSubsets = new D3DXVECTOR3*[m_pMeshHeader->NumMeshes];


    for (UINT meshi=0; meshi < m_pMeshHeader->NumMeshes; ++meshi) 
    {
        currentMesh = GetMesh( meshi );
        
        INT indsize;
        if (m_pIndexBufferArray[currentMesh->IndexBuffer].IndexType == IT_16BIT ) 
        {
            indsize = 2;
        }else 
        {
            indsize = 4;        
        }

        m_BoundingBoxCenterSubsets[meshi] = new D3DXVECTOR3[currentMesh->NumSubsets];
        m_BoundingBoxExtentsSubsets[meshi] = new D3DXVECTOR3[currentMesh->NumSubsets];

        m_numSubsets[meshi] = currentMesh->NumSubsets;

        for( UINT subset = 0; subset < currentMesh->NumSubsets; subset++ )
        {
            lower.x = FLT_MAX; lower.y = FLT_MAX; lower.z = FLT_MAX;
            upper.x = -FLT_MAX; upper.y = -FLT_MAX; upper.z = -FLT_MAX;

            pSubset = GetSubset( meshi, subset );

            PrimType = GetPrimitiveType11_( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
            assert( PrimType == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );// only triangle lists are handled.

            UINT IndexCount = ( UINT )pSubset->IndexCount;
            UINT IndexStart = ( UINT )pSubset->IndexStart;

            UINT *ind = ( UINT * )m_ppIndices[currentMesh->IndexBuffer];
            FLOAT *verts =  ( FLOAT* )m_ppVertices[currentMesh->VertexBuffers[0]];
            UINT stride = (UINT)m_pVertexBufferArray[currentMesh->VertexBuffers[0]].StrideBytes;
            assert (stride % 4 == 0);
            stride /=4;
            for (UINT vertind = IndexStart; vertind < IndexStart + IndexCount; ++vertind)
            { 
                UINT current_ind=0;
                if (indsize == 2) {
                    UINT ind_div2 = vertind / 2;
                    current_ind = ind[ind_div2];
                    if (vertind %2 ==0) {
                        current_ind = current_ind << 16;
                        current_ind = current_ind >> 16;
                    }else {
                        current_ind = current_ind >> 16;
                    }
                }else {
                    current_ind = ind[vertind];
                }
                tris++;
                D3DXVECTOR3 *pt = (D3DXVECTOR3*)&(verts[stride * current_ind]);
                if (pt->x < lower.x) {
                    lower.x = pt->x;
                }
                if (pt->y < lower.y) {
                    lower.y = pt->y;
                }
                if (pt->z < lower.z) {
                    lower.z = pt->z;
                }
                if (pt->x > upper.x) {
                    upper.x = pt->x;
                }
                if (pt->y > upper.y) {
                    upper.y = pt->y;
                }
                if (pt->z > upper.z) {
                    upper.z = pt->z;
                }
            }//end for loop over vertices

            D3DXVECTOR3 half = upper - lower;
            half *=0.5f;

            m_BoundingBoxCenterSubsets[meshi][subset] = lower + half;
            m_BoundingBoxExtentsSubsets[meshi][subset] = half;

        }//end for loop over subsets
    }//end for loop over meshes
}