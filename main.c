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
    GtkWidget *check_permutation;
    GtkWidget *check_alternate_signs;

    Matrix *orig_matrix;
    Matrix *display_matrix;

    GraphicsHandle *graphics_handle;
} appdata;

static void camera_value_changed(GtkSpinButton *button, gpointer userdata)
{
    double azimuth = gtk_spin_button_get_value(GTK_SPIN_BUTTON(appdata.spin_azimuth));
    double elevation = gtk_spin_button_get_value(GTK_SPIN_BUTTON(appdata.spin_elevation));

    graphics_set_camera(appdata.graphics_handle, azimuth, elevation);

    gtk_widget_queue_draw(appdata.glwidget);
}

static void matrix_properties_toggled(GtkToggleButton *button, gpointer userdata)
{
    matrix_copy(appdata.display_matrix, appdata.orig_matrix);

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(appdata.check_permutation)))
        matrix_permutate_matrix(appdata.display_matrix);

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(appdata.check_alternate_signs)))
        matrix_alternate_signs(appdata.display_matrix);

    gtk_widget_queue_draw(appdata.glwidget);
}

/* TODO: Use GtkGLArea if Gtk+>=3.16 */
int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);

    appdata.graphics_handle = graphics_init();

    graphics_set_camera(appdata.graphics_handle, 65.0, -60.0);

    appdata.orig_matrix = matrix_read_from_file(STDIN_FILENO);
    appdata.display_matrix = matrix_dup(appdata.orig_matrix);
    graphics_set_matrix_data(appdata.graphics_handle, appdata.display_matrix);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(window), "destroy",
            G_CALLBACK(gtk_main_quit), NULL);

    appdata.glwidget = gl_widget_new(appdata.graphics_handle);

    GtkWidget *hbox, *vbox, *label;
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    appdata.spin_azimuth = gtk_spin_button_new_with_range(0.0, 360.0, 5.0);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(appdata.spin_azimuth), TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(appdata.spin_azimuth), 65.0);
    appdata.spin_elevation = gtk_spin_button_new_with_range(-180.0, 180.0, 5.0);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(appdata.spin_elevation), TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(appdata.spin_elevation), -60.0);

    g_signal_connect(G_OBJECT(appdata.spin_azimuth), "value-changed",
            G_CALLBACK(camera_value_changed), NULL);
    g_signal_connect(G_OBJECT(appdata.spin_elevation), "value-changed",
            G_CALLBACK(camera_value_changed), NULL);

    label = gtk_label_new("Azimuth:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(hbox), appdata.spin_azimuth, FALSE, FALSE, 3);

    label = gtk_label_new("Elevation:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(hbox), appdata.spin_elevation, FALSE, FALSE, 3);

    appdata.check_permutation = gtk_check_button_new_with_label("Reorder entries");
    g_signal_connect(G_OBJECT(appdata.check_permutation), "toggled",
            G_CALLBACK(matrix_properties_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), appdata.check_permutation, FALSE, FALSE, 3);

    appdata.check_alternate_signs = gtk_check_button_new_with_label("Alternate signs");
    g_signal_connect(G_OBJECT(appdata.check_alternate_signs), "toggled",
            G_CALLBACK(matrix_properties_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), appdata.check_alternate_signs, FALSE, FALSE, 3);


    gtk_box_pack_start(GTK_BOX(vbox), appdata.glwidget, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_show_all(window);

    gtk_main();

    matrix_free(appdata.display_matrix);
    matrix_free(appdata.orig_matrix);

    graphics_cleanup(appdata.graphics_handle);

    return 0;
}
