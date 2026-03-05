# Texture (`txtr`)

Asset containing general-purpose texture data.
Some of the uses include:
- Materials
- UI Image Atlas (Titanfall 2 & older Apex  Legends versions only)

## Preview
TODO

## Export Formats

### PNG (Highest Mip)
The highest quality version of the texture asset is exported as a [Portable Network Graphics (PNG)](https://en.wikipedia.org/wiki/PNG) file.

If this texture asset is a texture array, the highest quality version of each item in the texture array will also be exported.

### PNG (All Mips)
All resolutions of the texture asset are exported as separate [Portable Network Graphics (PNG)](https://en.wikipedia.org/wiki/PNG) files.

Usually, this consists of smaller copies of the texture with dimensions in descending powers of two (1024, 512, 256, etc.)

### DDS (Highest Mip)
The highest quality version of the texture asset is exported as a [DirectDraw Surfaces (DDS)](https://en.wikipedia.org/wiki/DirectDraw_Surface) file.

If this texture asset is a texture array, the highest quality version of each item in the texture array will also be exported.

### DDS (All Mips)
All resolutions of the texture asset are exported as separate [DirectDraw Surfaces (DDS)](https://en.wikipedia.org/wiki/DirectDraw_Surface) files.

Usually, this consists of smaller copies of the texture with dimensions in descending powers of two (1024, 512, 256, etc.)

### DDS (Mipmapped)
All resolutions of the texture asset are exported as a single, mipmapped [DirectDraw Surfaces (DDS)](https://en.wikipedia.org/wiki/DirectDraw_Surface) file.


### JSON (Metadata)
Not yet implemented.
