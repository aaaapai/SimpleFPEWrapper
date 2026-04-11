#include "init.h"
#include "fpe/fpe.hpp"

SFPEW::External::EGLFunctionsTable g_eglFuncs;
SFPEW::External::BackendGLFunctionsTable g_glFuncs;

void Init() {
    std::string eglLibName;
    const char* envEglLib = std::getenv("SFPEW_EGL");
    if (envEglLib) {
        eglLibName = envEglLib;
    } else {
        eglLibName = "libEGL.so";
    }

    if (!SFPEW::Utils::BackendLoader::AcquireEGLFunctions(g_eglFuncs, eglLibName) ||
        g_eglFuncs.eglGetProcAddress == nullptr) {
        throw std::runtime_error("Failed to acquire EGL functions");
    }

    if (!SFPEW::Utils::BackendLoader::AcquireBackendGLFunctions(g_glFuncs, g_eglFuncs.eglGetProcAddress) ||
        g_glFuncs.glGetString == nullptr) {
        throw std::runtime_error("Failed to acquire BackendGL functions");
    } // FIXME: actually we should acquire gl functions after egl initialization

    init_fpe();
}

struct InitClass {
    InitClass() { Init(); }
};

static InitClass staticInitObject;
