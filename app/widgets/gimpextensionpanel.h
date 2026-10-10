/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 2026
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include "core/core-types.h"
#include "plug-in/plug-in-types.h"

typedef struct _GimpDialogFactory GimpDialogFactory;

void     gimp_extension_panel_dialogs_init (GimpDialogFactory *factory);
void     gimp_extension_panel_show_unrestored (Gimp *gimp);
gchar  * gimp_extension_panel_get_owner (GimpPlugIn *plug_in);
void     gimp_extension_panel_plugin_closed (GimpPlugInManager *manager,
                                             GimpPlugIn        *plug_in,
                                             gpointer           user_data);
gboolean gimp_extension_panel_register     (Gimp              *gimp,
                                            const gchar       *owner,
                                            const gchar       *identifier,
                                            const gchar       *title,
                                            const gchar       *content,
                                            const gchar       *presentation,
                                            const gchar       *selected_item,
                                            const gchar       *action_label,
                                            const gchar       *action_procedure,
                                            const gchar       *item_action_procedure,
                                            const gchar       *icon_name,
                                            GError           **error);
gboolean gimp_extension_panel_update       (Gimp              *gimp,
                                            const gchar       *owner,
                                            const gchar       *identifier,
                                            const gchar       *content,
                                            const gchar       *selected_item,
                                            GError           **error);
gboolean gimp_extension_panel_show         (Gimp              *gimp,
                                            const gchar       *owner,
                                            const gchar       *identifier,
                                            GError           **error);
gboolean gimp_extension_panel_unregister   (Gimp              *gimp,
                                            const gchar       *owner,
                                            const gchar       *identifier,
                                            GError           **error);
