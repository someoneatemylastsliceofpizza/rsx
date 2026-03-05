# Patch Master (`Ptch`)

This asset type is only used once across all of the game's RPak files, in `patch_master.rpak`.

The data in this asset defines the latest patch version for each patched RPak file. This enables the game to load the latest version of a pak's data by simply requesting the base version; e.g., loading common(02).rpak and common(01).rpak when common.rpak is loaded.

