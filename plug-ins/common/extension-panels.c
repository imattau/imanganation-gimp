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

static gint current_page = 16;
static gint sample_page_count = 5;

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

static gchar *
project_content_new (void)
{
  GString *content = g_string_new (
    "# Blades of Fate\n\t# Chapter 04");
  gint page;

  for (page = 14; page < 14 + sample_page_count; page++)
    {
      const gchar *status = page < current_page ? "✓  " :
                            page == current_page ? "●  " : "○  ";
      g_string_append_printf (content, "\n\t\t%sPage %d", status, page);
    }

  g_string_append (content,
                   "\n\t# Assets"
                   "\n\t\t# Characters"
                   "\n\t\t\tMei Lin"
                   "\n\t\t\tXiu Ying"
                   "\n\t\t# Locations"
                   "\n\t\t\tTemple Courtyard"
                   "\n\t\t# Props"
                   "\n\t\t\tMoonblade"
                   "\n\t\t# References"
                   "\n\t\t\tVisual guide");

  return g_string_free (content, FALSE);
}

static gchar *
filmstrip_content_new (void)
{
  GString *content = g_string_new (NULL);
  gint page;

  for (page = 14; page < 14 + sample_page_count; page++)
    {
      const gchar *marker = page < current_page ? "  ✓" :
                            page == current_page ? "  ●" : "  ○";
      if (page > 14)
        g_string_append_c (content, '\n');
      g_string_append_printf (content, "%d%s", page, marker);
    }

  return g_string_free (content, FALSE);
}

static gchar *
inspector_content_new (gint page)
{
  return g_strdup_printf (
    "# Panel 03\n# Context\nPage\tPage %02d\n# Characters\nLead\tMei Lin\nPartner\tXiu Ying\n# Location\nSetting\tTemple Courtyard\n# Shot\nFraming\tMedium\nAngle\tLow\n# Panel objects\nMei Lin\tCharacter\nXiu Ying\tCharacter\nCourtyard\tBackground\nSpeed lines\tEffects\nDialogue\tText\n# Continuity\nStatus\tClear",
    page);
}

static gboolean
update_page_panels (gint page)
{
  gchar *project;
  gchar *filmstrip;
  gchar *inspector;
  gchar *project_selection;
  gchar *filmstrip_selection;
  gboolean project_updated;
  gboolean inspector_updated;
  gboolean filmstrip_updated;
  gboolean success;

  if (page < 14 || page >= 14 + sample_page_count)
    return FALSE;

  current_page = page;
  project = project_content_new ();
  filmstrip = filmstrip_content_new ();
  inspector = inspector_content_new (page);
  project_selection = g_strdup_printf ("●  Page %d", page);
  filmstrip_selection = g_strdup_printf ("%d  ●", page);

  project_updated = call_panel_proc ("gimp-extension-panel-update", "project",
                                     project, NULL, project_selection,
                                     NULL, NULL, NULL, NULL);
  inspector_updated = call_panel_proc ("gimp-extension-panel-update", "inspector",
                                       inspector, NULL, "",
                                       NULL, NULL, NULL, NULL);
  filmstrip_updated = call_panel_proc ("gimp-extension-panel-update", "filmstrip",
                                       filmstrip, NULL, filmstrip_selection,
                                       NULL, NULL, NULL, NULL);
  success = project_updated && inspector_updated && filmstrip_updated;

  g_free (project);
  g_free (filmstrip);
  g_free (inspector);
  g_free (project_selection);
  g_free (filmstrip_selection);
  return success;
}

