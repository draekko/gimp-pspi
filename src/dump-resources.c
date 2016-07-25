#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define STRICT
#include <windows.h>
#undef STRICT

#include <glib.h>

#ifndef G_OS_WIN32

/* For winegcc compilation on Linux. Lifted from GLib. */

#define G_WIN32_HAVE_WIDECHAR_API() 1

/**
 * g_win32_error_message:
 * @error: error code.
 *
 * Translate a Win32 error code (as returned by GetLastError()) into
 * the corresponding message. The message is either language neutral,
 * or in the thread's language, or the user's language, the system's
 * language, or US English (see docs for FormatMessage()). The
 * returned string is in UTF-8. It should be deallocated with
 * g_free().
 *
 * Returns: newly-allocated error message
 **/
static gchar *
g_win32_error_message (gint error)
{
	gchar *retval;

	if (G_WIN32_HAVE_WIDECHAR_API ())
		{
			wchar_t *msg = NULL;
			int nchars;

			FormatMessageW (FORMAT_MESSAGE_ALLOCATE_BUFFER
			                |FORMAT_MESSAGE_IGNORE_INSERTS
			                |FORMAT_MESSAGE_FROM_SYSTEM,
			                NULL, error, 0,
			                (LPWSTR) &msg, 0, NULL);
			if (msg != NULL)
				{
					nchars = wcslen (msg);

					if (nchars > 2 && msg[nchars-1] == '\n' && msg[nchars-2] == '\r')
						msg[nchars-2] = '\0';

					retval = g_utf16_to_utf8 (msg, -1, NULL, NULL, NULL);

					LocalFree (msg);
				}
			else
				retval = g_strdup ("");
		}
	else
		{
			gchar *msg = NULL;
			int nbytes;

			FormatMessageA (FORMAT_MESSAGE_ALLOCATE_BUFFER
			                |FORMAT_MESSAGE_IGNORE_INSERTS
			                |FORMAT_MESSAGE_FROM_SYSTEM,
			                NULL, error, 0,
			                (LPTSTR) &msg, 0, NULL);
			if (msg != NULL)
				{
					nbytes = strlen (msg);

					if (nbytes > 2 && msg[nbytes-1] == '\n' && msg[nbytes-2] == '\r')
						msg[nbytes-2] = '\0';

					retval = g_locale_to_utf8 (msg, -1, NULL, NULL, NULL);

					LocalFree (msg);
				}
			else
				retval = g_strdup ("");
		}

	return retval;
}

#endif

static BOOL CALLBACK
enum_languages (HMODULE  dll,
                LPCTSTR  type,
                LPCTSTR  name,
                WORD     language,
                LONG     param)
{
	HRSRC hrsrc;
	HGLOBAL reshandle;
	WORD *resp;
	int i, size;

	printf ("LANGUAGE %d, %d\n", PRIMARYLANGID (language), SUBLANGID (language));

	if (HIWORD (type) == 0)
		printf ("%d ", (int) type);
	else
		printf ("%s ", type);

	if (HIWORD (name) == 0)
		printf ("%d {\n", (int) name);
	else
		printf ("%s {\n", name);

	if ((hrsrc = FindResource (dll, name, type)) == NULL)
		{
			fprintf (stderr, "FindResource failed: %s\n",
			         g_win32_error_message (GetLastError ()));
			return TRUE;
		}

	size = SizeofResource (dll, hrsrc);

	if ((reshandle = LoadResource (dll, hrsrc)) == NULL)
		{
			fprintf (stderr, "LoadResource failed: %s\n",
			         g_win32_error_message (GetLastError ()));
			return TRUE;
		}

	if ((resp = LockResource (reshandle)) == 0)
		{
			fprintf (stderr, "LockResource failed: %s\n",
			         g_win32_error_message (GetLastError ()));
			return TRUE;
		}

	for (i = 0; i < size; i += 2)
		{
			printf ("  %#x", *resp++);
			if (i < size - 2)
				printf (",");
			printf ("\n");
		}

	printf ("}\n\n");

	return TRUE;
}

static BOOL CALLBACK
enum_names (HMODULE  dll,
            LPCTSTR  type,
            LPTSTR   name,
            LONG     param)
{

	if (EnumResourceLanguages (dll, type, name, enum_languages, param) == 0)
		fprintf (stderr, "EnumResourceLanguages() failed: %s\n",
		         g_win32_error_message (GetLastError ()));
	return TRUE;
}

BOOL CALLBACK
enum_types (HMODULE dll,
            LPTSTR  type,
            LONG    param)
{
	if (EnumResourceNames (dll, type, enum_names, param) == 0)
		fprintf (stderr, "EnumResourceNames() failed: %s\n",
		         g_win32_error_message (GetLastError ()));
	return TRUE;
}

int
main (int    argc,
      char **argv)
{
	HMODULE dll;

	if (argc != 2)
		{
			fprintf (stderr, "Usage: %s plugin.8bf\n", argv[0]);
			return 1;
		}

	if ((dll = LoadLibrary (argv[1])) == NULL)
		{
			fprintf (stderr, "LoadLibrary() failed: %s\n",
			         g_win32_error_message (GetLastError ()));
			return 1;
		}

	if (EnumResourceTypes (dll, enum_types, 0) == 0)
		fprintf (stderr, "EnumResourceTypes() failed: %s\n",
		         g_win32_error_message (GetLastError ()));

	FreeLibrary (dll);

	return 0;
}
