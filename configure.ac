AC_PREREQ(2.59)

dnl Making releases:
dnl   app_patch_ver += 1;
dnl   app_interface_age += 1;
dnl   app_binary_age += 1;
dnl if any functions have been added, set interface age to 0.
dnl if backwards compatibility has been broken,
dnl set binary age and interface age to 0.

m4_define([app_major_ver], [1])
m4_define([app_minor_ver], [0])
m4_define([app_patch_ver], [0])
m4_define([app_interface_age], [0])
m4_define([app_binary_age], [0])
m4_define([app_version], [app_major_ver.app_minor_ver.app_patch_ver])

m4_define([app_lt_release], [app_major_ver.app_minor_ver])
m4_define([app_lt_current],
	[m4_eval(app_minor_ver * 100 + app_patch_ver - app_interface_age)])
m4_define([app_lt_revision], [app_interface_age])
m4_define([app_lt_age],
	[m4_eval(app_binary_age - app_interface_age)])

AC_INIT(SDL_PangoDraw, app_version,
	[https://github.com/ZoogieZork/SDL_PangoDraw])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/SDL_PangoDraw.c])
AM_INIT_AUTOMAKE
AM_SILENT_RULES

MAJOR_VERSION=app_major_ver
MINOR_VERSION=app_minor_ver
MICRO_VERSION=app_patch_ver
INTERFACE_AGE=app_interface_age
BINARY_AGE=app_binary_age
VERSION=app_version

AC_SUBST(MAJOR_VERSION)
AC_SUBST(MINOR_VERSION)
AC_SUBST(MICRO_VERSION)
AC_SUBST(INTERFACE_AGE)
AC_SUBST(BINARY_AGE)
AC_SUBST(VERSION)

# libtool versioning

LT_RELEASE=app_lt_release
LT_CURRENT=app_lt_current
LT_REVISION=app_lt_revision
LT_AGE=app_lt_age

AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

# Detect the canonical host and target build environment

AC_CANONICAL_HOST
AC_CANONICAL_TARGET

# Check for tools

AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL
AC_PROG_CC
AC_PROG_INSTALL

# The alpha architecture needs special flags for binary portability
case "$target" in
    alpha*-*-linux*)
        CFLAGS="$CFLAGS -mcpu=ev4 -Wa,-mall"
        ;;
    *-*-beos*)
        ac_default_prefix=/boot/develop/tools/gnupro
        ;;
    *-*-cygwin* | *-*-mingw32*)
        if test "$build" != "$target"; then # cross-compiling
            ac_default_prefix=/usr/local/cross-tools/i386-mingw32msvc
        fi
        ;;
esac

# Check for Pango

PKG_CHECK_MODULES(PANGO, pango >= 1.2.0, , AC_MSG_ERROR([*** pango >= 1.2.0 not found!]))
CFLAGS="$CFLAGS $PANGO_CFLAGS"
LIBS="$LIBS $PANGO_LIBS"

PKG_CHECK_MODULES(PANGOFT2, pangoft2 >= 1.2.0, , AC_MSG_ERROR([*** pangoft2 >= 1.2.0 not found!]))
CFLAGS="$CFLAGS $PANGOFT2_CFLAGS"
LIBS="$LIBS $PANGOFT2_LIBS"

# Check for SDL

PKG_CHECK_MODULES(SDL, [sdl >= 1.2.4])
CFLAGS="$CFLAGS $SDL_CFLAGS"
LIBS="$LIBS $SDL_LIBS"

AC_CONFIG_FILES([Makefile src/Makefile SDL_PangoDraw.pc docs/Makefile docs/Doxyfile VisualC2003/Makefile Wix/Makefile Wix/merge_module.xml Wix/dev.xml Wix/testbench.xml])

# Enable Doxygen targets

DX_DOXYGEN_FEATURE(ON)
DX_DOT_FEATURE(OFF)
DX_HTML_FEATURE(ON)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN([SDL_PangoDraw], [Doxyfile], [.])

# Finally create all the generated files
AC_OUTPUT
