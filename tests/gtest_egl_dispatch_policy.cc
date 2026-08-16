// SimpleFPEWrapper - tests/gtest_egl_dispatch_policy.cc
// Copyright (c) 2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
// https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "SimpleFPEWrapper/egl_dispatch.h"
#include "SimpleFPEWrapper/init.h"

#include <gtest/gtest.h>

#include <vector>

SFPEW::External::EGLFunctionsTable g_eglFuncs;
SFPEW::External::BackendGLFunctionsTable g_glFuncs;

namespace {

using Request = SfpewEglContextRequest;

TEST(EglDispatchPolicy, StrictDesktopCoreIsDirect) {
    const EGLint attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE,
    };

    EXPECT_EQ(sfpewClassifyEglContextAttributes(attribs, true).request,
              Request::CoreOnly);
}

TEST(EglDispatchPolicy, ExplicitCompatibilityProducesCoreFallback) {
    const EGLint attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 4,
        EGL_CONTEXT_MINOR_VERSION, 6,
        EGL_CONTEXT_OPENGL_PROFILE_MASK,
        EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,
        EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE, EGL_TRUE,
        EGL_CONTEXT_FLAGS_KHR,
        EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR | 0x40,
        0xdead, 7,
        EGL_NONE,
    };

    const auto result = sfpewClassifyEglContextAttributes(attribs, true);
    EXPECT_EQ(result.request, Request::Wrapped)
        << "forward-compatible compatibility requests must stay wrapped";

    const EGLint compatible_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 4,
        EGL_CONTEXT_MINOR_VERSION, 6,
        EGL_CONTEXT_OPENGL_PROFILE_MASK,
        EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,
        EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE, EGL_FALSE,
        EGL_CONTEXT_FLAGS_KHR, 0x40,
        0xdead, 7,
        EGL_NONE,
    };
    const auto compatible =
        sfpewClassifyEglContextAttributes(compatible_attribs, true);
    EXPECT_EQ(compatible.request, Request::Compatibility);
    EXPECT_EQ(compatible.core_fallback,
              (std::vector<EGLint>{
                  EGL_CONTEXT_MAJOR_VERSION, 4,
                  EGL_CONTEXT_MINOR_VERSION, 6,
                  EGL_CONTEXT_OPENGL_PROFILE_MASK,
                  EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
                  EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE, EGL_FALSE,
                  EGL_CONTEXT_FLAGS_KHR, 0x40,
                  0xdead, 7,
                  EGL_NONE,
              }));
}

TEST(EglDispatchPolicy, ForwardCompatibleAndMixedProfilesStayWrapped) {
    const EGLint forward[] = {
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE, EGL_TRUE,
        EGL_NONE,
    };
    const EGLint forward_khr[] = {
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_CONTEXT_FLAGS_KHR,
        EGL_CONTEXT_OPENGL_FORWARD_COMPATIBLE_BIT_KHR,
        EGL_NONE,
    };
    const EGLint mixed[] = {
        EGL_CONTEXT_OPENGL_PROFILE_MASK,
        EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT |
            EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,
        EGL_NONE,
    };

    EXPECT_EQ(sfpewClassifyEglContextAttributes(forward, true).request,
              Request::Wrapped);
    EXPECT_EQ(sfpewClassifyEglContextAttributes(forward_khr, true).request,
              Request::Wrapped);
    EXPECT_EQ(sfpewClassifyEglContextAttributes(mixed, true).request,
              Request::Wrapped);
}

TEST(EglDispatchPolicy, NonDesktopAndUnspecifiedRequestsStayWrapped) {
    const EGLint core[] = {
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE,
    };
    const EGLint no_profile[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_NONE};

    EXPECT_EQ(sfpewClassifyEglContextAttributes(core, false).request,
              Request::Wrapped);
    EXPECT_EQ(sfpewClassifyEglContextAttributes(no_profile, true).request,
              Request::Wrapped);
    EXPECT_EQ(sfpewClassifyEglContextAttributes(nullptr, true).request,
              Request::Wrapped);
}

TEST(EglDispatchPolicy, DuplicateAndUnterminatedRelevantListsStayWrapped) {
    const EGLint duplicate_profile[] = {
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_CONTEXT_OPENGL_PROFILE_MASK,
        EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,
        EGL_NONE,
    };
    const EGLint duplicate_flags[] = {
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_CONTEXT_FLAGS_KHR, 0,
        EGL_CONTEXT_FLAGS_KHR, 0,
        EGL_NONE,
    };
    std::vector<EGLint> unterminated(256, EGL_CONTEXT_MAJOR_VERSION);

    EXPECT_EQ(sfpewClassifyEglContextAttributes(duplicate_profile, true).request,
              Request::Wrapped);
    EXPECT_EQ(sfpewClassifyEglContextAttributes(duplicate_flags, true).request,
              Request::Wrapped);
    EXPECT_EQ(sfpewClassifyEglContextAttributes(unterminated.data(), true).request,
              Request::Wrapped);
}

} // namespace
