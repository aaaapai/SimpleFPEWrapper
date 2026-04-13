// SimpleFPEWrapper - SimpleFPEWrapper/drawing.cpp
// Copyright (c) 2026 MobileGL-Dev
// Licensed under the GNU Lesser General Public License v3.0:
//   https://www.gnu.org/licenses/gpl-3.0.txt
//   https://www.gnu.org/licenses/lgpl-3.0.txt
// SPDX-License-Identifier: LGPL-3.0-only
// End of Source File Header

#include "init.h"

#include "fpe/fpe.hpp"
#include "fpe/list.h"

void glDrawArrays(GLenum mode, GLint first, GLsizei count) {
    LIST_RECORD(glDrawArrays, {}, mode, first, count)

    // TODO: deal with draw in list later
    if (DisplayListManager::isCalling()) {
        return;
    }

    GET_PREV_PROGRAM
    int do_draw_element = commit_fpe_state_on_draw(&mode, &first, &count);
    if (do_draw_element) {
        g_glFuncs.glDrawElements(mode, count, GL_UNSIGNED_INT, (void*)0);
    } else
        g_glFuncs.glDrawArrays(mode, first, count);

    SET_PREV_PROGRAM
    g_glFuncs.glBindVertexArray(0);
}

void glTexImage2D(GLenum target, GLint level, GLint internalFormat, GLsizei width, GLsizei height, GLint border,
                  GLenum format, GLenum type, const GLvoid* pixels) {
    // Fix for 1.12
    if (format == GL_BGRA && type == GL_UNSIGNED_INT_8_8_8_8_REV) {
        internalFormat = GL_BGRA;
        format = GL_BGRA;
        type = GL_UNSIGNED_BYTE;
    }
    g_glFuncs.glTexImage2D(target, level, internalFormat, width, height, border, format, type, pixels);
}

void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
                     GLenum format, GLenum type, const void* pixels) {
    if (format == GL_BGRA && (type == GL_UNSIGNED_INT_8_8_8_8)) {
        format = GL_RGBA;
        type = GL_UNSIGNED_BYTE;
    }
    if (format == GL_BGRA && type == GL_UNSIGNED_INT_8_8_8_8_REV) {
        type = GL_UNSIGNED_BYTE;
    }

    g_glFuncs.glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void* pixels) {

    if (format == GL_BGRA && type == GL_UNSIGNED_INT_8_8_8_8) {
        format = GL_RGBA;
        type = GL_UNSIGNED_BYTE;
    }
    g_glFuncs.glReadPixels(x, y, width, height, format, type, pixels);
}
