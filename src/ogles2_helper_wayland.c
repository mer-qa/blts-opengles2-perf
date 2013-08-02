/* ogles2_helper_wayland.c -- Wayland-specific helper functions for GLES2

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

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <wayland-egl.h>

#include "ogles2_helper.h"


static void
wayland_output_listener_geometry(void *data,
		struct wl_output *wl_output,
		int32_t x,
		int32_t y,
		int32_t physical_width,
		int32_t physical_height,
		int32_t subpixel,
		const char *make,
		const char *model,
		int32_t transform)
{
	UNUSED_PARAM(data);
	UNUSED_PARAM(wl_output);
	UNUSED_PARAM(x);
	UNUSED_PARAM(y);
	UNUSED_PARAM(physical_width);
	UNUSED_PARAM(physical_height);
	UNUSED_PARAM(subpixel);
	UNUSED_PARAM(make);
	UNUSED_PARAM(model);
	UNUSED_PARAM(transform);
}

static void
wayland_output_listener_mode(void *data,
		struct wl_output *wl_output,
		uint32_t flags,
		int32_t width,
		int32_t height,
		int32_t refresh)
{
	UNUSED_PARAM(wl_output);
	UNUSED_PARAM(flags);
	UNUSED_PARAM(refresh);

	glesh_context *context = (glesh_context *)data;
	context->wayland_output_width = width;
	context->wayland_output_height = height;
}

static struct wl_output_listener
wayland_output_listener = {
	wayland_output_listener_geometry,
	wayland_output_listener_mode,
};


static void
wayland_registry_listener_global(void *data,
		struct wl_registry *wl_registry,
		uint32_t name,
		const char *interface,
		uint32_t version)
{
	UNUSED_PARAM(version);

	glesh_context *context = (glesh_context *)data;

	if (strcmp(interface, "wl_compositor") == 0)
	{
		context->wayland_compositor = wl_registry_bind(wl_registry,
				name, &wl_compositor_interface, 1);
	}
	else if (strcmp(interface, "wl_output") == 0)
	{
		context->wayland_output = wl_registry_bind(wl_registry,
				name, &wl_output_interface, 1);
		wl_output_add_listener(context->wayland_output,
				&wayland_output_listener, context);
	}
	else if (strcmp(interface, "wl_shell") == 0)
	{
		context->wayland_shell = wl_registry_bind(wl_registry,
				name, &wl_shell_interface, 1);
	}
}

static void
wayland_registry_listener_global_remove(void *data,
		struct wl_registry *wl_registry,
		uint32_t name)
{
	UNUSED_PARAM(data);
	UNUSED_PARAM(wl_registry);
	UNUSED_PARAM(name);
}

static struct wl_registry_listener
wayland_registry_listener = {
	wayland_registry_listener_global,
	wayland_registry_listener_global_remove,
};

static void
wayland_shell_surface_listener_ping(void *data,
		struct wl_shell_surface *wl_shell_surface,
		uint32_t serial)
{
	UNUSED_PARAM(data);

	wl_shell_surface_pong(wl_shell_surface, serial);
}

static void
wayland_shell_surface_listener_configure(void *data,
		struct wl_shell_surface *wl_shell_surface,
		uint32_t edges,
		int32_t width,
		int32_t height)
{
	UNUSED_PARAM(data);
	UNUSED_PARAM(wl_shell_surface);
	UNUSED_PARAM(edges);
	UNUSED_PARAM(width);
	UNUSED_PARAM(height);
}

static void
wayland_shell_surface_listener_popup_done(void *data,
		struct wl_shell_surface *wl_shell_surface)
{
	UNUSED_PARAM(data);
	UNUSED_PARAM(wl_shell_surface);
}

static struct wl_shell_surface_listener
wayland_shell_surface_listener = {
	wayland_shell_surface_listener_ping,
	wayland_shell_surface_listener_configure,
	wayland_shell_surface_listener_popup_done,
};


int glesh_create_context_wayland(glesh_context* context,
		int window_width,
		int window_height,
		int depth)
{
	context->wayland_display = wl_display_connect(NULL);
	if (!context->wayland_display)
	{
		BLTS_ERROR("Error: Unable to open Wayland display\n");
		glesh_destroy_context(context);
		return 0;
	}

	context->wayland_registry = wl_display_get_registry(
			context->wayland_display);
	wl_registry_add_listener(context->wayland_registry,
			&wayland_registry_listener, context);

	/**
	 * Wait for availability of compositor, output and
	 * screen size sent by the server to our listener.
	 **/
	while (context->wayland_compositor == NULL ||
			context->wayland_shell == NULL ||
			context->wayland_output == NULL ||
			context->wayland_output_width == 0 ||
			context->wayland_output_height == 0)
	{
		wl_display_dispatch(context->wayland_display);
	}

	if(!depth)
	{
		context->depth = 32;
	}
	else
	{
		context->depth = depth;
	}

	if(window_width == 0)
	{
		context->width = context->wayland_output_width;
	}
	else
	{
		context->width = window_width;
	}

	if(window_height == 0)
	{
		context->height = context->wayland_output_height;
	}
	else
	{
		context->height = window_height;
	}

	context->wayland_surface = wl_compositor_create_surface(
		context->wayland_compositor);

	if(context->wayland_surface == NULL) {
		glesh_report_eglerror("wl_compositor_create_surface");
		glesh_destroy_context(context);
		return 0;
	}

	struct wl_shell_surface *shell_surface = wl_shell_get_shell_surface(
			context->wayland_shell, context->wayland_surface);

	if(shell_surface == NULL) {
		glesh_report_eglerror("wl_shell_get_shell_surface");
		glesh_destroy_context(context);
		return 0;
	}

	wl_shell_surface_add_listener(shell_surface,
			&wayland_shell_surface_listener, context);

	context->wayland_window = wl_egl_window_create(
			context->wayland_surface,
			context->width, context->height);

	if(context->wayland_window == NULL) {
		glesh_report_eglerror("wl_egl_window_create");
		glesh_destroy_context(context);
		return 0;
	}

	context->egl_native_display = context->wayland_display;
	context->egl_native_window = context->wayland_window;

	return 1;
}

int glesh_destroy_context_wayland(glesh_context* context)
{
	if(context->wayland_window)
	{
		wl_egl_window_destroy(context->wayland_window);
		context->wayland_window = NULL;
	}

	if(context->wayland_surface)
	{
		wl_surface_destroy(context->wayland_surface);
		context->wayland_surface = NULL;
	}

	if(context->wayland_display)
	{
		wl_display_disconnect(context->wayland_display);
		context->wayland_display = NULL;
	}

	return 1;
}

int glesh_main_loop_step_wayland(glesh_context *context)
{
	wl_display_flush(context->wayland_display);
	wl_display_dispatch_pending(context->wayland_display);
	return 1;
}

struct glesh_ws_context_functions glesh_wayland = {
	glesh_create_context_wayland,
	glesh_destroy_context_wayland,
	glesh_main_loop_step_wayland,
};
