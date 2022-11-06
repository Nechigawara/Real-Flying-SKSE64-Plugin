//==================================================================================================
// This is AE VERSION
//==================================================================================================


#include "common/IDebugLog.h"  // IDebugLog
#include "skse64_common/skse_version.h"  // RUNTIME_VERSION
#include "skse64_common/Relocation.h"
#include "skse64/PluginAPI.h"  // SKSEInterface, PluginInfo

#include <ShlObj.h>  // CSIDL_MYDOCUMENTS

#include <string>
#include <vector>

#include "address.h"
#include "vec3.h"
#include "actor.h"
#include "versionlibdb.h"
#include "version.h"  // VERSION_VERSTRING, VERSION_MAJOR

#include "MinHook.h"

namespace cfg{
	float ForwardSpeedMulti = 5.1f;
	float FallingSpeedMulti = 2. / 3;
	float LiftUpSpeedMulti = 10.0f;
	float LiftDownSpeedMulti = 1.0f;
};

struct move_params {
	float delta_time;
private:
	char padding[0x0C]; //0x0C
public:
	vec3 input;
	float max_speed;
};

static PluginHandle g_pluginHandle = kPluginHandle_Invalid;

VersionDb db;
unsigned long long loc_OFF_FRAME_DELTA = 0;
unsigned long long loc_OFF_FJUMPHEIGHTMIN = 0;
unsigned long long loc_OFF_PLAYER = 0;
unsigned long long loc_OFF_MOVE = 0;
unsigned long long loc_OFF_SET_CAMERA = 0;
unsigned long long loc_OFF_LOAD_GAME = 0;

unsigned long long getOffset(unsigned long long ptr)
{
	unsigned long long b_offset = 0;
	db.FindOffsetById(ptr, b_offset);
	return b_offset;
}

bool IsOk()
{
	unsigned long long b_offset = 0;

	// Load Current Version
	db.Load();

	// Index Check
	if (!db.FindOffsetById(OFF_FRAME_DELTA, b_offset))
	{
		_FATALERROR(OFF_FRAME_DELTA + " does not exist in all versions of the database!");
		db.Clear();
		return false;
	}
	if (!db.FindOffsetById(OFF_FJUMPHEIGHTMIN, b_offset))
	{
		_FATALERROR(OFF_FJUMPHEIGHTMIN + " does not exist in all versions of the database!");
		db.Clear();
		return false;
	}
	if (!db.FindOffsetById(OFF_PLAYER, b_offset))
	{
		_FATALERROR(OFF_PLAYER + " does not exist in all versions of the database!");
		db.Clear();
		return false;
	}
	if (!db.FindOffsetById(OFF_MOVE, b_offset))
	{
		_FATALERROR(OFF_MOVE + " does not exist in all versions of the database!");
		db.Clear();
		return false;
	}
	if (!db.FindOffsetById(OFF_SET_CAMERA, b_offset))
	{
		_FATALERROR(OFF_SET_CAMERA + " does not exist in all versions of the database!");
		db.Clear();
		return false;
	}
	if (!db.FindOffsetById(OFF_LOAD_GAME, b_offset))
	{
		_FATALERROR(OFF_LOAD_GAME + " does not exist in all versions of the database!");
		db.Clear();
		return false;
	}

	// Set Address
	loc_OFF_FRAME_DELTA = getOffset(OFF_FRAME_DELTA);
	loc_OFF_FJUMPHEIGHTMIN = getOffset(OFF_FJUMPHEIGHTMIN);
	loc_OFF_PLAYER = getOffset(OFF_PLAYER);
	loc_OFF_MOVE = getOffset(OFF_MOVE);
	loc_OFF_SET_CAMERA = getOffset(OFF_SET_CAMERA);
	loc_OFF_LOAD_GAME = getOffset(OFF_LOAD_GAME);

	// Clear
	db.Clear();

	// Ok!
	return true;
}

bool isThisOK = IsOk();

//const float *frame_delta = (float*)(0x1B4ADE0); //0x142F92948
RelocPtr<float> frameDelta(loc_OFF_FRAME_DELTA);
RelocPtr<float> fJumpHeightMinAddr(loc_OFF_FJUMPHEIGHTMIN);
float fJumpHeightMin = 76;
bool enable_physics = true;
vec3 velocity;

