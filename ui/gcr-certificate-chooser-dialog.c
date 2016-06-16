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



enum {
	SIGNAL_0,
	SIGNAL_LOADED
};

static gint certificate_chooser_signals[SIGNAL_LOADED + 1] = { 0 };

struct _GcrCertificateChooserDialog {
       GtkDialog parent;
       GList *tokens;
       GtkListStore *store;
       GList *blacklist;
       gboolean loaded;
       GtkBuilder *builder;
       GcrViewer *page1_viewer;
       GcrViewer *page2_viewer;
       GtkWidget *hbox;
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

        /* Signals */
        
         void (*token_loaded) (GcrCertificateChooserDialog *sig);
};

static const char *token_blacklist[] = { 
        "pkcs11:manufacturer=Gnome%20Keyring;serial=1:SSH:HOME",
        "pkcs11:manufacturer=Gnome%20Keyring;serial=1:SECRET:MAIN",
        "pkcs11:manufacturer=Mozilla%20Foundation;token=NSS%20Generic%20Crypto%20Services",
         NULL
};

enum {
        ROW_TYPE,
        TOKEN_LABEL
};
        

enum {
      
        COLUMN_STRING,
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

        gchar *label;
        gtk_tree_model_get (model, iter, COLUMN_STRING, &label, -1);
        g_object_set(cell, 
                     "visible", TRUE,
                     "text", label,
                     NULL);
}
    
static void 
gcr_certificate_chooser_sidebar_constructed (GObject *obj)
{
        GcrCertificateChooserSidebar *self = GCR_CERTIFICATE_CHOOSER_SIDEBAR(obj);
        //GtkTreeSelection *selection;
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
on_page3_previous_button_clicked(GtkWidget *widget,
                                 gpointer  *data)
{
       
       GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data);
       gtk_window_set_title(GTK_WINDOW(self), "Choose Key");

       gtk_stack_set_visible_child(GTK_STACK(gtk_builder_get_object(
                                   self->builder, "content-area")),
                                   GTK_WIDGET(gtk_builder_get_object(
                                   self->builder, "page2-box")));

       gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(
                                     gtk_builder_get_object(
                                     self->builder, "page2-filechooser")
                                     ), self->key_uri);
       self->key_uri = NULL;
}

static void 
on_page2_next_button_clicked(GtkWidget *widget,
                             gpointer *data)
{
      
       GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data);
       gtk_window_set_title(GTK_WINDOW(self), "Confirm");
    //   GtkFileChooser *chooser= GTK_FILE_CHOOSER(gtk_builder_get_object(self->builder, "page2-filechooser"));
       //self->key_uri = gtk_file_chooser_get_filename(chooser);

       gtk_stack_set_visible_child(GTK_STACK(gtk_builder_get_object(
                                   self->builder, "content-area")),
                                   GTK_WIDGET(gtk_builder_get_object(
                                   self->builder, "page3-box")));
       //self->key_uri = gtk_file_chooser_get_filename(chooser);
       printf("The key uri is %s\n",self->key_uri);
       
}
static void 
on_page2_previous_button_clicked(GtkWidget *widget,
                                  gpointer *data)
{
      
       GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data);
       gtk_window_set_title(GTK_WINDOW(self), "Choose Certificate");
       self->is_certificate_choosen = FALSE;
       gtk_stack_set_visible_child(GTK_STACK(gtk_builder_get_object(
                                   self->builder, "content-area")),
                                   GTK_WIDGET(gtk_builder_get_object(
                                   self->builder, "page1-box")));
       gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(
                                     gtk_builder_get_object(
                                     self->builder, "page1-filechooser")
                                     ), self->certificate_uri);
       self->certificate_uri = NULL;
}
               
