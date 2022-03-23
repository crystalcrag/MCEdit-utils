# MCEdit-utils
 Utilities needed for MCEdit development

These utilities are not required at all to run the engine, but can but help debug some complex part of it.
All of these are based on SITGL/SDL1. Here are the purpose of each utility:
    * Frustum: simulate the frustum and cave culling of MCEdit using a simpified 2d (instead of 3d). Much easier to debug, the code has been kept as close as possible to MCEdit.
	* SKyLight: simluate skylight update using a 2d grid. SkyLight update is particularly annoying to debug, because it can usually involve hundreeds, if not thousands of block update in the voxel space. Limiting the problem to 2d makes much easier to debug.
	* Skydome: this utility is used to generate the dynamic sky texture used by the engine,
	* TileFinder: this is the utility that was used to generate all the texture/block models in resources/*.js files.
