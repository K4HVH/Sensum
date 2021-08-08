#include "HitPossitionHelper.h"
#include "runtime_saver.h"
#include "../settings/settings.h"

runtime_saver saver;

void HitPossitionHelper::OnFireEvent(IGameEvent* event)
{
	if (!event || !g::local_player)
	{
		return;
	}
	if (!strcmp(event->GetName(), "player_hurt"))
	{
		PlayerHurt(event);
	}
	if (!strcmp(event->GetName(), "bullet_impact"))
	{
		BulletImpact(event);
	}
}

void HitPossitionHelper::PlayerHurt(IGameEvent* event)
{
	//2nd

	c_base_player* attacker = GetPlayer(event->GetInt("attacker"));
	c_base_player* hurt = GetPlayer(event->GetInt("userid"));

	if (!attacker || !hurt || attacker != g::local_player)
	{
		return;
	}

	Vector EPosition = hurt->m_vecOrigin();
	ImpactInfoStruct BestImpact;
	//std::vector<ImpactInfoStruct> BestImpacts;
	float BestImpactDistance = -1.f;
	float Time = g::global_vars->realtime;
	//int Tick = g_GlobalVars->tickcount;
	//int LastTick = -1;
	for (auto info = saver.hitpositions.begin(); info != saver.hitpositions.end();)
	{
		if (Time > info->Time + (g::global_vars->interval_per_tick * 2))
		{
			info = saver.hitpositions.erase(info);
			continue;
		}

		Vector position = info->Pos;
		float Distance = EPosition.DistTo(position);

		BulletTracerInfoStruct binfo;
		binfo.DidHit = false;
		binfo.HitPos = info->Pos;
		binfo.ShotPos = info->ShotPos;
		binfo.Time = info->Time;
		//

		if (Distance < BestImpactDistance || BestImpactDistance == -1)
		{
			BestImpactDistance = Distance;
			BestImpact = *info;
			binfo.DidHit = true;
		}
		saver.BulletTracers.push_back(binfo);

		info++;
	}

	if (BestImpactDistance == -1)
	{
		return;
	}
}

void HitPossitionHelper::BulletImpact(IGameEvent* event)
{
	//1st
	c_base_player* shooter = GetPlayer(event->GetInt("userid"));

	if (!shooter || shooter != g::local_player)
	{
		return;
	}

	ImpactInfoStruct info;
	info.Pos = Vector(event->GetFloat("x"), event->GetFloat("y"), event->GetFloat("z"));
	info.ShotPos = g::local_player->m_vecOrigin() + g::local_player->m_vecViewOffset();
	info.Time = g::global_vars->realtime;
	info.Tick = g::global_vars->tickcount;
	saver.hitpositions.push_back(info);
}

c_base_player* HitPossitionHelper::GetPlayer(int i)
{
	return static_cast<c_base_player*>(g::entity_list->GetClientEntity(g::engine_client->GetPlayerForUserID(i)));
}

void HitPossitionHelper::ShotTracer(Vector shot_pos, Vector hit_pos)
{
	if (!g::local_player || !settings::visuals::bullet_tracer)
	{
		return;
	}

	Color clr = settings::visuals::colorBulletTracer;

	/*BeamInfo_t beamInfo;
	beamInfo.m_nType = TE_BEAMPOINTS;
	beamInfo.m_pszModelName = "sprites/physbeam.vmt";
	beamInfo.m_nModelIndex = -1;
	beamInfo.m_flHaloScale = 0.0f;
	beamInfo.m_flLife = 1.5f; //2.5f
	beamInfo.m_flWidth = 0.5f; //2.5f
	beamInfo.m_flEndWidth = 0.5f;
	beamInfo.m_flFadeLength = 0.0f;
	beamInfo.m_flAmplitude = 2.3f; // 3.0f
	beamInfo.m_flBrightness = settings::visuals::colorBulletTracer.a();
	beamInfo.m_flSpeed = 0.3f;
	beamInfo.m_nStartFrame = 0;
	beamInfo.m_flFrameRate = 0.f;
	beamInfo.m_flRed = settings::visuals::colorBulletTracer.r();
	beamInfo.m_flGreen = settings::visuals::colorBulletTracer.g();
	beamInfo.m_flBlue = settings::visuals::colorBulletTracer.b();
	beamInfo.m_nSegments = 2;
	beamInfo.m_bRenderable = true;
	beamInfo.m_nFlags = FBEAM_ONLYNOISEONCE | FBEAM_NOTILE | FBEAM_HALOBEAM;

	beamInfo.m_vecStart = shot_pos - Vector(0, -0.1f, 0.1f); // -0.1f, 0.1f
	beamInfo.m_vecEnd = hit_pos;*/

	auto start_pos = shot_pos;

	if (!start_pos.IsValid() || !hit_pos.IsValid() || start_pos.DistTo(hit_pos) < 0.1f)
		return;

	start_pos.z -= 4.f;

	BeamInfo_t beamInfo;
	beamInfo.m_nType = TE_BEAMPOINTS;
	beamInfo.m_pszModelName = "sprites/physbeam.vmt";
	beamInfo.m_nModelIndex = -1;
	beamInfo.m_flHaloScale = 0.0f;
	beamInfo.m_flLife = 1.f;
	beamInfo.m_flWidth = 2.0f;
	beamInfo.m_flEndWidth = 2.0f;
	beamInfo.m_flFadeLength = 0.0f;
	beamInfo.m_flAmplitude = 2.0f;
	beamInfo.m_flBrightness = 255.f;
	beamInfo.m_flSpeed = 0.2f;
	beamInfo.m_nStartFrame = 0;
	beamInfo.m_flFrameRate = 0.f;
	beamInfo.m_flRed = settings::visuals::colorBulletTracer.r();
	beamInfo.m_flGreen = settings::visuals::colorBulletTracer.g();
	beamInfo.m_flBlue = settings::visuals::colorBulletTracer.b();
	beamInfo.m_nSegments = 2;
	beamInfo.m_bRenderable = true;
	beamInfo.m_nFlags = FBEAM_FADEIN | FBEAM_FADEOUT;
	beamInfo.m_vecStart = start_pos;
	beamInfo.m_vecEnd = hit_pos;

	auto beam = g::view_render_beams->CreateBeamPoints(beamInfo);
	if (beam)
	{
		g::view_render_beams->DrawBeam(beam);
	}
}