/*************************************************************************/
/*  svg.cpp            			                                         */
/*************************************************************************/

#include "svg.h"
#include "os/file_access.h"
#include "thirdparty/misc/clipper.hpp"
#include "thirdparty/misc/triangulator.h"
#include "scene/2d/polygon_2d.h"
#include "../svg/image_loader_svg.h"

NSVGimage *NanoSVG::get_data() const {

	return data;
}

NanoSVG::NanoSVG(const String &p_path, const String &p_units, float p_dpi) {

	data = NULL;

	Vector<uint8_t> buf = FileAccess::get_file_as_array(p_path);

	String str;
	str.parse_utf8((const char *)buf.ptr(), buf.size());

	data = nsvgParse((char*)str.utf8().get_data(), p_units.utf8().get_data(), p_dpi);
}

NanoSVG::~NanoSVG() {

	if (data) {
		nsvgDelete(data);
		data = NULL;
	}
}

/////////////////////////

static void flatten_cubic_bezier(
	float x1, float y1, float x2, float y2,
	float x3, float y3, float x4, float y4,
	float p_tolerance, int p_level, Array &r_points)
{
	// adapted from nsvg__flattenCubicBez()

	float x12,y12,x23,y23,x34,y34,x123,y123,x234,y234,x1234,y1234;
	float dx,dy,d2,d3;

	if (p_level <= 0) return;

	x12 = (x1+x2)*0.5f;
	y12 = (y1+y2)*0.5f;
	x23 = (x2+x3)*0.5f;
	y23 = (y2+y3)*0.5f;
	x34 = (x3+x4)*0.5f;
	y34 = (y3+y4)*0.5f;
	x123 = (x12+x23)*0.5f;
	y123 = (y12+y23)*0.5f;

	dx = x4 - x1;
	dy = y4 - y1;
	d2 = Math::abs(((x2 - x4) * dy - (y2 - y4) * dx));
	d3 = Math::abs(((x3 - x4) * dy - (y3 - y4) * dx));

	if ((d2 + d3)*(d2 + d3) < p_tolerance * (dx*dx + dy*dy)) {
		r_points.push_back(Vector2(x4, y4));
		return;
	}

	x234 = (x23+x34)*0.5f;
	y234 = (y23+y34)*0.5f;
	x1234 = (x123+x234)*0.5f;
	y1234 = (y123+y234)*0.5f;

	flatten_cubic_bezier(x1,y1, x12,y12, x123,y123, x1234,y1234, p_tolerance, p_level - 1, r_points);
	flatten_cubic_bezier(x1234,y1234, x234,y234, x34,y34, x4,y4, p_tolerance, p_level - 1, r_points);
}

static Array flatten_path(NSVGpath *p_path, float p_scale, float p_tolerance, int p_max_levels) {

	const float s = p_scale;
	Array points;
	points.push_back(Vector2(p_path->pts[0] * s, p_path->pts[1] * s));
	for (int i = 0; i + 3 < p_path->npts; i += 3) {
		const float *p = &p_path->pts[i * 2];
		flatten_cubic_bezier(p[0] * s, p[1] * s, p[2] * s, p[3] * s, p[4] * s, p[5] * s, p[6] * s, p[7] * s, p_tolerance, p_max_levels, points);
	}
	points.push_back(Vector2(p_path->pts[0] * s, p_path->pts[1] * s));
	return points;
}

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

