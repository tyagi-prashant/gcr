/*
 * gnome-keyring
 *
 * Copyright (C) 2008 Stefan Walter
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gcr/gcr-icons.h"
#include "gcr/gcr-parser.h"
#include "gcr/gcr.h"
#include "gcr-dialog-util.h"
#include "gcr-secure-entry-buffer.h"
#include "gcr-certificate-chooser-dialog.h"
#include "gcr-certificate-chooser-pkcs11.c"
#include "gcr-viewer.h"
#include "gcr-viewer-widget.h"
#include "gcr-unlock-renderer.h"
#include "egg/egg-secure-memory.h"
#include <gtk/gtk.h>
#include "gcr-failure-renderer.h"

#include <glib/gi18n-lib.h>
#include <gdk/gdkx.h>

#include <unistd.h>
#include <string.h>
#include <errno.h>
/**
 * SECTION:gcr-certificate-chooser-dialog
 * @title: GcrCertificateChooserDialog
 * @short_description: A dialog which allows selection of personal certificates
 *
 * A dialog which guides the user through selection of a certificate and
 * corresponding private key, located in files or PKCS\#11 tokens.
 */

/**
 * GcrCertificateChooserDialog:
 *
 * A certificate chooser dialog object.
 */

/**
 * GcrCertificateChooserDialogClass:
 *
 * Class for #GcrCertificateChooserDialog
 */
#define GCR_CERTIFICATE_CHOOSER_DIALOG_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_CERTIFICATE_CHOOSER_DIALOG, GcrCertificateChooserDialogClass))
#define GCR_IS_CERTIFICATE_CHOOSER_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_CERTIFICATE_CHOOSER_DIALOG))
#define GCR_CERTIFICATE_CHOOSER_DIALOG_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_CERTIFICATE_CHOOSER_DIALOG, GcrCertificateChooserDialogClass))

#define GCR_CERTIFICATE_CHOOSER_SIDEBAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_CERTIFICATE_CHOOSER_SIDEBAR, GcrCertificateChooserSidebarClass))
#define GCR_IS_CERTIFICATE_CHOOSER_SIDEBAR_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_CERTIFICATE_CHOOSER_SIDEBAR))
#define GCR_CERTIFICATE_CHOOSER_SIDEBAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_CERTIFICATE_CHOOSER_SIDEBAR, GcrCertificateChooserSidebarClass))

struct _GcrCertificateChooserDialog {
       GtkDialog parent;
       GList *slots;
       GtkListStore *store;
       GList *blacklist;
       GtkBuilder *builder;
       GtkWidget *hbox;
       GBytes *data;
       GcrCertificateChooserSidebar *token_sidebar;
       char *certificate_uri;
       char *key_uri;
       GcrParser *parser;
       gboolean is_certificate_choosen;
       gboolean is_key_choosen;
       gint password_wrong_count;
};

struct _GcrCertificateChooserSidebar {
       GtkScrolledWindow parent;
       GtkWidget *tree_view;
};

struct _GcrCertificateChooserSidebarClass {
       GtkScrolledWindowClass parent_class;
};

typedef struct _GcrCertificateChooserDialogClass GcrCertificateChooserDialogClass;
typedef struct _GcrCertificateChooserSidebarClass GcrCertificateChooserSidebarClass;

struct _GcrCertificateChooserDialogClass {
	GtkDialogClass parent;
};

static const char *token_blacklist[] = { 
        "pkcs11:manufacturer=Gnome%20Keyring;serial=1:SSH:HOME",
        "pkcs11:manufacturer=Gnome%20Keyring;serial=1:SECRET:MAIN",
        "pkcs11:manufacturer=Mozilla%20Foundation;token=NSS%20Generic%20Crypto%20Services",
         NULL
};

static const gchar *file_system = "file-system";

static const char *no_key = "No key selected yet";

static const char *no_certificate = "No certificate selected yet";

typedef enum {
        TYPE_PKCS11,
        TYPE_FILE_SYSTEM
} RowType;

enum {
        ROW_TYPE,
        ROW_LABEL,
        ROW_SLOT,
        ROW_WIDGET,
        N_COLUMNS
};

G_DEFINE_TYPE (GcrCertificateChooserDialog, gcr_certificate_chooser_dialog, GTK_TYPE_DIALOG);
G_DEFINE_TYPE (GcrCertificateChooserSidebar, gcr_certificate_chooser_sidebar, GTK_TYPE_SCROLLED_WINDOW);

static void
gcr_certificate_chooser_sidebar_init (GcrCertificateChooserSidebar *self)
{

}

