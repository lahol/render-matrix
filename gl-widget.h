#pragma once

#include <glib-object.h>
#include <gtk/gtk.h>
#include "graphics.h"

G_BEGIN_DECLS

#define GL_WIDGET_TYPE (gl_widget_get_type())
#define GL_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), GL_WIDGET_TYPE, GlWidget))
#define IS_GL_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GL_WIDGET_TYPE))
#define GL_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), GL_WIDGET_TYPE, GlWidgetClass))
#define IS_GL_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GL_WIDGET_TYPE))
#define GL_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), GL_WIDGET_TYPE, GlWidgetClass))

typedef struct _GlWidget GlWidget;
typedef struct _GlWidgetPrivate GlWidgetPrivate;
typedef struct _GlWidgetClass GlWidgetClass;

struct _GlWidget {
    GtkDrawingArea parent_instance;

    /*< private >*/
    GlWidgetPrivate *priv;
};

struct _GlWidgetClass {
    GtkDrawingAreaClass parent_class;
};

GType gl_widget_get_type(void) G_GNUC_CONST;

GtkWidget *gl_widget_new(GraphicsHandle *handle);
void gl_widget_save_to_file(GlWidget *widget, const gchar *filename);

G_END_DECLS
