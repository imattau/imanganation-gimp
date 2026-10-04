/* GIMP - The GNU Image Manipulation Program
 * Copyright (C) 2026
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 */

#include "config.h"

#include <string.h>

#include <glib.h>

#include "libgimp/gimp.h"


#define EXTENSION_PROC "extension-imanganation-panels"
#define ACTION_PREFIX  "extension-imanganation-panels-action-"
#define SELECT_PREFIX  "extension-imanganation-panels-select-"

typedef struct _ExtensionPanels      ExtensionPanels;
typedef struct _ExtensionPanelsClass ExtensionPanelsClass;

struct _ExtensionPanels
{
  GimpPlugIn parent_instance;
};

struct _ExtensionPanelsClass
{
  GimpPlugInClass parent_class;
};

#define EXTENSION_PANELS_TYPE (extension_panels_get_type ())
#define EXTENSION_PANELS(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), EXTENSION_PANELS_TYPE, ExtensionPanels))

GType extension_panels_get_type (void);

static GList          * extension_panels_query_procedures (GimpPlugIn *plug_in);
static GimpProcedure  * extension_panels_create_procedure (GimpPlugIn *plug_in,
                                                           const gchar *name);
static GimpValueArray * extension_panels_run (GimpProcedure *procedure,
                                              GimpProcedureConfig *config,
                                              gpointer run_data);
static GimpValueArray * extension_panels_action (GimpProcedure *procedure,
                                                 GimpProcedureConfig *config,
                                                 gpointer run_data);
static GimpValueArray * extension_panels_select (GimpProcedure *procedure,
                                                 GimpProcedureConfig *config,
                                                 gpointer run_data);
static gboolean         call_panel_proc (const gchar *name,
                                         const gchar *identifier,
                                         const gchar *text,
                                         const gchar *presentation,
                                         const gchar *selected_item,
                                         const gchar *title,
                                         const gchar *action_label,
                                         const gchar *action,
                                         const gchar *item_action);

G_DEFINE_TYPE (ExtensionPanels, extension_panels, GIMP_TYPE_PLUG_IN)
GIMP_MAIN (EXTENSION_PANELS_TYPE)


static void
extension_panels_class_init (ExtensionPanelsClass *klass)
{
  GimpPlugInClass *plug_in_class = GIMP_PLUG_IN_CLASS (klass);
  plug_in_class->query_procedures = extension_panels_query_procedures;
  plug_in_class->create_procedure = extension_panels_create_procedure;
}

static void
extension_panels_init (ExtensionPanels *panels)
{
}

static GList *
extension_panels_query_procedures (GimpPlugIn *plug_in)
{
  return g_list_append (NULL, g_strdup (EXTENSION_PROC));
}

static GimpProcedure *
extension_panels_create_procedure (GimpPlugIn  *plug_in,
                                   const gchar *name)
{
  GimpProcedure *procedure;

  if (strcmp (name, EXTENSION_PROC))
    return NULL;

  procedure = gimp_procedure_new (plug_in, name, GIMP_PDB_PROC_TYPE_PERSISTENT,
                                  extension_panels_run, NULL, NULL);
  gimp_procedure_set_documentation (procedure,
                                    "Demonstrate extension supplied dock panels",
                                    "Registers three host rendered panels using the extension panel PDB API.",
                                    name);
  gimp_procedure_set_attribution (procedure, "imanganation",
                                  "imanganation", "2026");
  return procedure;
}

static gboolean
call_panel_proc (const gchar *name,
                 const gchar *identifier,
                 const gchar *text,
                 const gchar *presentation,
                 const gchar *selected_item,
                 const gchar *title,
                 const gchar *action_label,
                 const gchar *action,
                 const gchar *item_action)
{
  GimpProcedure *procedure = gimp_pdb_lookup_procedure (gimp_get_pdb (), name);
  GimpProcedureConfig *config;
  GimpValueArray *values;
  GimpPDBStatusType status;

  if (!procedure)
    return FALSE;
  config = gimp_procedure_create_config (procedure);
  g_object_set (config, "identifier", identifier, NULL);
  if (text)
    g_object_set (config, "content", text, NULL);
  if (title)
    g_object_set (config, "title", title, NULL);
  if (selected_item)
    g_object_set (config, "selected-item", selected_item, NULL);
  if (!strcmp (name, "gimp-extension-panel-register"))
    g_object_set (config, "presentation", presentation,
                  "action-label", action_label,
                  "action-procedure", action ? action : "",
                  "item-action-procedure", item_action ? item_action : "", NULL);
  values = gimp_procedure_run_config (procedure, config);
  status = g_value_get_enum (gimp_value_array_index (values, 0));
  gimp_value_array_unref (values);
  g_object_unref (config);
  g_object_unref (procedure);
  return status == GIMP_PDB_SUCCESS;
}

