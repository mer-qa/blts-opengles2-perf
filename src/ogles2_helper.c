/* ogles2_helper.c -- Various helper functions for GLES2

   Copyright (C) 2000-2010, Nokia Corporation.

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
#include <math.h>
#include <wayland-egl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <errno.h>
#include <blts_reporting.h>

#include "ogles2_helper.h"

static const EGLint default_config_attr[] =
{
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_NONE
};

unsigned short RGBA8888toRGB565(unsigned int val)
{
	return (((val >> 19) & 0x1F) << 11) | (((val >> 10) & 0x3F) << 5) |
		((val >> 3) & 0x1F);
}

static double time_step;

static int generate_cos_sin_tables(glesh_context* context)
{
	float angle;
	int t;

	context->cos_table = malloc(sizeof(float) * GLESH_COS_SIN_TABLE_SIZE);
	if(!context->cos_table)
	{
		BLTS_LOGGED_PERROR("malloc");
		return 0;
	}

	context->sin_table = malloc(sizeof(float) * GLESH_COS_SIN_TABLE_SIZE);
	if(!context->sin_table)
	{
		BLTS_LOGGED_PERROR("malloc");
		return 0;
	}

	for(t = 0; t < GLESH_COS_SIN_TABLE_SIZE; t++)
	{
		angle = 2.0f * GLESH_PI * ((float)t / (float)GLESH_COS_SIN_TABLE_SIZE);
		context->cos_table[t] = cos(angle);
		context->sin_table[t] = sin(angle);
	}

	return 1;
}

const char* glesh_egl_error_to_string(EGLint err)
{
	switch(err)
	{
		case EGL_SUCCESS:
			return "EGL_SUCCESS";
		case EGL_BAD_DISPLAY:
			return "EGL_BAD_DISPLAY";
		case EGL_NOT_INITIALIZED:
			return "EGL_NOT_INITIALIZED";
		case EGL_BAD_ACCESS:
			return "EGL_BAD_ACCESS";
		case EGL_BAD_ALLOC:
			return "EGL_BAD_ALLOC";
		case EGL_BAD_ATTRIBUTE:
			return "EGL_BAD_ATTRIBUTE";
		case EGL_BAD_CONFIG:
			return "EGL_BAD_CONFIG";
		case EGL_BAD_CONTEXT:
			return "EGL_BAD_CONTEXT";
		case EGL_BAD_CURRENT_SURFACE:
			return "EGL_BAD_CURRENT_SURFACE";
		case EGL_BAD_MATCH:
			return "EGL_BAD_MATCH";
		case EGL_BAD_NATIVE_PIXMAP:
			return "EGL_BAD_NATIVE_PIXMAP";
		case EGL_BAD_NATIVE_WINDOW:
			return "EGL_BAD_NATIVE_WINDOW";
		case EGL_BAD_PARAMETER:
			return "EGL_BAD_PARAMETER";
		case EGL_BAD_SURFACE:
			return "EGL_BAD_SURFACE";
		default:
			return "unknown";
	}
}

void glesh_report_eglerror(const char* location)
{
	EGLint err = eglGetError();
	BLTS_ERROR("%s failed (%d, %s).\n", location, err,
		glesh_egl_error_to_string(err));
}

int glesh_load_shader(GLenum type, const char *source)
{
	GLuint shader;
	GLint compiled;

	shader = glCreateShader(type);
	if(!shader)
	{
		BLTS_ERROR("glCreateShader failed\n");
		return 0;
	}

	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	if(!compiled)
	{
		GLint info_len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_len);

		if(info_len > 1)
		{
			char* info_log = malloc(sizeof(char) * info_len);
			if(info_log)
			{
				glGetShaderInfoLog(shader, info_len, NULL, info_log);
				BLTS_ERROR("Error compiling shader:\n%s\n", info_log);
				free (info_log);
			}
			else
			{
				BLTS_ERROR("Error compiling shader\n");
			}
		}
		else
		{
			BLTS_ERROR("Error compiling shader\n");
		}

		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

int glesh_load_program(const char *vertex_shader_src,
		const char *fragment_shader_src)
{
	GLuint vertex_shader;
	GLuint fragment_shader;
	GLuint program_object;
	GLint linked;

	vertex_shader = glesh_load_shader(GL_VERTEX_SHADER, vertex_shader_src);
	if(!vertex_shader)
	{
		return 0;
	}

	fragment_shader = glesh_load_shader(GL_FRAGMENT_SHADER,
		fragment_shader_src);
	if(!fragment_shader)
	{
		glDeleteShader(vertex_shader);
		return 0;
	}

	program_object = glCreateProgram();
	if(!program_object)
	{
		BLTS_ERROR("glCreateProgram failed\n");
		glDeleteShader(fragment_shader);
		glDeleteShader(vertex_shader);
		return 0;
	}

	glAttachShader(program_object, vertex_shader);
	glAttachShader(program_object, fragment_shader);
	glLinkProgram(program_object);
	glGetProgramiv(program_object, GL_LINK_STATUS, &linked);

	if(!linked)
	{
		GLint info_len = 0;

		glGetProgramiv(program_object, GL_INFO_LOG_LENGTH, &info_len);

		if(info_len > 1)
		{
			char* info_log = malloc(sizeof(char) * info_len);
			if(info_log)
			{
				glGetProgramInfoLog(program_object, info_len, NULL, info_log);
				BLTS_ERROR("Error linking shader program:\n%s\n", info_log);
				free (info_log);
			}
			else
			{
				BLTS_ERROR("Error linking shader program\n");
			}
		}
		else
		{
			BLTS_ERROR("Error linking shader program\n");
		}

		glDeleteProgram(program_object);
		return 0;
	}

	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	return program_object;
}


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


int glesh_create_context(glesh_context* context,
		const EGLint attribList[],
		int window_width,
		int window_height,
		int depth)
{
	EGLint num_configs;
	EGLint major_version;
	EGLint minor_version;
	EGLConfig* configs;
	EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE,
		EGL_NONE };
	int t;

	memset(context, 0, sizeof(glesh_context));

	generate_cos_sin_tables(context);

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

	if(!attribList)
	{
		attribList = default_config_attr;
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

	context->egl_display = eglGetDisplay(context->wayland_display);
	if(context->egl_display == EGL_NO_DISPLAY)
	{
		glesh_report_eglerror("eglGetDisplay");
		glesh_destroy_context(context);
		return 0;
	}

	if(!eglInitialize(context->egl_display, &major_version, &minor_version))
	{
		glesh_report_eglerror("eglInitialize");
		glesh_destroy_context(context);
		return 0;
	}

	eglBindAPI(EGL_OPENGL_ES_API);

	configs = malloc(sizeof(EGLConfig) * 20);
	if(!configs)
	{
		BLTS_LOGGED_PERROR("malloc");
		glesh_destroy_context(context);
		return 0;
	}

	if(!eglChooseConfig(context->egl_display, attribList,
		configs, 20, &num_configs) || !num_configs)
	{
		glesh_report_eglerror("eglChooseConfig");
		free(configs);
		glesh_destroy_context(context);
		return 0;
	}

	for(t = 0; t  < num_configs; t++)
	{
		context->egl_surface = eglCreateWindowSurface(context->egl_display,
			configs[t], context->wayland_window, NULL);
		if(context->egl_surface != EGL_NO_SURFACE)
		{
			context->egl_context = eglCreateContext(context->egl_display,
				configs[t], NULL, contextAttribs);
			if(context->egl_context != EGL_NO_CONTEXT)
			{
				break;
			}
			else
			{
				eglDestroySurface(context->egl_display, context->egl_surface);
			}
		}
	}
	free(configs);
	if(context->egl_surface == EGL_NO_SURFACE ||
		context->egl_context == EGL_NO_CONTEXT)
	{
		glesh_report_eglerror("eglCreateContext");
		glesh_destroy_context(context);
		return 0;
	}

	if(!eglMakeCurrent(context->egl_display, context->egl_surface,
		context->egl_surface, context->egl_context))
	{
		glesh_report_eglerror("eglMakeCurrent");
		glesh_destroy_context(context);
		return 0;
	}

	if(!eglQuerySurface(context->egl_display, context->egl_surface, EGL_WIDTH,
		&context->width))
	{
		glesh_report_eglerror("eglQuerySurface");
		glesh_destroy_context(context);
		return 0;
	}

	if(!eglQuerySurface(context->egl_display, context->egl_surface, EGL_HEIGHT,
		&context->height))
	{
		glesh_report_eglerror("eglQuerySurface");
		glesh_destroy_context(context);
		return 0;
	}

	glGenTextures(GLESH_MAX_TEXTURES, context->texture_pool);

	BLTS_DEBUG("Surface: %d x %d x %d\n",
		context->width, context->height, context->depth);

	return 1;
}

int glesh_destroy_context(glesh_context* context)
{
	unsigned int t;

	for(t = 0; t < context->num_objects; t++)
	{
		glesh_destroy_object(&context->objects[t]);
	}

	glDeleteTextures(GLESH_MAX_TEXTURES, context->texture_pool);

	if(context->egl_display)
	{
		eglMakeCurrent(context->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
			EGL_NO_CONTEXT);
		eglTerminate(context->egl_display);
		context->egl_display = NULL;
	}

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

	if(context->sin_table)
	{
		free(context->sin_table);
	}

	if(context->cos_table)
	{
		free(context->cos_table);
	}

	memset(context, 0, sizeof(glesh_context));

	return 1;
}

GLuint glesh_get_texture_from_pool(glesh_context* context)
{
	if(context->next_texture >= GLESH_MAX_TEXTURES) return 0;
	return context->texture_pool[context->next_texture++];
}

GLuint glesh_context_triangle_count(glesh_context* context)
{
	unsigned int t;
	GLuint triangle_count = 0;
	for(t = 0; t < context->num_objects; t++)
	{
		triangle_count += context->objects[t].num_triangles;
	}

	return triangle_count;
}

double glesh_time_step()
{
	return time_step;
}

int glesh_execute_main_loop(glesh_context* context,
		DRAW_FUNCTION,
		void* user_ptr,
		double runtime)
{
	int running = 1;
	struct rusage usage_start;
	struct rusage usage_end;
	double used_time = 0.0;
	FILE* fp;
	long unsigned int start_user_load, start_nice_load, start_sys_load, start_idle;
	double prev_time = 0;
	double cur_time;

	fp = fopen("/proc/stat", "r");
	if(fp)
	{
		char tmp[128];
		if(fscanf(fp, "%s %lu %lu %lu %lu", tmp, &start_user_load,
			&start_nice_load, &start_sys_load, &start_idle) != 5)
		{
			BLTS_ERROR("Failed to read /proc/stat\n");
		}
		fclose(fp);
	}

	context->perf_data.frames_rendered = 0;
	getrusage(RUSAGE_SELF,&usage_start);
	timing_start();

	while(running)
	{
		cur_time = timing_elapsed();
		time_step = cur_time - prev_time;
		prev_time = cur_time;
		if(!drawFunc(context, user_ptr))
		{
			BLTS_ERROR("Failed to draw frame %d\n",
				context->perf_data.frames_rendered);
			return 0;
		}
		context->perf_data.frames_rendered++;

		if(runtime == 0.0f)
		{
                        wl_display_dispatch_pending(context->wayland_display);
                        // XXX: set running = 0; when button pressed
		}
		else
		{
			if(cur_time >= runtime)
			{
				running = 0;
			}
		}
	}

	timing_stop();

	context->perf_data.total_time_elapsed = timing_elapsed();

	getrusage(RUSAGE_SELF,&usage_end);
	used_time = usage_end.ru_utime.tv_sec;
	used_time += usage_end.ru_utime.tv_usec * 1E-6;
	used_time += usage_end.ru_stime.tv_sec;
	used_time += usage_end.ru_stime.tv_usec * 1E-6;
	used_time -= usage_start.ru_utime.tv_sec;
	used_time -= usage_start.ru_utime.tv_usec * 1E-6;
	used_time -= usage_start.ru_stime.tv_sec;
	used_time -= usage_start.ru_stime.tv_usec * 1E-6;

	context->perf_data.total_load = 0.0f;

	fp = fopen("/proc/stat", "r");
	if(fp)
	{
		char tmp[128];
		long unsigned int user_load, nice_load, sys_load, idle;
		if(fscanf(fp, "%s %lu %lu %lu %lu", tmp, &user_load, &nice_load,
			 &sys_load, &idle) == 5)
		{
			user_load -= start_user_load;
			nice_load -= start_nice_load;
			sys_load -= start_sys_load;
			idle -= start_idle;
			context->perf_data.total_load = (double)(user_load + nice_load +
				sys_load);
			context->perf_data.total_load =  context->perf_data.total_load *
				100.0f / (context->perf_data.total_load + idle);
		}
		else
		{
			BLTS_ERROR("Failed to read /proc/stat\n");
		}

		fclose(fp);
	}

	context->perf_data.fps = 1.0f / (context->perf_data.total_time_elapsed /
		context->perf_data.frames_rendered);
	context->perf_data.cpu_usage = 100.0 * used_time /
		context->perf_data.total_time_elapsed;

	BLTS_DEBUG("Frames rendered: %d\n", context->perf_data.frames_rendered);
	BLTS_DEBUG("Total render time: %lf\n", context->perf_data.total_time_elapsed);
	BLTS_DEBUG("Frames per second: %lf\n", context->perf_data.fps);
	BLTS_DEBUG("Polygons in scene: %d\n", glesh_context_triangle_count(context));
	BLTS_DEBUG("CPU usage (this process): %lf %%\n", context->perf_data.cpu_usage);
	if(context->perf_data.total_load > 0.0f)
	{
		BLTS_DEBUG("CPU usage (all processes, all CPUs): %lf %%\n",
			context->perf_data.total_load);
	}
	else
	{
		BLTS_DEBUG("CPU usage (all processes, all CPUs): N/A\n");
	}

	blts_report_extended_result("framerate", context->perf_data.fps, "1/s", 0);
	blts_report_extended_result("cpu_use_test_process", context->perf_data.cpu_usage, "%", 0);
	blts_report_extended_result("cpu_use_all_processes", context->perf_data.total_load, "%", 0);

	return 1;
}

glesh_object* glesh_add_object(glesh_context* context, glesh_object* object)
{
	if(context->num_objects >= GLESH_MAX_OBJECTS)
	{
		BLTS_ERROR("glesh_add_object: Maximum number of objects reached\n");
		return NULL;
	}

	context->objects[context->num_objects++] = *object;

	return &context->objects[context->num_objects - 1];
}

glesh_texture* glest_texture_by_name(glesh_context* context,
	const char* texture_name)
{
	unsigned int t;
	for(t = 0; t < context->num_textures; t++)
	{
		if(strstr(context->textures[t].name, texture_name))
		{
			return &context->textures[t];
		}
	}

	return NULL;
}

glesh_texture* glesh_bitmap_to_texture(glesh_context* context,
	const GLenum format, const char* texture_name, unsigned char* data,
	glesh_bitmap_header* header)
{
	int element_size;
	unsigned char* buffer;
	unsigned int* ptr2 = (unsigned int*)data;
	int x, y;

	glesh_texture* tex = glest_texture_by_name(context, texture_name);
	if(tex)
	{
		/* texture already exists, return it */
		return tex;
	}

	if(context->num_textures >= GLESH_MAX_TEXTURES)
	{
		BLTS_ERROR("glesh_bitmap_to_texture: Maximum number of textures reached\n");
		return NULL;
	}

	if(format == GL_RGBA)
	{
		element_size = 4;
	}
	else if(format == GL_RGB)
	{
		element_size = 2;
	}
	else
	{
		BLTS_ERROR("Unsupported pixel format (%d).\n", format);
		return NULL;
	}

	buffer = (unsigned char*)malloc(header->biWidth * header->biHeight *
		element_size);
	if(!buffer)
	{
		BLTS_ERROR("Error allocating buffer for texture.\n");
		return NULL;
	}

	for(y = 0; y < header->biHeight; y++)
	{
		for(x = 0; x < header->biWidth; x++)
		{
			unsigned int col = *ptr2++;
			unsigned char* ptr = (unsigned char*)&buffer[(x + y *
				header->biWidth) * element_size];

			if(format == GL_RGBA)
			{
				*((unsigned int*)ptr) = col;
			}
			else if(format == GL_RGB)
			{
				*((unsigned short*)ptr) = RGBA8888toRGB565(col);
			}
		}
	}

	if(texture_name)
	{
		strcpy(context->textures[context->num_textures].name, texture_name);
	}
	context->textures[context->num_textures].width = header->biWidth;
	context->textures[context->num_textures].height = header->biHeight;

	context->textures[context->num_textures].tex_id =
		glesh_get_texture_from_pool(context);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture (GL_TEXTURE_2D,
		context->textures[context->num_textures].tex_id);
	if(format == GL_RGBA)
	{
		glTexImage2D ( GL_TEXTURE_2D, 0, format, header->biWidth,
			header->biHeight, 0, format, GL_UNSIGNED_BYTE, buffer );
	}
	else if(format == GL_RGB)
	{
		glTexImage2D ( GL_TEXTURE_2D, 0, format, header->biWidth,
			header->biHeight, 0, format, GL_UNSIGNED_SHORT_5_6_5, buffer );
	}
	glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	free(buffer);

	return &context->textures[context->num_textures++];
}

