#include "features.h"
#include "../settings/options.hpp"

namespace no_smoke
{
	DWORD smoke_count;
	uint8_t* offset;

	auto last_value = false;

	const std::vector<std::string> material_names =
	{
		xorstr_("particle/vistasmokev1/vistasmokev1_fire"),
		xorstr_("particle/vistasmokev1/vistasmokev1_emods"),
		xorstr_("particle/vistasmokev1/vistasmokev1_smokegrenade"),
		xorstr_("particle/vistasmokev1/vistasmokev1_emods_impactdust"),
	};

	void event()
	{
		last_value = settings::misc::no_smoke;

		for (const auto& name : material_names)
		{
			auto material = g::mat_system->FindMaterial(name.c_str(), TEXTURE_GROUP_CLIENT_EFFECTS);
			if (material)
				material->SetMaterialVarFlag(MATERIAL_VAR_WIREFRAME, last_value);
		}
	}

	void handle()
	{
		if (!offset)
			offset = utils::pattern_scan(IS_LINE_GOES_THROUGH_SMOKE);

		if (!smoke_count)
			smoke_count = *reinterpret_cast<DWORD*>(offset + 0x8);

		if (settings::misc::no_smoke)
			*reinterpret_cast<int*>(smoke_count) = 0;

		if (last_value != settings::misc::no_smoke)
			event();
	}
}