static Array get_polygons(const NSVGshape *p_shape, float p_scale, float p_tolerance, int p_max_levels) {

	ClipperLib::Paths clipper_paths;
	NSVGpath *path = p_shape->paths;
	while (path) {
		Array points = flatten_path(path, p_scale, p_tolerance, p_max_levels);

		ClipperLib::Path clipper_path;
		for (int i = 0; i < points.size(); i++) {
			const Vector2 p = points[i];
			clipper_path.push_back(ClipperLib::IntPoint(p.x, p.y));
		}
		clipper_paths.push_back(clipper_path);

		path = path->next;
	}

	ClipperLib::PolyFillType fill_type;
	switch (p_shape->fillRule) {
		case NSVG_FILLRULE_NONZERO: {
			fill_type = ClipperLib::pftNonZero;
		} break;
		case NSVG_FILLRULE_EVENODD: {
			fill_type = ClipperLib::pftEvenOdd;
		} break;
		default: {
			ERR_FAIL_V(Array());
		} break;
	}

	ClipperLib::SimplifyPolygons(clipper_paths, fill_type);

	List<TriangulatorPoly> polys;
	for (int i = 0; i < clipper_paths.size(); i++) {
		const int n = clipper_paths[i].size();
		TriangulatorPoly poly;
		poly.Init(n);
		for (int j = 0; j < n; j++) {
			const ClipperLib::IntPoint &p = clipper_paths[i][j];
			poly[j] = Vector2(p.X, p.Y);
		}
		if (!ClipperLib::Orientation(clipper_paths[i])) {
			poly.SetHole(true);
		}
		polys.push_back(poly);
	}

	List<TriangulatorPoly> solution;
	TriangulatorPartition partition;
	partition.RemoveHoles(&polys, &solution);

	Array solution_paths;
	for (int i = 0; i < solution.size(); i++) {
		Array solution_path;
		for (int j = 0; j < solution[i].GetNumPoints(); j++) {
			solution_path.push_back(solution[i][j]);
		}
		solution_paths.push_back(solution_path);
	}

	return solution_paths;
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

void SVG::update_mesh(Node *p_parent, float p_scale, float p_tolerance, int p_max_levels) {

	NSVGshape *shape = svg->get_data()->shapes;
	int untitled_no = 1;
	while (shape) {
		Color color = convert_nsvg_paint(shape->fill);
		Array polygons = get_polygons(shape, p_scale, p_tolerance, p_max_levels);
		const int n_polygons = polygons.size();

		String shape_name = shape->id;
		if (shape_name == "") {
			shape_name = "untitled-" + String::num(untitled_no++);
		}

		Node *container = NULL;
		if (p_parent->has_node(shape_name)) {
			container = p_parent->get_node(shape_name);
			while (container->get_child_count() > 0) {
				Node *child = container->get_child(0);
				container->remove_child(child);
				child->queue_delete();
			}
		} else {
			container = memnew(Node2D);
			container->set_name(shape_name);
			p_parent->add_child(container);
			container->set_owner(p_parent->get_owner());
		}

		for (int i = 0; i < n_polygons; i++) {
			Polygon2D *polygon = memnew(Polygon2D);
			polygon->set_polygon(polygons[i]);
			polygon->set_color(color);
			polygon->set_name("path" + String::num(i + 1));
			container->add_child(polygon);
			polygon->set_owner(p_parent->get_owner());
		}
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

void SVGMesh::update_mesh() {

	if (svg.is_valid()) {

		svg->update_mesh(this, tesselate_scale, tesselate_tolerance, 10);
	}
}

void SVGMesh::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_svg"), &SVGMesh::set_svg);
	ClassDB::bind_method(D_METHOD("get_svg"), &SVGMesh::get_svg);

	ClassDB::bind_method(D_METHOD("set_tesselate_scale"), &SVGMesh::set_tesselate_scale);
	ClassDB::bind_method(D_METHOD("get_tesselate_scale"), &SVGMesh::get_tesselate_scale);

	ClassDB::bind_method(D_METHOD("set_tesselate_tolerance"), &SVGMesh::set_tesselate_tolerance);
	ClassDB::bind_method(D_METHOD("get_tesselate_tolerance"), &SVGMesh::get_tesselate_tolerance);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "svg", PROPERTY_HINT_RESOURCE_TYPE, "Texture"), "set_svg", "get_svg");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "tesselate_scale"), "set_tesselate_scale", "get_tesselate_scale");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "tesselate_tolerance"), "set_tesselate_tolerance", "get_tesselate_tolerance");
}

Ref<Resource> SVGMesh::get_svg() const {

	return svg_image;
}

void SVGMesh::set_svg(const Ref<Resource> &p_svg) {

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

float SVGMesh::get_tesselate_scale() const {

	return tesselate_scale;
}

void SVGMesh::set_tesselate_scale(float p_scale) {

	tesselate_scale = p_scale;
	update_mesh();
}

float SVGMesh::get_tesselate_tolerance() const {

	return tesselate_tolerance;
}

void SVGMesh::set_tesselate_tolerance(float p_tolerance) {

	tesselate_tolerance = p_tolerance;
	update_mesh();
}

SVGMesh::SVGMesh() {

	tesselate_scale = 100;
	tesselate_tolerance = 10;
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