static void
get_token_label (GtkTreeViewColumn *column,
                 GtkCellRenderer *cell,
                 GtkTreeModel *model,
                 GtkTreeIter *iter,
                 gpointer user_data)
{
        GckSlot *slot;
        GckTokenInfo *info;
        RowType type;

        gtk_tree_model_get (model, iter, ROW_TYPE, &type, -1);
        if (type == TYPE_FILE_SYSTEM) {

                 g_object_set(cell,
                              "visible", TRUE,
                              "text", file_system,
                              NULL);
        } else {
                 gtk_tree_model_get (model, iter, ROW_SLOT, &slot, -1);
                 if (slot == NULL)
                       return ;
                 info = gck_slot_get_token_info (slot);

                 g_object_set(cell,
                              "visible", TRUE,
                              "text", info->label,
                              NULL);
        }
}

static void
update_file_chooser_filefilter (GcrCertificateChooserDialog *self)
{
        if (self->is_certificate_choosen) {

                 gtk_file_chooser_remove_filter(GTK_FILE_CHOOSER (gtk_builder_get_object (self->builder, "filechooser")),
                                                 gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (gtk_builder_get_object (self->builder, "filechooser"))));

                 gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (gtk_builder_get_object (self->builder, "filechooser")),
                                              GTK_FILE_FILTER (gtk_builder_get_object (self->builder, "key-filefilter")));
        } else {
                 gtk_file_chooser_remove_filter(GTK_FILE_CHOOSER (gtk_builder_get_object (self->builder, "filechooser")),
                                                 gtk_file_chooser_get_filter (GTK_FILE_CHOOSER (gtk_builder_get_object (self->builder, "filechooser"))));

                 gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (gtk_builder_get_object (self->builder, "filechooser")),
                                              GTK_FILE_FILTER (gtk_builder_get_object (self->builder, "cert-filefilter")));
        }
}

static void
on_token_select (GtkTreeModel *model,
                GtkTreePath *path,
                GtkTreeIter *iter,
                gpointer user_data)
{

        GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG(user_data);
        GckSlot *slot;
        RowType type;
        gboolean certificate_choosen = FALSE;
        GtkWidget *widget, *child2;

        gtk_tree_model_get (model, iter, ROW_TYPE, &type, -1);

        gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                           self->builder, "key-label")),
                           no_key);
        if (self->is_certificate_choosen )
                 certificate_choosen = TRUE;
        else {
                 gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                    self->builder, "certificate-label")),
                                    no_certificate);
        }

        child2 = gtk_paned_get_child2 (GTK_PANED(gtk_builder_get_object(self->builder, "selection-page")));
        if (child2 != NULL)
                 gtk_container_remove (GTK_CONTAINER (gtk_builder_get_object(self->builder, "selection-page")), child2);

        if (type == TYPE_FILE_SYSTEM) {
                 gtk_tree_model_get (model, iter, ROW_WIDGET, &widget, -1);

                 if (widget == NULL) {
                          gtk_paned_pack2(GTK_PANED(gtk_builder_get_object(self->builder, "selection-page")),
                                          GTK_WIDGET (gtk_builder_get_object (self->builder, "filechooser")),
                                          FALSE, FALSE);

                          gtk_list_store_set (GTK_LIST_STORE(model), iter,
                                              ROW_WIDGET,GTK_WIDGET (gtk_builder_get_object (self->builder, "filechooser")), -1);

                 } else {
                          gtk_paned_pack2(GTK_PANED(gtk_builder_get_object(self->builder, "selection-page")),
                                          widget, FALSE, FALSE);
                 }
                update_file_chooser_filefilter (self);

        } else {
                 gtk_tree_model_get (model, iter, ROW_WIDGET, &widget, -1);
                 gtk_tree_model_get (model, iter, ROW_SLOT, &slot, -1);

                 if (widget == NULL) {

                          GcrCertificateChooserPkcs11 *data = gcr_certificate_chooser_pkcs11_new (slot, certificate_choosen);
                          data->builder = self->builder;

                          gtk_list_store_set (GTK_LIST_STORE(model), iter,
                                              ROW_WIDGET,GTK_WIDGET (data), -1);

                          gtk_paned_add2(GTK_PANED(gtk_builder_get_object(self->builder, "selection-page")), GTK_WIDGET(data));
                          gtk_widget_show_all(GTK_WIDGET(gtk_builder_get_object(self->builder, "selection-page")));
                 } else {
                          GCR_CERTIFICATE_CHOOSER_PKCS11 (widget)->certificate_choosen = certificate_choosen;
                          update_object (GCR_CERTIFICATE_CHOOSER_PKCS11 (widget));

                          gtk_paned_add2(GTK_PANED(gtk_builder_get_object(self->builder, "selection-page")), GTK_WIDGET(widget));
                          gtk_widget_show_all(GTK_WIDGET(gtk_builder_get_object(self->builder, "selection-page")));
                 }
        }
}

