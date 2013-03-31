
lib_LTLIBRARIES = libSDL_PangoDraw.la

libSDL_PangoDrawincludedir = $(includedir)
libSDL_PangoDrawinclude_HEADERS = SDL_PangoDraw.h

libSDL_PangoDraw_la_SOURCES = SDL_PangoDraw.c

DEFS = @DEFS@ -DBUILD_DLL
libSDL_PangoDraw_la_LDFLAGS = \
	-no-undefined		\
    -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)

DISTCLEANFILES = *~ *~c *~h *.cross.cache inc