glesh_texture* glesh_texture_from_bmp_file(glesh_context* context,
	const GLenum format, const char* texture_name, const char* filename,
	int scale_w, int scale_h)
{
	glesh_bitmap_header header;
	unsigned char* data = NULL;

	if(format == GL_RGBA)
	{
		data = glesh_read_bitmap(filename, &header, 1, scale_w, scale_h);
	}
	else if(format == GL_RGB)
	{
		data = glesh_read_bitmap(filename, &header, 0, scale_w, scale_h);
	}
	else
	{
		BLTS_ERROR("Unsupported pixel format\n");
		return NULL;
	}

	if(!data)
	{
		BLTS_ERROR("Failed to read file %s\n", filename);
		return NULL;
	}

	return glesh_bitmap_to_texture(context, format,
		texture_name, data, &header);
}

unsigned char* glesh_generate_pattern(const int width, const int height,
	const int offset, const GLenum format)
{
	int element_size;
	unsigned char* buffer;
	int x, y;

	if(format == GL_RGBA)
	{
		element_size = 4;
	}
	else if(format == GL_RGB)
	{
		element_size = 2;
	}
	else
	{
		BLTS_ERROR("Unsupported pixel format (%d).\n", format);
		return NULL;
	}

	buffer = (unsigned char*)malloc(width * height * element_size);
	if(!buffer)
	{
		BLTS_ERROR("Error allocating buffer for texture.\n");
		return NULL;
	}

	for(y = 0; y < height; y++)
	{
		for(x = 0; x < width; x++)
		{
			unsigned int col =	(unsigned char)(x + offset) |
								(unsigned char)(y) << 8 |
								(unsigned char)(0xFF - (unsigned char)x +
								offset) << 16 | 0xFF << 24;
			unsigned char* ptr = (unsigned char*)&col;
			ptr = (unsigned char*)&buffer[(x + y * width) * element_size];

			if(format == GL_RGBA)
			{
				*((unsigned int*)ptr) = col;
			}
			else if(format == GL_RGB)
			{
				*((unsigned short*)ptr) = RGBA8888toRGB565(col);
			}
		}
	}

	return buffer;
}

