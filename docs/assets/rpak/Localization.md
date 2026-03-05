# Localization (`locl`)
<sup>This asset type does not exist in Titanfall 2</sup>

These assets contain a hash map of localization keys and the associated localized strings.

One of these localization assets exists for each supported text language, contained in their own RPak file, such as `localization_english.rpak`

## Preview
Currently RSX does not support previewing Animation Recording assets.

## Export Formats

### LOCL (.locl)
This exports the data as a Valve KeyValues text file

#### Layout

Example:
```js
"localization_english"
{
    // ...
    "5df85afa428ecca8" "Most Knockdowns"
	"fff4877635538a03" "My Heart!"
	"531ccf75516b27ff" "Bullet"
	"409e7534fca0c74b" "Remote Work"
	"4a0fcf19748b1532" "Battle Modder"
    // ...
}
```

The root key of the file (e.g., `"localization_english"`) will always be the same as the name of the .locl file (and also the contained RPak file).

Each localization entry consists of a 64-bit hex string as a key, and a string value in the language that the asset defines.

The key is the hashed version of the [Source Engine localization](https://developer.valvesoftware.com/wiki/VGUI_Documentation#Localization) token that identifies the string. For example, "`spray_heart_01`" becomes "`fff4877635538a03`" for the above string "My Heart!".