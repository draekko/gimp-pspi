#define PING() g_print (G_STRLOC ":%s:\n", __FUNCTION__)

/* pspi -- a GIMP plug-in to interface to Photoshop plug-ins.
 *
 * Copyright (C) 2001 Tor Lillqvist
 * Copyright (C) 2016 Ben Touchette
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#define STRICT
#include <windows.h>
#undef STRICT

#define WINDOWS 1		/* PSSDK headers want these */
#define MSWindows 1

#include <PIGeneral.h>
#include <PIAbout.h>
#include <PIFilter.h>
/*
#include <PIUtilities.h>
#include <PIProperties.h>
*/

#include <libgimp/gimp.h>

#include "main.h"
#include "plugin-intl.h"

#define RECT_NONEMPTY(r) (r.left < r.right && r.top < r.bottom)
#define PRINT_RECT(r) g_print ("%dx%d@%+d%+d", r.right-r.left, r.bottom-r.top, r.right, r.top)

#define PSPI_PARAMETER_TOKEN "pspi-parameter-%s"
#define PSPI_PARAMETER_HGLOBAL_TOKEN "pspi-parameter-hglobal-%s"
#define PSPI_PARAMETER_HGLOBAL_PTR_TOKEN "pspi-parameter-hglobal-ptr-%s"
#define PSPI_DATA_TOKEN "pspi-data-%s"

/* To avoid 'multi-character character constant' warnings, we use this hack: */

#define STRINGIFY(x) STRINGIFY2(x)
#define STRINGIFY2(x) #x
#define PICK_1_4_OF(s) ((((guchar)s[1])<<24)|(((guchar)s[2])<<16)|(((guchar)s[3])<<8)|((guchar)s[4]))
#define MULTIC(x) PICK_1_4_OF(STRINGIFY(x))

#ifndef long2fixed
#define long2fixed(x) (x << 16)
#endif

#ifndef fixed2long
#define fixed2long(x) (x >> 16)
#endif

struct PIentrypoint_
{
	HMODULE dll;
	int (CALLBACK *ep)(short  selector,
	                   void  *pluginParamBlock,
	                   long  *pluginData,
	                   int16 *result);
};

typedef struct
{
	const gchar *file;
	const struct stat *statp;
	PSPlugIn *pspi;
} EnumArg;

typedef struct
{
	gpointer pointer;
	guint size;
} PspiHandle;

typedef enum { NONE, PARAMETERS, PREPARE, START, FINISH } PspiPhase;

static PspiPhase prev_phase = NONE;

static BufferProcs *buffer_procs = NULL;
static ChannelPortProcs *channel_port_procs = NULL;
static PIDescriptorParameters *descriptor_parameters = NULL;
static HandleProcs *handle_procs = NULL;
static ImageServicesProcs *image_services_procs = NULL;
static PropertyProcs *property_procs = NULL;
static ResourceProcs *resource_procs = NULL;
static SPBasicSuite *basic_suite = NULL;

static GHashTable *handles = NULL;

/* Due to the strange design of the Photoshop API (no user data in
 * callbacks) we must keep state in global variables. Blecch.
 */

static GimpDrawable *drawable;
static int32 image_id;
static PlatformData platform;
static FilterRecord filter;
static int32 data;

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

static char *
int32_as_be_4c (int32 i)
{
	const char *cp = (const char *) &i;
	static char buf[5*10];
	static int bufindex = 0;

	char *bufp = buf + bufindex;

	bufp[0] = cp[3];
	bufp[1] = cp[2];
	bufp[2] = cp[1];
	bufp[3] = cp[0];
	bufp[4] = '\0';

	bufindex += 5;
	if (bufindex == sizeof(buf))
		bufindex = 0;

	return bufp;
}

static char *
image_mode_string (int image_mode)
{
	static char s[100];

	switch (image_mode)
		{
		case plugInModeBitmap:
			sprintf (s, "Bitmap");
			break;
		case plugInModeGrayScale:
			sprintf (s, "GrayScale");
			break;
		case plugInModeIndexedColor:
			sprintf (s, "IndexedColor");
			break;
		case plugInModeRGBColor:
			sprintf (s, "RGB");
			break;
		case plugInModeCMYKColor:
			sprintf (s, "CMYK");
			break;
		case plugInModeHSLColor:
			sprintf (s, "HSL");
			break;
		case plugInModeHSBColor:
			sprintf (s, "HSB");
			break;
		case plugInModeMultichannel:
			sprintf (s, "Multichannel");
			break;
		case plugInModeDuotone:
			sprintf (s, "Dutotone");
			break;
		case plugInModeLabColor:
			sprintf (s, "Lab");
			break;
		case plugInModeGray16:
			sprintf (s, "Gray16");
			break;
		case plugInModeRGB48:
			sprintf (s, "RGB48");
			break;
		case plugInModeLab48:
			sprintf (s, "Lab48");
			break;
		case plugInModeCMYK64:
			sprintf (s, "CMYK64");
			break;
		case plugInModeDeepMultichannel:
			sprintf (s, "DeepMultichannel");
			break;
		case plugInModeDuotone16:
			sprintf (s, "Duotone16");
			break;
		default:
			sprintf (s, "plugInMode?? (%d)", image_mode);
			return s;
		}
}

static OSErr
buffer_allocate_proc (int32     size,
                      BufferID *bufferID)
{
	*bufferID = g_malloc (size);
	PSPI_DEBUG (BUFFER_SUITE, g_print (G_STRLOC ":%s: %ld: %p\n", __FUNCTION__, size, *bufferID));

	return noErr;
}

static Ptr
buffer_lock_proc (BufferID bufferID,
                  Boolean  moveHigh)
{
	PSPI_DEBUG (BUFFER_SUITE, g_print (G_STRLOC ":%s: %p\n", __FUNCTION__, bufferID));

	return (Ptr) bufferID;
}

static void
buffer_unlock_proc (BufferID bufferID)
{
	PSPI_DEBUG (BUFFER_SUITE, g_print (G_STRLOC ":%s: %p\n", __FUNCTION__, bufferID));
}

static void
buffer_free_proc (BufferID bufferID)
{
	PSPI_DEBUG (BUFFER_SUITE, g_print (G_STRLOC ":%s: %p\n", __FUNCTION__, bufferID));

	g_free (bufferID);
}

static int32
buffer_space_proc (void)
{
	return 100000000;
}

static OSErr
channel_port_read_pixels_proc (ChannelReadPort        port,
                               const PSScaling       *scaling,
                               const VRect           *writeRect,
                               const PixelMemoryDesc *destination,
                               VRect                 *wroteRect)
{
	PING ();

	return errPlugInHostInsufficient;
}

static OSErr
channel_port_write_base_pixels_proc (ChannelWritePort       port,
                                     const VRect           *writeRect,
                                     const PixelMemoryDesc *source)
{
	PING ();

	return errPlugInHostInsufficient;
}

static OSErr
channel_port_read_port_for_write_port_proc (ChannelReadPort *readPort,
        ChannelWritePort writePort)
{
	PING ();

	return errPlugInHostInsufficient;
}

static gboolean
handle_valid (Handle h)
{
	return handles != NULL && g_hash_table_lookup (handles, h) != NULL;
}

static Handle
handle_new_proc (int32 size)
{
	PspiHandle *result = g_new (PspiHandle, 1);
	result->pointer = g_malloc (size);
	result->size = size;
	PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %ld: %p, %p\n",
	                                   __FUNCTION__,
	                                   size, result, result->pointer));
	if (handles == NULL)
		handles = g_hash_table_new (NULL, NULL);

	g_hash_table_insert (handles, result, result->pointer);

	return (Handle) result;
}

static void
handle_dispose_proc (Handle h)
{
	if (!handle_valid (h))
		{
			if (GlobalSize ((HGLOBAL) h) > 0)
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p is HGLOBAL!\n", __FUNCTION__, h));
					GlobalFree ((HGLOBAL) h);
					return;
				}
			else if (!IsBadReadPtr (h, sizeof (HGLOBAL *)) &&
			         GlobalSize (*(HGLOBAL *) h) > 0)
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p is HGLOBAL*!\n", __FUNCTION__, h));
					GlobalFree (*(HGLOBAL *) h);
					return;
				}
			else
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p INVALID!\n", __FUNCTION__, h));
					return;
				}
		}

	PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p, %p\n",
	                                   __FUNCTION__,
	                                   h, ((PspiHandle *)h)->pointer));
	g_free (((PspiHandle *) h)->pointer);
	g_free ((PspiHandle *) h);
	g_hash_table_remove (handles, h);
}

static int32
handle_get_size_proc (Handle h)
{
	UINT size;

	if (!handle_valid (h))
		{
			if ((size = GlobalSize ((HGLOBAL) h)) > 0)
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p is HGLOBAL!: %d\n",
					                                   __FUNCTION__,
					                                   h, size));
					return size;
				}
			else if (!IsBadReadPtr (h, sizeof (HGLOBAL *)) &&
			         (size = GlobalSize (*(HGLOBAL *) h)) > 0)
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p is HGLOBAL*!: %d\n",
					                                   __FUNCTION__,
					                                   h, size));
					return size;
				}
			else
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p INVALID!\n", __FUNCTION__, h));
					/* No idea... */
					return 0;
				}
		}

	PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p: %d\n",
	                                   __FUNCTION__, h, ((PspiHandle *)h)->size));
	return ((PspiHandle *) h)->size;
}