static void
on_token_selection_changed (GtkTreeSelection *selection,
                                gpointer data)
{

        GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG(data);
        gtk_tree_selection_selected_foreach (selection, on_token_select, self);
}

static void
gcr_certificate_chooser_sidebar_constructed (GObject *obj)
{
        GcrCertificateChooserSidebar *self = GCR_CERTIFICATE_CHOOSER_SIDEBAR(obj);
        GtkTreeViewColumn *col;
        GtkCellRenderer *cell;

        G_OBJECT_CLASS (gcr_certificate_chooser_sidebar_parent_class)->constructed (obj);

        self->tree_view = gtk_tree_view_new();
        col = gtk_tree_view_column_new ();

        cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, TRUE);
	g_object_set (G_OBJECT (cell), "editable", FALSE, NULL);
	gtk_tree_view_column_set_cell_data_func (col, cell,
	                                         get_token_label,
	                                         self, NULL);

	g_object_set (cell,
	              "ellipsize", PANGO_ELLIPSIZE_END,
	              "ellipsize-set", TRUE,
	              NULL);

        gtk_tree_view_append_column (GTK_TREE_VIEW(self->tree_view), col);
        gtk_tree_view_column_set_max_width (GTK_TREE_VIEW_COLUMN (col), 12);
        gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->tree_view));
        gtk_widget_show (GTK_WIDGET (self->tree_view));
}

static void
gcr_certificate_chooser_sidebar_class_init(GcrCertificateChooserSidebarClass *klass)
{

        GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
        gobject_class->constructed = gcr_certificate_chooser_sidebar_constructed;

}

GcrCertificateChooserSidebar *
gcr_certificate_chooser_sidebar_new ()
{

        GcrCertificateChooserSidebar *sidebar;
        sidebar = g_object_new(GCR_TYPE_CERTIFICATE_CHOOSER_SIDEBAR,
                            NULL);

        return g_object_ref_sink (sidebar);

}

static void
on_confirm_button_clicked (GtkWidget *widget,
                           gpointer data)
{
        printf ("The selected certificate uri is %s\n", cert_uri);
        printf ("The selected certificate password is %s\n", cert_password);
        printf ("The selected key uri is %s\n", key_uri);
        printf ("The selected key password is %s\n", key_password);
}

static void
on_choose_again_button_clicked(GtkWidget *widget,
                               gpointer  data)
{
       GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data);
       gtk_window_set_title(GTK_WINDOW(self), "Choose Certificate");

       gtk_stack_set_visible_child(GTK_STACK(gtk_builder_get_object(
                                   self->builder, "content-area")),
                                   GTK_WIDGET(gtk_builder_get_object(
                                   self->builder, "selection-page")));

       self->is_certificate_choosen = FALSE;
       self->is_key_choosen = FALSE;
       self->key_uri = NULL;
       self->certificate_uri = NULL;

       gtk_tree_view_set_cursor (GTK_TREE_VIEW (self->token_sidebar->tree_view), gtk_tree_path_new_from_string("0"), NULL, FALSE);

       gtk_widget_set_visible (GTK_WIDGET (gtk_builder_get_object (self->builder, "next-button")), TRUE);
       gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (self->builder, "next-button")), FALSE);

       update_file_chooser_filefilter (self);

       gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                    self->builder, "key-label")),
                                    no_key);

       gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                    self->builder, "certificate-label")),
                                    no_certificate);

       gtk_label_set_text (GTK_LABEL (gtk_builder_get_object (self->builder, "key-label")), no_key);

}

/**
 * set_cert_uri_and_password_from_pkcs11: (skip)
 * @self: the widget from the info is retrieved
 *
 * set the cert object uri and password that has came from file
 */
static void
set_cert_uri_and_password_from_file (GcrCertificateChooserDialog *self)
{
        GList *l, *m;

        l = stored_password;
        for (m = stored_uri; m != NULL; m = g_list_next (m)) {

                 if (!g_strcmp0 (m->data, cert_uri)) {
                          cert_password = l->data;
                          return ;
                 }
                 l = g_list_next (l);
        }
        cert_password = NULL;
}

