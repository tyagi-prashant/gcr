#include <string.h>
#define GCR_CERTIFICATE_CHOOSER_PKCS11_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11, GcrCertificateChooserPkcs11Class))
#define GCR_IS_CERTIFICATE_CHOOSER_PKCS11_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11))
#define GCR_CERTIFICATE_CHOOSER_PKCS11_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11, GcrCertificateChooserPkcs11Class))

enum {
        COLUMN_OBJECT,
        T_COLUMNS
};

static const char *page2 = "page2";

struct _GcrCertificateChooserPkcs11 {
        GtkScrolledWindow parent;
        GtkWidget *box;
        GckSlot *slot;
        GtkBuilder *builder;
        char *current_page;
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

G_DEFINE_TYPE (GcrCertificateChooserPkcs11, gcr_certificate_chooser_pkcs11, GTK_TYPE_SCROLLED_WINDOW) ;

static void 
on_cell_renderer_object(GtkTreeViewColumn *column,
                       GtkCellRenderer *cell,
                       GtkTreeModel *model,
                       GtkTreeIter *iter,
                       gpointer user_data)
{

        GckObject *object;
        gchar *label;
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
        gchar *label;
        GError *error = NULL;
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11 (data);

        gtk_tree_model_get (model, iter, 0, &object, -1);
        GckAttributes *attributes = gck_object_get (object,
                                                    self->cancellable,
                                                    &error, CKA_CLASS,
                                                    CKA_LABEL, CKA_ISSUER, GCK_INVALID);
        gck_attributes_find_string (attributes, CKA_LABEL, &label);

        if (self->current_page != page2) {
            
                 gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                    self->builder, "certificate-label")),
                                    label);
                 gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                    self->builder, "key-label")),
                                    "No key selected yet");
                 gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (
                                           self->builder, "page1-next-button")), TRUE);
        } else {
               
                 gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                    self->builder, "key-label")),
                                    "Key selected");
                 gtk_widget_set_sensitive (GTK_WIDGET (gtk_builder_get_object (
                                           self->builder, "page2-next-button")), TRUE);

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
        if (self->current_page != page2) {

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

        self->cancellable = NULL;
        self->collection = gcr_simple_collection_new ();
        self->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
        self->label = gtk_label_new ("Enter Pin");
        self->store = gtk_list_store_new (T_COLUMNS, 
                                          GCK_TYPE_OBJECT);
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
        self->objects = gck_enumerator_next_finish (GCK_ENUMERATOR(enumerator),
                                                    result,
                                                    &error);
        if (self->current_page == page2)
                 current_class_needed = CKO_PRIVATE_KEY;
        else
                 current_class_needed = CKO_CERTIFICATE;
        for (l = self->objects; l != NULL; l = g_list_next (l)) {

                 GckAttributes *attributes = gck_object_get (l->data,
                                                             self->cancellable,
                                                             &error, CKA_CLASS,
                                                             CKA_LABEL, CKA_ISSUER,GCK_INVALID);

                 if (!gcr_collection_contains (GCR_COLLECTION (self->collection), l->data)) {

                          if (gck_attributes_find_ulong (attributes, CKA_CLASS, &class) && class == current_class_needed) {

                                   gtk_list_store_append (self->store, &iter);
                                   gtk_list_store_set (self->store, &iter, COLUMN_OBJECT, l->data, -1);
                                   gcr_simple_collection_add (GCR_SIMPLE_COLLECTION(self->collection), l->data);
                          }
                 gck_attributes_unref (attributes);
                 }
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
                 gck_enumerator_next_async (enumerator,
                                            -1,
                                            self->cancellable,
                                            on_objects_loaded,
                                            g_object_ref (self));
        }
}

static void 
on_password_verify (GObject *session, 
                   GAsyncResult *result, 
                   gpointer data)
{
        GError *error = NULL;
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11(data);
        gtk_entry_set_text (GTK_ENTRY (self->entry), "");

        if (gck_session_login_finish (self->session, result, &error)) {

                 GckEnumerator *enumerator;
                 GckAttributes *match = gck_attributes_new_empty (GCK_INVALID);
 
                 gtk_widget_destroy (GTK_WIDGET (self->entry));
                 gtk_widget_destroy (GTK_WIDGET (self->label));
                 gtk_list_store_clear (self->store);
                 g_object_unref (self->collection);
                 self->collection = gcr_simple_collection_new ();
                 enumerator = gck_session_enumerate_objects (self->session,
                                                             match);
                 gck_enumerator_next_async (enumerator,
                                            -1,
                                            self->cancellable,
                                            on_objects_loaded,
                                            g_object_ref (self));
        }
        
        if (error != NULL) {

                 if (!g_strcmp0 (error->message, "The password or PIN is incorrect"))
                           gtk_label_set_text (GTK_LABEL (self->label), "The password or PIN is incorrect");
                 else {

                          gtk_widget_destroy (GTK_WIDGET (self->entry));         
                          gtk_widget_destroy (GTK_WIDGET (self->label));
                 }
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
        gtk_widget_destroy (GTK_WIDGET (widget));
        self->entry = gtk_entry_new ();
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
                                    gchar *page)
{
        GcrCertificateChooserPkcs11 *self;
        self = g_object_new (GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11,
                             NULL);
        self->current_page = page;
        self->slot = slot;
        self->info = gck_slot_get_token_info (self->slot);
        gck_session_open_async (self->slot, 
                                GCK_SESSION_READ_ONLY,
                                NULL,
                                self->cancellable,
                                get_session,
                                g_object_ref(self));

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