static OSErr
handle_set_size_proc (Handle h,
                      int32  newSize)
{
	if (!handle_valid (h))
		{
			if (GlobalSize ((HGLOBAL) h) > 0)
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p is HGLOBAL!, %ld\n",
					                                   __FUNCTION__, h, newSize));
					if (GlobalReAlloc ((HGLOBAL) h, newSize, 0) != (HGLOBAL) h)
						return nilHandleErr;
					return noErr;
				}
			else if (!IsBadReadPtr (h, sizeof (HGLOBAL *)) &&
			         GlobalSize (*(HGLOBAL *) h) > 0)
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p is HGLOBAL*!\n", __FUNCTION__, h));
					*((HGLOBAL *) h) = GlobalReAlloc (*(HGLOBAL *) h, newSize, 0);
					return noErr;
				}
			else
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p INVALID, %ld!\n",
					                                   __FUNCTION__, h, newSize));
					return nilHandleErr;
				}
		}

	((PspiHandle *) h)->pointer = g_realloc (((PspiHandle *) h)->pointer, newSize);
	((PspiHandle *) h)->size = newSize;
	PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p, %ld: %p\n",
	                                   __FUNCTION__,
	                                   h, newSize, ((PspiHandle *)h)->pointer));
	return noErr;
}

static Ptr
handle_lock_proc (Handle  h,
                  Boolean moveHigh)
{
	if (!handle_valid (h))
		{
			if (GlobalSize ((HGLOBAL) h) > 0)
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p is HGLOBAL!\n", __FUNCTION__, h));
					return GlobalLock ((HGLOBAL) h);
				}
			else if (!IsBadReadPtr (h, sizeof (HGLOBAL *)) &&
			         GlobalSize (*(HGLOBAL *) h) > 0)
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p is HGLOBAL*!\n", __FUNCTION__, h));
					return GlobalLock (*(HGLOBAL *) h);
				}
			else
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p INVALID!\n", __FUNCTION__, h));
					/* Guess that it still is a pointer to a pointer, sigh... */
					if (!IsBadReadPtr (h, sizeof (gpointer)) &&
					        !IsBadWritePtr (*(gpointer *) h, 8))
						{
							PSPI_DEBUG (HANDLE_SUITE, g_print ("  pointer-to-pointer? %p\n",
							                                   * (Ptr *) h));
							return *(Ptr *) h;
						}
					else
						return NULL;		/* Burn, baby, burn */
				}
		}

	PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p: %p\n",
	                                   __FUNCTION__, h, ((PspiHandle *)h)->pointer));
	return ((PspiHandle *) h)->pointer;
}

static void
handle_unlock_proc (Handle h)
{
	if (!handle_valid (h))
		{
			if (GlobalSize ((HGLOBAL) h) > 0)
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p is HGLOBAL!\n", __FUNCTION__, h));
					GlobalUnlock ((HGLOBAL) h);
					return;
				}
			else if (!IsBadReadPtr (h, sizeof (HGLOBAL *)) &&
			         GlobalSize (*(HGLOBAL *) h) > 0)
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p is HGLOBAL*!\n", __FUNCTION__, h));
					GlobalUnlock (*(HGLOBAL *) h);
					return;
				}
			else
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p INVALID!\n", __FUNCTION__, h));
					return;
				}
		}

	PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p, %p\n",
	                                   __FUNCTION__, h, ((PspiHandle *)h)->pointer));
}

static void
handle_recover_space_proc (int32 size)
{
	PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %ld\n", __FUNCTION__, size));
}

static void
handle_dispose_regular_proc (Handle h)
{
	/* FIXME: What is this supposed to do? */

	if (!handle_valid (h))
		{
			if (GlobalSize ((HGLOBAL) h) > 0)
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p is HGLOBAL!\n", __FUNCTION__, h));
					GlobalFree ((HGLOBAL) h);
					return;
				}
			else if (!IsBadReadPtr (h, sizeof (HGLOBAL *)) &&
			         GlobalSize (*(HGLOBAL *) h) > 0)
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p is HGLOBAL*!\n", __FUNCTION__, h));
					GlobalFree (*(HGLOBAL *) h);
					return;
				}
			else
				{
					PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p INVALID!\n", __FUNCTION__, h));
					return;
				}
		}

	PSPI_DEBUG (HANDLE_SUITE, g_print (G_STRLOC ":%s: %p\n", __FUNCTION__, h));
}

static OSErr
image_services_interpolate_1d_proc (PSImagePlane *source,
                                    PSImagePlane *destination,
                                    Rect         *area,
                                    Fixed        *coords,
                                    int16         method)
{
	PSPI_DEBUG (IMAGE_SERVICES_SUITE, PING ());

	return memFullErr;
}

static OSErr
image_services_interpolate_2d_proc (PSImagePlane *source,
                                    PSImagePlane *destination,
                                    Rect         *area,
                                    Fixed        *coords,
                                    int16         method)
{
	PSPI_DEBUG (IMAGE_SERVICES_SUITE, PING ());

	return memFullErr;
}

static OSErr
image_services_interpolate_1d_multi_proc (PSImageMultiPlane *source,
        PSImageMultiPlane *destination,
        Rect              *area,
        Fixed             *coords,
        int16              method)
{
	PSPI_DEBUG (IMAGE_SERVICES_SUITE, PING ());

	return memFullErr;
}

static OSErr
image_services_interpolate_2d_multi_proc (PSImageMultiPlane *source,
        PSImageMultiPlane *destination,
        Rect              *area,
        Fixed             *coords,
        int16              method)
{
	PSPI_DEBUG (IMAGE_SERVICES_SUITE, PING ());

	return memFullErr;
}

static OSErr
property_get_proc (PIType  signature,
                   PIType  key,
                   int32   index,
                   int32  *simpleProperty,
                   Handle *complexProperty)
{
	PSPI_DEBUG (PROPERTY_SUITE, g_print (G_STRLOC ":%s: %s %s\n",
	                                     __FUNCTION__,
	                                     int32_as_be_4c (signature),
	                                     int32_as_be_4c (key)));

	if (signature != kPhotoshopSignature)
		return errPlugInHostInsufficient;

	if (key == MULTIC (propNumberOfChannels))
		{
			gint num_channels;
			gint32 *channels = gimp_image_get_channels (image_id, &num_channels);
			g_free (channels);
			*simpleProperty = num_channels;
		}
	else if (key == MULTIC (propChannelName))
		{
			gint num_channels;
			gint32 *channels = gimp_image_get_channels (image_id, &num_channels);
			gchar *channel_name;
			gpointer p;

			if (index < 0 || index >= num_channels)
				{
					g_free (channels);
					return errPlugInPropertyUndefined;
				}
			channel_name = gimp_drawable_get_name (channels[index]);
			g_free (channels);
			*complexProperty = handle_new_proc (strlen (channel_name));
			p = handle_lock_proc (*complexProperty, TRUE);
			memcpy (p, channel_name, strlen (channel_name));
			handle_unlock_proc (*complexProperty);
			g_free (channel_name);
		}
	else if (key == MULTIC (propImageMode))
		{
			*simpleProperty = filter.imageMode;
		}
	else if (key == MULTIC (propNumberOfPaths))
		{
			int i;
			gint num_paths;
			gchar **paths = gimp_path_list (image_id, &num_paths);

			for (i = 0; i < num_paths; i++)
				g_free (paths[i]);
			g_free (paths);
			*simpleProperty = num_paths;
		}
	else if (key == MULTIC (propPathName))
		{
			int i;
			gint num_paths;
			gchar **paths = gimp_path_list (image_id, &num_paths);
			gpointer p;

			if (index < 0 || index >= num_paths)
				{
					for (i = 0; i < num_paths; i++)
						g_free (paths[i]);
					g_free (paths);
					return errPlugInPropertyUndefined;
				}
			*complexProperty = handle_new_proc (strlen (paths[index]));
			p = handle_lock_proc (*complexProperty, TRUE);
			memcpy (p, paths[index], strlen (paths[index]));
			handle_unlock_proc (*complexProperty);
			for (i = 0; i < num_paths; i++)
				g_free (paths[i]);
			g_free (paths);
		}
	else
		return errPlugInHostInsufficient;

	return noErr;
}

static OSErr
property_set_proc (PIType signature,
                   PIType key,
                   int32  index,
                   int32  simpleProperty,
                   Handle complexProperty)
{
	PSPI_DEBUG (PROPERTY_SUITE, g_print (G_STRLOC ":%s: %s %s\n",
	                                     __FUNCTION__,
	                                     int32_as_be_4c (signature),
	                                     int32_as_be_4c (key)));

	if (signature != kPhotoshopSignature)
		return errPlugInHostInsufficient;

	if (key == MULTIC (propNumberOfChannels))
		{
		}
	else if (key == MULTIC (propChannelName))
		{
		}
	else if (key == MULTIC (propImageMode))
		{
		}
	else if (key == MULTIC (propNumberOfPaths))
		{
		}
	else
		return errPlugInHostInsufficient;

	return noErr;
}

