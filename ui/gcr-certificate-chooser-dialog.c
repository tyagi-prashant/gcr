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

enum {
	PROP_0,
	PROP_IMPORTER
};

struct _GcrCertificateChooserDialog {
       GtkDialog parent;
       GtkWidget *notebook;
       GcrViewer *viewer;
       char *certificate_uri;
       char *key_uri;
       GcrParser *parser;
       gboolean is_certificate_choosen;
       gboolean is_key_choosen;
       GtkWidget *hbox;
       GtkWidget *vbox_password;
       GtkWidget *vbox1;
       GtkWidget *cert_hbox;
       GtkWidget *cert_image;
       GtkWidget *cert_label;
       GtkWidget *key_hbox;
       GtkWidget *key_image;
       GtkWidget *key_label;
       GtkWidget *password_entry;
       GtkWidget *password_entry_label;
       GtkWidget *default_button;
       GtkWidget *next_button;
       GtkWidget *previous_button;
       GtkWidget *page1_box;
       GtkWidget *page2_box;
       GtkWidget *page3_box;
       GtkWidget *page1_stack;
       GtkWidget *page2_stack;
       GtkWidget *page1_stack_switcher;
       GtkWidget *page2_stack_switcher;
       GtkWidget *page1_file_chooser;
       GtkWidget *page2_file_chooser;
};

typedef struct _GcrCertificateChooserDialogClass GcrCertificateChooserDialogClass;

struct _GcrCertificateChooserDialogClass {
	GtkDialogClass parent;
};

G_DEFINE_TYPE (GcrCertificateChooserDialog, gcr_certificate_chooser_dialog, GTK_TYPE_DIALOG);

static void
on_next_button_clicked(GtkWidget *widget, gpointer *data)
{
       GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data);
       gchar *fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (self->page1_file_chooser));

       printf("next-button %s\n", fname);
       g_free(self->certificate_uri);
       self->certificate_uri = fname;
}

static void
on_default_button_clicked(GtkWidget *widget, gpointer *data)
{
       GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data);
       gchar *fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (self->page1_file_chooser));
       printf("default-button %s\n", fname);

       if (g_file_test(fname, G_FILE_TEST_IS_DIR)) {
	       gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (self->page1_file_chooser), fname);
	       g_free(fname);
       } else {
	       /* This is used as a flag to the preview/added signals that this was *set* */
	       self->certificate_uri = fname;
	       gtk_file_chooser_set_filename(GTK_FILE_CHOOSER (self->page1_file_chooser), fname);
       }
}

static void
on_certificate_choosed(GcrParser *parser,
               gpointer         *data)
{       GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data); 
        GckAttributes *attributes;
        char *filename = gtk_file_chooser_get_preview_filename(GTK_FILE_CHOOSER(self->page1_file_chooser));
       // gtk_file_chooser_set_preview_widget_active(self->page1_file_chooser, FALSE);
        if(g_strcmp0(self->key_uri,  filename) != 0)
            self->is_key_choosen = FALSE;
        if (self->certificate_uri && g_strcmp0(self->certificate_uri, filename) != 0) {
		g_free(self->certificate_uri);
		self->certificate_uri = NULL;
	}
        gulong class;
        attributes = gcr_parser_get_parsed_attributes(parser);
        
        if (gck_attributes_find_ulong (attributes, CKA_CLASS, &class) && class == CKO_CERTIFICATE) {
		// XXX: Get subject, issuer and expiry and use gtk_label_set_markup to show them nicely
		gtk_label_set_text(GTK_LABEL(self->cert_label), gcr_parser_get_parsed_label(parser));
            self->is_certificate_choosen = TRUE;
            gtk_widget_set_sensitive(GTK_WIDGET(self->next_button), TRUE);
	    if (self->certificate_uri)
		    on_next_button_clicked(GTK_WIDGET(self->next_button), self);
	    else printf("not set\n");
        }

        if (gck_attributes_find_ulong (attributes, CKA_CLASS, &class) && class == CKO_PRIVATE_KEY){
            self->is_key_choosen = TRUE;
	    self->key_uri = filename;
            gtk_label_set_text(GTK_LABEL(self->key_label), "Key selected");

        }
}        

static void
on_page1_file_activated(GtkWidget *widget, gpointer *data)
{
	//GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data);
	GtkFileChooser *chooser = GTK_FILE_CHOOSER(widget);

	/* The user might have hit Ctrl-L and typed in a filename, and this
	   happens when they press Enter. It also happens when they double
	   click a file in the browser. */
	gchar *fname = gtk_file_chooser_get_filename(chooser);
	gtk_file_chooser_set_filename(chooser, fname);
	printf("fname chosen: %s\n", fname);
	g_free(fname);

	/* if Next button activated, then behave as if it's pressed */
}