static void
on_next_button_clicked(GtkWidget *widget, gpointer *data)
{
       GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data);
       GtkFileChooserWidget *chooser;
       gchar *fname;
       g_free(self->certificate_uri);
       self->is_certificate_choosen = TRUE;
       gtk_window_set_title(GTK_WINDOW(self), "Choose Key");

       chooser = GTK_FILE_CHOOSER_WIDGET(gtk_builder_get_object(
                                  self->builder, "page1-filechooser"));

       fname = gtk_file_chooser_get_preview_filename(GTK_FILE_CHOOSER(chooser));
       self->certificate_uri = fname;

       gtk_stack_set_visible_child(GTK_STACK(gtk_builder_get_object(
                                   self->builder, "content-area")),
                                   GTK_WIDGET(gtk_builder_get_object(
                                   self->builder, "page2-box")));
       if (self->is_key_choosen) {
           self->key_uri = fname;
           gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(
                                         gtk_builder_get_object(
                                         self->builder, "page2-filechooser")
                                         ), fname);
           gtk_button_clicked(GTK_BUTTON(gtk_builder_get_object(
                              self->builder, "page2-next-button")));
       } else {

           gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(
                                         gtk_builder_get_object(
                                         self->builder, "page2-filechooser")
                                         ),g_file_get_path(g_file_get_parent(
                                         g_file_new_for_path (fname))));
      }
       printf("next-button %s\n", fname);
}

static void
on_default_button_clicked(GtkWidget *widget, gpointer *data)
{
        GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data);
        GtkFileChooserWidget *chooser;

        if (!self->is_certificate_choosen) {        
	         chooser = GTK_FILE_CHOOSER_WIDGET(gtk_builder_get_object(
                                                   self->builder, 
                                                   "page1-filechooser"));
        } else {
	         chooser = GTK_FILE_CHOOSER_WIDGET(gtk_builder_get_object(
                                                   self->builder, 
                                                   "page2-filechooser"));
        }

        gchar *fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (chooser));
        printf("default-button %s\n", fname);

        if (g_file_test(fname, G_FILE_TEST_IS_DIR)) {
	        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (chooser), fname);
	        g_free(fname);
        } else {
	        /* This is used as a flag to the preview/added signals that this was *set* */
                if(!self->is_certificate_choosen)
	            self->certificate_uri = fname;
                else
	            self->key_uri = fname;
	        gtk_file_chooser_set_filename(GTK_FILE_CHOOSER (chooser), fname);
        }
}

static void
on_parser_parsed_item(GcrParser *parser,
                      gpointer  *data)
{        GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data); 
         GckAttributes *attributes;
         char *filename;
         gulong class;
         attributes = gcr_parser_get_parsed_attributes(parser);
     
         if (!self->is_certificate_choosen) {
                 filename = gtk_file_chooser_get_preview_filename(
                                    GTK_FILE_CHOOSER(gtk_builder_get_object(
                                    self->builder, "page1-filechooser")));
             
                 if (self->certificate_uri && g_strcmp0(self->certificate_uri, filename) != 0) {

                        g_free(self->certificate_uri);
		        self->certificate_uri = NULL;
	         }
        
                if (gck_attributes_find_ulong (attributes, CKA_CLASS, &class) && class == CKO_CERTIFICATE) {
		// XXX: Get subject, issuer and expiry and use gtk_label_set_markup to show them nicely
		//gtk_label_set_text(GTK_LABEL(self->cert_label), gcr_parser_get_parsed_label(parser));
                       gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                                   self->builder, "certificate-label")),
                                   gcr_parser_get_parsed_label(parser));

                       gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(
                                                           self->builder,
                                                           "page1-next-button")), TRUE);

	               if (self->certificate_uri) {
	                          gtk_button_clicked(GTK_BUTTON(gtk_builder_get_object(
                                                     self->builder,
                                                     "page1-next-button")));
                       } else 
                             printf("not set\n");
                 }
        } else {
               filename = gtk_file_chooser_get_preview_filename(
                                         GTK_FILE_CHOOSER(gtk_builder_get_object(
                                         self->builder, "page2-filechooser")));
                       
        }

        if (gck_attributes_find_ulong (attributes, CKA_CLASS, &class) && class == CKO_PRIVATE_KEY){
            self->is_key_choosen = TRUE;
	    if (self->key_uri) {
	               gtk_button_clicked(GTK_BUTTON(gtk_builder_get_object(
                                          self->builder,
                                          "page2-next-button")));
            }
	    else 
                    printf("not set\n");
            
            gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                               self->builder, "key-label")),
                               "Key selected");

            gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(
                                     self->builder, "page2-next-button")), TRUE);
        }

     
}        