static int16
resource_count_proc (ResType ofType)
{
	GimpParasite *parasite;
	gchar token[20];
	gint i;

	i = 0;
	while (TRUE)
		{
			sprintf (token, "pspi-res-%s-%d", int32_as_be_4c (ofType), i);
			parasite = gimp_image_parasite_find (image_id, token);
			if (parasite == NULL)
				break;
			gimp_parasite_free (parasite);
			i++;
		}

	PSPI_DEBUG (RESOURCE_SUITE, g_print (G_STRLOC ":%s: %s: %d\n",
	                                     __FUNCTION__,
	                                     int32_as_be_4c (ofType), i));
	return i;
}

static Handle
resource_get_proc (ResType ofType,
                   int16   index)
{
	GimpParasite *parasite;
	gchar token[20];
	Handle result;
	gpointer p;

	sprintf (token, "pspi-res-%s-%d", int32_as_be_4c (ofType), index);
	parasite = gimp_image_parasite_find (image_id, token);

	if (parasite == NULL)
		{
			PSPI_DEBUG (RESOURCE_SUITE, g_print (G_STRLOC ":%s: %s, %d: 0\n",
			                                     __FUNCTION__,
			                                     int32_as_be_4c (ofType), index));
			return (Handle) 0;
		}

	result = handle_new_proc (parasite->size);
	p = handle_lock_proc (result, FALSE);
	memmove (p, parasite->data, parasite->size);
	handle_unlock_proc (result);
	gimp_parasite_free (parasite);

	PSPI_DEBUG (RESOURCE_SUITE, g_print (G_STRLOC ":%s: %s, %d: %p\n",
	                                     __FUNCTION__,
	                                     int32_as_be_4c (ofType), index, result));
	return result;
}

static void
resource_delete_proc (ResType ofType,
                      int16   index)
{
	GimpParasite *parasite;
	gchar token[20];
	gint i;

	PSPI_DEBUG (RESOURCE_SUITE, g_print (G_STRLOC ":%s: %s, %d\n",
	                                     __FUNCTION__,
	                                     int32_as_be_4c (ofType), index));

	sprintf (token, "pspi-res-%s-%d", int32_as_be_4c (ofType), index);
	gimp_image_parasite_detach (image_id, token);

	i = index + 1;
	while (TRUE)
		{
			gchar token2[20];
			sprintf (token, "pspi-res-%s-%d", int32_as_be_4c (ofType), i);
			parasite = gimp_image_parasite_find (image_id, token);
			if (parasite == NULL)
				break;
			sprintf (token2, "pspi-res-%s-%d", int32_as_be_4c (ofType), i - 1);
			gimp_image_attach_new_parasite (image_id, token2, parasite->flags, parasite->size, parasite->data);
			gimp_image_parasite_detach (image_id, token);
			gimp_parasite_free (parasite);
			i++;
		}
}

static OSErr
resource_add_proc (ResType ofType,
                   Handle  data)
{
	gchar token[20];
	gint i;
	gpointer p;
	gint size;

	PSPI_DEBUG (RESOURCE_SUITE, g_print (G_STRLOC ":%s: %s, %p\n",
	                                     __FUNCTION__,
	                                     int32_as_be_4c (ofType), data));

	i = resource_count_proc (ofType);
	sprintf (token, "pspi-res-%s-%d", int32_as_be_4c (ofType), i);
	size = handle_get_size_proc (data);
	p = handle_lock_proc (data, FALSE);

	gimp_image_attach_new_parasite (image_id, token, GIMP_PARASITE_PERSISTENT,
	                                size, p);
	handle_unlock_proc (data);

	return noErr;
}

static OSErr
color_services_proc (ColorServicesInfo *info)
{

	PSPI_DEBUG (COLOR_SERVICES_SUITE, PING ());

	if (info->infoSize < sizeof (ColorServicesInfo))
		return errPlugInHostInsufficient;

	switch (info->selector)
		{
		case plugIncolorServicesChooseColor:
			break;
		case plugIncolorServicesConvertColor:
			break;
		case plugIncolorServicesSamplePoint:
			break;
		case plugIncolorServicesGetSpecialColor:
			break;
		default:
			g_assert_not_reached ();
		}

	PSPI_DEBUG (COLOR_SERVICES_SUITE, PING ());

	return errPlugInHostInsufficient;
}

SPAPI SPErr
SPBasicAcquireSuite (const char  *name,
                     int32   version,
                     const void **suite)
{
	PSPI_DEBUG (SPBASIC_SUITE, g_print (G_STRLOC ":%s: %s %ld\n",
	                                    __FUNCTION__,
	                                    name, version));

	return errPlugInHostInsufficient;
}

SPAPI SPErr
SPBasicReleaseSuite (const char *name,
                     int32  version)
{
	PSPI_DEBUG (SPBASIC_SUITE, g_print (G_STRLOC ":%s: %s %ld\n",
	                                    __FUNCTION__,
	                                    name, version));

	return 0;
}

SPAPI SPBoolean
SPBasicIsEqual (const char *token1,
                const char *token2)
{
	PSPI_DEBUG (SPBASIC_SUITE, g_print (G_STRLOC ":%s: %.4s %.4s\n",
	                                    __FUNCTION__,
	                                    token1, token2));

	return 0;
}

SPAPI SPErr
SPBasicAllocateBlock (size_t   size,
                      void **block )
{
	*block = g_malloc (size);

	PSPI_DEBUG (SPBASIC_SUITE, g_print (G_STRLOC ":%s: %ld: %p\n",
	                                    __FUNCTION__,
	                                    size, *block));

	return 0;
}

SPAPI SPErr
SPBasicFreeBlock (void *block)
{
	PSPI_DEBUG (SPBASIC_SUITE, g_print (G_STRLOC ":%s: %p\n",
	                                    __FUNCTION__,
	                                    block));

	g_free (block);

	return 0;
}

SPAPI SPErr
SPBasicReallocateBlock (void  *block,
                        size_t   newSize,
                        void **newblock)
{
	*newblock = g_realloc (block, newSize);

	PSPI_DEBUG (SPBASIC_SUITE, g_print (G_STRLOC ":%s: %p, %ld: %p\n",
	                                    __FUNCTION__,
	                                    block, newSize, *newblock));

	return 0;
}

SPAPI SPErr
SPBasicUndefined (void)
{
	PSPI_DEBUG (SPBASIC_SUITE, PING ());

	return 0;
}

static Boolean
abort_proc ()
{
	return FALSE;
}

static void
progress_proc (long done,
               long total)
{
	gimp_progress_update ((float) done / total);
}

static void
host_proc (int16  selector,
           int32 *data)
{
	PSPI_DEBUG (MISC_CALLBACKS, g_print (G_STRLOC ":%s: %d %p\n",
	                                     __FUNCTION__, selector, data));
}

static void
process_event_proc (void *event)
{
}

static OSErr
display_pixels_proc (const PSPixelMap *source,
                     const VRect      *srcRect,
                     int32             dstRow,
                     int32             dstCol,
                     void             *platformContext)
{
	HDC hdc = (HDC) platformContext, hmemdc;
	HBITMAP hbitmap, holdbm;
	struct
	{
		BITMAPINFOHEADER bi;
		union
		{
			WORD bmiIndices[256];
			DWORD bmiMasks[3];
			RGBQUAD bmiColors[256];
		} u;
	} bmi;
	guchar *bits;
	int i, x, y, bpp = 0;
	guchar *p, *q;
	int w = srcRect->right - srcRect->left, h = srcRect->bottom - srcRect->top;
	int bpl;

#ifdef PSPI_WITH_DEBUGGING
	if (debug_mask & PSPI_DEBUG_MISC_CALLBACKS)
		{
			g_print (G_STRLOC ":%s: source->bounds:%ld:%ldx%ld:%ld\n",
			         __FUNCTION__,
			         source->bounds.left, source->bounds.right,
			         source->bounds.top, source->bounds.bottom);
			g_print ("  srcRect:%ld:%ldx%ld:%ld\n",
			         srcRect->left, srcRect->right,
			         srcRect->top, srcRect->bottom);
			g_print ("  imageMode:%s", image_mode_string (source->imageMode));
			g_print ("  rowBytes:%ld colBytes:%ld planeBytes:%ld baseAddr:%p\n",
			         source->rowBytes, source->colBytes,
			         source->planeBytes,source->baseAddr);
			g_print ("  hdc:%p\n", hdc);
		}
#endif /* PSPI_WITH_DEBUGGING */

	/* Some plug-ins call displayPixels with bogus parameters */
	if (hdc == NULL || source->rowBytes == 0 || source->baseAddr == 0)
		return filterBadParameters;

	bmi.bi.biSize = sizeof (BITMAPINFOHEADER);
	bmi.bi.biWidth = w;
	bmi.bi.biHeight = -h;
	bmi.bi.biPlanes = 1;
	switch (source->imageMode)
		{
		case plugInModeGrayScale:
			bpp = 1;
			bmi.bi.biBitCount = 8;
			for (i = 0; i < 255; i++)
				{
					bmi.u.bmiColors[i].rgbBlue =
					    bmi.u.bmiColors[i].rgbGreen =
					        bmi.u.bmiColors[i].rgbRed = i;
					bmi.u.bmiColors[i].rgbReserved = 0;
				}
			break;
		case plugInModeRGBColor:
			bpp = 3;
			bmi.bi.biBitCount = 24;
			break;
		default:
			g_assert_not_reached ();
		}
	bmi.bi.biCompression = BI_RGB;
	bmi.bi.biSizeImage = 0;
	bmi.bi.biXPelsPerMeter = bmi.bi.biYPelsPerMeter = 0;
	bmi.bi.biClrUsed = 0;
	bmi.bi.biClrImportant = 0;

	if ((hbitmap = CreateDIBSection (hdc, (BITMAPINFO *) &bmi, DIB_RGB_COLORS,
	                                 (void **) &bits, NULL, 0)) == NULL)
		{
			g_message (_("pspi: CreateDIBSection() failed: %s"),
			           g_win32_error_message (GetLastError ()));
			return noErr;
		}

	bpl = ((w*bpp - 1)/4 + 1) * 4;
	if (bpp == 1)
		for (y = 0; y < h; y++)
			{
				memmove (bits + bpl*y,
				         ((char *) source->baseAddr) + srcRect->left + source->rowBytes*y,
				         w);
			}
	else
		for (y = 0; y < h; y++)
			{
				p = bits + bpl*y;
				q = ((char *) source->baseAddr) + srcRect->left*source->colBytes +
				    source->rowBytes*y;
				for (x = 0; x < w; x++)
					{
						/* dest DIB is BGR, source is RGB */
						p[0] = q[2];
						p[1] = q[1];
						p[2] = q[0];
						p += 3;
						q += source->colBytes;
					}
			}

	hmemdc = CreateCompatibleDC (hdc);
	holdbm = SelectObject (hmemdc, hbitmap);

	if (!BitBlt (hdc, dstCol, dstRow, bmi.bi.biWidth, -bmi.bi.biHeight, hmemdc,
	             0, 0, SRCCOPY))
		if (GetLastError () != ERROR_SUCCESS)
			g_message (_("pspi: BitBlt() failed: %s"),
			           g_win32_error_message (GetLastError ()));

	SelectObject (hmemdc, holdbm);
	DeleteDC (hmemdc);
	DeleteObject (hbitmap);

	return noErr;
}

