#include "features.h"
#include "../settings/options.hpp"

namespace no_flash
{
	void handle()
	{
		if (settings::misc::no_flash && g::local_player)
			g::local_player->m_flFlashDuration() = 0.f;
	}
}