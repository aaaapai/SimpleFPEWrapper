#pragma once

#include "GL/gl.h"
#include "types.h"
#include "vertexpointer_utils.h"

#ifdef __cplusplus
extern "C"
{
#endif

    GLAPI GLAPIENTRY void glVertexPointer(GLint size, GLenum type, GLsizei stride, const void* pointer);
    GLAPI GLAPIENTRY void glNormalPointer(GLenum type, GLsizei stride, const GLvoid* pointer);
    GLAPI GLAPIENTRY void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
    GLAPI GLAPIENTRY void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid* pointer);
    GLAPI GLAPIENTRY void glIndexPointer(GLenum type, GLsizei stride, const GLvoid* ptr);

    GLAPI GLAPIENTRY void glEnableClientState(GLenum cap);
    GLAPI GLAPIENTRY void glDisableClientState(GLenum cap);

#ifdef __cplusplus
}
#endif