static GimpValueArray *
extension_panels_action (GimpProcedure *procedure,
                         GimpProcedureConfig *config,
                         gpointer run_data)
{
  const gchar *identifier = run_data;
  gchar *content;
  gboolean success;

  if (!strcmp (identifier, "project"))
    {
      if (sample_page_count < 99)
        sample_page_count++;
      success = update_page_panels (14 + sample_page_count - 1);
      return gimp_procedure_new_return_values (
        procedure, success ? GIMP_PDB_SUCCESS : GIMP_PDB_EXECUTION_ERROR, NULL);
    }

  if (!strcmp (identifier, "filmstrip"))
    {
      gint last_page = 14 + sample_page_count - 1;
      gint next_page = current_page < last_page ? current_page + 1 : 14;

      success = update_page_panels (next_page);
      return gimp_procedure_new_return_values (
        procedure, success ? GIMP_PDB_SUCCESS : GIMP_PDB_EXECUTION_ERROR, NULL);
    }

  if (!strcmp (identifier, "inspector"))
    content = inspector_content_new (current_page);
  else
    content = NULL;

  success = content && call_panel_proc ("gimp-extension-panel-update",
                                        identifier, content, NULL, "",
                                        NULL, NULL, NULL, NULL);
  g_free (content);
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
  gchar *inspector;
  gboolean success;

  g_object_get (config, "item", &item, NULL);
  if (!strcmp (item, "Mei Lin") || !strcmp (item, "Xiu Ying"))
    {
      inspector = g_strdup_printf (
        "# %s\n# Character\nRole\t%s\nOutfit\tTravel\n# Expressions\nNeutral\tReady\nAngry\tReady\n# References\nImages\t3",
        item, !strcmp (item, "Mei Lin") ? "Lead" : "Partner");
      success = call_panel_proc ("gimp-extension-panel-update", "inspector",
                                 inspector, NULL, "",
                                 NULL, NULL, NULL, NULL);
      g_free (inspector);
      g_free ((gchar *) item);
      return gimp_procedure_new_return_values (
        procedure, success ? GIMP_PDB_SUCCESS : GIMP_PDB_EXECUTION_ERROR, NULL);
    }
  else if (!strcmp (item, "Temple Courtyard"))
    inspector = g_strdup ("# Temple Courtyard\n# Location\nTime\tDusk\nWeather\tOvercast\n# References\nImages\t3");
  else if (!strcmp (item, "Moonblade"))
    inspector = g_strdup ("# Moonblade\n# Prop\nOwner\tMei Lin\nMaterial\tJade steel\n# References\nImages\t2");
  else if (!strcmp (item, "Visual guide"))
    inspector = g_strdup ("# Visual Guide\n# Reference set\nPalette\tJade and indigo\nPages\t01–12\nUse\tStyle reference");
  else
    inspector = NULL;

  if (inspector)
    {
      success = call_panel_proc ("gimp-extension-panel-update", "inspector",
                                 inspector, NULL, "",
                                 NULL, NULL, NULL, NULL);
      g_free (inspector);
      g_free ((gchar *) item);
      return gimp_procedure_new_return_values (
        procedure, success ? GIMP_PDB_SUCCESS : GIMP_PDB_EXECUTION_ERROR, NULL);
    }

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
  if (page < 14 || page >= 14 + sample_page_count)
    {
      g_free ((gchar *) item);
      return gimp_procedure_new_return_values (procedure,
                                                GIMP_PDB_SUCCESS, NULL);
    }

  success = update_page_panels (page);
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
  gboolean project_registered;
  gboolean inspector_registered;
  gboolean filmstrip_registered;
  gchar *project = project_content_new ();
  gchar *inspector = inspector_content_new (current_page);
  gchar *filmstrip = filmstrip_content_new ();

  install_action (plug_in, "project");
  install_action (plug_in, "inspector");
  install_action (plug_in, "filmstrip");
  install_select_action (plug_in, "project");
  install_select_action (plug_in, "filmstrip");

  project_registered = call_panel_proc (
    "gimp-extension-panel-register", "project", project,
    "tree", "●  Page 16", "Project", "Add Page", actions[0],
    SELECT_PREFIX "project");
  inspector_registered = call_panel_proc (
    "gimp-extension-panel-register", "inspector", inspector,
    "properties", "", "Inspector", "Refresh", actions[1], NULL);
  filmstrip_registered = call_panel_proc (
    "gimp-extension-panel-register", "filmstrip", filmstrip,
    "tiles", "16  ●", "Page Filmstrip", "Next page", actions[2],
    SELECT_PREFIX "filmstrip");

  g_free (project);
  g_free (inspector);
  g_free (filmstrip);

  if (!project_registered || !inspector_registered || !filmstrip_registered)
    {
      call_panel_proc ("gimp-extension-panel-unregister", "project",
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL);
      call_panel_proc ("gimp-extension-panel-unregister", "inspector",
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL);
      call_panel_proc ("gimp-extension-panel-unregister", "filmstrip",
                       NULL, NULL, NULL, NULL, NULL, NULL, NULL);
      g_main_loop_unref (loop);
      return gimp_procedure_new_return_values (
        procedure, GIMP_PDB_EXECUTION_ERROR, NULL);
    }

  gimp_procedure_persistent_ready (procedure);
  gimp_plug_in_persistent_enable (plug_in);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  return gimp_procedure_new_return_values (procedure, GIMP_PDB_SUCCESS, NULL);
}