#if 0

static ReadImageDocumentDesc *
make_read_image_document_desc (void)
{
	ReadImageDocumentDesc *p = g_new (ReadImageDocumentDesc, 1);

	p->minVersion = 0;
	p->maxVersion = 1;
	p->imageMode = filter.imageMode;
	p->depth = 8;
	p->bounds.top = 0;
	p->bounds.left = 0;
	p->bounds.bottom = drawable->height;
	p->bounds.right = drawable->width;
	p->hResolution = filter.imageHRes;
	p->vResolution = filter.imageVRes;
	/* redLUT, greenLUT and blueLUT */

	/* XXX */
	return NULL;
}

#endif

static void
create_buf (guchar      **buf,
            int32        *stride,
            const Rect   *rect,
            int           loplane,
            int           hiplane)
{
	const int nplanes = hiplane - loplane + 1;
	const int w = (rect->right - rect->left);
	const int h = (rect->bottom - rect->top);

	*buf = g_malloc (nplanes * w * h);
	*stride = nplanes * w;
	PSPI_DEBUG (ADVANCE_STATE,
	            g_print (G_STRLOC ":%s: nplanes=%d w=%d h=%d stride=%ld buf=%p\n",
	                     __FUNCTION__,
	                     nplanes, w, h, *stride, *buf));
}

static void
fill_buf (guchar      **buf,
          int32        *stride,
          GimpPixelRgn *pr,
          const Rect   *rect,
          int           loplane,
          int           hiplane)
{
	const int nplanes = hiplane - loplane + 1;
	const int w = (rect->right - rect->left);
	const int h = (rect->bottom - rect->top);
	int gimpw, gimph;

	create_buf (buf, stride, rect, loplane, hiplane);

	PSPI_DEBUG (ADVANCE_STATE,
	            g_print (G_STRLOC ":%s: nplanes=%d loplane=%d hiplane=%d w=%d h=%d stride=%ld\n",
	                     __FUNCTION__,
	                     nplanes, loplane, hiplane, w, h, *stride));

	if (rect->left < 0 ||
	        rect->top < 0 ||
	        rect->left + w > drawable->width ||
	        rect->top + h > drawable->height)
		{
			/* At least part of the requested area is outside the drawable.
			 * Clear all of it for a start, then.
			 */
			memset (*buf, h * *stride, 0);
		}

	if (rect->left < drawable->width &&
	        rect->top < drawable->height)
		{
			/* At least a part of the requested area is inside the drawable */

			gimpw = w;
			if (rect->left + w > drawable->width)
				gimpw = drawable->width - rect->left;

			gimph = h;
			if (rect->top + h > drawable->height)
				gimph = drawable->height - rect->top;

			PSPI_DEBUG (ADVANCE_STATE, if (gimpw != w || gimph != h)
                  g_print ("  gimpw=%d gimph=%d\n", gimpw, gimph));

			if (nplanes == pr->bpp && gimpw == w)
				{
					gimp_pixel_rgn_get_rect (pr, *buf, rect->left, rect->top,
					                         gimpw, gimph);
				}
			else
				{
					guchar *row = g_malloc (pr->bpp * gimpw);
					gint i, j, y;

					for (y = rect->top; y < rect->top + gimph; y++)
						{
							gimp_pixel_rgn_get_row (pr, row, rect->left, y, gimpw);
							for (i = loplane; i <= hiplane; i++)
								{
									guchar *p = row + i;
									guchar *q = *buf + (y - rect->top) * *stride + (i - loplane);
									for (j = 0; j < gimpw; j++)
										{
											*q = *p;
											p += pr->bpp;
											q += nplanes;
										}
								}
						}
					g_free (row);
				}
		}

#ifdef PSPI_WITH_DEBUGGING
	if (debug_mask & PSPI_DEBUG_ADVANCE_STATE)
		{
			int i;
			for (i = 0; i < 8; i++)
				{
					int j;
					for (j = loplane; j <= hiplane; j++)
						g_print ("%02x", (*buf)[i*nplanes+j]);
					g_print (" ");
				}
			g_print ("\n");
		}
#endif /* PSPI_WITH_DEBUGGING */
}

static void
store_buf (guchar       *buf,
           int32         stride,
           GimpPixelRgn *pr,
           const Rect   *rect,
           int           loplane,
           int           hiplane)
{
	const int nplanes = hiplane - loplane + 1;
	const int w = (rect->right - rect->left);
	const int h = (rect->bottom - rect->top);
	int gimpw, gimph;

#ifdef PSPI_WITH_DEBUGGING
	if (debug_mask & PSPI_DEBUG_ADVANCE_STATE)
		{
			int i;
			g_print ("  buf=%p nplanes=%d loplane=%d hiplane=%d w=%d h=%d stride=%ld\n",
			         buf, nplanes, loplane, hiplane, w, h, stride);

			for (i = 0; i < 8; i++)
				{
					int j;
					for (j = loplane; j <= hiplane; j++)
						g_print ("%02x", buf[i*nplanes+j]);
					g_print (" ");
				}
			g_print ("\n");
		}
#endif /* PSPI_WITH_DEBUGGING */

	if (rect->left < drawable->width &&
	        rect->top < drawable->height)
		{
			/* At least a part of the area is inside the drawable */

			gimpw = w;
			if (rect->left + w > drawable->width)
				gimpw = drawable->width - rect->left;

			gimph = h;
			if (rect->top + h > drawable->height)
				gimph = drawable->height - rect->top;

			PSPI_DEBUG (ADVANCE_STATE, if (gimpw != w || gimph != h) g_print ("  gimpw=%d gimph=%d\n", gimpw, gimph));

			if (nplanes == pr->bpp && gimpw == w)
				{
					gimp_pixel_rgn_set_rect (pr, buf, rect->left, rect->top,
					                         gimpw, gimph);
				}
			else
				{
					guchar *row = g_malloc (pr->bpp * gimpw);
					gint i, j, y;

					for (y = rect->top; y < rect->top + gimph; y++)
						{
							gimp_pixel_rgn_get_row (pr, row, rect->left, y, gimpw);
							for (i = loplane; i <= hiplane; i++)
								{
									guchar *p = row + i;
									guchar *q = buf + (y - rect->top) * stride + (i - loplane);
									for (j = 0; j < gimpw; j++)
										{
											*p = *q;
											p += pr->bpp;
											q += nplanes;
										}
								}
							gimp_pixel_rgn_set_row (pr, row, rect->left, y, gimpw);
						}
					g_free (row);
				}
		}
}