glesh_texture* glesh_generate_texture(glesh_context* context,
	const GLenum format, const int width, const int height,
	const char* texture_name)
{
	unsigned char* buffer;

	glesh_texture* tex = glest_texture_by_name(context, texture_name);
	if(tex)
	{
		/* texture already exists, return it */
		return tex;
	}

	if(context->num_textures >= GLESH_MAX_TEXTURES)
	{
		BLTS_ERROR("glesh_generate_texture: Maximum number of textures reached\n");
		return NULL;
	}

	buffer = glesh_generate_pattern(width, height, 0, format);
	if(!buffer)
	{
		BLTS_ERROR("Error allocating buffer for texture.\n");
		return NULL;
	}

	if(texture_name)
	{
		strcpy(context->textures[context->num_textures].name, texture_name);
	}
	context->textures[context->num_textures].width = width;
	context->textures[context->num_textures].height = height;

	context->textures[context->num_textures].tex_id =
		glesh_get_texture_from_pool(context);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture (GL_TEXTURE_2D,
		context->textures[context->num_textures].tex_id);
	if(format == GL_RGBA)
	{
		glTexImage2D ( GL_TEXTURE_2D, 0, format, width, height, 0,
			format, GL_UNSIGNED_BYTE, buffer );
	}
	else if(format == GL_RGB)
	{
		glTexImage2D ( GL_TEXTURE_2D, 0, format, width, height, 0,
			format, GL_UNSIGNED_SHORT_5_6_5, buffer );
	}
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
	free(buffer);

	return &context->textures[context->num_textures++];
}

