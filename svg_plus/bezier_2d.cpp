/*************************************************************************/
/*  bezier_2d.cpp                                                        */
/*************************************************************************/

#include "bezier_2d.h"
#include "core/math/geometry.h"
#include "core/sort.h"
#include "scene/2d/polygon_2d.h"

static void flatten_cubic_bezier(
		float x1, float y1, float x2, float y2,
		float x3, float y3, float x4, float y4,
		int p_level, const Tesselator2D::TesselationParameters &p_parameters, ClipperLib::Path &r_path) {
	// adapted from nsvg__flattenCubicBez()

	float x12, y12, x23, y23, x34, y34, x123, y123, x234, y234, x1234, y1234;
	float dx, dy, d2, d3;

	if (p_level > p_parameters.max_levels) {
		return;
	}

	x12 = (x1 + x2) * 0.5f;
	y12 = (y1 + y2) * 0.5f;
	x23 = (x2 + x3) * 0.5f;
	y23 = (y2 + y3) * 0.5f;
	x34 = (x3 + x4) * 0.5f;
	y34 = (y3 + y4) * 0.5f;
	x123 = (x12 + x23) * 0.5f;
	y123 = (y12 + y23) * 0.5f;

	dx = x4 - x1;
	dy = y4 - y1;
	d2 = Math::abs(((x2 - x4) * dy - (y2 - y4) * dx));
	d3 = Math::abs(((x3 - x4) * dy - (y3 - y4) * dx));

	if ((d2 + d3) * (d2 + d3) < p_parameters.tolerance * (dx * dx + dy * dy)) {
		r_path.push_back(ClipperLib::IntPoint(x4, y4));
		return;
	}

	x234 = (x23 + x34) * 0.5f;
	y234 = (y23 + y34) * 0.5f;
	x1234 = (x123 + x234) * 0.5f;
	y1234 = (y123 + y234) * 0.5f;

	flatten_cubic_bezier(x1, y1, x12, y12, x123, y123, x1234, y1234, p_level + 1, p_parameters, r_path);
	flatten_cubic_bezier(x1234, y1234, x234, y234, x34, y34, x4, y4, p_level + 1, p_parameters, r_path);
}

static void flatten_path(const Vector<Vector2> &p_path, const Tesselator2D::TesselationParameters &p_parameters, ClipperLib::Path &r_path) {

	const float s = 1.0 * p_parameters.scale;
	const ClipperLib::IntPoint p0 = ClipperLib::IntPoint(p_path[0].x * s, p_path[0].y * s);

	r_path.push_back(p0);
	const int n = p_path.size();
	for (int i = 0; i + 3 < n; i += 3) {
		const Vector2 *p = &p_path[i];
		flatten_cubic_bezier(p[0].x * s, p[0].y * s, p[1].x * s, p[1].y * s, p[2].x * s, p[2].y * s, p[3].x * s, p[3].y * s, 1, p_parameters, r_path);
	}
	r_path.push_back(p0);
}

Tesselator2D *Bezier2D::_get_tesselator() const {

	Node *node = get_parent();
	while (node) {
		Tesselator2D *tesselator = Object::cast_to<Tesselator2D>(node);
		if (tesselator) {
			return tesselator;
		}

		node = node->get_parent();
	}

	return NULL;
}

void Bezier2D::_mark_dirty() {

	Tesselator2D *tesselator = _get_tesselator();
	if (tesselator) {
		tesselator->mark_dirty(tesselator->get_path_to(this));
	}
}

void Bezier2D::_tesselate_lock(const Tesselator2D::TesselationParameters &p_parameters, ClipperLib::Path &r_points) {

	r_points.clear();
	const float scale = p_parameters.scale;
	for (int i = 0; i < paths.size(); i++) {
		int k = 0;
		while (k < paths[i].points.size()) {
			ClipperLib::IntPoint p(paths[i].points[k].x * scale, paths[i].points[k].y * scale);
			r_points.push_back(p);
			k += 3;
		}
	}
}

void Bezier2D::_tesselate_fill(
		const Tesselator2D::TesselationParameters &p_parameters,
		ClipperLib::Paths &r_paths) {

	ClipperLib::Paths clipper_paths;
	for (int i = 0; i < paths.size(); i++) {
		ClipperLib::Path clipper_path;
		flatten_path(paths[i].points, p_parameters, clipper_path);
		clipper_paths.push_back(clipper_path);
	}

	ClipperLib::PolyFillType fill_type;
	switch (fill_rule) {
		case FILLRULE_NONZERO: {
			fill_type = ClipperLib::pftNonZero;
		} break;
		case FILLRULE_EVENODD: {
			fill_type = ClipperLib::pftEvenOdd;
		} break;
		default: {
			ERR_FAIL();
		} break;
	}

	ClipperLib::SimplifyPolygons(clipper_paths, r_paths, fill_type);
}

