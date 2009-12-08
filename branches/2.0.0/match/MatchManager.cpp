/* 
 * Copyright 2008, 2009 Nicolas Maingot
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

#include "MatchManager.h"

#include "BaseMatchState.h"
#include "../configuration/RunnableConfigurationFile.h"
#include "../plugin/ServerPlugin.h"
#include "../common/common.h"
#include "../messages/Countdown.h"
#include "../messages/I18nManager.h"
#include "../player/Player.h"
#include "../player/ClanMember.h"
#include "../sourcetv/TvRecord.h"
#include "../report/XmlReport.h"

#include <algorithm>

using namespace cssmatch;

using std::string;
using std::list;
using std::find_if;
using std::for_each;
using std::map;

MatchManager::MatchManager(BaseMatchState * iniState)
: initialState(iniState), currentState(NULL)
{
	if (iniState == NULL)
		throw MatchManagerException("Initial match state can't be NULL");

	listener = new EventListener<MatchManager>(this);

	setMatchState(iniState);
}


MatchManager::~MatchManager()
{
	Countdown::getInstance()->stop();

	delete listener;
}

MatchLignup * MatchManager::getLignup()
{
	return &lignup;
}

MatchInfo * MatchManager::getInfos()
{
	return &infos;
}

std::list<TvRecord *> * MatchManager::getRecords()
{
	return &records;
}

MatchStateId MatchManager::getInitialState() const
{
	return initialState->getId();
}

MatchClan * MatchManager::getClan(TeamCode code) throw(MatchManagerException)
{
	if (currentState != initialState)
	{
		MatchClan * clan = NULL;

		switch(code)
		{
		case T_TEAM:
			if (infos.setNumber & 1)
				clan = &lignup.clan1;
			else
				clan = &lignup.clan2;
			break;
		case CT_TEAM:
			if (infos.setNumber & 1)
				clan = &lignup.clan2;
			else
				clan = &lignup.clan1;	
			break;
		default:
			throw MatchManagerException("The plugin tried to grab a clan from an invalid team index");
		}

		return clan;
	}
	else
		throw MatchManagerException("No match in progress");
}

void MatchManager::player_disconnect(IGameEvent * event)
{
	// Announce any disconnection 

	ServerPlugin * plugin = ServerPlugin::getInstance();
	I18nManager * i18n = plugin->getI18nManager();

	RecipientFilter recipients;
	recipients.addAllPlayers();
	map<string, string> parameters;
	parameters["$username"] = event->GetString("name");
	parameters["$reason"] = event->GetString("reason");

	i18n->i18nChatSay(recipients,"player_leave_game",parameters);
}

void MatchManager::player_team(IGameEvent * event)
{
	// Search for any change in the team which requires a new clan name detection
	// i.e. n player to less than 2 players, 0 player to 1 player, 1 player to 2 players

	ServerPlugin * plugin = ServerPlugin::getInstance();
	ValveInterfaces * interfaces = plugin->getInterfaces();
	I18nManager * i18n = plugin->getI18nManager();

	TeamCode newSide = (TeamCode)event->GetInt("team");
	TeamCode oldSide = (TeamCode)event->GetInt("oldteam");

	int playercount = 0;
	TeamCode toReDetect = INVALID_TEAM;
	switch(newSide)
	{
	case T_TEAM:
		toReDetect = T_TEAM;
		playercount = plugin->getPlayerCount(T_TEAM) - 1;
		break;
	case CT_TEAM:
		toReDetect = CT_TEAM;
		playercount = plugin->getPlayerCount(CT_TEAM) - 1;
		break;
	}
	if ((toReDetect != INVALID_TEAM) && (playercount < 2))
			// "< 2" because the game does not immediatly update the player's team got via IPlayerInfo
			// And that's why we use a timer to redetect the clan's name
		plugin->addTimer(new ClanNameDetectionTimer(interfaces->gpGlobals->curtime+1.0f,toReDetect));

	toReDetect = INVALID_TEAM;
	switch(oldSide)
	{
	case T_TEAM:
		toReDetect = T_TEAM;
		playercount = plugin->getPlayerCount(T_TEAM);
		break;
	case CT_TEAM:
		toReDetect = CT_TEAM;
		playercount = plugin->getPlayerCount(CT_TEAM);
		break;
	}
	if ((toReDetect != INVALID_TEAM) && (playercount < 3)) // "< 3" see above
		plugin->addTimer(new ClanNameDetectionTimer(interfaces->gpGlobals->curtime+1.0f,toReDetect));
}

void MatchManager::player_changename(IGameEvent * event)
{
	// Retect the corresponfing clan name if needed
	// i.e. if the player is alone is his team

	ServerPlugin * plugin = ServerPlugin::getInstance();
	ValveInterfaces * interfaces = plugin->getInterfaces();
	I18nManager * i18n = plugin->getI18nManager();

	list<ClanMember *> * playerlist = plugin->getPlayerlist();
	list<ClanMember *>::const_iterator itPlayer = playerlist->begin();
	list<ClanMember *>::const_iterator invalidPlayer = playerlist->end();

	list<ClanMember *>::const_iterator thisPlayer = 
		find_if(itPlayer,invalidPlayer,PlayerHavingUserid(event->GetInt("userid")));
	if (thisPlayer != invalidPlayer)
	{
		TeamCode playerteam = (*thisPlayer)->getMyTeam();
		if ((playerteam > SPEC_TEAM) && (plugin->getPlayerCount(playerteam) == 1))
			plugin->addTimer(new ClanNameDetectionTimer(interfaces->gpGlobals->curtime+1.0f,playerteam));
	}
}

void MatchManager::detectClanName(TeamCode code) throw(MatchManagerException)
{
	/*ServerPlugin * plugin = ServerPlugin::getInstance();
	I18nManager * i18n = plugin->getI18nManager();*/

	if (currentState != initialState)
	{
		try
		{
			getClan(code)->detectClanName();

			/*RecipientFilter recipients;
			recipients.addAllPlayers();

			i18n->i18nChatSay(recipients,"match_retag");

			map<string,string> parameters;
			parameters["$team1"] = *(lignup.clan1.getName());
			parameters["$team2"] = *(lignup.clan2.getName());
			i18n->i18nChatSay(recipients,"match_name",0,parameters);*/


			updateHostname();
		}
		catch(const MatchManagerException & e)
		{
			printException(e,__FILE__,__LINE__);
		}
	}
	else
		throw MatchManagerException("No match in progress");
}

