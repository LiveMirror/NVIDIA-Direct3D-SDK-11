

**************************************************************************************************************
Overview:
This code sample shows a method of rendering dynamic scenes with diffuse intereflections in real time. The code is an implementation of the method introduced by Kaplanyan. For more information please refer to:
Cascaded light propagation volumes for real-time indirect illumination [Kaplanyan and Dachsbacher 2010]


**************************************************************************************************************
Options:

Moving Camera: use A,D,W,S,Q,E to move camera and left mouse drag to rotate camera.

Moving Light: use left mouse drag + SHIFT to move light

Move Flag: use I,K,J,L,U,O to move flag

Disable/Enable UI rendering: press 'y'

Reset Button: resets demo and options

Simple Light Setup Combo Option:
Direct light Strength: the amount that direct light contributes to the final scene rendering
Enable Interreflection: enable/disable diffuse inter-reflected light
Interreflection Contribution: the amount that diffuse interreflected light contributes to the final scene rendering
Flux amplifier: the multiplier that the incoming flux to a cell is multiplied with

Advanced Light Setup Combo Option:
Light Radius: the distance that the directional light has influence till
Bilinear init: helps reduce flickering at the cost of performance. When rasterizing pixels to the LPV or GV we rasterize 8 trilinearly weighted points rather than rendering one point at the snapped voxel center. Note: In order to avoid light leaking into solid objects this option should be enhanced to take into account the GV values. 
Use RSM Cascade: allow each LPV to use its own RSM
Use occlusion: use occlusion information in the light propagation
Multiple bounces: use multiple bounces in the light propagation (note that disabling this does not affect performance - to get the true performance advantage of not doing multiple bounces you have to undefined USE_MULTIPLE_BOUNCES.)
Depth Peel Layers: the number of layers of depth peeling used to create RSMs. Currently only depth peeling from the point of view of the camera is enabled (if you want to enable from the POV of the light you have to define g_depthPeelFromLight to be true) 
Occlusion Amplifier: a multiplier for the occlusion values used to occlude propagated light: occlusion = 1.0 - max(0,min(1.0f, occlusionAmplifier*innerProductSH(GVSHCoefficients , dirSH )))
Reflected light Amp: a multiplier for just the reflected flux that is computed at each cell
Cascade/Hierarchy Combo: choose between a cascaded LPV (this is the original approach) or a hierarchical LPV.
Movable LPV checkbox: choose whether to let the LPVs move with the viewer or be static at a location
Reset LPV Xform: reset the transformation of the LPV (resets to the center of the atrium)
Use single LPV checkbox: choose between a single LPV or multiple ones (for example in the cascade formation)
LPV level slider: if you are using a single LPV choose which one you want to use.
VPL displace: how much to displace the placement of VPLs along the normal
LPV iterations: number of iterations of propagation
Vis LPV Bounding Box: see the bounding boxes of the LPVs
Normal Based Damping: enable/disable light damping based on normal of geometry

Scene Setup Combo:
Use shadow map: only used for the direct lighting component of the final render
Render scene: toggle rendering the main scene
textures for rendering: enable/disable using texture maps for rendering the final scene. If this is disabled all geometry is assumed to be white.
textures for RSM: enable/disable using the mesh texture maps when rendering the RSM. If this is disabled all geometry is assumed to be white when rendering RSMs
show movable mesh: enable/disable rendering of movable flag
Shadow depth Bias: depth bias used for shadow map rendering
Animate Light: enable/disable animation of the light. Note that when this option is selected "bilinear initialization" is also automatically selected (which reduces light flickering but reduces performance); in order to disable "bilinear initialization" please navigate to the "Advanced Light Setup" combo option and disable bilinear init.  


Viz Intermediates Combo:
Combo box at the top lets you decide which intermediate texture you want to see (either one of the RSMs, or one of the LPVs or one of the GVs).
Viz texture: visualize the chosen intermediate above as a texture
Viz LPV: visualize the chosen LPV as spheres in the scene corresponding to voxel cell centers. Note that the only acceptable values in the combobox above when this option is checked are the LPV and GV textures, not the RSMs.


**************************************************************************************************************
Code Guide:

The main render loop is located in Lighting.cpp in the DrawScene function. In here, for each frame, we clear the LPVs, render the scene to the RSMs, rasterize the RSMs into LPVs and GVs, propagate the indirect illumination and finally render the scene with direct and indirect light. 

The implementation of the core LPV and RSM functions (for rendering RSMs, initializing LPVs and GVs and propagating light) are located in the file CoreLPVandRSM.cpp

The shader file that implements LPV and GV initialization functions is called LPV.hlsl. 

Light propagation shader code is located in LPV_propagate.hlsl in the function PropagateLPV. PropagateLPV_Simple is a simpler version of this code that ignores multiple bounces and geometry.

Code for rendering the scene is located in SimpleShading.hlsl. The main vertex and pixel shaders for rendering the scene (with direct and indirect light) are called VS and PS. The shaders for rendering to the RSM are called VS_RSM and PS_RSM (or VS_RSM_DepthPeeling and PS_RSM_DepthPeeling if depth peeling is enabled). 