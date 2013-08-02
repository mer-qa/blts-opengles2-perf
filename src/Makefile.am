# Makefile.am - Automake script for blts-opengles2-perf
#
#  Copyright (C) 2000-2010, Nokia Corporation.
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, version 2.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.

AM_CFLAGS = -O3 -g -Wall -Wextra -std=gnu99 -I$(includedir) -I.. -fPIC
AM_CPPFLAGS = -pthread
AM_LDFLAGS = -pthread

requiredlibs = -lm -lrt -lEGL -lGLESv2 -lwayland-client -lwayland-egl

h_sources = \
	ogles2_helper.h \
	test_common.h \
	test_blitter.h \
	ogles2_conf_file.h

c_sources = \
	ogles2_helper.c \
	ogles2_helper_wayland.c \
	ogles2_helper_fbdev.c \
	ogles2_conf_file.c \
	test_simple_tri.c \
	test_enum_glextensions.c \
	test_enum_eglextensions.c \
	test_enum_eglconfigs.c \
	test_polygons.c \
	test_frag_shader.c \
	test_vert_shader.c \
	test_texels.c \
	test_fillrate.c \
	test_blitter.c

library_includedir = $(includedir)/blts
#library_include_HEADERS = $(h_sources)

bin_PROGRAMS = \
	blts-opengles2-tests

blts_opengles2_tests_SOURCES = $(h_sources) $(c_sources) ogles2_perf_test.c
blts_opengles2_tests_CFLAGS = $(AM_CFLAGS) $(BLTS_COMMON_CFLAGS)
blts_opengles2_tests_LDADD = $(requiredlibs) $(BLTS_COMMON_LIBS)

