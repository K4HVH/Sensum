#include "features.h"
#include "../valve_sdk/interfaces/IPrediction.hpp"

CMoveData m_MoveData;

namespace engine_prediction
{
	int* m_pPredictionRandomSeed = nullptr;

	float m_flOldCurtime;
	float m_flOldFrametime;

	int m_fFlags;
	int m_nMoveType;

	auto was_predicted = false;

	bool is_enabled()
	{
		return settings::movement::engine_prediction && g::local_player && g::local_player->IsAlive();
	}

	void start(CUserCmd* cmd)
	{
		was_predicted = is_enabled();
		if (!was_predicted)
			return;

		c_base_player* local = reinterpret_cast<c_base_player*>(g::entity_list->GetClientEntity(g::engine_client->GetLocalPlayer()));

		if (!g::engine_client->IsConnected() || !g::engine_client->IsInGame())
			return;

		if (!m_pPredictionRandomSeed)
			m_pPredictionRandomSeed = *(int**)(utils::pattern_scan(PREDICTION_RANDOM_SEED) + 2);

		*m_pPredictionRandomSeed = cmd->random_seed;

		m_flOldCurtime = g::global_vars->curtime;
		m_flOldFrametime = g::global_vars->frametime;

		g::global_vars->curtime = local->m_nTickBase() * g::global_vars->interval_per_tick;
		g::global_vars->frametime = g::global_vars->interval_per_tick;

		g::game_movement->StartTrackPredictionErrors(local);

		memset(&m_MoveData, 0, sizeof(m_MoveData));
		g::move_helper->SetHost(local);
		g::prediction->SetupMove(local, cmd, g::move_helper, &m_MoveData);
		g::game_movement->ProcessMovement(local, &m_MoveData);
		g::prediction->FinishMove(local, cmd, &m_MoveData);
	}

	void finish(CUserCmd* cmd)
	{
		if (!was_predicted)
			return;

		c_base_player* local = reinterpret_cast<c_base_player*>(g::entity_list->GetClientEntity(g::engine_client->GetLocalPlayer()));

		g::game_movement->FinishTrackPredictionErrors(local);
		g::move_helper->SetHost(0);

		*m_pPredictionRandomSeed = -1;

		g::global_vars->curtime = m_flOldCurtime;
		g::global_vars->frametime = m_flOldFrametime;
	}
}