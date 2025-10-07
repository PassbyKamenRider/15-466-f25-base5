// reference: https://learnopengl.com/In-Practice/Text-Rendering
// reference: https://github.com/cmu15466-gsilvera/15-466-f22-base4 for clearing buffer to reload text

#include "Text.hpp"
#include <sstream>

Character Character::Load(hb_codepoint_t glyph_id, FT_Face face) {
    FT_Load_Glyph(face, glyph_id, FT_LOAD_RENDER);

    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, face->glyph->bitmap.width, face->glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE, face->glyph->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    Character ch(
        tex,
        glm::ivec2(face->glyph->bitmap.width, face->glyph->bitmap.rows),
        glm::ivec2(face->glyph->bitmap_left, face->glyph->bitmap_top),
        (unsigned int) (face->glyph->advance.x)
    );

    return ch;
}

Text::Text(float font_size) {
    FT_Library ft;
    if (FT_Init_FreeType(&ft))
        throw std::runtime_error("FT_Init_FreeType failed");

    if (FT_New_Face(ft, data_path("SourceHanSansSC-VF.ttf").c_str(), 0, &typeface))
        throw std::runtime_error("FT_New_Face failed");

    FT_Set_Pixel_Sizes(typeface, 0, (FT_UInt) (font_size));

    hb_font = hb_ft_font_create(typeface, nullptr);
    hb_buffer = hb_buffer_create();

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    const char* vs = R"(
        #version 330 core
        layout(location=0) in vec4 vertex;
        out vec2 TexCoords;
        uniform mat4 projection;
        void main() {
            gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
            TexCoords = vertex.zw;
        }
    )";

    const char* fs = R"(
        #version 330 core
        in vec2 TexCoords;
        out vec4 color;
        uniform sampler2D text;
        uniform vec3 textColor;
        void main() {
            float alpha = texture(text, TexCoords).r;
            color = vec4(textColor, alpha);
        }
    )";

    program = gl_compile_program(vs, fs);
}

void Text::Set_Text(const std::string &str) {
    lines.clear();
    std::stringstream ss(str);
    std::string line;
    while (std::getline(ss, line, '\n')) {
        lines.push_back(line);
    }
}

int Text::GetLineCount() {
    return (int) lines.size();
}

void Text::Render_Text(float x, float y, glm::vec2 window_size, glm::vec3 color) {
    glUseProgram(program);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glm::mat4 proj = glm::ortho(0.0f, window_size.x, 0.0f, window_size.y);
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, &proj[0][0]);
    glUniform3f(glGetUniformLocation(program, "textColor"), color.r, color.g, color.b);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(VAO);

    float line_height = (float) (typeface->size->metrics.height >> 6);

    for (size_t l = 0; l < lines.size(); l++) {
        hb_buffer_clear_contents(hb_buffer);
        hb_buffer_add_utf8(hb_buffer, lines[l].c_str(), -1, 0, -1);
        hb_buffer_guess_segment_properties(hb_buffer);
        hb_shape(hb_font, hb_buffer, nullptr, 0);

        unsigned int len = hb_buffer_get_length(hb_buffer);
        auto* info = hb_buffer_get_glyph_infos(hb_buffer, nullptr);
        auto* pos_glyph = hb_buffer_get_glyph_positions(hb_buffer, nullptr);

        float pen_x = x;
        float pen_y = y - (float) (l) * line_height;

        for (unsigned int i = 0; i < len; i++) {
            hb_codepoint_t glyph_id = info[i].codepoint;

            if (cache.find(glyph_id) == cache.end()) {
                cache[glyph_id] = Character::Load(glyph_id, typeface);
            }

            Character &ch = cache[glyph_id];

            float x_offset = (float) (pos_glyph[i].x_offset) / 64.0f;
            float y_offset = (float) (pos_glyph[i].y_offset) / 64.0f;

            float xpos = pen_x + ch.Bearing.x + x_offset;
            float ypos = pen_y - (ch.Size.y - ch.Bearing.y) + y_offset;
            float w = (float) (ch.Size.x);
            float h = (float) (ch.Size.y);
            float vertices[6][4] = {
                { xpos,     ypos + h,   0.0f, 0.0f },            
                { xpos,     ypos,       0.0f, 1.0f },
                { xpos + w, ypos,       1.0f, 1.0f },

                { xpos,     ypos + h,   0.0f, 0.0f },
                { xpos + w, ypos,       1.0f, 1.0f },
                { xpos + w, ypos + h,   1.0f, 0.0f }           
            };

            glBindTexture(GL_TEXTURE_2D, ch.TextureID);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            pen_x += (float) (ch.Advance >> 6) + x_offset;
            pen_y += y_offset;
        }
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}