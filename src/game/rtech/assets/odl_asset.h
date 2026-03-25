#pragma once

struct ODLAssetHeader_t
{
	uint64_t odlPakAssetGuid; // GUID of the "odlp" asset that describes the pakfile that contains this on-demand asset
	char* assetName; // Name of the asset that "originalAssetGuid" points to

	char unk10[8];
	uint64_t originalAssetGuid; // GUID of the original asset that this "odla" asset points to

	uint64_t placeholderAssetGuid; // GUID of the placeholder asset that is used while the real asset's pak is being loaded
								   // This seems to only be used for model assets as it has a value of 0 for ODL sticker assets
};

class ODLAsset
{
public:
	ODLAsset(const ODLAssetHeader_t* const header);

	const char* GetOriginalAssetName() const { return originalAssetName; };
	const char* GetPakName() const { return originalAssetPakName; };

	uint64_t GetPakAssetGuid() const { return pakAssetGuid; };
	uint64_t GetOriginalAssetGuid() const { return originalAssetGuid; };
	uint64_t GetPlaceholderAssetGuid() const { return placeholderAssetGuid; };

private:
	const char* originalAssetName;

	uint64_t pakAssetGuid;
	uint64_t originalAssetGuid;
	uint64_t placeholderAssetGuid; // "*_loading.rmdl". only set on ODL models as explained above

	const char* originalAssetPakName;
};

static_assert(sizeof(ODLAssetHeader_t) == 0x28);