int glesh_generate_sphere(int numSlices, float radius, glesh_object* object)
{
	int i, j;
	int num_parallels = numSlices / 2;
	int num_vertices = ( num_parallels + 1 ) * ( numSlices + 1 );
	int num_indices = num_parallels * numSlices * 6;
	float angleStep = (2.0f * GLESH_PI) / ((float) numSlices);

	object->vertices = malloc ( sizeof(GLfloat) * 3 * num_vertices );
	if(!object->vertices)
	{
		BLTS_ERROR("Failed to allocate vertices\n");
		return 0;
	}
	object->normals = malloc ( sizeof(GLfloat) * 3 * num_vertices );
	if(!object->normals)
	{
		BLTS_ERROR("Failed to allocate normals\n");
		return 0;
	}
	object->texcoords = malloc ( sizeof(GLfloat) * 2 * num_vertices );
	if(!object->texcoords)
	{
		BLTS_ERROR("Failed to allocate texture coordinates\n");
		return 0;
	}
	object->indices = malloc ( sizeof(GLuint) * num_indices );
	if(!object->indices)
	{
		BLTS_ERROR("Failed to allocate indices\n");
		return 0;
	}

	for ( i = 0; i < num_parallels + 1; i++ )
	{
		for ( j = 0; j < numSlices + 1; j++ )
		{
			int vertex = ( i * (numSlices + 1) + j ) * 3;

			object->vertices[vertex + 0] = radius * sinf(angleStep *
				(float)i) * sinf(angleStep * (float)j);
			object->vertices[vertex + 1] = radius * cosf(angleStep * (float)i);
			object->vertices[vertex + 2] = radius * sinf(angleStep *
				(float)i) * cosf(angleStep * (float)j);

			object->normals[vertex + 0] = object->vertices[vertex + 0] / radius;
			object->normals[vertex + 1] = object->vertices[vertex + 1] / radius;
			object->normals[vertex + 2] = object->vertices[vertex + 2] / radius;

			int texIndex = ( i * (numSlices + 1) + j ) * 2;
			object->texcoords[texIndex + 0] = (float) j / (float) numSlices;
			object->texcoords[texIndex + 1] = (1.0f - (float)i) /
				(float)(num_parallels - 1);
		}
	}

	GLuint *indexBuf = object->indices;
	for ( i = 0; i < num_parallels ; i++ )
	{
		for ( j = 0; j < numSlices; j++ )
		{
			*indexBuf++  = i * ( numSlices + 1 ) + j;
			*indexBuf++ = ( i + 1 ) * ( numSlices + 1 ) + j;
			*indexBuf++ = ( i + 1 ) * ( numSlices + 1 ) + ( j + 1 );

			*indexBuf++ = i * ( numSlices + 1 ) + j;
			*indexBuf++ = ( i + 1 ) * ( numSlices + 1 ) + ( j + 1 );
			*indexBuf++ = i * ( numSlices + 1 ) + ( j + 1 );
		}
	}
	object->num_indices = num_indices;
	object->num_triangles = num_indices / 3;
	object->num_vertices = num_vertices;

	return 1;
}

