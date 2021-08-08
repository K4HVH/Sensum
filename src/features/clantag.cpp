#include "features.h"
#include "../settings/globals.h"
#include "../helpers/utils.h"

namespace clantag
{
	size_t pos = 0;
	std::string clantag;
	float last_time = 0;

	uint8_t* fn_offset;
	using fn_type = int(__fastcall*)(const char*, const char*);

	void set(const char* tag)
	{
		set(tag, "Sensum");
	}

	void set(const char* tag, const char* label)
	{
		if (!fn_offset)
			fn_offset = utils::pattern_scan(SET_CLAN_TAG);

		reinterpret_cast<fn_type>(fn_offset)(tag, label);
	}

	void restore()
	{
		if (!globals::clantag::value.empty())
			set(globals::clantag::value.c_str());
	}

	void animate()
	{
		if (!g::local_player || !globals::clantag::animation)
			return;

		if (globals::clantag::value.empty() || globals::clantag::value.length() == 0)
			return;

		if (clantag != globals::clantag::value || clantag.length() < pos)
		{
			clantag = globals::clantag::value;
			pos = 0;
		}

		if (last_time + globals::clantag::delay > g::global_vars->realtime)
			return;

		last_time = g::global_vars->realtime;

		set(clantag.substr(0, pos).c_str(), xorstr_("Sensum"));

		pos++;
	}
}