#pragma once

#include <glib.h>

void util_write_to_png(const gchar *filename, guchar *buffer, guint32 width, guint32 height);
