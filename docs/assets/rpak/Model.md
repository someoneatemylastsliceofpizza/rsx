# Model (`mdl_`)

A 3D Model.

## Preview
TODO

## Export Formats

### Cast
Uses the [CAST](https://github.com/dtzxporter/cast) file format.

All LODs are exported as individual ".cast" files.

All materials used by the model are also exported alongside the model using the current Material and Texture export settings.

### RMAX
Custom export format

### RMDL
Exports the files in their raw binary format:
- .rmdl - Modified [source model file](https://developer.valvesoftware.com/wiki/MDL). Contains most of the non-mesh information about the model.
- .phy - Modified [valve physics file](https://developer.valvesoftware.com/wiki/PHY). Contains some collision data for the model
- .rson - [RSON](./RSON.md) text file containing information about linked [Animation Rig](./AnimationRig.md) and [Animation Sequence](./AnimationSequence.md) assets
- .vg - Raw mesh data, "hardware data". Typically streamed from `.starpak` files. Format varies greatly depending on asset version  

Materials are not exported when this export setting is used.

TODO: which versions use .vvc, .vvd, .dx11.vtx, .vvw?

### SMD
Uses the text-based [StudioModel Data](https://developer.valvesoftware.com/wiki/SMD) file format.

All materials used by the model are also exported alongside the model using the current Material and Texture export settings.

A corresponding [QC](https://developer.valvesoftware.com/wiki/QC) file is also generated alongside the .smd file, as this contains required information for recompiling the model using tools such as `studiomdl`.

### STL (Valve Physics)
Generates an [STL](https://en.wikipedia.org/wiki/STL_(file_format)) file based on the contents of the Valve Physics (.phy) segment of the model.


### STL (Respawn Physics)
Generates an [STL](https://en.wikipedia.org/wiki/STL_(file_format)) file based on the contents of Respawn's BVH4 collision segment of the model.
