/*************************************************************************/
/*  bezier_2d.h                                                          */
/*************************************************************************/

#ifndef BEZIER_2D_H
#define BEZIER_2D_H

#include "scene/2d/node_2d.h"
#include "tesselator_2d.h"
#include "thirdparty/misc/clipper.hpp"
#include "thirdparty/misc/triangulator.h"

class Bezier2D : public Node2D {

	GDCLASS(Bezier2D, Node2D);

public:
	enum FillRule {
		FILLRULE_NONZERO,
		FILLRULE_EVENODD
	};

private:
	struct Path {
		Vector<Vector2> points;
		bool closed;
	};

	Vector<Path> paths;

	FillRule fill_rule;
	Color fill_color;

	Color stroke_color;
	float stroke_width;

	bool antialiased;
	Vector2 offset;

	Tesselator2D *_get_tesselator() const;
	void _mark_dirty();

	void _draw_polygons(const Tesselator2D::Polygons &p_polygons, const Color &p_color) const;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void _tesselate_lock(
			const Tesselator2D::TesselationParameters &p_parameters,
			ClipperLib::Path &r_points);
	void _tesselate_fill(
			const Tesselator2D::TesselationParameters &p_parameters,
			ClipperLib::Paths &r_paths);
	void _tesselate_stroke(
			const Tesselator2D::TesselationParameters &p_parameters,
			const ClipperLib::Paths &p_fill,
			ClipperLib::Paths &r_paths);

	virtual Dictionary _edit_get_state() const;
	virtual void _edit_set_state(const Dictionary &p_state);

	virtual void _edit_set_pivot(const Point2 &p_pivot);
	virtual Point2 _edit_get_pivot() const;
	virtual bool _edit_use_pivot() const;
	virtual Rect2 _edit_get_rect() const;

	virtual bool _edit_is_selected_on_click(const Point2 &p_point, double p_tolerance) const;

	int get_path_count() const;
	Vector<Vector2> get_path_points(int p_path) const;
	void set_path_points(int p_path, const Vector<Vector2> &p_points);
	void add_path(const Vector<Vector2> &p_points);

	void set_fill_rule(const FillRule &p_fill_rule);
	FillRule get_fill_rule() const;

	void set_fill_color(const Color &p_color);
	Color get_fill_color() const;

	void set_stroke_color(const Color &p_color);
	Color get_stroke_color() const;

	void set_stroke_width(float p_width);
	float get_stroke_width() const;

	void set_antialiased(bool p_antialiased);
	bool get_antialiased() const;

	void set_offset(const Vector2 &p_offset);
	Vector2 get_offset() const;

	Bezier2D();
};

#endif // BEZIER_2D_H