static void 
on_unlock_renderer_clicked(GcrUnlockRenderer *unlock, 
                           gpointer user_data)
{
	GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (user_data);
        GError *error = NULL;
        GBytes *data;
     
        gcr_parser_add_password(self->parser,  _gcr_unlock_renderer_get_password(unlock));
        data = _gcr_unlock_renderer_get_locked_data (unlock);
        if (gcr_parser_parse_bytes (self->parser, data, &error)) {

                gcr_viewer_remove_renderer (self->viewer, GCR_RENDERER (unlock));
                gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(self->page1_file_chooser), FALSE);
	        while (gcr_viewer_count_renderers(self->viewer))
		    gcr_viewer_remove_renderer(self->viewer, gcr_viewer_get_renderer(self->viewer, 0));
                g_object_unref (unlock);

        } else if (g_error_matches (error, GCR_DATA_ERROR, GCR_ERROR_LOCKED)){
                _gcr_unlock_renderer_show_warning (unlock,  _("The password was incorrect"));
                _gcr_unlock_renderer_focus_password (unlock);
                _gcr_unlock_renderer_set_password (unlock, "");
                g_error_free (error);

        } else {
                _gcr_unlock_renderer_show_warning (unlock, error->message);
                g_error_free (error);
        }

      
}

static gboolean 
on_parser_authenticate_for_data(GcrParser *parser,
                         gint count,
                         gpointer *data_main)
{
     
	GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data_main);
        GcrUnlockRenderer *unlock;
        gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(self->page1_file_chooser), TRUE);

	while (gcr_viewer_count_renderers(self->viewer))
		gcr_viewer_remove_renderer(self->viewer, gcr_viewer_get_renderer(self->viewer, 0));

        unlock = _gcr_unlock_renderer_new_for_parsed(parser);
        if(unlock != NULL) {
               g_object_set(G_OBJECT(unlock), "label", "Please Unlock");
               gcr_viewer_add_renderer(self->viewer, GCR_RENDERER(unlock));
               g_signal_connect(unlock, "unlock-clicked", G_CALLBACK(on_unlock_renderer_clicked), self);
       }

       return TRUE;
}   
    
