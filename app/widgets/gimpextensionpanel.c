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

#include <gegl.h>
#include <gtk/gtk.h>

#include "libgimpbase/gimpbasetypes.h"
#define __GIMP_BASE_H_INSIDE__
#include "libgimpbase/gimpvaluearray.h"
#undef __GIMP_BASE_H_INSIDE__
#include "libgimpwidgets/gimpwidgets.h"

#include "plug-in/plug-in-types.h"
#include "widgets/widgets-types.h"

#include "core/core-types.h"
#include "core/gimp.h"
#include "core/gimpcontext.h"
#include "core/gimpviewable.h"
#include "plug-in/gimpplugin.h"
#include "plug-in/gimppluginprocedure.h"

#include "pdb/gimppdb.h"
#include "pdb/gimpprocedure.h"

#include "gimpextensionpanel.h"
#include "gimpdialogfactory.h"
#include "gimp-intl.h"
#include "gimpwindowstrategy.h"


#define EXTENSION_PANEL_MAX_CONTENT_BYTES (1024 * 1024)
#define EXTENSION_PANEL_MAX_CONTENT_ROWS  2048
#define EXTENSION_PANEL_MAX_ROW_BYTES     4096

typedef struct
{
  Gimp        *gimp;
  gchar       *owner;
  gchar       *identifier;
  gchar       *factory_identifier;
  gchar       *title;
  gchar       *content;
  gchar       *presentation;
  gchar       *selected_item;
  gchar       *action_label;
  gchar       *action_procedure;
  gchar       *item_action_procedure;
  GHashTable  *collapsed_tree_paths;
  gint         factory_view_size;
  GtkWidget   *content_box;
  GtkWidget   *action_button;
} GimpExtensionPanel;

static GHashTable *panels;
static GimpDialogFactory *panel_factory;
static gint next_panel_view_size = 10000;

static void panel_register_entry (GimpExtensionPanel *panel);

static gboolean
panel_action_is_valid (Gimp        *gimp,
                       const gchar *owner,
                       const gchar *procedure_name,
                       gboolean     takes_item,
                       GError     **error)
{
  GimpProcedure *procedure;
  GFile *file;
  gchar *procedure_owner;
  gboolean valid;

  if (!procedure_name || !*procedure_name)
    return TRUE;

  procedure = gimp_pdb_lookup_procedure (gimp->pdb, procedure_name);
  if (!procedure || !GIMP_IS_PLUG_IN_PROCEDURE (procedure))
    goto invalid;

  if (procedure->num_args != (takes_item ? 1 : 0) ||
      (takes_item && !G_IS_PARAM_SPEC_STRING (procedure->args[0])))
    goto invalid;

  file = gimp_plug_in_procedure_get_file (
    GIMP_PLUG_IN_PROCEDURE (procedure));
  if (!file)
    goto invalid;

  procedure_owner = g_file_get_basename (file);
#ifdef G_OS_WIN32
  if (g_str_has_suffix (procedure_owner, ".exe"))
    procedure_owner[strlen (procedure_owner) - 4] = '\0';
#endif
  valid = !strcmp (owner, procedure_owner);
  g_free (procedure_owner);
  if (valid)
    return TRUE;

invalid:
  g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
               "Panel action procedure '%s' must belong to its plug-in and "
               "accept %s", procedure_name,
               takes_item ? "one string argument" : "no arguments");
  return FALSE;
}

static gboolean
panel_identifier_is_valid (const gchar *text)
{
  const guchar *p;

  if (!text || !*text)
    return FALSE;
  for (p = (const guchar *) text; *p; p++)
    if (!g_ascii_isalnum (*p) && *p != '-' && *p != '_')
      return FALSE;
  return TRUE;
}

static gboolean
panel_owner_is_valid (const gchar *text)
{
  const guchar *p;

  if (!text || !*text)
    return FALSE;
  for (p = (const guchar *) text; *p; p++)
    if (!g_ascii_isalnum (*p) && *p != '-' && *p != '_' && *p != '.')
      return FALSE;
  return TRUE;
}

static gboolean
panel_content_is_valid (const gchar *content)
{
  const gchar *row_start = content;
  const gchar *p;
  gint rows = 1;

  if (!content || strlen (content) > EXTENSION_PANEL_MAX_CONTENT_BYTES ||
      !g_utf8_validate (content, -1, NULL))
    return FALSE;

  for (p = content; *p; p++)
    if (*p == '\n')
      {
        if (p - row_start > EXTENSION_PANEL_MAX_ROW_BYTES ||
            ++rows > EXTENSION_PANEL_MAX_CONTENT_ROWS)
          return FALSE;
        row_start = p + 1;
      }

  return p - row_start <= EXTENSION_PANEL_MAX_ROW_BYTES;
}

