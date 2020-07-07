// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fl_renderer_headless.h"

struct _FlRendererHeadless {
  FlRenderer parent_instance;
};

G_DEFINE_TYPE(FlRendererHeadless, fl_renderer_headless, fl_renderer_get_type())


// Implments FlRenderer::get_visual.
static GdkVisual* fl_renderer_headless_get_visual(FlRenderer* renderer,
                                                  GdkScreen* screen,
                                                  EGLint visual_id) {
    g_error("fl_renderer_headless_get_visual() not implemented");
    return nullptr;
}

// Implements FlRenderer::set_window
static void fl_renderer_headless_set_window (FlRenderer* renderer,
                                        GdkWindow* window) {
}

static EGLSurface fl_renderer_headless_create_surface(FlRenderer* renderer,
                                                      EGLDisplay display,
                                                      EGLConfig config) {
  return EGL_NO_SURFACE;
}

static void fl_renderer_headless_class_init(FlRendererHeadlessClass* klass) {
  FL_RENDERER_CLASS(klass)->get_visual = fl_renderer_headless_get_visual;
  FL_RENDERER_CLASS(klass)->set_window = fl_renderer_headless_set_window;
  FL_RENDERER_CLASS(klass)->create_surface =
      fl_renderer_headless_create_surface;
}

static void fl_renderer_headless_init(FlRendererHeadless* self) {}

FlRendererHeadless* fl_renderer_headless_new() {
  return FL_RENDERER_HEADLESS(
      g_object_new(fl_renderer_headless_get_type(), nullptr));
}
