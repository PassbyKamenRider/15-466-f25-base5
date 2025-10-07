#include "PlayMode.hpp"

#include "DrawLines.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"
#include "hex_dump.hpp"
#include "load_save_png.hpp"
#include "ColorTextureProgram.hpp"
#include "ColorProgram.hpp"

#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <random>
#include <array>

GLuint arenaVAO = 0;
GLuint arenaVBO = 0;
GLuint arenaEBO = 0;

GLuint LoadTexture(std::string const &filename) {
    glm::uvec2 size;
    std::vector<glm::u8vec4> data;

    load_png(filename, &size, &data, UpperLeftOrigin);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        size.x,
        size.y,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        data.data()
    );

    glBindTexture(GL_TEXTURE_2D, 0);

    return tex;
}

PlayMode::PlayMode(Client &client_) : client(client_), p1Text(32.f), p2Text(32.f), endText(64.f) {
	p1Text.Set_Text("GA");
	p2Text.Set_Text("ME");
	endText.Set_Text("You Made The Game!");

	// if (game.obstacles.size() == 0)
	// {
	// 	game.obstacles.push_back({glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 1.0f)});
    // 	game.obstacles.push_back({glm::vec2(4.0f, 2.0f), glm::vec2(0.5f, 3.0f)});
	// }

	arenaTexture = LoadTexture(data_path("background.png"));

	float vertices[] = {
		0.f, 0.f, 0.f, 0.f,
		1.f, 0.f, 1.f, 0.f,
		1.f, 1.f, 1.f, 1.f,
		0.f, 1.f, 0.f, 1.f
	};
	unsigned int indices[] = {0,1,2, 0,2,3};

	glGenVertexArrays(1, &arenaVAO);
	glGenBuffers(1, &arenaVBO);
	glGenBuffers(1, &arenaEBO);

	glBindVertexArray(arenaVAO);

	glBindBuffer(GL_ARRAY_BUFFER, arenaVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, arenaEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	glEnableVertexAttribArray(color_texture_program->Position_vec4);
	glVertexAttribPointer(color_texture_program->Position_vec4, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)0);

	glEnableVertexAttribArray(color_texture_program->TexCoord_vec2);
	glVertexAttribPointer(color_texture_program->TexCoord_vec2, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));

	glDisableVertexAttribArray(color_texture_program->Color_vec4);
	glVertexAttrib4f(color_texture_program->Color_vec4, 1.f, 1.f, 1.f, 1.f);

	glBindVertexArray(0);
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.repeat) {
			//ignore repeats
		} else if (evt.key.key == SDLK_A) {
			controls.left.downs += 1;
			controls.left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_D) {
			controls.right.downs += 1;
			controls.right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_W) {
			controls.up.downs += 1;
			controls.up.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_S) {
			controls.down.downs += 1;
			controls.down.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_SPACE) {
			controls.jump.downs += 1;
			controls.jump.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_A) {
			controls.left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_D) {
			controls.right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_W) {
			controls.up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_S) {
			controls.down.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_SPACE) {
			controls.jump.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {

	//queue data for sending to server:
	controls.send_controls_message(&client.connection);

	//reset button press counters:
	controls.left.downs = 0;
	controls.right.downs = 0;
	controls.up.downs = 0;
	controls.down.downs = 0;
	controls.jump.downs = 0;

	//send/receive data:
	client.poll([this](Connection *c, Connection::Event event){
		if (event == Connection::OnOpen) {
			std::cout << "[" << c->socket << "] opened" << std::endl;
		} else if (event == Connection::OnClose) {
			std::cout << "[" << c->socket << "] closed (!)" << std::endl;
			throw std::runtime_error("Lost connection to server!");
		} else { assert(event == Connection::OnRecv);
			//std::cout << "[" << c->socket << "] recv'd data. Current buffer:\n" << hex_dump(c->recv_buffer); std::cout.flush(); //DEBUG
			bool handled_message;
			try {
				do {
					handled_message = false;
					if (game.recv_state_message(c)) handled_message = true;
				} while (handled_message);
			} catch (std::exception const &e) {
				std::cerr << "[" << c->socket << "] malformed message from server: " << e.what() << std::endl;
				//quit the game:
				throw e;
			}
		}
	}, 0.0);
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {

	static std::array< glm::vec2, 16 > const circle = [](){
		std::array< glm::vec2, 16 > ret;
		for (uint32_t a = 0; a < ret.size(); ++a) {
			float ang = a / float(ret.size()) * 2.0f * float(M_PI);
			ret[a] = glm::vec2(std::cos(ang), std::sin(ang));
		}
		return ret;
	}();

	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_DEPTH_TEST);
	
	//figure out view transform to center the arena:
	float aspect = float(drawable_size.x) / float(drawable_size.y);
	float scale = std::min(
		2.0f * aspect / (Game::ArenaMax.x - Game::ArenaMin.x + 2.0f * Game::PlayerRadius),
		2.0f / (Game::ArenaMax.y - Game::ArenaMin.y + 2.0f * Game::PlayerRadius)
	);
	glm::vec2 offset = -0.5f * (Game::ArenaMax + Game::ArenaMin);

	glm::mat4 world_to_clip = glm::mat4(
		scale / aspect, 0.0f, 0.0f, offset.x,
		0.0f, scale, 0.0f, offset.y,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	);

	{
		DrawLines lines(world_to_clip);

		//helper:
		auto draw_text = [&](glm::vec2 const &at, std::string const &text, float H) {
			lines.draw_text(text,
				glm::vec3(at.x, at.y, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0x00, 0x00, 0x00, 0x00));
			float ofs = (1.0f / scale) / drawable_size.y;
			lines.draw_text(text,
				glm::vec3(at.x + ofs, at.y + ofs, 0.0),
				glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
				glm::u8vec4(0xff, 0xff, 0xff, 0x00));
		};

		// draw arena:
		{
			glUseProgram(color_texture_program->program);

			glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(Game::ArenaMin, 0.0f));
			model = glm::scale(model, glm::vec3(Game::ArenaMax - Game::ArenaMin, 1.0f));

			glUniformMatrix4fv(color_texture_program->CLIP_FROM_OBJECT_mat4, 1, GL_FALSE, glm::value_ptr(world_to_clip * model));

			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, arenaTexture);

			glBindVertexArray(arenaVAO);
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
			glBindVertexArray(0);

			glUseProgram(0);
		}

		// --- draw obstacles ---
		{}

		for (auto const &player : game.players) {
			glm::u8vec4 col = glm::u8vec4(player.color.x*255, player.color.y*255, player.color.z*255, 0xff);
			glm::vec4 clip = world_to_clip * glm::vec4(player.position + glm::vec2(0.0f, Game::PlayerRadius + 0.2f), 0.0f, 1.0f);
			glm::vec3 ndc = glm::vec3(clip) / clip.w;
			glm::vec2 screen_pos = glm::vec2(
				(ndc.x * 0.5f + 0.5f) * drawable_size.x,
				(ndc.y * 0.5f + 0.5f) * drawable_size.y
			);
			
			if (player.id == 1)
			{
				p1Text.Render_Text(
					screen_pos.x - 20.0f,
					screen_pos.y - 100.0f,
					glm::vec2(drawable_size),
					glm::vec3(0.2f, 0.6f, 1.0f)
				);
			}
			else
			{
				p2Text.Render_Text(
					screen_pos.x - 20.0f,
					screen_pos.y - 100.0f,
					glm::vec2(drawable_size),
					glm::vec3(1.0f, 0.4f, 0.4f)
				);
			}
		}

		if (game.hasWin)
		{
			endText.Render_Text(
				(drawable_size.x / 2) - 300.0f,
				(drawable_size.y / 2) - 64.0f,
				glm::vec2(drawable_size),
				glm::vec3(0.0f, 0.0f, 0.0f)
			);
		}
	}
	
	GL_ERRORS();
}