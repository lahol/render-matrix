#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <locale.h>

#include "graphics.h"
#include "gl-widget.h"
#include "matrix.h"
#include "matrix-mesh.h"
#include "mesh-export.h"
#include "util-projection.h"

struct {
    GtkWidget *glwidget;
    GtkWidget *spin_azimuth;
    GtkWidget *spin_elevation;
    GtkWidget *spin_tilt;
    GtkWidget *check_permutation;
    GtkWidget *check_alternate_signs;
    GtkWidget *check_shift_signs;
    GtkWidget *check_log_scale;

    Matrix *orig_matrix;
    Matrix *display_matrix;

    GraphicsHandle *graphics_handle;
} appdata;

struct {
    gboolean batchmode;
    double azimuth;
    double elevation;
    double tilt;
    double alpha_channel;

    gchar *output_filename;
    gboolean permutate_entries;
    gboolean alternate_signs;
    gboolean shift_signs;
    gboolean log_scale;
    gboolean optimize;
} config;

void main_config_default(void)
{
    config.batchmode = FALSE;
    config.output_filename = NULL;

    config.azimuth = 65.0;
    config.elevation = -60.0;
    config.tilt = 0.0;
    config.alpha_channel = 1.0;

    config.permutate_entries = FALSE;
    config.alternate_signs = FALSE;
    config.shift_signs = FALSE;
    config.log_scale = FALSE;
    config.optimize = FALSE;
}

static void camera_value_changed(GtkSpinButton *button, gpointer userdata)
{
    double azimuth = gtk_spin_button_get_value(GTK_SPIN_BUTTON(appdata.spin_azimuth));
    double elevation = gtk_spin_button_get_value(GTK_SPIN_BUTTON(appdata.spin_elevation));
    double tilt = gtk_spin_button_get_value(GTK_SPIN_BUTTON(appdata.spin_tilt));

    graphics_set_camera(appdata.graphics_handle, azimuth, elevation, tilt);

    gtk_widget_queue_draw(appdata.glwidget);
}

void main_update_display_matrix(void)
{
    matrix_copy(appdata.display_matrix, appdata.orig_matrix);
    if (config.log_scale)
        matrix_log_scale(appdata.display_matrix);
    if (config.permutate_entries)
        matrix_permutate_matrix(appdata.display_matrix);
    if (config.alternate_signs)
        matrix_alternate_signs(appdata.display_matrix, config.shift_signs);
}

static void matrix_properties_toggled(GtkToggleButton *button, gpointer userdata)
{
    matrix_copy(appdata.display_matrix, appdata.orig_matrix);

    config.log_scale =  gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(appdata.check_log_scale));
    config.permutate_entries = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(appdata.check_permutation));
    config.alternate_signs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(appdata.check_alternate_signs));
    config.shift_signs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(appdata.check_shift_signs));

    main_update_display_matrix();

    graphics_update_matrix_data(appdata.graphics_handle);

    gtk_widget_queue_draw(appdata.glwidget);
}

void main_save_matrix_to_file(const gchar *filename)
{
    ExportFileType type = mesh_export_get_type_from_filename(filename);

    switch (type) {
        case ExportFileTypePNG:
            gl_widget_save_to_file(GL_WIDGET(appdata.glwidget), filename);
            break;
        case ExportFileTypePDF:
        case ExportFileTypeSVG:
            {
                MatrixMesh *mesh = matrix_mesh_new();
                matrix_mesh_set_alpha_channel(mesh, config.alpha_channel);
                matrix_mesh_set_matrix(mesh, appdata.display_matrix);

                double projection[16];
                util_get_rotation_matrix_from_angles(projection, config.azimuth, config.elevation, config.tilt);
                mesh_export_to_file(filename, type, mesh, projection, config.optimize);

                matrix_mesh_free(mesh);
            }
            break;
        default:
            g_print("Unsupported file type.\n");
    }
}

