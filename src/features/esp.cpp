#include "features.h"
#include "../settings/globals.h"
#include "../render/render.h"
#include "../helpers/imdraw.h"
#include "../helpers/console.h"
#include "../helpers/entities.h"
#include "../helpers/autowall.h"
#include "../features/esp.hpp"
#include "../valve_sdk/interfaces/ISurface.h"
#include "../render/fonts/undefeated.hpp"
#include "../helpers/entities.h"

void draw_circle(Color& colors, const Vector& position) 
{
	static BeamInfo_t beam_info;
	beam_info.m_nType = TE_BEAMRINGPOINT;
	beam_info.m_pszModelName = "sprites/physbeam.vmt";
	beam_info.m_nModelIndex = g::mdl_info->GetModelIndex("sprites/physbeam.vmt");
	beam_info.m_nHaloIndex = -1;
	beam_info.m_flHaloScale = 5;
	beam_info.m_flLife = 1.f;
	beam_info.m_flWidth = 15.f;
	beam_info.m_flFadeLength = 1.0f;
	beam_info.m_flAmplitude = 0.f;
	beam_info.m_flRed = colors.r();
	beam_info.m_flGreen = colors.g();
	beam_info.m_flBlue = colors.b();
	beam_info.m_flBrightness = colors.a();
	beam_info.m_flSpeed = 0.f;
 	beam_info.m_nStartFrame = 0.f;
	beam_info.m_flFrameRate = 60.f;
	beam_info.m_nSegments = -1;
	beam_info.m_nFlags = FBEAM_SHADEIN;
	beam_info.m_vecCenter = position + Vector(0, 0, 5);
	beam_info.m_flStartRadius = 20.f;
	beam_info.m_flEndRadius = 320.f; //640.f

	Beam_t* beam = g::view_render_beams->CreateBeamRingPoint(beam_info);

	if (beam)
		g::view_render_beams->DrawBeam(beam);
}

ConVar* type = nullptr;
ConVar* mode = nullptr;

entities::sound_t _sounds[MAX_PLAYERS];

ImU32 GetU32(Color _color)
{
	return ((_color[3] & 0xff) << 24) + ((_color[2] & 0xff) << 16) + ((_color[1] & 0xff) << 8)
		+ (_color[0] & 0xff);
}

namespace esp
{
	decltype(entities::m_local) m_local;
	entities::player_data_t m_entities[MAX_PLAYERS];

	float last_time = 0.f;

	bool is_enabled()
	{
		if (!render::fonts::visuals || g::engine_client->IsConsoleVisible() || (!globals::esp_menu_opened && render::menu::is_visible()))
			return false;

		return g::engine_client->IsInGame() && g::local_player && settings::esp::enabled;
	}

	bool is_matchmaking()
	{
		if (!type)
			type = g::cvar->find("game_type");

		if (!mode)
			mode = g::cvar->find("game_mode");

		if (type->GetInt() != 0) //classic
			return false;

		if (mode->GetInt() != 0 && mode->GetInt() != 1 && mode->GetInt() != 2) //casual, mm, wingman
			return false;

		return true;
	}

	auto get_box(const Vector* transformed_points) -> RECT
	{
		static Vector screen_points[8];
		for (int i = 0; i < 8; i++)
		{
			if (!math::world2screen(transformed_points[i], screen_points[i]))
				return {};
		}

		auto top = screen_points[0].y;
		auto left = screen_points[0].x;
		auto right = screen_points[0].x;
		auto bottom = screen_points[0].y;

		for (int i = 1; i < 8; i++)
		{
			if (left > screen_points[i].x)
				left = screen_points[i].x;
			if (top > screen_points[i].y)
				top = screen_points[i].y;
			if (right < screen_points[i].x)
				right = screen_points[i].x;
			if (bottom < screen_points[i].y)
				bottom = screen_points[i].y;
		}

		return RECT{ (long)left, (long)top, (long)right, (long)bottom };
	}

