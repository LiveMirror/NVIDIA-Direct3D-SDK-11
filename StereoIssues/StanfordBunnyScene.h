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

#include "Scene.h"
#include <DXUTcamera.h>

const int ShadowBufferDim = 1024;
const int CursorSize = 32;

// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------
// A canonical Stanford Bunny scene. A light is projected onto the bunny to give it a little color,
// and a very trivial soft shadow implementation is used to demonstrate the common stereo 
// unprojection issue (and how to fix it).
class StanfordBunnyScene : public Scene
{
public:
    StanfordBunnyScene() : 
        mVertexLayout(0),
        mDebugLayout(0),
        mSceneFX(0),
        mMatWorldVar(0),
        mMatWorldViewProjectionVar(0),
        mMatViewProjectionInvVar(0),
        mVecLightDirVar(0),
        mVecAmbientColorVar(0),
        mHudTexVar(0),
        mSpotlightTexVar(0),
        mShadowTexVar(0),
        mStereoParamTexVar(0),
        mVecViewportSizeVar(0),
        mWorldPosTexVar(0),
        mWorldNormalTexVar(0),
        mMatSpotlightWorldViewProjectionVar(0),
        mMatHudViewProjectionVar(0),
        mPosTechniqueVar(0),
        mTechForwardRenderScene(0),
        mTechShadowBufferRender(0),
        mTechCursorRender(0),
        mTechDebugRender(0),
        mBogusShadowRTV(0),
        mShadowsSRV(0),
        mShadowsDSV(0),
        mCursorVb(0),
        mCursorIb(0),
        mAmbientColor(0.05f, 0.1f, 0.6f, 1.0f),
        mCursorRV(0),
        mSpotlightRV(0),
        mUpdateCount(0),
        mPosTechnique(0),
        mMouseTechnique(0),
        mNearZ(2),
        mFarZ(20),
        mDrawDebug(false)

    { 
    }

    // Resource handling
    virtual HRESULT LoadResources(ID3D10Device* d3d10) 
    {
        HRESULT hr = Scene::LoadResources(d3d10);

        WCHAR mediaPath[_MAX_PATH];
        DXUTFindDXSDKMediaFileCch(mediaPath, _MAX_PATH, L"bunny.sdkmesh");

        V_RETURN(mStanfordBunnyModel.Create(d3d10, mediaPath, false));

        // Read the D3DX effect file
        ID3D10Blob* bErrors;
        DXUTFindDXSDKMediaFileCch(mediaPath, _MAX_PATH, L"StanfordBunnyScene10.fx");
        hr = D3DX10CreateEffectFromFile(mediaPath, NULL, NULL, "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, d3d10, NULL,
                                        NULL, &mSceneFX, &bErrors, NULL);
        if (FAILED(hr)) {
            OutputDebugStringA((LPCSTR)bErrors->GetBufferPointer());
            return hr;
        }

        // Obtain technique objects
        mTechForwardRenderScene = mSceneFX->GetTechniqueByName("ForwardRenderScene");
        mTechShadowBufferRender = mSceneFX->GetTechniqueByName("ShadowBufferRender");
        mTechCursorRender = mSceneFX->GetTechniqueByName("CursorRender");
        mTechDebugRender = mSceneFX->GetTechniqueByName("DebugRender");

        // Create our vertex input layout
        const D3D10_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        };

        D3D10_PASS_DESC passDesc;
        V_RETURN(mTechForwardRenderScene->GetPassByIndex(0)->GetDesc(&passDesc));
        V_RETURN(d3d10->CreateInputLayout(layout, 3, passDesc.pIAInputSignature,
                                          passDesc.IAInputSignatureSize, &mVertexLayout));

        DXUTFindDXSDKMediaFileCch(mediaPath, _MAX_PATH, L"cursor.png");
        V_RETURN(D3DX10CreateShaderResourceViewFromFile(d3d10, mediaPath, NULL, NULL, &mCursorRV, NULL));

        DXUTFindDXSDKMediaFileCch(mediaPath, _MAX_PATH, L"bubblebunny.png");
        V_RETURN(D3DX10CreateShaderResourceViewFromFile(d3d10, mediaPath, NULL, NULL, &mSpotlightRV, NULL));

