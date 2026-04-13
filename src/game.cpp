#include "game.hpp"
#include "imgui/imgui.h"

//----------------------------------------------------------------------------//

#define CAMERA_FOV 45.0f
#define CAMERA_MAX_DIST 8000.0f
#define CAMERA_MIN_TILT 15.0f
#define CAMERA_MAX_TILT 89.0f
#define CAMERA_MAX_POSITION 7000.0f

//----------------------------------------------------------------------------//

bool _game_camera_init(GameCamera* cam);
void _game_camera_update(GameCamera* cam, f32 dt, GLFWwindow* window);
void _game_camera_cursor_moved(GameCamera* cam, f32 x, f32 y);
void _game_camera_scroll(GameCamera* cam, f32 amt);

//----------------------------------------------------------------------------//

void _game_cursor_pos_callback(GLFWwindow* window, f64 x, f64 y);
void _game_key_callback(GLFWwindow* window, int32 key, int32 scancode, int32 action, int32 mods);
void _game_char_callback(GLFWwindow* window, uint32 codepoint);
void _game_scroll_callback(GLFWwindow* window, f64 x, f64 y);

//----------------------------------------------------------------------------//

template<typename T>
void _game_decay_to(T& value, T target, f32 rate, f32 dt);

//----------------------------------------------------------------------------//

static void _game_message_log(const char* message, const char* file, int32 line);
#define MSG_LOG(m) _game_message_log(m, __FILENAME__, __LINE__)

static void _game_error_log(const char* message, const char* file, int32 line);
#define ERROR_LOG(m) _game_error_log(m, __FILENAME__, __LINE__)

//----------------------------------------------------------------------------//

bool game_init(GameState** state)
{
	*state = (GameState*)malloc(sizeof(GameState));
	GameState* s = *state;

	if(!s)
	{
		ERROR_LOG("failed to allocate GameState struct");
		return false;
	}

	if(!draw_init(&s->drawState))
	{
		ERROR_LOG("failed to intialize rendering");
		return false;
	}

	if(!_game_camera_init(&s->cam))
	{
		ERROR_LOG("failed to initialize camera");
		return false;
	}

	glfwSetWindowUserPointer(s->drawState->instance->window, s);
	glfwSetCursorPosCallback(s->drawState->instance->window, _game_cursor_pos_callback);
	glfwSetKeyCallback(s->drawState->instance->window, _game_key_callback);
	glfwSetCharCallback(s->drawState->instance->window, _game_char_callback);
	glfwSetScrollCallback(s->drawState->instance->window, _game_scroll_callback);

	return true;
}

void game_quit(GameState* s)
{
	draw_quit(s->drawState);
	free(s);
}

//----------------------------------------------------------------------------//

