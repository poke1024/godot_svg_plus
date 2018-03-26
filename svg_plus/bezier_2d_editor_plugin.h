/*************************************************************************/
/*  bezier_2d_editor_plugin.h          			                         */
/*************************************************************************/

#ifndef BEZIER_2D_EDITOR_PLUGIN_H
#define BEZIER_2D_EDITOR_PLUGIN_H

#include "bezier_2d.h"
#include "editor/plugins/abstract_polygon_2d_editor.h"

class Bezier2DEditor : public AbstractPolygon2DEditor {

	GDCLASS(Bezier2DEditor, AbstractPolygon2DEditor);

	Bezier2D *node;

protected:
	virtual Node2D *_get_node() const;
	virtual void _set_node(Node *p_polygon);

	virtual int _get_polygon_count() const;
	virtual Variant _get_polygon(int p_idx) const;
	virtual void _set_polygon(int p_idx, const Variant &p_polygon) const;

public:
	Bezier2DEditor(EditorNode *p_editor);
};

class Bezier2DEditorPlugin : public AbstractPolygon2DEditorPlugin {

	GDCLASS(Bezier2DEditorPlugin, AbstractPolygon2DEditorPlugin);

public:
	Bezier2DEditorPlugin(EditorNode *p_node);
};

#endif // BEZIER_2D_EDITOR_PLUGIN_H
