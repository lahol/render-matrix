#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "graphics.h"
#include "gl-widget.h"

/* TODO: Use GtkGLArea if Gtk+>=3.16 */
int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);

    GraphicsHandle *handle = graphics_init();

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(window), "destroy",
            G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *glwidget = gl_widget_new(handle);

    gtk_container_add(GTK_CONTAINER(window), glwidget);
    gtk_widget_show_all(window);

    gtk_main();

    graphics_cleanup(handle);

    return 0;
}
