#include "features.h"
#include "../helpers/autowall.h"
#include "../settings/globals.h"
#include "../helpers/input.h"
#include "../helpers/console.h"
#include "../helpers/entities.h"
#include "..\\hooks\hooks.h"
#include "..//helpers/notifies.h"

namespace aimbot
{
	bool silent_enabled = false;
	bool _is_trigger = false;
	bool _is_backshot = false;
	bool last_hit_state = false;
	
	float out_delay = 0.f;
	float network_delay = 0.f;
	float latency_delay = 0.f;
	float correct_nexttime = 0.f;
	float interpolation_time = 0.f;
	
	bool shot_delay = false;
	bool is_delayed = false;
	ULONGLONG shot_delay_time = 0;

	bool kill_delay = false;
	ULONGLONG kill_delay_time = 0;

	weapon_setting a_settings;
	c_base_player* target = nullptr;
	CCSWeaponInfo* weapon_data;
	c_base_combat_weapon* weapon = nullptr;

	matrix3x4_t bt_matrix[MAXSTUDIOBONES];

	struct entity_pos_t
	{
		int id;
		float sim_time;

		Vector origin;
		Vector eye_pos;
	};

	struct backtrack_data_t
	{
		matrix3x4_t matrix[128];
		bool is_moving;
		bool is_dormant;
		bool is_alive;
	};

	std::map<int, std::deque<backtrack_data_t>> data;

	std::vector<entity_pos_t> duplicates;

	namespace punches
	{
		QAngle current = { 0, 0, 0 };

		QAngle last = { 0, 0, 0 };
		QAngle last_corrected = { 0, 0, 0 };
	}

	namespace trigger
	{
		bool delay = false;
		ULONGLONG delay_time = 0;
		ULONGLONG delay_between_shots_time = 0;
	}

	struct target_t
	{
		int tick;
		float fov;
		float time; //simulation time

		QAngle abs_angles; //target angles
		QAngle aim_angles;
		QAngle silent_angles;

		int hitbox_id;
		std::array<Vector, 4> hitboxes;
		Vector velocity;
		Vector abs_origin;

		c_base_player* entity;

		bool is_visible = false;
		Vector hitbox;

		bool can_lock_on = false;
		bool can_lock_on_silent = false;

		bool operator<(const target_t& other) const
		{
			return fov < other.fov;
		}
	};

	int get_hitgroup(const int& hitbox)
	{
		switch (hitbox)
		{
		case HITBOX_HEAD:
		case HITBOX_NECK:
			return 1;

		case HITBOX_THORAX:
		case HITBOX_LOWER_CHEST:
		case HITBOX_UPPER_CHEST:
			return 2;

		case HITBOX_PELVIS:
		case HITBOX_BELLY:
			return 3;

		case HITBOX_RIGHT_HAND:
		case HITBOX_RIGHT_UPPER_ARM:
		case HITBOX_RIGHT_FOREARM:
			//return 4;

		case HITBOX_LEFT_HAND:
		case HITBOX_LEFT_UPPER_ARM:
		case HITBOX_LEFT_FOREARM:
			return 5;

		case HITBOX_RIGHT_THIGH:
		case HITBOX_RIGHT_CALF:
		case HITBOX_RIGHT_FOOT:
			//return 6;

		case HITBOX_LEFT_THIGH:
		case HITBOX_LEFT_CALF:
		case HITBOX_LEFT_FOOT:
			return 7;
		}

		return -1;
	}
	//--------------------------------------------------------------------------------
	bool is_suitable_by_min_damage(c_base_player* entity, const int& hitbox, const float& min_damage)
	{
		if (min_damage <= 1.f || min_damage > 100.f)
			return true;

		const auto hitgroup = get_hitgroup(hitbox);
		if (hitgroup == -1)
		{
			console::print("hitgroup not found for %d", hitbox);
			return false;
		}

		auto damage = float(weapon_data->iDamage);
		autowall::scale_damage(hitgroup, weapon_data->flArmorRatio, entity->m_bHasHelmet(), entity->m_ArmorValue(), damage);

		return damage >= min_damage || damage >= entity->m_iHealth();
	}
	//--------------------------------------------------------------------------------
	bool is_time_valid(const float& time)
	{
		return fabsf(out_delay - (correct_nexttime - time - interpolation_time)) <= 0.2f;
	}
	//--------------------------------------------------------------------------------
	bool has_rcs()
	{
		return a_settings.recoil.enabled && g::local_player->m_iShotsFired() > (a_settings.recoil.first_bullet ? 0 : 1);
	}
	//--------------------------------------------------------------------------------
	bool is_trigger()
	{
		return _is_trigger;
	}
	bool IsNotSilent(float fov)
	{
		return has_rcs() || !a_settings.silent.enabled || (a_settings.silent.enabled && fov > a_settings.silent.fov);
	}