int glesh_generate_cube(float scale, glesh_object* object)
{
	int i;
	int num_vertices = 24;
	int num_indices = 36;

	GLfloat cubeVerts[] =
	{
	-0.5f, -0.5f, -0.5f,
	-0.5f, -0.5f,  0.5f,
	0.5f, -0.5f,  0.5f,
	0.5f, -0.5f, -0.5f,
	-0.5f,  0.5f, -0.5f,
	-0.5f,  0.5f,  0.5f,
	0.5f,  0.5f,  0.5f,
	0.5f,  0.5f, -0.5f,
	-0.5f, -0.5f, -0.5f,
	-0.5f,  0.5f, -0.5f,
	0.5f,  0.5f, -0.5f,
	0.5f, -0.5f, -0.5f,
	-0.5f, -0.5f, 0.5f,
	-0.5f,  0.5f, 0.5f,
	0.5f,  0.5f, 0.5f,
	0.5f, -0.5f, 0.5f,
	-0.5f, -0.5f, -0.5f,
	-0.5f, -0.5f,  0.5f,
	-0.5f,  0.5f,  0.5f,
	-0.5f,  0.5f, -0.5f,
	0.5f, -0.5f, -0.5f,
	0.5f, -0.5f,  0.5f,
	0.5f,  0.5f,  0.5f,
	0.5f,  0.5f, -0.5f,
	};

	GLfloat cubeNormals[] =
	{
	0.0f, -1.0f, 0.0f,
	0.0f, -1.0f, 0.0f,
	0.0f, -1.0f, 0.0f,
	0.0f, -1.0f, 0.0f,
	0.0f, 1.0f, 0.0f,
	0.0f, 1.0f, 0.0f,
	0.0f, 1.0f, 0.0f,
	0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, -1.0f,
	0.0f, 0.0f, -1.0f,
	0.0f, 0.0f, -1.0f,
	0.0f, 0.0f, -1.0f,
	0.0f, 0.0f, 1.0f,
	0.0f, 0.0f, 1.0f,
	0.0f, 0.0f, 1.0f,
	0.0f, 0.0f, 1.0f,
	-1.0f, 0.0f, 0.0f,
	-1.0f, 0.0f, 0.0f,
	-1.0f, 0.0f, 0.0f,
	-1.0f, 0.0f, 0.0f,
	1.0f, 0.0f, 0.0f,
	1.0f, 0.0f, 0.0f,
	1.0f, 0.0f, 0.0f,
	1.0f, 0.0f, 0.0f,
	};

	GLfloat cubeTex[] =
	{
	0.0f, 0.0f,
	0.0f, 1.0f,
	1.0f, 1.0f,
	1.0f, 0.0f,
	1.0f, 0.0f,
	1.0f, 1.0f,
	0.0f, 1.0f,
	0.0f, 0.0f,
	0.0f, 0.0f,
	0.0f, 1.0f,
	1.0f, 1.0f,
	1.0f, 0.0f,
	0.0f, 0.0f,
	0.0f, 1.0f,
	1.0f, 1.0f,
	1.0f, 0.0f,
	0.0f, 0.0f,
	0.0f, 1.0f,
	1.0f, 1.0f,
	1.0f, 0.0f,
	0.0f, 0.0f,
	0.0f, 1.0f,
	1.0f, 1.0f,
	1.0f, 0.0f,
	};

	object->vertices = malloc ( sizeof(GLfloat) * 3 * num_vertices );
	if(!object->vertices)
	{
		BLTS_ERROR("Failed to allocate vertices\n");
		return 0;
	}
	memcpy(object->vertices, cubeVerts, sizeof( cubeVerts ));
	for ( i = 0; i < num_vertices * 3; i++ )
	{
		object->vertices[i] *= scale;
	}

	object->normals = malloc ( sizeof(GLfloat) * 3 * num_vertices );
	if(!object->normals)
	{
		BLTS_ERROR("Failed to allocate normals\n");
		return 0;
	}
	memcpy( object->normals, cubeNormals, sizeof( cubeNormals ) );

	object->texcoords = malloc ( sizeof(GLfloat) * 2 * num_vertices );
	if(!object->texcoords)
	{
		BLTS_ERROR("Failed to allocate texture coordinates\n");
		return 0;
	}
	memcpy( object->texcoords, cubeTex, sizeof( cubeTex ) ) ;

	GLuint cube_indices[] =
	{
	0, 2, 1,
	0, 3, 2,
	4, 5, 6,
	4, 6, 7,
	8, 9, 10,
	8, 10, 11,
	12, 15, 14,
	12, 14, 13,
	16, 17, 18,
	16, 18, 19,
	20, 23, 22,
	20, 22, 21
	};

	object->indices = malloc ( sizeof(GLuint) * num_indices );
	if(!object->indices)
	{
		BLTS_ERROR("Failed to allocate indices\n");
		return 0;
	}
	memcpy( object->indices, cube_indices, sizeof( cube_indices ) );

	object->num_indices = num_indices;
	object->num_triangles = num_indices / 3;
	object->num_vertices = num_vertices;

	return 1;
}