static gchar *
panel_key (const gchar *owner,
           const gchar *identifier)
{
  return g_strdup_printf ("%s\037%s", owner, identifier);
}

static GimpExtensionPanel *
panel_lookup (const gchar *owner,
              const gchar *identifier)
{
  gchar *key;
  GimpExtensionPanel *panel;

  if (!panels)
    return NULL;

  key = panel_key (owner, identifier);
  panel = g_hash_table_lookup (panels, key);
  g_free (key);
  return panel;
}

static void
panel_run_procedure (GimpExtensionPanel *panel,
                     const gchar       *procedure_name,
                     const gchar       *item)
{
  Gimp *gimp = panel->gimp;
  GimpValueArray *return_values;
  GError *error = NULL;

  if (procedure_name && *procedure_name)
    {
      if (item)
        return_values = gimp_pdb_execute_procedure_by_name (
          gimp->pdb, gimp_get_user_context (gimp), NULL, &error,
          procedure_name, G_TYPE_STRING, item, G_TYPE_NONE);
      else
        return_values = gimp_pdb_execute_procedure_by_name (
          gimp->pdb, gimp_get_user_context (gimp), NULL, &error,
          procedure_name, G_TYPE_NONE);
      if (return_values)
        {
          GimpPDBStatusType status =
            g_value_get_enum (gimp_value_array_index (return_values, 0));

          gimp_value_array_unref (return_values);

          if (!error && (status == GIMP_PDB_CALLING_ERROR ||
                         status == GIMP_PDB_EXECUTION_ERROR))
            gimp_message_literal (gimp, NULL, GIMP_MESSAGE_ERROR,
                                  _("The extension panel action failed."));
        }
    }

  if (error)
    {
      gimp_message_literal (gimp, NULL, GIMP_MESSAGE_ERROR,
                            error->message);
      g_clear_error (&error);
    }
}

static void
panel_action (GtkButton *button,
              gpointer   user_data)
{
  GimpExtensionPanel *panel = user_data;

  panel_run_procedure (panel, panel->action_procedure, NULL);
}

static void
panel_item_activated (GimpExtensionPanel *panel,
                      GtkWidget         *row)
{
  const gchar *item_id = g_object_get_data (G_OBJECT (row),
                                             "extension-panel-item-id");
  if (item_id)
    panel_run_procedure (panel, panel->item_action_procedure,
                         item_id);
}

static void
panel_list_row_activated (GtkListBox          *list,
                          GtkListBoxRow       *row,
                          GimpExtensionPanel  *panel)
{
  panel_item_activated (panel, GTK_WIDGET (row));
}

static void
panel_tree_row_activated (GtkTreeView         *tree_view,
                          GtkTreePath         *path,
                          GtkTreeViewColumn   *column,
                          GimpExtensionPanel  *panel)
{
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  GtkTreeIter   iter;

  if (gtk_tree_model_get_iter (model, &iter, path))
    {
      gchar    *item_id = NULL;
      gboolean  heading;

      gtk_tree_model_get (model, &iter, 2, &item_id, 1, &heading, -1);
      if (!heading)
        panel_run_procedure (panel, panel->item_action_procedure, item_id);
      g_free (item_id);
    }
}

static void
panel_tree_cell_data (GtkTreeViewColumn *column,
                      GtkCellRenderer   *renderer,
                      GtkTreeModel      *model,
                      GtkTreeIter       *iter,
                      gpointer           user_data)
{
  gboolean heading;

  gtk_tree_model_get (model, iter, 1, &heading, -1);
  g_object_set (renderer,
                "weight", heading ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
                NULL);
}

static gchar *
panel_tree_iter_key (GtkTreeModel *model,
                     GtkTreeIter  *iter)
{
  GPtrArray *labels = g_ptr_array_new_with_free_func (g_free);
  GtkTreeIter current = *iter;
  GString *key = g_string_new (NULL);
  gint i;

  for (;;)
    {
      gchar *label;
      GtkTreeIter parent;

      gtk_tree_model_get (model, &current, 0, &label, -1);
      g_ptr_array_add (labels, label);

      if (!gtk_tree_model_iter_parent (model, &parent, &current))
        break;
      current = parent;
    }

  for (i = labels->len - 1; i >= 0; i--)
    {
      const gchar *label = g_ptr_array_index (labels, i);

      g_string_append_printf (key, "%" G_GSIZE_FORMAT ":%s/",
                              strlen (label), label);
    }

  g_ptr_array_unref (labels);
  return g_string_free (key, FALSE);
}

