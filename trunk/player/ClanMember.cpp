/* 
 * Copyright 2008-2010 Nicolas Maingot
 * 
 * This file is part of CSSMatch.
 * 
 * CSSMatch is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * 
 * CSSMatch is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with CSSMatch; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Portions of this code are also Copyright � 1996-2005 Valve Corporation, All rights reserved
 */

#include "ClanMember.h"
#include "../plugin/ServerPlugin.h"

using namespace cssmatch;
using std::list;
using std::string;

EntityProp ClanMember::ownerHandler("CBaseCombatWeapon","m_hOwner");

ClanMember::ClanMember(int index, bool ref) : Player(index), referee(ref)
{
}

PlayerState * ClanMember::getLastRoundState()
{
	return &lastRoundState;
}

PlayerState * ClanMember::getLastHalfState()
{
	return &lastHalfState;
}

PlayerScore * ClanMember::getCurrentScore()
{
	return &currentScore;
}

void ClanMember::saveState(PlayerState * state)
{
	ServerPlugin * plugin = ServerPlugin::getInstance();
	ValveInterfaces * interfaces = plugin->getInterfaces();

	// Get the handle id of this player
	IHandleEntity * handle = identity.pEntity->GetNetworkable()->GetEntityHandle();
	const CBaseHandle & bHandle = handle->GetRefEHandle();
	int handleid = bHandle.ToInt();

	state->objects.clear();

	// FIXME: Ouch! (playercount * entitycount) iterations!
	for (int i = interfaces->gpGlobals->maxClients + 1; i < interfaces->engine->GetEntityCount(); i++)
	{
		edict_t * entity = interfaces->engine->PEntityOfEntIndex(i);
		if (isValidEntity(entity))
		{
			int tempHandleid = entity->GetNetworkable()->GetEntityHandle()->GetRefEHandle().ToInt();

			if ((strstr(entity->GetClassName(),"item_") != NULL) ||
				(strstr(entity->GetClassName(),"weapon_") != NULL))
			{
				if (ownerHandler.getProp<int>(entity) == handleid)
				{
					state->objects.push_back(entity->GetClassName());
				}
			}
		}
	}

	state->score.deaths = currentScore.deaths;
	state->score.kills = currentScore.kills;

	state->armor = getArmor();
	state->account = getAccount();

	//state->vecOrigin = getVecOrigin();
	//state->angle = getViewAngle();
}

void ClanMember::restoreState(PlayerState * state)
{
	currentScore.deaths = state->score.deaths;
	currentScore.kills = state->score.kills;

	if (state->armor > 0)
		setArmor(state->armor);

	if (state->account > -1)
		setAccount(state->account);

	list<string>::const_iterator itObject;
	for(itObject = state->objects.begin(); itObject != state->objects.end(); itObject++)
	{
		give(*itObject);
	}

	/*if (state->vecOrigin.IsValid())
		setVecOrigin(state->vecOrigin);*/

	/*if (state->angle.IsValid())
		setang(state->angle);*/

	// TODO: Restore the kills/deaths in the scoreboard ?			
}

bool ClanMember::isReferee() const
{
	return referee;
}

void ClanMember::setReferee(bool isReferee)
{
	referee = isReferee;
}