int glesh_generate_triangle_strip(float scale, glesh_object* object)
{
	int i;
	int num_vertices = 3;
	int num_indices = 3;

	GLfloat triVerts[] = {	-0.5f,	-0.5f,	0.0f,
							 0.0f,	0.5f,	0.0f,
							 0.5f,	-0.5f,	0.0f
							};

	GLfloat triNormals[] =
	{
	0.0f, 0.0f, -1.0f,
	0.0f, 0.0f, -1.0f,
	0.0f, 0.0f, -1.0f
	};

	GLfloat triTex[] =
	{
	1.0f, 1.0f,
	0.0f, 1.0f,
	0.0f, 0.0f,
	};

	object->vertices = malloc(sizeof(GLfloat) * 3 * num_vertices);
	if(!object->vertices)
	{
		BLTS_ERROR("Failed to allocate vertices\n");
		return 0;
	}
	memcpy(object->vertices, triVerts, sizeof(triVerts));
	for ( i = 0; i < num_vertices * 3; i++ )
	{
		object->vertices[i] *= scale;
	}

	object->normals = malloc(sizeof(GLfloat) * 3 * num_vertices);
	if(!object->normals)
	{
		BLTS_ERROR("Failed to allocate normals\n");
		return 0;
	}
	memcpy(object->normals, triNormals, sizeof(triNormals));

	object->texcoords = malloc(sizeof(GLfloat) * 2 * num_vertices);
	if(!object->texcoords)
	{
		BLTS_ERROR("Failed to allocate texture coordinates\n");
		return 0;
	}
	memcpy(object->texcoords, triTex, sizeof(triTex)) ;

	GLuint tri_indices[] =
	{
	0, 2, 1
	};

	object->indices = malloc ( sizeof(GLuint) * num_indices );
	if(!object->indices)
	{
		BLTS_ERROR("Failed to allocate indices\n");
		return 0;
	}
	memcpy( object->indices, tri_indices, sizeof( tri_indices ) );

	object->num_indices = num_indices;
	object->num_triangles = 1;
	object->num_vertices = num_vertices;

	return 1;
}

int glesh_generate_rectangle_strip(float scalex, float scaley,
	glesh_object* object)
{
	int i;
	int num_vertices = 4;
	int num_indices = 4;
	glesh_vector3* vp;

	GLfloat quadVerts[] = {	-0.5f,	-0.5f,	0.0f,
							-0.5f,	0.5f,	0.0f,
							 0.5f,	-0.5f,	0.0f,
							 0.5f,	0.5f,	0.0f };

	GLfloat quadNormals[] =
	{
	0.0f, 0.0f, -1.0f,
	0.0f, 0.0f, -1.0f,
	0.0f, 0.0f, -1.0f,
	0.0f, 0.0f, -1.0f,
	};

	GLfloat quadTex[] =
	{
	1.0f, 1.0f,
	1.0f, 0.0f,
	0.0f, 1.0f,
	0.0f, 0.0f,
	};

	object->vertices = malloc(sizeof(GLfloat) * 3 * num_vertices);
	if(!object->vertices)
	{
		BLTS_ERROR("Failed to allocate vertices\n");
		return 0;
	}
	memcpy(object->vertices, quadVerts, sizeof(quadVerts));
	vp = (glesh_vector3*)object->vertices;
	for ( i = 0; i < num_vertices; i++ )
	{
		vp[i].v[0] *= scalex;
		vp[i].v[1] *= scaley;
	}

	object->normals = malloc(sizeof(GLfloat) * 3 * num_vertices);
	if(!object->normals)
	{
		BLTS_ERROR("Failed to allocate normals\n");
		return 0;
	}
	memcpy(object->normals, quadNormals, sizeof(quadNormals));

	object->texcoords = malloc(sizeof(GLfloat) * 2 * num_vertices);
	if(!object->texcoords)
	{
		BLTS_ERROR("Failed to allocate texture coordinates\n");
		return 0;
	}
	memcpy(object->texcoords, quadTex, sizeof(quadTex)) ;

	GLuint quad_indices[] =
	{
	0, 2, 1, 3,
	};

	object->indices = malloc ( sizeof(GLuint) * num_indices );
	if(!object->indices)
	{
		BLTS_ERROR("Failed to allocate indices\n");
		return 0;
	}
	memcpy( object->indices, quad_indices, sizeof( quad_indices ) );

	object->num_indices = num_indices;
	object->num_triangles = 2;
	object->num_vertices = num_vertices;

	return 1;
}

int glesh_generate_plane(float scale, int numSlices, glesh_object* object)
{
	int i, j;
	int num_vertices = ( numSlices + 1 ) * ( numSlices + 1 );
	int num_indices = numSlices * numSlices * 6;

	object->vertices = malloc ( sizeof(GLfloat) * 3 * num_vertices );
	if(!object->vertices)
	{
		BLTS_ERROR("Failed to allocate vertices\n");
		return 0;
	}
	object->normals = malloc ( sizeof(GLfloat) * 3 * num_vertices );
	if(!object->normals)
	{
		BLTS_ERROR("Failed to allocate normals\n");
		return 0;
	}
	object->texcoords = malloc ( sizeof(GLfloat) * 2 * num_vertices );
	if(!object->texcoords)
	{
		BLTS_ERROR("Failed to allocate texture coordinates\n");
		return 0;
	}
	object->indices = malloc ( sizeof(GLuint) * num_indices );
	if(!object->indices)
	{
		BLTS_ERROR("Failed to allocate indices\n");
		return 0;
	}

	for(i = 0; i < numSlices + 1; i++)
	{
		for(j = 0; j < numSlices + 1; j++)
		{
			int vertex = (i * (numSlices + 1) + j) * 3;

			object->vertices[vertex + 0] = scale * (i - numSlices / 2) /
				numSlices;
			object->vertices[vertex + 1] = scale * (j - numSlices / 2) /
				numSlices;
			object->vertices[vertex + 2] = 0.0f;

			object->normals[vertex + 0] = 0.0f;
			object->normals[vertex + 1] = 0.0f;
			object->normals[vertex + 2] = -1.0f;

			int texIndex = ( i * (numSlices + 1) + j ) * 2;
			object->texcoords[texIndex + 0] = (float) j / (float) numSlices;
			object->texcoords[texIndex + 1] = (1.0f - (float)i) /
				(float)(numSlices - 1);
		}
	}

	GLuint *indexBuf = object->indices;
	for ( i = 0; i < numSlices ; i++ )
	{
		for ( j = 0; j < numSlices; j++ )
		{
			*indexBuf++  = i * (numSlices + 1) + j;
			*indexBuf++ = (i + 1) * (numSlices + 1) + j;
			*indexBuf++ = (i + 1) * (numSlices + 1) + (j + 1);

			*indexBuf++ = i * (numSlices + 1) + j;
			*indexBuf++ = (i + 1) * (numSlices + 1) + (j + 1);
			*indexBuf++ = i * (numSlices + 1) + (j + 1);
		}
	}
	object->num_indices = num_indices;
	object->num_triangles = num_indices / 3;
	object->num_vertices = num_vertices;

	return 1;
}

