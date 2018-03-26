/*************************************************************************/
/*  register_types.cpp                                                   */
/*************************************************************************/

#include "register_types.h"
#include "bezier_2d.h"
#include "bezier_2d_editor_plugin.h"
#include "svg.h"

static ResourceFormatLoaderSVG *svg_loader = NULL;

void register_svg_plus_types() {

	svg_loader = memnew(ResourceFormatLoaderSVG);
	ResourceLoader::add_resource_format_loader(svg_loader);

	ClassDB::register_class<SVG>();
	ClassDB::register_class<SVGInstance>();
	ClassDB::register_class<Bezier2D>();

	/*#ifdef TOOLS_ENABLED
	EditorNode *editor = EditorNode::get_singleton();
	editor->add_editor_plugin(memnew(Bezier2DEditorPlugin(editor)));
#endif*/
}

void unregister_svg_plus_types() {
}
