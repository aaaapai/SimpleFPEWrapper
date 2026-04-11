#pragma once

#include <GL/gl.h>
#include "transformation.h"
#include "state.h"
#include "vertexpointer.h"
#include "../init.h"

#define GET_PREV_PROGRAM                                                                                               \
    GLint m_prev_program;                                                                                              \
    g_glFuncs.glGetIntegerv(GL_CURRENT_PROGRAM, &m_prev_program);
#define SET_PREV_PROGRAM                                                                                               \
    if (m_prev_program) g_glFuncs.glUseProgram(m_prev_program);

#define g_glstate glstate_t::get_instance()

extern bool fpe_inited;

int init_fpe();

// 0 - keep DrawArray, 1 - switch to DrawElements
int commit_fpe_state_on_draw(GLenum* mode, GLint* first, GLsizei* count);
