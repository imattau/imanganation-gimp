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
#include "libgimp/gimpui.h"

#include <gtk/gtk.h>


#define EXTENSION_PROC "extension-imanganation-panels"
#define ACTION_PREFIX  "extension-imanganation-panels-action-"
#define SELECT_PREFIX  "extension-imanganation-panels-select-"

static gint current_page = 16;
static gint sample_page_count = 5;
static gint project_first_page = 14;
static gchar *project_title;
static gchar *project_chapter;
static gchar **project_characters;
static gchar **project_locations;
static gchar **project_props;
static gchar **project_references;
static GFile *project_file;

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
static gboolean         project_state_load (GError **error);
static gboolean         project_state_load_file (GFile   *file,
                                                 GError **error);
static gboolean         project_state_save (GError **error);
static gboolean         project_state_open (GFile   *file,
                                            GError **error);
static void             project_state_clear (void);

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

static gboolean
project_state_save (GError **error)
{
  GKeyFile *key_file = g_key_file_new ();
  gchar *data;
  gsize length;
  gboolean success;

  g_key_file_set_integer (key_file, "Project", "format-version", 1);
  g_key_file_set_string (key_file, "Project", "title", project_title);
  g_key_file_set_string (key_file, "Project", "chapter", project_chapter);
  g_key_file_set_integer (key_file, "Project", "first-page", project_first_page);
  g_key_file_set_integer (key_file, "Project", "page-count", sample_page_count);
  g_key_file_set_integer (key_file, "Project", "current-page", current_page);
  g_key_file_set_string_list (key_file, "Assets", "characters",
                              (const gchar * const *) project_characters,
                              g_strv_length (project_characters));
  g_key_file_set_string_list (key_file, "Assets", "locations",
                              (const gchar * const *) project_locations,
                              g_strv_length (project_locations));
  g_key_file_set_string_list (key_file, "Assets", "props",
                              (const gchar * const *) project_props,
                              g_strv_length (project_props));
  g_key_file_set_string_list (key_file, "Assets", "references",
                              (const gchar * const *) project_references,
                              g_strv_length (project_references));

  data = g_key_file_to_data (key_file, &length, error);
  g_key_file_unref (key_file);
  if (!data)
    return FALSE;

  success = g_file_replace_contents (project_file, data, length, NULL, FALSE,
                                     G_FILE_CREATE_PRIVATE, NULL, NULL, error);
  g_free (data);
  return success;
}

static gboolean
project_label_is_valid (const gchar *label,
                        gsize        max_length)
{
  return label && *label && strlen (label) <= max_length &&
         g_utf8_validate (label, -1, NULL) &&
         !strpbrk (label, "\r\n\t") &&
         !g_str_has_prefix (label, "# ");
}

static gboolean
project_asset_list_is_valid (gchar **assets)
{
  gint i;

  if (!assets || g_strv_length (assets) > 450)
    return FALSE;

  for (i = 0; assets[i]; i++)
    if (!project_label_is_valid (assets[i], 256))
      return FALSE;

  return TRUE;
}

