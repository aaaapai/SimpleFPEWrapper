#include "init.h"
#include "fpe/fpe.hpp"

SFPEW::External::EGLFunctionsTable g_eglFuncs;
SFPEW::External::GLESFunctionsTable g_glesFuncs;

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

    if (!SFPEW::Utils::BackendLoader::AcquireGLESFunctions(g_glesFuncs, g_eglFuncs.eglGetProcAddress) ||
        g_glesFuncs.glGetString == nullptr) {
        throw std::runtime_error("Failed to acquire GLES functions");
    } // FIXME: actually we should acquire gles functions after egl initialization

    init_fpe();
}

struct InitClass {
    InitClass() { Init(); }
};

static InitClass staticInitObject;