	void render_helpers()
	{
		if (settings::misc::smoke_helper)
		{
			visuals::RenderInfo();
			visuals::RenderCircle();
			visuals::drawring_3d();
		}

		if (settings::misc::flash_helper)
		{
			visuals::RenderCirclePopflash();
			visuals::drawring_3d_popflash();
		}

		if (settings::misc::hegrenade_helper)
		{
			visuals::RenderInfoHeGrenade();
			visuals::RenderCircleHeGrenade();
			visuals::drawring_3d_hegrenade();
		}

		if (settings::misc::molotov_helper)
		{
			visuals::RenderInfoMolotov();
			visuals::RenderCircleMolotov();
			visuals::drawring_3d_molotov();
		}
	}

	auto rotate_points(Vector* points, float rotation)->void
	{
		const auto points_center = (points[0] + points[1] + points[2]) / 3;
		for (int k = 0; k < 3; k++)
		{
			auto& point = points[k];
			point -= points_center;

			const auto temp_x = point.x;
			const auto temp_y = point.y;

			const auto theta = DEG2RAD(rotation);
			const auto c = cos(theta);
			const auto s = sin(theta);

			point.x = temp_x * c - temp_y * s;
			point.y = temp_x * s + temp_y * c;

			point += points_center;
		}
	};

	auto render_dot(ImDrawList* draw_list, const Vector& origin, const QAngle& angles, const ImU32& color) -> void
	{
		const auto display_size = ImGui::GetIO().DisplaySize;
		const auto screen_center = Vector(display_size.x * .5f, display_size.y * .5f);

		QAngle aim_angles;
		math::vector2angles(origin - g::local_player->m_vecOrigin(), aim_angles);

		const auto angle_yaw_rad = DEG2RAD(angles.yaw - aim_angles.yaw - 90);

		const auto size = 10;
		const auto radius = 45;

		const auto new_point_x = screen_center.x + ((((display_size.x - (size * 3)) * .5f) * (radius / 100.0f)) * cos(angle_yaw_rad)) + (int)(6.0f * (((float)size - 4.f) / 16.0f));
		const auto new_point_y = screen_center.y + ((((display_size.y - (size * 3)) * .5f) * (radius / 100.0f)) * sin(angle_yaw_rad));

		Vector points[3] =
		{
			Vector(new_point_x - size, new_point_y - size),
			Vector(new_point_x + size, new_point_y),
			Vector(new_point_x - size, new_point_y + size)
		};

		rotate_points(points, angles.yaw - aim_angles.yaw - 90.f);

		draw_list->AddTriangleFilled({ points[0].x + 1.f, points[0].y + 1.f }, { points[1].x + 1.f, points[1].y + 1.f }, { points[2].x + 1.f, points[2].y + 1.f }, IM_COL32_BLACK);
		draw_list->AddTriangleFilled({ points[0].x, points[0].y }, { points[1].x, points[1].y }, { points[2].x, points[2].y }, color);
	}

