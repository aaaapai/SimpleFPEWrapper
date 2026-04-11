#pragma once

#include <GL/gl.h>

#include "types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    GLAPI GLAPIENTRY void glEnable(GLenum cap);
    GLAPI GLAPIENTRY void glDisable(GLenum cap);
    GLAPI GLAPIENTRY void glClientActiveTexture(GLenum texture);
    GLAPI void GLAPIENTRY glAlphaFunc(GLenum func, GLclampf ref);
    GLAPI void GLAPIENTRY glFogf(GLenum pname, GLfloat param);
    GLAPI void GLAPIENTRY glFogi(GLenum pname, GLint param);
    GLAPI void GLAPIENTRY glFogfv(GLenum pname, const GLfloat* params);
    GLAPI void GLAPIENTRY glFogiv(GLenum pname, const GLint* params);

    GLAPI void GLAPIENTRY glShadeModel(GLenum mode);
    GLAPI void GLAPIENTRY glLightf(GLenum light, GLenum pname, GLfloat param);
    GLAPI void GLAPIENTRY glLighti(GLenum light, GLenum pname, GLint param);
    GLAPI void GLAPIENTRY glLightfv(GLenum light, GLenum pname, const GLfloat* params);
    GLAPI void GLAPIENTRY glLightiv(GLenum light, GLenum pname, const GLint* params);
    GLAPI void GLAPIENTRY glLightModelf(GLenum pname, GLfloat param);
    GLAPI void GLAPIENTRY glLightModeli(GLenum pname, GLint param);
    GLAPI void GLAPIENTRY glLightModelfv(GLenum pname, const GLfloat* params);
    GLAPI void GLAPIENTRY glLightModeliv(GLenum pname, const GLint* params);
    GLAPI void GLAPIENTRY glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);

#ifdef __cplusplus
}
#endif
