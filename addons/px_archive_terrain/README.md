# PX Archive Terrain

This addon provides a `PXArchiveTerrain` GDExtension node for Phoenix terrain archives.

## Use

1. Copy `addons/px_archive_terrain` into a Godot project.
2. Build the native library with `godot-cpp` available through `GODOT_CPP_PATH`.
3. Add a `PXArchiveTerrain` node.
4. Set `terrain_folder` to the terrain root that contains `LatXXN/LongXXXE.7z`.
5. Set `origin_latitude`, `origin_longitude`, and `origin_altitude` near the area you want to inspect.

The node uses the active `Camera3D` as the eye point by default. It converts that local camera position back to latitude, longitude, and altitude using the configured origin, opens the matching archive group, and loads only visible tile meshes.

Set `use_camera_eye` to false and call `set_eye_geodetic(latitude, longitude, altitude)` to drive archive selection manually.

## Build

Example:

```sh
set GODOT_CPP_PATH=D:\Programming\GitHub\godot-cpp
python -m SCons platform=windows target=template_debug arch=x86_64
python -m SCons platform=windows target=template_release arch=x86_64
```

Linux and macOS builds use the same addon source with the corresponding `platform` value.