	//--------------------------------------------------------------------------------
	bool is_enabled(CUserCmd* cmd)
	{
		_is_trigger = false;
		_is_backshot = false;

		weapon = g::local_player->m_hActiveWeapon();
		if (!weapon || !weapon->IsWeapon())
			return false;

		if (!weapon->HasBullets() || weapon->IsReloading())
			return false;

		int index = 0;
		if (settings::aimbot::setting_type == settings_type_t::separately)
			index = weapon->m_iItemDefinitionIndex();
		else if (settings::aimbot::setting_type == settings_type_t::subgroups)
		{
			switch (weapon->m_iItemDefinitionIndex())
			{
			case WEAPON_AWP:
			case WEAPON_SSG08:
			case WEAPON_DEAGLE:
				index = 200 + weapon->m_iItemDefinitionIndex();
				break;
			default:
				index = weapon->get_weapon_data()->WeaponType;
			}
		}
		else if (settings::aimbot::setting_type == settings_type_t::groups)
		{
			int group_index = -1;
			for (size_t k = 0; k < settings::aimbot::m_groups.size(); k++)
			{
				const auto& group = settings::aimbot::m_groups[k];
				if (std::find(group.weapons.begin(), group.weapons.end(), weapon->m_iItemDefinitionIndex()) != group.weapons.end())
				{
					group_index = k;
					break;
				}
			}

			if (group_index == -1)
				return false;
		}

		a_settings = settings::aimbot::m_items[index];
		if (!a_settings.enabled)
			return false;

		if (!(cmd->buttons & IN_ATTACK))
		{
			if (!_is_trigger)
			{
				if (!a_settings.trigger.enabled)
					return false;

				if (globals::binds::trigger > 0 && !(GetAsyncKeyState(globals::binds::trigger) & 0x8000))
					return false;

				_is_trigger = true;
			}
		}

		static int time_in_scope = 0;
		if (utils::is_sniper(weapon->m_iItemDefinitionIndex()))
		{
			if (!g::local_player->m_bIsScoped())
				time_in_scope = GetTickCount64();

			if (a_settings.check_zoom && static_cast<unsigned long long>(time_in_scope) + 60 > GetTickCount64())
				return false;
		}

		if (a_settings.fov == 0 && !a_settings.recoil.enabled)
			return false;

		return true;
	}
	//--------------------------------------------------------------------------------
	bool able_to_rcs()
	{
		auto nci = g::engine_client->GetNetChannelInfo();

		if (nci)
		{
			float ping = 1.f / nci->GetLatency(FLOW_INCOMING);

			if (ping >= 120.f)
				return false;
		}

		float fps = 1.f / g::global_vars->absoluteframetime;

		if (fps <= 30.f)
			return false;

		return true;
	}
	//--------------------------------------------------------------------------------
	bool RCS(QAngle& angle, c_base_player* target, CUserCmd* cmd)
	{
		if (!has_rcs() || !able_to_rcs() || (a_settings.recoil.pitch == 0 && a_settings.recoil.yaw == 0))
			return false;

		if (is_trigger())
			cmd->buttons |= IN_ATTACK;

		auto x = a_settings.recoil.pitch;
		auto y = a_settings.recoil.yaw;

		if (a_settings.recoil.humanize)
		{
			x += utils::random(0.1f, 0.5f);
			y += utils::random(0.1f, 0.6f);
		}

		static auto weapon_recoil_scale = g::cvar->find("weapon_recoil_scale");
		float scale = weapon_recoil_scale->GetFloat();

		if (scale != 2.f)
		{
			x = scale * x / 2.f;
			y = scale * y / 2.f;
		}

		//The perfect value for rcs IMO is 2.52f on both axis.
		x += (scale *= 1.26f) / 4.84615f; //Dont mind this magic number its just calculated...
		y += scale / 4.84615f;			  //...so the result at 100% yaw/pitch is always at 2.52f	

		if (x <= 0 && y <= 0)
			return false;

		QAngle punch = { };

		if (target)
			punch = { punches::current.pitch * x ,  punches::current.yaw * y, 0 };
		else if (a_settings.recoil.standalone)
			punch = { (punches::current.pitch - punches::last.pitch) * x, (punches::current.yaw - punches::last.yaw) * y, 0 };

		if ((punch.pitch != 0.f || punch.yaw != 0.f) && g::local_player->m_aimPunchAngle().roll == 0.f)
		{
			angle -= punch;
			angle.NormalizeClamp();
		}

		punches::last_corrected = punch;

		return true;
	}

