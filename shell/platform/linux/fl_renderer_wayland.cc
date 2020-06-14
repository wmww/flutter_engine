// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fl_renderer_wayland.h"

struct _FlRendererWayland {
  FlRenderer parent_instance;

  wl_egl_window* wl_window;
};

G_DEFINE_TYPE(FlRendererWayland, fl_renderer_wayland, fl_renderer_get_type())

static EGLSurface fl_renderer_wayland_create_surface(FlRenderer* renderer,
                                                     EGLDisplay display,
                                                     EGLConfig config) {
  FlRendererWayland* self = FL_RENDERER_WAYLAND(renderer);
  return eglCreateWindowSurface(
      display, config, reinterpret_cast<EGLNativeWindowType>(self->wl_window),
      nullptr);
}

static void fl_renderer_wayland_class_init(FlRendererWaylandClass* klass) {
  FL_RENDERER_CLASS(klass)->create_surface = fl_renderer_wayland_create_surface;
}

static void fl_renderer_wayland_init(FlRendererWayland* self) {}

FlRendererWayland* fl_renderer_wayland_new() {
  return FL_RENDERER_WAYLAND(
      g_object_new(fl_renderer_wayland_get_type(), nullptr));
}

void fl_renderer_wayland_set_window(FlRendererWayland* self,
                                    wl_egl_window* window) {
  g_return_if_fail(FL_IS_RENDERER_WAYLAND(self));
  self->wl_window = window;
}