/**
 * set_key_uri_and_password_from_pkcs11: (skip)
 * @self: the widget from the info is retrieved
 *
 * set the key object uri and password that has came from file
 */
static void
set_key_uri_and_password_from_file (GcrCertificateChooserDialog *self)
{
        GList *l, *m;

        l = stored_password;
        for (m = stored_uri; m != NULL; m = g_list_next (m)) {

                 if (!g_strcmp0 ((gchar *)m->data, key_uri)) {
                          key_password = (gchar *)l->data;
                          return ;
                 }
                 l = g_list_next (l);
        }
        key_password = NULL;
}

/**
 * set_cert_uri_and_password_from_pkcs11: (skip)
 * @widget: the widget from the info is retrieved
 *
 * set the cert object uri and password that has came from pkcs11 token
 */
static void
set_cert_uri_and_password_from_pkcs11 (GtkWidget *widget)
{
        GcrCertificateChooserPkcs11 *pkcs11 = GCR_CERTIFICATE_CHOOSER_PKCS11 (widget);
        GList *l, *m;
        gchar *uri = get_token_uri (pkcs11);
        l = stored_password;

        for (m = stored_uri; m != NULL; m = g_list_next (m)){

                 if (!g_strcmp0 ((gchar *)m->data, uri)) {
                          cert_password = (gchar *)l->data;
                          return;
                 }
                 l = g_list_next (l);
        }
        cert_password = NULL;
}

/**
 * set_key_uri_and_password_from_pkcs11: (skip)
 * @widget: the widget from the info is retrieved
 *
 * set the key object uri and pasword that has came from pkcs11 token
 */
static void
set_key_uri_and_password_from_pkcs11 (GtkWidget *widget)
{
        GcrCertificateChooserPkcs11 *pkcs11 = GCR_CERTIFICATE_CHOOSER_PKCS11 (widget);
        GList *l, *m;
        gchar *uri = get_token_uri (pkcs11);
        l = stored_password;

        for (m = stored_uri; m != NULL; m = g_list_next (m)){

                 if (!g_strcmp0 ((gchar *)m->data, uri)) {
                          key_password = (gchar *)l->data;
                          return;
                 }
                 l = g_list_next (l);
        }
        key_password = NULL;
}

/**
 * on_next_button_clicked: (skip)
 * @widget: the next button widget
 * @data: the data passed when widget is clicked
 *
 * Change the current page.
 */
static void
on_next_button_clicked(GtkWidget *widget, gpointer *data)
{
        GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data);
        GtkTreePath *path;
        GtkTreeIter iter;
        RowType type;
        GtkWidget *current_token;

        gtk_tree_view_get_cursor (GTK_TREE_VIEW (self->token_sidebar->tree_view), &path, NULL);
        gtk_tree_model_get_iter (GTK_TREE_MODEL (self->store), &iter, path);
        gtk_tree_model_get (GTK_TREE_MODEL (self->store), &iter, ROW_TYPE, &type, -1);
        gtk_tree_model_get (GTK_TREE_MODEL (self->store), &iter, ROW_WIDGET, &current_token, -1);

        if (!self->is_certificate_choosen) {
                 self->is_certificate_choosen = TRUE;

                 if (!g_strcmp0 (gtk_label_get_label (GTK_LABEL (gtk_builder_get_object
                                (self->builder, "key-label"))), "Key selected")) {

                          if (type == TYPE_FILE_SYSTEM) {
                                   set_key_uri_and_password_from_file (self);
                                   set_cert_uri_and_password_from_file (self);
                          } else {
                                   set_key_uri_and_password_from_pkcs11 (current_token);
                                   set_cert_uri_and_password_from_pkcs11 (current_token);
                          }

                          gtk_stack_set_visible_child(GTK_STACK(gtk_builder_get_object(
                                                      self->builder, "content-area")),
                                                      GTK_WIDGET(gtk_builder_get_object(
                                                      self->builder, "confirm-page")));

                          gtk_widget_set_visible(GTK_WIDGET(gtk_builder_get_object(
                                                 self->builder, "next-button")),FALSE);
                 } else {
                          if (type == TYPE_PKCS11) {
                                   set_cert_uri_and_password_from_pkcs11 (current_token);
                                   GCR_CERTIFICATE_CHOOSER_PKCS11 (current_token)->certificate_choosen = TRUE;
                                   update_object (GCR_CERTIFICATE_CHOOSER_PKCS11 (current_token));
                          } else {
                                   set_cert_uri_and_password_from_file (self);
                                   update_file_chooser_filefilter (self);
                          }

                          gtk_window_set_title(GTK_WINDOW(self), "Choose key");

                          gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(
                                                   self->builder, "next-button")),FALSE);
                }
        } else {
                 if (type == TYPE_FILE_SYSTEM)
                          set_key_uri_and_password_from_file (self);
                 else
                          set_key_uri_and_password_from_pkcs11 (current_token);

                 gtk_stack_set_visible_child(GTK_STACK(gtk_builder_get_object(
                                             self->builder, "content-area")),
                                             GTK_WIDGET(gtk_builder_get_object(
                                             self->builder, "confirm-page")));
                 gtk_widget_set_visible(GTK_WIDGET(gtk_builder_get_object(
                                        self->builder, "next-button")),FALSE);
                 gtk_window_set_title(GTK_WINDOW(self), "Confirm");
        }

}