	//--------------------------------------------------------------------------------
	float get_fov()
	{
		if (!silent_enabled || a_settings.silent.with_smooth)
			return a_settings.fov;

		return a_settings.silent.fov;
	}

	float GetFov() {
		if (!silent_enabled)
			return a_settings.fov;

		return a_settings.silent.fov > a_settings.fov ? a_settings.silent.fov : a_settings.fov;
	}
	//--------------------------------------------------------------------------------
	void handle_shot_delay(CUserCmd* cmd)
	{
		if (a_settings.silent.enabled)
			return;

		if (!is_delayed && !shot_delay && a_settings.shot_delay > 0)
		{
			is_delayed = true;
			shot_delay = true;
			shot_delay_time = GetTickCount64() + a_settings.shot_delay;
		}

		if (shot_delay && shot_delay_time <= GetTickCount64())
			shot_delay = false;

		if (shot_delay)
			cmd->buttons &= ~IN_ATTACK;
	}
	//--------------------------------------------------------------------------------
	bool is_able_to_aim()
	{
		if (a_settings.check_flash && g::local_player->IsFlashed())
			return false;

		if (a_settings.check_air && !(g::local_player->m_fFlags() & FL_ONGROUND))
			return false;

		return true;
	}
	//--------------------------------------------------------------------------------
	void reset_vars()
	{
		target = nullptr;

		is_delayed = false;
		shot_delay = false;
		last_hit_state = false;

		trigger::delay = false;
		trigger::delay_time = 0;
		trigger::delay_between_shots_time = 0;

		punches::last = { 0, 0, 0 };
		punches::current = { 0, 0, 0 };
		punches::last_corrected = { 0, 0, 0 };

		silent_enabled = a_settings.silent.enabled && a_settings.silent.fov > 0.f;
	}
	//--------------------------------------------------------------------------------
	std::vector<int> hitboxes()
	{
		std::vector<int> list;

		auto& active = g::local_player->m_hActiveWeapon();
		auto& aa_settings = settings::aimbot::m_items[active->m_iItemDefinitionIndex()];

		if (a_settings.hitboxes.head)
			list.push_back(HITBOX_HEAD); //голова

		if (a_settings.hitboxes.neck)
			list.push_back(HITBOX_NECK); //шея

		if (a_settings.hitboxes.body)
		{
			list.push_back(HITBOX_UPPER_CHEST); //верх груди
			list.push_back(HITBOX_LOWER_CHEST); //низ груди
			list.push_back(HITBOX_THORAX); //живот
			list.push_back(HITBOX_BELLY); //поясница
		}

		if (a_settings.hitboxes.hands)
		{
			//list.emplace_back(HITBOX_RIGHT_HAND);
			//list.emplace_back(HITBOX_LEFT_HAND);

			list.push_back(HITBOX_RIGHT_UPPER_ARM);
			list.push_back(HITBOX_RIGHT_FOREARM);
			list.push_back(HITBOX_LEFT_UPPER_ARM);
			list.push_back(HITBOX_LEFT_FOREARM);

			list.push_back(HITBOX_PELVIS); //таз
		}

		if (a_settings.hitboxes.legs)
		{
			list.push_back(HITBOX_RIGHT_THIGH); //бедро
			list.push_back(HITBOX_LEFT_THIGH); //бедро

			list.push_back(HITBOX_RIGHT_CALF); //колено
			list.push_back(HITBOX_LEFT_CALF);

			//list.emplace_back(HITBOX_RIGHT_FOOT); //стопа
			//list.emplace_back(HITBOX_LEFT_FOOT);
		}

		if (aa_settings.rcs_override_hitbox)
		{
			if (g::local_player->m_iShotsFired() >= 3)
			{
				list.pop_back();

				list.push_back(HITBOX_UPPER_CHEST); //верх груди
				list.push_back(HITBOX_LOWER_CHEST); //низ груди
				list.push_back(HITBOX_THORAX); //живот
				list.push_back(HITBOX_BELLY); //поясница
			}
			else if (g::local_player->m_iShotsFired() < 3)
			{
				list.pop_back();

				if (aa_settings.hitboxes.head)
					list.push_back(HITBOX_HEAD);

				if (aa_settings.hitboxes.neck)
					list.push_back(HITBOX_NECK);

				if (aa_settings.hitboxes.body)
				{
					list.push_back(HITBOX_UPPER_CHEST); //верх груди
					list.push_back(HITBOX_LOWER_CHEST); //низ груди
					list.push_back(HITBOX_THORAX); //живот
					list.push_back(HITBOX_BELLY); //поясница
				}

				if (aa_settings.hitboxes.legs)
				{
					list.push_back(HITBOX_RIGHT_THIGH); //бедро
					list.push_back(HITBOX_LEFT_THIGH); //бедро

					list.push_back(HITBOX_RIGHT_CALF); //колено
					list.push_back(HITBOX_LEFT_CALF);
				}

				if (aa_settings.hitboxes.hands)
				{
					list.push_back(HITBOX_RIGHT_UPPER_ARM);
					list.push_back(HITBOX_RIGHT_FOREARM);
					list.push_back(HITBOX_LEFT_UPPER_ARM);
					list.push_back(HITBOX_LEFT_FOREARM);

					list.push_back(HITBOX_PELVIS); //таз
				}
			}
		}

		return list;
	}
	//--------------------------------------------------------------------------------
	std::vector<target_t> get_targets(const QAngle& current_angles, const Vector& eye_pos, const int& current_tick)
	{
		const auto hitbox_ids = hitboxes();
		if (hitbox_ids.empty())
			return {};

		float fov = 0.f;
	
		QAngle aim_angles;
		QAngle silent_angles;

		std::vector<target_t> targets;
		std::vector<target_t> tick_players;

		duplicates.clear();

		const auto max_time = TICKS_TO_TIME(std::clamp<int>(a_settings.backtrack.ticks, 0, 12) + 1);
		for (const auto& tick_data : entities::m_items)
		{
			for (const auto& player_data : tick_data.players)
			{
				if (_is_backshot && !player_data.is_shooting)
					continue;

				if (player_data.index == 0 || player_data.index == g::local_player->GetIndex())
					continue;

				const auto sim_time = _is_backshot ? player_data.m_flShotTime : player_data.m_flSimulationTime;
				const auto delta_time = fabsf(out_delay - (correct_nexttime - sim_time - interpolation_time));
				if (!player_data.eye_pos.IsValid() || delta_time > 0.2f || delta_time > max_time)
					continue;

				const auto result_of_duplicates = std::find_if(duplicates.begin(), duplicates.end(), [player_data](entity_pos_t const& c)
				{
					const auto is_same = c.sim_time == player_data.m_flSimulationTime || (c.eye_pos == player_data.eye_pos && c.origin == player_data.world_pos);

					return c.id == player_data.index && is_same;
				});

				if (result_of_duplicates != duplicates.end())
					continue;

				auto player = c_base_player::GetPlayerByIndex(player_data.index);
				if (!player || !player->IsPlayer() || !player->IsAlive() || player->is_dormant())
					continue;

				auto model = player->GetModel();
				if (!model)
					continue;

				auto hdr = g::mdl_info->GetStudiomodel(model);
				if (!hdr)
					continue;

				duplicates.push_back({ player_data.index, player_data.m_flSimulationTime, player_data.eye_pos, player_data.world_pos });

				for (const auto& hitbox_id : hitbox_ids)
				{
					const auto& hitbox = player_data.hitboxes[hitbox_id][0];
					if (!hitbox.IsValid())
						continue;

					math::vector2angles(hitbox - eye_pos, aim_angles);
					aim_angles.NormalizeClamp();

					math::vector2angles(hitbox - eye_pos, silent_angles);
					silent_angles.NormalizeClamp();

					fov = a_settings.dynamic_fov ?
						math::GetRealDistanceFOV(eye_pos.DistTo(hitbox), current_angles, aim_angles) :
						math::GetFovToPlayer(current_angles, aim_angles);

					if (a_settings.fov < fov)
						continue;

					(a_settings.backtrack.legit ? tick_players : targets).push_back({
						tick_data.tick_count,
						fov,
						sim_time,
						player_data.angles,
						aim_angles,
						silent_angles,
						hitbox_id,
						player_data.hitboxes[hitbox_id],
						player_data.offset,
						_is_backshot ? player_data.shot_origin : player_data.world_pos,
						player
						});
				}
			}

			if (a_settings.backtrack.legit)
			{
				std::sort(tick_players.begin(), tick_players.end());
				for (const auto& item : tick_players)
					targets.push_back(item);

				tick_players.clear();
			}
		}

		if (!targets.empty() && !a_settings.backtrack.legit)
			std::sort(targets.begin(), targets.end());

		return targets;
	}
	//--------------------------------------------------------------------------------
	bool find_target(const QAngle& angles, const int& tick_count, target_t& result)
	{
		if (!kill_delay && a_settings.kill_delay > 0 && target && !target->IsAlive())
		{
			target = nullptr;
			shot_delay = false;
			kill_delay = true;
			kill_delay_time = GetTickCount64() + a_settings.kill_delay;
		}

		target = nullptr;

		if (kill_delay)
		{
			if (kill_delay_time <= GetTickCount64())
				kill_delay = false;
			else
				return false;
		}

		if (!is_able_to_aim() || a_settings.fov <= 0.f)
			return false;

		const auto eye_pos = g::local_player->GetEyePos();
		auto targets = get_targets(angles, eye_pos, tick_count);
		if (targets.empty())
			return false;

		Ray_t ray;
		CGameTrace tr;

		Vector old_pos;
		QAngle old_angles;

		const auto awall_damage = std::clamp<float>(a_settings.autowall.min_damage, 0.f, 100.f);
		const auto visible_damage = std::clamp<float>(a_settings.min_damage, 0.f, 100.f);

		const auto awall_enabled = a_settings.autowall.enabled && awall_damage >= 1.f;

		for (const auto& item : targets)
		{
			old_pos = item.entity->m_vecOrigin();

			auto hitbox_id = 0;
			auto pos_changed = false;

			auto reset_poses = [&item, old_pos](const bool& pos_changed)
			{
				if (!pos_changed)
					return;

				item.entity->SetAbsOrigin(old_pos);
				item.entity->InvalidateBoneCache();
			};

			CTraceFilterSkipTwoEntities filter(g::local_player, item.entity);
			for (const auto& hitbox : item.hitboxes)
			{
				if (!hitbox.IsValid())
					continue;

				if (!awall_enabled && a_settings.by_damage && !is_suitable_by_min_damage(item.entity, item.hitbox_id, visible_damage))
					continue;

				if (a_settings.check_smoke && utils::is_line_goes_through_smoke(eye_pos, hitbox))
					break;

				ray.Init(eye_pos, hitbox);
				g::engine_trace->trace_ray(ray, CONTENTS_SOLID | CONTENTS_MOVEABLE | CONTENTS_MONSTER | CONTENTS_DEBRIS | CONTENTS_HITBOX, &filter, &tr);

				auto is_visible = true;
				if (eye_pos.DistTo(tr.endpos) != eye_pos.DistTo(hitbox))
				{
					if (!awall_enabled || hitbox_id >= 1 || !is_suitable_by_min_damage(item.entity, item.hitbox_id, awall_damage))
						continue;

					hitbox_id++;
					pos_changed = true;

					item.entity->SetAbsOrigin(item.abs_origin);
					//item.entity->SetAbsAngles(item.abs_angles);
					item.entity->InvalidateBoneCache();

					const auto damage = autowall::get_damage(hitbox);
					const auto is_damaged = damage >= item.entity->m_iHealth() || damage >= awall_damage;
					if (!is_damaged)
						continue;

					is_visible = false;
				}
				else if (a_settings.by_damage && !is_suitable_by_min_damage(item.entity, item.hitbox_id, visible_damage))
					continue;

				reset_poses(pos_changed);

				QAngle aim_angles;
				math::vector2angles(hitbox - eye_pos, aim_angles);
				aim_angles.NormalizeClamp();

				QAngle silent_angles;
				math::vector2angles(hitbox - eye_pos, silent_angles);
				silent_angles.NormalizeClamp();

				result = item;
				result.aim_angles = aim_angles;
				result.silent_angles = silent_angles;
				result.is_visible = is_visible;
				result.hitbox = hitbox;
				target = item.entity;

				return true;
			}

			reset_poses(pos_changed);
		}

		return false;
	}
	//--------------------------------------------------------------------------------
	void fetch_net_delays(const int& tick_count)
	{
		const auto nci = g::engine_client->GetNetChannelInfo();

		static const auto sv_maxunlag = g::cvar->find("sv_maxunlag");

		const auto unlag = sv_maxunlag->GetFloat();
		interpolation_time = utils::get_interpolation_compensation();

		latency_delay = nci->GetLatency(FLOW_OUTGOING) + nci->GetLatency(FLOW_INCOMING);
		network_delay = std::clamp(latency_delay + interpolation_time, 0.f, unlag);
		out_delay = std::clamp(nci->GetLatency(FLOW_OUTGOING) + interpolation_time, 0.f, unlag);

		correct_nexttime = TICKS_TO_TIME(tick_count + 1) + network_delay;
	}
	//--------------------------------------------------------------------------------

