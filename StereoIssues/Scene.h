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
#include <DXUTgui.h>
#include <SDKmesh.h>

// Base Scene class, which contains functionality common to any of the Scenes
// we might want to support. This includes logic to draw a CDXUTSDKMesh object, 
// manage the cursor and window resolution, and provide 
class Scene
{
public:
    Scene() : 
        mCursorPosX(0),
        mCursorPosY(0),
        mWindowSizeX(0),
        mWindowSizeY(0),
        mConvergenceDepth(1.0),
        mStereoParamRV(0)
    { }

    virtual ~Scene() { }

    // Resource handling, expected to be overridden by derived classes.
    virtual HRESULT LoadResources(ID3D10Device* d3d10) { return D3D_OK; }
    virtual void UnloadResources() 
    { 
        SAFE_RELEASE(mStereoParamRV);
    }

    // Bind the stereo parameter, dealing with reference counting.
    void SetStereoParamRV(ID3D10ShaderResourceView* rv) 
    {
        rv->AddRef();
        SAFE_RELEASE(mStereoParamRV);
        mStereoParamRV = rv;
    }

    // Convergence depth is needed by mouse rendering.
    void SetConvergenceDepth(float newDepth) { mConvergenceDepth = newDepth; }

    virtual LRESULT HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) 
    { 
        // Update the mouse position.
        switch( uMsg )
        {
            case WM_MOUSEMOVE:
                mCursorPosX = LOWORD(lParam);
                mCursorPosY = HIWORD(lParam);
                break;
            default:
                break;
        }
        
        

        return 0; 
    }

    // Functions for updating a scene, rendering the scene and displaying helpful text, and 
    // drawing the cursor.
    // These are overridden by derived classes.
    virtual void Update(double fTime, float fElapsedTime) { }
    virtual HRESULT Render(ID3D10Device* d3d10) { return D3D_OK; }
    virtual void RenderText(CDXUTTextHelper* txtHelper, bool stereoActive) { }
    virtual HRESULT RenderCursor(ID3D10Device* d3d10) { return D3D_OK; }

    virtual HRESULT OnD3D10ResizedSwapChain(ID3D10Device* d3d10, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc) 
    { 
        // Need to know the new window size to properly create a viewport transform
        mWindowSizeX = DXUTGetWindowWidth();
        mWindowSizeY = DXUTGetWindowHeight();

        return D3D_OK; 
    }

    // UI functions, which allow scenes to provide controls for their functionality.
    virtual void InitUI(CDXUTDialog& dlg, int offsetX, int& offsetY, int& ctrlId) { }
    virtual void OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext) { }

    // Given a device, a mesh and a layout, render the specified technique to the current back buffer.
    HRESULT RenderMesh( ID3D10Device* d3d10, CDXUTSDKMesh& mesh, ID3D10InputLayout* layout, ID3D10EffectTechnique* renderTechnique ) const
    {
        HRESULT hr = D3D_OK;

        if (renderTechnique == 0) { return D3DERR_INVALIDCALL; }

        d3d10->IASetInputLayout(layout);

        UINT Strides[1];
        UINT Offsets[1];
        ID3D10Buffer* pVB[1];
        pVB[0] = mesh.GetVB10( 0, 0 );
        Strides[0] = ( UINT )mesh.GetVertexStride( 0, 0 );
        Offsets[0] = 0;
        d3d10->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
        d3d10->IASetIndexBuffer( mesh.GetIB10( 0 ), mesh.GetIBFormat10( 0 ), 0 );
        
        D3D10_TECHNIQUE_DESC techDesc;
        renderTechnique->GetDesc( &techDesc );
        SDKMESH_SUBSET* pSubset = NULL;
        D3D10_PRIMITIVE_TOPOLOGY PrimType;

        for( UINT p = 0; p < techDesc.Passes; ++p )
        {
            for( UINT subset = 0; subset < mesh.GetNumSubsets( 0 ); ++subset )
            {
                // Get the subset
                pSubset = mesh.GetSubset( 0, subset );

                PrimType = CDXUTSDKMesh::GetPrimitiveType10( ( SDKMESH_PRIMITIVE_TYPE )pSubset->PrimitiveType );
                d3d10->IASetPrimitiveTopology( PrimType );

                renderTechnique->GetPassByIndex( p )->Apply( 0 );
                d3d10->DrawIndexed( ( UINT )pSubset->IndexCount, 0, ( UINT )pSubset->VertexStart );
            }
        }

        return hr;
    }

    // Render a native vertex buffer using the specified technique. 
    // Useful for rendering postprocessing techniques, cursor rendering, debug, etc.
    HRESULT RenderVb(ID3D10Device* d3d10, ID3D10Buffer* vb, UINT vbStride, ID3D10Buffer* ib, DXGI_FORMAT ibFormat, UINT indexCount,
                   D3D10_PRIMITIVE_TOPOLOGY topology, ID3D10InputLayout* layout, ID3D10EffectTechnique* renderTechnique ) const
    {
        HRESULT hr = D3D_OK;

        if (renderTechnique == 0) { return D3DERR_INVALIDCALL; }

        d3d10->IASetInputLayout(layout);

        UINT Strides[1];
        UINT Offsets[1];
        ID3D10Buffer* pVB[1];
        pVB[0] = vb;
        Strides[0] = vbStride;
        Offsets[0] = 0;
        d3d10->IASetVertexBuffers( 0, 1, pVB, Strides, Offsets );
        d3d10->IASetIndexBuffer( ib, ibFormat, 0 );
        
        D3D10_TECHNIQUE_DESC techDesc;
        renderTechnique->GetDesc( &techDesc );

        for( UINT p = 0; p < techDesc.Passes; ++p )
        {
            d3d10->IASetPrimitiveTopology( topology );

            renderTechnique->GetPassByIndex(p)->Apply(0);
            d3d10->DrawIndexed(indexCount, 0, 0);
        }

        return hr;
    }

    // Attempt to intersect the specified ray with the provided mesh, and return an intersection point.
    // Currently just does a bounding box intersection, which means that the answer is only close, and not surface accurate.
    bool RayIntersectMesh(D3DXVECTOR3& outIntersection, const D3DXVECTOR3& rayStart, const D3DXVECTOR3& rayEnd, CDXUTSDKMesh& mesh)
    {
        // mesh is really const, but the functions we're calling are not const-correct.
        D3DXVECTOR3 ray = rayEnd - rayStart;

        for (UINT m = 0; m < mesh.GetNumMeshes(); ++m) {
            D3DXVECTOR3 center = mesh.GetMeshBBoxCenter(m);
            D3DXVECTOR3 radius = mesh.GetMeshBBoxExtents(m);

            D3DXPLANE p;
            D3DXVECTOR3 pPoint, pN;

            if (ray.x > 0) {
                pPoint = center - D3DXVECTOR3(radius.x, 0, 0);
                pN = D3DXVECTOR3(radius.x, 0, 0);
                D3DXPlaneFromPointNormal(&p, &pPoint, &pN);
                if (D3DXPlaneIntersectLine(&outIntersection, &p, &rayStart, &rayEnd)) {
                    if (outIntersection.y >= center.y - radius.y && outIntersection.y <= center.y + radius.y
                     && outIntersection.z >= center.z - radius.z && outIntersection.z <= center.z + radius.z) {
                        return true;
                    }
                }

            } else if (ray.x < 0) {
                pPoint = center + D3DXVECTOR3(radius.x, 0, 0);
                pN = D3DXVECTOR3(-radius.x, 0, 0);
                D3DXPlaneFromPointNormal(&p, &pPoint, &pN);

                if (D3DXPlaneIntersectLine(&outIntersection, &p, &rayStart, &rayEnd)) {
                    if (outIntersection.y >= center.y - radius.y && outIntersection.y <= center.y + radius.y
                     && outIntersection.z >= center.z - radius.z && outIntersection.z <= center.z + radius.z) {
                        return true;
                    }
                }
            }

            if (ray.y > 0) {
                pPoint = center - D3DXVECTOR3(0, radius.y, 0);
                pN = D3DXVECTOR3(0, radius.y, 0);
                D3DXPlaneFromPointNormal(&p, &pPoint, &pN);
                if (D3DXPlaneIntersectLine(&outIntersection, &p, &rayStart, &rayEnd)) {
                    if (outIntersection.x >= center.x - radius.x && outIntersection.x <= center.x + radius.x
                     && outIntersection.z >= center.z - radius.z && outIntersection.z <= center.z + radius.z) {
                        return true;
                    }
                }

            } else if (ray.y < 0) {
                pPoint = center + D3DXVECTOR3(0, radius.y, 0);
                pN = D3DXVECTOR3(0, -radius.y, 0);
                D3DXPlaneFromPointNormal(&p, &pPoint, &pN);
                if (D3DXPlaneIntersectLine(&outIntersection, &p, &rayStart, &rayEnd)) {
                    if (outIntersection.x >= center.x - radius.x && outIntersection.x <= center.x + radius.x
                     && outIntersection.z >= center.z - radius.z && outIntersection.z <= center.z + radius.z) {
                        return true;
                    }
                }
            }

            if (ray.z > 0) {
                pPoint = center - D3DXVECTOR3(0, 0, radius.z);
                pN = D3DXVECTOR3(0, 0, radius.z);
                D3DXPlaneFromPointNormal(&p, &pPoint, &pN);
                if (D3DXPlaneIntersectLine(&outIntersection, &p, &rayStart, &rayEnd)) {
                    if (outIntersection.x >= center.x - radius.x && outIntersection.x <= center.x + radius.x
                     && outIntersection.y >= center.y - radius.y && outIntersection.y <= center.y + radius.y) {
                        return true;
                    }
                }

            } else if (ray.z < 0) {
                pPoint = center + D3DXVECTOR3(0, 0, radius.z);
                pN = D3DXVECTOR3(0, 0, -radius.z);
                D3DXPlaneFromPointNormal(&p, &center, &pN);
                if (D3DXPlaneIntersectLine(&outIntersection, &p, &rayStart, &rayEnd)) {
                    if (outIntersection.x >= center.x - radius.x && outIntersection.x <= center.x + radius.x
                     && outIntersection.y >= center.y - radius.y && outIntersection.y <= center.y + radius.y) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

protected:
    // The cursor X and Y position
    UINT mCursorPosX; 
    UINT mCursorPosY; 

    // The size of the current window
    UINT mWindowSizeX;
    UINT mWindowSizeY;

    // Convergence or screen depth, the depth at which the eye rays cross.
    float mConvergenceDepth;

    // The shader resource view for the stereo param texture, which any stereo 
    // scene would need.
    ID3D10ShaderResourceView* mStereoParamRV;
};
