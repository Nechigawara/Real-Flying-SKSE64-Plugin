#pragma once

#include "vec3.h"
#include "skse64_common/Utilities.h"
#include "address.h"

struct actor
{
	struct physics_data
	{
		vec3 velocity()
		{
			const auto container = *(uintptr_t*)((uintptr_t)(this) + 0x340);  //0x340
			return *(vec3*)(container + 0x60); //0x60
		}

		void set_velocity(vec3 vel)
		{
			const auto container = *(uintptr_t*)((uintptr_t)(this) + 0x340);
			*(vec3*)(container + 0x60) = vel;
		}

		vec3 ground_normal()
		{
			return *(vec3*)((uintptr_t)(this) + 0x1B0); //0x1B0
		}

		void *ground_entity()
		{
			return *(void**)((uintptr_t)(this) + 0x2B0); //0x2B0
		}

		float fall_time()
		{
			return *(float*)((uintptr_t)(this) + 0x244); //~0x1E4+0x60=0x244
		}
	};

	physics_data *phys_data()
	{
		const auto level1 = *(uintptr_t*)((uintptr_t)(this) + 0xF0); //0xF0
		if (level1 == 0)
			return nullptr;

		const auto level2 = *(uintptr_t*)((uintptr_t)(level1)+0x08); //0x08
		if (level2 == 0)
			return nullptr;

		return *(physics_data**)((uintptr_t)(level2)+0x250); //0x250
	}

	vec3 position()
	{
		return *(vec3*)((uintptr_t)(this) + 0x54); //0x54
	}

	float yaw()
	{
		return *(float*)((uintptr_t)(this) + 0x50); //0x50
	}

	static actor *player()
	{
		static RelocPtr<actor*> _player(static_cast<uintptr_t>(OFF_PLAYER));
		return *_player.GetPtr();
	}
};
