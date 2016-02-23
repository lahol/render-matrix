#include <stdio.h>
#include <stdlib.h>
#include <glob.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <locale.h>

#include "graphics.h"
#include "gl-widget.h"
#include "matrix.h"
#include "matrix-mesh.h"
#include "mesh-export.h"
#include "util-projection.h"
#include "util-colors.h"

struct {
    GtkWidget *glwidget;
    GtkWidget *spin_azimuth;
    GtkWidget *spin_elevation;
    GtkWidget *spin_tilt;
    GtkWidget *check_permutation;
    GtkWidget *check_alternate_signs;
    GtkWidget *check_shift_signs;
    GtkWidget *check_log_scale;

    GList *infiles;

    Matrix *display_matrix;
    struct {
        GList *head;
        GList *tail;
        GList *current;
    } matrix_list;

    GraphicsHandle *graphics_handle;
} appdata;

struct {
    gboolean batchmode;
    double azimuth;
    double elevation;
    double tilt;
    double alpha_channel;
    double export_width;
    double colorbar_pos_x; /* >= 0 -> bounding_box->width + pos, <0: left of plot */

    gchar *output_filename;
    gboolean permutate_entries;
    gboolean alternate_signs;
    gboolean shift_signs;
    gboolean log_scale;
    gboolean optimize;
    gboolean export_standalone;
    gboolean export_colorbar;
    gboolean grayscale;
    gboolean absolute_values;
} config;

void main_config_default(void)
{
    config.batchmode = FALSE;
    config.output_filename = NULL;

    config.azimuth = 65.0;
    config.elevation = -60.0;
    config.tilt = 0.0;
    config.alpha_channel = 1.0;
    config.export_width = 15.0;
    config.colorbar_pos_x = 1.0;

    config.permutate_entries = FALSE;
    config.alternate_signs = FALSE;
    config.shift_signs = FALSE;
    config.log_scale = FALSE;
    config.optimize = FALSE;
    config.export_standalone = FALSE;
    config.export_colorbar = TRUE;
    config.grayscale = FALSE;
    config.absolute_values = FALSE;
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
    if (!appdata.matrix_list.current)
        return;
    matrix_copy(appdata.display_matrix, appdata.matrix_list.current->data);
    if (config.log_scale)
        matrix_log_scale(appdata.display_matrix);
    if (config.permutate_entries)
        matrix_permutate_matrix(appdata.display_matrix);
    if (config.alternate_signs)
        matrix_alternate_signs(appdata.display_matrix, config.shift_signs);
    if (config.absolute_values)
        matrix_set_absolute(appdata.display_matrix);
}

static void matrix_properties_toggled(GtkToggleButton *button, gpointer userdata)
{
    config.log_scale =  gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(appdata.check_log_scale));
    config.permutate_entries = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(appdata.check_permutation));
    config.alternate_signs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(appdata.check_alternate_signs));
    config.shift_signs = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(appdata.check_shift_signs));

    main_update_display_matrix();

    graphics_update_matrix_data(appdata.graphics_handle);

    gtk_widget_queue_draw(appdata.glwidget);
}

void main_matrix_next(void)
{
    appdata.matrix_list.current = g_list_next(appdata.matrix_list.current);
    if (appdata.matrix_list.current == NULL)
        appdata.matrix_list.current = appdata.matrix_list.head;

    main_update_display_matrix();
    graphics_update_matrix_data(appdata.graphics_handle);
    gtk_widget_queue_draw(appdata.glwidget);
}

