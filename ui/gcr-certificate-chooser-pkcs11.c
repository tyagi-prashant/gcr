
#define GCR_CERTIFICATE_CHOOSER_PKCS11_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11, GcrCertificateChooserPkcs11Class))
#define GCR_IS_CERTIFICATE_CHOOSER_PKCS11_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11))
#define GCR_CERTIFICATE_CHOOSER_PKCS11_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11, GcrCertificateChooserPkcs11Class))

enum {
        OBJECT_TYPE,
        T_COLUMNS
};

struct _GcrCertificateChooserPkcs11 {
        GtkScrolledWindow parent;
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
on_object_type_render (GObject *obj, GAsyncResult *result, gpointer data)
{
        GError *error = NULL;
        gulong class;
        GtkCellRenderer *cell = GTK_CELL_RENDERER (data);
        GckAttributes *attributes = gck_object_get_finish (GCK_OBJECT(obj), result, &error);
       if (error != NULL)
                 printf("object error occur\n");
      else {
                if (gck_attributes_find_ulong (attributes, CKA_CLASS, &class) && class == CKO_PRIVATE_KEY) {
                         g_object_set(cell, 
                                      "visible", TRUE,
                                      "text", "Private Key",
                                       NULL);
                        printf ("found private key\n");
                 
               } else { 
                         g_object_set(cell, 
                                      "visible", TRUE,
                                      "text", "Certificate",
                                       NULL);
                         printf ("found certificate\n");
              }
        }
}
        
static void 
on_cell_renderer_object(GtkTreeViewColumn *column,
                       GtkCellRenderer *cell,
                       GtkTreeModel *model,
                       GtkTreeIter *iter,
                       gpointer user_data)
{

        printf ("int on_cell_renderer function\n");
        GckObject *object;

        const gulong *attr_types = {attr_types};
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11 (user_data);
        gtk_tree_model_get (model, iter, 0, &object, -1);
        gck_object_get_async (object, attr_types, 0, self->cancellable, on_object_type_render, cell);
        

        
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
        gtk_tree_view_set_model (GTK_TREE_VIEW (self->tree_view), GTK_TREE_MODEL (self->store));
         gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->tree_view    ));
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
        printf("the length of objects is %d\n", g_list_length(self->objects));
        for (l = self->objects; l != NULL; l = g_list_next (l)) {
                 gtk_list_store_append (self->store, &iter);
                 gtk_list_store_set (self->store, &iter, OBJECT_TYPE, l->data, -1);
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

GcrCertificateChooserPkcs11 *
gcr_certificate_chooser_pkcs11_new (GckSlot *slot)
{
        GcrCertificateChooserPkcs11 *self;
       // GTlsInteraction *interaction;
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

        return g_object_ref_sink (self);
}

