/*
 *
 * Copyright (C) 2016 Prashant Tyagi and David Woodhouse
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


#include <string.h>
#define GCR_CERTIFICATE_CHOOSER_PKCS11_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11, GcrCertificateChooserPkcs11Class))
#define GCR_IS_CERTIFICATE_CHOOSER_PKCS11_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11))
#define GCR_CERTIFICATE_CHOOSER_PKCS11_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11, GcrCertificateChooserPkcs11Class))

enum {
        COLUMN_OBJECT,
        T_COLUMNS
};

struct _GcrCertificateChooserPkcs11 {
        GtkScrolledWindow parent;
        GtkWidget *search_bar;
        GtkWidget *box;
        GckSlot *slot;
        GtkBuilder *builder;
        gboolean certificate_choosen;
        GtkListStore *store;
        GckTokenInfo *info;
        GtkWidget *tree_view;
        gchar *uri;
        GtkWidget *entry;
        GtkWidget *label;
        GCancellable *cancellable;
        GckSession *session;
        GList *objects;
        GcrCollection *collection;
};

struct _GcrCertificateChooserPkcs11Class {
        GtkScrolledWindowClass parent_class;
};

typedef struct _GcrCertificateChooserPkcs11Class GcrCertificateChooserPkcs11Class;

G_DEFINE_TYPE (GcrCertificateChooserPkcs11, gcr_certificate_chooser_pkcs11, GTK_TYPE_SCROLLED_WINDOW);

static void
on_cell_renderer_object(GtkTreeViewColumn *column,
                       GtkCellRenderer *cell,
                       GtkTreeModel *model,
                       GtkTreeIter *iter,
                       gpointer user_data)
{
        GckObject *object;
        gchar *label = NULL;
        GError *error = NULL;
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11 (user_data);

        gtk_tree_model_get (model, iter, 0, &object, -1);
        GckAttributes *attributes = gck_object_get (object,
                                                    self->cancellable,
                                                    &error, CKA_CLASS,
                                                    CKA_LABEL, GCK_INVALID);
       if (error != NULL)
                 printf("object error occur\n");
       else {    
                 if (gck_attributes_find_string (attributes, CKA_LABEL, &label)) {

                          g_object_set(cell,
                                       "visible", TRUE,
                                       "text", label,
                                       NULL);
                 } else if (label == NULL) {
                          g_object_set(cell,
                                       "visible", TRUE,
                                       "text", "NULL",
                                       NULL);
                 }
        }
}

static void
on_tree_node_select (GtkTreeModel *model,
                     GtkTreePath *path,
                     GtkTreeIter *iter,
                     gpointer data)
{
        GckObject *object;
        gchar *cert_label, *key_label;
        gulong class;
        GError *error = NULL;
        GList *l;
        GckUriData *uri_data;
        GckAttributes *cert_attributes, *key_attributes;
        const GckAttribute *cert_attribute, *key_attribute;
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11 (data);

        uri_data = gck_uri_data_new ();
        uri_data->module_info = gck_module_get_info (gck_slot_get_module (self->slot));
        uri_data->token_info = self->info;
        uri_data->any_unrecognized = FALSE;

        gtk_tree_model_get (model, iter, 0, &object, -1);
        if (!self->certificate_choosen) {
                 cert_attributes = gck_object_get (object, self->cancellable,
                                                   &error, CKA_CLASS,
                                                   CKA_VALUE, CKA_LABEL, CKA_ISSUER,
                                                   CKA_ID,GCK_INVALID);

                 GcrCertificateRenderer *renderer =  gcr_certificate_renderer_new_for_attributes ("Certificate", cert_attributes);
                 GcrCertificate *certificate = gcr_certificate_renderer_get_certificate (renderer);
                 gcr_certificate_widget_set_certificate (GCR_CERTIFICATE_WIDGET (gtk_builder_get_object
                                                        (self->builder, "certficate-info")), certificate);
                 uri_data->attributes = cert_attributes;
                 cert_uri = gck_uri_build (uri_data, GCK_URI_FOR_ANY);

                 gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                    self->builder, "key-label")),
                                    "No key selected yet");

                 cert_attribute = gck_attributes_find (cert_attributes, CKA_ID);
                 gck_attributes_find_string (cert_attributes, CKA_LABEL, &cert_label);

                 for (l = self->objects; l != NULL; l = g_list_next (l)) {
                          key_attributes = gck_object_get (l->data, self->cancellable,
                                                           &error, CKA_CLASS, CKA_LABEL,
                                                           CKA_ID, GCK_INVALID);

                          gck_attributes_find_ulong (key_attributes, CKA_CLASS, &class);
                          if (class == CKO_PRIVATE_KEY) {

                                   gck_attributes_find_string (key_attributes, CKA_LABEL, &key_label);
                                   key_attribute = gck_attributes_find (key_attributes, CKA_ID);

                                   if (gck_attribute_equal (key_attribute, cert_attribute) && !(g_strcmp0 (cert_label, key_label))) {

                                            gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                                                         self->builder, "key-label")), "Key selected");

                                             key_attributes = gck_object_get (l->data, self->cancellable,
                                                                              &error, CKA_CLASS,
                                                                              CKA_LABEL, CKA_ID,
                                                                              CKA_PUBLIC_EXPONENT,CKA_KEY_TYPE,
                                                                              CKK_RSA, CKA_MODULUS,
                                                                              CKA_VALUE, GCK_INVALID);

                                            gcr_key_widget_set_attributes (GCR_KEY_WIDGET (gtk_builder_get_object
                                                                            (self->builder, "key-info")), key_attributes);
                                            uri_data->attributes = key_attributes;
                                            key_uri = gck_uri_build (uri_data, GCK_URI_FOR_ANY);
                                   }
                          }
                 }

                 gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                    self->builder, "certificate-label")),
                                    cert_label);

                 gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (
                                           self->builder, "next-button")), TRUE);
        } else {
                 key_attributes = gck_object_get (object, self->cancellable,
                                                  &error, CKA_CLASS, GCK_INVALID);
                 if (key_attributes == NULL)
                          return ;

                 gck_attributes_find_ulong (key_attributes, CKA_CLASS, &class);

                 if (class == CKO_PRIVATE_KEY) {
                          key_attributes = gck_object_get (object, self->cancellable,
                                                           &error, CKA_CLASS, CKA_LABEL,
                                                           CKA_ID,CKA_PUBLIC_EXPONENT,
                                                           CKA_KEY_TYPE, CKK_RSA, CKA_MODULUS,
                                                           CKA_VALUE, GCK_INVALID);

                          gcr_key_widget_set_attributes (GCR_KEY_WIDGET (gtk_builder_get_object
                                                        (self->builder, "key-info")), key_attributes);

                          uri_data->attributes = key_attributes;
                          key_uri = gck_uri_build (uri_data, GCK_URI_FOR_ANY);

                          gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                                       self->builder, "key-label")), "Key selected");

                          gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (
                                                    self->builder, "next-button")), TRUE);
                }

        }
}

static void
on_tree_view_selection_changed (GtkTreeSelection *selection,
                                gpointer data)
{
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11(data);
        gtk_tree_selection_selected_foreach (selection, on_tree_node_select, self);
}

static void 
on_cell_renderer_pixbuf(GtkTreeViewColumn *column,
               GtkCellRenderer *cell,
               GtkTreeModel *model,
               GtkTreeIter *iter,
               gpointer user_data)
{
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11(user_data);
        if (!self->certificate_choosen) {
                 g_object_set(cell,
                              "visible", TRUE,
                              "gicon", g_themed_icon_new(GCR_ICON_CERTIFICATE),
                              NULL);
        } else {
                 g_object_set(cell,
                              "visible", TRUE,
                              "gicon", g_themed_icon_new(GCR_ICON_KEY),
                              NULL);
        }
}

static void
on_row_activated (GtkTreeView       *tree_view,
                  GtkTreePath       *path,
                  GtkTreeViewColumn *column,
                  gpointer           data)
{
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11(data);
        gtk_button_clicked (GTK_BUTTON (gtk_builder_get_object (self->builder, "next-button")));
}

static gint
sort_model (GtkTreeModel *model,
            GtkTreeIter *a,
            GtkTreeIter *b,
            gpointer data)
{
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11(data);
        GckObject *object1, *object2;
        GckAttributes *object1_attributes, *object2_attributes;
        gint ret = 0;
        GError *error1 = NULL, *error2 = NULL;
        gchar *object1_label = NULL, *object2_label = NULL;

        gtk_tree_model_get (model, a, 0, &object1, -1);
        gtk_tree_model_get (model, b, 0, &object2, -1);

        object1_attributes = gck_object_get (object1, self->cancellable,
                                             &error1, CKA_LABEL,
                                             GCK_INVALID);

        object2_attributes = gck_object_get (object2, self->cancellable,
                                             &error2, CKA_LABEL,
                                             GCK_INVALID);

        if (error1 != NULL || error2 != NULL)
                 return ret;

        gck_attributes_find_string (object1_attributes, CKA_LABEL, &object1_label);
        gck_attributes_find_string (object2_attributes, CKA_LABEL, &object2_label);

        if (object1_label == NULL || object2_label == NULL) {
                 if (object1_label == NULL && object2_label == NULL)
                          return ret;
                 ret = (object1_label == NULL) ? -1 : 1;
        } else {
                 ret = g_utf8_collate(object1_label, object2_label);
        }

        return ret;
}

static void
gcr_certificate_chooser_pkcs11_constructed (GObject *obj)
{
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11(obj);
        GtkTreeViewColumn *col;
        GtkCellRenderer *cell;
        GtkTreeSelection *tree_selection;
        G_OBJECT_CLASS(gcr_certificate_chooser_pkcs11_parent_class)->constructed (obj) ;

        self->tree_view = gtk_tree_view_new();
        col = gtk_tree_view_column_new ();
        gtk_tree_view_column_set_clickable (GTK_TREE_VIEW_COLUMN (col), FALSE);

        cell = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	g_object_set (cell,
	              "xpad", 6,
	              NULL);

        cell = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (col, cell, FALSE);
	gtk_tree_view_column_set_cell_data_func (col, cell,
	                                         on_cell_renderer_pixbuf,
	                                         self, NULL);
        cell = gtk_cell_renderer_text_new ();
        gtk_tree_view_column_pack_start (col, cell, TRUE);
        g_object_set (G_OBJECT (cell), "editable", FALSE, NULL);
        gtk_tree_view_column_set_cell_data_func (col, cell,
                                                 on_cell_renderer_object,
                                                 self, NULL);
        
        g_object_set (cell,
                      "ellipsize", PANGO_ELLIPSIZE_END,
                      "ellipsize-set", TRUE,
                       NULL);

        gtk_tree_view_append_column (GTK_TREE_VIEW(self->tree_view), col);
        gtk_tree_view_column_set_max_width (GTK_TREE_VIEW_COLUMN (col), 12);
        gtk_container_add (GTK_CONTAINER (self->box), GTK_WIDGET (self->tree_view));
        gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->box));
        gtk_tree_view_set_model (GTK_TREE_VIEW (self->tree_view), GTK_TREE_MODEL (self->store));

        tree_selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (self->tree_view));
        gtk_tree_selection_set_mode (tree_selection, GTK_SELECTION_MULTIPLE);

        g_signal_connect (tree_selection, "changed", G_CALLBACK (on_tree_view_selection_changed), self);
        g_signal_connect (self->tree_view, "row-activated", G_CALLBACK (on_row_activated), self);

        gtk_widget_show (GTK_WIDGET (self->tree_view));
 }

static void
gcr_certificate_chooser_pkcs11_class_init (GcrCertificateChooserPkcs11Class *klass)
{

        GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
        gobject_class -> constructed = gcr_certificate_chooser_pkcs11_constructed;

}

static void
gcr_certificate_chooser_pkcs11_init (GcrCertificateChooserPkcs11 *self)
{
        GtkTreeSortable *sortable;
        self->cancellable = NULL;
        self->collection = gcr_simple_collection_new ();
        self->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
        self->label = gtk_label_new ("Enter Pin");

        self->store = gtk_list_store_new (T_COLUMNS, 
                                          GCK_TYPE_OBJECT);
        sortable = GTK_TREE_SORTABLE (self->store);
        gtk_tree_sortable_set_sort_func (sortable, 0,
                                          sort_model, self, NULL);
        gtk_tree_sortable_set_sort_column_id(sortable, 0, GTK_SORT_ASCENDING);

}

static void
on_objects_loaded (GObject *enumerator,
                   GAsyncResult *result,
                   gpointer data)
{
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11(data);
        GError *error = NULL;
        GList *l;
        GtkTreeIter iter;
        gulong class, current_class_needed;
        gtk_list_store_clear (self->store);
        self->objects = gck_enumerator_next_finish (GCK_ENUMERATOR(enumerator),
                                                    result,
                                                    &error);
        if (error != NULL)
                 return ;

        error = NULL;
        if (self->certificate_choosen) {
                 current_class_needed = CKO_PRIVATE_KEY;

                 gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                    self->builder, "key-label")),
                                    "No key selected yet");

        } else {
                 current_class_needed = CKO_CERTIFICATE;

                 gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                    self->builder, "certificate-label")),
                                    "No certificate selected yet");

                 gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                    self->builder, "key-label")),
                                    "No key selected yet");
        }
        for (l = self->objects; l != NULL; l = g_list_next (l)) {
                 GckAttributes *attributes = gck_object_get (l->data,
                                                             self->cancellable,
                                                             &error, CKA_CLASS,
                                                             CKA_LABEL, CKA_ISSUER,
                                                             CKA_ID, GCK_INVALID);
                 if (error != NULL)
                          continue;
                 if (!gcr_collection_contains (GCR_COLLECTION (self->collection), l->data)) {
                          if (gck_attributes_find_ulong (attributes, CKA_CLASS, &class) && class == current_class_needed) {
                                   gtk_list_store_append (self->store, &iter);
                                   gtk_list_store_set (self->store, &iter, COLUMN_OBJECT, l->data, -1);
                                   gcr_simple_collection_add (GCR_SIMPLE_COLLECTION(self->collection), l->data);
                          }
                 }
                 gck_attributes_unref (attributes);
                 error = NULL;
        }
}

static void
get_session (GObject *slot,
             GAsyncResult *result,
             gpointer data)
{
        GError *error = NULL;
        GckEnumerator *enumerator;
        GckAttributes *match = gck_attributes_new_empty (GCK_INVALID);
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11(data);
        self->session = gck_session_open_finish (result, &error);

        if (error != NULL)
                 printf("%s\n", error->message);
        else {
                 enumerator = gck_session_enumerate_objects (self->session,
                                                             gck_attributes_ref_sink (match));
                 gck_attributes_unref (match);
                 gck_enumerator_next_async (enumerator, -1,
                                            self->cancellable, on_objects_loaded,self);
        }
}

static void
update_object (GcrCertificateChooserPkcs11 *self)
{
        GckEnumerator *enumerator;
        GckAttributes *match = gck_attributes_new_empty (GCK_INVALID);

        gtk_list_store_clear (self->store);

        g_object_unref (self->collection);
        self->collection = gcr_simple_collection_new ();

        enumerator = gck_session_enumerate_objects (self->session, match);
        gck_enumerator_next_async (enumerator,-1,
                                   self->cancellable, on_objects_loaded, self);
}

static gchar*
get_token_uri (GcrCertificateChooserPkcs11 *self)
{
        GckUriData *uri_data;

        uri_data = gck_uri_data_new ();
        uri_data->module_info = gck_module_get_info (gck_slot_get_module (self->slot));
        uri_data->token_info = self->info;
        uri_data->any_unrecognized = FALSE;

        return gck_uri_build (uri_data, GCK_URI_FOR_TOKEN);
}

static void
on_password_verify (GObject *session,
                   GAsyncResult *result,
                   gpointer data)
{
        GError *error = NULL;
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11(data);

        if (gck_session_login_finish (self->session, result, &error)) {

                 stored_password = g_list_append (stored_password, g_strdup (gtk_entry_get_text GTK_ENTRY((self->entry))));
                 stored_uri = g_list_append (stored_uri, get_token_uri (self));

                 update_object (self);

                 gtk_widget_destroy (GTK_WIDGET (self->entry));
                 gtk_widget_destroy (GTK_WIDGET (self->label));

                 gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (
                                           self->builder, "next-button")), FALSE);
        }

        if (error != NULL) {

                 if (!g_strcmp0 (error->message, "The password or PIN is incorrect"))
                           gtk_label_set_text (GTK_LABEL (self->label), "The password or PIN is incorrect");
                 else {

                          gtk_widget_destroy (GTK_WIDGET (self->entry));
                          gtk_widget_destroy (GTK_WIDGET (self->label));
                 }
                 gtk_entry_set_text (GTK_ENTRY (self->entry), "");
        }
}

static void
on_password_enter (GtkWidget *widget, 
                   gpointer data)
{
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11(data);
        const gchar *password = gtk_entry_get_text (GTK_ENTRY (widget));
        const gint len = gtk_entry_get_text_length (GTK_ENTRY (widget));

        gck_session_login_async (self->session,
                                 CKU_USER,
                                 (guchar *)password,
                                 len,
                                 NULL,
                                 on_password_verify,
                                 self);
}

static void
on_login_button_clicked (GtkWidget *widget,
                         gpointer data)
{
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11(data);
        self->entry = gtk_entry_new ();

        gtk_widget_destroy (GTK_WIDGET (widget));

        gtk_entry_set_max_length (GTK_ENTRY (self->entry), 50);
        gtk_entry_set_visibility (GTK_ENTRY (self->entry), FALSE);

        gtk_container_add (GTK_CONTAINER (self->box), GTK_WIDGET (self->label));
        gtk_container_add (GTK_CONTAINER (self->box), GTK_WIDGET (self->entry));

        gtk_box_reorder_child (GTK_BOX (self->box), GTK_WIDGET (self->tree_view), 2);
        g_signal_connect (self->entry, "activate",
		      G_CALLBACK (on_password_enter),
		      self);

        gtk_widget_show_all (GTK_WIDGET (self->box));
}

GcrCertificateChooserPkcs11 *
gcr_certificate_chooser_pkcs11_new (GckSlot *slot,
                                    gboolean is_certificate_choosen)
{
        GcrCertificateChooserPkcs11 *self;
        self = g_object_new (GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11,
                             NULL);

        self->certificate_choosen = is_certificate_choosen;
        self->slot = slot;
        self->info = gck_slot_get_token_info (self->slot);

        gck_session_open_async (self->slot,
                                GCK_SESSION_READ_ONLY,
                                NULL,
                                self->cancellable,
                                get_session,
                                self);

        if ((self->info)->flags & CKF_LOGIN_REQUIRED ) {
                 GtkWidget *button = gtk_button_new_with_label ("Click Here To See More!");
                 gtk_container_add (GTK_CONTAINER (self->box), GTK_WIDGET (button));
                 gtk_box_reorder_child (GTK_BOX (self->box), GTK_WIDGET (self->tree_view), 1);

                 g_signal_connect (button, "clicked",
                                   G_CALLBACK (on_login_button_clicked),
                                   self);
        }

        return g_object_ref_sink (self);
}

