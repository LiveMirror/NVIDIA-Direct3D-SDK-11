
**************************************************************************************************************
Overview:
Simulating and rendering realistic hair with tens of thousands of strands is something that until recently was not feasible for games. However, with the increasing programmability and power of Graphics Hardware not only is it possible to simulate and render realistic hair entirely on the Graphics Processing Unit (GPU), but it is also possible to do it at interactive frame rates. Furthermore, new hardware features like the Tessellation and Compute Shaders make the creation and simulation of hair easier and faster. This sample shows how to simulate and render hair on the GPU using both D3D11’s new shader stages (tessellation and compute shader) and using older APIs like D3D10. 


**************************************************************************************************************
Options:

In the default start up mode the sample exhibits hair being simulated using the compute shader and rendered using hardware tessellation. To reset all the options to the default use the “Reset” button near the top. 

Camera: Use left mouse drag to move the camera, and mouse wheel to zoom the camera. 

Light: To move the light use SHIFT + left mouse drag (the light is indicated by a white arrow) 

Model: To move the model use right mouse drag. To rotate the model drag the middle mouse button / mouse wheel.

Wind: Use SHIFT + right mouse drag to move the direction of the wind (shown by the green arrow)

Animation: The sample provides a way to move the head in a pre-animated motion (note that hair is still simulated in real time), which can be turned on by the “Play Animation” check box, and looped with the “Loop Animation” check box.

Hair Rendering: There are a number of ways to change the look of the hair, including changing its color using the provided combo box, and making it short or curly using checkboxes.To choose the type of interpolation to be used, Single Strand or Multi Strand, use the check boxes “Render M Strands” and “Render S Strands”.
The checkbox “Hardware Tessellation” allows you to toggle between using D3D11 Tessellation for rendering the hair, or rendering them using D3D10 style “instanced tessellation” (se Tariq08).

LOD: By default the sample uses dynamic LOD for the hair rendering, so that as you zoom the camera in or out different amount of hair are rendered. The dynamic LOD level is indicated by the slider under the “Dynamic LOD” checkbox – note that as you zoom the camera the  LOD and Hair Width sliders changes their values, indicating current LOD. If you wish to use manual LOD you can uncheck the “Dynamic LOD” checkbox and manually adjust the number of hairs and the width of each hair using the two sliders.

Hair Simulation : By default the hair is simulated using the Compute Shader. In order to disable this you can uncheck the “Compute Shader” checkbox, in which case the sample uses multipass simulation on the vertex/geometry shader. “Simulation LOD” allows the sample to drop the rate of simulation to once every n frames based on the LOD. In order to disable Simulation LOD, uncheck the checkbox. When simulation LOD is disabled you have the option of pausing the simulation using the “Simulate” checkbox.
The “Show Collision” checkbox allows the user to visualize the collision implicits that are being used in the hair simulation. Wind is being applied to the simulation, which can be turned off using the “Add Wind Force” checkbox, or by pressing ‘k’. The “Wind Strength” slider directly below this checkbox allows the user to adjust the amount of wind force.


**************************************************************************************************************
Code Guide:

In order to integrate this demo the most important files to look at are Hair.cpp (functions RenderInterpolatedHair, StepHairSimulation, and OnD3D11FrameRender), HairSimulateCS.hlsl (shaders for hair simulation using the compute shader), and Hair.fx (techniques for hair rendering and simulation). Some useful techniques in Hair.fx are InterpolateAndRenderM_HardwareTess (render hair using multistrand interpolation), InterpolateAndRenderS_HardwareTess (render hair using single strand based interpolation) and InterpolateAndRenderCollisions_HardwareTess (pass to render multi strand hair to a texture in order to find colliding vertices).

In addition, you have to create and import a hair cut into the application. For this demo we have used a default haircut available in Maya, exported it as points and parsed and massaged in LoadHairFile.cpp.
Please note that a number of the parameters used for hair rendering and simulation are specific to this hair cut and the dimensions of our scene, and will have to be modified to fit different hair models and scenes.
