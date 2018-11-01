
To create another maya hair project you need to create another folder, with the following four files, named exactly

scalp_mesh.obj
The roots of the hair strands tessellated into a mesh. Refer to Hair\MeshProcess for how to create this file 

hair_vertices.txt
The actual hair strands, with the hair strands ordered the *same* way as scalp_mesh orders the roots. Refer to Hair\MeshProcess for how to create this file  

collision_objects.txt
txt file containing all the collision implicits representing the model. Refer to LongMayaHairResources\collision_objects for the syntax
These implicits will be all that the hair will collide against

full_model.x
the model to render, this can be the whole characrter, just the scalp, or anything in between
this model is used for rendering and shadowing purposes, not simulation

