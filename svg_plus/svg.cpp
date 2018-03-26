/*************************************************************************/
/*  svg.cpp            			                                         */
/*************************************************************************/

#include "svg.h"
#include "../svg/image_loader_svg.h"
#include "bezier_2d.cpp"
#include "os/file_access.h"
#include "scene/2d/polygon_2d.h"

NSVGimage *NanoSVG::get_data() const {

	return data;
}

NanoSVG::NanoSVG(const String &p_path, const String &p_units, float p_dpi) {

	data = NULL;

	Vector<uint8_t> buf = FileAccess::get_file_as_array(p_path);

	String str;
	str.parse_utf8((const char *)buf.ptr(), buf.size());

	data = nsvgParse((char *)str.utf8().get_data(), p_units.utf8().get_data(), p_dpi);
}

NanoSVG::~NanoSVG() {

	if (data) {
		nsvgDelete(data);
		data = NULL;
	}
}

/////////////////////////

void SVGPath::_bind_methods() {

	ClassDB::bind_method(D_METHOD("is_closed"), &SVGPath::is_closed);
	ClassDB::bind_method(D_METHOD("get_points"), &SVGPath::get_points);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "closed"), "", "is_closed");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "points"), "", "get_points");
}

bool SVGPath::is_closed() const {

	return path->closed;
}

Array SVGPath::get_points() const {

	Array points;
	ERR_FAIL_COND_V(points.resize(path->npts) != OK, Array());
	for (int i = 0; i < path->npts; i++) {
		points[i] = Vector2(path->pts[2 * i + 0], path->pts[2 * i + 1]);
	}
	return points;
}

SVGPath::SVGPath(const Ref<NanoSVG> &p_svg, const NSVGpath *p_path) {

	svg = p_svg;
	path = p_path;
}

/////////////////////////

static Variant convert_nsvg_paint(const NSVGpaint &p_paint) {

	switch (p_paint.type) {
		case NSVG_PAINT_NONE: {
			return Variant();
		} break;
		case NSVG_PAINT_COLOR: {
			const float r = (p_paint.color & 0xff) / 255.0;
			const float g = ((p_paint.color >> 8) & 0xff) / 255.0;
			const float b = ((p_paint.color >> 16) & 0xff) / 255.0;
			const float a = ((p_paint.color >> 24) & 0xff) / 255.0;
			return Color(r, g, b, a);
		} break;
	}
	return Variant();
}

static Rect2 get_shape_bounds(const NSVGshape *p_shape) {

	return Rect2(
			p_shape->bounds[0],
			p_shape->bounds[1],
			p_shape->bounds[2] - p_shape->bounds[0],
			p_shape->bounds[3] - p_shape->bounds[1]);
}

void SVGShape::_bind_methods() {

	ClassDB::bind_method(D_METHOD("get_fill"), &SVGShape::get_fill);
	ClassDB::bind_method(D_METHOD("get_stroke"), &SVGShape::get_stroke);
	ClassDB::bind_method(D_METHOD("get_opacity"), &SVGShape::get_opacity);
	ClassDB::bind_method(D_METHOD("get_paths"), &SVGShape::get_paths);

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "fill"), "", "get_fill");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "stroke"), "", "get_stroke");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "opacity"), "", "get_opacity");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "paths"), "", "get_paths");
}

Variant SVGShape::get_fill() const {

	return convert_nsvg_paint(shape->fill);
}

Variant SVGShape::get_stroke() const {

	return convert_nsvg_paint(shape->stroke);
}

float SVGShape::get_opacity() const {

	return shape->opacity;
}

Array SVGShape::get_paths() const {

	Array paths;
	NSVGpath *path = shape->paths;
	while (path) {
		paths.push_back(memnew(SVGPath(svg, path)));
		path = path->next;
	}
	return paths;
}

SVGShape::SVGShape(const Ref<NanoSVG> &p_svg, const NSVGshape *p_shape) {

	svg = p_svg;
	shape = p_shape;
}

/////////////////////////

Error SVG::load(const String &p_path, const String &p_units, float p_dpi) {

	Ref<NanoSVG> new_svg = memnew(NanoSVG(p_path, p_units, p_dpi));
	if (!new_svg->get_data()) {
		return ERR_FILE_CORRUPT;
	}
	svg = new_svg;
	return OK;
}

float SVG::get_width() const {

	return svg.is_valid() ? svg->get_data()->width : 0;
}

float SVG::get_height() const {

	return svg.is_valid() ? svg->get_data()->height : 0;
}

Array SVG::get_shapes() const {

	ERR_FAIL_COND_V(!svg.is_valid(), Array());

	Array shapes;
	NSVGshape *shape = svg->get_data()->shapes;
	while (shape) {
		shapes.push_back(memnew(SVGShape(svg, shape)));
		shape = shape->next;
	}

	return shapes;
}

