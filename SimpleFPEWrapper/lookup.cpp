#include "EGL/egl.h"
#include "init.h"
#include "fpe/transformation.h"

#define GETPROC(name, var)                                                                                             \
    if (std::strcmp(#name, var) == 0) {                                                                                \
        return (__eglMustCastToProperFunctionPointerType)name;                                                         \
    }

SFPEW_APIENTRY __eglMustCastToProperFunctionPointerType eglGetProcAddress(const char* name) {
    GETPROC(glGetString, name)
    GETPROC(glGetStringi, name)
    GETPROC(glGetIntegerv, name)
    GETPROC(glDrawArrays, name)

    GETPROC(glBegin, name)
    GETPROC(glEnd, name)
    // GETPROC(glVertex2d, name)
    // GETPROC(glVertex2f, name)
    // GETPROC(glVertex2i, name)
    // GETPROC(glVertex2s, name)
    // GETPROC(glVertex3d, name)
    GETPROC(glVertex3f, name)
    // GETPROC(glVertex3i, name)
    // GETPROC(glVertex3s, name)
    // GETPROC(glVertex4d, name)
    GETPROC(glVertex4f, name)
    // GETPROC(glVertex4i, name)
    // GETPROC(glVertex4s, name)
    // GETPROC(glVertex2dv, name)
    // GETPROC(glVertex2fv, name)
    // GETPROC(glVertex2iv, name)
    // GETPROC(glVertex2sv, name)
    // GETPROC(glVertex3dv, name)
    // GETPROC(glVertex3fv, name)
    // GETPROC(glVertex3iv, name)
    // GETPROC(glVertex3sv, name)
    // GETPROC(glVertex4dv, name)
    // GETPROC(glVertex4fv, name)
    // GETPROC(glVertex4iv, name)
    // GETPROC(glVertex4sv, name)
    // GETPROC(glNormal3b, name)
    // GETPROC(glNormal3d, name)
    GETPROC(glNormal3f, name)
    // GETPROC(glNormal3i, name)
    // GETPROC(glNormal3s, name)
    // GETPROC(glNormal3bv, name)
    // GETPROC(glNormal3dv, name)
    // GETPROC(glNormal3fv, name)
    // GETPROC(glNormal3iv, name)
    // GETPROC(glNormal3sv, name)
    GETPROC(glGenLists, name)
    GETPROC(glDeleteLists, name)
    GETPROC(glIsList, name)
    GETPROC(glNewList, name)
    GETPROC(glEndList, name)
    GETPROC(glCallList, name)
    GETPROC(glCallLists, name)
    GETPROC(glListBase, name)
    GETPROC(glEnable, name)
    GETPROC(glDisable, name)
    GETPROC(glClientActiveTexture, name)
    GETPROC(glAlphaFunc, name)
    GETPROC(glFogf, name)
    GETPROC(glFogi, name)
    GETPROC(glFogfv, name)
    GETPROC(glFogiv, name)
    GETPROC(glShadeModel, name)
    GETPROC(glLightf, name)
    GETPROC(glLighti, name)
    GETPROC(glLightfv, name)
    GETPROC(glLightiv, name)
    GETPROC(glLightModelf, name)
    GETPROC(glLightModeli, name)
    GETPROC(glLightModelfv, name)
    GETPROC(glLightModeliv, name)
    GETPROC(glColor4f, name)
    GETPROC(glMatrixMode, name)
    GETPROC(glLoadIdentity, name)
    GETPROC(glOrtho, name)
    GETPROC(glOrthof, name)
    GETPROC(glScalef, name)
    GETPROC(glTranslatef, name)
    GETPROC(glRotatef, name)
    GETPROC(glMultMatrixf, name)
    GETPROC(glRotated, name)
    GETPROC(glScaled, name)
    GETPROC(glTranslated, name)
    GETPROC(glPushMatrix, name)
    GETPROC(glPopMatrix, name)
    GETPROC(glVertexPointer, name)
    GETPROC(glNormalPointer, name)
    GETPROC(glColorPointer, name)
    GETPROC(glTexCoordPointer, name)
    GETPROC(glIndexPointer, name)
    GETPROC(glEnableClientState, name)
    GETPROC(glDisableClientState, name)
    GETPROC(glGetFloatv, name)
    GETPROC(glTexImage2D, name)
    GETPROC(glTexSubImage2D, name)
    GETPROC(glReadPixels, name)
    if (std::strcmp("glDeleteBuffersARB", name) == 0) {
        name = "glDeleteBuffers";
    } else if (std::strcmp("glGenBuffersARB", name) == 0) {
        name = "glGenBuffers";
    } else if (std::strcmp("glBindBufferARB", name) == 0) {
        name = "glBindBuffer";
    } else if (std::strcmp("glBufferDataARB", name) == 0) {
        name = "glBufferData";
    } else if (std::strcmp("glBufferSubDataARB", name) == 0) {
        name = "glBufferSubData";
    } else if (std::strcmp("glGetBufferSubDataARB", name) == 0) {
        name = "glGetBufferSubData";
    } else if (std::strcmp("glMapBufferARB", name) == 0) {
        name = "glMapBuffer";
    } else if (std::strcmp("glUnmapBufferARB", name) == 0) {
        name = "glUnmapBuffer";
    } else if (std::strcmp("glGetBufferParameterivARB", name) == 0) {
        name = "glGetBufferParameteriv";
    } else if (std::strcmp("glGetBufferPointervARB", name) == 0) {
        name = "glGetBufferPointerv";
    } else if (std::strcmp("glIsBufferARB", name) == 0) {
        name = "glIsBuffer";
    }

    __eglMustCastToProperFunctionPointerType ptr = g_eglFuncs.eglGetProcAddress(name);
    if (!ptr) {
        printf("eglGetProcAddress: eglGetProcAddress also failed to find '%s'\n", name);
    }
    return ptr;
}

SFPEW_APIENTRY void* glXGetProcAddress(const char* name) {
    return (void*)eglGetProcAddress(name);
}

SFPEW_APIENTRY void* glXGetProcAddressARB(const char* name) {
    return glXGetProcAddress(name);
}
