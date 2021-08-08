#include "hooks.h"
#include "../settings/globals.h"
#include "../settings/settings.h"
#include "../helpers/console.h"
#include "../helpers/entities.h"

namespace hooks
{
	void __stdcall fire_bullets::hooked(int type)
	{
		original(g::fire_bullets, type);

		const auto index = g::fire_bullets->m_iPlayer + 1;
		auto* entity = c_base_player::GetPlayerByIndex(index);
		if (!entity || !entity->IsPlayer())
			return;

		for (auto& tick : entities::m_items)
		{
			for (auto& player : tick.players)
			{
				if (index == player.index)
				{
					player.is_shooting = true;
					player.m_flShotTime = entity->m_flSimulationTime();
					player.shot_origin = g::fire_bullets->m_vecOrigin;

					return;
				}
			}
		}
	}
}