static void
panel_tree_state_changed (GtkTreeView        *tree_view,
                          GtkTreeIter        *iter,
                          GtkTreePath        *path,
                          GimpExtensionPanel *panel)
{
  GtkTreeModel *model = gtk_tree_view_get_model (tree_view);
  gchar *key = panel_tree_iter_key (model, iter);

  if (gtk_tree_view_row_expanded (tree_view, path))
    g_hash_table_remove (panel->collapsed_tree_paths, key);
  else
    g_hash_table_add (panel->collapsed_tree_paths, g_strdup (key));

  g_free (key);
}

static void
panel_tree_restore_collapsed (GimpExtensionPanel *panel,
                              GtkTreeView        *tree_view,
                              GtkTreeModel       *model,
                              GtkTreeIter        *parent)
{
  GtkTreeIter iter;
  gboolean    valid;

  valid = parent ? gtk_tree_model_iter_children (model, &iter, parent) :
                   gtk_tree_model_get_iter_first (model, &iter);

  while (valid)
    {
      gboolean heading;

      gtk_tree_model_get (model, &iter, 1, &heading, -1);
      if (heading)
        {
          GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
          gchar *key = panel_tree_iter_key (model, &iter);

          if (g_hash_table_contains (panel->collapsed_tree_paths, key))
            gtk_tree_view_collapse_row (tree_view, path);
          g_free (key);
          gtk_tree_path_free (path);

          panel_tree_restore_collapsed (panel, tree_view, model, &iter);
        }

      valid = gtk_tree_model_iter_next (model, &iter);
    }
}

static gboolean
panel_tree_has_collapsed_ancestor (GimpExtensionPanel *panel,
                                   GtkTreeModel       *model,
                                   GtkTreeIter        *iter)
{
  GtkTreeIter parent;

  while (gtk_tree_model_iter_parent (model, &parent, iter))
    {
      gchar *key = panel_tree_iter_key (model, &parent);
      gboolean collapsed = g_hash_table_contains (panel->collapsed_tree_paths,
                                                   key);

      g_free (key);
      if (collapsed)
        return TRUE;

      *iter = parent;
    }

  return FALSE;
}

static gboolean
panel_content_has_item (const gchar *content,
                        const gchar *presentation,
                        const gchar *item)
{
  gchar **items = g_strsplit (content, "\n", -1);
  gboolean found = FALSE;
  gint i;

  for (i = 0; items[i]; i++)
    {
      const gchar *label = items[i];
      const gchar *tab;

      if (!strcmp (presentation, "tree"))
        {
          while (*label == '\t')
            label++;
          if (g_str_has_prefix (label, "# "))
            continue;
        }

      tab = strchr (label, '\t');

      if (tab ? (tab - label == strlen (item) &&
                 !strncmp (label, item, tab - label)) :
                !strcmp (label, item))
        {
          found = TRUE;
          break;
        }
    }

  g_strfreev (items);
  return found;
}

static void
panel_tile_activated (GtkFlowBox         *flow,
                      GtkFlowBoxChild    *child,
                      GimpExtensionPanel *panel)
{
  GtkWidget *frame = gtk_bin_get_child (GTK_BIN (child));
  if (frame)
    panel_item_activated (panel, frame);
}

