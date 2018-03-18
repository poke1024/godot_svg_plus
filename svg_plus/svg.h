/*************************************************************************/
/*  svg.h 			                                                     */
/*************************************************************************/

#ifndef SVG_H
#define SVG_H

#include <nanosvg.h>
#include <nanosvgrast.h>

#include "core/image.h"
#include "io/resource_loader.h"
#include "scene/2d/node_2d.h"

class NanoSVG : public Reference {
	GDCLASS(NanoSVG, Reference);

	NSVGimage *data;

public:
	NSVGimage *get_data() const;

	NanoSVG(const String &p_path, const String &p_units, float p_dpi);
	~NanoSVG();
};

class SVGPath : public Reference {
	GDCLASS(SVGPath, Reference);

	Ref<NanoSVG> svg;
	const NSVGpath *path;

protected:
	static void _bind_methods();

public:
	bool is_closed() const;
	Array get_points() const;

	SVGPath(const Ref<NanoSVG> &p_svg, const NSVGpath *p_path);
};

class SVGShape : public Reference {
	GDCLASS(SVGShape, Reference);

	Ref<NanoSVG> svg;
	const NSVGshape *shape;

protected:
	static void _bind_methods();

public:
	Variant get_fill() const;
	Variant get_stroke() const;
	float get_opacity() const;
	Array get_paths() const;

	SVGShape(const Ref<NanoSVG> &p_svg, const NSVGshape *p_shape);
};

class SVG : public Resource {
	GDCLASS(SVG, Resource);

	Ref<NanoSVG> svg;

protected:
	static void _bind_methods();

public:
	Error load(const String &p_path, const String &p_units, float p_dpi);

	float get_width() const;
	float get_height() const;

	Array get_shapes() const;

	void update_mesh(Node *p_parent, float p_scale, float p_tolerance, int p_max_levels);
	Ref<Image> rasterize(int p_width, int p_height, float p_tx, float p_ty, float p_scale) const;
};

/////////////

class SVGMesh : public Node2D {
	GDCLASS(SVGMesh, Node2D);

	Ref<Resource> svg_image;
	Ref<SVG> svg;
	float tesselate_scale;
	float tesselate_tolerance;

	void update_mesh();

protected:
	static void _bind_methods();

public:
	Ref<Resource> get_svg() const;
	void set_svg(const Ref<Resource> &p_svg);

	float get_tesselate_scale() const;
	void set_tesselate_scale(float p_scale);

	float get_tesselate_tolerance() const;
	void set_tesselate_tolerance(float p_tolerance);

	SVGMesh();
};

/////////////

class ResourceFormatLoaderSVG : public ResourceFormatLoader {
public:
	virtual RES load(const String &p_path, const String &p_original_path = "", Error *r_error = NULL);
	virtual void get_recognized_extensions(List<String> *p_extensions) const;
	virtual bool handles_type(const String &p_type) const;
	virtual String get_resource_type(const String &p_path) const;
};

#endif