static void
on_file_activated(GtkWidget *widget, gpointer *data)
{
	GtkFileChooser *chooser = GTK_FILE_CHOOSER(widget);

	/* The user might have hit Ctrl-L and typed in a filename, and this
	   happens when they press Enter. It g_object_newalso happens when they double
	   click a file in the browser. */
	gchar *fname = gtk_file_chooser_get_filename(chooser);
	gtk_file_chooser_set_filename(chooser, fname);
	printf("fname chosen: %s\n", fname);
	g_free(fname);

	/* if Next button activated, then behave as if it's pressed */
}

static gboolean
on_unlock_renderer_clicked(GcrUnlockRenderer *unlock, 
                           gpointer user_data)
{
	GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (user_data);
        GError *error = NULL;
        GBytes *data;
        GcrViewer *viewer;
        GtkFileChooser *chooser;

        if (!self->is_certificate_choosen) {
                 viewer = GCR_VIEWER(self->page1_viewer);
	         chooser = GTK_FILE_CHOOSER(gtk_builder_get_object(
                                            self->builder, 
                                            "page1-filechooser"));
        } else {
                 viewer = GCR_VIEWER(self->page2_viewer);
	         chooser = GTK_FILE_CHOOSER(gtk_builder_get_object(
                                            self->builder, 
                                            "page2-filechooser"));
        }
     
        gcr_parser_add_password(self->parser,  _gcr_unlock_renderer_get_password(unlock));
        data = _gcr_unlock_renderer_get_locked_data (unlock);
        if (gcr_parser_parse_bytes (self->parser, data, &error)) {

                gcr_viewer_remove_renderer (viewer, GCR_RENDERER (unlock));
                gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(chooser), FALSE);
                g_object_unref (unlock);

        } 
        return TRUE;

      
}

static gboolean 
on_parser_authenticate_for_data(GcrParser *parser,
                         gint count,
                         gpointer *data_main)
{
     
	GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (data_main);
        GcrUnlockRenderer *unlock;
        GcrViewer *viewer;
        GtkFileChooserWidget *chooser;

        if (!self->is_certificate_choosen) {
                 viewer = GCR_VIEWER(self->page1_viewer);
	         chooser = GTK_FILE_CHOOSER_WIDGET(gtk_builder_get_object(
                                                   self->builder, 
                                                   "page1-filechooser"));
        } else {
                 viewer = GCR_VIEWER(self->page2_viewer);
	         chooser = GTK_FILE_CHOOSER_WIDGET(gtk_builder_get_object(
                                                   self->builder, 
                                                   "page2-filechooser"));
        }
       
        gtk_file_chooser_set_preview_widget_active(GTK_FILE_CHOOSER(chooser), TRUE);

	while (gcr_viewer_count_renderers(viewer))
		gcr_viewer_remove_renderer(viewer, gcr_viewer_get_renderer(viewer, 0));

        unlock = _gcr_unlock_renderer_new_for_parsed(parser);
        if(unlock != NULL) {
               g_object_set(G_OBJECT(unlock), "label", "Please Unlock", NULL);
               gcr_viewer_add_renderer(viewer, GCR_RENDERER(unlock));
               if(self->password_wrong_count) {
                              _gcr_unlock_renderer_show_warning (unlock,  _("The password was incorrect"));
                              _gcr_unlock_renderer_focus_password (unlock);
                              _gcr_unlock_renderer_set_password (unlock, "");
               }


               g_signal_connect(unlock, "unlock-clicked", G_CALLBACK(on_unlock_renderer_clicked), self);
       }
        self->password_wrong_count += 1;

       return TRUE;
}

static void 
on_page2_update_preview(GtkWidget *widget, 
                        gpointer *user_data)
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

        gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                           self->builder, "key-label")),
                           "No Key selected yet");
    
	gtk_file_chooser_set_preview_widget_active(chooser, FALSE);

	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(
                                 self->builder, "page2-next-button")),
                                 FALSE);

	if (!filename || g_file_test(filename, G_FILE_TEST_IS_DIR))
		return;
        
        if(!g_file_get_contents (filename, (gchar**)&data, &n_data, NULL))
            printf("couldn't read file");

        
        bytes = g_bytes_new_take(data, n_data); 
        if (!gcr_parser_parse_bytes (self->parser, bytes, &err))
            printf("couldn't parse data: %s", err->message);

	printf("Preview %s\n", filename);
	g_free(filename);

}