static void
on_parser_parsed_item(GcrParser *parser,
                      gpointer  *data)
{        GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data);
         GckAttributes *attributes;
         gulong class;
         GtkFileChooser *chooser = GTK_FILE_CHOOSER (gtk_builder_get_object (self->builder, "filechooser"));
         attributes = gcr_parser_get_parsed_attributes(parser);

        if (!self->is_certificate_choosen) {
                 if (gck_attributes_find_ulong (attributes, CKA_CLASS, &class) && class == CKO_CERTIFICATE) {

                          gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                             self->builder, "certificate-label")),
                                              gcr_parser_get_parsed_label (parser));

                          gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(
                                                   self->builder,"next-button")), TRUE);

                          GcrCertificateRenderer *renderer =  gcr_certificate_renderer_new_for_attributes ("Certificate", attributes);
                          GcrCertificate *certificate = gcr_certificate_renderer_get_certificate (renderer);
                          gcr_certificate_widget_set_certificate (GCR_CERTIFICATE_WIDGET (gtk_builder_get_object
                                                                 (self->builder, "certficate-info")), certificate);
                          cert_uri = gtk_file_chooser_get_uri (chooser);
                 } else if (gck_attributes_find_ulong (attributes, CKA_CLASS, &class) && class == CKO_PRIVATE_KEY) {

                          gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                             self->builder, "key-label")),
                                             "Key selected");

                          gcr_key_widget_set_attributes (GCR_KEY_WIDGET (gtk_builder_get_object
                                                                         (self->builder, "key-info")), attributes);

                          self->is_key_choosen = TRUE;
                          key_uri = gtk_file_chooser_get_uri (chooser);
                 }
        } else {

                 if (gck_attributes_find_ulong (attributes, CKA_CLASS, &class) && class == CKO_PRIVATE_KEY){

                          gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                             self->builder, "key-label")),
                                             "Key selected");

                          gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(
                                                   self->builder, "next-button")), TRUE);

                          gcr_key_widget_set_attributes (GCR_KEY_WIDGET (gtk_builder_get_object
                                                                        (self->builder, "key-info")), attributes);

                          key_uri = gtk_file_chooser_get_uri (chooser);
                  }
        }
}

static void
on_file_activated(GtkWidget *widget, gpointer data)
{
	GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data);

        if (gtk_widget_get_sensitive (GTK_WIDGET (gtk_builder_get_object
                                      (self->builder, "next-button"))))
                 gtk_button_clicked (GTK_BUTTON (gtk_builder_get_object (self->builder, "next-button")));

}

static gboolean
on_unlock_renderer_clicked(GtkEntry *entry,
                           gpointer user_data)
{
	GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (user_data);
        GError *error = NULL;
        GtkFileChooser *chooser;

	chooser = GTK_FILE_CHOOSER(gtk_builder_get_object(
                                   self->builder, "filechooser"));

        gcr_parser_add_password(self->parser, gtk_entry_get_text (GTK_ENTRY (entry)));

        if (gcr_parser_parse_bytes (self->parser, self->data, &error)) {

                 stored_uri = g_list_append (stored_uri, gtk_file_chooser_get_uri (chooser));
                 stored_password = g_list_append (stored_password, g_strdup (gtk_entry_get_text (GTK_ENTRY (entry))));

                 gtk_widget_destroy (gtk_file_chooser_get_preview_widget (chooser));
                 gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(chooser), FALSE);

        }
        return TRUE;
}

/**
 * on_parser_authenticate_for_data: (skip)
 * @parser: the parser
 * @count: the number of times this function is called for a particular file
 * @data_main: the data passed to this function
 *
 * Render the widget for asking a pin if the content of a file is locked
 */
