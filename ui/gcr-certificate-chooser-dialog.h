/*
 * gnome-keyring
 *
 * Copyright (C) 2008 Stefan Walter
 * Copyright (C) 2011 Collabora Ltd.
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
 *
 * Author: Stef Walter <stefw@collabora.co.uk>
 */

#ifndef __GCR_CERTIFICATE_CHOOSER_DIALOG_H__
#define __GCR_CERTIFICATE_CHOOSER_DIALOG_H__


#include  "gck/gck.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GCR_TYPE_CERTIFICATE_CHOOSER_DIALOG               (gcr_certificate_chooser_dialog_get_type ())
#define GCR_CERTIFICATE_CHOOSER_DIALOG(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_CERTIFICATE_CHOOSER_DIALOG, GcrCertificateChooserDialog))
#define GCR_IS_CERTIFICATE_CHOOSER_DIALOG(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_CERTIFICATE_CHOOSER_DIALOG))

#define GCR_TYPE_CERTIFICATE_CHOOSER_SIDEBAR               (gcr_certificate_chooser_sidebar_get_type ())
#define GCR_CERTIFICATE_CHOOSER_SIDEBAR(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_CERTIFICATE_CHOOSER_SIDEBAR, GcrCertificateChooserSidebar))
#define GCR_IS_CERTIFICATE_CHOOSER_SIDEBAR(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_CERTIFICATE_CHOOSER_SIDEBAR))

#define GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11              (gcr_certificate_chooser_pkcs11_get_type ())
#define GCR_CERTIFICATE_CHOOSER_PKCS11(obj)         (G_TYPE_CHECK_INSTANCE_CAST ((obj), GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11, GcrCertificateChooserPkcs11))
#define GCR_IS_CERTIFICATE_CHOOSER_PKCS11(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GCR_TYPE_CERTIFICATE_CHOOSER_PKCS11))

typedef struct _GcrCertificateChooserDialog GcrCertificateChooserDialog;
typedef struct _GcrCertificateChooserPkcs11 GcrCertificateChooserPkcs11;
typedef struct _GcrCertificateChooserSidebar GcrCertificateChooserSidebar;

GType                   gcr_certificate_chooser_dialog_get_type     (void) G_GNUC_CONST;

GType                   gcr_certificate_chooser_sidebar_get_type     (void) G_GNUC_CONST;

GType                   gcr_certificate_chooser_pkcs11_get_type     (void) G_GNUC_CONST;

GList                   *stored_uri = NULL;

GList                   *stored_password = NULL;

gchar                   *cert_uri = NULL;

gchar                   *cert_password = NULL;

gchar                   *key_uri = NULL;

gchar                   *key_password = NULL;

GcrCertificateChooserDialog * gcr_certificate_chooser_dialog_new    (GtkWindow *parent);

GcrCertificateChooserSidebar * gcr_certificate_chooser_sidebar_new    (void);

GcrCertificateChooserPkcs11 * gcr_certificate_chooser_pkcs11_new    (GckSlot *slot, gboolean is_certificate_choosen);

gboolean                gcr_certificate_chooser_dialog_run               (GcrCertificateChooserDialog *self);

void                    gcr_certificate_chooser_dialog_run_async         (GcrCertificateChooserDialog *self,
                                                                     GCancellable *cancellable,
                                                                     GAsyncReadyCallback callback,
                                                                     gpointer user_data);

gboolean                gcr_certificate_chooser_dialog_run_finish        (GcrCertificateChooserDialog *self,
                                                                     GAsyncResult *result);

G_END_DECLS

#endif /* __GCR_CERTIFICATE_CHOOSER_DIALOG_H__ */
