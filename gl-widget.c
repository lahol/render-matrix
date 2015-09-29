#include "gl-widget.h"
#include <glib/gi18n.h>
#include <glib/gprintf.h>

#if GTK_CHECK_VERSION(3,16,0)
#endif

/*[begin < 3.16]*/
#include <gdk/gdkx.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
/*[end < 3.16]*/

struct _GlWidgetPrivate {
    /* private data */
    GraphicsHandle *graphics_handle;

/*[begin < 3.16]*/
    GLXContext glx_context;
    Display *display;
    Window window;
/*[end < 3.16]*/

    gint width;
    gint height;

    gdouble last_x;
    gdouble last_y;
};

/*[begin < 3.16]*/
G_DEFINE_TYPE(GlWidget, gl_widget, GTK_TYPE_DRAWING_AREA);
/*[end < 3.16]*/

enum {
    PROP_0,
    PROP_GRAPHICS_HANDLE,
    N_PROPERTIES
};

void gl_widget_set_graphics_handle(GlWidget *widget, GraphicsHandle *handle)
{
    g_return_if_fail(IS_GL_WIDGET(widget));
    widget->priv->graphics_handle = handle;
}

static void gl_widget_dispose(GObject *gobject)
{
    GlWidget *self = GL_WIDGET(gobject);

/*[begin < 3.16]*/
    if (self->priv->glx_context) {
        glXDestroyContext(self->priv->display, self->priv->glx_context);
        self->priv->glx_context = NULL;
    }
/*[end < 3.16]*/

    G_OBJECT_CLASS(gl_widget_parent_class)->dispose(gobject);
}

static void gl_widget_finalize(GObject *gobject)
{
    /*GlWidget *self = GL_WIDGET(gobject);*/

    G_OBJECT_CLASS(gl_widget_parent_class)->finalize(gobject);
}