static void save_to_file_button_clicked(GtkButton *button, gpointer userdata)
{
    GtkWidget *dialog = gtk_file_chooser_dialog_new("Save Image",
            GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(button))),
            GTK_FILE_CHOOSER_ACTION_SAVE,
            "Cancel", GTK_RESPONSE_CANCEL,
            "Save", GTK_RESPONSE_ACCEPT,
            NULL);
    GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
    gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);

    gtk_file_chooser_set_current_name(chooser, "matrix-image.png");

    GtkFileFilter *filter;
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "PNG-Images");
    gtk_file_filter_add_pattern(filter, "*.png");
    gtk_file_chooser_add_filter(chooser, filter);

    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "SVG-Images");
    gtk_file_filter_add_pattern(filter, "*.svg");
    gtk_file_chooser_add_filter(chooser, filter);

    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "PDF-Files");
    gtk_file_filter_add_pattern(filter, "*.pdf");
    gtk_file_chooser_add_filter(chooser, filter);

    gint res = gtk_dialog_run(GTK_DIALOG(dialog));
    gchar *filename;

    if (res == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(chooser);
        main_save_matrix_to_file(filename);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

/* TODO: Use GtkGLArea if Gtk+>=3.16 */

void main_init_ui(void)
{
    appdata.graphics_handle = graphics_init();
    graphics_set_alpha_channel(appdata.graphics_handle, config.alpha_channel);
    graphics_set_matrix_data(appdata.graphics_handle, appdata.display_matrix);

    graphics_set_camera(appdata.graphics_handle, config.azimuth, config.elevation, config.tilt);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(G_OBJECT(window), "destroy",
            G_CALLBACK(gtk_main_quit), NULL);

    appdata.glwidget = gl_widget_new(appdata.graphics_handle);

    GtkWidget *hbox, *vbox, *label, *button;
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    appdata.spin_azimuth = gtk_spin_button_new_with_range(0.0, 360.0, 5.0);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(appdata.spin_azimuth), TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(appdata.spin_azimuth), config.azimuth);
    appdata.spin_elevation = gtk_spin_button_new_with_range(-90.0, 90.0, 5.0);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(appdata.spin_elevation), FALSE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(appdata.spin_elevation), config.elevation);
    appdata.spin_tilt = gtk_spin_button_new_with_range(-180.0, 180.0, 5.0);
    gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(appdata.spin_tilt), TRUE);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(appdata.spin_tilt), config.tilt);

    g_signal_connect(G_OBJECT(appdata.spin_azimuth), "value-changed",
            G_CALLBACK(camera_value_changed), NULL);
    g_signal_connect(G_OBJECT(appdata.spin_elevation), "value-changed",
            G_CALLBACK(camera_value_changed), NULL);
    g_signal_connect(G_OBJECT(appdata.spin_tilt), "value-changed",
            G_CALLBACK(camera_value_changed), NULL);

    label = gtk_label_new("Azimuth:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(hbox), appdata.spin_azimuth, FALSE, FALSE, 3);

    label = gtk_label_new("Elevation:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(hbox), appdata.spin_elevation, FALSE, FALSE, 3);

    label = gtk_label_new("Tilt:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 3);
    gtk_box_pack_start(GTK_BOX(hbox), appdata.spin_tilt, FALSE, FALSE, 3);

    appdata.check_permutation = gtk_check_button_new_with_label("Reorder entries");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(appdata.check_permutation), config.permutate_entries);
    g_signal_connect(G_OBJECT(appdata.check_permutation), "toggled",
            G_CALLBACK(matrix_properties_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), appdata.check_permutation, FALSE, FALSE, 3);

    appdata.check_alternate_signs = gtk_check_button_new_with_label("Alternate signs");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(appdata.check_alternate_signs), config.alternate_signs);
    g_signal_connect(G_OBJECT(appdata.check_alternate_signs), "toggled",
            G_CALLBACK(matrix_properties_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), appdata.check_alternate_signs, FALSE, FALSE, 3);

    appdata.check_shift_signs = gtk_check_button_new_with_label("Shift signs");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(appdata.check_shift_signs), config.shift_signs);
    g_signal_connect(G_OBJECT(appdata.check_shift_signs), "toggled",
            G_CALLBACK(matrix_properties_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), appdata.check_shift_signs, FALSE, FALSE, 3);

    appdata.check_log_scale = gtk_check_button_new_with_label("Log scale");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(appdata.check_log_scale), config.log_scale);
    g_signal_connect(G_OBJECT(appdata.check_log_scale), "toggled",
            G_CALLBACK(matrix_properties_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), appdata.check_log_scale, FALSE, FALSE, 3);

    button = gtk_button_new_with_label("Save image");
    g_signal_connect(G_OBJECT(button), "clicked",
            G_CALLBACK(save_to_file_button_clicked), NULL);
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 3);


    gtk_box_pack_start(GTK_BOX(vbox), appdata.glwidget, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_show_all(window);

}