static GtkWidget *
panel_create_tree_content (GimpExtensionPanel *panel,
                           gchar             **items)
{
  GtkTreeStore *store = gtk_tree_store_new (3, G_TYPE_STRING, G_TYPE_BOOLEAN,
                                            G_TYPE_STRING);
  GtkWidget *tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  GtkCellRenderer *renderer = gtk_cell_renderer_text_new ();
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes (
    "", renderer, "text", 0, NULL);
  GtkTreePath *selected_path = NULL;
  GtkTreeIter parents[64];
  gboolean    parent_valid[64] = { FALSE };
  gint i;

  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree), FALSE);
  gtk_tree_view_set_activate_on_single_click (GTK_TREE_VIEW (tree), TRUE);
  gtk_tree_view_column_set_cell_data_func (column, renderer,
                                           panel_tree_cell_data, NULL, NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (tree), column);
  g_signal_connect (tree, "row-activated",
                    G_CALLBACK (panel_tree_row_activated), panel);

  for (i = 0; items[i]; i++)
    {
      const gchar *text = items[i];
      gint depth = 0;
      gboolean heading;
      gchar *item_id = NULL;
      GtkTreeIter iter;
      GtkTreeIter *parent;
      gint level;

      while (*text == '\t')
        {
          text++;
          depth++;
        }
      depth = MIN (depth, G_N_ELEMENTS (parents) - 1);
      while (depth > 0 && !parent_valid[depth - 1])
        depth--;

      heading = g_str_has_prefix (text, "# ");
      if (heading)
        text += 2;

      if (!heading)
        {
          const gchar *tab = strchr (text, '\t');
          if (tab)
            {
              item_id = g_strndup (text, tab - text);
              text = tab + 1;
            }
          else
            item_id = g_strdup (text);
        }

      parent = depth > 0 ? &parents[depth - 1] : NULL;
      gtk_tree_store_append (store, &iter, parent);
      gtk_tree_store_set (store, &iter,
                          0, text,
                          1, heading,
                          2, item_id,
                          -1);
      g_free (item_id);

      for (level = depth + 1; level < G_N_ELEMENTS (parent_valid); level++)
        parent_valid[level] = FALSE;
      parent_valid[depth] = heading;
      if (heading)
        parents[depth] = iter;

      {
        gchar *row_id = NULL;
        gtk_tree_model_get (GTK_TREE_MODEL (store), &iter, 2, &row_id, -1);
        if (!heading && row_id && !strcmp (row_id, panel->selected_item))
          selected_path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &iter);
        g_free (row_id);
      }
    }

  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree));
  panel_tree_restore_collapsed (panel, GTK_TREE_VIEW (tree),
                                GTK_TREE_MODEL (store), NULL);
  g_signal_connect (tree, "row-expanded",
                    G_CALLBACK (panel_tree_state_changed), panel);
  g_signal_connect (tree, "row-collapsed",
                    G_CALLBACK (panel_tree_state_changed), panel);
  if (selected_path)
    {
      GtkTreeIter selected_iter;
      gboolean hidden;

      gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &selected_iter,
                               selected_path);
      hidden = panel_tree_has_collapsed_ancestor (
        panel, GTK_TREE_MODEL (store), &selected_iter);

      if (hidden)
        gtk_tree_selection_select_path (
          gtk_tree_view_get_selection (GTK_TREE_VIEW (tree)), selected_path);
      else
        {
          gtk_tree_view_set_cursor (GTK_TREE_VIEW (tree), selected_path,
                                    NULL, FALSE);
          gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (tree), selected_path,
                                        NULL, FALSE, 0.0, 0.0);
        }
      gtk_tree_path_free (selected_path);
    }

  g_object_unref (store);
  return tree;
}

