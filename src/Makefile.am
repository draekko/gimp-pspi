## Process this file with automake to produce Makefile.in

if OS_WIN32
mwindows = -mwindows
endif

EXTRA_DIST = \
	genproxies.m4 \
	Makefile.msvc

VERSION_FLAGS = -DVERSION=\"@PLUGIN_VERSION@\"

PSSDK_CFLAGS = \
	-I $(PSSDK)/PhotoshopAPI/Photoshop \
	-I $(PSSDK)/PhotoshopAPI/Pica_sp \
	-I $(PSSDK)/SampleCode/Common/Includes

bin_PROGRAMS = pspi

noinst_DATA = piproxy.8bf

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

piproxy.8bf : piproxy.c proxies.c piproxy_res.o
	$(CC) -shared $(GLIB_CFLAGS) $(PSSDK_CFLAGS) -o $@ piproxy.c piproxy_res.o $(GLIB_LIBS)

proxies.c : genproxies.m4
	m4 genproxies.m4 >$@

# Copy the plug-in to be tested to target.8bf
# then this rule will extract the resources into a .rc file for piproxy.8bf
piproxy_res.o : target.8bf
	windres -i target.8bf -o target.res
	windres -i target.res -o piproxy_res.o

