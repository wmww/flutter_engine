// Copyright 2020 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fl_renderer_wayland.h"

#include <string>
#include <wayland-egl-core.h>

static wl_registry* registry_global = nullptr;
static wl_subcompositor* subcompositor_global = nullptr;

static void registry_handle_global (void *,
                                    wl_registry *registry,
                                    uint32_t id,
                                    const char *name,
                                    uint32_t max_version) {
  if (std::string{name} == std::string{wl_subcompositor_interface.name}) {
    const wl_interface* interface = &wl_subcompositor_interface;
    uint32_t version = std::min(static_cast<uint32_t>(interface->version),
                                max_version);
    subcompositor_global = static_cast<wl_subcompositor*>(
        wl_registry_bind(registry, id, interface, version));
  }
}

static void registry_handle_global_remove (void *, wl_registry *, uint32_t) {}

static const wl_registry_listener registry_listener = {
    .global = registry_handle_global,
    .global_remove = registry_handle_global_remove,
};

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

struct _FlRendererWayland {
  FlRenderer parent_instance;

  wl_surface* toplevel_surface;
  wl_subsurface* subsurface;
  wl_surface* subsurface_surface;
  wl_surface* resource_surface;
  wl_egl_window *subsurface_egl_window;
  wl_egl_window *resource_egl_window;
};

G_DEFINE_TYPE(FlRendererWayland, fl_renderer_wayland, fl_renderer_get_type())

static void fl_renderer_wayland_set_window(FlRenderer* renderer,
                                           GdkWindow* window) {
    FlRendererWayland* self = FL_RENDERER_WAYLAND(renderer);
    g_return_if_fail(GDK_IS_WAYLAND_WINDOW(window));
    self->toplevel_surface = gdk_wayland_window_get_wl_surface(
        GDK_WAYLAND_WINDOW(window));
}

// Implements FlRenderer::create_display
static EGLDisplay fl_renderer_wayland_create_display(FlRenderer* /*renderer*/) {
    GdkWaylandDisplay* gdk_display =
    GDK_WAYLAND_DISPLAY(gdk_display_get_default());
    g_return_val_if_fail(gdk_display, nullptr);
    return eglGetDisplay(gdk_wayland_display_get_wl_display(gdk_display));
}

static EGLSurfacePair fl_renderer_wayland_create_surface(FlRenderer* renderer,
                                                         EGLDisplay egl_display,
                                                         EGLConfig config) {
  static const EGLSurfacePair null_result{EGL_NO_SURFACE, EGL_NO_SURFACE};

  FlRendererWayland* self = FL_RENDERER_WAYLAND(renderer);

  GdkWaylandDisplay* gdk_display =
      GDK_WAYLAND_DISPLAY(gdk_display_get_default());
      g_return_val_if_fail(gdk_display, null_result);

  wl_compositor* compositor =
      gdk_wayland_display_get_wl_compositor(gdk_display);
      g_return_val_if_fail(compositor, null_result);

  self->subsurface_surface = wl_compositor_create_surface(compositor);
  self->resource_surface = wl_compositor_create_surface(compositor);
  g_return_val_if_fail(self->subsurface_surface, null_result);
  g_return_val_if_fail(self->resource_surface, null_result);

  self->subsurface_egl_window = wl_egl_window_create(
      self->subsurface_surface, 600, 400);
  self->resource_egl_window = wl_egl_window_create(
      self->resource_surface, 1, 1);
  g_return_val_if_fail(self->subsurface_egl_window, null_result);
  g_return_val_if_fail(self->resource_egl_window, null_result);

  EGLSurface visible = eglCreateWindowSurface(
      egl_display, config, self->subsurface_egl_window, nullptr);
  EGLSurface resource = eglCreateWindowSurface(
      egl_display, config, self->resource_egl_window, nullptr);
  g_return_val_if_fail(visible != EGL_NO_SURFACE, null_result);
  g_return_val_if_fail(resource != EGL_NO_SURFACE, null_result);

  lazy_initialize_wayland_globals();
  g_return_val_if_fail(subcompositor_global, null_result);

  self->subsurface = wl_subcompositor_get_subsurface(subcompositor_global,
                                                     self->subsurface_surface,
                                                     self->toplevel_surface);
  g_return_val_if_fail(self->subsurface, null_result);
  wl_subsurface_set_desync(self->subsurface);

  return EGLSurfacePair{visible, resource};
}

static void fl_renderer_wayland_class_init(FlRendererWaylandClass* klass) {
  FL_RENDERER_CLASS(klass)->set_window = fl_renderer_wayland_set_window;
  FL_RENDERER_CLASS(klass)->create_display = fl_renderer_wayland_create_display;
  FL_RENDERER_CLASS(klass)->create_surface = fl_renderer_wayland_create_surface;
}

static void fl_renderer_wayland_init(FlRendererWayland* self) {}

FlRendererWayland* fl_renderer_wayland_new() {
  return FL_RENDERER_WAYLAND(
      g_object_new(fl_renderer_wayland_get_type(), nullptr));
}
