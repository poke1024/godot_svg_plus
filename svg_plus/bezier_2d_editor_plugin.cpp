/*************************************************************************/
/*  bezier_2d_editor_plugin.cpp   		                                 */
/*************************************************************************/

#include "bezier_2d_editor_plugin.h"

Node2D *Bezier2DEditor::_get_node() const {

	return node;
}

void Bezier2DEditor::_set_node(Node *p_polygon) {

	node = Object::cast_to<Bezier2D>(p_polygon);
}

int Bezier2DEditor::_get_polygon_count() const {

	return node->get_path_count();
}

Variant Bezier2DEditor::_get_polygon(int p_idx) const {

	Vector<Vector2> p = node->get_path_points(p_idx);
	Vector<Vector2> q;
	int i = 0;
	while (i < p.size()) {
		q.push_back(p[i]);
		i += 3;
	}
	return q;
}

void Bezier2DEditor::_set_polygon(int p_idx, const Variant &p_polygon) const {

	Vector<Vector2> p = node->get_path_points(p_idx);
	Vector<Vector2> q = p_polygon;
	int i = 0;
	int j = 0;
	while (i < p.size() && j < q.size()) {
		p[i] = q[j++];
		i += 3;
	}
	node->set_path_points(p_idx, p);
}

Bezier2DEditor::Bezier2DEditor(EditorNode *p_editor) :
		AbstractPolygon2DEditor(p_editor) {
}

Bezier2DEditorPlugin::Bezier2DEditorPlugin(EditorNode *p_node) :
		AbstractPolygon2DEditorPlugin(p_node, memnew(Bezier2DEditor(p_node)), "Bezier2D") {
}
