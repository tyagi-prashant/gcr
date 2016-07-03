
#define GCR_CERTIFICATE_CHOOSER_PKCS11_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11, GcrCertificateChooserPkcs11Class))
#define GCR_IS_CERTIFICATE_CHOOSER_PKCS11_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11))
#define GCR_CERTIFICATE_CHOOSER_PKCS11_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11, GcrCertificateChooserPkcs11Class))

enum {
        COLUMN_OBJECT,
        T_COLUMNS
};

struct _GcrCertificateChooserPkcs11 {
        GtkScrolledWindow parent;
        GtkWidget *box;
        GckSlot *slot;
        GtkListStore *store;
        GckTokenInfo *info;
        GtkWidget *tree_view;
        gchar *uri;
        GCancellable *cancellable;
        GckSession *session;
        GList *objects;
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
        gulong class;
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
                 if (gck_attributes_find_ulong (attributes, CKA_CLASS, &class) && class == CKO_PRIVATE_KEY) {
                          g_object_set(cell,
                                      "visible", TRUE,
                                      "text", "Private Key",
                                      NULL);
                 } else {
                          if (gck_attributes_find_string (attributes, CKA_LABEL, &label)) {
                                   g_object_set(cell,
                                                "visible", TRUE,
                                                "text", label,
                                                NULL);
                        }
              }
        }
}

static void
gcr_certificate_chooser_pkcs11_constructed (GObject *obj)
{
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11(obj);
        GtkTreeViewColumn *col;
        GtkCellRenderer *cell;
        G_OBJECT_CLASS(gcr_certificate_chooser_pkcs11_parent_class)->constructed (obj) ;

        self->tree_view = gtk_tree_view_new();
        col = gtk_tree_view_column_new ();
        
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
        self->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
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
        self->objects = gck_enumerator_next_finish (GCK_ENUMERATOR(enumerator),
                                                    result,
                                                     &error);

        for (l = self->objects; l != NULL; l = g_list_next (l)) {
                 gtk_list_store_append (self->store, &iter);
                 gtk_list_store_set (self->store, &iter, COLUMN_OBJECT, l->data, -1);
        }
        gtk_tree_view_set_model (GTK_TREE_VIEW (self->tree_view), GTK_TREE_MODEL (self->store));
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
                                                             match);
                 gck_enumerator_next_async (enumerator,
                                            -1,
                                            self->cancellable,
                                            on_objects_loaded,
                                            g_object_ref (self));
        }
}

static void
on_login_button_clicked (GtkWidget *button,
                         gpointer data)
{

}

GcrCertificateChooserPkcs11 *
gcr_certificate_chooser_pkcs11_new (GckSlot *slot)
{
        GcrCertificateChooserPkcs11 *self;
        GtkWidget *button;
        self = g_object_new (GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11,
                             NULL);
        self->slot = slot;
        self->info = gck_slot_get_token_info (self->slot);
        gck_session_open_async (self->slot, 
                                GCK_SESSION_READ_ONLY,
                                NULL,
                                self->cancellable,
                                get_session,
                                g_object_ref(self));

        if ((self->info)->flags & CKF_LOGIN_REQUIRED ) {

                 button = gtk_button_new_with_label ("Click Here To See More!");
                 gtk_container_add (GTK_CONTAINER (self->box), GTK_WIDGET (button));
                 gtk_box_reorder_child (GTK_BOX (self->box), GTK_WIDGET (self->tree_view), 1);

                  g_signal_connect (button, "clicked",
                                   G_CALLBACK (on_login_button_clicked),
                                   self);
        }

        return g_object_ref_sink (self);
}