        // Create our effect bindings.
        mMatWorldVar = mSceneFX->GetVariableByName("gMatWorld")->AsMatrix();
        mMatWorldViewProjectionVar = mSceneFX->GetVariableByName("gMatWorldViewProjection")->AsMatrix();
        mMatViewProjectionInvVar = mSceneFX->GetVariableByName("gMatViewProjectionInv")->AsMatrix();
        mVecLightDirVar = mSceneFX->GetVariableByName("gVecLightDir")->AsVector();
        mVecAmbientColorVar = mSceneFX->GetVariableByName("gAmbientLightColor")->AsVector();
        mHudTexVar = mSceneFX->GetVariableByName("gHudTex")->AsShaderResource();
        mSpotlightTexVar = mSceneFX->GetVariableByName("gSpotlightTex")->AsShaderResource();
        mShadowTexVar = mSceneFX->GetVariableByName("gShadowTex")->AsShaderResource();
        mStereoParamTexVar = mSceneFX->GetVariableByName("gStereoParamTex")->AsShaderResource();
        mVecViewportSizeVar = mSceneFX->GetVariableByName("gViewportSize")->AsVector();
        mWorldPosTexVar = mSceneFX->GetVariableByName("gWorldPosTex")->AsShaderResource();
        mWorldNormalTexVar = mSceneFX->GetVariableByName("gWorldNormalTex")->AsShaderResource();
        mMatSpotlightWorldViewProjectionVar = mSceneFX->GetVariableByName("gMatSpotlightWorldViewProjection")->AsMatrix();
        mMatHudViewProjectionVar = mSceneFX->GetVariableByName("gMatHudViewProjection")->AsMatrix();
        mPosTechniqueVar = mSceneFX->GetVariableByName("gPosTechnique")->AsScalar();

        // Create the shadow buffer and shader resource view.
        D3D10_TEXTURE2D_DESC shadowBufferDesc;
        shadowBufferDesc.Width = shadowBufferDesc.Height = ShadowBufferDim;
        shadowBufferDesc.MipLevels = 1;
        shadowBufferDesc.ArraySize = 1;
        shadowBufferDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        shadowBufferDesc.SampleDesc.Count = 1; shadowBufferDesc.SampleDesc.Quality = 0;
        shadowBufferDesc.Usage = D3D10_USAGE_DEFAULT;
        shadowBufferDesc.BindFlags = D3D10_BIND_SHADER_RESOURCE | D3D10_BIND_DEPTH_STENCIL;
        shadowBufferDesc.CPUAccessFlags = 0;
        shadowBufferDesc.MiscFlags = 0;
        
        ID3D10Texture2D* shadowTexture = 0;
        V_RETURN(d3d10->CreateTexture2D(&shadowBufferDesc, NULL, &shadowTexture));

        D3D10_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc;
        depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthStencilViewDesc.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
        depthStencilViewDesc.Texture2D.MipSlice = 0;
        V_RETURN(d3d10->CreateDepthStencilView(shadowTexture, &depthStencilViewDesc, &mShadowsDSV));

        D3D10_SHADER_RESOURCE_VIEW_DESC shaderResourceDesc;
        shaderResourceDesc.Format = DXGI_FORMAT_R32_FLOAT;
        shaderResourceDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
        shaderResourceDesc.Texture2D.MostDetailedMip = 0;
        shaderResourceDesc.Texture2D.MipLevels = 1;
        V_RETURN(d3d10->CreateShaderResourceView(shadowTexture, &shaderResourceDesc, &mShadowsSRV));
        SAFE_RELEASE(shadowTexture);

        V_RETURN(CreateCursorQuad(d3d10));

        V_RETURN(CreateDebugData(d3d10));
        
        // Setup the default camera
        D3DXVECTOR3 vecEye( -5.0f, 1.5f, 1.5 );
        D3DXVECTOR3 vecAt ( 0.0f, 0.0f, 0.0f );

        mSceneCamera.SetViewParams( &vecEye, &vecAt );
        mSceneCamera.SetRadius(5, 4, 15);

