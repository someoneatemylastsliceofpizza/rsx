# Asset Types
This is a list of all of the asset types that are supported in some form by RSX



| Type | Name                                                              | Support (Name/Preview/Export)<sup>1</sup> | Notes                                                                                                    |
| ---- | ----------------------------------------------------------------- | ----------------------------------------- | -------------------------------------------------------------------------------------------------------- |
| anir | [Animation Recording](./assets/rpak/AnimationRecording.md)        | 🟥/🟥/🟩                                     |                                                                                                          |
| arig | [Animation Rig](./assets/rpak/AnimationRig.md)                    | 🟩/🟥/🟩                                     |                                                                                                          |
| aseq | [Animation Sequence](./assets/rpak/AnimationSequence.md)          | 🟩/🟨/🟩                                     |                                                                                                          |
| asqd | [Animation Sequence Data](./assets/rpak/AnimationSequenceData.md) | 🟥/🟥/🟩                                     |                                                                                                          |
| dtbl | [Datatable](./assets/rpak/Datatable.md)                           | 🟥/🟩/🟩                                     |                                                                                                          |
| efct | [Particle Effect](./assets/rpak/ParticleEffect.md)                | 🟥/🟥/🟥                                     |                                                                                                          |
| font | [UI Font Atlas](./assets/rpak/Font.md)                            | 🟨/🟩/🟩                                     |                                                                                                          |
| impa | [Impact Definition](./assets/rpak/ImpactDefinition.md)            | 🟩/🟥/🟩                                     |                                                                                                          |
| locl | [Localization](./assets/rpak/Localization.md)                     | 🟩/🟥/🟩                                     |                                                                                                          |
| matl | [Material](./assets/rpak/Material.md)                             | 🟩/🟨/🟩                                     | Preview includes some material properties and textures, but not visual material preview                  |
| mdl_ | [Model](./assets/rpak/Model.md)                                   | 🟩/🟩/🟩                                     |                                                                                                          |
| msnp | [Material Snapshot](./assets/rpak/MatrialSnapshot.md)             | 🟥/🟩/🟥                                     |                                                                                                          |
| odla | [On-Demand Loading Asset](./assets/rpak/OdlAsset.md)              | 🟥/🟥/🟥                                     | Not enabled on current releases. Support depends on the linked asset. See page for more information      |
| Ptch | [Patch Master](./assets/rpak/PatchMaster.md)                      | 🟥/🟥/🟥                                     | Used internally by RSX to load the latest available pakfile patch                                        |
| rlcd | [LCD Screen Effect](./assets/rpak/LcdScreenEffect.md)             | 🟥/🟥/🟩                                     |                                                                                                          |
| rmap | [Map](./assets/rpak/Map.md)                                       | 🟩/🟥/🟥                                     |                                                                                                          |
| rson | [RSON](./assets/rpak/RSON.md)                                     | 🟥/🟩/🟩                                     |                                                                                                          |
| rtk  | [RTK Script](./assets/rpak/RTK.md)                                | 🟩/🟩/🟩                                     | Stored and exported as text, however the text format is not documented                                   |
| shdr | [Shader](./assets/rpak/Shader.md)                                 | 🟥/🟨/🟩                                     |                                                                                                          |
| shds | [Shader Set](./assets/rpak/ShaderSet.md)                          | 🟥/🟨/🟩                                     |                                                                                                          |
| stgs | [Settings Data](./assets/rpak/Settings.md)                        | 🟩/🟩/🟩                                     |                                                                                                          |
| stlt | [Settings Layout](./assets/rpak/SettingsLayout.md)                | 🟩/🟩/🟩                                     |                                                                                                          |
| subt | [Subtitles](./assets/rpak/Subtitles.md)                           | 🟩/🟩/🟩                                     |                                                                                                          |
| txan | [Texture Animation](./assets/rpak/TextureAnimation.md)            | 🟥/🟥/🟩                                     |                                                                                                          |
| txls | [Texture List](./assets/rpak/TextureList.md)                      | 🟥/🟩/🟩                                     | Only one of these assets exists, so naming is unsupported but hardcoded                                  |
| txtr | [Texture](./assets/rpak/Texture.md)                               | 🟨/🟩/🟩                                     | Naming is supported, but only when the "debug name" variable exists. See asset page for more information |
| ui   | [RUI Element](./assets/rpak/UI.md)                                | 🟩/🟩/🟩                                     |                                                                                                          |
| uiia | [UI Image](./assets/rpak/UIImage.md)                              | 🟨/🟩/🟩                                     | Naming is supported, but only when the "debug name" variable exists. See asset page for more information |
| uimg | [UI Image Atlas](./assets/rpak/UIImageAtlas.md)                   | 🟨/🟩/🟩                                     | Only present in Titanfall 2 and older Apex builds (pre-S11.1). Name is derived from texture if available |
| wepn | [Weapon Definition](./assets/rpak/WeaponDefinition.md)            | 🟩/🟥/🟩                                     |                                                                                                          |
| wrap | [Wrapped File](./assets/rpak/WrappedFile.md)                      | 🟩/🟨/🟩                                     | Preview is WIP for certain types of wrapped files, but most are not able to be previewed                 |

<sup>1</sup>:
- Name : original asset name can be retrieved or derived by RSX
- Preview : asset contents can be previewed without exporting
- Export: asset can be exported to a documented binary or text format