void SVG::update_mesh(Node *p_parent) {

	NSVGshape *shape;

	shape = svg->get_data()->shapes;
	Rect2 bounds;
	if (shape) {
		bounds = get_shape_bounds(shape);
		shape = shape->next;
	}
	if (shape) {
		bounds = bounds.merge(get_shape_bounds(shape));
		shape = shape->next;
	}

	const float ox = bounds.position.x + bounds.size.width / 2;
	const float oy = bounds.position.y + bounds.size.height / 2;
	const float original_size = MAX(bounds.size.width, bounds.size.height);
	const float sx = 100 / original_size;
	const float sy = 100 / original_size;

	int untitled_no = 1;

	shape = svg->get_data()->shapes;
	while (shape) {
		Bezier2D *bezier = memnew(Bezier2D);

		Color fill_color = convert_nsvg_paint(shape->fill);
		bezier->set_fill_color(fill_color);

		Color stroke_color = convert_nsvg_paint(shape->stroke);
		bezier->set_stroke_color(stroke_color);
		bezier->set_stroke_width(shape->stroke.type != NSVG_PAINT_NONE ? shape->strokeWidth : 0.0);

		switch (shape->fillRule) {
			case NSVG_FILLRULE_NONZERO: {
				bezier->set_fill_rule(Bezier2D::FILLRULE_NONZERO);
			} break;
			case NSVG_FILLRULE_EVENODD: {
				bezier->set_fill_rule(Bezier2D::FILLRULE_EVENODD);
			} break;
			default: {
				ERR_FAIL();
			} break;
		}

		NSVGpath *path = shape->paths;
		while (path) {
			Vector<Vector2> points;
			points.resize(path->npts);
			for (int i = 0; i < path->npts; i++) {
				points[i] = Vector2(
						(path->pts[2 * i + 0] - ox) * sx,
						(path->pts[2 * i + 1] - oy) * sy);
			}
			bezier->add_path(points);

			path = path->next;
		}

		String shape_name = shape->id;
		if (shape_name == "") {
			shape_name = "untitled-" + String::num(untitled_no++);
		}

		if (p_parent->has_node(shape_name)) {
			// FIXME
			Node *old = p_parent->get_node(shape_name);
			p_parent->remove_child(old);
			old->queue_delete();
		}
		bezier->set_name(shape_name);
		p_parent->add_child(bezier);
		bezier->set_owner(p_parent->get_owner());

		shape = shape->next;
	}
}

Ref<Image> SVG::rasterize(int p_width, int p_height, float p_tx, float p_ty, float p_scale) const {

	ERR_FAIL_COND_V(!svg.is_valid(), Ref<Image>());

	SVGRasterizer rasterizer;

	ERR_FAIL_COND_V(p_width <= 0, Ref<Image>());
	ERR_FAIL_COND_V(p_height <= 0, Ref<Image>());

	const int w = p_width;
	const int h = p_height;

	PoolVector<uint8_t> dst_image;
	dst_image.resize(w * h * 4);

	PoolVector<uint8_t>::Write dw = dst_image.write();

	rasterizer.rasterize(svg->get_data(), p_tx, p_ty, p_scale, (unsigned char *)dw.ptr(), w, h, w * 4);

	dw = PoolVector<uint8_t>::Write();
	Ref<Image> image;
	image.instance();
	image->create(w, h, false, Image::FORMAT_RGBA8, dst_image);

	return image;
}

void SVG::_bind_methods() {

	ClassDB::bind_method(D_METHOD("get_width"), &SVG::get_width);
	ClassDB::bind_method(D_METHOD("get_height"), &SVG::get_height);
	ClassDB::bind_method(D_METHOD("get_shapes"), &SVG::get_shapes);

	ClassDB::bind_method(D_METHOD("load", "path", "units", "dpi"), &SVG::load, DEFVAL("px"), DEFVAL(96.0f));
	ClassDB::bind_method(D_METHOD("rasterize", "tx", "ty", "scale"), &SVG::rasterize, DEFVAL(0.0f), DEFVAL(0.0f), DEFVAL(1.0f));

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "shapes"), "", "get_shapes");
}

/////////////////////////

void SVGInstance::update_mesh() {

	if (svg.is_valid()) {

		svg->update_mesh(this);
		update();
	}
}

void SVGInstance::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_svg"), &SVGInstance::set_svg);
	ClassDB::bind_method(D_METHOD("get_svg"), &SVGInstance::get_svg);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "svg", PROPERTY_HINT_RESOURCE_TYPE, "Texture"), "set_svg", "get_svg");
}

Ref<Resource> SVGInstance::get_svg() const {

	return svg_image;
}

void SVGInstance::set_svg(const Ref<Resource> &p_svg) {

	// this is a hack, as we always get a texture here due to
	// Godot's default SVG importer being an image importer.
	if (p_svg.is_valid()) {
		svg.instance();
		if (svg->load(p_svg->get_path(), "px", 96) != OK) {
			svg = Ref<SVG>();
		} else {
			update_mesh();
		}
	} else {
		svg_image = Ref<Resource>();
		svg = Ref<SVG>();
	}
}

SVGInstance::SVGInstance() {
}

/////////////////////////

RES ResourceFormatLoaderSVG::load(const String &p_path, const String &p_original_path, Error *r_error) {

	if (r_error)
		*r_error = ERR_FILE_CANT_OPEN;

	Ref<SVG> svg;
	svg.instance();
	Error err = svg->load(p_path, "px", 96);
	if (r_error)
		*r_error = err;

	return svg;
}

void ResourceFormatLoaderSVG::get_recognized_extensions(List<String> *p_extensions) const {

	p_extensions->push_back("svg");
}

bool ResourceFormatLoaderSVG::handles_type(const String &p_type) const {

	return (p_type == "SVG");
}

String ResourceFormatLoaderSVG::get_resource_type(const String &p_path) const {

	String el = p_path.get_extension().to_lower();
	if (el == "svg")
		return "SVG";
	return "";
}