GLfloat* glesh_add_vertices(glesh_object* object, int count)
{
	object->num_vertices += count;

	if(!object->vertices)
	{
		object->vertices = malloc(sizeof(GLfloat) * 3 * object->num_vertices);
	}
	else
	{
		object->vertices = realloc(object->vertices,
			sizeof(GLfloat) * 3 * object->num_vertices);
	}

	return object->vertices;
}

void glesh_generate_rotation_matrix(glesh_matrix* mat,
		GLfloat angle,
		GLfloat x,
		GLfloat y,
		GLfloat z)
{
	GLfloat sinAngle, cosAngle;
	GLfloat mag = sqrtf(x * x + y * y + z * z);

	sinAngle = sinf(angle * GLESH_PI / 180.0f);
	cosAngle = cosf(angle * GLESH_PI / 180.0f);
	if(mag > 0.0f)
	{
		GLfloat xx, yy, zz, xy, yz, zx, xs, ys, zs;
		GLfloat om_cos;

		x /= mag;
		y /= mag;
		z /= mag;

		xx = x * x;
		yy = y * y;
		zz = z * z;
		xy = x * y;
		yz = y * z;
		zx = z * x;
		xs = x * sinAngle;
		ys = y * sinAngle;
		zs = z * sinAngle;
		om_cos = 1.0f - cosAngle;

		mat->m[0][0] = (om_cos * xx) + cosAngle;
		mat->m[0][1] = (om_cos * xy) - zs;
		mat->m[0][2] = (om_cos * zx) + ys;
		mat->m[0][3] = 0.0F;

		mat->m[1][0] = (om_cos * xy) + zs;
		mat->m[1][1] = (om_cos * yy) + cosAngle;
		mat->m[1][2] = (om_cos * yz) - xs;
		mat->m[1][3] = 0.0F;

		mat->m[2][0] = (om_cos * zx) - ys;
		mat->m[2][1] = (om_cos * yz) + xs;
		mat->m[2][2] = (om_cos * zz) + cosAngle;
		mat->m[2][3] = 0.0F;

		mat->m[3][0] = 0.0F;
		mat->m[3][1] = 0.0F;
		mat->m[3][2] = 0.0F;
		mat->m[3][3] = 1.0F;
	}
}

void glesh_rotate(glesh_matrix* mat,
		GLfloat angle,
		GLfloat x,
		GLfloat y,
		GLfloat z)
{
	glesh_matrix rotMat;
	glesh_generate_rotation_matrix(&rotMat, angle, x, y, z);
	glesh_multiply(mat, &rotMat, mat);
}

void glesh_multiply(glesh_matrix* result,
		glesh_matrix* srcA,
		glesh_matrix* srcB)
{
	glesh_matrix tmp;
	int i;

	for (i=0; i<4; i++)
	{
		tmp.m[i][0] =	(srcA->m[i][0] * srcB->m[0][0]) +
			(srcA->m[i][1] * srcB->m[1][0]) +
			(srcA->m[i][2] * srcB->m[2][0]) +
			(srcA->m[i][3] * srcB->m[3][0]);

		tmp.m[i][1] =	(srcA->m[i][0] * srcB->m[0][1]) +
			(srcA->m[i][1] * srcB->m[1][1]) +
			(srcA->m[i][2] * srcB->m[2][1]) +
			(srcA->m[i][3] * srcB->m[3][1]);

		tmp.m[i][2] =	(srcA->m[i][0] * srcB->m[0][2]) +
			(srcA->m[i][1] * srcB->m[1][2]) +
			(srcA->m[i][2] * srcB->m[2][2]) +
			(srcA->m[i][3] * srcB->m[3][2]);

		tmp.m[i][3] =	(srcA->m[i][0] * srcB->m[0][3]) +
			(srcA->m[i][1] * srcB->m[1][3]) +
			(srcA->m[i][2] * srcB->m[2][3]) +
			(srcA->m[i][3] * srcB->m[3][3]);
	}
	memcpy(result, &tmp, sizeof(glesh_matrix));
}

void glesh_set_to_identity(glesh_matrix* mat)
{
	memset(mat, 0, sizeof(glesh_matrix));
	mat->m[0][0] = 1.0f;
	mat->m[1][1] = 1.0f;
	mat->m[2][2] = 1.0f;
	mat->m[3][3] = 1.0f;
}

void glesh_frustum(glesh_matrix* result,
		float left,
		float right,
		float bottom,
		float top,
		float nearZ,
		float farZ)
{
	float deltaX = right - left;
	float deltaY = top - bottom;
	float deltaZ = farZ - nearZ;
	glesh_matrix frust;

	if((nearZ <= 0.0f) || (farZ <= 0.0f) ||
		(deltaX <= 0.0f) || (deltaY <= 0.0f) || (deltaZ <= 0.0f))
	{
		return;
	}

	frust.m[0][0] = 2.0f * nearZ / deltaX;
	frust.m[0][1] = frust.m[0][2] = frust.m[0][3] = 0.0f;

	frust.m[1][1] = 2.0f * nearZ / deltaY;
	frust.m[1][0] = frust.m[1][2] = frust.m[1][3] = 0.0f;

	frust.m[2][0] = (right + left) / deltaX;
	frust.m[2][1] = (top + bottom) / deltaY;
	frust.m[2][2] = -(nearZ + farZ) / deltaZ;
	frust.m[2][3] = -1.0f;

	frust.m[3][2] = -2.0f * nearZ * farZ / deltaZ;
	frust.m[3][0] = frust.m[3][1] = frust.m[3][3] = 0.0f;

	glesh_multiply(result, &frust, result);
}

