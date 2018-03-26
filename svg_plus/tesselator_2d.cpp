/*************************************************************************/
/*  tesselator_2d.cpp                                                    */
/*************************************************************************/

#include "tesselator_2d.h"
#include "bezier_2d.h"

struct HashMapHasherIntPoint {
	static _FORCE_INLINE_ uint32_t hash(const ClipperLib::IntPoint &p_p) {
		return hash_one_uint64(p_p.X ^ p_p.Y);
	}
};

struct HashMapComparatorIntPoint {
	static bool compare(const ClipperLib::IntPoint &p_lhs, const ClipperLib::IntPoint &p_rhs) {
		return p_lhs.X == p_rhs.X && p_lhs.Y == p_rhs.Y;
	}
};

class Points {
public:
	typedef ClipperLib::Path Path;
	typedef ClipperLib::Paths Paths;

private:
	typedef HashMap<ClipperLib::IntPoint, int, HashMapHasherIntPoint, HashMapComparatorIntPoint> PointHashMap;

	enum State {
		NEUTRAL,
		LOCKED,
		REMOVED
	};

	struct Point {
		ClipperLib::IntPoint p;
		State state;

		inline void lock() {
			state = LOCKED;
		}

		inline void remove() {
			ERR_FAIL_COND(state == LOCKED);
			state = REMOVED;
		}

		inline bool is_locked() const {
			return state == LOCKED;
		}

		inline bool is_removed() const {
			return state == REMOVED;
		}

		inline Vector2 to_vector2() const {
			return Vector2(p.X, p.Y);
		}
	};

	Vector<Point> points;
	PointHashMap index;

	void map(const Path &p_points, Vector<int> &r_index_path);
	int split(const Vector<int> &p_path, int p_begin, int p_end);
	void simplify(float p_detail, const Vector<int> &p_path, int p_begin, int p_end, Vector<int> &r_path);

public:
	void lock(const Path &p_path);
	void simplify(float p_detail, const Paths &p_paths, Paths &r_paths);
};

void Points::map(const Path &p_points, Vector<int> &r_index_path) {
	for (int i = 0; i < p_points.size(); i++) {
		const ClipperLib::IntPoint &p = p_points[i];
		int *k_ptr = index.getptr(p);
		if (k_ptr) {
			if (points[*k_ptr].is_removed()) {
				continue;
			}
			r_index_path.push_back(*k_ptr);
		} else {
			r_index_path.push_back(points.size());
			index[p] = points.size();
			Point point;
			point.p = p;
			point.state = NEUTRAL;
			points.push_back(point);
		}
	}
}

inline double distance_squared(const ClipperLib::IntPoint &p_a, const ClipperLib::IntPoint &p_b) {
	return (Vector2(p_a.X, p_a.Y) - Vector2(p_b.X, p_b.Y)).length_squared();
}

int Points::split(const Vector<int> &p_path, int p_begin, int p_end) {

	if (p_end - p_begin < 3) {
		return 0;
	}

	const Point &p0 = points[p_path[p_begin]];

	int max_i = 0;
	float max_d = 0.0;

	for (int i = p_begin + 1; i < p_end; i++) {
		const Point &pi = points[p_path[i]];
		if (pi.is_removed()) {
			continue;
		}
		float d = distance_squared(pi.p, p0.p);
		if (d > max_d) {
			max_i = i;
			max_d = d;
		}
	}

	return max_i;
}

void Points::simplify(float p_detail, const Vector<int> &p_path, int p_begin, int p_end, Vector<int> &r_result) {

	if (p_end - p_begin < 3) {
		for (int i = p_begin; i < p_end; i++) {
			Point &p = points[p_path[i]];
			if (p.is_removed()) {
				continue;
			} else {
				p.lock();
			}
			r_result.push_back(p_path[i]);
		}
		return;
	}

	int n = p_end - 1;

	Vector2 p0 = points[p_path[p_begin]].to_vector2();
	Vector2 pn = points[p_path[n]].to_vector2();

	while ((pn - p0).length_squared() < p_detail * p_detail) {
		if (n - p_begin <= 2) {

			int k = (p_begin + p_end) / 2;
			int c = 0;
			bool ok = false;
			while (k - c >= p_begin || k + c < p_end) {
				if (k - c >= p_begin && !points[p_path[k - c]].is_removed()) {
					k -= c;
					ok = true;
					break;
				}
				if (k + c < p_end && !points[p_path[k + c]].is_removed()) {
					k += c;
					ok = true;
					break;
				}
				c++;
			}
			if (ok) {
				points[p_path[k]].lock();
			}

			for (int i = p_begin; i < p_end; i++) {
				if (points[p_path[i]].is_locked()) {
					r_result.push_back(p_path[i]);
				} else {
					points[p_path[i]].remove();
				}
			}

			return;
		}

		n -= 1;
		pn = points[p_path[n]].to_vector2();
	}

	const Vector2 dir = (pn - p0).normalized();

	float max_d = 0.0;
	int max_i = -1;

	float split_d = 0.0;
	float split_i = -1;

	float last_t;
	float last_t_sign;

	for (int i = p_begin + 1; i < n; i++) {

		const Point &pp = points[p_path[i]];
		const Vector2 p = pp.to_vector2();

		float t = dir.dot(p - p0);

		float d = ((p0 + dir * t) - p).length_squared();
		if (!pp.is_locked()) {
			if (d > max_d) {
				max_d = d;
				max_i = i;
			}
		}
		if (!pp.is_removed()) {
			if (d > split_d) {
				split_d = d;
				split_i = i;
			}
		}
	}

	if (max_i >= 0 && max_d < p_detail * p_detail) {
		points[p_path[max_i]].remove();
		simplify(p_detail, p_path, p_begin, max_i, r_result);
		simplify(p_detail, p_path, max_i + 1, p_end, r_result);
	} else if (split_i >= 0) {
		points[p_path[split_i]].lock();
		simplify(p_detail, p_path, p_begin, split_i, r_result);
		r_result.push_back(p_path[split_i]);
		simplify(p_detail, p_path, split_i + 1, p_end, r_result);
	} else {
		for (int i = p_begin; i < p_end; i++) {
			Point &p = points[p_path[i]];
			if (p.is_removed()) {
				continue;
			} else {
				p.lock();
			}
			r_result.push_back(p_path[i]);
		}
	}
}

