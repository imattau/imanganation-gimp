/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 2026
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#include <glib-object.h>
#include <gegl.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "libgimpbase/gimpbasetypes.h"
#define __GIMP_BASE_H_INSIDE__
#include "libgimpbase/gimpparamspecs.h"
#include "libgimpbase/gimpvaluearray.h"
#undef __GIMP_BASE_H_INSIDE__

#include "pdb-types.h"

#include "core/gimp.h"
#include "core/gimpparamspecs.h"
#include "plug-in/gimpplugin.h"
#include "plug-in/gimppluginmanager.h"
#include "core/gimp-gui.h"

#include "pdb/gimppdb.h"
#include "pdb/gimpprocedure.h"

#include "extension-panel-cmds.h"


static GimpValueArray * panel_register_invoker   (GimpProcedure        *procedure,
                                                  Gimp                 *gimp,
                                                  GimpContext          *context,
                                                  GimpProgress         *progress,
                                                  const GimpValueArray *args,
                                                  GError              **error);
static GimpValueArray * panel_update_invoker     (GimpProcedure        *procedure,
                                                  Gimp                 *gimp,
                                                  GimpContext          *context,
                                                  GimpProgress         *progress,
                                                  const GimpValueArray *args,
                                                  GError              **error);
static GimpValueArray * panel_show_invoker       (GimpProcedure        *procedure,
                                                  Gimp                 *gimp,
                                                  GimpContext          *context,
                                                  GimpProgress         *progress,
                                                  const GimpValueArray *args,
                                                  GError              **error);
static GimpValueArray * panel_unregister_invoker (GimpProcedure        *procedure,
                                                  Gimp                 *gimp,
                                                  GimpContext          *context,
                                                  GimpProgress         *progress,
                                                  const GimpValueArray *args,
                                                  GError              **error);

static gchar *
panel_get_owner (GimpPlugIn *plug_in)
{
  gchar *owner;

  if (!plug_in || !plug_in->file)
    return NULL;

  owner = g_file_get_basename (plug_in->file);
#ifdef G_OS_WIN32
  if (g_str_has_suffix (owner, ".exe"))
    owner[strlen (owner) - 4] = '\0';
#endif
  return owner;
}

static GimpProcedure *
panel_procedure_new (const gchar    *name,
                     GimpMarshalFunc invoker,
                     const gchar    *help)
{
  GimpProcedure *procedure = gimp_procedure_new (invoker, FALSE);

  gimp_object_set_static_name (GIMP_OBJECT (procedure), name);
  gimp_procedure_set_static_help (procedure, help, help, NULL);
  gimp_procedure_set_static_attribution (procedure, "GIMP contributors",
                                         "GIMP contributors", "2026");
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("identifier", "identifier",
                                                       "Panel id within owner",
                                                       FALSE, FALSE, TRUE, NULL,
                                                       GIMP_PARAM_READWRITE));
  return procedure;
}

