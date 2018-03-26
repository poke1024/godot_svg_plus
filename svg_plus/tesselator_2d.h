/*************************************************************************/
/*  tesselator_2d.h                                                      */
/*************************************************************************/

#ifndef TESSELATOR_2D_H
#define TESSELATOR_2D_H

#include "scene/2d/node_2d.h"
#include "thirdparty/misc/clipper.hpp"

class Bezier2D;

class Tesselator2D : public Node2D {

	GDCLASS(Tesselator2D, Node2D);

public:
	typedef Vector<Vector<Vector2> > Polygons;
	typedef ClipperLib::Paths IntPolygons;

	struct TesselationParameters {
		float quality; // for final path simplification
		bool meld;
		float scale; // for path geometry computations
		float tolerance; // for bezier flattening
		int max_levels; // for bezier flattening
	};

	struct Tesselation {
		Polygons fill;
		Polygons stroke;
		Rect2 bounds;
	};

private:
	struct Cache {
		NodePath path;
		bool valid;
		IntPolygons base;
		Tesselation tesselation;
	};

	HashMap<NodePath, Cache> cache;
	TesselationParameters parameters;

	bool meld_dirty;

	float get_detail() const;
	void update_record(Cache *p_record);
	void update_tesselation(Tesselation &r_tesselation, const IntPolygons &p_fill, const IntPolygons &p_stroke) const;
	void compute_meld();
	void _refresh();

protected:
	static void _bind_methods();

public:
	void register_shape(const NodePath &p_path);
	void deregister_shape(const NodePath &p_path);
	void mark_dirty(const NodePath &p_path);

	void get_tesselation(const NodePath &p_path, Tesselation &r_tesselation);
	Rect2 get_edit_rect(const NodePath &p_path);

	void set_quality(float p_quality);
	float get_quality() const;

	void set_meld(bool p_meld);
	bool get_meld() const;

	Tesselator2D();
};

#endif // TESSELATOR_2D_H