void Points::simplify(float p_detail, const Paths &p_paths, Paths &r_paths) {
	r_paths.clear();

	for (int i = 0; i < p_paths.size(); i++) {
		Vector<int> indices;
		map(p_paths[i], indices);

		if (indices.size() < 3) {
			continue;
		}

		Vector<int> s_indices;
		const int s = split(indices, 0, indices.size());
		simplify(p_detail, indices, 0, s, s_indices);
		simplify(p_detail, indices, s, indices.size(), s_indices);

		if (s_indices.size() < 3) {
			continue;
		}

		Path s_path;
		s_path.resize(s_indices.size());
		for (int j = 0; j < s_indices.size(); j++) {
			s_path[j] = points[s_indices[j]].p;
		}
		r_paths.push_back(s_path);
	}
}

void Points::lock(const Path &p_path) {
	for (int i = 0; i < p_path.size(); i++) {
		if (index.has(p_path[i])) {
			continue;
		}

		index[p_path[i]] = points.size();
		Point point;
		point.p = p_path[i];
		point.state = LOCKED;
		points.push_back(point);
	}
}

static void remove_holes(float p_scale, const Tesselator2D::IntPolygons &p_polygons, Tesselator2D::Polygons &r_polygons) {

	List<TriangulatorPoly> polys;
	for (int i = 0; i < p_polygons.size(); i++) {
		const int n = p_polygons[i].size();
		TriangulatorPoly poly;
		poly.Init(n);
		for (int j = 0; j < n; j++) {
			const ClipperLib::IntPoint &p = p_polygons[i][j];
			poly[j] = Vector2(p.X, p.Y) / p_scale;
		}
		if (poly.GetOrientation() == TRIANGULATOR_CW) {
			poly.SetHole(true);
		}
		polys.push_back(poly);
	}

	TriangulatorPartition partition;
	List<TriangulatorPoly> h_polys;
	partition.RemoveHoles(&polys, &h_polys);

	r_polygons.clear();
	for (int i = 0; i < h_polys.size(); i++) {
		const int n = h_polys[i].GetNumPoints();
		if (n >= 3) {
			Vector<Vector2> polygon;
			polygon.resize(n);
			for (int j = 0; j < n; j++) {
				polygon[j] = h_polys[i][j];
			}
			r_polygons.push_back(polygon);
		}
	}
}

void Tesselator2D::update_tesselation(Tesselation &r_tesselation, const IntPolygons &p_fill, const IntPolygons &p_stroke) const {

	remove_holes(parameters.scale, p_fill, r_tesselation.fill);
	remove_holes(parameters.scale, p_stroke, r_tesselation.stroke);

	if (r_tesselation.fill.empty()) {
		r_tesselation.bounds = Rect2();
	} else {
		Rect2 r(r_tesselation.fill[0][0], Vector2(0, 0));
		for (int i = 0; i < r_tesselation.fill.size(); i++) {
			for (int j = 0; j < r_tesselation.fill[i].size(); j++) {
				r.expand_to(r_tesselation.fill[i][j]);
			}
		}
		for (int i = 0; i < r_tesselation.stroke.size(); i++) {
			for (int j = 0; j < r_tesselation.stroke[i].size(); j++) {
				r.expand_to(r_tesselation.stroke[i][j]);
			}
		}
		r_tesselation.bounds = r;
	}
}

float Tesselator2D::get_detail() const {

	const float quality = 2.0 * parameters.quality / 100.0;
	const float detail = parameters.scale * 1.0 / quality;
	return detail;
}