static OSErr
advance_state_proc (void)
{
	/* Ugly, ugly */
	static gboolean src_valid = FALSE, dst_valid = FALSE;
	static GimpPixelRgn src, dst;
	static Rect outRect;
	static gint outRowBytes, outLoPlane, outHiPlane;

#ifdef PSPI_WITH_DEBUGGING
	if (debug_mask & PSPI_DEBUG_ADVANCE_STATE)
		{
			g_print (G_STRLOC ":%s: in=", __FUNCTION__);
			PRINT_RECT (filter.inRect);
			g_print (",%d--%d", filter.inLoPlane, filter.inHiPlane);
			g_print ("  out=");
			PRINT_RECT (filter.outRect);
			g_print (",%d--%d", filter.outLoPlane, filter.outHiPlane);
			g_print ("\n  inData=%p outData=%p drawable->bpp=%d\n",
			         filter.inData, filter.outData, drawable->bpp);
		}
#endif /* PSPI_WITH_DEBUGGING */

	if (src_valid)
		{
			g_free (filter.inData);
			filter.inData = NULL;
			src_valid = FALSE;
		}

	if (dst_valid)
		{
			PSPI_DEBUG (ADVANCE_STATE, g_print ("  outData:\n"));
			store_buf ((guchar *) filter.outData, outRowBytes, &dst, &outRect,
			           outLoPlane, outHiPlane);
			g_free (filter.outData);
			filter.outData = NULL;
			dst_valid = FALSE;
		}

	if (RECT_NONEMPTY (filter.inRect))
		{
			gimp_pixel_rgn_init (&src, drawable, filter.inRect.left, filter.inRect.top,
			                     filter.inRect.right - filter.inRect.left,
			                     filter.inRect.bottom - filter.inRect.top,
			                     FALSE, FALSE);
			fill_buf ((guchar **) &filter.inData, &filter.inRowBytes, &src,
			          &filter.inRect, filter.inLoPlane, filter.inHiPlane);
			src_valid = TRUE;
		}
	else
		filter.inData = NULL;

	if (RECT_NONEMPTY (filter.outRect))
		{
			GimpPixelRgn src2;
			gimp_pixel_rgn_init (&src2, drawable, filter.outRect.left, filter.outRect.top,
			                     filter.outRect.right - filter.outRect.left,
			                     filter.outRect.bottom - filter.outRect.top,
			                     FALSE, FALSE);
			gimp_pixel_rgn_init (&dst, drawable, filter.outRect.left, filter.outRect.top,
			                     filter.outRect.right - filter.outRect.left,
			                     filter.outRect.bottom - filter.outRect.top,
			                     TRUE, TRUE);
			fill_buf ((guchar **) &filter.outData, &filter.outRowBytes, &src,
			          &filter.outRect, filter.outLoPlane, filter.outHiPlane);
			outRowBytes = filter.outRowBytes;
			outRect = filter.outRect;
			outLoPlane = filter.outLoPlane;
			outHiPlane = filter.outHiPlane;
			dst_valid = TRUE;
		}
	else
		filter.outData = NULL;

	return noErr;
}

static void
clean (gchar *s)
{
	gchar *tail;
	gchar *slash;

	while (*s == ' ')
		strcpy (s, s + 1);

	tail = s + strlen (s) - 1;
	while (tail > s && *tail == ' ')
		*tail-- = '\0';

	while ((slash = strchr (s, '/')) != NULL)
		{
			*slash = ':';
			s = slash + 1;
		}
}

static void
install (PSPlugIn          *pspi,
         const gchar       *file,
         const struct stat *statp,
         gchar             *category,
         gchar             *name,
         gchar             *image_types,
         gchar             *entrypoint)
{
	gchar *pdb_name;
	gchar *menu_path;

	pdb_name = make_pdb_name (file, entrypoint);

	menu_path = g_strdup_printf (FILTER_MENU_PREFIX "%s/%s", category, name);

	install_pdb (name, pdb_name, file, menu_path, image_types);

	add_entry_to_plugin (pspi, name, pdb_name, menu_path, image_types, entrypoint);
}

static const char *
resource_name (LPCTSTR resource)
{
	static char *bfr = NULL;

	if (HIWORD (resource) == 0)
		{
			if (bfr == NULL)
				bfr = g_strdup ("          ");
			sprintf (bfr, "#%d", (int) resource);
		}
	else
		{
			if (bfr == NULL)
				bfr = g_strdup (resource);
			else
				{
					if (strlen (bfr) < strlen (resource))
						bfr = g_realloc (bfr, strlen (resource));
					strcpy (bfr, resource);
				}
		}
	return bfr;
}

static BOOL CALLBACK
enum_names (HMODULE  dll,
            LPCTSTR  type,
            LPTSTR   name,
            LONG     param)
{
	HRSRC pipl;
	HGLOBAL reshandle;
	long int  resp, version;
	PIProperty *pipp;
	EnumArg *arg = (EnumArg *) param;
	int i, count;
	char entrypoint[256] = "";
	gchar *menu_category = NULL, *menu_name = NULL, *image_types = NULL;

	PSPI_DEBUG (PIPL, g_print ("%s: name=%s\n", arg->file, resource_name (name)));

	if ((pipl = FindResource (dll, name, type)) == NULL)
		{
			g_message (_("pspi: FindResource() failed for %s in %s"),
			           resource_name (name), arg->file);
			return TRUE;
		}

	if ((reshandle = LoadResource (dll, pipl)) == NULL)
		{
			g_message (_("pspi: LoadResource() failed for %s: %s"),
			           arg->file, g_win32_error_message (GetLastError ()));
			return TRUE;
		}

	if ((resp = (long int) LockResource (reshandle)) == 0)
		{
			g_message (_("pspi: LockResource() failed for PiPL resource from %s: %s"),
			           arg->file, g_win32_error_message (GetLastError ()));
			return TRUE;
		}

	resp = ((resp + 3) & ~3) + 2; // weird, yes
	version = *((int *) resp);
	if (version != 0)
		{
			g_message (_("pspi: Wrong version of PiPL resource in %s: %ld, expected 0"),
			           arg->file, version);
			return TRUE;
		}

	count = ((int *) resp)[1];
	pipp = (PIProperty *) (((char *) resp) + 8);

	for (i = 0; i < count; i++)
		{
			PSPI_DEBUG (PIPL, g_print ("property: %s\n",
			                           int32_as_be_4c (pipp->propertyKey)));
			if (pipp->propertyKey == PIKindProperty)
				{
					if (memcmp (pipp->propertyData, "MFB8", 4) != 0)
						{
							g_message (_("pspi: %s is not a Photoshop filter plug-in?"),
							           arg->file);
							return TRUE;
						}
				}
			else if (pipp->propertyKey == PIWin64X86CodeProperty)
				{
					strncpy (entrypoint, pipp->propertyData, pipp->propertyLength);
				}
			else if (pipp->propertyKey == PIWin32X86CodeProperty)
				{
					strncpy (entrypoint, pipp->propertyData, pipp->propertyLength);
				}
			else if (pipp->propertyKey == PIVersionProperty)
				{
					DWORD *versionp = (DWORD *) pipp->propertyData;
					if (HIWORD (*versionp) > latestFilterVersion ||
					        (HIWORD (*versionp) == latestFilterVersion &&
					         LOWORD (*versionp) > latestFilterSubVersion))
						{
							g_message (_("pspi: Photoshop plug-in %s requires interface version %d.%d, I only know %d.%d"),
							           arg->file, HIWORD (*versionp), LOWORD (*versionp),
							           latestFilterVersion, latestFilterSubVersion);
							return TRUE;
						}
				}
			else if (pipp->propertyKey == PIImageModesProperty)
				{
					WORD mode = GUINT16_SWAP_LE_BE(*((WORD *) pipp->propertyData));

					image_types = g_strconcat
					              ((mode & (0x8000 >> plugInModeGrayScale)) ? "GRAY* " : "",
					               (mode & (0x8000 >> plugInModeRGBColor)) ? "RGB* " : "",
					               NULL);

					if (strlen (image_types) == 0)
						{
							PSPI_DEBUG (PIPL, g_print ("entry %s doesn't support any of GIMP's image modes\n",
							                           resource_name (name)));
							return TRUE;
						}
				}
			else if (pipp->propertyKey == PICategoryProperty)
				{
					menu_category = g_malloc0 (pipp->propertyLength);
					strncpy (menu_category, pipp->propertyData+1,
					         (int) (guchar) pipp->propertyData[0]);
					clean (menu_category);
				}
			else if (pipp->propertyKey == PINameProperty)
				{
					menu_name = g_malloc0 (pipp->propertyLength);
					strncpy (menu_name, pipp->propertyData+1,
					         (int) (guchar) pipp->propertyData[0]);
					clean (menu_name);
				}

			pipp = (PIProperty *) ((&pipp->propertyData)
			                       + pipp->propertyLength);
		}

	if (entrypoint[0] == 0)
		{
			g_message (_("pspi: No entrypoint for %s in %s"), resource_name (name), arg->file);
			return TRUE;
		}

	if (menu_category == NULL || menu_category[0] == '\0')
		{
			g_message (_("pspi: No category specified for %s in %s"), resource_name (name), arg->file);
			return TRUE;
		}

  gchar* menu_name2;
	if (menu_name == NULL || menu_name[0] == '\0')
		{
			g_message (_("pspi: No name specified for %s in %s"), resource_name (name), arg->file);
			return TRUE;
		}
  else
    {
      menu_name2 = strdup(menu_name);
	    if (strcmp (menu_name2 + strlen (menu_name2) - 3, "...") == 0)
		    menu_name2[strlen (menu_name2) - 3] = '\0';
    }

	if (image_types == NULL)
		{
			/* Assume RGB* */
			image_types = g_strdup ("RGB*");
		}

	if (GetProcAddress (dll, entrypoint) == NULL)
		{
			g_message (_("pspi: GetProcAddress(%s,%s) failed: %s"),
			           arg->file, resource_name (entrypoint),
			           g_win32_error_message (GetLastError ()));
			return TRUE;
		}

	PSPI_DEBUG (PIPL, g_print ("%s: %s, %s, %s, %s\n", arg->file, menu_category,
	                           menu_name, image_types, entrypoint));

	install (arg->pspi, arg->file, arg->statp,
	         menu_category, menu_name2, image_types, entrypoint);

	return TRUE;
}