static GimpValueArray *
extension_panels_action (GimpProcedure *procedure,
                         GimpProcedureConfig *config,
                         gpointer run_data)
{
  const gchar *identifier = run_data;
  const gchar *content;
  gboolean success;

  if (!strcmp (identifier, "project"))
    content = "# Blades of Fate\n# Chapter 04\n\t✓  Page 14\n\t✓  Page 15\n\t✓  Page 16\n\t●  Page 17\n\t○  Page 18\n# Assets\n\t# Characters\n\t# Locations\n\t# Props\n\t# References";
  else if (!strcmp (identifier, "inspector"))
    content = "# Panel 04\n# Context\nPage\tPage 17\n# Characters\nLead\tMei Lin\nPartner\tXiu Ying\n# Location\nSetting\tTemple Courtyard\n# Shot\nFraming\tMedium\nAngle\tLow\n# Continuity\nStatus\tClear";
  else
    content = "14  ✓\n15  ✓\n16  ✓\n17  ●\n18  ○";

  success = call_panel_proc ("gimp-extension-panel-update",
                             identifier, content, NULL,
                             !strcmp (identifier, "project") ? "●  Page 17" :
                             (!strcmp (identifier, "filmstrip") ? "17  ●" : ""),
                             NULL, NULL, NULL, NULL);
  return gimp_procedure_new_return_values (procedure,
                                            success ? GIMP_PDB_SUCCESS : GIMP_PDB_EXECUTION_ERROR,
                                            NULL);
}

static GimpValueArray *
extension_panels_select (GimpProcedure       *procedure,
                         GimpProcedureConfig *config,
                         gpointer             run_data)
{
  const gchar *item;
  const gchar *digits;
  gint page;
  gchar *project;
  gchar *filmstrip;
  gchar *inspector;
  gchar *project_selection;
  gchar *filmstrip_selection;
  gboolean success;

  g_object_get (config, "item", &item, NULL);
  digits = strstr (item, "Page ");
  if (digits)
    digits += strlen ("Page ");
  else
    {
      digits = item;
      if (g_str_has_prefix (digits, "● "))
        digits += strlen ("● ");
    }

  if (!g_ascii_isdigit (*digits))
    {
      g_free ((gchar *) item);
      return gimp_procedure_new_return_values (procedure,
                                                GIMP_PDB_SUCCESS, NULL);
    }
  page = *digits ? (gint) g_ascii_strtoll (digits, NULL, 10) : 0;
  if (page < 14 || page > 18)
    {
      g_free ((gchar *) item);
      return gimp_procedure_new_return_values (procedure,
                                                GIMP_PDB_SUCCESS, NULL);
    }

  project = g_strdup_printf (
    "# Blades of Fate\n# Chapter 04\n\t%sPage 14\n\t%sPage 15\n\t%sPage 16\n\t%sPage 17\n\t%sPage 18\n# Assets\n\t# Characters\n\t# Locations\n\t# Props\n\t# References",
    page == 14 ? "●  " : "✓  ", page == 15 ? "●  " : "✓  ",
    page == 16 ? "●  " : "✓  ", page == 17 ? "●  " : "○  ",
    page == 18 ? "●  " : "○  ");
  filmstrip = g_strdup_printf (
    "%s14%s\n%s15%s\n%s16%s\n%s17%s\n%s18%s",
    page == 14 ? "● " : "", page == 14 ? "" : "  ✓",
    page == 15 ? "● " : "", page == 15 ? "" : "  ✓",
    page == 16 ? "● " : "", page == 16 ? "" : "  ✓",
    page == 17 ? "● " : "", page == 17 ? "" : "  ○",
    page == 18 ? "● " : "", page == 18 ? "" : "  ○");
  inspector = g_strdup_printf (
    "# Panel 03\n# Context\nPage\tPage %02d\n# Characters\nLead\tMei Lin\nPartner\tXiu Ying\n# Location\nSetting\tTemple Courtyard\n# Shot\nFraming\tMedium\nAngle\tLow\n# Continuity\nStatus\tClear",
    page);
  project_selection = g_strdup_printf ("●  Page %d", page);
  filmstrip_selection = g_strdup_printf ("● %d", page);

  success = call_panel_proc ("gimp-extension-panel-update", "project",
                             project, NULL, project_selection,
                             NULL, NULL, NULL, NULL) &&
            call_panel_proc ("gimp-extension-panel-update", "inspector",
                             inspector, NULL, "",
                             NULL, NULL, NULL, NULL) &&
            call_panel_proc ("gimp-extension-panel-update", "filmstrip",
                             filmstrip, NULL, filmstrip_selection,
                             NULL, NULL, NULL, NULL);
  g_free (project);
  g_free (filmstrip);
  g_free (inspector);
  g_free (project_selection);
  g_free (filmstrip_selection);
  g_free ((gchar *) item);
  return gimp_procedure_new_return_values (
    procedure, success ? GIMP_PDB_SUCCESS : GIMP_PDB_EXECUTION_ERROR, NULL);
}