void Bezier2D::_tesselate_stroke(
		const Tesselator2D::TesselationParameters &p_parameters,
		const ClipperLib::Paths &p_fill,
		ClipperLib::Paths &r_paths) {

	if (stroke_width > 0) {
		ClipperLib::Path brush;

		const float w = int(0.5 * stroke_width * p_parameters.scale);
		brush.push_back(ClipperLib::IntPoint(-w, -w));
		brush.push_back(ClipperLib::IntPoint(w, -w));
		brush.push_back(ClipperLib::IntPoint(w, w));
		brush.push_back(ClipperLib::IntPoint(-w, w));

		ClipperLib::Paths fill_closed;
		for (int i = 0; i < p_fill.size(); i++) {
			if (p_fill[i].empty()) {
				continue;
			}
			ClipperLib::Path p;
			for (int j = 0; j < p_fill[i].size(); j++) {
				p.push_back(p_fill[i][j]);
			}
			p.push_back(p_fill[i][0]);
			fill_closed.push_back(p);
		}

		ClipperLib::MinkowskiSum(brush, fill_closed, r_paths, false);
	} else {
		r_paths.clear();
	}
}

Dictionary Bezier2D::_edit_get_state() const {
	Dictionary state = Node2D::_edit_get_state();
	state["offset"] = offset;
	return state;
}

void Bezier2D::_edit_set_state(const Dictionary &p_state) {
	Node2D::_edit_set_state(p_state);
	set_offset(p_state["offset"]);
}

void Bezier2D::_edit_set_pivot(const Point2 &p_pivot) {
	set_position(get_transform().xform(p_pivot));
	set_offset(get_offset() - p_pivot);
}

Point2 Bezier2D::_edit_get_pivot() const {
	return Vector2();
}

bool Bezier2D::_edit_use_pivot() const {
	return true;
}

Rect2 Bezier2D::_edit_get_rect() const {

	Tesselator2D *tesselator = _get_tesselator();
	ERR_FAIL_COND_V(!tesselator, Rect2());

	Tesselator2D::Tesselation tesselation;
	tesselator->get_tesselation(tesselator->get_path_to(this), tesselation);

	return Rect2(tesselation.bounds.position + offset, tesselation.bounds.size);
}

bool Bezier2D::_edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const {

	Tesselator2D *tesselator = _get_tesselator();
	ERR_FAIL_COND_V(!tesselator, false);

	Tesselator2D::Tesselation tesselation;
	tesselator->get_tesselation(tesselator->get_path_to(this), tesselation);

	Point2 p = p_point - offset;

	for (int i = 0; i < tesselation.fill.size(); i++) {
		if (Geometry::is_point_in_polygon(p, tesselation.fill[i])) {
			return true;
		}
	}

	for (int i = 0; i < tesselation.stroke.size(); i++) {
		if (Geometry::is_point_in_polygon(p, tesselation.stroke[i])) {
			return true;
		}
	}

	return false;
}

int Bezier2D::get_path_count() const {

	return paths.size();
}

Vector<Vector2> Bezier2D::get_path_points(int p_path) const {

	return paths[p_path].points;
}

void Bezier2D::set_path_points(int p_path, const Vector<Vector2> &p_points) {

	paths[p_path].points = p_points;
	_mark_dirty();
	update();
}

void Bezier2D::add_path(const Vector<Vector2> &p_points) {

	Path path;
	path.points = p_points;
	path.closed = true;
	paths.push_back(path);

	_mark_dirty();
	update();
}

void Bezier2D::set_fill_color(const Color &p_color) {

	fill_color = p_color;
	update();
}

Color Bezier2D::get_fill_color() const {

	return fill_color;
}

void Bezier2D::set_stroke_color(const Color &p_color) {

	stroke_color = p_color;
	update();
}

Color Bezier2D::get_stroke_color() const {

	return stroke_color;
}

void Bezier2D::set_stroke_width(float p_width) {

	stroke_width = p_width;
	_mark_dirty();
	update();
}

