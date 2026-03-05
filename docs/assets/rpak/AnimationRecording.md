# Animation Recording (`anir`)

Animation recording assets contain data for playing back a series of animations in-game. This can simulate character movement, such as the "ghost" in Titanfall 2's gauntlet runs, and Bloodhound's tutorial mode in Apex Legends.

Recordings can either be created by the game, like in Titanfall 2's gauntlet, or loaded from RPak assets, like the Apex Legends tutorial.

## Preview
Currently RSX does not support previewing Animation Recording assets.

## Export Formats

### ANIR (.anir)
This is a custom file format.

#### File Structures

```cpp
struct Vector
{
    float x,y,z;
};

struct Vector2D
{
    float x,y;
};

typedef Vector QAngle;

struct AnimRecordingFileHeader_s
{
	int magic; // ANIR_FILE_MAGIC ('R'<<24 | 'I'<<16 | 'N'<<8 | 'A')
	unsigned short fileVersion; // Version of the ANIR file format that this file was built with
	unsigned short assetVersion; // Version of the Animation Recording asset that this file contains

	Vector startPos; // 3-dimensional coordinates for the origin of this recording
	Vector startAngles; // 3-dimensional vector for the initial angles of this recording

	int stringBufSize; // Size of the buffer used to store all of the strings referenced within this file

	int numElements; // Number of pose parameters set by this recording
	int numSequences; // Number of animation sequences used in this recording

	int numRecordedFrames; // Number of animation frames in this recording
	int numRecordedOverlays;

	int animRecordingId;
};

struct AnimRecordingFrame_s
{
	float currentTime; // Time at which this frame is played
	Vector origin;
	QAngle angles;
	int animRecordingButtons;
	int animSequenceIndex;
	float animCycle;
	float animPlaybackRate;
	int overlayIndex;
	int activeAnimOverlayCount;

    // The following fields are unknown but still written to the exported file
	__int16 field_34;
	__int16 field_36;
	int field_38;
	int field_3C;
	__int16 field_40;
	char field_42;
	char field_43;
};

struct AnimRecordingOverlay_s
{
	int animSequenceIndex;
	float animOverlayCycle;
	float overlays[6];
	int slot;
};
```

#### File Layout

```cpp
AnimRecordingFileHeader_s header;

string poseParamNames[header.numElements]; // Set of null-terminated strings
Vector2D poseParamValues[header.numElements];

string animSequenceNames[header.numSequences];

AnimRecordingFrame_s recordedFrames[header.numRecordedFrames];
AnimRecordingOverlay_s recordedOverlays[header.numRecordedOverlays];
```