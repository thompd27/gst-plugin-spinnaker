# Note: plugindir is set in configure

plugin_LTLIBRARIES = libgstspinnaker.la

# Path to installation of the flycap SDK 
SPINNAKER_CFLAGS = -I/usr/include/spinnaker/spinc
SPINNAKER_LIBS = -lSpinnaker_C -L/usr/lib

# sources used to compile this plug-in
libgstspinnaker_la_SOURCES = gstspinnaker.c gstspinnaker.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstspinnaker_la_CFLAGS = $(GST_CFLAGS) $(SPINNAKER_CFLAGS)
libgstspinnaker_la_LIBADD = $(GST_LIBS) $(SPINNAKER_LIBS) -lgstvideo-1.0
libgstspinnaker_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstspinnaker_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstspinnaker.h
