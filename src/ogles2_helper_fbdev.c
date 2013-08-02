/* ogles2_helper_fbdev.c -- fbdev-specific helper functions for GLES2

   Copyright (C) 2013 Jolla Ltd.
   Contact: Thomas Perl <thomas.perl@jollamobile.com>

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, version 2.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "ogles2_helper.h"


int glesh_create_context_fbdev(glesh_context* context,
		int window_width,
		int window_height,
		int depth)
{
	if(!depth)
	{
		context->depth = 32;
	}
	else
	{
		context->depth = depth;
	}

	/**
	 * No support for window_width and window_height on fbdev.
	 * The fbdev device will always be fullscreen.
	 **/

	context->egl_native_display = EGL_DEFAULT_DISPLAY;
	context->egl_native_window = NULL;

	return 1;
}

int glesh_destroy_context_fbdev(glesh_context* context)
{
	return 1;
}

int glesh_main_loop_step_fbdev(glesh_context *context)
{
	return 1;
}

struct glesh_ws_context_functions glesh_fbdev = {
	glesh_create_context_fbdev,
	glesh_destroy_context_fbdev,
	glesh_main_loop_step_fbdev,
};