bool isFlyingActive;
bool isFlyingUp;
bool isFlyingDown;

using move_t = void(__fastcall*)(actor::physics_data*, move_params*);
RelocPtr<move_t> origin_move_function(loc_OFF_MOVE);
LPVOID orig_move;
//std::shared_ptr<RenHook::Hook> orig_move;

void __fastcall hook_move(actor::physics_data* phys_data, move_params* params) {
	if (!enable_physics) {
		return static_cast<move_t>(orig_move)(phys_data, params);
	}

	auto* player = actor::player(loc_OFF_PLAYER);

	if (player == nullptr || player->phys_data() != phys_data) {
		return static_cast<move_t>(orig_move)(phys_data, params);
	}

	// Read Skyrim engine variable "fJumpHeightMin"
	//memcpy(&fJumpHeightMin, (void *)0x01E08320, 4); // TODO
	fJumpHeightMin = *fJumpHeightMinAddr.GetPtr();

	isFlyingActive = isFlyingUp = isFlyingDown = false;

	if ((fJumpHeightMin > 349.0f) && (fJumpHeightMin < 451.0f))	isFlyingActive = true;
	if ((fJumpHeightMin > 449.0f) && (fJumpHeightMin < 451.0f))	isFlyingUp = true;
	if ((fJumpHeightMin > 349.0f) && (fJumpHeightMin < 351.0f))	isFlyingDown = true;

	// If Character is Falling but HAS wings:
	if ((phys_data->velocity().z < 0) && (isFlyingActive == true)) {
		velocity.y = velocity.y * cfg::ForwardSpeedMulti;
		velocity.x = velocity.x * cfg::ForwardSpeedMulti;
		params->input.x = params->input.x * cfg::ForwardSpeedMulti;
		params->input.y = params->input.y * cfg::ForwardSpeedMulti;

		velocity.z = phys_data->velocity().z * cfg::FallingSpeedMulti;

		if (isFlyingUp == true) {
			//params->input.x = params->input.x * cfg::Forward_Speed / 2.0f;
			//params->input.y = params->input.y * cfg::Forward_Speed / 2.0f;
			velocity.z = velocity.z + cfg::LiftUpSpeedMulti;
		}

		if (isFlyingDown == true) {
			velocity.z = velocity.z - cfg::LiftDownSpeedMulti;
		}

		phys_data->set_velocity(velocity);
	}

	return static_cast<move_t>(orig_move)(phys_data, params);
}

using change_cam_t = void(__fastcall*)(uintptr_t, uintptr_t);
RelocAddr<change_cam_t> origin_change_camera_function(loc_OFF_SET_CAMERA);
//std::shared_ptr<RenHook::Hook> orig_change_camera;
LPVOID orig_change_camera;

//NOT Actually hooked right now because I dont know if it is neccessary
//or harmful because this function has been hooked by SKSE64
void __fastcall hook_change_camera(uintptr_t camera, uintptr_t new_state) {
	const auto cam_id = *(int*)(new_state + 0x18);
	enable_physics = cam_id != 2;
	static_cast<change_cam_t>(orig_change_camera)(camera, new_state);
}

using load_game_t = bool(__fastcall*)(void*, void*, bool);
RelocAddr<load_game_t> origin_load_game_function(loc_OFF_LOAD_GAME);
LPVOID orig_load_game;

//NOT Actually hooked right now because SKSE64 hooked it too, I think I'll just use the SKSE64 API
bool __fastcall hook_load_game(void* thisptr, void* a1, bool a2) {
	velocity = vec3();
	return static_cast<load_game_t>(orig_load_game)(thisptr, a1, a2);
}

void read_cfg() {
	std::ifstream config("Data/SKSE/Plugins/RealFlyingPlugin.cfg");
	if (config.fail()) {
		return;
	}

	while (!config.eof()) {
		std::string line;
		std::getline(config, line);

		char key[32];
		float value;
		if (sscanf_s(line.c_str(), "%s %f", key, 32, &value) != 2) {
			continue;
		}

		if (strncmp(key, "//", 2) == 0) {
			continue;
		}

		else if (strcmp(key, "ForwardSpeedMulti") == 0) {
			cfg::ForwardSpeedMulti = value;
		}
		else if (strcmp(key, "FallingSpeedMulti") == 0) {
			cfg::FallingSpeedMulti = value;
		}
		else if (strcmp(key, "LiftUpSpeedMulti") == 0) {
			cfg::LiftUpSpeedMulti = value;
		}
		else if (strcmp(key, "LiftDownSpeedMulti") == 0) {
			cfg::LiftDownSpeedMulti = value;
		}
	}
}

