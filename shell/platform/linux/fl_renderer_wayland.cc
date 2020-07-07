// Copyright 2020 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define WL_EGL_PLATFORM

#include "fl_renderer_wayland.h"
#include "flutter/shell/platform/linux/egl_utils.h"

#include <gdk/gdkwayland.h>
#include <wayland-egl-core.h>

struct _FlRendererWayland {
  FlRenderer parent_instance;

  GdkWindow* toplevel_window;

  struct {
    wl_subsurface* subsurface;
    wl_surface* surface;
    wl_egl_window* egl_window;
    GdkRectangle geometry;
    gint scale;
  } subsurface;

  // The resource surface will not be mapped, but needs to be a wl_surface
  // because ONLY window EGL surfaces are supported on Wayland
  struct {
    wl_surface* surface;
    wl_egl_window* egl_window;
  } resource;
};

static wl_registry* registry_global = nullptr;
static wl_subcompositor* subcompositor_global = nullptr;

static struct {
  void** global;
  const wl_interface* interface;
} globals[] = {{reinterpret_cast<void**>(&subcompositor_global),
                &wl_subcompositor_interface}};

// wl_registry.global callback
// Binds to all globals in the globals array
static void registry_handle_global(void*,
                                   wl_registry* registry,
                                   uint32_t id,
                                   const char* name,
                                   uint32_t max_version) {
  for (unsigned i = 0; i < sizeof(globals) / sizeof(globals[0]); i++) {
    const wl_interface* interface = globals[i].interface;
    if (strcmp(name, interface->name) == 0) {
      uint32_t version =
          MIN(static_cast<uint32_t>(interface->version), max_version);
      *globals[i].global = static_cast<wl_subcompositor*>(
          wl_registry_bind(registry, id, interface, version));
    }
  }
}

// wl_registry.global_remove callback
// Can be safely ignored unless we bind to globals that might be removed
static void registry_handle_global_remove(void*, wl_registry*, uint32_t) {}

static const wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

// The first time this function is called, all Wayland globals are initialized
// (this blocks for a round trip to the Wayland compositor)
// Subsequent calls return immediately
static void lazy_initialize_wayland_globals() {
  if (registry_global)
    return;

  GdkWaylandDisplay* gdk_display =
      GDK_WAYLAND_DISPLAY(gdk_display_get_default());
  g_return_if_fail(gdk_display);

  wl_display* display = gdk_wayland_display_get_wl_display(gdk_display);
  registry_global = wl_display_get_registry(display);
  wl_registry_add_listener(registry_global, &registry_listener, nullptr);
  wl_display_roundtrip(display);
}

G_DEFINE_TYPE(FlRendererWayland, fl_renderer_wayland, fl_renderer_get_type())

// Implements FlRenderer::set_window
static void fl_renderer_wayland_set_window(FlRenderer* renderer,
                                           GdkWindow* window) {
  FlRendererWayland* self = FL_RENDERER_WAYLAND(renderer);
  g_return_if_fail(GDK_IS_WAYLAND_WINDOW(window));
  self->toplevel_window = GDK_WAYLAND_WINDOW(window);
}

// Implements FlRenderer::create_display
static EGLDisplay fl_renderer_wayland_create_display(FlRenderer* /*renderer*/) {
  GdkWaylandDisplay* gdk_display =
      GDK_WAYLAND_DISPLAY(gdk_display_get_default());
  g_return_val_if_fail(gdk_display, nullptr);
  return eglGetDisplay(gdk_wayland_display_get_wl_display(gdk_display));
}