static gboolean 
on_parser_authenticate_for_data(GcrParser *parser,
                         gint count,
                         gpointer *data_main)
{
	GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data_main);
        GtkFileChooserWidget *chooser;
        GtkWidget *box, *entry;

	chooser = GTK_FILE_CHOOSER_WIDGET(gtk_builder_get_object(
                                          self->builder, "filechooser"));

        gtk_file_chooser_set_preview_widget_active (GTK_FILE_CHOOSER(chooser), TRUE);
        box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);

        entry = gtk_entry_new ();
        gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);

        gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (gtk_label_new ("Enter Pin")));
        gtk_container_add (GTK_CONTAINER (box), GTK_WIDGET (entry));
        gtk_widget_show_all (GTK_WIDGET (box));

        gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER(chooser), GTK_WIDGET (box));
        self->data = gcr_parser_get_parsed_bytes (parser);
        g_signal_connect(entry, "activate", G_CALLBACK(on_unlock_renderer_clicked), self);
        self->password_wrong_count += 1;

        return TRUE;
}

/**
 * on_update_preview: (skip)
 * @widget: the widget of which preview is updated
 * @user_data: the data passed to this function
 *
 * Set the key cert label on every update to preview
 */
static void
on_update_preview(GtkWidget *widget, gpointer *user_data)
{
	GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (user_data);
        GError *err = NULL;
        guchar *data;
        gsize n_data;
        GBytes *bytes;
        self->password_wrong_count = 0;
        self->is_key_choosen = FALSE;
	GtkFileChooser *chooser = GTK_FILE_CHOOSER(widget);

	char *filename = gtk_file_chooser_get_preview_filename(chooser);

        if (self->is_certificate_choosen) {

                 gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                    self->builder, "key-label")),
                                    no_key);

        } else {

                 gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                    self->builder, "certificate-label")),
                                    no_certificate);

                 gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                    self->builder, "key-label")),
                                    no_key);
        }
	gtk_file_chooser_set_preview_widget_active(chooser, FALSE);

	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(
                                 self->builder, "next-button")),
                                  FALSE);
	if (!filename || g_file_test(filename, G_FILE_TEST_IS_DIR))
		return;

        if(!g_file_get_contents (filename, (gchar**)&data, &n_data, NULL))
            printf("couldn't read file\n");

        bytes = g_bytes_new_take(data, n_data);
        if (!gcr_parser_parse_bytes (self->parser, bytes, &err))
            printf("couldn't parse data: %s\n", err->message);

	if (self->certificate_uri && g_strcmp0(self->certificate_uri,
                                               filename) != 0) {
		g_free(self->certificate_uri);
		self->certificate_uri = NULL;
	}
	g_free(filename);
}

/**
 * is_token_usable:(skip)
 * @self: the GcrCertificateChooserDialog type object
 * @slot: the slot from which the info of token will be retrieved
 * @tokeninfo: the token info of a slot
 *
 * Return TRUE if the token is usable else FALSE otherwise
 */
static gboolean
is_token_usable (GcrCertificateChooserDialog *self,
                 GckSlot *slot,
                 GckTokenInfo *token)
{
        GList *l;

        if (!(token->flags & CKF_TOKEN_INITIALIZED)) {
                return FALSE;
        }
        if ((token->flags & CKF_LOGIN_REQUIRED) &&
            !(token->flags & CKF_USER_PIN_INITIALIZED)) {
                return FALSE;
        }

        for (l = self->blacklist; l != NULL; l = g_list_next (l)) {
                if (gck_slot_match (slot, l->data)) 
                        return FALSE;
        }

        return TRUE;
}

/**
 * load_token: (skip)
 * @self: the GcrCertificateChooserDialog type object
 *
 * To retrieve the list of available token from the slots and insert them
 * into the tree model
 */
static void
load_token (GcrCertificateChooserDialog *self)
{
        GtkTreeIter iter;
        GList *l;
        GckTokenInfo *token;

        gtk_list_store_clear (self->store);
        gtk_list_store_append (self->store, &iter);
        gtk_list_store_set(self->store, &iter,
                           ROW_TYPE, TYPE_FILE_SYSTEM,
                           ROW_LABEL, "file-system",
                           ROW_SLOT, NULL, -1);

        for (l = self->slots; l != NULL; l = g_list_next (l)) {
                 token = gck_slot_get_token_info (GCK_SLOT(l->data));

                 gtk_list_store_append (self->store, &iter);
                 gtk_list_store_set(self->store, &iter,
                                    ROW_TYPE, TYPE_PKCS11,
                                    ROW_LABEL, token->label,
                                    ROW_SLOT, l->data, -1);
        }
}