void MatchManager::updateHostname()
{
	ServerPlugin * plugin = ServerPlugin::getInstance();
	try
	{
		ConVar * hostname = plugin->getConVar("cssmatch_hostname");
		string newHostname = hostname->GetString();

		// Replace %s by the clan names
		size_t clanNameSlot = newHostname.find("%s");
		if (clanNameSlot != string::npos)
		{
			const string * clan1Name = lignup.clan1.getName();
			newHostname.replace(clanNameSlot,2,*clan1Name,0,clan1Name->size());
		}

		clanNameSlot = newHostname.find("%s");
		if (clanNameSlot != string::npos)
		{
			const std::string * clan2Name = lignup.clan2.getName();
			newHostname.replace(clanNameSlot,2,*clan2Name,0,clan2Name->size());
		}

		// Set the new hostname
		hostname->SetValue(newHostname.c_str());
	}
	catch(const ServerPluginException & e)
	{
		printException(e,__FILE__,__LINE__);
	}

}

void MatchManager::setMatchState(BaseMatchState * newState) throw(MatchManagerException)
{
	if (newState == NULL)
		throw MatchManagerException("The news state can't be NULL");

	if (currentState != NULL) // currentState is initialized to NULL
		currentState->endState();

	currentState = newState;
	currentState->startState();
}

MatchStateId MatchManager::getMatchState() const
{
	return currentState->getId();
}

void MatchManager::start(RunnableConfigurationFile & config, bool warmup, BaseMatchState * state)
	 throw(MatchManagerException)
{
	// Make sure there is not already a match in progress
	if (currentState == initialState)
	{
		ServerPlugin * plugin = ServerPlugin::getInstance();
		ValveInterfaces * interfaces = plugin->getInterfaces();
		I18nManager * i18n = plugin->getI18nManager();

		// Global recipient list
		RecipientFilter recipients;
		recipients.addAllPlayers();

		// Update match infos
		infos.setNumber = 1;
		infos.roundNumber = 1;

		// Reset the stats of all players
		list<ClanMember *> * playerlist = plugin->getPlayerlist();
		for_each(playerlist->begin(),playerlist->end(),ResetStats());

		// Cancel any timers in progress
		plugin->removeTimers();

		// Execute the configuration file
		config.execute();

		// Print the plugin list to the server log
		plugin->queueCommand("plugin_print\n");

		// Save the current date
		infos.startTime = *getLocalTime();

		// Start to listen some events
		listener->addCallback("player_disconnect",&MatchManager::player_disconnect);
		listener->addCallback("player_team",&MatchManager::player_team);
		listener->addCallback("player_changename",&MatchManager::player_changename);

		try
		{
			// Monitor some variable
			plugin->addTimer(
				new ConVarMonitorTimer(
					interfaces->gpGlobals->curtime + 1.0f,plugin->getConVar("sv_alltalk"),"0","sv_alltalk"));
			plugin->addTimer(
				new ConVarMonitorTimer(
					interfaces->gpGlobals->curtime + 1.0f,plugin->getConVar("sv_cheats"),"0","sv_cheats"));

			// Set the new server password
			string password = plugin->getConVar("cssmatch_password")->GetString();
			plugin->getConVar("sv_password")->SetValue(password.c_str());

			map<string, string> parameters;
			parameters["$password"] = password;
			plugin->addTimer(
				new TimerI18nPopupSay(
					interfaces->gpGlobals->curtime+5.0f,recipients,"match_password_popup",5,parameters));

			// Maybe no warmup is needed
			infos.warmup = warmup;

			// Start the first state
			setMatchState(state);

			// Try to find the clan names
			detectClanName(T_TEAM);
			detectClanName(CT_TEAM);
		}
		catch(const ServerPluginException & e)
		{
			printException(e,__FILE__,__LINE__);
		}
	}
	else
		throw MatchManagerException("There is already a match in progress");
}

