#Makefile.am for pjx library/DLL

lib_LTLIBRARIES = libpjx.la

libpjxincludedir = $(includedir)
libpjxinclude_HEADERS = pjx-attributes.h pjx-layout.h

libpjx_la_SOURCES = \
	pjx-attributes.c pjx-layout.c pjx-markup.c

DEFS = @DEFS@ -DBUILD_DLL
libpjx_la_LDFLAGS = \
	-no-undefined		\
    -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

DISTCLEANFILES = *~ *~c *~h *.cross.cache inc
