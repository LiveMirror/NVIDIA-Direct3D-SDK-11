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

#include "DXUT.h"
#include <strsafe.h>
#pragma warning(disable: 4995)
#include "MultiAnimation.h"
#pragma warning(default: 4995)




//-----------------------------------------------------------------------------
// Name: HeapCopy()
// Desc: Allocate buffer in memory and copy the content of sName to the
//       buffer, then return the address of the buffer.
//-----------------------------------------------------------------------------
CHAR* HeapCopy( CHAR *sName )
{
    DWORD dwLen = (DWORD) strlen( sName ) + 1;
    CHAR * sNewName = new CHAR[ dwLen ];
    if( sNewName )
        StringCchCopyA( sNewName, dwLen, sName );
    return sNewName;
}




//-----------------------------------------------------------------------------
// Name: CMultiAnimAllocateHierarchy::CMultiAnimAllocateHierarchy()
// Desc: Constructor of CMultiAnimAllocateHierarchy
//-----------------------------------------------------------------------------
CMultiAnimAllocateHierarchy::CMultiAnimAllocateHierarchy() :
    m_pMA( NULL )
{
}




//-----------------------------------------------------------------------------
// Name: CMultiAnimAllocateHierarchy::SetMA()
// Desc: Sets the member CMultiAnimation pointer.  This is the CMultiAnimation
//       we work with during the callbacks from D3DX.
//-----------------------------------------------------------------------------
HRESULT CMultiAnimAllocateHierarchy::SetMA( THIS_ CMultiAnim *pMA )
{
    m_pMA = pMA;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CMultiAnimAllocateHierarchy::CreateFrame()
// Desc: Called by D3DX during the loading of a mesh hierarchy.  The app can
//       customize its behavior.  At a minimum, the app should allocate a
//       D3DXFRAME or a child of it and fill in the Name member.
//-----------------------------------------------------------------------------
HRESULT CMultiAnimAllocateHierarchy::CreateFrame( THIS_ LPCSTR Name, LPD3DXFRAME * ppFrame )
{
    assert( m_pMA );

    * ppFrame = NULL;

    MultiAnimFrame * pFrame = new MultiAnimFrame;
    if( pFrame == NULL )
    {
        DestroyFrame( pFrame );
        return E_OUTOFMEMORY;
    }

    ZeroMemory( pFrame, sizeof( MultiAnimFrame ) );

    if( Name )
        pFrame->Name = (CHAR *) HeapCopy( (CHAR *) Name );
    else
    {
        pFrame->Name = new CHAR[1];
        pFrame->Name[0] = 0;
    }

    if( pFrame->Name == NULL )
    {
        DestroyFrame( pFrame );
        return E_OUTOFMEMORY;
    }

    * ppFrame = pFrame;
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CMultiAnimAllocateHierarchy::CreateMeshContainer()
// Desc: Called by D3DX during the loading of a mesh hierarchy. At a minumum,
//       the app should allocate a D3DXMESHCONTAINER or a child of it and fill
//       in the members based on the parameters here.  The app can further
//       customize the allocation behavior here.  In our case, we Initialize
//       m_amxBoneOffsets from the skin info for convenience reason.
//       Then we call ConvertToIndexedBlendedMesh to obtain a new mesh object
//       that's compatible with the palette size we have to work with.
//-----------------------------------------------------------------------------
HRESULT CMultiAnimAllocateHierarchy::CreateMeshContainer( THIS_
    LPCSTR Name,
    CONST D3DXMESHDATA *pMeshData,
    CONST D3DXMATERIAL *pMaterials,
    CONST D3DXEFFECTINSTANCE *pEffectInstances,
    DWORD NumMaterials,
    CONST DWORD *pAdjacency,
    LPD3DXSKININFO pSkinInfo, 
    LPD3DXMESHCONTAINER *ppNewMeshContainer )
{
    assert( m_pMA );

    * ppNewMeshContainer = NULL;

    HRESULT hr = S_OK;

    MultiAnimMC * pMC = new MultiAnimMC;
    if( pMC == NULL )
    { hr = E_OUTOFMEMORY; goto e_Exit; }

    ZeroMemory( pMC, sizeof( MultiAnimMC ) );

    D3DXMatrixIdentity(&pMC->m_pStaticCombinedMatrix);

    // only support mesh type
    if( pMeshData->Type != D3DXMESHTYPE_MESH )
    { hr = E_FAIL; goto e_Exit; }

    if( Name )
        pMC->Name = (CHAR *) HeapCopy( (CHAR *) Name );
    else
    {
        pMC->Name = new CHAR[1];
        pMC->Name[0] = 0;
    }

    // copy the mesh over
    pMC->MeshData.Type = pMeshData->Type;
    pMC->MeshData.pMesh = pMeshData->pMesh;
    pMC->MeshData.pMesh->AddRef();

    // copy adjacency over
    {
        DWORD dwNumFaces = pMC->MeshData.pMesh->GetNumFaces();
        pMC->pAdjacency = new DWORD[ 3 * dwNumFaces ];
        if( pMC->pAdjacency == NULL )
        { hr = E_OUTOFMEMORY; goto e_Exit; }

        CopyMemory( pMC->pAdjacency, pAdjacency, 3 * sizeof( DWORD ) * dwNumFaces );
    }

    // ignore effects instances
    pMC->pEffects = NULL;

    // alloc and copy materials
    pMC->NumMaterials = max( 1, NumMaterials );
    pMC->pMaterials = new D3DXMATERIAL           [ pMC->NumMaterials ];
    if( pMC->pMaterials == NULL )
    { hr = E_OUTOFMEMORY; goto e_Exit; }

    if( NumMaterials > 0 )
    {
        D3DXATTRIBUTERANGE *pAttributeTable = NULL;
        DWORD attrTableSize = 0;
        pMC->MeshData.pMesh->GetAttributeTable(pAttributeTable,&attrTableSize);
        pAttributeTable = new D3DXATTRIBUTERANGE[attrTableSize];
        pMC->MeshData.pMesh->GetAttributeTable(pAttributeTable,&attrTableSize);

        if(attrTableSize > 1)
        {
            StringCchCopyA(pMC->m_pTextureFilename,MAX_PATH,"TOO MANY ATTR");
        }
        else
        {
            assert(pAttributeTable && pAttributeTable->AttribId < NumMaterials);
            CopyMemory( pMC->pMaterials, pMaterials, NumMaterials * sizeof( D3DXMATERIAL ) );
            if(pMC->pMaterials[pAttributeTable->AttribId].pTextureFilename)
                StringCchCopyA(pMC->m_pTextureFilename,MAX_PATH,pMC->pMaterials[pAttributeTable->AttribId].pTextureFilename);
            else
                pMC->m_pTextureFilename[0] = 0;
        }
        delete [] pAttributeTable;
    }
    else    // mock up a default material and set it
    {
        ZeroMemory( & pMC->pMaterials[ 0 ].MatD3D, sizeof( D3DMATERIAL9 ) );
        pMC->pMaterials[ 0 ].MatD3D.Diffuse.r = 0.5f;
        pMC->pMaterials[ 0 ].MatD3D.Diffuse.g = 0.5f;
        pMC->pMaterials[ 0 ].MatD3D.Diffuse.b = 0.5f;
        pMC->pMaterials[ 0 ].MatD3D.Specular = pMC->pMaterials[ 0 ].MatD3D.Diffuse;
        pMC->pMaterials[ 0 ].pTextureFilename = NULL;
    }

    IDirect3DDevice9* pd3dDevice = NULL;
    hr = pMC->MeshData.pMesh->GetDevice( &pd3dDevice );
    if( FAILED(hr) )
        goto e_Exit;

    ID3DXMesh* pTempMesh = NULL;
    ID3DXMesh* pNewMesh = NULL;
    // Create a new vertex declaration to hold all the required data
    const D3DVERTEXELEMENT9 VertexDecl[] =
    {
        { 0, 0,  D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
        { 0, 12, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
        { 0, 24, D3DDECLTYPE_FLOAT2,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
        { 0, 32, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0 },
        { 0, 44, D3DDECLTYPE_FLOAT3,   D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0 },
        D3DDECL_END()
    };
    
    // Clone mesh to the new vertex format
    hr = pMC->MeshData.pMesh->CloneMesh( pMC->MeshData.pMesh->GetOptions(), VertexDecl, pd3dDevice, &pTempMesh );
    if( FAILED(hr) )
        goto e_Exit;

//    ID3DXMesh* pOptMesh = NULL;
//    ID3DXBuffer *pVertexRemap = NULL;
//    pTempMesh->Optimize(D3DXMESHOPT_ATTRSORT|D3DXMESHOPT_DONOTSPLIT,pAdjacency,pMC->pAdjacency,NULL,&pVertexRemap,&pOptMesh);    
  
    // Compute tangents, which are required for normal mapping
    hr = D3DXComputeTangentFrameEx( pTempMesh, D3DDECLUSAGE_TEXCOORD, 0, D3DDECLUSAGE_TANGENT, 0,
                                    D3DX_DEFAULT, 0, D3DDECLUSAGE_NORMAL, 0,
                                    0, NULL, -1, 0, -1, &pNewMesh, NULL );


    pTempMesh->Release();
    pMC->MeshData.pMesh->Release();
    pMC->MeshData.pMesh = pNewMesh;

    // if this is a static mesh, exit early; we're only interested in skinned meshes
    if( pSkinInfo != NULL )
    {
        // save the skininfo object
        pMC->pSkinInfo = pSkinInfo;
        pSkinInfo->AddRef();

        //
        // Determine the palette size we need to work with, then call ConvertToIndexedBlendedMesh
        // to set up a new mesh that is compatible with the palette size.
        //
        {
            pMC->m_dwNumPaletteEntries = pSkinInfo->GetNumBones();
        }

        // generate the skinned mesh - creates a mesh with blend weights and indices
        hr = pMC->pSkinInfo->ConvertToIndexedBlendedMesh( pMC->MeshData.pMesh,
            D3DXMESH_MANAGED | D3DXMESHOPT_VERTEXCACHE,
            pMC->m_dwNumPaletteEntries,
            pMC->pAdjacency,
            NULL,
            NULL,
            NULL,
            &pMC->m_dwMaxNumFaceInfls,
            &pMC->m_dwNumAttrGroups,
            &pMC->m_pBufBoneCombos,
            &pMC->m_pWorkingMesh );

        if( FAILED( hr ) )
            goto e_Exit;
    }
    else
    {
        pMC->m_dwMaxNumFaceInfls = 0;
        pMC->m_dwNumAttrGroups = 0;
        pMC->m_pBufBoneCombos = NULL;
    }

e_Exit:

    if( FAILED( hr ) )
    {
        if( pMC )
            DestroyMeshContainer( pMC );
    }
    else
        * ppNewMeshContainer = pMC;

    return hr;
}




//-----------------------------------------------------------------------------
// Name: CMultiAnimAllocateHierarchy::DestroyFrame()
// Desc: Called by D3DX during the Release of a mesh hierarchy.  Here we should
//       free all resources allocated in CreateFrame().
//-----------------------------------------------------------------------------
HRESULT CMultiAnimAllocateHierarchy::DestroyFrame( THIS_ LPD3DXFRAME pFrameToFree )
{
    assert( m_pMA );

    MultiAnimFrame * pFrame = (MultiAnimFrame *) pFrameToFree;

    if( pFrame->Name )
        delete [] pFrame->Name;

    delete pFrame;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CMultiAnimAllocateHierarchy::DestroyMeshContainer()
// Desc: Called by D3DX during the Release of a mesh hierarchy.  Here we should
//       free all resources allocated in CreateMeshContainer().
//-----------------------------------------------------------------------------
HRESULT CMultiAnimAllocateHierarchy::DestroyMeshContainer( THIS_ LPD3DXMESHCONTAINER pMeshContainerToFree )
{
    assert( m_pMA );

    MultiAnimMC * pMC = (MultiAnimMC *) pMeshContainerToFree;

    if( pMC->Name )
        delete [] pMC->Name;

    if( pMC->MeshData.pMesh )
        pMC->MeshData.pMesh->Release();

    if( pMC->pAdjacency )
        delete [] pMC->pAdjacency;

    if( pMC->pMaterials )
        delete [] pMC->pMaterials;

    if( pMC->pSkinInfo )
        pMC->pSkinInfo->Release();

    if( pMC->m_pWorkingMesh )
    {
        pMC->m_pWorkingMesh->Release();
        pMC->m_pWorkingMesh = NULL;
    }

    pMC->m_dwNumPaletteEntries = 0;
    pMC->m_dwMaxNumFaceInfls = 0;
    pMC->m_dwNumAttrGroups = 0;

    if( pMC->m_pBufBoneCombos )
    {
        pMC->m_pBufBoneCombos->Release();
        pMC->m_pBufBoneCombos = NULL;
    }

    delete pMeshContainerToFree;

    return S_OK;
}