static gboolean
project_state_load_file (GFile   *file,
                         GError **error)
{
  GKeyFile *key_file = g_key_file_new ();
  gchar *path;
  GError *local_error = NULL;
  gboolean success = FALSE;

  path = g_file_get_path (file);
  if (!path)
    {
      g_set_error_literal (&local_error, G_IO_ERROR,
                           G_IO_ERROR_NOT_SUPPORTED,
                           "Manga project manifests must be local files");
      goto failed;
    }
  success = g_key_file_load_from_file (key_file, path, G_KEY_FILE_NONE,
                                       &local_error);
  g_free (path);
  if (!success)
    goto failed;

  if (g_key_file_get_integer (key_file, "Project", "format-version",
                              &local_error) != 1)
    goto invalid;
  project_title = g_key_file_get_string (key_file, "Project", "title",
                                         &local_error);
  if (!project_title)
    goto invalid;
  project_chapter = g_key_file_get_string (key_file, "Project", "chapter",
                                           &local_error);
  if (!project_chapter)
    goto invalid;
  project_first_page = g_key_file_get_integer (key_file, "Project", "first-page",
                                                &local_error);
  if (local_error || project_first_page < 1 || project_first_page > 999999)
    goto invalid;
  sample_page_count = g_key_file_get_integer (key_file, "Project", "page-count",
                                               &local_error);
  if (local_error)
    goto invalid;
  current_page = g_key_file_get_integer (key_file, "Project", "current-page",
                                         &local_error);
  if (local_error || sample_page_count < 1 || sample_page_count > 99 ||
      current_page < project_first_page ||
      current_page >= project_first_page + sample_page_count)
    goto invalid;

  project_characters = g_key_file_get_string_list (key_file, "Assets", "characters",
                                                    NULL, &local_error);
  if (!project_characters)
    goto invalid;
  project_locations = g_key_file_get_string_list (key_file, "Assets", "locations",
                                                   NULL, &local_error);
  if (!project_locations)
    goto invalid;
  project_props = g_key_file_get_string_list (key_file, "Assets", "props",
                                               NULL, &local_error);
  if (!project_props)
    goto invalid;
  project_references = g_key_file_get_string_list (key_file, "Assets", "references",
                                                   NULL, &local_error);
  if (!project_references)
    goto invalid;

  if (!project_label_is_valid (project_title, 128) ||
      !project_label_is_valid (project_chapter, 128) ||
      !project_asset_list_is_valid (project_characters) ||
      !project_asset_list_is_valid (project_locations) ||
      !project_asset_list_is_valid (project_props) ||
      !project_asset_list_is_valid (project_references))
    goto invalid;

  success = TRUE;
  project_file = g_object_ref (file);
  goto out;

invalid:
  if (!local_error)
    g_set_error_literal (&local_error, G_KEY_FILE_ERROR,
                         G_KEY_FILE_ERROR_INVALID_VALUE,
                         "The manga project manifest contains invalid values");
failed:
  g_propagate_error (error, local_error);

out:
  g_clear_error (&local_error);
  g_key_file_unref (key_file);
  return success;
}

static gboolean
project_state_load (GError **error)
{
  GFile *directory = gimp_directory_file ("imanganation", NULL);
  GFile *file;
  gboolean success;

  if (!g_file_query_exists (directory, NULL) &&
      !g_file_make_directory_with_parents (directory, NULL, error))
    {
      g_object_unref (directory);
      return FALSE;
    }
  g_object_unref (directory);

  file = gimp_directory_file ("imanganation", "blades-of-fate.project", NULL);
  if (g_file_query_exists (file, NULL))
    {
      success = project_state_load_file (file, error);
    }
  else
    {
      project_title = g_strdup ("Blades of Fate");
      project_chapter = g_strdup ("Chapter 04");
      project_first_page = 14;
      current_page = 16;
      sample_page_count = 5;
      project_characters = g_strsplit ("Mei Lin\nXiu Ying", "\n", -1);
      project_locations = g_strsplit ("Temple Courtyard", "\n", -1);
      project_props = g_strsplit ("Moonblade", "\n", -1);
      project_references = g_strsplit ("Visual guide", "\n", -1);
      project_file = g_object_ref (file);
      success = project_state_save (error);
    }

  g_object_unref (file);
  return success;
}

static void
project_state_clear (void)
{
  g_clear_pointer (&project_title, g_free);
  g_clear_pointer (&project_chapter, g_free);
  g_clear_pointer (&project_characters, g_strfreev);
  g_clear_pointer (&project_locations, g_strfreev);
  g_clear_pointer (&project_props, g_strfreev);
  g_clear_pointer (&project_references, g_strfreev);
  g_clear_object (&project_file);
}