void MatchManager::stop() throw (MatchManagerException)
{
	// Make sure there is a match in progress
	if (currentState != initialState)
	{
		ServerPlugin * plugin = ServerPlugin::getInstance();
		ValveInterfaces * interfaces = plugin->getInterfaces();
		I18nManager * i18n = plugin->getI18nManager();

		// Send all the announcements
		RecipientFilter recipients;
		recipients.addAllPlayers();

		i18n->i18nChatSay(recipients,"match_end");

		const string * tagClan1 = lignup.clan1.getName();
		ClanStats * clan1Stats = lignup.clan1.getStats();
		int clan1Score = clan1Stats->scoreCT + clan1Stats->scoreT;
		const string * tagClan2 = lignup.clan2.getName();
		ClanStats * clan2Stats = lignup.clan2.getStats();
		int clan2Score = clan2Stats->scoreCT + clan2Stats->scoreT;

		map<string,string> parameters;
		parameters["$team1"] = *tagClan1;
		parameters["$score1"] = toString(clan1Score);
		parameters["$team2"] = *tagClan2;
		parameters["$score2"] = toString(clan2Score);
		i18n->i18nPopupSay(recipients,"match_end_popup",6,parameters);
		//i18n->i18nConsoleSay(recipients,"match_end_popup",parameters);

		map<string,string> parametersWinner;
		if (clan1Score > clan2Score)
		{
			parametersWinner["$team"] = *tagClan1;
			i18n->i18nChatSay(recipients,"match_winner",parametersWinner);
		}
		else if (clan1Score < clan2Score)
		{
			parametersWinner["$team"] = *tagClan2;
			i18n->i18nChatSay(recipients,"match_winner",parametersWinner);
		}
		else
		{
			i18n->i18nChatSay(recipients,"match_no_winner");
		}

		// Stop all event listeners
		listener->removeCallbacks();

		// Stop to monitor the ConVars monitored
		plugin->removeTimers();

		// Remove any old tv record
		for_each(records.begin(),records.end(),TvRecordToRemove());
		records.clear();

		// Write the report
		if (plugin->getConVar("cssmatch_report")->GetBool())
		{
			XmlReport report(this);
			report.write();
		}

		// Return to the initial state
		setMatchState(initialState);

		// Do a time break before returning to the initial configuration
		int breakDuration = plugin->getConVar("cssmatch_end_set")->GetInt();
		if (breakDuration > 0)
		{
			map<string,string> parametersBreak;
			parametersBreak["$time"] = toString(breakDuration);
			Countdown::getInstance()->fire(breakDuration);
			plugin->addTimer(
				new TimerI18nChatSay(	interfaces->gpGlobals->curtime + 2.0f,
										recipients,
										"match_dead_time",
										parametersBreak));
			plugin->addTimer(new RestoreConfigTimer(interfaces->gpGlobals->curtime + breakDuration + 2.0f));
		}
	}
	else
		throw MatchManagerException("No match in progress");
}

