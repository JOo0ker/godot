# PX Archive Terrain

This addon provides a `PXArchiveTerrain` GDExtension node for Phoenix terrain archives.

## Use

1. Copy `addons/px_archive_terrain` into a Godot project.
2. Build the native library with `godot-cpp` available through `GODOT_CPP_PATH`.
3. Add a `PXArchiveTerrain` node.
4. Set `terrain_folder` to the terrain root that contains `LatXXN/LongXXXE.7z`.
5. Set `origin_latitude`, `origin_longitude`, and `origin_altitude` near the area you want to inspect.

The node uses the active `Camera3D` as the eye point. It converts that local camera position back to latitude, longitude, and altitude using the configured origin, opens the matching archive group, and updates terrain tiles as the camera moves and rotates.

## Inspector text

The exported property names remain English for scene compatibility. When the editor language uses Chinese, the inspector shows Chinese labels and Chinese property descriptions. English labels and descriptions remain available when the editor language is English.

## Build

Example:

```sh
set GODOT_CPP_PATH=D:\Programming\GitHub\godot-cpp
python -m SCons platform=windows target=template_debug arch=x86_64
python -m SCons platform=windows target=template_release arch=x86_64
```

Linux and macOS builds use the same addon source with the corresponding `platform` value.
