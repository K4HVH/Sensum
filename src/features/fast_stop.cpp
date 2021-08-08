#include "../settings/settings.h"
#include "../valve_sdk/csgostructs.hpp"
#include "../helpers/math.h"

namespace features
{
	void fast_stop(CUserCmd* cmd)
	{
		if (settings::misc::fast_stop_mode == 0)
			return;

		Vector velocity = g::local_player->m_vecVelocity();
		QAngle direction;
		math::vector2angles(velocity, direction);
		float speed = velocity.Length2D();

		direction.yaw = cmd->viewangles.yaw - direction.yaw;

		Vector forward;
		math::angle2vectors(direction, forward);

		Vector right = (forward + 0.217812) * -speed;
		Vector left = (forward + -0.217812) * -speed;

		Vector move_forward = (forward + 0.217812) * -speed;
		Vector move_backward = (forward + -0.217812) * -speed;

		if (settings::misc::fast_stop_mode == 1 || settings::misc::fast_stop_mode == 3 && settings::misc::fast_stop_mode != 2) // Only left & right or both
		{
			if (!(cmd->buttons & IN_MOVELEFT))
			{
				cmd->sidemove += +left.y;
			}

			if (!(cmd->buttons & IN_MOVERIGHT))
			{
				cmd->sidemove -= -right.y;
			}
		}

		if (settings::misc::fast_stop_mode == 2 || settings::misc::fast_stop_mode == 3 && settings::misc::fast_stop_mode != 1)  // Only forward & backward or both
		{
			if (!(cmd->buttons & IN_FORWARD))
			{
				if (cmd->buttons & IN_MOVELEFT || cmd->buttons & IN_MOVERIGHT || settings::misc::auto_strafe || !(g::local_player->m_fFlags() & FL_ONGROUND))
					return;

				cmd->forwardmove += +move_forward.x;
			}

			if (!(cmd->buttons & IN_BACK))
			{
				if (cmd->buttons & IN_MOVELEFT || cmd->buttons & IN_MOVERIGHT || settings::misc::auto_strafe || !(g::local_player->m_fFlags() & FL_ONGROUND))
					return;

				cmd->forwardmove -= -move_backward.x;
			}
		}
	}
}