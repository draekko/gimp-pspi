## Process this file with automake to produce Makefile.in

if OS_WIN32
mwindows = -mwindows
endif

EXTRA_DIST = \
	genproxies.m4	\
	piproxy.c	\
	Makefile.msvc

VERSION_FLAGS = -DVERSION=\"@PLUGIN_VERSION@\"

PSSDK_CFLAGS = \
	-I $(PSSDK)/PhotoshopAPI/Photoshop \
	-I $(PSSDK)/PhotoshopAPI/Pica_sp \
	-I $(PSSDK)/SampleCode/Common/Includes

bin_PROGRAMS = pspi

noinst_PROGRAMS = dump-resources copy-resources

dump_resources_CFLAGS = @GLIB_CFLAGS@
dump_resources_LDADD = @GLIB_LIBS@

copy_resources_CFLAGS = @GLIB_CFLAGS@
copy_resources_LDADD = @GLIB_LIBS@

bindir = $(GIMP_PLUGIN_DIR)/plug-ins

pspi_SOURCES = \
	interface.c	\
	interface.h	\
	main.c		\
	main.h		\
	pspi.c		\
	pspi.h		\
	plugin-intl.h

pspi_LDFLAGS = $(mwindows)

INCLUDES = \
	-DPSPI_WITH_DEBUGGING \
	@GIMP_CFLAGS@	\
	$(VERSION_FLAGS)\
	$(PSSDK_CFLAGS)

pspi_LDADD = \
	$(GIMP_LIBS)	\
	$(INTLLIBS)	\
	-ladvapi32

AM_CPPFLAGS = \
	-DLOCALEDIR=\""$(LOCALEDIR)"\"		\
	-DDATADIR=\""$(DATADIR)"\"		\
	-DPLUGIN_NAME=\""$(PLUGIN_NAME)"\"	\
	-DGETTEXT_PACKAGE=\""$(GETTEXT_PACKAGE)"\"

piproxy.8bf : piproxy.c proxies.c
	$(CC) -shared -DENTRYPOINT=$(ENTRYPOINT) $(GLIB_CFLAGS) $(PSSDK_CFLAGS) -o $@ piproxy.c $(GLIB_LIBS)

proxies.c : genproxies.m4
	m4 genproxies.m4 >$@
