Multi-View Soft Shadows

The Multi-View Soft Shadows (MVSS) algorithm renders contact-hardening shadows by averaging hard shadows from multiple point lights evenly distributed on an area light. This is similar to accumulation-buffer rendering, the main difference being that with MVSS the scene is rendered a single time from the eye’s point of view. First, the scene geometry is rasterized into multiple shadow maps with one shadow map per point light. Second, soft shadows are rendered by averaging the hard shadows from each shadow map in a pixel shader. In this DirectX 11 code sample, the shadow maps are implemented as a depth texture array, they are generated with one depth-only pass per shadow map, and they are fetched with one bilinear hardware-PCF sample per shadow map.

Running the Sample

The left button controls the light position and the right button the camera position. The mouse wheel zooms in and out.
The light source is a disk area light. The “Light Size” slider changes the radius of the light.
The “Visualize Depth” check box outputs the shadow-map depths on the screen. The visualized depths are the one from the last-rendered shadow map.
In the GUI, the sample displays the GPU time spent generating the 28 shadow maps, and performing the shading pass, as well as the number of shadow-casting triangles rendered in each shadow map, and the total amount of allocated video memory for the shadow maps.

Code Organization

In MultiViewSoftShadows.h, Scene::Render() is the main rendering method, which renders the shadow maps and performs the shading pass.
MultiViewSoftShadows.cpp contains the code for the GUI and DXUT callbacks.
The HLSL source code of the shaders is located in the “shaders” subdirectory.