	void get_backtrack_data(CUserCmd* cmd)
	{
		if(!a_settings.enabled || a_settings.backtrack.ticks <= 0)
		   return;

		if (!settings::chams::enemy::backtrack_chams && !settings::chams::teammates::backtrack_chams)
			return;

		if (!g::engine_client->IsInGame() || !g::engine_client->IsConnected())
			return;

		backtrack_data_t bt;

		c_base_player* player;
		for (int i = 1; i < g::engine_client->GetMaxClients(); i++)
		{
			player = c_base_player::GetPlayerByIndex(i);

			if (!player)
				continue;

			if (!player->IsPlayer() || !player->IsAlive() || player->IsDormant())
				continue;

			auto model = player->GetModel();

			if (!model)
				return;

			auto hdr = g::mdl_info->GetStudiomodel(model);

			if (!hdr)
				return;

			bt.is_moving = (player->m_vecVelocity().x != 0.f || player->m_vecVelocity().y != 0.f || player->m_vecVelocity().z != 0.f);
			bt.is_dormant = player->IsDormant();
			bt.is_alive = player->IsAlive();

			auto hitbox_set = hdr->GetHitboxSet(player->m_nHitboxSet());

			if (!hitbox_set)
				return;

			auto hitbox_head = hitbox_set->GetHitbox(HITBOX_HEAD);
			auto hitbox_center = (hitbox_head->bbmin + hitbox_head->bbmax) * 0.5f;

			player->PVSFix();

			player->SetupBones(bt.matrix, 128, BONE_USED_BY_ANYTHING, g::global_vars->curtime);

			data[i].push_front(bt);

			for (int j = 0; j < data[i].size(); j++)
			{
				if (!data[i].empty())
				{
					if (!player)
						data.at(i).clear();

					if (j > a_settings.backtrack.ticks)
						data[i].pop_back();

					if (data[i].front().is_dormant || !data[i].front().is_alive)
						data[i].clear();

					if (!(player->m_vecVelocity().x != 0.f || player->m_vecVelocity().y != 0.f || player->m_vecVelocity().z != 0.f))
						data.at(i).clear();
				}
			}
		}
	}

