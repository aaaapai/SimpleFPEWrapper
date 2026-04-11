#pragma once

#include <string>
#include "defines.h"

struct scratch_t {
    std::string last_stage_linkage;
    std::string vs_body;

    bool has_vertex_color = false;
    bool has_texcoord[MAX_TEX] = {false};
};

class fpe_shader_generator {
public:
    fpe_shader_generator(const struct fixed_function_state_t& state) : state_(state) {}

    struct program_t generate_program();

private:
    static std::string vertex_shader(const struct fixed_function_state_t& state, scratch_t& scratch);
    static std::string fragment_shader(const struct fixed_function_state_t& state, scratch_t& scratch);

    const fixed_function_state_t& state_;
    scratch_t scratch_;
};
