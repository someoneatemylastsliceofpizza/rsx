# Subtitles (`subt`)
Compiled closed-caption data.  
This data is similar to Source Engine's [closed-caption](https://developer.valvesoftware.com/wiki/Closed_Captions) files, except compiled into an RPak file.

## Preview
TODO

## Export Formats

### TXT
Subtitles are exported as a newline separated list

### CSV

Columns:  
`\"hash\",\"color\",\"subtitle\"`

- hash : CRC32 checksum of the subtitle's key
- color : color of the subtitle when displayed to the user. Derived from \<clr> tags in the subtitle text
- subtitle : subtitle text

#### Subtitle Tags/Codes
The following metadata tags may appear in the subtitle text to customise how the text is displayed to the user:

- clr : Changes the colour of the subtitle text
- len : Overrides the duration of the subtitle
- delay : Sets a delay for parts of the subtitle to be displayed

Note: there may be more than these that I have not seen. For a full list that may be supported from Source Engine, see the [Valve Developer Wiki article](https://developer.valvesoftware.com/wiki/Closed_Captions) on Closed Captions.