float Bezier2D::get_stroke_width() const {

	return stroke_width;
}

void Bezier2D::set_fill_rule(const FillRule &p_fill_rule) {

	if (fill_rule != p_fill_rule) {
		fill_rule = p_fill_rule;
		_mark_dirty();
		update();
	}
}

Bezier2D::FillRule Bezier2D::get_fill_rule() const {

	return fill_rule;
}

void Bezier2D::set_antialiased(bool p_antialiased) {

	antialiased = p_antialiased;
	update();
}

bool Bezier2D::get_antialiased() const {

	return antialiased;
}

void Bezier2D::set_offset(const Vector2 &p_offset) {

	offset = p_offset;
	update();
	_change_notify("offset");
}

Vector2 Bezier2D::get_offset() const {

	return offset;
}

void Bezier2D::_draw_polygons(const Tesselator2D::Polygons &p_polygons, const Color &p_color) const {

	if (p_polygons.size() < 1) {
		return;
	}

	Vector<Vector2> points;
	Vector<Vector2> uvs;

	Vector<int> indices;
	for (int i = 0; i < p_polygons.size(); i++) {

		const int n = p_polygons[i].size();
		const int p0 = points.size();
		points.resize(p0 + n);

		for (int j = 0; j < n; j++) {
			points[p0 + j] = p_polygons[i][j] + offset;
		}

		Vector<int> sub_indices = Geometry::triangulate_polygon(p_polygons[i]);

		const int from = indices.size();
		indices.resize(from + sub_indices.size());
		for (int j = 0; j < sub_indices.size(); j++) {
			indices[from + j] = sub_indices[j] + p0;
		}
	}

	if (indices.empty()) {
		return;
	}

	Vector<Color> colors;
	colors.resize(points.size());
	for (int i = 0; i < points.size(); i++) {
		colors[i] = p_color;
	}

	VS::get_singleton()->canvas_item_add_triangle_array(get_canvas_item(), indices, points, colors, uvs, RID());
}

void Bezier2D::_notification(int p_what) {

	switch (p_what) {

		case NOTIFICATION_DRAW: {

			Tesselator2D *tesselator = _get_tesselator();
			ERR_FAIL_COND(!tesselator);

			Tesselator2D::Tesselation tesselation;
			tesselator->get_tesselation(tesselator->get_path_to(this), tesselation);

			_draw_polygons(tesselation.fill, get_fill_color());
			_draw_polygons(tesselation.stroke, get_stroke_color());

		} break;

		case NOTIFICATION_ENTER_TREE: {

			Tesselator2D *tesselator = _get_tesselator();
			if (tesselator) {
				tesselator->register_shape(tesselator->get_path_to(this));
			}

		} break;
	}
}

void Bezier2D::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_fill_color", "color"), &Bezier2D::set_fill_color);
	ClassDB::bind_method(D_METHOD("get_fill_color"), &Bezier2D::get_fill_color);

	ClassDB::bind_method(D_METHOD("set_stroke_color", "color"), &Bezier2D::set_stroke_color);
	ClassDB::bind_method(D_METHOD("get_stroke_color"), &Bezier2D::get_stroke_color);

	ClassDB::bind_method(D_METHOD("set_stroke_width", "width"), &Bezier2D::set_stroke_width);
	ClassDB::bind_method(D_METHOD("get_stroke_width"), &Bezier2D::get_stroke_width);

	ClassDB::bind_method(D_METHOD("set_antialiased", "antialiased"), &Bezier2D::set_antialiased);
	ClassDB::bind_method(D_METHOD("get_antialiased"), &Bezier2D::get_antialiased);

	ClassDB::bind_method(D_METHOD("set_offset", "offset"), &Bezier2D::set_offset);
	ClassDB::bind_method(D_METHOD("get_offset"), &Bezier2D::get_offset);

	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "fill_color"), "set_fill_color", "get_fill_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "stroke_color"), "set_stroke_color", "get_stroke_color");
	ADD_PROPERTY(PropertyInfo(Variant::REAL, "stroke_width"), "set_stroke_width", "get_stroke_width");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset"), "set_offset", "get_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "antialiased"), "set_antialiased", "get_antialiased");
}

Bezier2D::Bezier2D() {

	fill_rule = FILLRULE_EVENODD;
	fill_color = Color(1, 1, 1);
	stroke_color = Color(0, 0, 0);
	stroke_width = 3.0;
	offset = Vector2(0, 0);
	antialiased = false;
}
