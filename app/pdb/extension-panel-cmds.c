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
  GimpProcedure *procedure = gimp_procedure_new (invoker, TRUE);

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
                                                       "for non-activating headings. Properties use '# ' section headings "
                                                       "and tab-separated name/value rows.",
                                                       FALSE, FALSE, TRUE, NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("presentation", "presentation",
                                                       "Host layout: list, tree, tiles, or properties",
                                                       FALSE, FALSE, TRUE, NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("selected-item", "selected item",
                                                       "Rendered item label highlighted by the host, or empty; tree "
                                                       "indentation and heading markers are excluded",
                                                       FALSE, FALSE, TRUE, NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("action-label", "action label",
                                                       "Button label, or empty",
                                                       FALSE, FALSE, TRUE, NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("action-procedure", "action procedure",
                                                       "Zero-argument procedure called by the button",
                                                       FALSE, FALSE, TRUE, NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("item-action-procedure", "item action procedure",
                                                       "One-string procedure called with the activated item's "
                                                       "rendered label, or empty",
                                                       FALSE, FALSE, TRUE, NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_pdb_register_procedure (pdb, procedure);
  g_object_unref (procedure);

  procedure = panel_procedure_new ("gimp-extension-panel-update",
                                   panel_update_invoker,
                                   "Update text content and selection in an extension-owned dock. "
                                   "The content uses the format selected when the dock was registered.");
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("content", "content",
                                                       "Host-rendered text using the registered presentation format",
                                                       FALSE, FALSE, TRUE, NULL,
                                                       GIMP_PARAM_READWRITE));
  gimp_procedure_add_argument (procedure,
                               gimp_param_spec_string ("selected-item", "selected item",
                                                       "Rendered item label highlighted by the host, or empty",
                                                       FALSE, FALSE, TRUE, NULL,
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
  GimpPlugIn *plug_in = gimp->plug_in_manager->current_plug_in;
  gchar *owner = panel_get_owner (plug_in);
  gboolean success = owner && gimp->gui.extension_panel_register &&
    gimp->gui.extension_panel_register (gimp, owner, identifier, title, content,
                                        presentation,
                                        selected_item,
                                        action_label, action_procedure,
                                        item_action_procedure, error);

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