	void render(ImDrawList* draw_list)
	{
		if (!is_enabled())
			return;

		if (entities::locker.try_lock())
		{
			memcpy(m_entities, entities::m_items.front().players, sizeof(m_entities));
			entities::locker.unlock();
		}

		if (entities::local_mutex.try_lock())
		{
			m_local = entities::m_local;
			entities::local_mutex.unlock();
		}

		if (settings::esp::sound && entities::m_mutex_sounds.try_lock())
		{
			memcpy(_sounds, entities::m_sounds, sizeof(_sounds));

			entities::m_mutex_sounds.unlock();
		}

		int x, y;

		g::engine_client->GetScreenSize(x, y);

		int xx = x / 2;
		int yy = y / 2;

		#ifdef _DEBUG
		globals::draw_list->AddText(ImVec2(xx - 100.f, yy + 250.f), utils::to_im32(Color::White), "Sensum was launched in DEBUG - Expect problems!");
		#endif

		QAngle angles;
		g::engine_client->GetViewAngles(angles);

		static const auto white_color = ImGui::GetColorU32(ImVec4::White);
		static const auto smoke_color = ImGui::GetColorU32(ImVec4(1.f, 1.f, 1.f, 0.4f));
		static const auto orange_color = ImGui::GetColorU32(ImVec4::Orange);
		static const auto green_color = ImGui::GetColorU32(ImVec4::Green);
		static const auto black_color = ImGui::GetColorU32(ImVec4::Black);
		static const auto yellow_color = ImGui::GetColorU32(ImVec4::Yellow);
		static const auto red_color = ImGui::GetColorU32(ImVec4::Red);

		ImGui::PushFont(render::fonts::visuals);
		
		static Vector out;

		for (const auto& data : m_entities)
		{
			if (data.index == 0 || !data.hitboxes[0][0].IsValid())
				continue;

			for (const auto& sound : _sounds)
			{
				if (!data.is_alive)
					continue;

				if (sound.index != 0)
				{
					if (settings::esp::sound)
					{
						if (math::world2screen(sound.origin, out))
						{
							globals::draw_list->AddRect(ImVec2(out.x + 3.f, out.y + 3.f), ImVec2(out.x - 3.f, out.y - 3.f), utils::to_im32(Color::White));
						}
					}

					if (settings::esp::soundesp)
					{
						if ((last_time + 0.35f) < g::global_vars->realtime)
						{
							draw_circle(settings::esp::colorSoundEsp, sound.origin);

							last_time = g::global_vars->realtime;
						}
					}
				}
			}
		}

		RECT box;
		for (const auto& data : m_entities)
		{
			if (data.index == 0 || !data.hitboxes[0][0].IsValid())
				continue;

			const auto bbox = get_box(data.points);

			const auto on_screen = (bbox.left > 0 || bbox.right > 0) && (bbox.top > 0 || bbox.bottom > 0);
			if (settings::esp::offscreen && !render::menu::is_visible() && (!on_screen || g::local_player->m_bIsScoped()))
				render_dot(draw_list, data.origin, angles, IM_COL32(255, 0, 50, 255));

			if (settings::esp::visible_only && (!data.is_visible || data.in_smoke || m_local.is_flashed || !on_screen))
				continue;

			{
				bool at_screen = true;
				Vector screen_points[8] = {};
				for (int i = 0; i < 8; i++)
				{
					if (!math::world2screen(data.points[i], screen_points[i]))
					{
						at_screen = false;
						break;
					}
				}

				if (!at_screen)
					continue;

				auto left = screen_points[0].x;
				auto top = screen_points[0].y;
				auto right = screen_points[0].x;
				auto bottom = screen_points[0].y;

				for (int i = 1; i < 8; i++)
				{
					if (left > screen_points[i].x)
						left = screen_points[i].x;
					if (top > screen_points[i].y)
						top = screen_points[i].y;
					if (right < screen_points[i].x)
						right = screen_points[i].x;
					if (bottom < screen_points[i].y)
						bottom = screen_points[i].y;
				}

				box = RECT{ (long)left, (long)top, (long)right, (long)bottom };
				if (box.bottom == 0 || box.left == 0 || box.right == 0 || box.top == 0)
					continue;
			}

			const auto visible_color = utils::to_im32(settings::esp::visibleColor);
			const auto occluded_color = utils::to_im32(settings::esp::occludedColor);

			const auto width = box.right - box.left;
			const auto height = box.bottom - box.top;

			const auto box_color = data.is_dormant ? smoke_color : data.is_visible && !data.in_smoke && !m_local.is_flashed ? visible_color : occluded_color;

			if (settings::esp::bone_esp)
			{
				if (!data.player)
					continue;

				if (!g::local_player)
					continue;

				if (!data.is_player || data.player == g::local_player || data.m_iHealth <= 0 || data.is_dormant || data.team_num == g::local_player->m_iTeamNum())
					continue;

				if (!data.hdr)
					continue;

				static Vector vParent, vChild, sParent, sChild;

				for (int j = 0; j < data.hdr->numbones; j++)
				{
					mstudiobone_t* pBone = data.hdr->GetBone(j);

					if (pBone && (pBone->flags & BONE_USED_BY_HITBOX) && (pBone->parent != -1))
					{
						vChild = data.player->get_bone_position(j);
						vParent = data.player->get_bone_position(pBone->parent);

						int iChestBone = 6;
						Vector vBreastBone;
						Vector vUpperDirection = data.player->get_bone_position(iChestBone + 1) - data.player->get_bone_position(iChestBone);
						vBreastBone = data.player->get_bone_position(iChestBone) + vUpperDirection / 2;
						Vector vDeltaChild = vChild - vBreastBone;
						Vector vDeltaParent = vParent - vBreastBone;

						if ((vDeltaParent.Length() < 9 && vDeltaChild.Length() < 9))
							vParent = vBreastBone;

						if (j == iChestBone - 1)
							vChild = vBreastBone;

						if (abs(vDeltaChild.z) < 5 && (vDeltaParent.Length() < 5 && vDeltaChild.Length() < 5) || j == iChestBone)
							continue;

						if (math::world2screen(vParent, sParent) && math::world2screen(vChild, sChild))
						{
							globals::draw_list->AddLine(ImVec2(sParent.x, sParent.y), ImVec2(sChild.x, sChild.y), utils::to_im32(settings::esp::bone_esp_color));
						}
					}
				}
			}

			if (settings::esp::names)
			{
				const auto name_size = ImGui::CalcTextSize(data.name.c_str());
				auto x = box.left + width / 2.f - name_size.x / 2.f;

				imdraw::outlined_text(
					data.name.c_str(), ImVec2(x, box.top - name_size.y), data.is_dormant ? smoke_color : white_color
				);
			}

			if (settings::esp::is_scoped && data.is_scoped && !data.is_dormant)
			{
				static const auto scoped_text_size = ImGui::CalcTextSize("Scoped");
				auto x = box.left + width / 2.f - scoped_text_size.x / 2.f;

				imdraw::outlined_text("Scoped", ImVec2(x, box.top - scoped_text_size.y - 20.f), white_color);
			}

			if (settings::esp::is_flashed && data.is_flashed && !data.is_dormant)
			{
				static const auto flashed_text_size = ImGui::CalcTextSize("Flashed");
				auto x = box.left + width / 2.f - flashed_text_size.x / 2.f;

				imdraw::outlined_text("Flashed", ImVec2(x, box.top - flashed_text_size.y - 12.f), yellow_color);
			}

			if (settings::esp::is_defusing && data.is_defusing && !data.is_dormant)
			{
				static const auto defusing_text_size = ImGui::CalcTextSize("Defusing");

				imdraw::outlined_text("Defusing", ImVec2(box.right + 2.f, box.top - defusing_text_size.y - -30.f), red_color);
			}

			if (settings::esp::kevlarinfo && !data.is_dormant && is_matchmaking())
			{
				const char* text = "";

				if (data.has_kevlar && data.has_helmet)
					text = "HK";
				
				if (data.has_kevlar && !data.has_helmet)
					text = "K";

				static const auto defusing_text_size = ImGui::CalcTextSize(text);

				imdraw::outlined_text(text, ImVec2(box.right + 2.f, box.top - defusing_text_size.y - -10.0f), white_color);
			}

			if (settings::esp::is_desyncing && data.is_desyncing)
			{
				static const auto defusing_text_size = ImGui::CalcTextSize("Desync");

				imdraw::outlined_text("Desync", ImVec2(box.right + 2.f, box.top - defusing_text_size.y - -20.0f), red_color);
			}


			if (settings::esp::bomb_esp && data.is_c4_carrier && !data.is_dormant)
			{
				static const auto defusing_text_size = ImGui::CalcTextSize("C4");

				imdraw::outlined_text("C4", ImVec2(box.right + 2.f, box.top - defusing_text_size.y - -20.0f), green_color);
			}

			if (settings::esp::haskit && data.has_defkit && !data.is_dormant && is_matchmaking())
			{
				static const auto defusing_text_size = ImGui::CalcTextSize("KIT");

				imdraw::outlined_text("KIT", ImVec2(box.right + 2.f, box.top - defusing_text_size.y - -20.0f), white_color);
			}

			if (settings::esp::money && !data.is_dormant && is_matchmaking())
			{
				char buf[64];
				sprintf_s(buf, "$%1.f", data.m_iMoney);

				const auto weapon_size = ImGui::CalcTextSize(buf);

				auto y_pos = box.bottom + 22.f;
				if (settings::esp::health && settings::esp::health_position == 2)
					y_pos += 7.f;

				if (settings::esp::armour && settings::esp::armour_position == 2)
					y_pos += 7.f;

				imdraw::outlined_text(buf, ImVec2(box.left + width / 2.f - weapon_size.x / 2.f, y_pos), white_color);
			}

			if (settings::esp::ammo && !data.is_dormant && !data.has_knife)
			{
				char buf[256];
				sprintf_s(buf, "(%1.f/%1.f)", data.m_iAmmo, data.m_MaxAmmo);

				const auto weapon_size = ImGui::CalcTextSize(buf);

				auto y_pos = box.bottom + 12.f;
				if (settings::esp::health && settings::esp::health_position == 2)
					y_pos += 7.f;

				if (settings::esp::armour && settings::esp::armour_position == 2)
					y_pos += 7.f;

				imdraw::outlined_text(buf, ImVec2(box.left + width / 2.f - weapon_size.x / 2.f, y_pos), orange_color);
			}

			if (settings::esp::snaplines && !data.is_dormant)
			{
				draw_list->AddLine(ImVec2(m_local.local_pos.x, m_local.local_pos.y), ImVec2(data.old_origin.x, data.old_origin.y), green_color, 1.5f);
			}

			auto render_line = [box, draw_list](const Color& color, const int& value, const int& position, const bool& with_offset = false, const int& offset_position = 0)
			{
				const auto im_color = utils::to_im32(color);

				const auto width = (float(fabs(box.right - box.left))) / 100.f * (100.f - value);
				const auto height = (float(fabs(box.bottom - box.top))) / 100.f * (100.f - value);

				//black width + offset between lines
				const auto offset = with_offset && position == offset_position ? 7.f : 0.f;
				if (position == 0)
				{
					draw_list->AddRectFilled(ImVec2(box.left - 2.f - offset, box.top), ImVec2(box.left - 7.f - offset, box.bottom), black_color);
					draw_list->AddRectFilled(ImVec2(box.left - 3.f - offset, box.top + height), ImVec2(box.left - 6.f - offset, box.bottom), im_color);
				}
				else if (position == 1)
				{
					draw_list->AddRectFilled(ImVec2(box.right + 2.f + offset, box.top), ImVec2(box.right + 7.f + offset, box.bottom), black_color);
					draw_list->AddRectFilled(ImVec2(box.right + 3.f + offset, box.top + height), ImVec2(box.right + 6.f + offset, box.bottom), im_color);
				}
				else if (position == 2)
				{
					draw_list->AddRectFilled(ImVec2(box.left, box.bottom + 2.f + offset), ImVec2(box.right, box.bottom + 7.f + offset), black_color);
					draw_list->AddRectFilled(ImVec2(box.left, box.bottom + 3.f + offset), ImVec2(box.right, box.bottom + 6.f + offset), im_color);
				}
			};

			if (settings::esp::health)
			{
				const auto green = int(data.m_iHealth * 2.55f);
				const auto red = 255 - green;

				if (settings::esp::health_position == 0 || settings::esp::health_position == 1 || settings::esp::health_position == 2)
				{
					render_line(Color(red, green, 0, 255), data.m_iHealth, settings::esp::health_position, false);
				}

				if (settings::esp::health_position == 3)
				{
					char buf[64];
					sprintf_s(buf, "HP: %1.f", data.m_iHealth);

					static const auto defusing_text_size = ImGui::CalcTextSize(buf);

					//imdraw::outlined_text((const char*)buf, ImVec2(box.left - -10.f, box.top - defusing_text_size.y - -10.0f), white_color);

					auto y_pos = box.bottom + 32.f;
					if (settings::esp::health && settings::esp::health_position == 2)
						y_pos += 7.f;

					if (settings::esp::armour && settings::esp::armour_position == 2)
						y_pos += 7.f;

					if (!settings::esp::ammo)
						y_pos -= 10.f;

					if (!settings::esp::money)
						y_pos -= 10.f;

					imdraw::outlined_text(buf, ImVec2(box.left + width / 2.f - defusing_text_size.x / 2.f, y_pos), white_color);
				}
			}

			if (settings::esp::armour)
			{
				render_line(Color(0, 50, 255, 255), data.m_ArmorValue, settings::esp::armour_position, settings::esp::health, settings::esp::health_position);
			}

			if (settings::esp::weapons && !data.weapon.empty())
			{
				const auto weapon_size = ImGui::CalcTextSize(data.weapon.c_str());

				const auto icon_size = ImGui::CalcTextSize(data.icon.c_str());

				if (!render::fonts::weapon_icon)
					return;

				auto y_pos = box.bottom + 2.f;
				if (settings::esp::health && settings::esp::health_position == 2)
					y_pos += 7.f;

				if (settings::esp::armour && settings::esp::armour_position == 2)
					y_pos += 7.f;

				switch (settings::esp::weapon_mode)
				{
				case 0:
					imdraw::outlined_text(data.weapon.c_str(), ImVec2(box.left + width / 2.f - weapon_size.x / 2.f, y_pos), data.is_dormant ? smoke_color : orange_color);
					break;
				case 1:
					ImGui::PushFont(render::fonts::weapon_icon);
					imdraw::outlined_text(data.icon.c_str(), ImVec2(box.left + width / 2.f - data.wep_str_size / 2.f, y_pos), white_color); //was 28
					ImGui::PopFont();
					break;
				}
			}

			if (settings::esp::boxes)
			{
				static const auto thickness = 1.f;
				if (settings::esp::box_type == EBoxType::Normal)
					draw_list->AddRect(ImVec2(box.left, box.top), ImVec2(box.right, box.bottom), box_color, thickness, 15, 1.5f);
				else
				{
					static const auto delta = 5.f;

					std::pair<ImVec2, ImVec2> points[] =
					{
						{ImVec2(box.left, box.top), ImVec2(box.left + width / delta, box.top)}, //left top
						{ImVec2(box.left, box.bottom), ImVec2(box.left + width / delta, box.bottom)}, //left bottom

						{ImVec2(box.right, box.top), ImVec2(box.right - width / delta, box.top) }, //right top
						{ImVec2(box.right, box.bottom), ImVec2(box.right - width / delta, box.bottom)}, //right bottom

						{ImVec2(box.left, box.top), ImVec2(box.left, box.top + height / delta)}, //left top-bottom
						{ImVec2(box.left, box.bottom), ImVec2(box.left, box.bottom - height / delta)}, //left bottom-top

						{ImVec2(box.right, box.top), ImVec2(box.right, box.top + height / delta)}, //right top-bottom
						{ImVec2(box.right, box.bottom), ImVec2(box.right, box.bottom - height / delta)} //right bottom-top
					};

					for (const auto& point : points)
						draw_list->AddLine(point.first, point.second, box_color, thickness);
				}
			}
		}
		ImGui::PopFont();
	}
}