static GtkWidget *
panel_create_content (GimpExtensionPanel *panel)
{
  gchar **items = g_strsplit (panel->content, "\n", -1);
  GtkWidget *container;
  gint i;

  if (!strcmp (panel->presentation, "properties"))
    {
      GtkWidget *grid = gtk_grid_new ();
      gint row = 0;

      gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
      gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
      container = grid;

      for (i = 0; items[i]; i++)
        {
          gchar *separator;
          GtkWidget *label;

          if (g_str_has_prefix (items[i], "# "))
            {
              label = gtk_label_new (items[i] + 2);
              gtk_label_set_xalign (GTK_LABEL (label), 0.0);
              gtk_style_context_add_class (gtk_widget_get_style_context (label),
                                           "heading");
              gtk_widget_set_margin_top (label, row ? 8 : 2);
              gtk_grid_attach (GTK_GRID (grid), label, 0, row++, 2, 1);
              continue;
            }

          separator = strchr (items[i], '\t');
          if (separator)
            {
              GtkWidget *name = gtk_label_new (NULL);
              GtkWidget *value = gtk_label_new (NULL);

              *separator = '\0';
              gtk_label_set_text (GTK_LABEL (name), items[i]);
              gtk_label_set_text (GTK_LABEL (value), separator + 1);
              gtk_label_set_xalign (GTK_LABEL (name), 0.0);
              gtk_label_set_xalign (GTK_LABEL (value), 0.0);
              gtk_label_set_line_wrap (GTK_LABEL (value), TRUE);
              gtk_style_context_add_class (gtk_widget_get_style_context (name),
                                           GTK_STYLE_CLASS_DIM_LABEL);
              gtk_grid_attach (GTK_GRID (grid), name, 0, row, 1, 1);
              gtk_grid_attach (GTK_GRID (grid), value, 1, row++, 1, 1);
            }
          else
            {
              label = gtk_label_new (items[i]);
              gtk_label_set_xalign (GTK_LABEL (label), 0.0);
              gtk_grid_attach (GTK_GRID (grid), label, 0, row++, 2, 1);
            }
      }
    }
  else if (!strcmp (panel->presentation, "tree"))
    {
      container = panel_create_tree_content (panel, items);
    }
  else if (!strcmp (panel->presentation, "tiles"))
    {
      GtkWidget *flow = gtk_flow_box_new ();
      GtkFlowBoxChild *selected_child = NULL;

      gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (flow), GTK_SELECTION_SINGLE);
      gtk_flow_box_set_activate_on_single_click (GTK_FLOW_BOX (flow), TRUE);
      g_signal_connect (flow, "child-activated",
                        G_CALLBACK (panel_tile_activated), panel);
      gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (flow), 6);
      gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (flow), 2);
      gtk_flow_box_set_row_spacing (GTK_FLOW_BOX (flow), 6);
      gtk_flow_box_set_column_spacing (GTK_FLOW_BOX (flow), 6);
      container = flow;

      for (i = 0; items[i]; i++)
        {
          GtkWidget *frame = gtk_frame_new (NULL);
          GtkWidget *tile = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
          GtkWidget *thumbnail = gtk_image_new_from_icon_name (
            "image-x-generic", GTK_ICON_SIZE_DIALOG);
          const gchar *text = items[i];
          const gchar *tab = strchr (text, '\t');
          gchar *item_id;
          GtkWidget *label;
          GdkPixbuf *pixbuf = NULL;
          if (tab)
            {
              item_id = g_strndup (text, tab - text);
              text = tab + 1;
              tab = strchr (text, '\t');
              if (tab)
                {
                  gchar *preview_path = g_strdup (tab + 1);

                  *((gchar *) tab) = '\0';
                  if (g_path_is_absolute (preview_path))
                    {
                      GError *error = NULL;

                      pixbuf = gdk_pixbuf_new_from_file_at_scale (
                        preview_path, 96, 96, TRUE, &error);
                      g_clear_error (&error);
                    }
                  g_free (preview_path);
                }
            }
          else
            item_id = g_strdup (text);
          label = gtk_label_new (text);
          gtk_widget_set_size_request (frame, 86, 112);
          gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
          gtk_label_set_xalign (GTK_LABEL (label), 0.5);
          if (pixbuf)
            {
              gtk_image_set_from_pixbuf (GTK_IMAGE (thumbnail), pixbuf);
              g_object_unref (pixbuf);
            }
          gtk_widget_set_size_request (thumbnail, 80, 80);
          gtk_box_pack_start (GTK_BOX (tile), thumbnail, TRUE, TRUE, 0);
          gtk_box_pack_end (GTK_BOX (tile), label, FALSE, FALSE, 0);
          gtk_container_add (GTK_CONTAINER (frame), tile);
          g_object_set_data_full (G_OBJECT (frame), "extension-panel-item-id",
                                  item_id, g_free);
          g_object_set_data (G_OBJECT (frame), "extension-panel-item-label", label);
          gtk_container_add (GTK_CONTAINER (flow), frame);
          if (!strcmp (item_id, panel->selected_item))
            selected_child = GTK_FLOW_BOX_CHILD (gtk_widget_get_parent (frame));
        }
      if (selected_child)
        gtk_flow_box_select_child (GTK_FLOW_BOX (flow), selected_child);
    }
  else
    {
      GtkWidget *list = gtk_list_box_new ();
      GtkListBoxRow *selected_row = NULL;

      gtk_list_box_set_selection_mode (GTK_LIST_BOX (list), GTK_SELECTION_SINGLE);
      gtk_list_box_set_activate_on_single_click (GTK_LIST_BOX (list), TRUE);
      g_signal_connect (list, "row-activated",
                        G_CALLBACK (panel_list_row_activated), panel);
      container = list;

      for (i = 0; items[i]; i++)
        {
          GtkWidget *row = gtk_list_box_row_new ();
          const gchar *text = items[i];
          const gchar *tab = strchr (text, '\t');
          gchar *item_id;
          GtkWidget *label;

          if (tab)
            {
              item_id = g_strndup (text, tab - text);
              text = tab + 1;
            }
          else
            item_id = g_strdup (text);
          label = gtk_label_new (text);
          gtk_label_set_xalign (GTK_LABEL (label), 0.0);
          gtk_widget_set_margin_start (label, 8);
          gtk_widget_set_margin_end (label, 8);
          gtk_widget_set_margin_top (label, 5);
          gtk_widget_set_margin_bottom (label, 5);
          gtk_container_add (GTK_CONTAINER (row), label);
          g_object_set_data_full (G_OBJECT (row), "extension-panel-item-id",
                                  item_id, g_free);
          gtk_container_add (GTK_CONTAINER (list), row);
          if (!strcmp (item_id, panel->selected_item))
            selected_row = GTK_LIST_BOX_ROW (row);
        }
      if (selected_row)
        gtk_list_box_select_row (GTK_LIST_BOX (list), selected_row);
    }

  g_strfreev (items);
  return container;
}