// SKSE DLL Build
extern "C" {
	/*__declspec(dllexport) SKSEPluginVersionData SKSEPlugin_Version =
	{
		SKSEPluginVersionData::kVersion,
		REALFLY_VERSION_MAJOR,
		"Realy Flying AE Plugin",
		"Nechigawara",
		"",
		SKSEPluginVersionData::kVersionIndependent_AddressLibraryPostAE,
		0,
		0,
	};*/

	__declspec(dllexport) SKSEPluginVersionData SKSEPlugin_Version =
	{
		SKSEPluginVersionData::kVersion,
		REALFLY_VERSION_MAJOR,
		"Realy Flying AE Plugin (1.6.629+)",
		"Nechigawara",
		"", // I'm not giving you my email address, leave any support requests on github or nexusmods
		SKSEPluginVersionData::kVersionIndependentEx_NoStructUse,
		SKSEPluginVersionData::kVersionIndependent_AddressLibraryPostAE,
		0,
		0,
	};

	// If I'm feel like I want to make Dual SE-AE Plugin
	/*bool SKSEPlugin_Query(const SKSEInterface* a_skse, PluginInfo* a_info)
	{
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim Special Edition\\SKSE\\RealFlyingPlugin.log");
		gLog.SetPrintLevel(IDebugLog::kLevel_DebugMessage);
		gLog.SetLogLevel(IDebugLog::kLevel_DebugMessage);

		_MESSAGE("RealFlyingAE v%s", REALFLY_VERSION_VERSTRING);

		a_info->infoVersion = PluginInfo::kInfoVersion;
		a_info->name = "RealFlyingAE";
		a_info->version = REALFLY_VERSION_MAJOR;

		if (a_skse->isEditor)
		{
			_FATALERROR("[FATAL ERROR] Loaded in editor, marking as incompatible!");
			return false;
		}
		else if (a_skse->runtimeVersion < RUNTIME_VERSION_1_6_317) {
			_FATALERROR("[FATAL ERROR] Unsupported runtime version %08X!\n", a_skse->runtimeVersion);
			return false;
		}

		_MESSAGE("[MESSAGE] Real Flying AE loaded");

		return true;
	}*/

	bool SKSEPlugin_Load(const SKSEInterface* a_skse)
	{
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim Special Edition\\SKSE\\RealFlyingPlugin.log");
		gLog.SetPrintLevel(IDebugLog::kLevel_DebugMessage);
		gLog.SetLogLevel(IDebugLog::kLevel_DebugMessage);

		_MESSAGE("RealFlyingAE v%s", REALFLY_VERSION_VERSTRING);

		if (a_skse->runtimeVersion >= RUNTIME_VERSION_1_6_317) //1_6_317 was the first AE version
		{
			//logging is not set up in SKSEPlugin_Query on AE, so we set it up here
			_MESSAGE("[MESSAGE] Real Flying AE loaded on AE");

		}
		else {
			_FATALERROR("[FATAL ERROR] Unsupported runtime version %08X!\n", a_skse->runtimeVersion);
			return false;
		}

		if (!isThisOK) {
			_MESSAGE("Address Library not found or The Address this mod need not support");
			return false;
		}

		// Read Config
		read_cfg();

		if (MH_CreateHook(origin_move_function.GetPtr(), hook_move, &orig_move) != MH_OK) {
			_ERROR("Hook Creation failed.");
			return false;
		}

		if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) {
			_ERROR("Apply Hook failed.");
			return false;
		}

		return true;
	}

};

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	switch (ul_reason_for_call) {
	case DLL_PROCESS_ATTACH:
		MH_Initialize();
		break;
	case DLL_THREAD_ATTACH:
		break;
	case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		MH_Uninitialize();
		break;
	}
	return TRUE;
}
