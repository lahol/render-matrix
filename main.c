#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "graphics.h"
#include "gl-widget.h"
#include "matrix.h"

struct {
    GtkWidget *glwidget;
    GtkWidget *spin_azimuth;
    GtkWidget *spin_elevation;

    GraphicsHandle *graphics_handle;
} appdata;

static void camera_value_changed(GtkSpinButton *button, gpointer userdata)
{
    double azimuth = gtk_spin_button_get_value(GTK_SPIN_BUTTON(appdata.spin_azimuth));
    double elevation = gtk_spin_button_get_value(GTK_SPIN_BUTTON(appdata.spin_elevation));

    graphics_set_camera(appdata.graphics_handle, azimuth, elevation);

    gtk_widget_queue_draw(appdata.glwidget);
}

/* TODO: Use GtkGLArea if Gtk+>=3.16 */
int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);

    appdata.graphics_handle = graphics_init();

    graphics_set_camera(appdata.graphics_handle, 65.0, -60.0);

    Matrix *m = matrix_read_from_file(STDIN_FILENO);
    if (m) {
        g_print("matrix: %u x %u\n", m->n_rows, m->n_columns);
        matrix_permutate_matrix(m);
        matrix_alternate_signs(m);
    }
    graphics_set_matrix_data(appdata.graphics_handle, m);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(window), "destroy",
            G_CALLBACK(gtk_main_quit), NULL);

    appdata.glwidget = gl_widget_new(appdata.graphics_handle);

    GtkWidget *hbox, *vbox;
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    appdata.spin_azimuth = gtk_spin_button_new_with_range(0.0, 360.0, 5.0);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(appdata.spin_azimuth), TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(appdata.spin_azimuth), 65.0);
    appdata.spin_elevation = gtk_spin_button_new_with_range(-180.0, 180.0, 5.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(appdata.spin_elevation), -60.0);

    g_signal_connect(G_OBJECT(appdata.spin_azimuth), "value-changed",
            G_CALLBACK(camera_value_changed), NULL);
    g_signal_connect(G_OBJECT(appdata.spin_elevation), "value-changed",
            G_CALLBACK(camera_value_changed), NULL);

    gtk_box_pack_start(GTK_BOX(hbox), appdata.spin_azimuth, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(hbox), appdata.spin_elevation, FALSE, FALSE, 3);

    gtk_box_pack_start(GTK_BOX(vbox), appdata.glwidget, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_show_all(window);

    gtk_main();

    matrix_free(m);

    graphics_cleanup(appdata.graphics_handle);

    return 0;
}