static GtkWidget *
panel_create_scrolled_content (GimpExtensionPanel *panel)
{
  GtkWidget *scrolled = gtk_scrolled_window_new (NULL, NULL);
  GtkWidget *content = panel_create_content (panel);

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
                                  GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_max_content_height (GTK_SCROLLED_WINDOW (scrolled),
                                              420);
  gtk_container_add (GTK_CONTAINER (scrolled), content);

  return scrolled;
}

static GtkWidget *
panel_new (GimpDialogFactory *factory,
           GimpContext      *context,
           GimpUIManager    *ui_manager,
           gint              view_size,
           gpointer          user_data)
{
  GimpExtensionPanel *panel = user_data;
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  GtkWidget *content = panel_create_scrolled_content (panel);

  gtk_container_set_border_width (GTK_CONTAINER (box), 6);
  gtk_box_pack_start (GTK_BOX (box), content, TRUE, TRUE, 0);

  panel->content_box = content;
  if (panel->action_label && *panel->action_label)
    {
      GtkWidget *button = gtk_button_new_with_label (panel->action_label);
      g_signal_connect (button, "clicked", G_CALLBACK (panel_action), panel);
      gtk_box_pack_end (GTK_BOX (box), button, FALSE, FALSE, 0);
      panel->action_button = button;
    }

  gtk_widget_show_all (box);
  return box;
}

static GtkWidget *
panel_new_generic (GimpDialogFactory *factory, GimpContext *context,
                   GimpUIManager *ui_manager, gint view_size)
{
  GimpExtensionPanel *panel = NULL;
  GList *values;
  GList *iter;

  values = panels ? g_hash_table_get_values (panels) : NULL;
  for (iter = values; iter; iter = iter->next)
    {
      GimpExtensionPanel *candidate = iter->data;
      if (candidate->factory_view_size == view_size)
        {
          panel = candidate;
          break;
        }
    }
  g_list_free (values);

  return panel ? panel_new (factory, context, ui_manager, view_size, panel) :
                 gtk_label_new ("Extension panel unavailable");
}

static void
panel_free (GimpExtensionPanel *panel)
{
  g_free (panel->owner);
  g_free (panel->identifier);
  g_free (panel->factory_identifier);
  g_free (panel->title);
  g_free (panel->content);
  g_free (panel->presentation);
  g_free (panel->selected_item);
  g_free (panel->action_label);
  g_free (panel->action_procedure);
  g_free (panel->item_action_procedure);
  g_hash_table_unref (panel->collapsed_tree_paths);
  g_free (panel);
}

void
gimp_extension_panel_dialogs_init (GimpDialogFactory *factory)
{
  GList *values;
  GList *iter;

  panel_factory = factory;
  if (! panels)
    panels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                    (GDestroyNotify) panel_free);

  values = g_hash_table_get_values (panels);
  for (iter = values; iter; iter = iter->next)
    panel_register_entry (iter->data);
  g_list_free (values);
}

void
gimp_extension_panel_show_unrestored (Gimp *gimp)
{
  GList *values = panels ? g_hash_table_get_values (panels) : NULL;
  GList *iter;

  if (!panel_factory)
    {
      g_list_free (values);
      return;
    }

  for (iter = values; iter; iter = iter->next)
    {
      GimpExtensionPanel *panel = iter->data;
      /* Show newly registered panels once, but respect a saved closed state. */
      if (!gimp_dialog_factory_find_session_info (panel_factory,
                                                  panel->factory_identifier))
        gimp_extension_panel_show (gimp, panel->owner, panel->identifier, NULL);
    }
  g_list_free (values);
}

