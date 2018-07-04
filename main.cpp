#include <Windows.h>

#include <fstream>
#include <string>

#include "MinHook.h"
#include <skse64_common/Relocation.h>

#include "vec3.h"
#include "actor.h"

namespace cfg
{
	float Forward_Speed = 5.1f;
	float Falling_Speed = 1.5f;
	float LiftUp_Speed = 10.0f;
	float LiftDown_Speed = 1.0f;
};

struct move_params {
	float delta_time;
private:
	char padding[0xC]; //0x0C
public:
	vec3 input;
	float max_speed;
};

//const float *frame_delta = (float*)(0x1B4ADE0); //0x142F92948
RelocPtr<float> frameDelta(0x2F92948);
RelocPtr<float> fJumpHeightMinAddr(0x01E08320);
float fJumpHeightMin = 76;
bool enable_physics = true;
vec3 velocity;

bool isFlyingActive;
bool isFlyingUp;
bool isFlyingDown;

using move_t = void(__fastcall*)(actor::physics_data*, move_params*);
RelocPtr<move_t> origin_move_function(0xDBF690);
LPVOID orig_move;
//std::shared_ptr<RenHook::Hook> orig_move;

void __fastcall hook_move(actor::physics_data *phys_data, move_params *params) {
	if (!enable_physics)
		return static_cast<move_t>(orig_move)(phys_data, params);

	auto *player = actor::player();
	if (player == nullptr || player->phys_data() != phys_data)
		return static_cast<move_t>(orig_move)(phys_data, params);

	// Read Skyrim engine variable "fJumpHeightMin"
	//memcpy(&fJumpHeightMin, (void *)0x01E08320, 4); // TODO
	fJumpHeightMin = *fJumpHeightMinAddr.GetPtr();

	isFlyingActive = isFlyingUp = isFlyingDown = false;

	if ((fJumpHeightMin > 349.0f) && (fJumpHeightMin < 451.0f))	isFlyingActive = true;
	if ((fJumpHeightMin > 449.0f) && (fJumpHeightMin < 451.0f))	isFlyingUp = true;
	if ((fJumpHeightMin > 349.0f) && (fJumpHeightMin < 351.0f))	isFlyingDown = true;

	// If Character is Falling but HAS wings:
	if ((phys_data->velocity().z < 0) && (isFlyingActive == true))
	{
		velocity.y = velocity.y * cfg::Forward_Speed;
		velocity.x = velocity.x * cfg::Forward_Speed;
		params->input.x = params->input.x * cfg::Forward_Speed;
		params->input.y = params->input.y * cfg::Forward_Speed;

		velocity.z = phys_data->velocity().z / cfg::Falling_Speed;

		if (isFlyingUp == true)
		{
			//params->input.x = params->input.x * cfg::Forward_Speed / 2.0f;
			//params->input.y = params->input.y * cfg::Forward_Speed / 2.0f;
			velocity.z = velocity.z + cfg::LiftUp_Speed;
		}
		if (isFlyingDown == true) velocity.z = velocity.z - cfg::LiftDown_Speed;

		phys_data->set_velocity(velocity);
	}

	return static_cast<move_t>(orig_move)(phys_data, params);
}

using change_cam_t = void(__fastcall*)(uintptr_t, uintptr_t);
RelocAddr<change_cam_t> origin_change_camera_function(0x4F6110);
//std::shared_ptr<RenHook::Hook> orig_change_camera;
LPVOID orig_change_camera;

void __fastcall hook_change_camera(uintptr_t camera, uintptr_t new_state)
{
	const auto cam_id = *(int*)(new_state + 0x18);

	enable_physics = cam_id != 2;
	static_cast<change_cam_t>(orig_change_camera)(camera, new_state);
}

using load_game_t = bool(__fastcall*)(void*, void*, bool);
RelocAddr<load_game_t> origin_load_game_function(0x57D650);
LPVOID orig_load_game;

bool __fastcall hook_load_game(void *thisptr, void *a1, bool a2)
{
	velocity = vec3();
	return static_cast<load_game_t>(orig_load_game)(thisptr, a1, a2);
}

struct PluginInfo {
	unsigned int infoVersion;
	const char *name;
	unsigned int version;
};

void read_cfg()
{
	std::ifstream config("Data/SKSE/Plugins/RealFlyingPlugin.cfg");
	if (config.fail())
		return;

	while (!config.eof()) {
		std::string line;
		std::getline(config, line);

		char key[32];
		float value;
		if (sscanf_s(line.c_str(), "%s %f", key, 32, &value) != 2)
			continue;

		if (strncmp(key, "//", 2) == 0)
			continue;

		else if (strcmp(key, "Forward_Speed") == 0)
			cfg::Forward_Speed = value;
		else if (strcmp(key, "Falling_Speed") == 0)
			cfg::Falling_Speed = value;
		else if (strcmp(key, "LiftUp_Speed") == 0)
			cfg::LiftUp_Speed = value;
		else if (strcmp(key, "LiftDown_Speed") == 0)
			cfg::LiftDown_Speed = value;

	}
}

extern "C" __declspec(dllexport) bool SKSEPlugin_Query(void *skse, PluginInfo *info)
{
	info->infoVersion = 3;
	info->version = 3;
	info->name = "Real Flying Plugin";

	return true;
}

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(void *skse)
{
	read_cfg();
	if (MH_Initialize() != MH_OK) {
		return false;
	}
	
	if (MH_CreateHook(origin_move_function.GetPtr(), hook_move, &orig_move) != MH_OK) {
		return false;
	}

	if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
		return false;
	}
	/*
	if (MH_CreateHook() != MH_OK) {
		return false;
	}
	*/

	//orig_move = RenHook::Hook::Create(origin_move_function.GetUIntPtr(), hook_move);
	//orig_change_camera = RenHook::Hook::Create(origin_change_camera_function.GetUIntPtr(), hook_change_camera);
	//orig_load_game = RenHook::Hook::Create(origin_load_game_function.GetUIntPtr(), hook_load_game);
	return true;
}