void glesh_perspective(glesh_matrix* result,
		float fovy,
		float aspect,
		float nearZ,
		float farZ)
{
	GLfloat frustumW, frustumH;
	frustumH = tanf(fovy / 360.0f * GLESH_PI) * nearZ;
	frustumW = frustumH * aspect;
	glesh_frustum(result, -frustumW, frustumW, -frustumH, frustumH, nearZ, farZ);
}

void glesh_translate(glesh_matrix* result, GLfloat tx, GLfloat ty, GLfloat tz)
{
	result->m[3][0] += (result->m[0][0] * tx +
		result->m[1][0] * ty +
		result->m[2][0] * tz);
	result->m[3][1] += (result->m[0][1] * tx +
		result->m[1][1] * ty +
		result->m[2][1] * tz);
	result->m[3][2] += (result->m[0][2] * tx +
		result->m[1][2] * ty +
		result->m[2][2] * tz);
	result->m[3][3] += (result->m[0][3] * tx +
		result->m[1][3] * ty +
		result->m[2][3] * tz);
}

int glesh_init_object(glesh_object* object)
{
	memset(object, 0, sizeof(glesh_object));
	glesh_set_to_identity(&object->modelview);

	return 1;
}

int glesh_destroy_object(glesh_object* object)
{
	if(object->vertices)
	{
		free(object->vertices);
		object->vertices = NULL;
	}

	if(object->normals)
	{
		free(object->normals);
		object->normals = NULL;
	}

	if(object->texcoords)
	{
		free(object->texcoords);
		object->texcoords = NULL;
	}

	if(object->indices)
	{
		free(object->indices);
		object->indices = NULL;
	}

	return 1;
}

int glesh_attach_texture(glesh_object* object, glesh_texture* tex)
{
	object->tex = tex;
	return 1;
}

/* Loads 24-bit RGB/BGR and 32-bit ARGB/ABGR bitmaps */
unsigned char* glesh_read_bitmap(const char* filename,
	glesh_bitmap_header* header, int bgr, int scale_w, int scale_h)
{
	unsigned int filedatalen;
	int x, y, ys, sxinc, syinc, k = 0;
	int image_width, image_height;
	unsigned char* sourcedata;

	FILE* fp = fopen(filename, "rb");
	if(!fp)
	{
		BLTS_LOGGED_PERROR("fopen");
		return NULL;
	}

	fseek(fp, 10, SEEK_SET);
	int offset = 0;
	if(fread(&offset, 1, 4, fp) != 4)
	{
		BLTS_ERROR("Failed to read bitmap header offset from file %s\n", filename);
		fclose(fp);
		return NULL;
	}

	if(fread(header, 1, sizeof(glesh_bitmap_header), fp) !=
		sizeof(glesh_bitmap_header))
	{
		BLTS_ERROR("Failed to read bitmap header from file %s\n", filename);
		fclose(fp);
		return NULL;
	}

	fseek(fp, offset, SEEK_SET);

	if(scale_w)
	{
		image_width = scale_w;
	}
	else
	{
		image_width = header->biWidth;
	}

	if(scale_h)
	{
		image_height = scale_h;
	}
	else
	{
		image_height = header->biHeight;
	}

	unsigned char* p_data = (unsigned char *) malloc(image_width *
		image_height * 4);
	if(!p_data)
	{
		BLTS_LOGGED_PERROR("malloc");
		fclose(fp);
		return NULL;
	}

	if(header->biBitCount == 32)
	{
		filedatalen = header->biWidth * header->biHeight * 4;
	}
	else
	{
		filedatalen = header->biWidth * header->biHeight * 3;
	}
	unsigned char* p_temp = (unsigned char *) malloc( filedatalen );
	if(!p_temp)
	{
		BLTS_LOGGED_PERROR("malloc");
		fclose(fp);
		return NULL;
	}

	if(fread(p_temp, 1, filedatalen, fp) != filedatalen)
	{
		BLTS_ERROR("Failed to read bitmap data from file %s\n", filename);
		fclose(fp);
		free(p_data);
		return NULL;
	}
	fclose(fp);

	sxinc = (header->biWidth << 16) / image_width;
	syinc = (header->biHeight << 16) / image_height;

	k = 0;
	for(y = image_height - 1; y >= 0; y--)
	{
		ys = (y * syinc >> 16) * header->biWidth;
		for(x = 0; x < image_width; x++)
		{
			if(header->biBitCount == 32)
			{
				sourcedata = &p_temp[4 * ((x * sxinc >> 16) + ys)];

				if(bgr)
				{
					p_data[k] = sourcedata[2];
					p_data[k+1] = sourcedata[1];
					p_data[k+2] = sourcedata[0];
					p_data[k+3] = sourcedata[3];
				}
				else
				{
					p_data[k] = sourcedata[0];
					p_data[k+1] = sourcedata[1];
					p_data[k+2] = sourcedata[2];
					p_data[k+3] = sourcedata[3];
				}
			}
			else
			{
				sourcedata = &p_temp[3 * ((x * sxinc >> 16) + ys)];

				if(bgr)
				{
					p_data[k] = sourcedata[2];
					p_data[k+1] = sourcedata[1];
					p_data[k+2] = sourcedata[0];
				}
				else
				{
					p_data[k] = sourcedata[0];
					p_data[k+1] = sourcedata[1];
					p_data[k+2] = sourcedata[2];
				}
				p_data[k+3] = 0;
			}
			k += 4;
		}
	}

	header->biWidth = image_width;
	header->biHeight = image_height;

	free(p_temp);

	return p_data;
}

