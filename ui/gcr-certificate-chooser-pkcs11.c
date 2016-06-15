
#define GCR_CERTIFICATE_CHOOSER_PKCS11_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11, GcrCertificateChooserPkcs11Class))
#define GCR_IS_CERTIFICATE_CHOOSER_PKCS11_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11))
#define GCR_CERTIFICATE_CHOOSER_PKCS11_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11, GcrCertificateChooserPkcs11Class))

struct _GcrCertificateChooserPkcs11 {
        GtkScrolledWindow parent;
        GckSlot *slot;
        GckTokenInfo *info;
        gchar *uri;
        GckSession *session;
};

struct _GcrCertificateChooserPkcs11Class {
        GtkScrolledWindowClass parent_class;
};

typedef struct _GcrCertificateChooserPkcs11Class GcrCertificateChooserPkcs11Class;

G_DEFINE_TYPE (GcrCertificateChooserPkcs11, gcr_certificate_chooser_pkcs11, GTK_TYPE_SCROLLED_WINDOW) ;

static void
gcr_certificate_chooser_pkcs11_constructed (GObject *obj)
{
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11(obj);
        
        G_OBJECT_CLASS(gcr_certificate_chooser_pkcs11_parent_class)->constructed (obj) ;

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


}

static void
get_session (GckSlot *slot,
             GAsyncResult *result,
             gpointer *data)
{
        GckSession *session;
        GError *error = NULL;
        GcrCertificateChooserPkcs11 *self = GCR_CERTIFICATE_CHOOSER_PKCS11(data);
        self->session = gck_slot_open_session_finish (slot, result, &error);
       
        if (error != NULL)
             return session;
        else
             printf("%s\n", error->message);
}

GcrCertificateChooserPkcs11 *
gcr_certificate_chooser_pkcs11_new (GckSlot *slot, GckTokenInfo *info)
{
        GcrCertificateChooserPkcs11 *self;
        self = g_object_new (GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11,
                             NULL);
        self->slot = slot;
        self->info = info;
        return g_object_ref_sink (self);
}