void main_save_matrix_to_file(const gchar *filename)
{
    ExportFileType type = mesh_export_get_type_from_filename(filename);

    ExportConfig expconfig;
    expconfig.type = type;
    expconfig.remove_hidden = config.optimize;
    expconfig.image_width = config.export_width;
    expconfig.standalone = config.export_standalone;
    expconfig.colorbar_pos_x = config.colorbar_pos_x;
    expconfig.alpha_channel = config.alpha_channel;
    expconfig.show_colorbar = config.export_colorbar;
    expconfig.permutate_entries = config.permutate_entries;
    expconfig.alternate_signs = config.alternate_signs;
    expconfig.shift_signs = config.shift_signs;
    expconfig.log_scale = config.log_scale;
    expconfig.absolute_values = config.absolute_values;

    switch (type) {
        case ExportFileTypePNG:
            gl_widget_save_to_file(GL_WIDGET(appdata.glwidget), filename);
            break;
        case ExportFileTypePDF:
        case ExportFileTypeSVG:
        case ExportFileTypeTikZ:
            {
#if 0
                MatrixMesh *mesh = matrix_mesh_new();
                matrix_mesh_set_alpha_channel(mesh, config.alpha_channel);
                matrix_mesh_set_matrix(mesh, appdata.display_matrix);

                double projection[16];
                util_get_rotation_matrix_from_angles(projection, config.azimuth, config.elevation, config.tilt);
                mesh_export_to_file(filename, type, mesh, projection, &expconfig);

                matrix_mesh_free(mesh);
#else
                double projection[16];
                util_get_rotation_matrix_from_angles(projection, config.azimuth, config.elevation, config.tilt);
                mesh_export_matrices_to_files(filename, type, appdata.matrix_list.head, projection, &expconfig);
#endif
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

    button = gtk_button_new_with_label("Next");
    g_signal_connect(G_OBJECT(button), "clicked",
            G_CALLBACK(main_matrix_next), NULL);
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 3);


    gtk_box_pack_start(GTK_BOX(vbox), appdata.glwidget, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 2);

    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_widget_show_all(window);

}

void main_cleanup(void)
{
    matrix_free(appdata.display_matrix);
    g_list_free_full(appdata.matrix_list.head, (GDestroyNotify)matrix_free);
    g_list_free_full(appdata.infiles, g_free);

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
    { "absolute-values", 0, 0, G_OPTION_ARG_NONE, &config.absolute_values, "Only show magnitude of entries", NULL },
    { "shift-signs", 'S', 0, G_OPTION_ARG_NONE, &config.shift_signs, "Shift signs (only with --alternate-signs)", NULL },
    { "log-scale", 'L', 0, G_OPTION_ARG_NONE, &config.log_scale, "Use log-scale, i.e. sgn(value) * log(1+|value|)", NULL },
    { "output", 'o', 0, G_OPTION_ARG_FILENAME, &config.output_filename, "Output filename", "Filename" },
    { "optimize", 'O', 0, G_OPTION_ARG_NONE, &config.optimize, "Remove hidden faces from output", NULL },
    { "alpha", 'T', 0, G_OPTION_ARG_DOUBLE, &config.alpha_channel, "Alpha channel (between 0.0 and 1.0)", NULL },
    { "standalone", 's', 0, G_OPTION_ARG_NONE, &config.export_standalone, "Produce standalone file", NULL },
    { "width", 'w', 0, G_OPTION_ARG_DOUBLE, &config.export_width, "Width of TikZ picture", NULL },
    { "colorbar-x", 0, 0, G_OPTION_ARG_DOUBLE, &config.colorbar_pos_x, "Relative position of colorbar", "offset" },
    { "colorbar", 0, 0, G_OPTION_ARG_NONE, &config.export_colorbar, "Print a colorbar in export", NULL },
    { "no-colorbar", 0, G_OPTION_FLAG_REVERSE, G_OPTION_ARG_NONE, &config.export_colorbar, "Do not print colorbar", NULL },
    { "grayscale", 0, 0, G_OPTION_ARG_NONE, &config.grayscale, "Use grayscale", NULL },
    { NULL }
};

gboolean main_parse_command_line(int *argc, char ***argv)
{
    main_config_default();
    GOptionContext *context = g_option_context_new(" [files […]] – render histogram of matrix from stdin");
    g_option_context_add_main_entries(context, _command_line_options, "render-matrix");
    if (!g_option_context_parse(context, argc, argv, NULL)) {
        g_option_context_free(context);
        return FALSE;
    }
    g_option_context_free(context);
/*  Read more arguments? */
    /* Read glob style input files without interpretation. */
    glob_t infiles;
    int glob_flags = 0;
    int i, j, rc;

    if (*argc > 1) {
        for (i = 1; i < *argc; ++i) {
            rc = glob((*argv)[i], glob_flags, NULL, &infiles);
            if (rc == GLOB_ABORTED || rc == GLOB_NOSPACE) {
                g_printerr("Globbing failed.\n");

                globfree(&infiles);
                return FALSE;
            }
            else if (rc == GLOB_NOMATCH) {
                if (g_strcmp0((*argv)[i], "-") == 0) {
                    appdata.infiles = g_list_prepend(appdata.infiles, g_strdup("-"));
                    globfree(&infiles);
                    continue;
                }
                else {
                    g_print("Pattern `%s' matches no files.\n", (*argv)[i]);
                }
            }

            if (infiles.gl_pathc) {
                for (j = 0; j < infiles.gl_pathc; ++j) {
                    appdata.infiles = g_list_prepend(appdata.infiles, g_strdup(infiles.gl_pathv[j]));
                }
            }

            globfree(&infiles);
        }

    }

    appdata.infiles = g_list_reverse(appdata.infiles);

    return TRUE;
}

GList *main_read_input_files(void)
{
    /* run through all input files (none or - -> STDIN_FILENO) and concatenate the lists
     * read from the files. */
    int fd;
    GList *tmp;

    if (appdata.infiles == NULL) {
        fprintf(stderr, "No input files given. Reading from stdin.\n");
        return matrix_read_from_file(STDIN_FILENO);
    }

    GList *matrices = NULL;
    for (tmp = appdata.infiles; tmp; tmp = g_list_next(tmp)) {
        if (g_strcmp0((gchar *)tmp->data, "-") == 0) {
            matrices = g_list_concat(matrices, matrix_read_from_file(STDIN_FILENO));
        }
        else {
            fd = open((gchar *)tmp->data, O_RDONLY);
            if (fd < 0) {
                fprintf(stderr, "Unable to open file `%s'. Skipping.\n", (gchar *)tmp->data);
            }
            else {
                matrices = g_list_concat(matrices, matrix_read_from_file(fd));
                close(fd);
            }
        }
    }

    return matrices;
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);

    setlocale(LC_NUMERIC, "C");

    if (!main_parse_command_line(&argc, &argv))
        return 1;

    if (config.grayscale)
        util_colors_set_grayscale(1);

    appdata.matrix_list.head = main_read_input_files();
    appdata.matrix_list.current = appdata.matrix_list.head;
    appdata.matrix_list.tail = g_list_last(appdata.matrix_list.head);
    appdata.display_matrix = matrix_new();

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