void
query_8bf (const gchar       *file,
           const struct stat *statp)
{
	HMODULE dll;
	EnumArg *arg;

	PSPI_DEBUG (PSPIRC, g_print ("Trying %s\n", file));

	if ((dll = LoadLibrary (file)) == NULL)
		{
			g_message (_("pspi: LoadLibrary() failed for %s: %s"),
			           file, g_win32_error_message (GetLastError ()));
			return;
		}

	arg = g_new (EnumArg, 1);
	arg->file = file;
	arg->statp = statp;
	arg->pspi = g_new (PSPlugIn, 1);
	arg->pspi->location = g_strdup (file);
	arg->pspi->timestamp = statp->st_mtime;
	arg->pspi->present = TRUE;
	arg->pspi->entries = NULL;

	if (EnumResourceNames (dll, "PIPL", &enum_names, (LONG) arg) == 0)
		g_message (_("pspi: EnumResourceNames(PIPL) failed for %s: %s"),
		           file, g_win32_error_message (GetLastError ()));

	add_found_plugin (arg->pspi);

	g_free (arg);

	FreeLibrary (dll);
}

static void
setup_suites (void)
{
	static gboolean beenhere = FALSE;

	if (beenhere)
		return;

	beenhere = TRUE;

	buffer_procs = g_new (BufferProcs, 1);
	buffer_procs->bufferProcsVersion = kCurrentBufferProcsVersion;
	buffer_procs->numBufferProcs = kCurrentBufferProcsCount;
	buffer_procs->allocateProc = buffer_allocate_proc;
	buffer_procs->lockProc = buffer_lock_proc;
	buffer_procs->unlockProc = buffer_unlock_proc;
	buffer_procs->freeProc = buffer_free_proc;
	buffer_procs->spaceProc = buffer_space_proc;

	channel_port_procs = g_new (ChannelPortProcs, 1);
	channel_port_procs->channelPortProcsVersion = kCurrentChannelPortProcsVersion;
	channel_port_procs->numChannelPortProcs = kCurrentChannelPortProcsCount;
	channel_port_procs->readPixelsProc = channel_port_read_pixels_proc;
	channel_port_procs->writeBasePixelsProc = channel_port_write_base_pixels_proc;
	channel_port_procs->readPortForWritePortProc = channel_port_read_port_for_write_port_proc;

	descriptor_parameters = g_new (PIDescriptorParameters, 1);
	descriptor_parameters->descriptorParametersVersion = kCurrentDescriptorParametersVersion;
	/* XXX */

	handle_procs = g_new (HandleProcs, 1);
	handle_procs->handleProcsVersion = kCurrentHandleProcsVersion;
	handle_procs->numHandleProcs = kCurrentHandleProcsCount;
	handle_procs->newProc = handle_new_proc;
	handle_procs->disposeProc = handle_dispose_proc;
	handle_procs->getSizeProc = handle_get_size_proc;
	handle_procs->setSizeProc = handle_set_size_proc;
	handle_procs->lockProc = handle_lock_proc;
	handle_procs->unlockProc = handle_unlock_proc;
	handle_procs->recoverSpaceProc = handle_recover_space_proc;
	handle_procs->disposeRegularHandleProc = handle_dispose_regular_proc;

	image_services_procs = g_new (ImageServicesProcs, 1);
	image_services_procs->imageServicesProcsVersion = kCurrentImageServicesProcsVersion;
	image_services_procs->numImageServicesProcs = kCurrentImageServicesProcsCount;
	image_services_procs->interpolate1DProc = image_services_interpolate_1d_proc;
	image_services_procs->interpolate2DProc = image_services_interpolate_2d_proc;
	image_services_procs->interpolate1DMultiProc = image_services_interpolate_1d_multi_proc;
	image_services_procs->interpolate2DMultiProc = image_services_interpolate_2d_multi_proc;

	property_procs = g_new (PropertyProcs, 1);
	property_procs->propertyProcsVersion = kCurrentPropertyProcsVersion;
	property_procs->numPropertyProcs = kCurrentPropertyProcsCount;
	property_procs->getPropertyProc = property_get_proc;
	property_procs->setPropertyProc = property_set_proc;

	resource_procs = g_new (ResourceProcs, 1);
	resource_procs->resourceProcsVersion = kCurrentResourceProcsVersion;
	resource_procs->numResourceProcs = kCurrentResourceProcsCount;
	resource_procs->countProc = resource_count_proc;
	resource_procs->getProc = resource_get_proc;
	resource_procs->deleteProc = resource_delete_proc;
	resource_procs->addProc = resource_add_proc;

	basic_suite = g_new (SPBasicSuite, 1);
	basic_suite->AcquireSuite = SPBasicAcquireSuite;
	basic_suite->ReleaseSuite = SPBasicReleaseSuite;
	basic_suite->IsEqual = SPBasicIsEqual;
	basic_suite->AllocateBlock = SPBasicAllocateBlock;
	basic_suite->FreeBlock = SPBasicFreeBlock;
	basic_suite->ReallocateBlock = SPBasicReallocateBlock;
	basic_suite->Undefined = SPBasicUndefined;
}

static void
setup_filter_record (void)
{
	static gboolean beenhere = FALSE;
	guchar red, green, blue;
	GimpRGB back, fore;

	if (beenhere)
		return;

	beenhere = TRUE;

	platform.hwnd = 0;

	filter.serialNumber = 0;
	filter.abortProc = abort_proc;
	filter.progressProc = progress_proc;
	filter.parameters = NULL;
	gimp_palette_get_background (&back);
	gimp_rgb_get_uchar (&back, &red, &green, &blue);
	filter.background.red = (red * 65535) / 255;
	filter.background.green = (green * 65535) / 255;
	filter.background.blue = (blue * 65535) / 255;
	filter.backColor[0] = red;
	filter.backColor[1] = green;
	filter.backColor[2] = blue;
	filter.backColor[3] = 0xFF;
	gimp_palette_get_foreground (&fore);
	gimp_rgb_get_uchar (&fore, &red, &green, &blue);
	filter.foreground.red = (red * 65535) / 255;
	filter.foreground.green = (green * 65535) / 255;
	filter.foreground.blue = (blue * 65535) / 255;
	filter.foreColor[0] = red;
	filter.foreColor[1] = green;
	filter.foreColor[2] = blue;
	filter.foreColor[3] = 0xFF;
	filter.maxSpace = 100000000;
	memcpy ((char *) &filter.hostSig, "GIMP", 4);
	filter.hostProc = host_proc;
	filter.platformData = &platform;
	filter.bufferProcs = buffer_procs;
	filter.resourceProcs = resource_procs;
	filter.processEvent = process_event_proc;
	filter.displayPixels = display_pixels_proc;
	filter.handleProcs = handle_procs;

	filter.supportsDummyChannels = FALSE; /* Docs say supportsDummyPlanes */
	filter.supportsAlternateLayouts = FALSE;
	filter.wantLayout = 0;
	filter.filterCase = 0;
	filter.dummyPlaneValue = -1;
	/* premiereHook */
	filter.advanceState = advance_state_proc;
	filter.supportsAbsolute = TRUE;
	filter.wantsAbsolute = FALSE;
	filter.getPropertyObsolete = property_get_proc;
	/* cannotUndo */
	filter.supportsPadding = FALSE;
	/* inputPadding */
	/* outputPadding */
	/* maskPadding */
	filter.samplingSupport = FALSE;
	/* reservedByte */
	/* inputRate */
	/* maskRate */
	filter.colorServices = color_services_proc;

	filter.imageServicesProcs = image_services_procs;
	filter.propertyProcs = property_procs;
	filter.inTileHeight = gimp_tile_height ();
	filter.inTileWidth = gimp_tile_width ();
	filter.inTileOrigin.h = 0;
	filter.inTileOrigin.v = 0;
	filter.absTileHeight = filter.inTileHeight;
	filter.absTileWidth = filter.inTileWidth;
	filter.absTileOrigin.h = 0;
	filter.absTileOrigin.v = 0;
	filter.outTileHeight = filter.inTileHeight;
	filter.outTileWidth = filter.inTileWidth;
	filter.outTileOrigin.h = 0;
	filter.outTileOrigin.v = 0;
	filter.maskTileHeight = filter.inTileHeight;
	filter.maskTileWidth = filter.inTileWidth;
	filter.maskTileOrigin.h = 0;
	filter.maskTileOrigin.v = 0;

	filter.descriptorParameters = NULL /* descriptor_parameters */;
	filter.channelPortProcs = NULL /* channel_port_procs */;
	filter.documentInfo = NULL;

	filter.sSPBasic = NULL /* basic_suite */;
	filter.plugInRef = NULL;
	filter.depth = 8;

	filter.iCCprofileData = NULL;
	filter.iCCprofileSize = 0;
	filter.canUseICCProfiles = FALSE;

	memset (filter.reserved, 0, sizeof (filter.reserved));
}