static void
on_page1_update_preview(GtkWidget *widget, gpointer *user_data)
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
        gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                           self->builder, "key-label")),
                           "No key selected yet");
        
        gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                           self->builder, "certificate-label")),
                          "No certificate selected");
    
	gtk_file_chooser_set_preview_widget_active(chooser, FALSE);

	gtk_widget_set_sensitive(GTK_WIDGET(gtk_builder_get_object(
                                 self->builder, "page1-next-button")),
                                  FALSE);
	if (!filename || g_file_test(filename, G_FILE_TEST_IS_DIR))
		return;
        
        if(!g_file_get_contents (filename, (gchar**)&data, &n_data, NULL))
            printf("couldn't read file");

        
        bytes = g_bytes_new_take(data, n_data); 
        if (!gcr_parser_parse_bytes (self->parser, bytes, &err))
            printf("couldn't parse data: %s", err->message);
       
     
	if (self->certificate_uri && g_strcmp0(self->certificate_uri,
                                               filename) != 0) {
		g_free(self->certificate_uri);
		self->certificate_uri = NULL;
	}

	printf("Preview %s\n", filename);
	g_free(filename);
}

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
        self->tokens = NULL;

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
                        if (is_token_usable (self, s->data, token)) { 
                               GcrCertificateChooserPkcs11 *data = gcr_certificate_chooser_pkcs11_new(s->data);
                               self->tokens = g_list_append (self->tokens, data);
                        }
                        gck_token_info_free (token);
                }

               gck_list_unref_free (slots);
        }

        self->loaded = TRUE;
        g_signal_emit (G_OBJECT (self), certificate_chooser_signals[SIGNAL_LOADED], 0);

        gck_list_unref_free (modules);
        g_object_unref (self);
}
static void 
on_token_load(GObject *obj, gpointer *data)
{
       
	GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (obj);
        GtkTreeIter iter;
        GcrCertificateChooserSidebar *sidebar;
        GList *l;
        GckTokenInfo *info;
        GcrCertificateChooserPkcs11 *pkcs11;
        for(l = self->tokens; l != NULL; l = g_list_next(l)) {
               gtk_list_store_append(self->store, &iter);
               pkcs11 = (GcrCertificateChooserPkcs11 *)l->data;
               info = pkcs11->info;
               gtk_list_store_set(self->store, &iter,COLUMN_STRING, (gchar *)info->label, -1);
               gck_token_info_free (info);
               g_object_unref (pkcs11);
        }
        sidebar = gcr_certificate_chooser_sidebar_new();
        gtk_tree_view_set_model(GTK_TREE_VIEW(sidebar->tree_view),GTK_TREE_MODEL(self->store));
        gtk_container_add(GTK_CONTAINER(gtk_builder_get_object(self->builder, "page1-pkcs11")), GTK_WIDGET(sidebar));
        gtk_widget_show_all(GTK_WIDGET(gtk_builder_get_object(self->builder, "page1-pkcs11")));        
       
}	