static void
install_action (GimpPlugIn *plug_in,
                const gchar *identifier)
{
  gchar *name = g_strconcat (ACTION_PREFIX, identifier, NULL);
  GimpProcedure *procedure = gimp_procedure_new (plug_in, name,
                                                 GIMP_PDB_PROC_TYPE_TEMPORARY,
                                                 extension_panels_action,
                                                 g_strdup (identifier), g_free);
  gimp_procedure_set_documentation (procedure, "Update sample dock content",
                                    "Called by the host rendered dock action button.", name);
  gimp_procedure_set_attribution (procedure, "imanganation", "imanganation", "2026");
  gimp_plug_in_add_temp_procedure (plug_in, procedure);
  g_object_unref (procedure);
  g_free (name);
}

static void
install_select_action (GimpPlugIn *plug_in,
                       const gchar *identifier)
{
  gchar *name = g_strconcat (SELECT_PREFIX, identifier, NULL);
  GimpProcedure *procedure = gimp_procedure_new (
    plug_in, name, GIMP_PDB_PROC_TYPE_TEMPORARY,
    extension_panels_select, NULL, NULL);

  gimp_procedure_add_string_argument (procedure, "item", "Item",
                                     "Activated item label",
                                     NULL, G_PARAM_READWRITE);
  gimp_procedure_set_documentation (procedure, "Select a sample project item",
                                    "Receives an activated host-rendered panel item.", name);
  gimp_procedure_set_attribution (procedure, "imanganation", "imanganation", "2026");
  gimp_plug_in_add_temp_procedure (plug_in, procedure);
  g_object_unref (procedure);
  g_free (name);
}

static GimpValueArray *
extension_panels_run (GimpProcedure *procedure,
                      GimpProcedureConfig *config,
                      gpointer run_data)
{
  GimpPlugIn *plug_in = gimp_procedure_get_plug_in (procedure);
  GMainLoop *loop = g_main_loop_new (NULL, FALSE);
  const gchar *actions[] = {
    ACTION_PREFIX "project", ACTION_PREFIX "inspector", ACTION_PREFIX "filmstrip"
  };
  const gchar *project = "# Blades of Fate\n# Chapter 04\n\t✓  Page 14\n\t✓  Page 15\n\t●  Page 16\n\t○  Page 17\n\t○  Page 18\n# Assets\n\t# Characters\n\t# Locations\n\t# Props\n\t# References";
  const gchar *inspector = "# Panel 03\n# Context\nPage\tPage 16\n# Characters\nLead\tMei Lin\nPartner\tXiu Ying\n# Location\nSetting\tTemple Courtyard\n# Shot\nFraming\tMedium\nAngle\tLow\n# Continuity\nStatus\tClear";
  const gchar *filmstrip = "14  ✓\n15  ✓\n16  ●\n17  ○\n18  ○";

  install_action (plug_in, "project");
  install_action (plug_in, "inspector");
  install_action (plug_in, "filmstrip");
  install_select_action (plug_in, "project");
  install_select_action (plug_in, "filmstrip");

  call_panel_proc ("gimp-extension-panel-register", "project", project,
                   "tree", "●  Page 16", "Project", "Open page", actions[0],
                   SELECT_PREFIX "project");
  call_panel_proc ("gimp-extension-panel-register", "inspector", inspector,
                   "properties", "", "Inspector", "Refresh", actions[1], NULL);
  call_panel_proc ("gimp-extension-panel-register", "filmstrip", filmstrip,
                   "tiles", "16  ●", "Page Filmstrip", "Next page", actions[2],
                   SELECT_PREFIX "filmstrip");

  gimp_procedure_persistent_ready (procedure);
  gimp_plug_in_persistent_enable (plug_in);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  return gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS, NULL);
}