void
gimp_extension_panel_plugin_closed (GimpPlugInManager *manager,
                                    GimpPlugIn        *plug_in,
                                    gpointer           user_data)
{
  gchar *owner;
  GList *keys;
  GList *iter;

  if (!panels)
    return;

  owner = gimp_extension_panel_get_owner (plug_in);
  if (!owner)
    return;
  keys = g_hash_table_get_keys (panels);
  for (iter = keys; iter; iter = iter->next)
    {
      GimpExtensionPanel *panel = g_hash_table_lookup (panels, iter->data);
      if (!strcmp (panel->owner, owner))
        {
          GtkWidget *widget = panel_factory ?
            gimp_dialog_factory_find_widget (panel_factory,
                                             panel->factory_identifier) : NULL;
          if (widget)
            gtk_widget_destroy (widget);
          if (panel_factory)
            gimp_dialog_factory_unregister_entry (panel_factory,
                                                  panel->factory_identifier);
          g_hash_table_remove (panels, iter->data);
        }
    }
  g_list_free (keys);
  g_free (owner);
}

gchar *
gimp_extension_panel_get_owner (GimpPlugIn *plug_in)
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

static void
panel_register_entry (GimpExtensionPanel *panel)
{
  if (!panel_factory ||
      gimp_dialog_factory_find_entry (panel_factory,
                                      panel->factory_identifier))
    return;

  gimp_dialog_factory_register_entry (panel_factory,
                                      panel->factory_identifier,
                                      panel->title, panel->title,
                                      NULL, NULL, panel_new_generic, NULL,
                                      panel->factory_view_size, TRUE, TRUE,
                                      FALSE, TRUE, TRUE, FALSE, TRUE);
}

gboolean
gimp_extension_panel_register (Gimp        *gimp,
                               const gchar *owner,
                               const gchar *identifier,
                               const gchar *title,
                               const gchar *content,
                               const gchar *presentation,
                               const gchar *selected_item,
                               const gchar *action_label,
                               const gchar *action_procedure,
                               const gchar *item_action_procedure,
                               GError     **error)
{
  GimpExtensionPanel *panel;
  gchar *key;
  gboolean newly_registered;

  if (!panel_owner_is_valid (owner) ||
      !panel_identifier_is_valid (identifier) || strlen (identifier) > 64 ||
      !title || !content || !presentation || !selected_item ||
      !action_label || !action_procedure ||
      !item_action_procedure ||
      strlen (title) > 256 ||
      strlen (action_label) > 128 ||
      strlen (action_procedure) > 256 ||
      strlen (item_action_procedure) > 256 ||
      strlen (selected_item) > EXTENSION_PANEL_MAX_ROW_BYTES ||
      (strcmp (presentation, "list") && strcmp (presentation, "tree") &&
       strcmp (presentation, "tiles") &&
       strcmp (presentation, "properties")) ||
      !g_utf8_validate (title, -1, NULL) ||
      !panel_content_is_valid (content) ||
      !g_utf8_validate (selected_item, -1, NULL))
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                           "Invalid extension panel identity or text");
      return FALSE;
    }

  if (!panel_action_is_valid (gimp, owner, action_procedure, FALSE, error) ||
      !panel_action_is_valid (gimp, owner, item_action_procedure, TRUE, error))
    return FALSE;

  if (*selected_item)
    {
      if (!panel_content_has_item (content, presentation, selected_item))
        {
          g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                               "Selected item is not present in panel content");
          return FALSE;
        }
    }

  if (!panels)
    panels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
                                    (GDestroyNotify) panel_free);
  panel = panel_lookup (owner, identifier);
  newly_registered = panel == NULL;
  if (!panel)
    {
      panel = g_new0 (GimpExtensionPanel, 1);
      panel->gimp = gimp;
      panel->owner = g_strdup (owner);
      panel->identifier = g_strdup (identifier);
      /* ':' is excluded from both validated identifiers, so the pair stays
       * unique even when either component contains hyphens. */
      panel->factory_identifier = g_strdup_printf ("extension-panel-%s::%s",
                                                   owner, identifier);
      panel->factory_view_size = next_panel_view_size++;
      panel->collapsed_tree_paths = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                           g_free, NULL);
      key = panel_key (owner, identifier);
      g_hash_table_insert (panels, key, panel);
    }
  g_free (panel->title);
  g_free (panel->content);
  g_free (panel->presentation);
  g_free (panel->selected_item);
  g_free (panel->action_label);
  g_free (panel->action_procedure);
  g_free (panel->item_action_procedure);
  panel->title = g_strdup (title);
  panel->content = g_strdup (content);
  panel->presentation = g_strdup (presentation);
  panel->selected_item = g_strdup (selected_item);
  panel->action_label = g_strdup (action_label);
  panel->action_procedure = g_strdup (action_procedure);
  panel->item_action_procedure = g_strdup (item_action_procedure);

  if (panel_factory)
    {
      panel_register_entry (panel);

      /* When registration happens after dialog-factory restoration (for example,
       * a Python extension started from its menu), show only a genuinely new dock.
       * Existing session info records both placement and the user's closed state. */
      if (newly_registered &&
          !gimp_dialog_factory_find_session_info (panel_factory,
                                                  panel->factory_identifier))
        {
          GError *show_error = NULL;

          if (!gimp_extension_panel_show (gimp, owner, identifier, &show_error))
            {
              g_warning ("Could not show newly registered extension panel: %s",
                         show_error ? show_error->message : "unknown error");
              g_clear_error (&show_error);
            }
        }
    }

  return TRUE;
}

