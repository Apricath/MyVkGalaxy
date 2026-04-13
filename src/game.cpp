#include "game.hpp"
#include "imgui/imgui.h"

//----------------------------------------------------------------------------//

#define CAMERA_FOV 45.0f
#define CAMERA_MAX_DIST 48000.0f
#define CAMERA_MIN_TILT 15.0f
#define CAMERA_MAX_TILT 89.0f
#define CAMERA_MAX_POSITION 7000.0f

//----------------------------------------------------------------------------//

bool _game_camera_init(GameCamera* cam);
void _game_camera_update(GameCamera* cam, f32 dt, GLFWwindow* window);
void _game_camera_cursor_moved(GameCamera* cam, f32 x, f32 y);
void _game_camera_scroll(GameCamera* cam, f32 amt);
void _game_apply_preset_camera(GameState* s);
void _game_apply_selected_preset(GameState* s);
void _game_start_preset_showcase(GameState* s);
void _game_update_preset_showcase(GameState* s, f32 dt);

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

	s->selectedPreset = 0;
	s->presetShowcaseActive = false;
	s->presetShowcaseTimer = 0.0f;
	s->presetShowcaseIndex = 0u;
	_game_apply_selected_preset(s);
	_game_apply_preset_camera(s);

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

		_game_update_preset_showcase(s, dt);

		draw_imgui_begin_frame(s->drawState, dt);
		ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(420.0f, 760.0f), ImGuiCond_FirstUseEver);
		if(ImGui::Begin("Galaxy Particle Controls"))
		{
			ImGui::Text("FPS %.1f", ImGui::GetIO().Framerate);
			ImGui::Text("Camera dist %.2f", s->cam.dist);
			ImGui::Text("Active particles %u / %u", draw_get_active_particle_count(s->drawState), DRAW_NUM_PARTICLES);
			ImGui::Separator();

			bool regenerateParticles = false;

			const char* presetPreview = draw_get_galaxy_preset_name((uint32)s->selectedPreset);
			if(ImGui::BeginCombo("Galaxy preset", presetPreview))
			{
				for(uint32 presetIdx = 0; presetIdx < draw_get_galaxy_preset_count(); ++presetIdx)
				{
					bool selected = s->selectedPreset == (int32)presetIdx;
					if(ImGui::Selectable(draw_get_galaxy_preset_name(presetIdx), selected))
					{
						s->presetShowcaseActive = false;
						s->selectedPreset = (int32)presetIdx;
						_game_apply_selected_preset(s);
						_game_apply_preset_camera(s);
						regenerateParticles = true;
					}
					if(selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			if(s->presetShowcaseActive)
				ImGui::Text("Preset showcase %u / %u, next switch in %.1fs", s->presetShowcaseIndex + 1u, draw_get_galaxy_preset_count(), 6.0f - s->presetShowcaseTimer);

			if(ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen))
			{
				bool hasDarkMatter = s->drawState->particleGenParams.hasDarkMatter != 0u;
				if(ImGui::Checkbox("dark matter", &hasDarkMatter))
				{
					s->drawState->particleGenParams.hasDarkMatter = hasDarkMatter ? 1u : 0u;
					regenerateParticles = true;
				}

				ImGui::Checkbox("pause time", &s->drawState->particleTimePaused);
				if(s->drawState->particleTimePaused)
					ImGui::SliderFloat("time (years)", &s->drawState->particleVertParams.time, 0.0f, 200000000.0f, "%.0f");
				else
					ImGui::Text("time %.0f years", s->drawState->particleVertParams.time);

				ImGui::SliderFloat("time step length (years)", &s->drawState->particleVertParams.timeStepYears, 1000.0f, 10000.0f, "%.0f");
				regenerateParticles |= ImGui::SliderFloat("basic temperature (K)", &s->drawState->particleGenParams.dustBaseTemp, 1000.0f, 8000.0f, "%.0f");
			}

			if(ImGui::CollapsingHeader("Density Waves", ImGuiTreeNodeFlags_DefaultOpen))
			{
				regenerateParticles |= ImGui::SliderFloat("galaxy core radius", &s->drawState->particleGenParams.coreRad, 100.0f, 16000.0f, "%.0f");
				regenerateParticles |= ImGui::SliderFloat("galaxy radius", &s->drawState->particleGenParams.galaxyRad, 1000.0f, 24000.0f, "%.0f");
				regenerateParticles |= ImGui::SliderFloat("angular offset", &s->drawState->particleGenParams.angleOffset, 0.00005f, 0.00150f, "%.5f");
				regenerateParticles |= ImGui::SliderFloat("inner excentricity", &s->drawState->particleGenParams.exInner, 0.50f, 1.60f, "%.2f");
				regenerateParticles |= ImGui::SliderFloat("outer excentricity", &s->drawState->particleGenParams.exOuter, 0.50f, 1.20f, "%.2f");
				int perturbationCount = (int)s->drawState->particleVertParams.perturbationCount;
				if(ImGui::SliderInt("ellipse disturbances", &perturbationCount, 0, 6))
					s->drawState->particleVertParams.perturbationCount = (uint32)perturbationCount;
				ImGui::SliderFloat("disturbance damping", &s->drawState->particleVertParams.perturbationDamping, 1.0f, 100.0f, "%.0f");
			}

			if(ImGui::CollapsingHeader("Particles", ImGuiTreeNodeFlags_DefaultOpen))
			{
				int numStars = (int)s->drawState->particleGenParams.numStars;
				if(ImGui::SliderInt("stars##count", &numStars, 1, DRAW_NUM_PARTICLES))
				{
					s->drawState->particleGenParams.numStars = (uint32)numStars;
					regenerateParticles = true;
				}

				int numDust = (int)s->drawState->particleGenParams.numDust;
				if(ImGui::SliderInt("dust##count", &numDust, 0, DRAW_NUM_PARTICLES))
				{
					s->drawState->particleGenParams.numDust = (uint32)numDust;
					regenerateParticles = true;
				}

				int numFilaments = (int)s->drawState->particleGenParams.numFilaments;
				if(ImGui::SliderInt("filaments##count", &numFilaments, 0, DRAW_NUM_PARTICLES))
				{
					s->drawState->particleGenParams.numFilaments = (uint32)numFilaments;
					regenerateParticles = true;
				}

				int numH2Regions = (int)s->drawState->particleGenParams.numH2Regions;
				if(ImGui::SliderInt("H2 regions##count", &numH2Regions, 0, DRAW_NUM_PARTICLES / 2))
				{
					s->drawState->particleGenParams.numH2Regions = (uint32)numH2Regions;
					regenerateParticles = true;
				}

				regenerateParticles |= ImGui::SliderFloat("baseHeight", &s->drawState->particleGenParams.baseHeight, -1000.0f, 1000.0f);
				regenerateParticles |= ImGui::SliderFloat("height", &s->drawState->particleGenParams.height, 0.0f, 1200.0f);
				regenerateParticles |= ImGui::SliderFloat("minStarOpacity", &s->drawState->particleGenParams.minStarOpacity, 0.0f, 1.0f);
				regenerateParticles |= ImGui::SliderFloat("maxStarOpacity", &s->drawState->particleGenParams.maxStarOpacity, 0.0f, 1.0f);
				regenerateParticles |= ImGui::SliderFloat("minDustOpacity", &s->drawState->particleGenParams.minDustOpacity, 0.0f, 0.1f);
				regenerateParticles |= ImGui::SliderFloat("maxDustOpacity", &s->drawState->particleGenParams.maxDustOpacity, 0.0f, 0.2f);
			}

			if(ImGui::CollapsingHeader("Display", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("axis", &s->drawState->showGrid);

				bool showStars = (s->drawState->particleVertParams.displayFlags & PARTICLE_DISPLAY_STARS) != 0u;
				if(ImGui::Checkbox("stars##display", &showStars))
				{
					if(showStars) s->drawState->particleVertParams.displayFlags |= PARTICLE_DISPLAY_STARS;
					else s->drawState->particleVertParams.displayFlags &= ~PARTICLE_DISPLAY_STARS;
				}

				bool showDust = (s->drawState->particleVertParams.displayFlags & PARTICLE_DISPLAY_DUST) != 0u;
				if(ImGui::Checkbox("dust##display", &showDust))
				{
					if(showDust) s->drawState->particleVertParams.displayFlags |= PARTICLE_DISPLAY_DUST;
					else s->drawState->particleVertParams.displayFlags &= ~PARTICLE_DISPLAY_DUST;
				}

				bool showFilaments = (s->drawState->particleVertParams.displayFlags & PARTICLE_DISPLAY_FILAMENTS) != 0u;
				if(ImGui::Checkbox("dust filaments##display", &showFilaments))
				{
					if(showFilaments) s->drawState->particleVertParams.displayFlags |= PARTICLE_DISPLAY_FILAMENTS;
					else s->drawState->particleVertParams.displayFlags &= ~PARTICLE_DISPLAY_FILAMENTS;
				}

				bool showH2 = (s->drawState->particleVertParams.displayFlags & PARTICLE_DISPLAY_H2) != 0u;
				if(ImGui::Checkbox("H2 regions##display", &showH2))
				{
					if(showH2) s->drawState->particleVertParams.displayFlags |= PARTICLE_DISPLAY_H2;
					else s->drawState->particleVertParams.displayFlags &= ~PARTICLE_DISPLAY_H2;
				}

				ImGui::SliderFloat("star size", &s->drawState->particleVertParams.starSize, 0.2f, 8.0f, "%.2f");
				ImGui::SliderFloat("dust render size", &s->drawState->particleVertParams.dustSize, 2.0f, 60.0f, "%.1f");
				ImGui::SliderFloat("H2 size", &s->drawState->particleVertParams.h2Size, 2.0f, 60.0f, "%.1f");
				ImGui::SliderFloat("H2 distance check", &s->drawState->particleVertParams.h2Dist, 10.0f, 2000.0f);
			}

			if(ImGui::Button("Regenerate Particles"))
				regenerateParticles = true;

			if(regenerateParticles)
			{
				if(s->drawState->particleGenParams.coreRad > s->drawState->particleGenParams.galaxyRad)
					s->drawState->particleGenParams.coreRad = s->drawState->particleGenParams.galaxyRad;
				s->drawState->particleGenParams.farFieldRad = s->drawState->particleGenParams.galaxyRad * 2.0f;
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
	cam->angle = cam->targetAngle = 0.0f;
	cam->tilt = cam->targetTilt = 45.0f;

	return true;
}

void _game_apply_preset_camera(GameState* s)
{
	if(!s || !s->drawState)
		return;

	s->cam.targetCenter = {0.0f, 0.0f, 0.0f};
	s->cam.center = s->cam.targetCenter;
	s->cam.targetAngle = 0.0f;
	s->cam.angle = s->cam.targetAngle;
	s->cam.targetTilt = 45.0f;
	s->cam.tilt = s->cam.targetTilt;

	s->cam.targetDist = CAMERA_MAX_DIST;
	s->cam.dist = CAMERA_MAX_DIST;
}

void _game_apply_selected_preset(GameState* s)
{
	if(!s || !s->drawState || s->selectedPreset < 0)
		return;

	draw_apply_galaxy_preset(s->drawState, (uint32)s->selectedPreset);
	s->drawState->particleGenerationDirty = true;
}

void _game_start_preset_showcase(GameState* s)
{
	if(!s)
		return;

	s->presetShowcaseActive = true;
	s->presetShowcaseTimer = 0.0f;
	s->presetShowcaseIndex = 0u;
	s->selectedPreset = 0;
	_game_apply_selected_preset(s);
	_game_apply_preset_camera(s);
}

void _game_update_preset_showcase(GameState* s, f32 dt)
{
	if(!s || !s->presetShowcaseActive)
		return;

	const uint32 presetCount = draw_get_galaxy_preset_count();
	if(presetCount == 0u)
	{
		s->presetShowcaseActive = false;
		return;
	}

	s->presetShowcaseTimer += dt;
	if(s->presetShowcaseTimer < 6.0f)
		return;

	s->presetShowcaseTimer -= 6.0f;

	if(s->presetShowcaseIndex + 1u >= presetCount)
	{
		s->presetShowcaseActive = false;
		s->presetShowcaseIndex = presetCount - 1u;
		s->selectedPreset = (int32)s->presetShowcaseIndex;
		return;
	}

	s->presetShowcaseIndex += 1u;
	s->selectedPreset = (int32)s->presetShowcaseIndex;
	_game_apply_selected_preset(s);
	_game_apply_preset_camera(s);
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
	GameState* s = (GameState*)glfwGetWindowUserPointer(window);
	ImGuiIO& io = ImGui::GetIO();
	if(key >= 0 && key < IM_ARRAYSIZE(io.KeysDown))
		io.KeysDown[key] = action != GLFW_RELEASE;
	io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
	io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
	io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
	io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];

	if(action == GLFW_PRESS && !io.WantCaptureKeyboard)
	{
		if(key == GLFW_KEY_M)
			_game_start_preset_showcase(s);
	}

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
