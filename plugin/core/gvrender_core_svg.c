/* $Id$ $Revision$ */
/* vim:set shiftwidth=4 ts=8: */

/**********************************************************
*      This software is part of the graphviz package      *
*                http://www.graphviz.org/                 *
*                                                         *
*            Copyright (c) 1994-2004 AT&T Corp.           *
*                and is licensed under the                *
*            Common Public License, Version 1.0           *
*                      by AT&T Corp.                      *
*                                                         *
*        Information and Software Systems Research        *
*              AT&T Research, Florham Park NJ             *
**********************************************************/

/* Comments on the SVG coordinate system (SN 8 Dec 2006):
   The initial <svg> element defines the SVG coordinate system so
   that the graphviz canvas (in units of points) fits the intended
   absolute size in inches.  After this, the situation should be
   that "px" = "pt" in SVG, so we can dispense with stating units.
   Also, the input units (such as fontsize) should be preserved
   without scaling in the output SVG (as long as the graph size
   was not constrained.)
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "macros.h"
#include "const.h"

#include "gvplugin_render.h"
#include "gvcint.h"
#include "graph.h"
#include "types.h"		/* need the SVG font name schemes */

typedef enum { FORMAT_SVG, FORMAT_SVGZ, } format_type;

extern char *xml_string(char *str);
extern void core_init_compression(GVJ_t * job, compression_t compression);
extern void core_fini_compression(GVJ_t * job);
extern void core_fputs(GVJ_t * job, char *s);
extern void core_printf(GVJ_t * job, const char *format, ...);

/* SVG dash array */
static char *sdarray = "5,2";
/* SVG dot array */
static char *sdotarray = "1,5";

static void svg_bzptarray(GVJ_t * job, pointf * A, int n)
{
    int i;
    char c;

    c = 'M';			/* first point */
    for (i = 0; i < n; i++) {
	core_printf(job, "%c%g,%g", c, A[i].x, -A[i].y);
	if (i == 0)
	    c = 'C';		/* second point */
	else
	    c = ' ';		/* remaining points */
    }
}

static void svg_print_color(GVJ_t * job, gvcolor_t color)
{
    switch (color.type) {
    case COLOR_STRING:
	core_fputs(job, color.u.string);
	break;
    case RGBA_BYTE:
	if (color.u.rgba[3] == 0) /* transparent */
	    core_fputs(job, "none");
	else
	    core_printf(job, "#%02x%02x%02x",
		color.u.rgba[0], color.u.rgba[1], color.u.rgba[2]);
	break;
    default:
	assert(0);		/* internal error */
    }
}

static void svg_grstyle(GVJ_t * job, int filled)
{
    obj_state_t *obj = job->obj;

    core_fputs(job, " style=\"fill:");
    if (filled)
	svg_print_color(job, obj->fillcolor);
    else
	core_fputs(job, "none");
    core_fputs(job, ";stroke:");
    svg_print_color(job, obj->pencolor);
    if (obj->penwidth != PENWIDTH_NORMAL)
	core_printf(job, ";stroke-width:%g", obj->penwidth);
    if (obj->pen == PEN_DASHED) {
	core_printf(job, ";stroke-dasharray:%s", sdarray);
    } else if (obj->pen == PEN_DOTTED) {
	core_printf(job, ";stroke-dasharray:%s", sdotarray);
    }
    core_fputs(job, ";\"");
}

static void svg_comment(GVJ_t * job, char *str)
{
    core_fputs(job, "<!-- ");
    core_fputs(job, xml_string(str));
    core_fputs(job, " -->\n");
}