void game_main_loop(GameState* s)
{
	f32 lastTime = (f32)glfwGetTime();

	f32 accumTime = 0.0f;
	uint32 accumFrames = 0;

	while(!glfwWindowShouldClose(s->drawState->instance->window))
	{
		f32 curTime = (f32)glfwGetTime();
		f32 dt = curTime - lastTime;
		lastTime = curTime;

		accumTime += dt;
		accumFrames++;
		if(accumTime >= 1.0f)
		{
			float avgDt = accumTime / accumFrames;

			char windowName[64];
			snprintf(windowName, sizeof(windowName), "VkGalaxy [FPS: %.0f (%.2fms)]", 1.0f / avgDt, avgDt * 1000.0f);
			glfwSetWindowTitle(s->drawState->instance->window, windowName);

			accumTime -= 1.0f;
			accumFrames = 0;
		}

		draw_imgui_begin_frame(s->drawState, dt);
		ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(420.0f, 560.0f), ImGuiCond_FirstUseEver);
		if(ImGui::Begin("Galaxy Particle Controls"))
		{
			ImGui::Text("FPS %.1f", ImGui::GetIO().Framerate);
			ImGui::Text("Camera dist %.2f", s->cam.dist);
			ImGui::Separator();

			bool regenerateParticles = false;
			int numStars = (int)s->drawState->particleGenParams.numStars;
			if(ImGui::SliderInt("numStars", &numStars, 1, DRAW_NUM_PARTICLES))
			{
				s->drawState->particleGenParams.numStars = (uint32)numStars;
				s->drawState->particleVertParams.numStars = (uint32)numStars;
				regenerateParticles = true;
			}

			if(ImGui::CollapsingHeader("ParticleGenParamsGPU", ImGuiTreeNodeFlags_DefaultOpen))
			{
				regenerateParticles |= ImGui::SliderFloat("maxRad", &s->drawState->particleGenParams.maxRad, 100.0f, 6000.0f);
				regenerateParticles |= ImGui::SliderFloat("bulgeRad", &s->drawState->particleGenParams.bulgeRad, 1.0f, 4000.0f);
				regenerateParticles |= ImGui::SliderFloat("angleOffset", &s->drawState->particleGenParams.angleOffset, 0.0f, 12.56f);
				regenerateParticles |= ImGui::SliderFloat("eccentricity", &s->drawState->particleGenParams.eccentricity, 0.05f, 2.0f);
				regenerateParticles |= ImGui::SliderFloat("baseHeight", &s->drawState->particleGenParams.baseHeight, -1000.0f, 1000.0f);
				regenerateParticles |= ImGui::SliderFloat("height", &s->drawState->particleGenParams.height, 0.0f, 1200.0f);
				regenerateParticles |= ImGui::SliderFloat("minTemp", &s->drawState->particleGenParams.minTemp, 1000.0f, 12000.0f);
				regenerateParticles |= ImGui::SliderFloat("maxTemp", &s->drawState->particleGenParams.maxTemp, 1000.0f, 15000.0f);
				regenerateParticles |= ImGui::SliderFloat("dustBaseTemp", &s->drawState->particleGenParams.dustBaseTemp, 500.0f, 12000.0f);
				regenerateParticles |= ImGui::SliderFloat("minStarOpacity", &s->drawState->particleGenParams.minStarOpacity, 0.0f, 1.0f);
				regenerateParticles |= ImGui::SliderFloat("maxStarOpacity", &s->drawState->particleGenParams.maxStarOpacity, 0.0f, 1.0f);
				regenerateParticles |= ImGui::SliderFloat("minDustOpacity", &s->drawState->particleGenParams.minDustOpacity, 0.0f, 0.1f);
				regenerateParticles |= ImGui::SliderFloat("maxDustOpacity", &s->drawState->particleGenParams.maxDustOpacity, 0.0f, 0.2f);
				regenerateParticles |= ImGui::SliderFloat("speed", &s->drawState->particleGenParams.speed, 0.01f, 60.0f);
			}

			if(ImGui::CollapsingHeader("ParticleParamsVertGPU", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("pause time", &s->drawState->particleTimePaused);
				if(s->drawState->particleTimePaused)
					ImGui::SliderFloat("time", &s->drawState->particleVertParams.time, 0.0f, 120.0f);
				else
					ImGui::Text("time %.2f", s->drawState->particleVertParams.time);

				ImGui::Text("numStars mirrors generator: %u", s->drawState->particleVertParams.numStars);
				ImGui::SliderFloat("starSize", &s->drawState->particleVertParams.starSize, 0.1f, 100.0f);
				ImGui::SliderFloat("dustSize", &s->drawState->particleVertParams.dustSize, 1.0f, 2000.0f);
				ImGui::SliderFloat("h2Size", &s->drawState->particleVertParams.h2Size, 1.0f, 1000.0f);
				ImGui::SliderFloat("h2Dist", &s->drawState->particleVertParams.h2Dist, 0.0f, 2000.0f);
			}

			if(ImGui::Button("Regenerate Particles"))
				regenerateParticles = true;

			if(regenerateParticles)
			{
				if(s->drawState->particleGenParams.bulgeRad > s->drawState->particleGenParams.maxRad)
					s->drawState->particleGenParams.bulgeRad = s->drawState->particleGenParams.maxRad;
				if(s->drawState->particleGenParams.maxTemp < s->drawState->particleGenParams.minTemp)
					s->drawState->particleGenParams.maxTemp = s->drawState->particleGenParams.minTemp;
				if(s->drawState->particleGenParams.maxStarOpacity < s->drawState->particleGenParams.minStarOpacity)
					s->drawState->particleGenParams.maxStarOpacity = s->drawState->particleGenParams.minStarOpacity;
				if(s->drawState->particleGenParams.maxDustOpacity < s->drawState->particleGenParams.minDustOpacity)
					s->drawState->particleGenParams.maxDustOpacity = s->drawState->particleGenParams.minDustOpacity;
				s->drawState->particleGenerationDirty = true;
			}
		}
		ImGui::End();

		_game_camera_update(&s->cam, dt, s->drawState->instance->window);

		DrawParams drawParams;
		drawParams.cam.pos = s->cam.pos;
		drawParams.cam.up = s->cam.up;
		drawParams.cam.target = s->cam.center;
		drawParams.cam.dist = s->cam.dist;
		drawParams.cam.fov = CAMERA_FOV;
		draw_render(s->drawState, &drawParams, dt);
	
		glfwPollEvents();
	}
}

//----------------------------------------------------------------------------//

bool _game_camera_init(GameCamera* cam)
{
	if(!cam)
	{
		ERROR_LOG("failed to allocate GameCamera struct");
		return false;
	}

	cam->up = {0.0f, 1.0f, 0.0f};
	cam->center = cam->targetCenter = {0.0f, 0.0f, 0.0f};

	cam->dist = cam->targetDist = CAMERA_MAX_DIST;
	cam->angle = cam->targetAngle = 45.0f;
	cam->tilt = cam->targetTilt = 45.0f;

	return true;
}

