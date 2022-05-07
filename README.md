# MCEdit-utils
 Utilities needed for MCEdit development

These utilities are not required to run the engine, but can but help debug some part of it.
All user interfaces are based on SITGL/SDL1. Here are the purpose of each utility:

* **TileFinder**: this is the utility that was used to generate all the texture/block models in resources/*.js files.
* **Frustum**: simulate the frustum and cave culling of MCEdit v2 using a simpified 2d view (XY plane). Much easier to debug, the code has been kept as close as possible to MCEdit. The algorithm is also described extensively in doc/internals.html (MCEdit repository), this utility implements everything described in this document.
* **SkyLight**: simulate skylight updates using a 2d grid. SkyLight updates are particularly annoying to debug, because it usually involves hundreds, if not thousands of voxel updates. Limiting the problem to 2d makes it much easier to debug. Contrary to the frustum utility, this one is not as close as the code used in the 3d engine (most notably: Y increases downward, whereas in 3d space it increases upward). Still, the code should be very close in its logic to what is used in the 3d engine.
* **ChunkLoad**: simluate multi-threaded chunk loading and block allocation/free. These parts are not trivial at all, and therefore are documented in doc/internals.html (MCEdit repository).
* **Skydome**: this utility is used to generate the dynamic sky texture used by this engine.
* **StaticTables**: there are a few static tables in this engine that have what appears to be cryptic numbers coming out of nowhere. Those tables are usually too small to be generated by code (the code would take way more space than the tables themselves). This is the utility used to generate them: its a basic console command (does not rely on SITGL/SDL1) that outputs all the tables to stdout.

# Quick overview: TileFinder

![TileFinder screenshot](https://raw.githubusercontent.com/crystalcrag/WikiResources/main/TileFinder_v2.png)

# Quick overview: Frustum

![Frustum screenshot](https://raw.githubusercontent.com/crystalcrag/WikiResources/main/FrustumApp.png)

# Quick overview: ChunkLoad

![ChunkLoad screenshot](https://raw.githubusercontent.com/crystalcrag/WikiResources/main/ChunkLoadApp.png)

# Quick overview: SkyDome

![SkyDome screenshot](https://raw.githubusercontent.com/crystalcrag/WikiResources/main/SkydomeApp.png)

# Quick overview: SkyLight

![SkyLight screenshot](https://raw.githubusercontent.com/crystalcrag/WikiResources/main/SkyLightApp.png)