static void
setup_sizes (void)
{
	gint x1, y1, x2, y2;
	gdouble xres, yres;

	filter.imageSize.h = drawable->width;
	filter.imageSize.v = drawable->height;
	filter.planes = drawable->bpp;
	gimp_drawable_mask_bounds (drawable->drawable_id, &x1, &y1, &x2, &y2);
	filter.filterRect.top = y1;
	filter.filterRect.left = x1;
	filter.filterRect.bottom = y2;
	filter.filterRect.right = x2;

	gimp_image_get_resolution (image_id, &xres, &yres);
	filter.imageHRes = long2fixed ((long) (xres + 0.5));
	filter.imageVRes = long2fixed ((long) (yres + 0.5));
	filter.floatCoord.h = x1;
	filter.floatCoord.v = y1;
	filter.wholeSize.h = gimp_image_width (image_id);
	filter.wholeSize.v = gimp_image_height (image_id);
}

static GimpPDBStatusType
error_message (int16  result,
               gchar *phase)
{
	/* Many plug-ins seem to return error code 1 to indicate Cancel */
	if (result == userCanceledErr || result == 1)
		return GIMP_PDB_CANCEL;
	else
		{
			gchar *emsg;

			if (result == errReportString)
				{
					int msglen = (*filter.errorString)[0];
					emsg = g_malloc (msglen + 1);
					strncpy (emsg, (*filter.errorString)+1, msglen);
					emsg[msglen] = '\0';
				}
			else
				{
					gchar *s;
					gchar num[20];

					switch (result)
						{
						case readErr:
							s = _("File read error");
							break;
						case writErr:
							s = _("File write error");
							break;
						case openErr:
							s = _("File open error");
							break;
						case dskFulErr:
							s = _("Disk full");
							break;
						case ioErr:
							s = _("File I/O error");
							break;
						case memFullErr:
							s = _("Out of memory");
							break;
						case nilHandleErr:
							s = _("Null handle error");
							break;
						case filterBadParameters:
							s = _("Bad parameters");
							break;
						case filterBadMode:
							s = _("Unsupported image mode");
							break;
						case errPlugInHostInsufficient:
							s = _("Requires services not provided by this GIMP plug-in");
							break;
						case errPlugInPropertyUndefined:
							s = _("A requested property could not be found");
							break;
						case errHostDoesNotSupportColStep:
							s = _("This GIMP plug-in does not support colBytes other than 1");
							break;
						case errInvalidSamplePoint:
							s = _("Invalid sample point");
							break;
						default:
							sprintf (num, _("error code %d"), result);
							s = num;
							break;
						}
					emsg = g_strdup_printf ("%s: %s", phase, s);
				}
			g_message (_("pspi: Problem invoking Photoshop plug-in: %s"), emsg);
			g_free (emsg);
			return GIMP_PDB_EXECUTION_ERROR;
		}
}

static GimpPDBStatusType
load_dll (PSPlugInEntry *pspie)
{
	if (pspie->entry != NULL)
		return GIMP_PDB_SUCCESS;

	/* We are invoked with "re-run last filter" */
	pspie->entry = g_new (PIentrypoint, 1);

	if ((pspie->entry->dll = LoadLibrary (pspie->pspi->location)) == NULL)
		{
			g_message (_("pspi: LoadLibrary(%s) failed: %s"),
			           pspie->pspi->location,
			           g_win32_error_message (GetLastError ()));
			return GIMP_PDB_EXECUTION_ERROR;
		}

	pspie->entry->ep = (int (CALLBACK *)(short, void *, long *, int16*))
	                   GetProcAddress (pspie->entry->dll, pspie->entrypoint_name);

	if (pspie->entry->ep == NULL)
		{
			g_message (_("pspi: GetProcAddress(%s,%s) failed: %s"),
			           pspie->pspi->location, pspie->entrypoint_name,
			           g_win32_error_message (GetLastError ()));
			FreeLibrary (pspie->entry->dll);
			return GIMP_PDB_EXECUTION_ERROR;
		}
	return GIMP_PDB_SUCCESS;
}

static void
save_stuff (const PSPlugInEntry *pspie)
{
	gchar *token;
	gint size;

	if (filter.parameters != NULL)
		{
			if (handle_valid (filter.parameters))
				{
					token = g_strdup_printf (PSPI_PARAMETER_TOKEN, pspie->pdb_name);
					size = handle_get_size_proc ((Handle) filter.parameters);
					PSPI_DEBUG (CALL, g_print ("Saving parameters: %d bytes\n", size));
					gimp_set_data (token,
					               handle_lock_proc ((Handle) filter.parameters, TRUE),
					               size);
					g_free (token);
					handle_unlock_proc ((Handle) filter.parameters);
				}
			else if ((size = GlobalSize ((HGLOBAL) filter.parameters)) > 0)
				{
					token = g_strdup_printf (PSPI_PARAMETER_HGLOBAL_TOKEN,
					                         pspie->pdb_name);
					PSPI_DEBUG (CALL,
					            g_print ("Saving HGLOBAL parameters: %p %d bytes\n",
					                     filter.parameters, size));
					gimp_set_data (token,
					               GlobalLock ((HGLOBAL) filter.parameters),
					               size);
					g_free (token);
					GlobalUnlock ((HGLOBAL) filter.parameters);
				}
			else if (!IsBadReadPtr (filter.parameters, sizeof (HGLOBAL *)) &&
			         (size = GlobalSize (*(HGLOBAL *) filter.parameters)) > 0)
				{
					token = g_strdup_printf (PSPI_PARAMETER_HGLOBAL_PTR_TOKEN, pspie->pdb_name);
					PSPI_DEBUG (CALL,
					            g_print ("Saving HGLOBAL* parameters: %p %d bytes\n",
					                     *(HGLOBAL *) filter.parameters, size));
					gimp_set_data (token,
					               GlobalLock (*(HGLOBAL *) filter.parameters),
					               size);
					g_free (token);
					GlobalUnlock (*(HGLOBAL *) filter.parameters);
				}
		}

	PSPI_DEBUG (CALL, g_print ("Saving data %#lx\n", data));
	token = g_strdup_printf (PSPI_DATA_TOKEN, pspie->pdb_name);
	size = gimp_set_data (token, &data, sizeof (int32));
	g_free (token);
}

static void
restore_stuff (const PSPlugInEntry *pspie)
{
	gchar *token, *token2, *token3;
	gint size;

	/* If this is not re-application of last filter, just keep whatever there
	 * is in the filter record.
	 */
	if (prev_phase == PARAMETERS)
		return;

	token = g_strdup_printf (PSPI_PARAMETER_TOKEN, pspie->pdb_name);
	token2 = g_strdup_printf (PSPI_PARAMETER_HGLOBAL_TOKEN, pspie->pdb_name);
	token3 = g_strdup_printf (PSPI_PARAMETER_HGLOBAL_PTR_TOKEN, pspie->pdb_name);

	if ((size = gimp_get_data_size (token)) > 0)
		{
			PSPI_DEBUG (CALL, g_print ("Restoring parameters: %d bytes\n", size));
			filter.parameters = handle_new_proc (size);
			gimp_get_data (token,
			               handle_lock_proc ((Handle) filter.parameters, TRUE));
			handle_unlock_proc ((Handle) filter.parameters);
		}
	else if ((size = gimp_get_data_size (token2)) > 0)
		{
			filter.parameters = GlobalAlloc (GMEM_MOVEABLE, size);
			PSPI_DEBUG (CALL,
			            g_print ("Restoring HGLOBAL parameters: %d bytes: %p\n",
			                     size,
			                     filter.parameters));
			gimp_get_data (token2,
			               GlobalLock ((HGLOBAL) filter.parameters));
			GlobalUnlock ((HGLOBAL) filter.parameters);
		}
	else if ((size = gimp_get_data_size (token3)) > 0)
		{
			filter.parameters = (Handle) g_new (HGLOBAL, 1);
			*((HGLOBAL *) filter.parameters) = GlobalAlloc (GMEM_MOVEABLE, size);
			PSPI_DEBUG (CALL,
			            g_print ("Restoring HGLOBAL* parameters: %d bytes: %p\n",
			                     size,
			                     *(HGLOBAL *) filter.parameters));
			gimp_get_data (token3,
			               GlobalLock (*(HGLOBAL *) filter.parameters));
			GlobalUnlock (*(HGLOBAL *) filter.parameters);
		}
	else
		filter.parameters = NULL;

	g_free (token);
	g_free (token2);
	g_free (token3);

	token = g_strdup_printf (PSPI_DATA_TOKEN, pspie->pdb_name);
	size = gimp_get_data_size (token);
	if (size == sizeof (int32))
		{
			gimp_get_data (token, &data);
			PSPI_DEBUG (CALL, g_print ("Restored data %#lx\n", data));
		}
	else
		data = 0;
	g_free (token);
}

