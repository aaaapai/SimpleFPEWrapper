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
        g_glesFuncs.glDrawElements(mode, count, GL_UNSIGNED_INT, (void*)0);
    } else
        g_glesFuncs.glDrawArrays(mode, first, count);

    SET_PREV_PROGRAM
    g_glesFuncs.glBindVertexArray(0);
}
