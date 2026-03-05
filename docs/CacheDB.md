# Cache Database

The RSX cache database is a lookup table for strings values hashed by the reSource engine's 64-bit hash function to return the original string.

This hash function is primarily used for calculating the GUID of each asset in RPak files using the asset's name.

These values are stored by RSX so that assets can be given their real name even if they do not have a name variable stored in the RPak file, such as Datatable or RSON assets.

## File Format
Cache database files use the layout and structures as defined below.

The current 

### Structures
```cpp
struct CacheDBHeader_t
{
	uint32_t fileVersion;       // Version of the CacheDB file format that this file was built with
	uint32_t fileCRC;           // CRC32 value calculated from all of the data in the file after this header structure
	uint32_t numMappings;       // Number of hash mappings in this file

	uint32_t reserved_0;        // Padding reserved for future use

	uint64_t stringTableOffset; // Offset in this file for all strings referenced by hash mappings

	uint32_t reserved_1[2];     // Padding reserved for future use
}

struct CacheHashMapping_t
{
	uint64_t guid; // Hashed value of the string below
	uint32_t strOffset; // absolute string value offset: (header.stringTableOffset + strOffset);
	
	// If this mapping relates to an asset, this offset will point to a string for where the asset can be found
	// This allows RSX to automatically load the container file that holds any missing dependency assets.
	uint32_t fileNameOffset; // Initialised as zero if unused
};
```

### Layout
```cpp
CacheDBHeader_t header;
CacheHashMapping_t[header.numMappings];
```