void VGSHelper::Init()
{
	for (int size = 1; size < 128; size++)
	{
		fonts[size] = g::surface->CreateFont_();
		g::surface->SetFontGlyphSet(fonts[size], "Sans-serif", size, 700, 0, 0, FONTFLAG_DROPSHADOW, FONTFLAG_ANTIALIAS);

		icons[size] = g::surface->CreateFont_();
		g::surface->SetFontGlyphSet(icons[size], (const char*)undefeated_compressed_data, undefeated_compressed_size, 700, 0, 0, FONTFLAG_DROPSHADOW, FONTFLAG_ANTIALIAS);
	}

	Inited = true;
}

void VGSHelper::DrawText(std::string text, float x, float y, Color color, int size)
{
	if (!Inited)
		Init();

	//int size = text.size() + 1;
	g::surface->DrawClearApparentDepth();
	wchar_t buf[256];
	g::surface->DrawSetTextFont(fonts[size]);
	g::surface->DrawSetTextColor(color);

	if (MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, buf, 256))
	{
		g::surface->DrawSetTextPos(x, y);
		g::surface->DrawPrintText(buf, wcslen(buf));
	}
}

void VGSHelper::drawring_3d(int16_t x, int16_t y, int16_t z, int16_t radius, uint16_t points, Color color1, float thickness)
{
	Vector pos = Vector(x, y, z);

	float step = (float)M_PI * 2.0f / points;

	for (float a = 0; a < (M_PI * 2.0f); a += step)
	{
		Vector start(radius * cosf(a) + pos.x, radius * sinf(a) + pos.y, pos.z);
		Vector end(radius * cosf(a + step) + pos.x, radius * sinf(a + step) + pos.y, pos.z);

		Vector start2d;
		Vector end2d;

		Vector start22d(start2d.x, start2d.y);
		Vector end22d(end2d.x, end2d.y);
		if (math::world2screen(start, start2d) &&
			math::world2screen(end, end2d))
		{
			start22d.x = start2d.x;
			start22d.y = start2d.y;

			end22d.x = end2d.x;
			end22d.y = end2d.y;

			globals::draw_list->AddLine(ImVec2(start22d.x, start22d.y), ImVec2(end22d.x, end22d.y), utils::to_im32(color1), thickness);
		}
	}
}

