/*************************************************************************/
/*  register_types.cpp                                                   */
/*************************************************************************/

#include "register_types.h"
#include "../svg_plus/svg.h"

static ResourceFormatLoaderSVG *svg_loader = NULL;

void register_svg_plus_types() {

	svg_loader = memnew(ResourceFormatLoaderSVG);
	ResourceLoader::add_resource_format_loader(svg_loader);
	ClassDB::register_class<SVG>();
	ClassDB::register_class<SVGMesh>();
}

void unregister_svg_plus_types() {

}