static gboolean
project_state_open (GFile   *file,
                    GError **error)
{
  GFile *previous_file = g_object_ref (project_file);
  GError *local_error = NULL;

  project_state_clear ();
  if (project_state_load_file (file, &local_error))
    {
      g_object_unref (previous_file);
      return TRUE;
    }

  project_state_clear ();
  if (!project_state_load_file (previous_file, NULL))
    g_warning ("Could not restore the previous project after an open error");
  g_object_unref (previous_file);
  g_propagate_error (error, local_error);
  return FALSE;
}

static gchar *
project_content_new (void)
{
  GString *content = g_string_new (NULL);
  gint page;

  g_string_append_printf (content, "# %s\n\t# %s",
                          project_title, project_chapter);

  for (page = project_first_page;
       page < project_first_page + sample_page_count; page++)
    {
      const gchar *status = page < current_page ? "✓  " :
                            page == current_page ? "●  " : "○  ";
      g_string_append_printf (content, "\n\t\t%sPage %d", status, page);
    }

  g_string_append (content, "\n\t# Assets\n\t\t# Characters");
  for (page = 0; project_characters[page]; page++)
    g_string_append_printf (content, "\n\t\t\t%s", project_characters[page]);
  g_string_append (content, "\n\t\t# Locations");
  for (page = 0; project_locations[page]; page++)
    g_string_append_printf (content, "\n\t\t\t%s", project_locations[page]);
  g_string_append (content, "\n\t\t# Props");
  for (page = 0; project_props[page]; page++)
    g_string_append_printf (content, "\n\t\t\t%s", project_props[page]);
  g_string_append (content, "\n\t\t# References");
  for (page = 0; project_references[page]; page++)
    g_string_append_printf (content, "\n\t\t\t%s", project_references[page]);
  g_string_append (content, "\n\t＋ Add Page");

  return g_string_free (content, FALSE);
}

static gchar *
filmstrip_content_new (void)
{
  GString *content = g_string_new (NULL);
  gint page;

  for (page = project_first_page;
       page < project_first_page + sample_page_count; page++)
    {
      const gchar *marker = page < current_page ? "  ✓" :
                            page == current_page ? "  ●" : "  ○";
      if (page > project_first_page)
        g_string_append_c (content, '\n');
      g_string_append_printf (content, "%d%s", page, marker);
    }

  return g_string_free (content, FALSE);
}

static gchar *
inspector_content_new (gint page)
{
  return g_strdup_printf (
    "# Page %d\n# Project\nTitle\t%s\nChapter\t%s\n# Active panel\nPanel\t03\n# Characters\nLead\tMei Lin\nPartner\tXiu Ying\n# Location\nSetting\tTemple Courtyard\n# Shot\nFraming\tMedium\nAngle\tLow\n# Panel objects\nMei Lin\tCharacter\nXiu Ying\tCharacter\nCourtyard\tBackground\nSpeed lines\tEffects\nDialogue\tText\n# Continuity\nStatus\tClear",
    page, project_title, project_chapter);
}

