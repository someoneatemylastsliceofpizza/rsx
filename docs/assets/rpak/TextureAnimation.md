# Texture Animation (`txan`)

## Preview
Currently, RSX does not support previewing Texture Animation assets.

## Export Formats

### TXAN (.txan)
This is a custom file format.

#### File Structures

```cpp
struct TextureAnimFileHeader_t
{
    int32_t magic;         // "TXAN" (0x4E415854)
    uint16_t fileVersion;  // Incremented on any changes to the file format
    uint16_t assetVersion; // Version of the TXAN asset that this file describes
    uint32_t layerCount;   // Number of texture animation layers in the file
    uint32_t slotCount;    // Number of texture animation slots in the file
};

struct TextureAnimLayer_t
{
	// the layer animates from start texture to end, start can be higher than
	// the end as the engine simply just wraps the number around; we can
	// animate from texture 10 to texture 2, and from 2 to 10, etc.
    uint16_t startSlot;
	uint16_t endSlot;

	float unk2; // some scale used to switch between textures, cross-fading perhaps?

	uint16_t flags;
	uint16_t unk5; // most likely padding for 4 byte boundary, unused in functions above.
};
```

#### File Layout

```cpp
TextureAnimFileHeader_t header;

TextureAnimLayer_t layers[header.layerCount];

uint8_t slotIndices[header.slotCount];

```