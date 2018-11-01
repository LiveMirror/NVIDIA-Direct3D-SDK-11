GregoryACC11 Tessellation sample 


Overview:
==================
This sample implements a recent scheme on efficiently approximating Catmull-Clark subdivision surfaces (ACC)[4]. Compared with other similar schemes [1,2,3], [4] fits the best to 
the DirectX11 tessellation pipeline as it is the fastest yet general enough to allow the meshes that contain triangles, quads, and boundaries. This sample shows how to tessellates 
3 types of meshes in a typical game scene: an animated character (a monster frog), a static object (a tree), and an environmental object (pebble road).


UI can be used to:
==================
- Toggle Adaptive Tessellation Mode:
  * off ==> enables the Static TFs slider bar to adjust the tessellation factors manually
  * on ==> Adaptive tessellation based on screen space and depth value gradient in the displacement map
- Toggle wireframe
- Show Gregory patches
  Highlight the gregory patches in purple. The rest are bicubic patches. 
- Toggle flat shading
  * off ==> use normal map for shading computation
  * on ==> use flat shading
- Adjust spotlighting
  * use the slider bar to adjust the cone coverage of the spotlight
- Toggle Animation

Code guide:
==================
There are two possible types of objects in a rendering scene that need ACC tessellation: animated objects and non-animated objects. For static objects, there's no need to 
compute patch control points per frame as they always remain the same. Therefore, it's better to separate code path of non-animated objects from animated objects. In this sample,
DynamicMesh.cpp" and "DynamicMesh.h" defines the common data members and methods for animated meshes while "StaticMesh.cpp" and "StaticMesh.h" defines the common data members and 
methods for an non-animated meshes.

In this sample, an animated, displaced monster frog is an instance of a DynamicMesh, and the tree model is an instance of a StaticMesh. Both of them are tessellated using gregory 
patches and bicubic patches. The detailed algorithm is described in [4]. The pebble road is tessellated using the simplest tessellation approach: triangle dicing. 

The baker tool takes the common file types, such as .obj, and pre-computes the stencils for each type of patch primitive. It reorganizes the positions, normals, stencils, connection info, and 
everything else that are required by ACC schemes for fast objects-loading to the tessellation pipeline. "BzrFile_stencil.cpp" and "Bzrfile_stencil.h" load the meshes from bzr input files.  


References:
==================
[1]  “Approximating Catmull-Clark Subdivision Surfaces with Bicubic Patches”, Charles Loop, Scott Schaefer,  ACM Transactions on Graphics, Volume 27, issue 1,  2008.
[2]  “GPU Smoothing of Quad Meshes”, T. Ni, Y. Yeo, A. Myles, V. Goel, J. Peters, In IEEE International Conference on Shape Modeling and Applications,  2008.
[3]  “Fast Parallel Construction of Smooth Surfaces from Meshes with Tri/Quad/Pent Facets ”, A. Myles, T. Ni, J. Peters, Symposium on Geometry Processing, 2008. 
[4]  “Approximating Subdivision Surfaces with Gregory Patches for Hardware Tessellation”, Charles Loop, Scott Schaefer, Tianyun Ni, Ignacio Castano, ACM Transactions on 
      Graphics (Proceedings of SIGGRAPH Asia), Vol. 28 No. 5, pages 151:1 - 151:9, 2009.


 

