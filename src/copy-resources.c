#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define STRICT
#include <windows.h>
#undef STRICT

#include <glib.h>

static BOOL CALLBACK
enum_languages (HMODULE  source,
		LPCTSTR  type,
		LPCTSTR  name,
		WORD     language,
		LONG     param)
{
  HRSRC hrsrc;
  HGLOBAL reshandle;
  WORD *resp;
  int size;
#if 0
  printf ("lang=%#x ", language);

  if (HIWORD (type) == 0)
    printf ("%d ", (int) type);
  else
    printf ("%s ", type);

  if (HIWORD (name) == 0)
    printf ("%d\n", (int) name);
  else
    printf ("%s\n", name);
#endif
  if ((hrsrc = FindResource (source, name, type)) == NULL)
    {
      fprintf (stderr, "FindResource failed: %s\n",
	       g_win32_error_message (GetLastError ()));
      return TRUE;
    }

  size = SizeofResource (source, hrsrc);

  if ((reshandle = LoadResource (source, hrsrc)) == NULL)
    {
      fprintf (stderr, "LoadResource() failed: %s\n",
	       g_win32_error_message (GetLastError ()));
      return TRUE;
    }

  if ((resp = LockResource (reshandle)) == 0)
    {
      fprintf (stderr, "LockResource() failed: %s",
	       g_win32_error_message (GetLastError ()));
      return TRUE;
    }

  if (!UpdateResource ((HANDLE) param, type, name, language, resp, size))
    {
      fprintf (stderr, "UpdateResource() failed: %s\n",
	       g_win32_error_message (GetLastError ()));
      return TRUE;
    }

  return TRUE;
}

static BOOL CALLBACK
enum_names (HMODULE  source,
	    LPCTSTR  type,
	    LPTSTR   name,
	    LONG     param)
{

  if (EnumResourceLanguages (source, type, name, enum_languages, param) == 0)
    fprintf (stderr, "EnumResourceLanguages() failed: %s\n",
	     g_win32_error_message (GetLastError ()));
  return TRUE;
}

BOOL CALLBACK
enum_types (HMODULE source,
	    LPTSTR  type,
	    LONG    param)
{
  if (EnumResourceNames (source, type, enum_names, param) == 0)
    fprintf (stderr, "EnumResourceNames() failed: %s\n",
	     g_win32_error_message (GetLastError ()));
  return TRUE;
}

int
main (int    argc,
      char **argv)
{
  HMODULE source;
  HANDLE target;

  if (argc != 3)
    {
      fprintf (stderr, "Usage: %s source target\n", argv[0]);
      return 1;
    }
 
  if ((source = LoadLibrary (argv[1])) == NULL)
    {
      fprintf (stderr, "LoadLibrary() failed: %s\n",
	       g_win32_error_message (GetLastError ()));
      return 1;
    }

  if ((target = BeginUpdateResource (argv[2], TRUE)) == NULL)
    {
      fprintf (stderr, "BeginUpdateResource() failed: %s\n",
	       g_win32_error_message (GetLastError ()));
      return 1;
    }

  if (EnumResourceTypes (source, enum_types, (LONG) target) == 0)
    {
      fprintf (stderr, "EnumResourceTypes() failed: %s\n",
	       g_win32_error_message (GetLastError ()));
      return 1;
    }

  if (!EndUpdateResource (target, FALSE))
    {
      fprintf (stderr, "EndUpdateResource() failed: %s\n",
	       g_win32_error_message (GetLastError ()));
      return 1;
    }

  FreeLibrary (source);

  return 0;
}