void VGSHelper::DrawIcon(std::string text, float x, float y, Color color, int size)
{
	if (!Inited)
		Init();

	//int size = text.size() + 1;
	g::surface->DrawClearApparentDepth();
	wchar_t buf[256];
	g::surface->DrawSetTextFont(icons[size]);
	g::surface->DrawSetTextColor(color);

	if (MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, buf, 256))
	{
		g::surface->DrawSetTextPos(x, y);
		g::surface->DrawPrintText(buf, wcslen(buf));
	}
}

void VGSHelper::DrawLine(float x1, float y1, float x2, float y2, Color color, float size)
{
	/*
	if (outline) {
		g_VGuiSurface->DrawSetColor(Color::Black);
		//g_VGuiSurface->DrawSetApparentDepth(size + 1.f);
		//g_VGuiSurface->DrawLine(x1, y1, x2, y2);
		g_VGuiSurface->DrawFilledRect(x1 - size, y1 - size, x2 + size, y2 + size);
	}
	*/
	g::surface->DrawSetColor(color);

	if (size == 1.f)
		g::surface->DrawLine(x1, y1, x2, y2);
	else
		g::surface->DrawFilledRect(x1 - (size / 2.f), y1 - (size / 2.f), x2 + (size / 2.f), y2 + (size / 2.f));
}
void VGSHelper::DrawBox(float x1, float y1, float x2, float y2, Color clr, float size)
{
	/*
	if (outline) {
		g_VGuiSurface->DrawSetColor(Color::Black);
		g_VGuiSurface->DrawFilledRect(x1 - 1.f, y1, x1 + 1.f, y2); // left
		g_VGuiSurface->DrawFilledRect(x2 - 1.f, y1, x2 + 1.f, y2); // right
		g_VGuiSurface->DrawFilledRect(x1, y1 - 1.f, x2, y1 + 1.f); // top
		g_VGuiSurface->DrawFilledRect(x1, y2 - 1.f, x2, y2 + 1.f); // bottom
	}
	*/
	//g_VGuiSurface->DrawSetColor(clr);
	//g_VGuiSurface->DrawSetApparentDepth(size);
	//g_VGuiSurface->DrawOutlinedRect(static_cast<int>(x1), static_cast<int>(y1), static_cast<int>(x2), static_cast<int>(y2));
	DrawLine(x1, y1, x2, y1, clr, size);
	DrawLine(x1, y2, x2, y2, clr, size);
	DrawLine(x1, y1, x1, y2, clr, size);
	DrawLine(x2, y1, x2, y2, clr, size);
}
void VGSHelper::DrawFilledBox(float x1, float y1, float x2, float y2, Color clr)
{
	g::surface->DrawSetColor(clr);
	//g_VGuiSurface->DrawSetApparentDepth(size);
	g::surface->DrawFilledRect(static_cast<int> (x1), static_cast<int> (y1), static_cast<int> (x2), static_cast<int> (y2));
}
void VGSHelper::DrawTriangle(int count, Vertex_t* vertexes, Color c)
{
	static int Texture = g::surface->CreateNewTextureID(true); // need to make a texture with procedural true
	unsigned char buffer[4] = { (unsigned char)c.r(), (unsigned char)c.g(), (unsigned char)c.b(), (unsigned char)c.a() }; // r,g,b,a

	g::surface->DrawSetTextureRGBA(Texture, buffer, 1, 1); //Texture, char array of texture, width, height
	g::surface->DrawSetColor(c); // keep this full color and opacity use the RGBA @top to set values.
	g::surface->DrawSetTexture(Texture); // bind texture

	g::surface->DrawTexturedPolygon(count, vertexes);
}
void VGSHelper::DrawBoxEdges(float x1, float y1, float x2, float y2, Color clr, float edge_size, float size)
{
	if (fabs(x1 - x2) < (edge_size * 2))
	{
		//x2 = x1 + fabs(x1 - x2);
		edge_size = fabs(x1 - x2) / 4.f;
	}

	DrawLine(x1, y1, x1, y1 + edge_size + (0.5f * edge_size), clr, size);
	DrawLine(x2, y1, x2, y1 + edge_size + (0.5f * edge_size), clr, size);
	DrawLine(x1, y2, x1, y2 - edge_size - (0.5f * edge_size), clr, size);
	DrawLine(x2, y2, x2, y2 - edge_size - (0.5f * edge_size), clr, size);
	DrawLine(x1, y1, x1 + edge_size, y1, clr, size);
	DrawLine(x2, y1, x2 - edge_size, y1, clr, size);
	DrawLine(x1, y2, x1 + edge_size, y2, clr, size);
	DrawLine(x2, y2, x2 - edge_size, y2, clr, size);
}
void VGSHelper::DrawCircle(float x, float y, float r, int seg, Color clr)
{
	g::surface->DrawSetColor(clr);
	g::surface->DrawOutlinedCircle(x, y, r, seg);
}

void VGSHelper::DrawCircle(Vector2D position, float r, int seg, Color clr)
{
	g::surface->DrawSetColor(clr);
	g::surface->DrawOutlinedCircle(position.x, position.y, r, seg);
}

ImVec2 VGSHelper::GetSize(std::string text, int size)
{
	if (!Inited)
		Init();

	wchar_t buf[256];
	int x, y;

	if (MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, buf, 256))
	{
		g::surface->GetTextSize(fonts[size], buf, x, y);
		return ImVec2(x, y);
	}

	return ImVec2(0, 0);
}