/**
 * on_initialized_registered: (skip)
 * @unused: The Gobject
 * @result: the asynchronic result
 * @user_data: the data passed to this function
 *
 * To retrieve the list of available slots in the system
 */
static void
on_initialized_registered (GObject *unused,
                           GAsyncResult *result,
                           gpointer user_data)
{
        GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (user_data);
        GList *slots, *s;
        GList *modules, *m;
        GError *error = NULL;
        GckTokenInfo *token;
        self->slots = NULL;

        modules = gck_modules_initialize_registered_finish (result, &error);
        if (error != NULL) {
                 g_warning ("%s", error->message);
                 g_clear_error (&error);
        }

        for (m = modules; m != NULL; m = g_list_next (m)) {
                 slots = gck_module_get_slots (m->data, TRUE);

                 for (s = slots; s; s = g_list_next (s)) {
                          token = gck_slot_get_token_info (s->data);
                          if (token == NULL)
                                   continue;
                          if (is_token_usable (self, s->data, token))
                                   self->slots = g_list_append (self->slots, s->data);
                          gck_token_info_free (token);
                 }

        }
        load_token (self);

        gck_list_unref_free (modules);
}

static void
gcr_certificate_chooser_dialog_constructed (GObject *obj)
{
	GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (obj);
        GtkWidget *content;
        GtkTreeSelection *selection;
        GError *error = NULL;

        G_OBJECT_CLASS (gcr_certificate_chooser_dialog_parent_class)->constructed (obj);
        gck_modules_initialize_registered_async(NULL, on_initialized_registered,
                                                g_object_ref(self));

        if (!gtk_builder_add_from_file (self->builder, UIDIR"gcr-certificate-chooser-dialog.ui", &error)) {
                  g_warning ("couldn't load ui builder file: %s", error->message);
                  return;
          }

	content = GTK_WIDGET(GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))));
        gtk_window_set_default_size (GTK_WINDOW (self), 1200, 800);

	self->hbox = GTK_WIDGET(gtk_builder_get_object(self->builder, "certificate-chooser-dialog"));
        gtk_box_pack_start (GTK_BOX(content), self->hbox, TRUE, TRUE, 0);

        gtk_image_set_from_gicon(GTK_IMAGE(gtk_builder_get_object(
                                 self->builder, "certificate-image")),
                                 g_themed_icon_new(GCR_ICON_CERTIFICATE),
                                 GTK_ICON_SIZE_DIALOG);

        gtk_image_set_from_gicon(GTK_IMAGE(gtk_builder_get_object(
                                 self->builder, "key-image")),
                                 g_themed_icon_new(GCR_ICON_KEY),
                                 GTK_ICON_SIZE_DIALOG);

        gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                           self->builder, "key-label")),
                           no_key);

        gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                           self->builder, "certificate-label")),
                           no_certificate);

        g_signal_connect(self->parser, "parsed",
                         G_CALLBACK(on_parser_parsed_item), self);

        g_signal_connect(self->parser, "authenticate",
                         G_CALLBACK(on_parser_authenticate_for_data), self);

       /* Page1 Construction */
        self->token_sidebar = gcr_certificate_chooser_sidebar_new();
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(self->token_sidebar->tree_view));

        gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
        g_signal_connect (selection, "changed", G_CALLBACK(on_token_selection_changed), self);
        gtk_tree_view_set_model(GTK_TREE_VIEW(self->token_sidebar->tree_view),GTK_TREE_MODEL(self->store));
        gtk_paned_pack1(GTK_PANED(gtk_builder_get_object(self->builder, "selection-page")), GTK_WIDGET(self->token_sidebar),TRUE, FALSE);

	GtkFileFilter *cert_filefilter = GTK_FILE_FILTER(gtk_builder_get_object(self->builder, "cert-filefilter"));
	GtkFileFilter *key_filefilter = GTK_FILE_FILTER(gtk_builder_get_object(self->builder, "key-filefilter"));

	gtk_file_filter_set_name (cert_filefilter,"X.509 Certificate Format");
	gtk_file_filter_set_name (key_filefilter,"X.509 Key Format");

        GtkFileChooserWidget *filechooser = GTK_FILE_CHOOSER_WIDGET(gtk_builder_get_object(self->builder,
                                                    "filechooser"));

        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(filechooser),cert_filefilter);

	g_signal_connect(GTK_FILE_CHOOSER (filechooser),
                         "update-preview", G_CALLBACK (
                          on_update_preview), self);

	g_signal_connect(GTK_FILE_CHOOSER (filechooser),
                         "file-activated", G_CALLBACK (
                          on_file_activated), self);

        g_signal_connect(GTK_WIDGET(gtk_builder_get_object(self->builder,
                         "next-button")), "clicked",
                          G_CALLBACK(on_next_button_clicked), self);

	g_signal_connect(GTK_BUTTON (gtk_builder_get_object(
                         self->builder, "choose-again-button")),
                         "clicked", G_CALLBACK (
                         on_choose_again_button_clicked), self);

	g_signal_connect(GTK_BUTTON (gtk_builder_get_object(
                         self->builder, "confirm-button")),
                         "clicked", G_CALLBACK (
                         on_confirm_button_clicked), self);

	gtk_widget_show_all(GTK_WIDGET (self));

        gtk_window_set_title(GTK_WINDOW(self), "Choose Certificate");
	gtk_window_set_modal (GTK_WINDOW (self), TRUE);
}