	void backtrack_chams(IMatRenderContext* context, const DrawModelState_t& state, const ModelRenderInfo_t& info, matrix3x4_t* bone)
	{
		if (a_settings.backtrack.ticks <= 0)
			return;

		if (!settings::chams::enemy::backtrack_chams && !settings::chams::teammates::backtrack_chams)
			return;

		if (!g::engine_client->IsInGame() || !g::engine_client->IsConnected() || !bone)
			return;

		if (!g::local_player)
			return;

		static auto original = hooks::draw_model_execute::original;

		c_base_player* player = reinterpret_cast<c_base_player*>(g::entity_list->GetClientEntity(info.entity_index));

		static IMaterial* material = g::mat_system->FindMaterial("debug/debugambientcube", TEXTURE_GROUP_OTHER);

		if (info.entity_index > 0 && info.entity_index <= 64)
		{
			if (data.empty())
				return;
			
			if (data.count(info.entity_index) > 0)
			{
				auto& bt_data = data.at(info.entity_index);

				if (bt_data.empty())
					return;

				for (int i = 1; i < bt_data.size(); i++)
				{
					if (!player)
						continue;

					if (!player->IsPlayer())
						continue;

					if (!player->IsAlive())
						continue;

					if (i > a_settings.backtrack.ticks)
						continue;

					auto active_wpn = g::local_player->m_hActiveWeapon().Get();

					if (!active_wpn)
						continue;

					auto wpn_data = active_wpn->get_weapon_data();

					if (!wpn_data)
						continue;

					if (wpn_data->WeaponType == WEAPONTYPE_KNIFE)
						continue;

					bool is_enemy = g::local_player->m_iTeamNum() != player->m_iTeamNum();
					bool is_teammate = !is_enemy;

					if (is_enemy && settings::chams::enemy::backtrack_chams)
					{
						if (i == a_settings.backtrack.ticks)
						{
							if (bt_data.at(i).is_moving)
							{
								material->ColorModulate(settings::chams::enemy::color_backtrack.r() / 255.0f, settings::chams::enemy::color_backtrack.g() / 255.0f, settings::chams::enemy::color_backtrack.b() / 255.0f);
								material->AlphaModulate(settings::chams::enemy::color_backtrack.a() / 255.0f);
								material->SetMaterialVarFlag(MATERIAL_VAR_IGNOREZ, false);
								g::mdl_render->ForcedMaterialOverride(material);
								original(g::mdl_render, context, &state, &info, bt_data.at(i).matrix);
								g::mdl_render->ForcedMaterialOverride(nullptr);
							}
						}
					}

					if (is_teammate && settings::chams::teammates::backtrack_chams)
					{
						if (i == a_settings.backtrack.ticks)
						{
							if (bt_data.at(i).is_moving)
							{
								material->ColorModulate(settings::chams::teammates::color_backtrack.r() / 255.0f, settings::chams::teammates::color_backtrack.g() / 255.0f, settings::chams::teammates::color_backtrack.b() / 255.0f);
								material->AlphaModulate(settings::chams::teammates::color_backtrack.a() / 255.0f);
								material->SetMaterialVarFlag(MATERIAL_VAR_IGNOREZ, false);
								g::mdl_render->ForcedMaterialOverride(material);
								original(g::mdl_render, context, &state, &info, bt_data.at(i).matrix);
								g::mdl_render->ForcedMaterialOverride(nullptr);
							}
						}
					}
				}
			}
		}
	}

