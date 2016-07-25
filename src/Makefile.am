## Process this file with automake to produce Makefile.in

if OS_WIN32
mwindows = -mwindows
endif

bindir = $(GIMP_LIBDIR)/plug-ins

EXTRA_DIST = \
	genproxies.m4	\
	piproxy.c

PSSDK_CFLAGS = \
	-I$(PSSDK)/photoshopapi/photoshop \
	-I$(PSSDK)/photoshopapi/pica_sp \
	-I$(PSSDK)/samplecode/common/includes

bin_PROGRAMS = pspi

noinst_PROGRAMS = dump-resources copy-resources

pspi_SOURCES = \
	interface.c	\
	interface.h	\
	main.c		\
	main.h		\
	pspi.c	\
	pspi.h		\
	plugin-intl.h

pspi_LDFLAGS = $(mwindows)

AM_CPPFLAGS = \
	-DPSPI_WITH_DEBUGGING \
	@GIMP_CFLAGS@	\
	$(PSSDK_CFLAGS) \
	-fPIC

LDADD = \
	$(GIMP_LIBS)	\
	-ladvapi32 -lgdi32

AM_CPPFLAGS += \
	-DLOCALEDIR=\""$(LOCALEDIR)"\"

install-exec-hook:
if OS_WIN32
else
	$(INSTALL) pspi.exe.so $(bindir)
endif