static void
gcr_certificate_chooser_dialog_constructed (GObject *obj)
{
	GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (obj);
        GtkWidget *content;
        GError *error = NULL;
        G_OBJECT_CLASS (gcr_certificate_chooser_dialog_parent_class)->constructed (obj);
        gck_modules_initialize_registered_async(NULL, on_initialized_registered,
                                                g_object_ref(self));

        if (!gtk_builder_add_from_file (self->builder, UIDIR "gcr-certificate-chooser-dialog.ui", &error))     {
                  g_warning ("couldn't load ui builder file: %s", error->message);
                  return;
          }


	content = GTK_WIDGET(GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self))));

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
                           "No Key selected");
        
        gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(
                           self->builder, "certificate-label")),
                           "No certificate selected");

        g_signal_connect(self->parser, "parsed",
                         G_CALLBACK(on_parser_parsed_item), self);

        g_signal_connect(self->parser, "authenticate",
                         G_CALLBACK(on_parser_authenticate_for_data), self);

       /* Page1 Construction */
	GtkFileFilter *page1_filefilter = GTK_FILE_FILTER(gtk_builder_get_object(self->builder, "page1-filefilter"));
	gtk_file_filter_set_name (page1_filefilter,"X.509 Certificate Format");

        GtkFileChooserWidget *page1_filechooser = GTK_FILE_CHOOSER_WIDGET(gtk_builder_get_object(self->builder,
                                                    "page1-filechooser"));
	g_signal_connect(GTK_BUTTON(gtk_builder_get_object(
                         self->builder, "default-button")),
                         "clicked", G_CALLBACK(on_default_button_clicked),
                         self);

        gtk_container_add(GTK_CONTAINER(content),
                          GTK_WIDGET(gtk_builder_get_object(
                          self->builder, "default-button")));
	
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(page1_filechooser),page1_filefilter);

	gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER (
                                            page1_filechooser),
                                            GTK_WIDGET (self->page1_viewer));

	g_signal_connect(GTK_FILE_CHOOSER (page1_filechooser),
                         "update-preview", G_CALLBACK (
                          on_page1_update_preview), self);

	g_signal_connect(GTK_FILE_CHOOSER (page1_filechooser),
                         "file-activated", G_CALLBACK (
                          on_file_activated), self);

        g_signal_connect(GTK_WIDGET(gtk_builder_get_object(self->builder,
                         "page1-next-button")), "clicked",
                          G_CALLBACK(on_next_button_clicked), self);

	gtk_widget_grab_default(GTK_WIDGET(gtk_builder_get_object(
                                self->builder, "default-button")));

        /* Page2 Construction */
	GtkFileFilter *page2_filefilter = GTK_FILE_FILTER(gtk_builder_get_object(self->builder, "page2-filefilter"));
	gtk_file_filter_set_name (page2_filefilter,"X.509 Key Format");
       
        GtkFileChooserWidget *page2_file_chooser = GTK_FILE_CHOOSER_WIDGET(gtk_builder_get_object(self->builder,
                                                    "page2-filechooser"));
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (page2_file_chooser),
                                     page2_filefilter);
        
	gtk_file_chooser_set_preview_widget(GTK_FILE_CHOOSER (page2_file_chooser),
                                            GTK_WIDGET (self->page2_viewer));

	g_signal_connect(GTK_FILE_CHOOSER (page2_file_chooser),
                         "update-preview", G_CALLBACK (
                         on_page2_update_preview), self);

	g_signal_connect(GTK_BUTTON (gtk_builder_get_object(
                         self->builder, "page2-prev-button")), 
                         "clicked", G_CALLBACK (
                         on_page2_previous_button_clicked), self);

	g_signal_connect(GTK_BUTTON (gtk_builder_get_object(
                         self->builder, "page2-next-button")), 
                         "clicked", G_CALLBACK (
                         on_page2_next_button_clicked),self);

	g_signal_connect(GTK_FILE_CHOOSER (page2_file_chooser),
                         "file-activated", G_CALLBACK (
                         on_file_activated), self);
       
        
	g_signal_connect(GTK_BUTTON (gtk_builder_get_object(
                         self->builder, "page3-prev-button")),
                         "clicked", G_CALLBACK (
                         on_page3_previous_button_clicked), self);
 
        
	g_signal_connect(self, "token-loaded",
                         G_CALLBACK (on_token_load), self);
    
       
	gtk_widget_show_all(GTK_WIDGET (self));
	gtk_window_set_modal (GTK_WINDOW (self), TRUE);
}

static void
gcr_certificate_chooser_dialog_init (GcrCertificateChooserDialog *self)
{
        GError *error = NULL;
        GckUriData *uri;
        guint i;
        self->loaded = FALSE;
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

        self->store = gtk_list_store_new (N_COLUMNS,
                                          G_TYPE_STRING);
        self->parser = gcr_parser_new();
        self->page1_viewer = gcr_viewer_new();
        self->page2_viewer = gcr_viewer_new();
        self->is_certificate_choosen = FALSE;
        self->password_wrong_count = 0;
        gtk_window_set_title(GTK_WINDOW(self), "Choose Certificate");

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
        certificate_chooser_signals[SIGNAL_LOADED] =
                           g_signal_new("token-loaded",
                                        G_TYPE_FROM_CLASS (klass),
                                        G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                                        G_STRUCT_OFFSET (GcrCertificateChooserDialogClass, token_loaded),
                                        NULL, NULL,
                                        g_cclosure_marshal_VOID__VOID,
                                        G_TYPE_NONE, 0);

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