        return hr;
    }

    HRESULT CreateCursorQuad(ID3D10Device* d3d10)
    {
        HRESULT hr = D3D_OK;

        struct BufferData
        {
            float x, y, z;
            float nx, ny, nz;
            float tu, tv;
        };

        BufferData vertices[] = 
        {
            {  0,  0, 0, 0, 0, 0, 0, 0 },
            {  1,  0, 0, 0, 0, 0, 1, 0 },
            {  0, -1, 0, 0, 0, 0, 0, 1 },
            {  1, -1, 0, 0, 0, 0, 1, 1 },
        };

        D3D10_BUFFER_DESC bufferDesc;
        bufferDesc.ByteWidth = sizeof(vertices);
        bufferDesc.Usage = D3D10_USAGE_DEFAULT;
        bufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = 0;
        bufferDesc.MiscFlags = 0;

        D3D10_SUBRESOURCE_DATA initData = { 0 };
        initData.pSysMem = vertices;
        V_RETURN(d3d10->CreateBuffer(&bufferDesc, &initData, &mCursorVb));

        UINT indices[] = { 0, 1, 2, 3 };

        bufferDesc.ByteWidth = sizeof(indices);
        bufferDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;
        initData.pSysMem = indices;
        V_RETURN(d3d10->CreateBuffer(&bufferDesc, &initData, &mCursorIb));

        return hr;
    }

    HRESULT CreateDebugData(ID3D10Device* d3d10)
    {
        HRESULT hr = D3D_OK;

        UINT numMeshes = mStanfordBunnyModel.GetNumMeshes();
        if (numMeshes == 0) { 
            return D3DERR_UNSUPPORTEDFACTORVALUE;
        }

        const UINT kVertsPerMesh = 8;
        const UINT kIndicesPerMesh = 16;

        const UINT numVerts = kVertsPerMesh * numMeshes;
        const UINT numIndices = kIndicesPerMesh * numMeshes;

        D3DXVECTOR3* verts = new D3DXVECTOR3[numVerts];
        UINT* indices = new UINT[numIndices];
        
        for (UINT m = 0; m < mStanfordBunnyModel.GetNumMeshes(); ++m) {
            D3DXVECTOR3 center = mStanfordBunnyModel.GetMeshBBoxCenter(m);
            D3DXVECTOR3 radius = mStanfordBunnyModel.GetMeshBBoxExtents(m);

            UINT vertOffset = m * kVertsPerMesh;
            UINT indexOffset = m * kIndicesPerMesh;

            verts[vertOffset + 0] = D3DXVECTOR3(center.x - radius.x, center.y - radius.y, center.z - radius.z);
            verts[vertOffset + 1] = D3DXVECTOR3(center.x - radius.x, center.y - radius.y, center.z + radius.z);
            verts[vertOffset + 2] = D3DXVECTOR3(center.x + radius.x, center.y - radius.y, center.z + radius.z);
            verts[vertOffset + 3] = D3DXVECTOR3(center.x + radius.x, center.y - radius.y, center.z - radius.z);
            verts[vertOffset + 4] = D3DXVECTOR3(center.x - radius.x, center.y + radius.y, center.z - radius.z);
            verts[vertOffset + 5] = D3DXVECTOR3(center.x - radius.x, center.y + radius.y, center.z + radius.z);
            verts[vertOffset + 6] = D3DXVECTOR3(center.x + radius.x, center.y + radius.y, center.z + radius.z);
            verts[vertOffset + 7] = D3DXVECTOR3(center.x + radius.x, center.y + radius.y, center.z - radius.z);

            indices[indexOffset + 0] = vertOffset + 0;
            indices[indexOffset + 1] = vertOffset + 1;
            indices[indexOffset + 2] = vertOffset + 2;
            indices[indexOffset + 3] = vertOffset + 3;
            indices[indexOffset + 4] = vertOffset + 0;
            indices[indexOffset + 5] = vertOffset + 4;
            indices[indexOffset + 6] = vertOffset + 5;
            indices[indexOffset + 7] = vertOffset + 1;
            indices[indexOffset + 8] = vertOffset + 5;
            indices[indexOffset + 9] = vertOffset + 6;
            indices[indexOffset + 10] = vertOffset + 2;
            indices[indexOffset + 11] = vertOffset + 6;
            indices[indexOffset + 12] = vertOffset + 7;
            indices[indexOffset + 13] = vertOffset + 3;
            indices[indexOffset + 14] = vertOffset + 7;
            indices[indexOffset + 15] = vertOffset + 4;
        }

        D3D10_BUFFER_DESC bufferDesc;
        bufferDesc.ByteWidth = sizeof(D3DXVECTOR3) * numVerts;
        bufferDesc.Usage = D3D10_USAGE_DEFAULT;
        bufferDesc.BindFlags = D3D10_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = 0;
        bufferDesc.MiscFlags = 0;

        D3D10_SUBRESOURCE_DATA initData = { 0 };
        initData.pSysMem = verts;
        V(d3d10->CreateBuffer(&bufferDesc, &initData, &mDebugVb));

        bufferDesc.ByteWidth = sizeof(long) * numIndices;
        bufferDesc.BindFlags = D3D10_BIND_INDEX_BUFFER;

        initData.pSysMem = indices;
        V(d3d10->CreateBuffer(&bufferDesc, &initData, &mDebugIb));

        delete [] verts;
        delete [] indices;

        // Create the layout.
        const D3D10_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
        };

        D3D10_PASS_DESC passDesc;
        V_RETURN(mTechDebugRender->GetPassByIndex(0)->GetDesc(&passDesc));

        V_RETURN(d3d10->CreateInputLayout(layout, 1, passDesc.pIAInputSignature,
                                          passDesc.IAInputSignatureSize, &mDebugLayout));

        return hr;
    }

    virtual void UnloadResources() 
    {
        Scene::UnloadResources();

        mStanfordBunnyModel.Destroy();

        SAFE_RELEASE(mDebugVb);
        SAFE_RELEASE(mDebugIb);

        SAFE_RELEASE(mCursorVb);
        SAFE_RELEASE(mCursorIb);

        SAFE_RELEASE(mBogusShadowRTV);
        SAFE_RELEASE(mShadowsSRV);
        SAFE_RELEASE(mShadowsDSV);

        SAFE_RELEASE(mDebugLayout);
        SAFE_RELEASE(mVertexLayout);
        SAFE_RELEASE(mSceneFX);
        SAFE_RELEASE(mSpotlightRV);
        SAFE_RELEASE(mCursorRV);

    }

    virtual LRESULT HandleMessages( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
    {
        LRESULT res = Scene::HandleMessages(hWnd, uMsg, wParam, lParam);

        // The DXUT camera needs a chance to process messages.
        mSceneCamera.HandleMessages(hWnd, uMsg, wParam, lParam);
        return res;
    }

    float RayCast(float viewspaceX, float viewspaceY)
    {
        // The safe value depends on your application. If you have a lot of empty space like this 
        // sample, covergence depth is a reasonable default. If your game is an open world shooter, 
        // then a value that is 'far away' is probably a good choice. Values less than or equal to 
        // 0 should be ignored, and the safe value used instead.

        // Note that the quality of this method is directly proportional to the relative error 
        // between bounding boxes and the objects contained within. For example, in the Stanford 
        // Bunny, the cursor will sometimes appear doubled when over something very far from the 
        // closest plane of the bounding box.
        float retVal = mConvergenceDepth;

        D3DXMATRIX invWV, view = (*mSceneCamera.GetWorldMatrix()) * (*mSceneCamera.GetViewMatrix());
        D3DXMatrixInverse( &invWV, NULL, &view );

        D3DXVECTOR3 viewRay(viewspaceX, viewspaceY, 1);

        D3DXVECTOR3 rayStart, rayEnd, rayDir;

        rayDir.x = viewRay.x * invWV._11 + viewRay.y * invWV._21 + viewRay.z * invWV._31;
        rayDir.y = viewRay.x * invWV._12 + viewRay.y * invWV._22 + viewRay.z * invWV._32;
        rayDir.z = viewRay.x * invWV._13 + viewRay.y * invWV._23 + viewRay.z * invWV._33;
        rayStart.x = invWV._41;
        rayStart.y = invWV._42;
        rayStart.z = invWV._43;  

        D3DXVECTOR3 intersection;

        rayEnd = rayStart + rayDir * mFarZ;

        if (RayIntersectMesh(intersection, rayStart, rayEnd, mStanfordBunnyModel)) {
            D3DXVECTOR3 dist = intersection - rayStart;

            float len = D3DXVec3Length(&dist);
            if (len > 0) {
                retVal = len;
            }
        }

        return retVal;
    }

    virtual void Update(double fTime, float fElapsedTime) 
    {
        mSceneCamera.FrameMove(fElapsedTime);
        // Camera World matrix is a misnomer, it's really just the ViewProjection matrix.
        mViewProjection = (*mSceneCamera.GetWorldMatrix()) * (*mSceneCamera.GetViewMatrix()) * (*mSceneCamera.GetProjMatrix());

        D3DXVECTOR3 eye(15, 15, 15);
        D3DXVECTOR3 at(0, 0, 0);
        D3DXVECTOR3 up(0, 1, 0);

        // Bound the spotlight tightly to the bunny
        D3DXMATRIX view, proj;
        D3DXMatrixLookAtLH(&view, &eye, &at, &up);
        D3DXMatrixPerspectiveFovLH(&proj, 6.0f / 180.0f * D3DX_PI, 1, 5, 50);
        D3DXMatrixMultiply(&mSpotlightWorldViewProjection, &view, &proj);

        // Update cursor matrix. 
        D3DXMATRIX hudScale, hudTranslate, hudDepth;
        D3DXMatrixIdentity(&hudScale);

        hudScale._11 = 32.f / mWindowSizeX;
        hudScale._22 = 32.f / mWindowSizeY;
        hudScale._41 = -1;
        hudScale._42 = 1;

        D3DXMatrixTranslation(&hudTranslate, mCursorPosX * 2.0f / mWindowSizeX, mCursorPosY * -2.0f / mWindowSizeY, 1);
        D3DXMatrixMultiply(&hudDepth, &hudScale, &hudTranslate);

        // Determine the depth under the mouse cursor.
        float depth = RayCast((mCursorPosX *  2.0f / mWindowSizeX - 1) / mSceneCamera.GetProjMatrix()->_11, 
                              (mCursorPosY * -2.0f / mWindowSizeY + 1) / mSceneCamera.GetProjMatrix()->_22 );

        if (mMouseTechnique == 1) {
            // One method of making 2D hud elements appear to hang at the correct location in world space
            // is to provide them with a correct depth. To do that and maintain the correct size in view 
            // space, you must scale the entire matrix up by this depth value. This technique is useful
            // if you can compute the depth cheaply on the CPU (for example if you have a raytrace 
            // function, or have a CPU copy of a previous frame's depth buffer). 
            mHudViewProjection = hudDepth * depth;
        } else {
            mHudViewProjection = hudDepth;
        }

    }

    HRESULT ForwardRenderScene(ID3D10Device* d3d10)
    {
        HRESULT hr = D3D_OK;

        // Get our current render targets and viewport so we can restore them after rendering shadow buffers.
        UINT viewportCount = 1;
        D3D10_VIEWPORT origVp, vp;

        ID3D10RenderTargetView *origRTV = 0;
        ID3D10DepthStencilView *origDSV = 0;
        d3d10->OMGetRenderTargets(1, &origRTV, &origDSV);

        d3d10->RSGetViewports(&viewportCount, &vp);
        origVp = vp;
        float viewportSize[2] = { vp.Width * 1.0f, vp.Height * 1.0f };

        // Create the matrices for light directions, 
        D3DXVECTOR3 lightDir(-15, -15, -15);
        D3DXVECTOR3 lightDirNorm;
        D3DXVec3Normalize(&lightDirNorm, &lightDir);

        D3DXMATRIX shadowWvp, wvp, bunnyWorldMatrix, invVP;
        D3DXMatrixInverse(&invVP, NULL, &mViewProjection);

        D3DXMatrixIdentity(&bunnyWorldMatrix);
        D3DXMatrixMultiply(&shadowWvp, &bunnyWorldMatrix, &mSpotlightWorldViewProjection);

        // Render objects into the shadow depth buffer first.
        vp.Height = vp.Width = ShadowBufferDim;
        d3d10->RSSetViewports(viewportCount, &vp);        

        d3d10->ClearDepthStencilView( mShadowsDSV, D3D10_CLEAR_DEPTH, 1.0, 0 );
        d3d10->OMSetRenderTargets(1, &mBogusShadowRTV, mShadowsDSV);

        V(mMatWorldVar->SetMatrix((float*)&bunnyWorldMatrix));
        V(mMatWorldViewProjectionVar->SetMatrix((float*)&mSpotlightWorldViewProjection));
        V_RETURN(RenderMesh(d3d10, mStanfordBunnyModel, mVertexLayout, mTechShadowBufferRender));

        // Now render the scene normally. Our render is pretty simple.
        D3DXMatrixMultiply(&wvp, &bunnyWorldMatrix, &mViewProjection);

        // Seet our viewports and restore the primary back buffer targets
        d3d10->RSSetViewports(viewportCount, &origVp);
        d3d10->OMSetRenderTargets(1, &origRTV, origDSV);
        // Now that we've set these back, go ahead and release the ref that was 
        // granted to us when we got them.
        origRTV->Release();
        origDSV->Release();

        // Set all of the variables we need to properly render our bunny.
        V(mMatWorldVar->SetMatrix((float*)&bunnyWorldMatrix));
        V(mMatWorldViewProjectionVar->SetMatrix((float*)&wvp));
        V(mMatViewProjectionInvVar->SetMatrix((float*)&invVP));
        V(mVecLightDirVar->SetFloatVector((float*)&lightDirNorm));
        V(mVecAmbientColorVar->SetFloatVector((float*)&mAmbientColor));
        V(mSpotlightTexVar->SetResource(mSpotlightRV));
        V(mShadowTexVar->SetResource(mShadowsSRV));
        V(mStereoParamTexVar->SetResource(mStereoParamRV));
        V(mVecViewportSizeVar->SetFloatVector(viewportSize));
        V(mMatSpotlightWorldViewProjectionVar->SetMatrix((float*)&mSpotlightWorldViewProjection));
        V(mPosTechniqueVar->SetInt(mPosTechnique));

        // Draw it using the forward render technique.
        V_RETURN(RenderMesh(d3d10, mStanfordBunnyModel, mVertexLayout, mTechForwardRenderScene));

        // Forcibly remove the Shader Resource View for the shadow texture so that when we render
        // shadows next frame the runtime doesn't complain that we've bound the resource 
        // for read and write.
        V(mShadowTexVar->SetResource(0));
        mTechForwardRenderScene->GetPassByIndex(0)->Apply(0);

        return hr;
    }

    virtual HRESULT RenderCursor(ID3D10Device* d3d10)
    {
        HRESULT hr = Scene::RenderCursor(d3d10);

        V(mHudTexVar->SetResource(mCursorRV));
        // Set the hud view projection matrix
        V(mMatHudViewProjectionVar->SetMatrix((float*)&mHudViewProjection));

        // Then render the cursor vb.
        V_RETURN(RenderVb(d3d10, mCursorVb, 32, mCursorIb, DXGI_FORMAT_R32_UINT, 4, 
                          D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, mVertexLayout, 
                          mTechCursorRender));

        // If we're drawing debug information, do that here.
        if (mDrawDebug) {
            V_RETURN(RenderVb(d3d10, mDebugVb, sizeof(D3DXVECTOR3), mDebugIb, DXGI_FORMAT_R32_UINT, 16,
                              D3D10_PRIMITIVE_TOPOLOGY_LINESTRIP, mDebugLayout,
                              mTechDebugRender));
        }

        return D3D_OK;
    }

    virtual HRESULT Render(ID3D10Device* d3d10) 
    {
        HRESULT hr = D3D_OK;
        V_RETURN(Scene::Render(d3d10));
        V_RETURN(ForwardRenderScene(d3d10));

        return hr;
    }

    void RenderPosText(CDXUTTextHelper* txtHelper, bool stereoActive)
    {
        if (stereoActive) {
            switch (mPosTechnique) {
            case 0: 
                txtHelper->DrawTextLine(L"Canonical unprojection using screen UV and a provided depth value fails to account");
                txtHelper->DrawTextLine(L"for a hidden mono-to-stereo transformation, and shows faults when rendering in stereo.");
                break;

            case 1:
                txtHelper->DrawTextLine(L"By passing the World Position through an attribute, we avoid the mono-to-stereo");
                txtHelper->DrawTextLine(L"transform, which is performed only on the output position from the vertex shader.");
                txtHelper->DrawTextLine(L"This costs an additional 3 scalar attributes (and 1 vector attribute), which could");
                txtHelper->DrawTextLine(L"be an issue with fat interchange formats.");
                break;

            case 2:
                txtHelper->DrawTextLine(L"Using a specially crafted texture, we're able to get per-eye constant data into the");
                txtHelper->DrawTextLine(L"vertex or pixel shader. The downside of this technique is that it adds latency to");
                txtHelper->DrawTextLine(L"the shader, which may cause performance issues in long shaders with many constants.");
            default:
                txtHelper->DrawTextLine(L"");
            };
        } else {
            txtHelper->DrawTextLine(L"When stereo is disabled, all methods produce identical results.\n");
        }
    }

    void RenderMouseText(CDXUTTextHelper* txtHelper, bool stereoActive)
    {
        if (stereoActive) {
            switch (mMouseTechnique) {
            case 0: 
                txtHelper->DrawTextLine(L"Mouse cursors (and crosshairs, for example) are typically drawn at a specific UV");
                txtHelper->DrawTextLine(L"location. When stereo is active, the mouse is drawn in one UV location, but the");
                txtHelper->DrawTextLine(L"background behind the mouse is different in the left and right image. This causes");
                txtHelper->DrawTextLine(L"your brain to separate the mouse cursor or crosshair, which is typically not desired.");
                break;

            case 1:
                txtHelper->DrawTextLine(L"By determining the screen depth under the mouse cursor (or crosshair) and rendering");
                txtHelper->DrawTextLine(L"at that depth, we can make the cursor separate by the same amount. Your brain will merge");
                txtHelper->DrawTextLine(L"the two mice back together into one cursor, making it appear to be directly over the top");
                txtHelper->DrawTextLine(L"of what is underneath it. Note that the quality of this technique is dependent upon");
                txtHelper->DrawTextLine(L"the quality of the depth information provided--the more accurate the information the");
                txtHelper->DrawTextLine(L"better the fit. This manifests in the sample as the cursor being very accurate near");
                txtHelper->DrawTextLine(L"while appearing to separate over surfaces very far from the bounding box. This could");
                txtHelper->DrawTextLine(L"be addressed by finding the polygon intersected by the pick ray.");

                break;

            default:
                txtHelper->DrawTextLine(L"");
            };
        } else {
            txtHelper->DrawTextLine(L"When stereo is disabled, the mouse is drawn as expected.");
        }
    }

    virtual void RenderText(CDXUTTextHelper* txtHelper, bool stereoActive)
    {
        Scene::RenderText(txtHelper, stereoActive);

        RenderPosText(txtHelper, stereoActive);
        txtHelper->DrawTextLine(L""); // empty line for better readability.
        RenderMouseText(txtHelper, stereoActive);
    }

    virtual HRESULT OnD3D10ResizedSwapChain(ID3D10Device* d3d10, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC* pBackBufferSurfaceDesc) 
    {
        HRESULT hr = Scene::OnD3D10ResizedSwapChain(d3d10, pSwapChain, pBackBufferSurfaceDesc);

        float fAspectRatio = pBackBufferSurfaceDesc->Width / ( FLOAT )pBackBufferSurfaceDesc->Height;
        mSceneCamera.SetProjParams( D3DX_PI / 4, fAspectRatio, mNearZ, mFarZ );
        mSceneCamera.SetWindow( pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height );
        mSceneCamera.SetButtonMasks( MOUSE_LEFT_BUTTON, MOUSE_WHEEL, MOUSE_MIDDLE_BUTTON );

        return hr;
    }

    virtual void InitUI(CDXUTDialog& dlg, int offsetX, int& offsetY, int& ctrlId) 
    { 
        Scene::InitUI(dlg, offsetX, offsetY, ctrlId);

        // Setup UI to control which techniques we're using.
        CDXUTComboBox* pComboBox;
        mPosTechniqueCtrl = ctrlId++;
        dlg.AddComboBox(mPosTechniqueCtrl, offsetX - 20, offsetY += 35, 145, 22, 0, false, &pComboBox);
        pComboBox->AddItem(L"Derived World Pos", NULL);
        pComboBox->AddItem(L"Saved World Pos", NULL);
        pComboBox->AddItem(L"StereoToMono Xform", NULL);

        mMouseTechniqueCtrl = ctrlId++;
        dlg.AddComboBox(mMouseTechniqueCtrl, offsetX - 20, offsetY += 35, 145, 22, 0, false, &pComboBox);
        pComboBox->AddItem(L"Screen Space Mouse", NULL);
        pComboBox->AddItem(L"Laser Sight Mouse", NULL);
    }

    virtual void OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl* pControl, void* pUserContext) 
    { 
        if (nControlID == mPosTechniqueCtrl) {
            CDXUTComboBox* pComboBox = (CDXUTComboBox*)pControl;
            mPosTechnique = pComboBox->GetSelectedIndex();
        }

        if (nControlID == mMouseTechniqueCtrl) {
            CDXUTComboBox* pComboBox = (CDXUTComboBox*)pControl;
            mMouseTechnique = pComboBox->GetSelectedIndex();
        }
    }



