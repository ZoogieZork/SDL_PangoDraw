
ACLOCAL_AMFLAGS = -I m4 --install ${ACLOCAL_FLAGS}
AUTOMAKE_OPTIONS = foreign

SUBDIRS = src
DIST_SUBDIRS = src docs VisualC2003

EXTRA_DIST = \
    SDL_PangoDraw.pc.in sidebyside_patch \
	test/AUTHORS test/ChangeLog test/configure.in test/COPYING \
	test/INSTALL test/Makefile.am test/markup.txt test/NEWS \
	test/README test/testbench.c \
	Wix/COPYING.txt Wix/gettext.README Wix/glib2.README Wix/libiconv.README \
	Wix/license.rtf Wix/pango.README Wix/README Wix/shave.xslt Wix/ui.xml \
	Wix/bitmap

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = SDL_PangoDraw.pc