static void
on_page1_update_preview(GtkWidget *widget, gpointer *data_main)
{
	GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data_main);
	GtkFileChooser *chooser = GTK_FILE_CHOOSER(widget);
	
        GError *err = NULL;
        guchar *data;
        gsize n_data;
        GBytes *bytes;
        gtk_label_set_text(GTK_LABEL(self->cert_label), "No certificate selected yet");
        gtk_label_set_text(GTK_LABEL(self->key_label), "No key selected yet");
    
	gtk_file_chooser_set_preview_widget_active(chooser, FALSE);
	char *filename = gtk_file_chooser_get_preview_filename(chooser);
	gtk_widget_set_sensitive(GTK_WIDGET(self->next_button), FALSE);
	if (!filename || g_file_test(filename, G_FILE_TEST_IS_DIR))
		return;
        
        if(!g_file_get_contents (filename, (gchar**)&data, &n_data, NULL))
            printf("couldn't read file");

        
        bytes = g_bytes_new_take(data, n_data); 
        if (!gcr_parser_parse_bytes (self->parser, bytes, &err))
            printf("couldn't parse data: %s", err->message);
       
     
	if (self->certificate_uri && g_strcmp0(self->certificate_uri, filename) != 0) {
		g_free(self->certificate_uri);
		self->certificate_uri = NULL;
	}

	printf("Preview %s\n", filename);
	g_free(filename);
}
static void
gcr_certificate_chooser_dialog_constructed (GObject *obj)
{
	GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (obj);
        GtkWidget  *content;
        gtk_window_set_title(GTK_WINDOW(self), "Certificate Chooser");
        G_OBJECT_CLASS (gcr_certificate_chooser_dialog_parent_class)->constructed (obj);

	content = GTK_WIDGET(GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))));

	self->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
        gtk_container_add(GTK_CONTAINER(content), GTK_WIDGET(self->hbox));
	self->vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_container_add(GTK_CONTAINER(self->hbox), GTK_WIDGET(self->vbox1));
        self->viewer = gcr_viewer_new();

	self->cert_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_container_add(GTK_CONTAINER(self->vbox1), GTK_WIDGET(self->cert_hbox));
	self->cert_image = gtk_image_new_from_gicon(g_themed_icon_new(GCR_ICON_CERTIFICATE), GTK_ICON_SIZE_DIALOG);
	gtk_container_add(GTK_CONTAINER(self->cert_hbox), GTK_WIDGET(self->cert_image));
	self->cert_label = gtk_label_new("No certificate selected yet");
	gtk_container_add(GTK_CONTAINER(self->cert_hbox), GTK_WIDGET(self->cert_label));

	self->key_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
	gtk_container_add(GTK_CONTAINER(self->vbox1), GTK_WIDGET(self->key_hbox));
	self->key_image = gtk_image_new_from_gicon(g_themed_icon_new(GCR_ICON_KEY), GTK_ICON_SIZE_DIALOG);
	gtk_container_add(GTK_CONTAINER(self->key_hbox), GTK_WIDGET(self->key_image));
	self->key_label = gtk_label_new("No key selected yet");
	gtk_container_add(GTK_CONTAINER(self->key_hbox), GTK_WIDGET(self->key_label));

        self->notebook = gtk_stack_new();
        gtk_stack_set_transition_type(GTK_STACK(self->notebook),GTK_STACK_TRANSITION_TYPE_NONE);
        gtk_container_add(GTK_CONTAINER(self->hbox), GTK_WIDGET(self->notebook));

	GtkFileFilter *filefilter = gtk_file_filter_new();
	gtk_file_filter_set_name (filefilter,"X.509 Certificate Format");
	gtk_file_filter_add_pattern (filefilter, "*.pem");
	gtk_file_filter_add_pattern (filefilter, "*.crt");
	gtk_file_filter_add_pattern (filefilter, "*.cer");
	gtk_file_filter_add_pattern (filefilter, "*.der");
	gtk_file_filter_add_pattern (filefilter, "*.crt");
	gtk_file_filter_add_pattern (filefilter, "*.p12");
	gtk_file_filter_add_pattern (filefilter, "*.pfx");

        /*Page1 Construction */
	self->default_button = gtk_button_new();
	g_signal_connect(GTK_WIDGET(self->default_button), "clicked", G_CALLBACK(on_default_button_clicked), self);
        gtk_container_add(GTK_CONTAINER(content), GTK_WIDGET(self->default_button));
	
        self->next_button = gtk_button_new_with_label("Next");
        self->page1_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        self->page1_stack = gtk_stack_new();
        self->page1_file_chooser = gtk_file_chooser_widget_new(GTK_FILE_CHOOSER_ACTION_OPEN);
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (self->page1_file_chooser), filefilter);
        gtk_stack_set_transition_type(GTK_STACK(self->page1_stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
        gtk_stack_set_transition_duration(GTK_STACK(self->page1_stack), 1000);
        gtk_container_set_border_width(GTK_CONTAINER(self->page1_box), 1);
	gtk_stack_add_named(GTK_STACK(self->notebook), GTK_WIDGET(self->page1_box), "Page1");
        gtk_stack_add_titled(GTK_STACK(self->page1_stack), GTK_WIDGET(self->page1_file_chooser), "Files", "Chooser From File");
        gtk_stack_add_titled(GTK_STACK(self->page1_stack), GTK_WIDGET(gtk_label_new("Chooser after some time :)")), "Pkcs#11", "Chooser From PKCS#11");
        self->page1_stack_switcher = gtk_stack_switcher_new();
        gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(self->page1_stack_switcher), GTK_STACK(self->page1_stack));
        gtk_box_pack_start(GTK_BOX(self->page1_box), self->page1_stack_switcher, TRUE, TRUE, 0);
        gtk_box_pack_start(GTK_BOX(self->page1_box), self->page1_stack, TRUE, TRUE, 0);

	gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER (self->page1_file_chooser), GTK_WIDGET (self->viewer));

	g_signal_connect(GTK_FILE_CHOOSER (self->page1_file_chooser), "update-preview", G_CALLBACK (on_page1_update_preview), self);
	g_signal_connect(GTK_FILE_CHOOSER (self->page1_file_chooser), "file-activated", G_CALLBACK (on_page1_file_activated), self);
        gtk_file_chooser_set_extra_widget(GTK_FILE_CHOOSER(self->page1_file_chooser), self->next_button);
	gtk_widget_set_sensitive(GTK_WIDGET(self->next_button), FALSE);
        if(!gtk_file_chooser_get_preview_filename(GTK_FILE_CHOOSER(self->page1_file_chooser))) 
	    gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(self->page1_file_chooser), FALSE);

        g_signal_connect(GTK_WIDGET(self->next_button), "clicked", G_CALLBACK(on_next_button_clicked), self);
	gtk_widget_show_all(GTK_WIDGET (self));
	gtk_widget_hide(GTK_WIDGET(self->default_button));
	gtk_widget_set_can_default(GTK_WIDGET(self->default_button), TRUE);
	gtk_widget_grab_default(GTK_WIDGET(self->default_button));
        self->parser = gcr_parser_new();
        g_signal_connect(self->parser, "parsed", G_CALLBACK(on_certificate_choosed), self);
        g_signal_connect(self->parser, "authenticate", G_CALLBACK(on_parser_authenticate_for_data), self);
       
        /*Page2 Construction */
        /*Page3 Construction */

	/* Add our various buttons */
	/*button = gtk_dialog_add_button (GTK_DIALOG (self), _("_Next"), GTK_RESPONSE_HELP);
	gtk_button_set_use_underline (GTK_BUTTON (button), TRUE)ignal_handler_disconnect (self->pv->parser, sig);

	button = gtk_dialog_add_button (GTK_DIALOG (self), _("_OK"), GTK_RESPONSE_OK);
	gtk_button_set_use_underline (GTK_BUTTON (button), TRUE);
	gtk_dialog_set_default_response (GTK_DIALOG (self), GTK_RESPONSE_OK);
*/
       
	gtk_window_set_modal (GTK_WINDOW (self), TRUE);
}

static void
gcr_certificate_chooser_dialog_init (GcrCertificateChooserDialog *self)
{

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
	                       "transient-for", parent,
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