gboolean
gimp_extension_panel_show (Gimp        *gimp,
                           const gchar *owner,
                           const gchar *identifier,
                           GError     **error)
{
  GimpExtensionPanel *panel = panel_lookup (owner, identifier);

  if (!panel || !panel_factory)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                           "Extension panel is not registered");
      return FALSE;
    }

  panel_register_entry (panel);
  gimp_window_strategy_show_dockable_dialog (
    GIMP_WINDOW_STRATEGY (gimp_get_window_strategy (gimp)), gimp,
    panel_factory, gimp_get_monitor_at_pointer (), panel->factory_identifier);
  return TRUE;
}

gboolean
gimp_extension_panel_update (Gimp        *gimp,
                             const gchar *owner,
                             const gchar *identifier,
                             const gchar *content,
                             const gchar *selected_item,
                             GError     **error)
{
  GimpExtensionPanel *panel = panel_lookup (owner, identifier);
  GtkWidget *dockable;

  if (!panel)
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_NOENT,
                           "Extension panel is not registered");
      return FALSE;
    }

  if (!panel_content_is_valid (content) || !selected_item ||
      strlen (selected_item) > EXTENSION_PANEL_MAX_ROW_BYTES ||
      !g_utf8_validate (selected_item, -1, NULL))
    {
      g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                           "Invalid extension panel content");
      return FALSE;
    }

  if (*selected_item)
    {
      if (!panel_content_has_item (content, panel->presentation,
                                   selected_item))
        {
          g_set_error_literal (error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                               "Selected item is not present in panel content");
          return FALSE;
        }
    }

  g_free (panel->content);
  g_free (panel->selected_item);
  panel->content = g_strdup (content);
  panel->selected_item = g_strdup (selected_item);
  dockable = panel_factory ? gimp_dialog_factory_find_widget (
    panel_factory, panel->factory_identifier) : NULL;
  if (dockable)
    {
      GtkWidget *old_content = panel->content_box;
      GtkAdjustment *hadjustment = gtk_scrolled_window_get_hadjustment (
        GTK_SCROLLED_WINDOW (old_content));
      GtkAdjustment *vadjustment = gtk_scrolled_window_get_vadjustment (
        GTK_SCROLLED_WINDOW (old_content));
      gdouble hvalue = gtk_adjustment_get_value (hadjustment);
      gdouble vvalue = gtk_adjustment_get_value (vadjustment);
      GtkWidget *new_content = panel_create_scrolled_content (panel);
      GtkWidget *parent = gtk_widget_get_parent (old_content);

      if (parent)
        {
          gtk_container_remove (GTK_CONTAINER (parent), old_content);
          gtk_box_pack_start (GTK_BOX (parent), new_content, TRUE, TRUE, 0);
          gtk_box_reorder_child (GTK_BOX (parent), new_content, 0);
          gtk_widget_show_all (new_content);
          panel->content_box = new_content;
          gtk_adjustment_set_value (
            gtk_scrolled_window_get_hadjustment (
              GTK_SCROLLED_WINDOW (new_content)), hvalue);
          gtk_adjustment_set_value (
            gtk_scrolled_window_get_vadjustment (
              GTK_SCROLLED_WINDOW (new_content)), vvalue);
        }
    }
  return TRUE;
}

gboolean
gimp_extension_panel_unregister (Gimp        *gimp,
                                 const gchar *owner,
                                 const gchar *identifier,
                                 GError     **error)
{
  GimpExtensionPanel *panel = panel_lookup (owner, identifier);
  gchar *key;

  if (!panel)
    return TRUE;
  if (panel_factory)
    {
      GtkWidget *widget = gimp_dialog_factory_find_widget (
        panel_factory, panel->factory_identifier);
      if (widget)
        gtk_widget_destroy (widget);
      gimp_dialog_factory_unregister_entry (panel_factory,
                                            panel->factory_identifier);
    }
  key = panel_key (owner, identifier);
  g_hash_table_remove (panels, key);
  g_free (key);
  return TRUE;
}