static void
gcr_certificate_chooser_dialog_init (GcrCertificateChooserDialog *self)
{
        GError *error = NULL;
        GckUriData *uri;
        guint i;
        self->is_certificate_choosen = FALSE;
        self->is_key_choosen = FALSE;

        self->blacklist = NULL;

        for (i = 0; token_blacklist[i] != NULL; i++) {
                 uri = gck_uri_parse (token_blacklist[i], GCK_URI_FOR_TOKEN | GCK_URI_FOR_MODULE, &error);
                 if (uri == NULL) {
                          g_warning ("couldn't parse pkcs11 blacklist uri: %s", error->message);
                          g_clear_error (&error);
                 }
                 self->blacklist = g_list_prepend (self->blacklist, uri);
        }

        self->builder = gtk_builder_new();
        self->parser = gcr_parser_new();
        self->store = gtk_list_store_new (N_COLUMNS,
                                          G_TYPE_UINT, G_TYPE_STRING,
                                          GCK_TYPE_SLOT, GTK_TYPE_WIDGET);
}

static void
gcr_certificate_chooser_dialog_finalize (GObject *obj)
{
	//GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (obj);

	G_OBJECT_CLASS (gcr_certificate_chooser_dialog_parent_class)->finalize (obj);
}

static void
gcr_certificate_chooser_dialog_class_init (GcrCertificateChooserDialogClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->constructed = gcr_certificate_chooser_dialog_constructed;
	gobject_class->finalize = gcr_certificate_chooser_dialog_finalize;

}

/**
 * gcr_certificate_chooser_dialog_new:
 * @parent: the parent window
 *
 * Create a new certxificate chooser dialog.
 *
 * Returns: (transfer full): A new #GcrCertificateChooserDialog object
 */
GcrCertificateChooserDialog *
gcr_certificate_chooser_dialog_new (GtkWindow *parent)
{
	GcrCertificateChooserDialog *dialog;

	g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);

	dialog = g_object_new (GCR_TYPE_CERTIFICATE_CHOOSER_DIALOG,
	                       NULL);

	return g_object_ref_sink (dialog);
}


gboolean
gcr_certificate_chooser_dialog_run (GcrCertificateChooserDialog *self)
{
	gboolean ret = FALSE;

	g_return_val_if_fail (GCR_IS_CERTIFICATE_CHOOSER_DIALOG (self), FALSE);

	if (gtk_dialog_run (GTK_DIALOG (self)) == GTK_RESPONSE_OK) {
		ret = TRUE;
	}

	gtk_widget_hide (GTK_WIDGET (self));

	return ret;
}

void
gcr_certificate_chooser_dialog_run_async (GcrCertificateChooserDialog *self,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
	g_return_if_fail (GCR_IS_CERTIFICATE_CHOOSER_DIALOG (self));

	_gcr_dialog_util_run_async (GTK_DIALOG (self), cancellable, callback, user_data);
}

gboolean
gcr_certificate_chooser_dialog_run_finish (GcrCertificateChooserDialog *self,
                                      GAsyncResult *result)
{
	gint response;

	g_return_val_if_fail (GCR_IS_CERTIFICATE_CHOOSER_DIALOG (self), FALSE);

	response = _gcr_dialog_util_run_finish (GTK_DIALOG (self), result);

	gtk_widget_hide (GTK_WIDGET (self));

	return (response == GTK_RESPONSE_OK) ? TRUE : FALSE;
}

