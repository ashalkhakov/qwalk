# Intro

Qwalk is a tool for converting between different 3D game model formats, most importantly MDL, MD2,
and MD3 (Quake, Quake II, Quake III). Some modelling software has import/export functionality for these
formats, but in most cases it is incomplete, especially if you want to export to MDL or MD2. Qwalk's
goal is to implement all features of each of these formats.

The current version of the Qwalk is a command-line model converter. Qwalk also comes with a simple OpenGL model viewer. Later on, these will probably merged into a GUI-driven viewer+converter.

# Supported formats

## MDL (Quake)

MDLs can be imported and exported with full functionality. Each and every dusty corner of the format specs are supported.
It's not recommended to use this tool to tweak MDLs (that is, import from MDL then export to MDL), because there may some loss of precision
or "drift" of vertex positions, and because this program doesn't export the "front/back" skin mapping, which means that if you
import a model which uses traditional front/back skin mapping (that is, almost every MDL in existence), some vertices will be
unnecessarily duplicated.

## MD2 (Quake II)

MD2s can be imported and exported with full functionality.
When an MD2 is imported, the program will look for the external skin files referenced by the model. So, make sure you are in the
right directory (e.g. "baseq2") when you run the program, because most skins have a path like "models/monsters/soldier/skin.pcx".
Alternatively, you can manually specify a texture using the "-tex" command-line parameter.

## MD3 (Quake III)

MD3s can be imported with full functionality. Exporting is currently not implemented at all.
MD3s currently do not automatically import any textures at all. You'll have to use the "-tex" command-line parameter to manually
specify an image to load as a texture. The PCX, TGA, and JPEG formats are supported. Also note that MD3s which use more than one
texture file aren't currently supported.
Tags are rendered in the model viewer as tri-colour axes, but they aren't useful for anything yet.

# Guide

## Model converter

The model converter is a command-line tool which can import MDL, MD2, or MD3 models, and export to MDL or MD2.

### Usage

```
modelconv [options] -i infilename outfilename
Output format is specified by the file extension of outfilename.
Options:
  -i filename        specify the model to load (required).
  -notex             remove all existing skins from model after importing.
  -tex filename      replace the model's texture with the given texture. This
                     is required for any texture to be loaded onto MD2 or MD3
                     models (the program doesn't automatically load external
                     skins). Supported formats are PCX, TGA, and JPEG.
  -texwidth #        see below
  -texheight #       resample the model's texture to the given dimensions.
  -skinpath x        specify the path that skins will be exported to when
                     exporting to md2 (e.g. "models/players"). Should not
                     contain trailing slash. If skinpath is not specified,
                     skins will be created in the same folder as the model.
  -flags #           set model flags, such as rocket smoke trail, rotate, etc.
                     See Quake's defs.qc for a list of all flags.
  -synctype x        set synctype flag, only used by Quake. The valid values
                     are sync (default) and rand.
  -offsets_x #       see below
  -offsets_y #       see below
  -offsets_z #       set the offsets vector, which only exists in the MDL
                     format and is not used by Quake. It's only supported here
                     for reasons of completeness.
  -renormal          recalculate vertex normals.
  -rename_frames     rename all frames to "frame1", "frame2", etc.
  -force             force "yes" response to all confirmation requests
                     regarding overwriting existing files or creating
                     nonexistent paths.</pre>
```

Examples:
* `modelconv -i tris.md2 out.mdl`
  Converts the model `tris.md2` to `out.mdl`. All textures will be converted to the Quake palette (properly taking fullbrights into account).
* `modelconv -i model.md3 -tex texture.jpg -texwidth 300 -texheight 200 out.mdl`
  Imports the model `model.md3`, loads into it the image `texture.jpg` (resampled to 300x200), and exports it to MDL, converting the texture to the Quake palette.

## Model viewer

The model viewer is currently very basic. It has no GUI and no animation playback controls.
It simply loops through all animations in the model. You can move the camera by moving the mouse while the
left mouse button is held, and zoom in and out by using the mouse wheel. Press SPACEBAR to toggle "pause lighting",
which will stop the light source from following the camera. Press SLASH to cycle through the skins in the model,
if it has more than one.

### Usage

```
viewer [options] filename
Options:
  -tex filename      replace the model's texture with the given texture. This
                     is required for any texture to be loaded onto MD2 or MD3
                     models (the program doesn't automatically load external
                     skins). Supported formats are PCX, TGA, and JPEG.
  -width #           see below
  -height #          specify the video resolution of the window. The default is
                     640x480. The window is resizeable when the program is
                     running.
  -bpp #             specify the bits-per-pixel resolution. Valid values are 16
                     or 32 (default).
  -frame #           specify a single frame to view. If this frame index refers
                     to a framegroup, that framegroup will be animated.
  -texturefiltering  enable bilinear texture filtering (smoothed texture). The
                     default is nearest-neighbour sampling, which looks
                     "blocky" (similar to software-rendered engines).
  -firstperson       attach the model the view, as the gun models in Quake.
  -nolerp            don't interpolate the vertex positions when animating.
  -wireframe         enable wireframe rendering.
```

# Todo

For a full todo list, see the [TODO file on the git repository](qwalk/TODO).

Some more far-fetched major features for later versions:

* Segmented Quake 3 player models (using multiple MD3s) could be connecting using the original tags and animation.cfg and exported to a single mesh.
* Resample animations at different playback speeds, smoothed with interpolation.
* Load OBJ models.
* Load Doom 3 skeletal md5mesh/md5anim files and export them to vertex animation formats.
* Convert MAP files (e.g. the health/ammo pickups) to models.
* Read files out of PAK or PK3/ZIP files.
* Generate sample QuakeC code for monsters, containing animation information and stub AI code.