void Tesselator2D::update_record(Cache *p_record) {

	Bezier2D *shape = Object::cast_to<Bezier2D>(get_node(p_record->path));
	ERR_FAIL_COND(!shape);

	shape->_tesselate_fill(parameters, p_record->base);
	Points points;
	IntPolygons fill;
	points.simplify(get_detail(), p_record->base, fill);

	Points stroke_points;
	IntPolygons stroke;
	shape->_tesselate_stroke(parameters, fill, stroke);
	IntPolygons stroke_simple;
	stroke_points.simplify(get_detail(), stroke, stroke_simple);

	update_tesselation(p_record->tesselation, fill, stroke_simple);

	p_record->valid = true;
}

void Tesselator2D::compute_meld() {

	if (!parameters.meld) {
		return;
	}

	const NodePath *path = cache.next(NULL);
	while (path) {
		Cache *record = cache.getptr(*path);
		Bezier2D *shape = Object::cast_to<Bezier2D>(get_node(record->path));
		ERR_FAIL_COND(!shape);
		if (!record->valid) {
			shape->_tesselate_fill(parameters, record->base);
		}

		path = cache.next(path);
	}

	Points points;

	path = cache.next(NULL);
	while (path) {
		Cache *record = cache.getptr(*path);
		Bezier2D *shape = Object::cast_to<Bezier2D>(get_node(record->path));
		ERR_FAIL_COND(!shape);

		ClipperLib::Path locked;
		shape->_tesselate_lock(parameters, locked);
		points.lock(locked);

		path = cache.next(path);
	}

	const int n = get_child_count();

	for (int i = n - 1; i >= 0; i--) { // inverse order is an advantage for melding

		Node *child = get_child(i);
		Bezier2D *shape = Object::cast_to<Bezier2D>(child);
		if (!shape) {
			continue;
		}

		Cache *record = cache.getptr(get_path_to(child));
		ERR_FAIL_COND(!record);

		IntPolygons fill;
		points.simplify(get_detail(), record->base, fill);
		IntPolygons stroke;
		shape->_tesselate_stroke(parameters, fill, stroke);
		IntPolygons stroke_simple;
		points.simplify(get_detail(), stroke, stroke_simple);

		update_tesselation(record->tesselation, fill, stroke);

		record->valid = true;
	}
}

void Tesselator2D::_refresh() {

	meld_dirty = true;

	const NodePath *path = cache.next(NULL);
	while (path) {
		cache.getptr(*path)->valid = false;
		path = cache.next(path);
	}

	propagate_call("update", Array(), false);
}

void Tesselator2D::register_shape(const NodePath &p_path) {

	Cache record;
	record.path = p_path;
	record.valid = false;
	cache[p_path] = record;
	meld_dirty = true;
}

void Tesselator2D::deregister_shape(const NodePath &p_path) {

	cache.erase(p_path);
	meld_dirty = true;
}

void Tesselator2D::mark_dirty(const NodePath &p_path) {

	Cache *record = cache.getptr(p_path);
	ERR_FAIL_COND(!record);
	record->valid = false;
	meld_dirty = true;
}

void Tesselator2D::get_tesselation(const NodePath &p_path, Tesselation &r_tesselation) {

	if (meld_dirty) {
		compute_meld();
		meld_dirty = false;
	}

	Cache *record = cache.getptr(p_path);
	ERR_FAIL_COND(!record);
	if (!record->valid) {
		update_record(record);
	}

	r_tesselation = record->tesselation;
}

void Tesselator2D::set_quality(float p_quality) {

	if (p_quality != parameters.quality) {

		parameters.quality = p_quality;
		_refresh();
	}
}

float Tesselator2D::get_quality() const {

	return parameters.quality;
}

void Tesselator2D::set_meld(bool p_meld) {

	if (p_meld != parameters.meld) {

		parameters.meld = p_meld;
		_refresh();
	}
}

bool Tesselator2D::get_meld() const {

	return parameters.meld;
}

void Tesselator2D::_bind_methods() {

	ClassDB::bind_method(D_METHOD("set_quality", "quality"), &Tesselator2D::set_quality);
	ClassDB::bind_method(D_METHOD("get_quality"), &Tesselator2D::get_quality);

	ClassDB::bind_method(D_METHOD("set_meld", "meld"), &Tesselator2D::set_meld);
	ClassDB::bind_method(D_METHOD("get_meld"), &Tesselator2D::get_meld);

	ADD_PROPERTY(PropertyInfo(Variant::REAL, "quality", PROPERTY_HINT_RANGE, "0,100,0.1"), "set_quality", "get_quality");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "meld"), "set_meld", "get_meld");
}

Tesselator2D::Tesselator2D() {

	parameters.quality = 100;
	parameters.meld = true;
	parameters.scale = 10.0;
	parameters.tolerance = 0.1;
	parameters.max_levels = 10;
	meld_dirty = true;
}