static void svg_begin_job(GVJ_t * job)
{
    char *s;

    switch (job->render.id) {
    case FORMAT_SVGZ:
	core_init_compression(job, COMPRESSION_ZLIB);
	break;
    case FORMAT_SVG:
	core_init_compression(job, COMPRESSION_NONE);
	break;
    }

    core_fputs(job, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n");
    if ((s = agget(job->gvc->g, "stylesheet")) && s[0]) {
        core_fputs(job, "<?xml-stylesheet href=\"");
        core_fputs(job, s);
        core_fputs(job, "\" type=\"text/css\"?>\n");
    }
    core_fputs(job, "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.0//EN\"\n");
    core_fputs(job, " \"http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd\"");

    /* This is to work around a bug in the SVG 1.0 DTD */
    core_fputs(job, " [\n <!ATTLIST svg xmlns:xlink CDATA #FIXED \"http://www.w3.org/1999/xlink\">\n]");

    core_fputs(job, ">\n<!-- Generated by ");
    core_fputs(job, xml_string(job->common->info[0]));
    core_fputs(job, " version ");
    core_fputs(job, xml_string(job->common->info[1]));
    core_fputs(job, " (");
    core_fputs(job, xml_string(job->common->info[2]));
    core_fputs(job, ")\n     For user: ");
    core_fputs(job, xml_string(job->common->user));
    core_fputs(job, " -->\n");
}

static void svg_begin_graph(GVJ_t * job)
{
    obj_state_t *obj = job->obj;

    core_fputs(job, "<!--");
    if (obj->u.g->name[0]) {
        core_fputs(job, " Title: ");
	core_fputs(job, xml_string(obj->u.g->name));
    }
    core_printf(job, " Pages: %d -->\n", job->pagesArraySize.x * job->pagesArraySize.y);

    core_printf(job, "<svg width=\"%dpt\" height=\"%dpt\"\n",
	job->width, job->height);
    core_printf(job, " viewBox=\"%.2f %.2f %.2f %.2f\"",
        job->canvasBox.LL.x, job->canvasBox.LL.y,
        job->canvasBox.UR.x, job->canvasBox.UR.y);
    /* namespace of svg */
    core_fputs(job, " xmlns=\"http://www.w3.org/2000/svg\"");
    /* namespace of xlink */
    core_fputs(job, " xmlns:xlink=\"http://www.w3.org/1999/xlink\"");
    core_fputs(job, ">\n");
}

static void svg_end_graph(GVJ_t * job)
{
    core_fputs(job, "</svg>\n");
    core_fini_compression(job);
}

static void svg_begin_layer(GVJ_t * job, char *layername, int layerNum, int numLayers)
{
    core_fputs(job, "<g id=\"");
    core_fputs(job, xml_string(layername));
    core_fputs(job, "\" class=\"layer\">\n");
}

static void svg_end_layer(GVJ_t * job)
{
    core_fputs(job, "</g>\n");
}

static void svg_begin_page(GVJ_t * job)
{
    obj_state_t *obj = job->obj;

    /* its really just a page of the graph, but its still a graph,
     * and it is the entire graph if we're not currently paging */
    core_printf(job, "<g id=\"graph%d\" class=\"graph\"", job->common->viewNum);
    core_printf(job, " transform=\"scale(%g %g) rotate(%d) translate(%g %g)\">\n",
	    job->scale.x, job->scale.y, -job->rotation,
	    job->translation.x, -job->translation.y);
    /* default style */
    if (obj->u.g->name[0]) {
        core_fputs(job, "<title>");
        core_fputs(job, xml_string(obj->u.g->name));
        core_fputs(job, "</title>\n");
    }
}

static void svg_end_page(GVJ_t * job)
{
    core_fputs(job, "</g>\n");
}

static void svg_begin_cluster(GVJ_t * job)
{
    obj_state_t *obj = job->obj;

    core_printf(job, "<g id=\"cluster%ld\" class=\"cluster\">",
	    obj->u.sg->meta_node->id);
    core_fputs(job, "<title>");
    core_fputs(job, xml_string(obj->u.sg->name));
    core_fputs(job, "</title>\n");
}

static void svg_end_cluster(GVJ_t * job)
{
    core_fputs(job, "</g>\n");
}

static void svg_begin_node(GVJ_t * job)
{
    obj_state_t *obj = job->obj;

    core_printf(job, "<g id=\"node%ld\" class=\"node\">", obj->u.n->id);
    core_fputs(job, "<title>");
    core_fputs(job, xml_string(obj->u.n->name));
    core_fputs(job, "</title>\n");
}

static void svg_end_node(GVJ_t * job)
{
    core_fputs(job, "</g>\n");
}

static void
svg_begin_edge(GVJ_t * job)
{
    obj_state_t *obj = job->obj;
    char *edgeop;

    core_printf(job, "<g id=\"edge%ld\" class=\"edge\">", obj->u.e->id);
    if (obj->u.e->tail->graph->root->kind & AGFLAG_DIRECTED)
	edgeop = "&#45;&gt;";
    else
	edgeop = "&#45;&#45;";
    core_fputs(job, "<title>");
    core_fputs(job, xml_string(obj->u.e->tail->name));
    core_fputs(job, edgeop);
    /* can't do this in single core_printf because
     * xml_string's buffer gets reused. */
    core_fputs(job, xml_string(obj->u.e->head->name));
    core_fputs(job, "</title>\n");
}

static void svg_end_edge(GVJ_t * job)
{
    core_fputs(job, "</g>\n");
}

static void
svg_begin_anchor(GVJ_t * job, char *href, char *tooltip, char *target)
{
    core_fputs(job, "<a");
    if (href && href[0])
	core_printf(job, " xlink:href=\"%s\"", xml_string(href));
    if (tooltip && tooltip[0])
	core_printf(job, " xlink:title=\"%s\"", xml_string(tooltip));
    if (target && target[0])
	core_printf(job, " target=\"%s\"", xml_string(target));
    core_fputs(job, ">\n");
}

static void svg_end_anchor(GVJ_t * job)
{
    core_fputs(job, "</a>\n");
}

static void svg_textpara(GVJ_t * job, pointf p, textpara_t * para)
{
    obj_state_t *obj = job->obj;
    PostscriptAlias *pA;

    core_fputs(job, "<text");
    switch (para->just) {
    case 'l':
	core_fputs(job, " text-anchor=\"start\"");
	break;
    case 'r':
	core_fputs(job, " text-anchor=\"end\"");
	break;
    default:
    case 'n':
	core_fputs(job, " text-anchor=\"middle\"");
	break;
    }
    core_printf(job, " x=\"%g\" y=\"%g\"", p.x, -p.y);
    core_fputs(job, " style=\"");
    pA = para->postscript_alias;
    if (pA) {
	char *family=NULL, *weight=NULL, *stretch=NULL, *style=NULL;
	switch(GD_fontnames(job->gvc->g)) {
		case PSFONTS:
		    family = pA->name;
		    weight = pA->weight;
		    style = pA->style;
		    break;
		case NATIVEFONTS:
		case SVGFONTS: /* same as NATIVEFONTS - jce */
		default:
		    family = pA->family;
		    weight = pA->weight;
		    style = pA->style;
		    break;
	}
	stretch = pA->stretch;

        core_printf(job, "font-family:%s;", family);
        if (weight) core_printf(job, "font-weight:%s;", weight);
        if (stretch) core_printf(job, "font-stretch:%s;", stretch);
        if (style) core_printf(job, "font-style:%s;", style);
    }
    else
	core_printf(job, "font-family:%s;", para->fontname);
    core_printf(job, "font-size:%.2f;", para->fontsize);
    switch (obj->pencolor.type) {
    case COLOR_STRING:
	if (strcasecmp(obj->pencolor.u.string, "black"))
	    core_printf(job, "fill:%s;", obj->pencolor.u.string);
	break;
    case RGBA_BYTE:
	core_printf(job, "fill:#%02x%02x%02x;",
		obj->pencolor.u.rgba[0], obj->pencolor.u.rgba[1], obj->pencolor.u.rgba[2]);
	break;
    default:
	assert(0);		/* internal error */
    }
    core_fputs(job, "\">");
    core_fputs(job, xml_string(para->str));
    core_fputs(job, "</text>\n");
}

static void svg_ellipse(GVJ_t * job, pointf * A, int filled)
{
    /* A[] contains 2 points: the center and corner. */
    core_fputs(job, "<ellipse");
    svg_grstyle(job, filled);
    core_printf(job, " cx=\"%g\" cy=\"%g\"", A[0].x, -A[0].y);
    core_printf(job, " rx=\"%g\" ry=\"%g\"",
	    A[1].x - A[0].x, A[1].y - A[0].y);
    core_fputs(job, "/>\n");
}

static void
svg_bezier(GVJ_t * job, pointf * A, int n, int arrow_at_start,
	      int arrow_at_end, int filled)
{
    core_fputs(job, "<path");
    svg_grstyle(job, filled);
    core_fputs(job, " d=\"");
    svg_bzptarray(job, A, n);
    core_fputs(job, "\"/>\n");
}

static void svg_polygon(GVJ_t * job, pointf * A, int n, int filled)
{
    int i;

    core_fputs(job, "<polygon");
    svg_grstyle(job, filled);
    core_fputs(job, " points=\"");
    for (i = 0; i < n; i++)
	core_printf(job, "%g,%g ", A[i].x, -A[i].y);
    core_printf(job, "%g,%g", A[0].x, -A[0].y);	/* because Adobe SVG is broken */
    core_fputs(job, "\"/>\n");
}

static void svg_polyline(GVJ_t * job, pointf * A, int n)
{
    int i;

    core_fputs(job, "<polyline");
    svg_grstyle(job, 0);
    core_fputs(job, " points=\"");
    for (i = 0; i < n; i++)
	core_printf(job, "%g,%g ", A[i].x, -A[i].y);
    core_fputs(job, "\"/>\n");
}

/* color names from http://www.w3.org/TR/SVG/types.html */
/* NB.  List must be LANG_C sorted */
static char *svg_knowncolors[] = {
    "aliceblue", "antiquewhite", "aqua", "aquamarine", "azure",
    "beige", "bisque", "black", "blanchedalmond", "blue",
    "blueviolet", "brown", "burlywood",
    "cadetblue", "chartreuse", "chocolate", "coral",
    "cornflowerblue", "cornsilk", "crimson", "cyan",
    "darkblue", "darkcyan", "darkgoldenrod", "darkgray",
    "darkgreen", "darkgrey", "darkkhaki", "darkmagenta",
    "darkolivegreen", "darkorange", "darkorchid", "darkred",
    "darksalmon", "darkseagreen", "darkslateblue", "darkslategray",
    "darkslategrey", "darkturquoise", "darkviolet", "deeppink",
    "deepskyblue", "dimgray", "dimgrey", "dodgerblue",
    "firebrick", "floralwhite", "forestgreen", "fuchsia",
    "gainsboro", "ghostwhite", "gold", "goldenrod", "gray",
    "green", "greenyellow", "grey",
    "honeydew", "hotpink", "indianred",
    "indigo", "ivory", "khaki",
    "lavender", "lavenderblush", "lawngreen", "lemonchiffon",
    "lightblue", "lightcoral", "lightcyan", "lightgoldenrodyellow",
    "lightgray", "lightgreen", "lightgrey", "lightpink",
    "lightsalmon", "lightseagreen", "lightskyblue",
    "lightslategray", "lightslategrey", "lightsteelblue",
    "lightyellow", "lime", "limegreen", "linen",
    "magenta", "maroon", "mediumaquamarine", "mediumblue",
    "mediumorchid", "mediumpurple", "mediumseagreen",
    "mediumslateblue", "mediumspringgreen", "mediumturquoise",
    "mediumvioletred", "midnightblue", "mintcream",
    "mistyrose", "moccasin",
    "navajowhite", "navy", "oldlace",
    "olive", "olivedrab", "orange", "orangered", "orchid",
    "palegoldenrod", "palegreen", "paleturquoise",
    "palevioletred", "papayawhip", "peachpuff", "peru", "pink",
    "plum", "powderblue", "purple",
    "red", "rosybrown", "royalblue",
    "saddlebrown", "salmon", "sandybrown", "seagreen", "seashell",
    "sienna", "silver", "skyblue", "slateblue", "slategray",
    "slategrey", "snow", "springgreen", "steelblue",
    "tan", "teal", "thistle", "tomato", "turquoise",
    "violet",
    "wheat", "white", "whitesmoke",
    "yellow", "yellowgreen"
};

gvrender_engine_t svg_engine = {
    svg_begin_job,
    0,				/* svg_end_job */
    svg_begin_graph,
    svg_end_graph,
    svg_begin_layer,
    svg_end_layer,
    svg_begin_page,
    svg_end_page,
    svg_begin_cluster,
    svg_end_cluster,
    0,				/* svg_begin_nodes */
    0,				/* svg_end_nodes */
    0,				/* svg_begin_edges */
    0,				/* svg_end_edges */
    svg_begin_node,
    svg_end_node,
    svg_begin_edge,
    svg_end_edge,
    svg_begin_anchor,
    svg_end_anchor,
    svg_textpara,
    0,				/* svg_resolve_color */
    svg_ellipse,
    svg_polygon,
    svg_bezier,
    svg_polyline,
    svg_comment,
    0,				/* svg_library_shape */
};

gvrender_features_t render_features_svg = {
    GVRENDER_Y_GOES_DOWN
        | GVRENDER_DOES_TRANSFORM
	| GVRENDER_DOES_LABELS
	| GVRENDER_DOES_MAPS
	| GVRENDER_DOES_TARGETS
	| GVRENDER_DOES_TOOLTIPS, /* flags */
    4.,                         /* default pad - graph units */
    svg_knowncolors,		/* knowncolors */
    sizeof(svg_knowncolors) / sizeof(char *),	/* sizeof knowncolors */
    RGBA_BYTE,			/* color_type */
    "svg",                      /* imageloader for usershapes */
};

gvdevice_features_t device_features_svg = {
    GVDEVICE_DOES_TRUECOLOR,	/* flags */
    {0.,0.},			/* default margin - points */
    {0.,0.},                    /* default page width, height - points */
    {72.,72.},			/* default dpi */
};

gvdevice_features_t device_features_svgz = {
    GVDEVICE_BINARY_FORMAT
      | GVDEVICE_COMPRESSED_FORMAT
      | GVDEVICE_DOES_TRUECOLOR,/* flags */
    {0.,0.},			/* default margin - points */
    {0.,0.},                    /* default page width, height - points */
    {72.,72.},			/* default dpi */
};

gvplugin_installed_t gvrender_svg_types[] = {
    {FORMAT_SVG, "svg", 1, &svg_engine, &render_features_svg},
    {0, NULL, 0, NULL, NULL}
};

gvplugin_installed_t gvdevice_svg_types[] = {
    {FORMAT_SVG, "svg:svg", 1, NULL, &device_features_svg},
#if HAVE_LIBZ
    {FORMAT_SVGZ, "svgz:svg", 1, NULL, &device_features_svgz},
#endif
    {0, NULL, 0, NULL, NULL}
};