void _game_camera_update(GameCamera* cam, f32 dt, GLFWwindow* window)
{
	const ImGuiIO& io = ImGui::GetIO();
	if(io.WantCaptureKeyboard)
		return;

	f32 camSpeed = 1.0f * dt * cam->dist;
	f32 angleSpeed = 45.0f * dt;
	f32 tiltSpeed = 30.0f * dt;

	qm::vec4 forward4 = qm::rotate(cam->up, cam->angle) * qm::vec4(0.0f, 0.0f, -1.0f, 1.0f);
	qm::vec4 side4 = qm::rotate(cam->up, cam->angle) * qm::vec4(1.0f, 0.0f, 0.0f, 1.0f);

	qm::vec3 forward = qm::vec3(forward4.x, forward4.y, forward4.z);
	qm::vec3 side = qm::vec3(side4.x, side4.y, side4.z);

	qm::vec3 camVel(0.0f, 0.0f, 0.0f);
	if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camVel = camVel + forward;
	if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camVel = camVel - forward;
	if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camVel = camVel + side;
	if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camVel = camVel - side;

	if(glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
		cam->targetAngle -= angleSpeed;
	if(glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
		cam->targetAngle += angleSpeed;

	if(glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS)
		cam->targetTilt = fminf(cam->targetTilt + tiltSpeed, CAMERA_MAX_TILT);
	if(glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
		cam->targetTilt = fmaxf(cam->targetTilt - tiltSpeed, CAMERA_MIN_TILT);

	cam->targetCenter = cam->targetCenter + camSpeed * qm::normalize(camVel);
	if(qm::length(cam->targetCenter) > CAMERA_MAX_POSITION)
		cam->targetCenter = qm::normalize(cam->targetCenter) * CAMERA_MAX_POSITION;

	_game_decay_to(cam->center, cam->targetCenter, 0.985f, dt);
	_game_decay_to(cam->dist  , cam->targetDist  , 0.99f , dt);
	_game_decay_to(cam->angle , cam->targetAngle , 0.99f , dt);
	_game_decay_to(cam->tilt  , cam->targetTilt  , 0.99f , dt);

	qm::vec4 toPos = qm::rotate(side, -cam->tilt) * qm::vec4(forward, 1.0f);
	cam->pos = cam->center - cam->dist * qm::normalize(qm::vec3(toPos.x, toPos.y, toPos.z));
}

void _game_camera_cursor_moved(GameCamera* cam, f32 x, f32 y)
{

}

void _game_camera_scroll(GameCamera* cam, f32 amt)
{
	cam->targetDist -= 0.1f * cam->targetDist * amt;
	cam->targetDist = roundf(cam->targetDist * 100.0f) / 100.0f;

	if(cam->targetDist < 1.0f)
		cam->targetDist = 1.0f;
	
	if(cam->targetDist > CAMERA_MAX_DIST)
		cam->targetDist = CAMERA_MAX_DIST;
}

//----------------------------------------------------------------------------//

void _game_cursor_pos_callback(GLFWwindow* window, f64 x, f64 y)
{
	GameState* s = (GameState*)glfwGetWindowUserPointer(window);

	_game_camera_cursor_moved(&s->cam, (f32)x, (f32)y);
}

void _game_key_callback(GLFWwindow* window, int32 key, int32 scancode, int32 action, int32 mods)
{
	ImGuiIO& io = ImGui::GetIO();
	if(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown))
		io.KeysDown[key] = action != GLFW_RELEASE;
	io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
	io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
	io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
	io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];

	if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void _game_char_callback(GLFWwindow* window, uint32 codepoint)
{
	ImGui::GetIO().AddInputCharacter((unsigned short)codepoint);
}

void _game_scroll_callback(GLFWwindow* window, f64 x, f64 y)
{
	GameState* s = (GameState*)glfwGetWindowUserPointer(window);
	draw_imgui_add_scroll(s->drawState, (f32)x, (f32)y);

	if(y != 0.0 && !ImGui::GetIO().WantCaptureMouse)
		_game_camera_scroll(&s->cam, (f32)y);
}

//----------------------------------------------------------------------------//

template<typename T>
void _game_decay_to(T& value, T target, f32 rate, f32 dt)
{
	value = value + (target - value) * (1.0f - powf(rate, 1000.0f * dt));
}

//----------------------------------------------------------------------------//

static void _game_message_log(const char* message, const char* file, int32 line)
{
	printf("GAME MESSAGE in %s at line %i - \"%s\"\n\n", file, line, message);
}

static void _game_error_log(const char* message, const char* file, int32 line)
{
	printf("GAME ERROR in %s at line %i - \"%s\"\n\n", file, line, message);
}
