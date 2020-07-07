// Copyright 2020 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fl_renderer_wayland.h"

struct _FlRendererWayland {
  FlRenderer parent_instance;

  struct wl_surface* surface;
};

G_DEFINE_TYPE(FlRendererWayland, fl_renderer_wayland, fl_renderer_get_type())

static GdkVisual* fl_renderer_wayland_get_visual(FlRenderer* renderer,
                                                 GdkScreen* screen,
                                                 EGLint visual_id) {
    g_warning("fl_renderer_wayland_get_visual() not implemented");
    return nullptr;
}

static void fl_renderer_wayland_set_window(FlRenderer* renderer,
                                           GdkWindow* window) {
    FlRendererWayland* self = FL_RENDERER_WAYLAND(renderer);
    g_return_if_fail(GDK_IS_WAYLAND_WINDOW(window));
    self->surface = gdk_wayland_window_get_wl_surface(
        GDK_WAYLAND_WINDOW(window));
}

static EGLSurface fl_renderer_wayland_create_surface(FlRenderer* renderer,
                                                     EGLDisplay display,
                                                     EGLConfig config) {
  FlRendererWayland* self = FL_RENDERER_WAYLAND(renderer);
  return eglCreateWindowSurface(
      display, config, reinterpret_cast<EGLNativeWindowType>(self->surface),
      nullptr);
}

static void fl_renderer_wayland_class_init(FlRendererWaylandClass* klass) {
  FL_RENDERER_CLASS(klass)->get_visual = fl_renderer_wayland_get_visual;
  FL_RENDERER_CLASS(klass)->set_window = fl_renderer_wayland_set_window;
  FL_RENDERER_CLASS(klass)->create_surface = fl_renderer_wayland_create_surface;
}

static void fl_renderer_wayland_init(FlRendererWayland* self) {}

FlRendererWayland* fl_renderer_wayland_new() {
  return FL_RENDERER_WAYLAND(
      g_object_new(fl_renderer_wayland_get_type(), nullptr));
}
