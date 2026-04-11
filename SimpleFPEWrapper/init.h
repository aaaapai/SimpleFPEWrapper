#pragma once
#include <string>
#include <cstring>
#include <stdexcept>
#include "backend/loader.h"

#define SFPEW_APIENTRY __attribute__((visibility("default"))) extern "C"
extern SFPEW::External::EGLFunctionsTable g_eglFuncs;
extern SFPEW::External::BackendGLFunctionsTable g_glFuncs;

SFPEW_APIENTRY const GLubyte* glGetString(GLenum name);
SFPEW_APIENTRY const GLubyte* glGetStringi(GLenum name, GLuint index);
SFPEW_APIENTRY void glGetIntegerv(GLenum pname, GLint* params);
SFPEW_APIENTRY void glDrawArrays(GLenum mode, GLint first, GLsizei count);
SFPEW_APIENTRY void glGetFloatv(GLenum pname, GLfloat* params);
