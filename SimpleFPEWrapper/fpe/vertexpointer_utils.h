#pragma once

#include <GL/gl.h>

int vp2idx(GLenum vp);
uint32_t vp_mask(GLenum vp);
GLenum idx2vp(int idx);