void main_cleanup(void)
{
    matrix_free(appdata.display_matrix);
    matrix_free(appdata.orig_matrix);

    graphics_cleanup(appdata.graphics_handle);
}

/* TODO: batch-mode (--batch, --azimuth, --elevation, --tilt, --export, --permute, --alternate-signs, --shift-signs, …) */
static GOptionEntry _command_line_options[] = {
    { "batch", 'b', 0, G_OPTION_ARG_NONE, &config.batchmode, "Run in batchmode (no GUI)", NULL },
    { "azimuth", 'a', 0, G_OPTION_ARG_DOUBLE, &config.azimuth, "Azimuth", "Angle in degree",  },
    { "elevation", 'e', 0, G_OPTION_ARG_DOUBLE, &config.elevation, "Elevation", "Angle in degree" },
    { "tilt", 't', 0, G_OPTION_ARG_DOUBLE, &config.tilt, "Tilt", "Angle in degree" },
    { "permutate-entries", 'P', 0, G_OPTION_ARG_NONE, &config.permutate_entries, "Permutate entries", NULL },
    { "reorder-entries", 'R', 0, G_OPTION_ARG_NONE, &config.permutate_entries, "same as --permutate-entries", NULL },
    { "alternate-signs", 'A', 0, G_OPTION_ARG_NONE, &config.alternate_signs, "Overlay matrix with alternating signs", NULL },
    { "shift-signs", 'S', 0, G_OPTION_ARG_NONE, &config.shift_signs, "Shift signs (only with --alternate-signs)", NULL },
    { "log-scale", 'L', 0, G_OPTION_ARG_NONE, &config.log_scale, "Use log-scale, i.e. sgn(value) * log(1+|value|)", NULL },
    { "output", 'o', 0, G_OPTION_ARG_FILENAME, &config.output_filename, "Output filename", "Filename" },
    { "optimize", 'O', 0, G_OPTION_ARG_NONE, &config.optimize, "Remove hidden faces from output", NULL },
    { "alpha", 'A', 0, G_OPTION_ARG_DOUBLE, &config.alpha_channel, "Alpha channel (between 0.0 and 1.0)", NULL },
    { NULL }
};

gboolean main_parse_command_line(int *argc, char ***argv)
{
    main_config_default();
    GOptionContext *context = g_option_context_new(" – render histogram of matrix from stdin");
    g_option_context_add_main_entries(context, _command_line_options, "render-matrix");
    if (!g_option_context_parse(context, argc, argv, NULL)) {
        g_option_context_free(context);
        return FALSE;
    }
    g_option_context_free(context);
/*  Read more arguments?
    if (*argc > 1) {
    }*/
    return TRUE;
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);

    setlocale(LC_NUMERIC, "C");

    if (!main_parse_command_line(&argc, &argv))
        return 1;

    appdata.orig_matrix = matrix_read_from_file(STDIN_FILENO);
    appdata.display_matrix = matrix_dup(appdata.orig_matrix);

    main_update_display_matrix();

    /* TODO: warn if unrecognized options, e.g. output_filename without batchmode */
    if (!config.batchmode) {
        main_init_ui();
    }
    else {
        if (config.output_filename)
            main_save_matrix_to_file(config.output_filename);
        goto done;
    }

    gtk_main();

done:
    main_cleanup();

    return 0;
}