static void gl_widget_set_property(GObject *object, guint prop_id, 
        const GValue *value, GParamSpec *spec)
{
    GlWidget *gl_widget = GL_WIDGET(object);

    switch (prop_id) {
        case PROP_GRAPHICS_HANDLE:
            gl_widget_set_graphics_handle(gl_widget, (GraphicsHandle *)g_value_get_pointer(value));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

static void gl_widget_get_property(GObject *object, guint prop_id,
        GValue *value, GParamSpec *spec)
{
    GlWidget *gl_widget = GL_WIDGET(object);

    switch (prop_id) {
    	case PROP_GRAPHICS_HANDLE:
            g_value_set_pointer(value, gl_widget->priv->graphics_handle);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, spec);
            break;
    }
}

/* Versions < 3.16 with GtkDrawingArea */
static gboolean gl_widget_configure_event(GtkWidget *widget, GdkEventConfigure *event)
{
    GtkAllocation alloc;
    gtk_widget_get_allocation(widget, &alloc);
    GlWidgetPrivate *priv = GL_WIDGET(widget)->priv;

    priv->width = alloc.width;
    priv->height = alloc.height;

    graphics_set_window_size(priv->graphics_handle, alloc.width, alloc.height);

    return TRUE;
}

static gboolean gl_widget_draw(GtkWidget *widget, cairo_t *cr)
{
    GlWidgetPrivate *priv = GL_WIDGET(widget)->priv;
/*[begin < 3.16]*/

    glXMakeCurrent(priv->display, priv->window, priv->glx_context);
/*[end < 3.16]*/

    graphics_render(GL_WIDGET(widget)->priv->graphics_handle, NULL, NULL);

/*[begin < 3.16]*/
    glXSwapBuffers(priv->display, priv->window);
/*[end < 3.16]*/

    return TRUE;
}

static void gl_widget_realize(GtkWidget *widget)
{
    GTK_WIDGET_CLASS(gl_widget_parent_class)->realize(widget);
/*[begin < 3.16]*/
    GdkWindow *gdkwin = gtk_widget_get_window(widget);
    GL_WIDGET(widget)->priv->window = GDK_WINDOW_XID(gdkwin);
    gdk_window_set_events(gdkwin,
                          gdk_window_get_events(gdkwin) |
                          GDK_BUTTON_PRESS_MASK |
                          GDK_BUTTON_RELEASE_MASK |
                          GDK_POINTER_MOTION_MASK |
                          GDK_SCROLL_MASK);
/*[end < 3.16]*/
}

static gboolean gl_widget_scroll_event(GtkWidget *widget, GdkEventScroll *event)
{
    if (event->direction == GDK_SCROLL_UP)
        graphics_camera_zoom(GL_WIDGET(widget)->priv->graphics_handle, 1);
    else if (event->direction == GDK_SCROLL_DOWN)
        graphics_camera_zoom(GL_WIDGET(widget)->priv->graphics_handle, -1);

    gtk_widget_queue_draw(widget);

    return FALSE;
}

static gboolean gl_widget_button_press_event(GtkWidget *widget, GdkEventButton *event)
{
    GlWidgetPrivate *priv = GL_WIDGET(widget)->priv;
    if (event->button == 3) {
        graphics_camera_move_start(priv->graphics_handle, event->x, event->y);
    }
    else if (event->button == 1) {
        graphics_camera_arcball_rotate_start(priv->graphics_handle, event->x, event->y);
    }
    return FALSE;
}

static gboolean gl_widget_button_release_event(GtkWidget *widget, GdkEventButton *event)
{
    GlWidgetPrivate *priv = GL_WIDGET(widget)->priv;
    if (event->button == 3) {
        graphics_camera_move_finish(priv->graphics_handle, event->x, event->y);
    }
    else if (event->button == 1) {
        ArcBallRestriction rst = ArcBallRestrictionNone;
        if (event->state & GDK_SHIFT_MASK)
            rst = ArcBallRestrictionVertical;
        else if (event->state & GDK_CONTROL_MASK)
            rst = ArcBallRestrictionHorizontal;
        graphics_camera_arcball_rotate_finish(priv->graphics_handle, event->x, event->y, rst);
    }

    gtk_widget_queue_draw(widget);
    return FALSE;
}

static gboolean gl_widget_motion_notify_event(GtkWidget *widget, GdkEventMotion *event)
{
    GlWidgetPrivate *priv = GL_WIDGET(widget)->priv;
    if (event->state & GDK_BUTTON3_MASK) {
        graphics_camera_move_update(priv->graphics_handle, event->x, event->y);
    }
    else if (event->state & GDK_BUTTON1_MASK) {
        ArcBallRestriction rst = ArcBallRestrictionNone;
        if (event->state & GDK_SHIFT_MASK)
            rst = ArcBallRestrictionVertical;
        else if (event->state & GDK_CONTROL_MASK)
            rst = ArcBallRestrictionHorizontal;
        graphics_camera_arcball_rotate_update(priv->graphics_handle, event->x, event->y, rst);
    }

    gtk_widget_queue_draw(widget);
    return FALSE;
}

static void gl_widget_class_init(GlWidgetClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS(klass);

    /* override GObject methods */
    gobject_class->dispose = gl_widget_dispose;
    gobject_class->finalize = gl_widget_finalize;
    gobject_class->set_property = gl_widget_set_property;
    gobject_class->get_property = gl_widget_get_property;

    gtkwidget_class->realize = gl_widget_realize;
    gtkwidget_class->configure_event = gl_widget_configure_event;
    gtkwidget_class->draw = gl_widget_draw;
    gtkwidget_class->scroll_event = gl_widget_scroll_event; /* button_press_event, key_press_event … */
    gtkwidget_class->button_press_event = gl_widget_button_press_event;
    gtkwidget_class->button_release_event = gl_widget_button_release_event;
    gtkwidget_class->motion_notify_event = gl_widget_motion_notify_event;

    g_object_class_install_property(gobject_class,
            PROP_GRAPHICS_HANDLE,
            g_param_spec_pointer("graphics-handle",
                "GraphicsHandle",
                "Graphics Handle",
                G_PARAM_READWRITE));

    g_type_class_add_private(G_OBJECT_CLASS(klass), sizeof(GlWidgetPrivate));
}

static void gl_widget_init_gl(GlWidget *self)
{
/*[begin < 3.16]*/
    XVisualInfo *vi = NULL;

    int attribs[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        GLX_DEPTH_SIZE, 24,
#ifdef WITH_MSAA
        GLX_SAMPLE_BUFFERS, 1,
        GLX_SAMPLES, 4,
#endif
        0
    };

    self->priv->display = gdk_x11_display_get_xdisplay(gdk_display_get_default());

    if (!glXQueryExtension(self->priv->display, NULL, NULL)) {
        g_printerr("OpenGL not supported by default display.\n");
        return;
    }

    vi = glXChooseVisual(self->priv->display,
            gdk_x11_screen_get_screen_number(gdk_display_get_default_screen(gdk_display_get_default())),
            attribs);

    if (!vi) {
        g_printerr("Could not configure OpenGL.\n");
        return;
    }

    self->priv->glx_context = glXCreateContext(self->priv->display, vi, NULL, True);

    XFree(vi);
/*[end < 3.16]*/
}

static void gl_widget_init(GlWidget *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self,
            GL_WIDGET_TYPE, GlWidgetPrivate);

    gtk_widget_set_has_window(GTK_WIDGET(self), TRUE);
/*[begin < 3.16]*/
    gtk_widget_set_double_buffered(GTK_WIDGET(self), FALSE);
/*[end < 3.16]*/

    gl_widget_init_gl(self);
}