static gchar *
asset_inspector_content_new (const gchar *item)
{
  const gchar *category;

  if (g_strv_contains ((const gchar * const *) project_characters, item))
    category = "Character";
  else if (g_strv_contains ((const gchar * const *) project_locations, item))
    category = "Location";
  else if (g_strv_contains ((const gchar * const *) project_props, item))
    category = "Prop";
  else if (g_strv_contains ((const gchar * const *) project_references, item))
    category = "Reference";
  else
    return NULL;

  return g_strdup_printf (
    "# %s\n# %s\nProject\t%s\nStatus\tAvailable\n# References\nImages\t0",
    item, category, project_title);
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
  gint previous_page;

  if (page < project_first_page ||
      page >= project_first_page + sample_page_count)
    return FALSE;

  previous_page = current_page;
  current_page = page;
  if (!project_state_save (NULL))
    {
      current_page = previous_page;
      return FALSE;
    }
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

static gboolean
project_add_page (void)
{
  gint previous_count = sample_page_count;

  if (sample_page_count < 99)
    sample_page_count++;
  if (!project_state_save (NULL))
    {
      sample_page_count = previous_count;
      return FALSE;
    }

  return update_page_panels (project_first_page + sample_page_count - 1);
}

static gboolean
project_open_dialog (void)
{
  GtkFileChooserNative *dialog;
  GtkFileFilter *filter;
  GFile *file;
  GError *error = NULL;
  gint response;
  gboolean success = TRUE;

  gimp_ui_init ("imanganation-project");
  dialog = gtk_file_chooser_native_new ("Open Manga Project", NULL,
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        "_Open", "_Cancel");
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, "Imanganation project (*.project)");
  gtk_file_filter_add_pattern (filter, "*.project");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (dialog), filter);

  response = gtk_native_dialog_run (GTK_NATIVE_DIALOG (dialog));
  if (response == GTK_RESPONSE_ACCEPT)
    {
      file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (dialog));
      if (file)
        {
          success = project_state_open (file, &error);
          if (success)
            success = update_page_panels (current_page);
          g_object_unref (file);
        }
      else
        {
          success = FALSE;
        }

      if (error)
        {
          gimp_message (error->message);
          g_clear_error (&error);
        }
    }

  g_object_unref (dialog);
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

  if (!strcmp (identifier, "open-project"))
    {
      success = project_open_dialog ();
      return gimp_procedure_new_return_values (
        procedure, success ? GIMP_PDB_SUCCESS : GIMP_PDB_EXECUTION_ERROR, NULL);
    }

  if (!strcmp (identifier, "filmstrip"))
    {
      gint last_page = project_first_page + sample_page_count - 1;
      gint next_page = current_page < last_page ? current_page + 1 : project_first_page;

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
  if (!strcmp (item, "＋ Add Page"))
    {
      success = project_add_page ();
      g_free ((gchar *) item);
      return gimp_procedure_new_return_values (
        procedure, success ? GIMP_PDB_SUCCESS : GIMP_PDB_EXECUTION_ERROR, NULL);
    }

  inspector = asset_inspector_content_new (item);

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
  if (page < project_first_page ||
      page >= project_first_page + sample_page_count)
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
  GError *error = NULL;
  const gchar *actions[] = {
    ACTION_PREFIX "open-project", ACTION_PREFIX "inspector",
    ACTION_PREFIX "filmstrip"
  };
  gboolean project_registered;
  gboolean inspector_registered;
  gboolean filmstrip_registered;
  gchar *project;
  gchar *inspector;
  gchar *filmstrip;
  gchar *project_selection;
  gchar *filmstrip_selection;

  if (!project_state_load (&error))
    {
      if (error)
        {
          gimp_message (error->message);
          g_clear_error (&error);
        }
      g_main_loop_unref (loop);
      return gimp_procedure_new_return_values (
        procedure, GIMP_PDB_EXECUTION_ERROR, NULL);
    }

  project = project_content_new ();
  inspector = inspector_content_new (current_page);
  filmstrip = filmstrip_content_new ();
  project_selection = g_strdup_printf ("●  Page %d", current_page);
  filmstrip_selection = g_strdup_printf ("%d  ●", current_page);

  install_action (plug_in, "open-project");
  install_action (plug_in, "inspector");
  install_action (plug_in, "filmstrip");
  install_select_action (plug_in, "project");
  install_select_action (plug_in, "filmstrip");

  project_registered = call_panel_proc (
    "gimp-extension-panel-register", "project", project,
    "tree", project_selection, "Project", "Open Project…", actions[0],
    SELECT_PREFIX "project");
  inspector_registered = call_panel_proc (
    "gimp-extension-panel-register", "inspector", inspector,
    "properties", "", "Inspector", "Refresh", actions[1], NULL);
  filmstrip_registered = call_panel_proc (
    "gimp-extension-panel-register", "filmstrip", filmstrip,
    "tiles", filmstrip_selection, "Page Filmstrip", "Next page", actions[2],
    SELECT_PREFIX "filmstrip");

  g_free (project);
  g_free (inspector);
  g_free (filmstrip);
  g_free (project_selection);
  g_free (filmstrip_selection);

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
