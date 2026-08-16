// SimpleFPEWrapper - tests/gtest_egl_direct_dispatch.cc
// Copyright (c) 2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "sfpew_gtest.h"

#include <dlfcn.h>

namespace {

using ResolveFn = __eglMustCastToProperFunctionPointerType (*)(const char*);
using CreateContextFn = EGLContext (*)(EGLDisplay, EGLConfig, EGLContext, const EGLint*);
using MakeCurrentFn = EGLBoolean (*)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
using DestroyContextFn = EGLBoolean (*)(EGLDisplay, EGLContext);

class EglDirectDispatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        wrapper_ = dlopen(WRAPPER_LIB_PATH, RTLD_NOW | RTLD_LOCAL);
        ASSERT_NE(wrapper_, nullptr) << dlerror();
        resolve_ = reinterpret_cast<ResolveFn>(dlsym(wrapper_, "eglGetProcAddress"));
        ASSERT_NE(resolve_, nullptr);

        display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display_ == EGL_NO_DISPLAY || !eglInitialize(display_, nullptr, nullptr))
            GTEST_SKIP() << "no EGL display";

        const EGLint config_attribs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                                         EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                                         EGL_NONE};
        EGLint count = 0;
        if (!eglChooseConfig(display_, config_attribs, &config_, 1, &count) || count == 0)
            GTEST_SKIP() << "no desktop GL config";

        const EGLint surface_attribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
        surface_ = eglCreatePbufferSurface(display_, config_, surface_attribs);
        if (surface_ == EGL_NO_SURFACE) GTEST_SKIP() << "no pbuffer";
        if (!eglBindAPI(EGL_OPENGL_API)) GTEST_SKIP() << "no desktop GL API";

        create_ = reinterpret_cast<CreateContextFn>(resolve_("eglCreateContext"));
        make_current_ = reinterpret_cast<MakeCurrentFn>(resolve_("eglMakeCurrent"));
        destroy_ = reinterpret_cast<DestroyContextFn>(resolve_("eglDestroyContext"));
        ASSERT_NE(create_, nullptr);
        ASSERT_NE(make_current_, nullptr);
        ASSERT_NE(destroy_, nullptr);
    }

    void TearDown() override {
        if (display_ != EGL_NO_DISPLAY) {
            if (make_current_ != nullptr)
                make_current_(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (context_ != EGL_NO_CONTEXT && destroy_ != nullptr) destroy_(display_, context_);
            if (surface_ != EGL_NO_SURFACE) eglDestroySurface(display_, surface_);
        }
        if (wrapper_ != nullptr) dlclose(wrapper_);
    }

    void* wrapper_ = nullptr;
    ResolveFn resolve_ = nullptr;
    CreateContextFn create_ = nullptr;
    MakeCurrentFn make_current_ = nullptr;
    DestroyContextFn destroy_ = nullptr;
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLConfig config_ = nullptr;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
};

TEST_F(EglDirectDispatchTest, CoreContextUsesBackendPointersAfterItBecomesCurrent) {
    const EGLint core_attribs[] = {EGL_CONTEXT_MAJOR_VERSION,
                                   3,
                                   EGL_CONTEXT_MINOR_VERSION,
                                   3,
                                   EGL_CONTEXT_OPENGL_PROFILE_MASK,
                                   EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                                   EGL_NONE};
    context_ = create_(display_, config_, EGL_NO_CONTEXT, core_attribs);
    if (context_ == EGL_NO_CONTEXT) GTEST_SKIP() << "no core profile context";
    ASSERT_EQ(make_current_(display_, surface_, surface_, context_), EGL_TRUE);

    const auto wrapper_draw_arrays = resolve_("glDrawArrays");
    const auto backend_draw_arrays = eglGetProcAddress("glDrawArrays");
    const auto wrapper_make_current = resolve_("eglMakeCurrent");
    const auto backend_make_current = eglGetProcAddress("eglMakeCurrent");

    ASSERT_NE(backend_draw_arrays, nullptr);
    ASSERT_NE(backend_make_current, nullptr);
    EXPECT_EQ(wrapper_draw_arrays, backend_draw_arrays);
    EXPECT_EQ(wrapper_make_current, backend_make_current);
}

} // namespace