GimpPDBStatusType
pspi_about (PSPlugInEntry *pspie)
{
	AboutRecord about;
	GimpPDBStatusType status;
	GList *list;
	int16 result;

	if ((status = load_dll (pspie)) != GIMP_PDB_SUCCESS)
		return status;

	setup_suites ();

	platform.hwnd = 0;
	about.platformData = &platform;
	about.sSPBasic = NULL /* basic_suite */;
	about.plugInRef = NULL;
	memset (about.reserved, 0, sizeof (about.reserved));

	/* A module might have several plug-in entry points. Call the About
	 * box selector for each of them. Only one of them should display
	 * the about box. Says the Photoshop API docs.
	 */
	list = pspie->pspi->entries;
	while (list != NULL)
		{
			pspie = list->data;
			list = list->next;
			result = noErr;
			PSPI_DEBUG (CALL, g_print (G_STRLOC ":%s: calling filterSelectorAbout\n",
			                           __FUNCTION__));
			(*pspie->entry->ep) (filterSelectorAbout, &about, &data, &result);
			PSPI_DEBUG (CALL, g_print (G_STRLOC ":%s: after filterSelectorAbout: %d\n",
			                           __FUNCTION__,
			                           result));
			if (result != noErr)
				{
					FreeLibrary (pspie->entry->dll);
					return error_message (result, "filterSelectorAbout");
				}
		}

	FreeLibrary (pspie->entry->dll);
	return GIMP_PDB_SUCCESS;
}

GimpPDBStatusType
pspi_params (PSPlugInEntry *pspie)
{
	GimpPDBStatusType status;
	int16 result;

	if ((status = load_dll (pspie)) != GIMP_PDB_SUCCESS)
		return status;

	setup_suites ();
	setup_filter_record ();

	result = noErr;
	PSPI_DEBUG (CALL, g_print (G_STRLOC ":%s: calling filterSelectorParameters\n",
	                           __FUNCTION__));
	(*pspie->entry->ep) (filterSelectorParameters, &filter, &data, &result);
	PSPI_DEBUG (CALL, g_print (G_STRLOC ":%s: after filterSelectorParameters: %d\n",
	                           __FUNCTION__,
	                           result));
	if (result != noErr)
		{
			FreeLibrary (pspie->entry->dll);
			return error_message (result, "filterSelectorParameters");
		}

	save_stuff (pspie);

	prev_phase = PARAMETERS;

	return GIMP_PDB_SUCCESS;
}

GimpPDBStatusType
pspi_prepare (PSPlugInEntry *pspie,
              GimpDrawable  *dr)
{
	GimpImageType image_type;
	GimpPDBStatusType status;
	int16 result;

	/* Set globals, yecch */
	drawable = dr;
	image_id = gimp_drawable_get_image (drawable->drawable_id);

	image_type = gimp_drawable_type (drawable->drawable_id);

	if ((status = load_dll (pspie)) != GIMP_PDB_SUCCESS)
		return status;

	setup_suites ();
	setup_filter_record ();
	setup_sizes ();

	restore_stuff (pspie);

	filter.isFloating = gimp_drawable_is_layer (drawable->drawable_id) && gimp_layer_is_floating_sel (drawable->drawable_id);
	if (filter.isFloating) PSPI_DEBUG (CALL, g_print ("isFloating TRUE\n"));
	filter.haveMask = FALSE;      /* ??? */
	filter.autoMask = FALSE;      /* ??? */
	/* maskRect */
	filter.maskData = NULL;
	filter.maskRowBytes = 0;

	switch (image_type)
		{
		case GIMP_RGB_IMAGE:
			filter.imageMode = plugInModeRGBColor;
			filter.inLayerPlanes = 3;
			filter.inTransparencyMask = 0;
			break;
		case GIMP_RGBA_IMAGE:
			filter.imageMode = plugInModeRGBColor;
			filter.inLayerPlanes = 3;
			filter.inTransparencyMask = 1;
			break;
		case GIMP_GRAY_IMAGE:
			filter.imageMode = plugInModeGrayScale;
			filter.inLayerPlanes = 1;
			filter.inTransparencyMask = 0;
			break;
		case GIMP_GRAYA_IMAGE:
			filter.imageMode = plugInModeGrayScale;
			filter.inLayerPlanes = 1;
			filter.inTransparencyMask = 1;
			break;
		default:
			g_assert_not_reached ();
		}

	filter.inLayerMasks = 0;
	filter.inInvertedLayerMasks = 0;
	filter.inNonLayerPlanes = 0;

	filter.outLayerPlanes = filter.inLayerPlanes;
	filter.outTransparencyMask = filter.inTransparencyMask;
	filter.outLayerMasks = filter.inLayerMasks;
	filter.outInvertedLayerMasks = filter.inInvertedLayerMasks;
	filter.outNonLayerPlanes = filter.inNonLayerPlanes;

	filter.absLayerPlanes = filter.inLayerPlanes;
	filter.absTransparencyMask = filter.inTransparencyMask;
	filter.absLayerMasks = filter.inLayerMasks;
	filter.absInvertedLayerMasks = filter.inInvertedLayerMasks;
	filter.absNonLayerPlanes = filter.inNonLayerPlanes;

	filter.inPreDummyPlanes = 0;
	filter.inPostDummyPlanes = 0;
	filter.outPreDummyPlanes = 0;
	filter.outPostDummyPlanes = 0;

	filter.inColumnBytes = 0;
	filter.inPlaneBytes = 1;
	filter.outColumnBytes = 0;
	filter.outPlaneBytes = 1;

#if 0
	filter.documentInfo = make_read_image_document_desc ();
#else
	filter.documentInfo = NULL;
#endif

	result = noErr;
	PSPI_DEBUG (CALL, g_print (G_STRLOC ":%s: calling filterSelectorPrepare\n",
	                           __FUNCTION__));
	(*pspie->entry->ep) (filterSelectorPrepare, &filter, &data, &result);
	PSPI_DEBUG (CALL, g_print (G_STRLOC ":%s: after filterSelectorPrepare: %d\n",
	                           __FUNCTION__,
	                           result));
	if (result != noErr)
		{
			FreeLibrary (pspie->entry->dll);
			return error_message (result, "filterSelectorPrepare");
		}

	prev_phase = PREPARE;

	return GIMP_PDB_SUCCESS;
}

GimpPDBStatusType
pspi_apply (const PSPlugInEntry *pspie,
            GimpDrawable        *dr)
{
	int16 result;

	g_assert (drawable == dr);
	g_assert (prev_phase == PREPARE);

	result = noErr;
	PSPI_DEBUG (CALL, g_print (G_STRLOC ":%s: calling filterSelectorStart\n",
	                           __FUNCTION__));
	(*pspie->entry->ep) (filterSelectorStart, &filter, &data, &result);
	PSPI_DEBUG (CALL, g_print (G_STRLOC ":%s: after filterSelectorStart: %d\n",
	                           __FUNCTION__,
	                           result));
	if (result != noErr)
		{
			FreeLibrary (pspie->entry->dll);
			return error_message (result, "filterSelectorStart");
		}

	while (RECT_NONEMPTY (filter.inRect)
	        || RECT_NONEMPTY (filter.outRect)
	        || (filter.haveMask && RECT_NONEMPTY (filter.maskRect)))
		{
			advance_state_proc ();
			result = noErr;
			PSPI_DEBUG (CALL, g_print (G_STRLOC ":%s: calling filterSelectorContinue\n",
			                           __FUNCTION__));
			(*pspie->entry->ep) (filterSelectorContinue, &filter, &data, &result);
			PSPI_DEBUG (CALL, g_print (G_STRLOC ":%s: after filterSelectorContinue: %d\n",
			                           __FUNCTION__,
			                           result));

			if (result != noErr)
				{
					int16 saved_result = result;

					result = noErr;
					PSPI_DEBUG (CALL, g_print (G_STRLOC ":%s: calling filterSelectorFinish\n",
					                           __FUNCTION__));
					(*pspie->entry->ep) (filterSelectorFinish, &filter, &data, &result);
					PSPI_DEBUG (CALL, g_print (G_STRLOC ":%s: after filterSelectorFinish: %d\n",
					                           __FUNCTION__,
					                           result));
					FreeLibrary (pspie->entry->dll);
					return error_message (saved_result, "filterSelectorContinue");
				}
		}
	advance_state_proc ();

#if 0
	/* Some plug-ins crash in filterSelectorFinish. Skip it altogether...? */
	result = noErr;
	PSPI_DEBUG (CALL, g_print (G_STRLOC ":%s: calling filterSelectorFinish\n",
	                           __FUNCTION__));
	(*pspie->entry->ep) (filterSelectorFinish, &filter, &data, &result);
	PSPI_DEBUG (CALL, g_print (G_STRLOC ":%s: after filterSelectorFinish: %d\n",
	                           __FUNCTION__,
	                           result));
	if (result != noErr)
		{
			FreeLibrary (pspie->entry->dll);
			return error_message (result, "filterSelectorFinish");
		}
#endif

	FreeLibrary (pspie->entry->dll);
	return GIMP_PDB_SUCCESS;
}
