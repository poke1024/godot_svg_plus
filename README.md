Preliminary note: This project is currently abandoned (it turned into [TÖVE](https://github.com/poke1024/tove2d)).

This is an experimental module for Godot to add better SVG support, more specifically:

* import SVGs as polygon meshes in the editor (providing a new `SVGMesh` node)
* access SVGs resources (shapes, paths, points) via GDScript (using a new `SVG` resource)

This module is experimental as it

* currently only supports non-gradient fills
* uses `Polygon2D` to render the geometry

A full module would support strokes, gradients and use some kind of Godot 2d mesh. Still, for many art styles (like e.g. kenney.nl), gradient-free meshes can go a very long way.

How to install: move `svg_plus` into your `godot/modules` folder and recompile Godot.

Demo video:

https://www.dropbox.com/s/4j82nn5ok7osgte/demo.mov?dl=0

The demo graphics in the demo projects are from https://kenney.nl/.