GtkWidget *gl_widget_new(GraphicsHandle *handle)
{
    return g_object_new(GL_WIDGET_TYPE, "graphics-handle", handle, NULL);
}

/* FIXME: The following should be somewhere else. Saving to file is not the job of the widget implementation.
 * When we manage to write to a real offscreen buffer independent of the window this would be the place to go. */
struct TiksMark {
    double x;
    double y;
    guint8 align;
    gchar *text;
};

static void _gl_widget_free_tiks_mark(struct TiksMark *mark)
{
    if (mark) {
        g_free(mark->text);
        g_free(mark);
    }
}

static void _gl_widget_tiks_callback(double x, double y, guint8 align, gchar *text, GList **list)
{
    g_return_if_fail(list != NULL);
    g_print("tiks @(%f,%f): %s\n", x, y, text);
    struct TiksMark *mark = g_malloc0(sizeof(struct TiksMark));
    mark->x = x;
    mark->y = y;
    mark->align = align;
    mark->text = g_strdup(text);

    *list = g_list_prepend(*list, mark);
}

void _gl_widget_save_to_latex(const gchar *filename, UtilRectangle *render_area, GList *tiks)
{
    g_return_if_fail(filename != NULL);
    g_return_if_fail(render_area != NULL);
/*    gchar *directory = g_path_get_dirname(filename);*/
    gchar *basename = g_path_get_basename(filename);
    gchar *base = g_strdup(filename);
    gchar *lastdot = strrchr(base, '.');
    gchar *lastdirsep = strrchr(base, G_DIR_SEPARATOR);
    if (lastdot != NULL && lastdirsep < lastdot)
        lastdot[0] = '\0';


    gchar *texfile = g_strdup_printf("%s.tex", base);

    fprintf(stderr, "open %s\n", texfile);

    FILE *f;
    GList *tmp;
    struct TiksMark *mark;

    double size = render_area->width > render_area->height ? render_area->width : render_area->height;
   
    if ((f = fopen(texfile, "w")) == NULL)
        goto out;

    fprintf(f, "\\documentclass[border=1cm]{standalone}\n\\usepackage[percent]{overpic}\n\\begin{document}\n");
    fprintf(f, "\\begin{overpic}[width=\\textwidth]{%s}\n", basename);

    for (tmp = tiks; tmp != NULL; tmp = g_list_next(tmp)) {
        mark = (struct TiksMark *)tmp->data;
        fprintf(f, "\\put(%f,%f){\\makebox(0,0)[%c%c]{\\tiny %s}}\n",
                (mark->x - render_area->x) / size * 100.0f,
                (1.0f - (mark->y - render_area->y) / size) * 100.0f,
                (mark->align & TiksAlignRight) ? 'r' : 'l',
                (mark->align & TiksAlignBottom) ? 't' : 'b',
                mark->text);
    }

    fprintf(f, "\\end{overpic}\n");
    fprintf(f, "\\end{document}\n");

out:
    if (f)
        fclose(f);
    g_free(texfile);
    g_free(basename);
    g_free(base);
}

void gl_widget_save_to_file(GlWidget *widget, const gchar *filename)
{
    g_return_if_fail(IS_GL_WIDGET(widget));
/*[begin < 3.16]*/
    glXMakeCurrent(widget->priv->display, widget->priv->window, widget->priv->glx_context);
/*[end < 3.16]*/
    GList *tiks = NULL;
    UtilRectangle render_area;

    /* render and get tiks */
    graphics_render(widget->priv->graphics_handle, (GraphicsTiksCallback)_gl_widget_tiks_callback, &tiks);
    graphics_get_render_area(widget->priv->graphics_handle, &render_area);
    graphics_save_buffer_to_file(widget->priv->graphics_handle, filename);
    _gl_widget_save_to_latex(filename, &render_area, tiks);
    g_list_free_full(tiks, (GDestroyNotify)_gl_widget_free_tiks_mark);

    /* re-render */
    graphics_render(widget->priv->graphics_handle, NULL, NULL);
}

