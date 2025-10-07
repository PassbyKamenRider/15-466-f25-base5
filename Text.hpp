#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <map>
#include <vector>

#include "data_path.hpp"
#include "gl_compile_program.hpp"
#include "gl_errors.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <hb-ft.h>
#include <hb.h>

struct Character {
    unsigned int TextureID;
    glm::ivec2 Size;
    glm::ivec2 Bearing;
    unsigned int Advance;

    static Character Load(hb_codepoint_t glyph_id, FT_Face face);
};

class Text {
public:
    Text(float font_size);
    void Set_Text(const std::string &str);
    void Render_Text(float x, float y, glm::vec2 window_size, glm::vec3 color);
    int GetLineCount();

private:
    FT_Face typeface;
    hb_font_t* hb_font;
    hb_buffer_t* hb_buffer;

    std::map<hb_codepoint_t, Character> cache;

    unsigned int VAO, VBO;
    unsigned int program;
    float font_size;
    std::vector<std::string> lines;
};