void MatchManager::restartRound() throw (MatchManagerException)
{
	if (currentState != initialState)
	{
		ServerPlugin * plugin = ServerPlugin::getInstance(); 

		// Remove all player stats related to this round
		list<ClanMember *> * playerlist = plugin->getPlayerlist();
		list<ClanMember *>::iterator itPlayer = playerlist->begin();
		list<ClanMember *>::iterator invalidPlayer = playerlist->end();
		while(itPlayer != invalidPlayer)
		{
			PlayerStats * currentStats = (*itPlayer)->getCurrentStats();
			PlayerStats * lastRoundStats = (*itPlayer)->getLastRoundStats();
			currentStats->deaths = lastRoundStats->deaths;
			currentStats->kills = lastRoundStats->kills;

			// TODO: Restore the kills/deaths in the scoreboard ?

			itPlayer++;
		}

		// Back to the last round (round_start will maybe increment that)
		infos.roundNumber -= 1; // FIXME: In cssmatch1, if the first round was restarted, 3 restarts were scheduled

		// Do the restart
		plugin->queueCommand("mp_restartgame 1\n");
	}
	else
		throw MatchManagerException("No match in progress");
}

void MatchManager::restartSet() throw (MatchManagerException)
{
	if (currentState != initialState)
	{
		ServerPlugin * plugin = ServerPlugin::getInstance();

		// Restore the score of each player
		list<ClanMember *> * playerlist = plugin->getPlayerlist();
		list<ClanMember *>::iterator itPlayer = playerlist->begin();
		list<ClanMember *>::iterator invalidPlayer = playerlist->end();
		while(itPlayer != invalidPlayer)
		{
			PlayerStats * currentStats = (*itPlayer)->getCurrentStats();
			PlayerStats * lastSetStats = (*itPlayer)->getLastSetStats();
			currentStats->deaths = lastSetStats->deaths;
			currentStats->kills = lastSetStats->kills;

			// TODO: Restore the kills/deaths in the scoreboard ?

			itPlayer++;
		}

		// Restore the score of each clan
		MatchClan * clanT = getClan(T_TEAM);
		ClanStats * currentStatsClanT = clanT->getStats();
		ClanStats * lastSetStatsClanT = clanT->getLastSetStats();
		currentStatsClanT->scoreT = lastSetStatsClanT->scoreT;

		MatchClan * clanCT = getClan(CT_TEAM);
		ClanStats * currentStatsClanCT = clanCT->getStats();
		ClanStats * lastSetStatsClanCT = clanCT->getLastSetStats();
		currentStatsClanCT->scoreCT = lastSetStatsClanCT->scoreCT;

		// Back to the first round (round_start will maybe increment that)
		infos.roundNumber = 0;

		// Do the restart
		plugin->queueCommand("mp_restartgame 1\n");
	}
	else
		throw MatchManagerException("No match in progress");
}


ClanNameDetectionTimer::ClanNameDetectionTimer(float date, TeamCode teamCode)
	: BaseTimer(date), team(teamCode)
{
}

void ClanNameDetectionTimer::execute()
{
	ServerPlugin * plugin = ServerPlugin::getInstance(); 
	MatchManager * manager = plugin->getMatch();
	manager->detectClanName(team);
}

RestoreConfigTimer::RestoreConfigTimer(float date) : BaseTimer(date)
{
}

void RestoreConfigTimer::execute()
{
	ServerPlugin * plugin = ServerPlugin::getInstance(); 
	string configPatch = DEFAULT_CONFIGURATION_FILE;
	try
	{
		configPatch = plugin->getConVar("cssmatch_default_config")->GetString();

		RunnableConfigurationFile config(string(CFG_FOLDER_PATH) + configPatch);
		config.execute();
	}
	catch(const ServerPluginException & e)
	{
		printException(e,__FILE__,__LINE__);
	}
	catch(const ConfigurationFileException & e)
	{
		I18nManager * i18n = plugin->getI18nManager();

		RecipientFilter recipients;
		recipients.addAllPlayers();

		map<string,string> parameters;
		parameters["$file"] = configPatch;
		i18n->i18nChatWarning(recipients,"error_file_not_found",parameters);
		i18n->i18nChatWarning(recipients,"match_please_changelevel");
	}
}

ConVarMonitorTimer::ConVarMonitorTimer(	float date,
										ConVar * varToWatch,
										const string & expectedValue,
										const string & warningMessage)
	: BaseTimer(date), toWatch(varToWatch), value(expectedValue), message(warningMessage) 
{
}

void ConVarMonitorTimer::execute()
{
	ServerPlugin * plugin = ServerPlugin::getInstance();
	ValveInterfaces * interfaces = plugin->getInterfaces();

	if (toWatch->GetString() != value)
	{
		I18nManager * i18n = plugin->getI18nManager();

		RecipientFilter recipients;
		recipients.addAllPlayers();

		i18n->i18nCenterSay(recipients,message);
	}

	plugin->addTimer(new ConVarMonitorTimer(interfaces->gpGlobals->curtime + 1.0f,toWatch,value,message));
}
