#include <std_include.hpp>
#include "loader/component_loader.hpp"

#include "dvars.hpp"

#include "game/game.hpp"
#include "game/dvars.hpp"

#include "dvars.hpp"

#include <utils/nt.hpp>
#include <utils/hook.hpp>
#include <utils/flags.hpp>

namespace gameplay
{
	namespace
	{
		utils::hook::detour stuck_in_client_hook;

		void stuck_in_client_stub(void* entity)
		{
			if (dvars::bg_playerEjection->current.enabled)
			{
				stuck_in_client_hook.invoke<void>(entity);
			}
		}

		void* bg_bounces_stub()
		{
			return utils::hook::assemble([](utils::hook::assembler& a)
			{
				const auto no_bounce = a.newLabel();
				const auto loc_70FB6F = a.newLabel();

				a.push(rax);

				a.mov(rax, qword_ptr(reinterpret_cast<int64_t>(&dvars::bg_bounces)));
				a.mov(al, byte_ptr(rax, 0x10));
				a.cmp(ptr(rbp, -0x66), al);

				a.pop(rax);
				a.jz(no_bounce);
				a.jmp(0x70FBF0_b);

				a.bind(no_bounce);
				a.cmp(ptr(rsp, 0x44), r14d);
				a.jnz(loc_70FB6F);
				a.jmp(0x70FBE1_b);

				a.bind(loc_70FB6F);
				a.jmp(0x70FB6F_b);
			});
		}

		int get_gravity()
		{
			return static_cast<int>(game::BG_GetGravity());
		}

		void* bg_gravity_stub()
		{
			return utils::hook::assemble([](utils::hook::assembler& a)
			{
				// do moveSpeedScaleMultiplier first (xmm0)
				a.call(0xBB3030_b);
				a.mov(ptr(rdi, 0x32C), eax);

				// get bg_gravity as int
				a.pushad64();
				a.push(rdi);
				a.call_aligned(get_gravity);
				a.pop(rdi);
				a.mov(dword_ptr(rdi, 0x78), eax);
				a.popad64();

				a.jmp(0xAFA342_b);
			});
		}

		void* g_speed_stub()
		{
			return utils::hook::assemble([](utils::hook::assembler& a)
			{
				a.push(rax);

				a.mov(rax, qword_ptr(reinterpret_cast<int64_t>(&*reinterpret_cast<game::dvar_t**>(0x3C98330_b))));
				a.mov(eax, dword_ptr(rax, 0x10));

				a.mov(dword_ptr(rdi, 0x7C), eax);

				a.pop(rax);

				// original code
				a.mov(eax, ptr(rdi, 0x1FD4));
				a.add(eax, ptr(rdi, 0x1FD0));

				a.jmp(0xAFB1EC_b);
			});
		}

		void cg_calculate_weapon_movement_debug_stub(game::cg_s* glob, float* origin)
		{
			// Retrieve the hook value
			float value = utils::hook::invoke<float>(0x889B60_b, glob, origin);

			// Initialize values
			float valueX = -6.0f * value;
			float valueY = 0.0f * value;
			float valueZ = 0.0f * value;

			// Apply values
			origin[0] += valueX * glob->viewModelAxis[0][0];
			origin[1] += valueX * glob->viewModelAxis[0][1];
			origin[2] += valueX * glob->viewModelAxis[0][2];

			origin[0] += valueY * glob->viewModelAxis[1][0];
			origin[1] += valueY * glob->viewModelAxis[1][1];
			origin[2] += valueY * glob->viewModelAxis[1][2];

			origin[0] += valueZ * glob->viewModelAxis[2][0];
			origin[1] += valueZ * glob->viewModelAxis[2][1];
			origin[2] += valueZ * glob->viewModelAxis[2][2];

			// Apply dvar values
			origin[0] += dvars::cg_gun_x->current.value * glob->viewModelAxis[0][0];
			origin[1] += dvars::cg_gun_x->current.value * glob->viewModelAxis[0][1];
			origin[2] += dvars::cg_gun_x->current.value * glob->viewModelAxis[0][2];

			origin[0] += dvars::cg_gun_y->current.value * glob->viewModelAxis[1][0];
			origin[1] += dvars::cg_gun_y->current.value * glob->viewModelAxis[1][1];
			origin[2] += dvars::cg_gun_y->current.value * glob->viewModelAxis[1][2];

			origin[0] += dvars::cg_gun_z->current.value * glob->viewModelAxis[2][0];
			origin[1] += dvars::cg_gun_z->current.value * glob->viewModelAxis[2][1];
			origin[2] += dvars::cg_gun_z->current.value * glob->viewModelAxis[2][2];
		}
	}

	class component final : public component_interface
	{
	public:
		void post_unpack() override
		{
			// Implement ejection dvar
			dvars::bg_playerEjection = game::Dvar_RegisterBool("bg_playerEjection", true, game::DVAR_FLAG_REPLICATED, "Flag whether player ejection is on or off");
			stuck_in_client_hook.create(0xAFD9B0_b, stuck_in_client_stub);

			// Implement bounces dvar
			dvars::bg_bounces = game::Dvar_RegisterBool("bg_bounces", false, game::DVAR_FLAG_REPLICATED, "Enables bounces");
			utils::hook::jump(0x70FBB7_b, bg_bounces_stub(), true);

			// Modify gravity dvar
			dvars::override::register_float("bg_gravity", 800, 0, 1000, 0xC0 | game::DVAR_FLAG_REPLICATED);
			utils::hook::nop(0xAFA330_b, 18);
			utils::hook::jump(0xAFA330_b, bg_gravity_stub(), true);

			// Modify speed dvar
			dvars::override::register_int("g_speed", 190, 0x80000000, 0x7FFFFFFF, 0xC0 | game::DVAR_FLAG_REPLICATED);
			utils::hook::nop(0xAFB1DF_b, 13);
			utils::hook::jump(0xAFB1DF_b, g_speed_stub(), true);

			// Implement gun position dvars
			dvars::cg_gun_x = game::Dvar_RegisterFloat("cg_gun_x", 0.0f, -800.0f, 800.0f, game::DvarFlags::DVAR_FLAG_NONE, "Forward position of the viewmodel");
			dvars::cg_gun_y = game::Dvar_RegisterFloat("cg_gun_y", 0.0f, -800.0f, 800.0f, game::DvarFlags::DVAR_FLAG_NONE, "Right position of the viewmodel");
			dvars::cg_gun_z = game::Dvar_RegisterFloat("cg_gun_z", 0.0f, -800.0f, 800.0f, game::DvarFlags::DVAR_FLAG_NONE, "Up position of the viewmodel");
			utils::hook::jump(0x8D5930_b, cg_calculate_weapon_movement_debug_stub);
		}
	};
}

REGISTER_COMPONENT(gameplay::component)