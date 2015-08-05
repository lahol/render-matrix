#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "graphics.h"

static gboolean da_configure_event(GtkWidget *widget, GdkEventConfigure *event, GraphicsHandle *handle)
{
    graphics_set_window_size(handle,
            gtk_widget_get_allocated_width(widget),
            gtk_widget_get_allocated_height(widget));
    gtk_widget_queue_draw(widget);
    return TRUE;
}

static gboolean da_draw_event(GtkWidget *widget, cairo_t *cr, GraphicsHandle *handle)
{
    graphics_render(handle);
    return FALSE;
}

static void da_realize_event(GtkWidget *widget, GraphicsHandle *handle)
{
    graphics_set_window(handle, GDK_WINDOW_XID(gtk_widget_get_window(widget)));
}

/* TODO: Use GtkGLArea if Gtk+>=3.16 */
int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);

    GraphicsHandle *handle = graphics_init();

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(window), "destroy",
            G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(GTK_WIDGET(drawing_area), 300, 300);
    gtk_widget_set_double_buffered(GTK_WIDGET(drawing_area), FALSE);
    g_signal_connect(G_OBJECT(drawing_area), "configure-event",
            G_CALLBACK(da_configure_event), handle);
    g_signal_connect(G_OBJECT(drawing_area), "draw",
            G_CALLBACK(da_draw_event), handle);
    g_signal_connect(G_OBJECT(drawing_area), "realize",
            G_CALLBACK(da_realize_event), handle);

    gtk_container_add(GTK_CONTAINER(window), drawing_area);
    gtk_widget_show_all(window);

    gtk_main();

    graphics_cleanup(handle);

    return 0;
}