void
extension_panel_procs_init (GimpPDB *pdb)
{
  GimpProcedure *procedure;

  procedure = panel_procedure_new ("gimp-extension-panel-register",
                                   panel_register_invoker,
                                   "Register an extension-owned dock rendered by GIMP. "
                                   "The plug-in supplies text content and procedure names; "
                                   "GTK widgets remain inside the host process.");
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("title", "title", "Panel title",
                                                       FALSE, FALSE, TRUE, NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("content", "content",
                                                       "Host-rendered text. Tree rows use leading tabs for depth and '# ' "
                                                       "for non-activating headings; a tree row or heading may end with "
                                                       "'<TAB>!procedure:Label|procedure:Label', a right-click menu whose "
                                                       "items run that one-string procedure of the same plug-in with the "
                                                       "row id (empty for a heading); tile rows may end with the same menu, and "
                                                       "'# ' rows split tiles (not a strip) into headed sections. "
                                                       "A '!!reorder<TAB>procedure' row makes tiles draggable: a drop runs "
                                                       "that one-string procedure with 'dragged<TAB>target'. "
                                                       "Activatable list, tile and tree rows "
                                                       "may use id<TAB>label; tile rows may add a third tab-separated "
                                                       "absolute PNG preview path. Legacy rows use their label as the id. "
                                                       "Properties use '# ' section headings and tab-separated name/value "
                                                       "rows (a third field '!procedure:item:Label' adds a button that runs that "
                                                       "one-string procedure with item), plus '@key<TAB>name<TAB>value' "
                                                       "editable fields and "
                                                       "'!procedure<TAB>label' buttons (a no-argument procedure of the "
                                                       "same plug-in). Content is limited to 1 MiB, "
                                                       "2048 rows, and 4096 bytes per row.",
                                                       FALSE, FALSE, FALSE, NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("presentation", "presentation",
                                                       "Host layout: list, tree, tiles, strip (tiles in one horizontal row), "
                                                       "or properties",
                                                       FALSE, FALSE, TRUE, NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("selected-item", "selected item",
                                                       "Stable row id highlighted by the host (or legacy label), or empty",
                                                       FALSE, FALSE, FALSE, NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("action-label", "action label",
                                                       "Button label (a strip shows a \"+\" tile with it as the tooltip), or empty",
                                                       FALSE, FALSE, FALSE, NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("action-procedure", "action procedure",
                                                       "Zero-argument procedure called by the button",
                                                       FALSE, FALSE, FALSE, NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("item-action-procedure", "item action procedure",
                                                       "One-string procedure called with the activated item's stable row id "
                                                       "(or legacy label); for properties, with 'key<TAB>value' when a "
                                                       "field is edited. Or empty",
                                                       FALSE, FALSE, FALSE, NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("icon-name", "icon name",
                                                       "Optional icon name from the active GIMP icon theme; "
                                                       "omitted uses the plug-in icon",
                                                       FALSE, TRUE, FALSE, NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_pdb_register_procedure (pdb, procedure);
  g_object_unref (procedure);

  procedure = panel_procedure_new ("gimp-extension-panel-update",
                                   panel_update_invoker,
                                   "Update text content and selection in an extension-owned dock. "
                                   "The content uses the format selected when the dock was registered.");
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("content", "content",
                                                       "Host-rendered text using the registered presentation format; "
                                                       "limited to 1 MiB, 2048 rows, and 4096 bytes per row",
                                                       FALSE, FALSE, FALSE, NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("selected-item", "selected item",
                                                       "Stable row id highlighted by the host (or legacy label), or empty",
                                                       FALSE, FALSE, FALSE, NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_pdb_register_procedure (pdb, procedure);
  g_object_unref (procedure);

  procedure = panel_procedure_new ("gimp-extension-panel-show",
                                   panel_show_invoker,
                                   "Show an extension-owned dock.");
  gimp_pdb_register_procedure (pdb, procedure);
  g_object_unref (procedure);

  procedure = panel_procedure_new ("gimp-extension-panel-unregister",
                                   panel_unregister_invoker,
                                   "Remove an extension-owned dock.");
  gimp_pdb_register_procedure (pdb, procedure);
  g_object_unref (procedure);
}

static GimpValueArray *
panel_register_invoker (GimpProcedure        *procedure,
                        Gimp                 *gimp,
                        GimpContext          *context,
                        GimpProgress         *progress,
                        const GimpValueArray *args,
                        GError              **error)
{
  const gchar *identifier = g_value_get_string (gimp_value_array_index (args, 0));
  const gchar *title = g_value_get_string (gimp_value_array_index (args, 1));
  const gchar *content = g_value_get_string (gimp_value_array_index (args, 2));
  const gchar *presentation = g_value_get_string (gimp_value_array_index (args, 3));
  const gchar *selected_item = g_value_get_string (gimp_value_array_index (args, 4));
  const gchar *action_label = g_value_get_string (gimp_value_array_index (args, 5));
  const gchar *action_procedure = g_value_get_string (gimp_value_array_index (args, 6));
  const gchar *item_action_procedure = g_value_get_string (gimp_value_array_index (args, 7));
  const gchar *icon_name = g_value_get_string (gimp_value_array_index (args, 8));
  GimpPlugIn *plug_in = gimp->plug_in_manager->current_plug_in;
  gchar *owner = panel_get_owner (plug_in);
  gboolean success = owner && gimp->gui.extension_panel_register &&
    gimp->gui.extension_panel_register (gimp, owner, identifier, title,
                                        content,
                                        presentation,
                                        selected_item,
                                        action_label, action_procedure,
                                        item_action_procedure, icon_name, error);

  if (!owner)
    g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         "Only a running plug-in can register a dock");
  g_free (owner);

  return gimp_procedure_get_return_values (procedure, success,
                                           error ? *error : NULL);
}

static GimpValueArray *
panel_show_invoker (GimpProcedure        *procedure,
                    Gimp                 *gimp,
                    GimpContext          *context,
                    GimpProgress         *progress,
                    const GimpValueArray *args,
                    GError              **error)
{
  GimpPlugIn *plug_in = gimp->plug_in_manager->current_plug_in;
  gchar *owner = panel_get_owner (plug_in);
  gboolean success = owner && gimp->gui.extension_panel_show &&
    gimp->gui.extension_panel_show (gimp, owner,
      g_value_get_string (gimp_value_array_index (args, 0)), error);

  if (!owner)
    g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         "Only a running plug-in can show a dock");
  g_free (owner);

  return gimp_procedure_get_return_values (procedure, success,
                                           error ? *error : NULL);
}

static GimpValueArray *
panel_update_invoker (GimpProcedure        *procedure,
                      Gimp                 *gimp,
                      GimpContext          *context,
                      GimpProgress         *progress,
                      const GimpValueArray *args,
                      GError              **error)
{
  GimpPlugIn *plug_in = gimp->plug_in_manager->current_plug_in;
  gchar *owner = panel_get_owner (plug_in);
  gboolean success = owner && gimp->gui.extension_panel_update &&
    gimp->gui.extension_panel_update (gimp, owner,
      g_value_get_string (gimp_value_array_index (args, 0)),
      g_value_get_string (gimp_value_array_index (args, 1)),
      g_value_get_string (gimp_value_array_index (args, 2)), error);

  if (!owner)
    g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         "Only a running plug-in can update a dock");
  g_free (owner);

  return gimp_procedure_get_return_values (procedure, success,
                                           error ? *error : NULL);
}

static GimpValueArray *
panel_unregister_invoker (GimpProcedure        *procedure,
                          Gimp                 *gimp,
                          GimpContext          *context,
                          GimpProgress         *progress,
                          const GimpValueArray *args,
                          GError              **error)
{
  GimpPlugIn *plug_in = gimp->plug_in_manager->current_plug_in;
  gchar *owner = panel_get_owner (plug_in);
  gboolean success = owner && gimp->gui.extension_panel_unregister &&
    gimp->gui.extension_panel_unregister (gimp, owner,
      g_value_get_string (gimp_value_array_index (args, 0)), error);

  if (!owner)
    g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                         "Only a running plug-in can unregister a dock");
  g_free (owner);

  return gimp_procedure_get_return_values (procedure, success,
                                           error ? *error : NULL);
}
