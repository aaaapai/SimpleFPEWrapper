#pragma once

#include <GL/gl.h>

#define DEBUG 0

class PointerUtils {
public:
    static int type_to_bytes(GLenum type);
    static int pname_to_count(GLenum pname);
};
