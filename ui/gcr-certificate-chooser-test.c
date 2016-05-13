/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* gcr-viewer-tool.c: Command line utility

   Copyright (C) 2011 Collabora Ltd.

   The Gnome Keyring Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Keyring Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   see <http://www.gnu.org/licenses/>.

   Author: Stef Walter <stefw@collabora.co.uk>
*/

#include "config.h"

#include "gcr-certificate-chooser-dialog.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <locale.h>
#include <stdlib.h>
#include <string.h>

static gboolean
print_version_and_exit (const gchar *option_name, const gchar *value,
                        gpointer data, GError **error)
{
	g_print("%s -- %s\n", _("GCR Certificate and Key Viewer"), VERSION);
	exit (0);
	return TRUE;
}

static const GOptionEntry options[] = {
	{ "version", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
	  print_version_and_exit, N_("Show the application's version"), NULL},
	{ NULL }
};

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	GcrCertificateChooserDialog *dialog;

#ifdef HAVE_LOCALE_H
	/* internationalisation */
	setlocale (LC_ALL, "");
#endif

#ifdef HAVE_GETTEXT
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
	textdomain (GETTEXT_PACKAGE);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
#endif

	context = g_option_context_new (N_("- Test certificate chooser dialog"));
	g_option_context_set_translation_domain (context, GETTEXT_PACKAGE);
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);

	g_option_context_add_group (context, gtk_get_option_group (TRUE));

	if (! g_option_context_parse (context, &argc, &argv, &error)) {
		g_critical ("Failed to parse arguments: %s", error->message);
		g_error_free (error);
		g_option_context_free (context);
		return 1;
	}

	g_option_context_free (context);
	g_set_application_name (_("Certificate Chooser"));

	gtk_init (&argc, &argv);

	dialog = gcr_certificate_chooser_dialog_new (NULL);
	gcr_certificate_chooser_dialog_run(dialog);

	//	gtk_main ();

	return 0;
}
