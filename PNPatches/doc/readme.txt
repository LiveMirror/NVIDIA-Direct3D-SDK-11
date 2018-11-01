PNPatches sample navigation keys are as follows:
================================================

'W','S','A','D' - move camera (fly mode)
Left mouse button - change camera direction
'1','2','3' - choose one of predefined camera positions
'Backspace' - switch UI on/off

UI can be used to:
==================

- Choose one of geometry rendering modes:
  * no geometry
  * coarse model (initial untessellated model)
  * tessellated coarse (coarse model tessellated and displaced, with normal map)
  * detailed geometry (model with high poly count used to generate
    displacement and normal map)
- Increase Tessellation level (only possible when tessellated coarse geometry
  is rendered).
- Update Camera. Update internal frustum information used to cull patches in Hull
  Shader.
- Show Wireframe.
- Show Normals. Renders normals, tangents, and bitangents as colored lines.
- Fix Displacement Seams. Enable/disable technique used to fix seams between
  triangles adjacent in world space but disjoint in texture space.
- Draw Normal Map.
- Draw Texture Coordinates.
- Draw Displacement Map.
- Draw Checker Board. Debug rendering mode allowing to see size of single
  displacement texel overlayed on top of rendered geometry.
- Reload Effect. If you changed .fx file, pressing this button will force
  application to load, compile, and apply new shaders for rendering geometry.