	void handle(CUserCmd* cmd)
	{
		if (!g::local_player || !g::local_player->IsAlive())
			return;

		if (!is_enabled(cmd))
		{
			reset_vars();
			return;
		}

		weapon_data = weapon->get_weapon_data();

		target_t result;
		QAngle silent_angles;

		auto angles = cmd->viewangles;
		const auto current = angles;

		const auto eye_pos = g::local_player->GetEyePos();

		fetch_net_delays(cmd->tick_count);
		
		auto set_and_restore_poses = [](const Vector& origin, const QAngle& angles, const std::function<void()>& fn)
		{
			const auto& old_pos = target->m_vecOrigin();
			const auto& old_angles = target->GetAbsAngles();

			target->SetAbsOrigin(origin);
			//target->SetAbsAngles(angles);
			target->InvalidateBoneCache();

			fn();

			target->SetAbsOrigin(old_pos);
			//target->SetAbsAngles(old_angles);
			target->InvalidateBoneCache();
		};

		int tick_count = cmd->tick_count;
		if (find_target(angles + (has_rcs() ? punches::last_corrected : QAngle{}), tick_count, result))
		{
			auto reset_angles = [cmd, &result, &angles, &silent_angles, &tick_count, current]()
			{
				target = nullptr;
				tick_count = cmd->tick_count;
			};

			angles = result.aim_angles;
			silent_angles = result.silent_angles;
			tick_count = TIME_TO_TICKS(result.time + interpolation_time);
			
			const auto min_hitchance = is_trigger() ? a_settings.trigger.hitchance : a_settings.min_hitchanse;
			if (min_hitchance > 5.f)
			{
				auto hit_state = false;
				set_and_restore_poses(result.abs_origin, result.abs_angles, [&hit_state, angles, min_hitchance, result]()
					{
						hit_state = utils::hitchance(target, angles, min_hitchance/*, 150.f, hitbox_groups[result.hitbox_id]*/);
					});

				if (!hit_state)
					reset_angles();
			}

			if (result.is_visible)
			{
				if (target && !a_settings.by_damage && !is_suitable_by_min_damage(target, result.hitbox_id, a_settings.min_damage))
					reset_angles();
			}

			if (target)
			{
				silent_angles = result.silent_angles;
				if (!is_trigger())
					handle_shot_delay(cmd);
			}

			if (is_trigger())
			{
				if (!target || trigger::delay_between_shots_time > GetTickCount64())
					return;

				if (trigger::delay)
				{
					if (trigger::delay_time + a_settings.trigger.delay > GetTickCount64())
						return;
				}
				else if (a_settings.trigger.delay > 0)
				{
					trigger::delay = true;
					trigger::delay_time = GetTickCount64();
					return;
				}

				if (weapon->CanFire() || a_settings.trigger.delay_btw_shots == 0)
				{
					trigger::delay_between_shots_time = GetTickCount64() + a_settings.trigger.delay_btw_shots;

					cmd->buttons |= IN_ATTACK;
				}
			}
		}

		punches::current = g::local_player->m_aimPunchAngle();
		{
			if (!RCS(angles, target, cmd))
				punches::last_corrected = { 0, 0, 0 };

			angles.NormalizeClamp();

			RCS(silent_angles, target, cmd);
			silent_angles.NormalizeClamp();
		}
		punches::last = punches::current;

		if (target)
			math::smooth(a_settings.smooth, current, angles, angles, a_settings.recoil.humanize);

		angles.NormalizeClamp();

		auto engine_angles = true;

		auto can_use_silent = true;
		if (a_settings.recoil.humanize)
		{
			const auto chance = rand() % static_cast<int>(11);
			can_use_silent = chance > 3;
		}

		if (target && silent_enabled && can_use_silent)
		{
			can_use_silent = target && silent_enabled && a_settings.silent.fov >= result.fov && weapon->CanFire();
			cmd->viewangles = can_use_silent ? silent_angles : angles;
		}

		math::correct_movement(cmd, current);

		if (engine_angles)
			g::engine_client->SetViewAngles(angles);

		cmd->viewangles = angles;
		
		silent_enabled = a_settings.silent.enabled && a_settings.silent.always;

		if (target && shot_delay && a_settings.autodelay)
		{
			set_and_restore_poses(result.abs_origin, result.abs_angles, [eye_pos, angles, result]()
				{
					Ray_t ray;
					CGameTrace tr;

					Vector range;
					math::angle2vectors(angles, range);
					range *= weapon->get_weapon_data()->flRange;

					if (result.is_visible)
					{
						CTraceFilterSkipEntity filter(g::local_player);

						ray.Init(eye_pos, eye_pos + range);
						g::engine_trace->trace_ray(ray, MASK_SHOT | CONTENTS_GRATE, &filter, &tr);

						if (tr.hit_entity == result.entity)
							shot_delay = false;
					}
					else if (autowall::get_damage(eye_pos + range) > 1.f)
					{
						shot_delay = false;
					}
				});

			if (!shot_delay)
				cmd->buttons |= IN_ATTACK;
		}

		if (a_settings.autopistol && weapon->IsPistol() && !weapon->CanFire())
			cmd->buttons &= ~IN_ATTACK;

		if (cmd->buttons & IN_ATTACK)
			cmd->tick_count = tick_count;
	}
}