private:
    // Meshes
    CDXUTSDKMesh mStanfordBunnyModel;

    // Input Layout
    ID3D10InputLayout* mVertexLayout;
    ID3D10InputLayout* mDebugLayout;

    // FX, Techniques and Variables
    ID3D10Effect* mSceneFX;
    ID3D10EffectMatrixVariable* mMatWorldVar;
    ID3D10EffectMatrixVariable* mMatWorldViewProjectionVar;
    ID3D10EffectMatrixVariable* mMatViewProjectionInvVar;
    ID3D10EffectVectorVariable* mVecLightDirVar;
    ID3D10EffectVectorVariable* mVecAmbientColorVar;
    ID3D10EffectShaderResourceVariable* mHudTexVar;
    ID3D10EffectShaderResourceVariable* mSpotlightTexVar;
    ID3D10EffectShaderResourceVariable* mShadowTexVar;
    ID3D10EffectShaderResourceVariable* mStereoParamTexVar;
    ID3D10EffectVectorVariable* mVecViewportSizeVar;

    ID3D10EffectShaderResourceVariable* mWorldPosTexVar;
    ID3D10EffectShaderResourceVariable* mWorldNormalTexVar;

    ID3D10EffectMatrixVariable* mMatSpotlightWorldViewProjectionVar;
    ID3D10EffectMatrixVariable* mMatHudViewProjectionVar;
    ID3D10EffectScalarVariable* mPosTechniqueVar;
    
    ID3D10EffectTechnique* mTechForwardRenderScene;
    ID3D10EffectTechnique* mTechShadowBufferRender;
    ID3D10EffectTechnique* mTechCursorRender;
    ID3D10EffectTechnique* mTechDebugRender;
    
    ID3D10RenderTargetView* mBogusShadowRTV;
    ID3D10ShaderResourceView* mShadowsSRV;
    ID3D10DepthStencilView* mShadowsDSV;

    ID3D10Buffer* mCursorVb;
    ID3D10Buffer* mCursorIb;

    ID3D10Buffer* mDebugVb;
    ID3D10Buffer* mDebugIb;

    // CPU copy of FX variables
    D3DXMATRIX mViewProjection;
    D3DXVECTOR4 mAmbientColor;
    ID3D10ShaderResourceView* mCursorRV;
    ID3D10ShaderResourceView* mSpotlightRV;

    D3DXMATRIX mSpotlightWorldViewProjection;
    D3DXMATRIX mHudViewProjection;

    CModelViewerCamera mSceneCamera;
    int mUpdateCount;

    // state variables
    int mPosTechnique;
    int mMouseTechnique;

    // GUI variables
    int mPosTechniqueCtrl;
    int mMouseTechniqueCtrl;

    float mNearZ;
    float mFarZ;

    bool mDrawDebug;
};