// Implements FlRenderer::create_surfaces
static gboolean fl_renderer_wayland_create_surfaces(FlRenderer* renderer,
                                                    EGLDisplay egl_display,
                                                    EGLConfig config,
                                                    EGLSurface* visible,
                                                    EGLSurface* resource,
                                                    GError** error) {
  FlRendererWayland* self = FL_RENDERER_WAYLAND(renderer);

  if (!GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default())) {
    g_set_error(error, fl_renderer_error_quark(), FL_RENDERER_ERROR_FAILED,
                "fl_renderer_wayland_create_surface: expected Wayland display");
    return FALSE;
  }
  GdkWaylandDisplay* gdk_display =
      GDK_WAYLAND_DISPLAY(gdk_display_get_default());

  wl_compositor* compositor =
      gdk_wayland_display_get_wl_compositor(gdk_display);
  if (!compositor) {
    g_set_error(error, fl_renderer_error_quark(), FL_RENDERER_ERROR_FAILED,
                "fl_renderer_wayland_create_surface: no wl_compositor");
    return FALSE;
  }

  self->subsurface.geometry = GdkRectangle{0, 0, 1, 1};
  self->subsurface.scale = 1;
  self->subsurface.surface = wl_compositor_create_surface(compositor);
  self->resource.surface = wl_compositor_create_surface(compositor);
  if (!self->subsurface.surface || !self->resource.surface) {
    g_set_error(
        error, fl_renderer_error_quark(), FL_RENDERER_ERROR_FAILED,
        "fl_renderer_wayland_create_surface: failed to create wl_surfaces");
    return FALSE;
  }

  self->subsurface.egl_window =
      wl_egl_window_create(self->subsurface.surface, 1, 1);
  self->resource.egl_window =
      wl_egl_window_create(self->resource.surface, 1, 1);
  if (!self->subsurface.egl_window || !self->resource.egl_window) {
    g_set_error(
        error, fl_renderer_error_quark(), FL_RENDERER_ERROR_FAILED,
        "fl_renderer_wayland_create_surface: failed to create EGL windows");
    return FALSE;
  }

  *visible = eglCreateWindowSurface(egl_display, config,
                                    self->subsurface.egl_window, nullptr);
  *resource = eglCreateWindowSurface(egl_display, config,
                                     self->resource.egl_window, nullptr);
  if (*visible == EGL_NO_SURFACE || *resource == EGL_NO_SURFACE) {
    EGLint egl_error = eglGetError();  // must be before egl_config_to_string()
    g_autofree gchar* config_string = egl_config_to_string(egl_display, config);
    g_set_error(error, fl_renderer_error_quark(), FL_RENDERER_ERROR_FAILED,
                "Failed to create EGL surfaces using configuration (%s): %s",
                config_string, egl_error_to_string(egl_error));
    return FALSE;
  }

  lazy_initialize_wayland_globals();
  if (!subcompositor_global) {
    g_set_error(error, fl_renderer_error_quark(), FL_RENDERER_ERROR_FAILED,
                "fl_renderer_wayland_create_surface: could not bind to "
                "wl_subcompositor");
    return FALSE;
  }

  wl_surface* toplevel_surface =
      gdk_wayland_window_get_wl_surface(self->toplevel_window);
  if (!toplevel_surface) {
    g_set_error(error, fl_renderer_error_quark(), FL_RENDERER_ERROR_FAILED,
                "fl_renderer_wayland_create_surface: could not get toplevel "
                "wl_surface");
    return FALSE;
  }

  self->subsurface.subsurface = wl_subcompositor_get_subsurface(
      subcompositor_global, self->subsurface.surface, toplevel_surface);
  if (!self->subsurface.subsurface) {
    g_set_error(
        error, fl_renderer_error_quark(), FL_RENDERER_ERROR_FAILED,
        "fl_renderer_wayland_create_surface: could not create subsurface");
    return FALSE;
  }

  wl_subsurface_set_desync(self->subsurface.subsurface);
  wl_subsurface_set_position(self->subsurface.subsurface, 0, 0);
  wl_surface_commit(self->subsurface.surface);

  // Give the subsurface an empty input region so the main surface gets input
  wl_region* region = wl_compositor_create_region(compositor);
  wl_surface_set_input_region(self->subsurface.surface, region);
  wl_region_destroy(region);
  wl_surface_commit(self->subsurface.surface);

  return TRUE;
}

// Implements FlRenderer::set_geometry
static void fl_renderer_wayland_set_geometry(FlRenderer* renderer,
                                             GdkRectangle* geometry,
                                             gint scale) {
  FlRendererWayland* self = FL_RENDERER_WAYLAND(renderer);

  if (!self->subsurface.egl_window || !self->subsurface.surface)
    return;

  if (scale != self->subsurface.scale)
    wl_surface_set_buffer_scale(self->subsurface.surface, scale);

  // NOTE: position is unscaled but size is scaled

  if (geometry->x != self->subsurface.geometry.x ||
      geometry->y != self->subsurface.geometry.y) {
    wl_subsurface_set_position(self->subsurface.subsurface, geometry->x,
                               geometry->y);
  }

  if (geometry->width != self->subsurface.geometry.width ||
      geometry->height != self->subsurface.geometry.height ||
      scale != self->subsurface.scale) {
    wl_egl_window_resize(self->subsurface.egl_window, geometry->width * scale,
                         geometry->height * scale, 0, 0);
  }

  self->subsurface.geometry = *geometry;
  self->subsurface.scale = scale;
}

static void fl_renderer_wayland_class_init(FlRendererWaylandClass* klass) {
  FL_RENDERER_CLASS(klass)->set_window = fl_renderer_wayland_set_window;
  FL_RENDERER_CLASS(klass)->create_display = fl_renderer_wayland_create_display;
  FL_RENDERER_CLASS(klass)->create_surfaces =
      fl_renderer_wayland_create_surfaces;
  FL_RENDERER_CLASS(klass)->set_geometry = fl_renderer_wayland_set_geometry;
}

static void fl_renderer_wayland_init(FlRendererWayland* self) {}

FlRendererWayland* fl_renderer_wayland_new() {
  return FL_RENDERER_WAYLAND(
      g_object_new(fl_renderer_wayland_get_type(), nullptr));
}
