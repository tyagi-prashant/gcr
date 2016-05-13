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

#include "gcr-dialog-util.h"
#include "gcr-secure-entry-buffer.h"
#include "gcr-certificate-chooser-dialog.h"

#include "egg/egg-secure-memory.h"

#include <gtk/gtk.h>

#include <glib/gi18n-lib.h>

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
	GtkBuilder *builder;
	GtkWidget *password_area;
	GtkLabel *token_label;
	GtkImage *token_image;
	GtkEntry *password_entry;
	GtkEntry *label_entry;
	gboolean label_changed;
};

typedef struct _GcrCertificateChooserDialogClass GcrCertificateChooserDialogClass;

struct _GcrCertificateChooserDialogClass {
	GtkDialogClass parent;
};

G_DEFINE_TYPE (GcrCertificateChooserDialog, gcr_certificate_chooser_dialog, GTK_TYPE_DIALOG);

static void
on_label_changed (GtkEditable *editable,
                  gpointer user_data)
{
	GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (user_data);
	self->label_changed = TRUE;
}

static void
gcr_certificate_chooser_dialog_constructed (GObject *obj)
{
	GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (obj);
	GError *error = NULL;
	GtkEntryBuffer *buffer;
	GtkWidget *widget;
	GtkBox *contents;
	GtkWidget *button;

	G_OBJECT_CLASS (gcr_certificate_chooser_dialog_parent_class)->constructed (obj);

	if (!gtk_builder_add_from_file (self->builder, UIDIR "gcr-pkcs11-import-dialog.ui", &error)) {
		g_warning ("couldn't load ui builder file: %s", error->message);
		return;
	}

	/* Fill in the dialog from builder */
	widget = GTK_WIDGET (gtk_builder_get_object (self->builder, "pkcs11-import-dialog"));
	contents = GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (self)));
	gtk_box_pack_start (contents, widget, TRUE, TRUE, 0);

	/* The password area */
	self->password_area = GTK_WIDGET (gtk_builder_get_object (self->builder, "unlock-area"));
	gtk_widget_hide (self->password_area);

	/* Add a secure entry */
	buffer = gcr_secure_entry_buffer_new ();
	self->password_entry = GTK_ENTRY (gtk_builder_get_object (self->builder, "password-entry"));
	gtk_entry_set_buffer (self->password_entry, buffer);
	gtk_entry_set_activates_default (self->password_entry, TRUE);
	g_object_unref (buffer);

	self->token_label = GTK_LABEL (gtk_builder_get_object (self->builder, "token-description"));
	self->token_image = GTK_IMAGE (gtk_builder_get_object (self->builder, "token-image"));

	/* Setup the label */
	self->label_entry = GTK_ENTRY (gtk_builder_get_object (self->builder, "label-entry"));
	g_signal_connect (self->label_entry, "changed", G_CALLBACK (on_label_changed), self);
	gtk_entry_set_activates_default (self->label_entry, TRUE);

	/* Add our various buttons */
	button = gtk_dialog_add_button (GTK_DIALOG (self), _("_Cancel"), GTK_RESPONSE_CANCEL);
	gtk_button_set_use_underline (GTK_BUTTON (button), TRUE);
	button = gtk_dialog_add_button (GTK_DIALOG (self), _("_OK"), GTK_RESPONSE_OK);
	gtk_button_set_use_underline (GTK_BUTTON (button), TRUE);
	gtk_dialog_set_default_response (GTK_DIALOG (self), GTK_RESPONSE_OK);

	gtk_window_set_modal (GTK_WINDOW (self), TRUE);
}

static void
gcr_certificate_chooser_dialog_init (GcrCertificateChooserDialog *self)
{
	self->builder = gtk_builder_new ();
}

static void
gcr_certificate_chooser_dialog_finalize (GObject *obj)
{
	GcrCertificateChooserDialog *self = GCR_CERTIFICATE_CHOOSER_DIALOG (obj);

	g_object_unref (self->builder);

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

