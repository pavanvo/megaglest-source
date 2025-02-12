// ==============================================================
//	This file is part of Glest (www.glest.org)
//
//	Copyright (C) 2001-2008 Martiño Figueroa
//
//	You can redistribute this code and/or modify it under
//	the terms of the GNU General Public License as published
//	by the Free Software Foundation; either version 2 of the
//	License, or (at your option) any later version
// ==============================================================

#include "commander.h"

#include "world.h"
#include "unit.h"
#include "conversion.h"
#include "upgrade.h"
#include "command.h"
#include "command_type.h"
#include "network_manager.h"
#include "console.h"
#include "config.h"
#include "platform_util.h"
#include "game.h"
#include "game_settings.h"
#include "game.h"

using namespace Shared::Graphics;
using namespace Shared::Util;
using namespace Shared::Platform;

namespace Glest{ namespace Game{

// =====================================================
// 	class Commander
// =====================================================
Commander::Commander() {
	this->world					= NULL;
	this->pauseNetworkCommands 	= false;
}

Commander::~Commander() {
}

void Commander::init(World *world){
	this->world= world;
}

bool Commander::canSubmitCommandType(const Unit *unit, const CommandType *commandType) const {
	bool canSubmitCommand=true;
	const MorphCommandType *mct = dynamic_cast<const MorphCommandType*>(commandType);
	if(mct && unit->getCommandSize() > 0) {
		Command *cur_command= unit->getCurrCommand();
		if(cur_command != NULL) {
			const MorphCommandType *cur_mct= dynamic_cast<const MorphCommandType*>(cur_command->getCommandType());
			if(cur_mct && unit->getCurrSkill() && unit->getCurrSkill()->getClass() == scMorph) {
				const UnitType *morphUnitType		= mct->getMorphUnit();
				const UnitType *cur_morphUnitType	= cur_mct->getMorphUnit();

				if(morphUnitType != NULL && cur_morphUnitType != NULL && morphUnitType->getId() == cur_morphUnitType->getId()) {
					canSubmitCommand = false;
				}
			}
		}
	}
	return canSubmitCommand;
}

std::pair<CommandResult,string> Commander::tryGiveCommand(const Selection *selection, const CommandType *commandType,
									const Vec2i &pos, const UnitType* unitType,
									CardinalDir facing, bool tryQueue,Unit *targetUnit) const {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	if(this->pauseNetworkCommands == true) {
		return std::pair<CommandResult,string>(crFailUndefined,"");
	}

	std::pair<CommandResult,string> result(crFailUndefined,"");
	if(selection->isEmpty() == false && commandType != NULL) {
		Vec2i refPos;
		CommandResultContainer results;

		refPos = world->getMap()->computeRefPos(selection);

		const Unit *builderUnit = world->getMap()->findClosestUnitToPos(selection, pos, unitType);

		int builderUnitId 					= builderUnit->getId();
		CommandStateType commandStateType 	= cst_None;
		int commandStateValue 				= -1;

		int unitCommandGroupId = -1;
		if(selection->getCount() > 1) {
			unitCommandGroupId = world->getNextCommandGroupId();
		}

		//give orders to all selected units
		for(int i = 0; i < selection->getCount(); ++i) {
			const Unit *unit = selection->getUnit(i);


			std::pair<CommandResult,string> resultCur(crFailUndefined,"");
			bool canSubmitCommand = canSubmitCommandType(unit, commandType);
			if(canSubmitCommand == true) {
				int unitId= unit->getId();
				Vec2i currPos= world->getMap()->computeDestPos(refPos, unit->getPosNotThreadSafe(), pos);

				Vec2i usePos = currPos;
				const CommandType *useCommandtype = commandType;
				if(dynamic_cast<const BuildCommandType *>(commandType) != NULL) {
					usePos = pos;
					if(builderUnit->getId() != unitId) {
						useCommandtype 		= unit->getType()->getFirstRepairCommand(unitType);
						commandStateType 	= cst_linkedUnit;
						commandStateValue 	= builderUnitId;
					}
					else {
						commandStateType 	= cst_None;
						commandStateValue 	= -1;
					}
				}

				if(useCommandtype != NULL) {
					auto mct= unit->getCurrMorphCt();
					int nextUnitTypeId= mct? mct->getMorphUnit()->getId(): -1;
					NetworkCommand networkCommand(this->world,nctGiveCommand, unitId,
							useCommandtype->getId(), usePos, unitType->getId(), nextUnitTypeId,
							(targetUnit != NULL ? targetUnit->getId() : -1), 
							facing, tryQueue, commandStateType,commandStateValue,
							unitCommandGroupId);

					//every unit is ordered to a the position
					resultCur= pushNetworkCommand(&networkCommand);
				}
			}

			results.push_back(resultCur);
		}

		return computeResult(results);
	}
	return std::pair<CommandResult,string>(crFailUndefined,"");
}

std::pair<CommandResult,string> Commander::tryGiveCommand(const Unit* unit, const CommandType *commandType,
									const Vec2i &pos, const UnitType* unitType,
									CardinalDir facing, bool tryQueue,Unit *targetUnit,
									int unitGroupCommandId) const {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	if(this->pauseNetworkCommands == true) {
		return std::pair<CommandResult,string>(crFailUndefined,"");
	}

	Chrono chrono;
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled) chrono.start();

	assert(this->world != NULL);
	assert(unit != NULL);
	assert(commandType != NULL);
	assert(unitType != NULL);

	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s] Line: %d took msecs: %lld\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());

	std::pair<CommandResult,string> result(crFailUndefined,"");
	bool canSubmitCommand=canSubmitCommandType(unit, commandType);
	if(canSubmitCommand == true) {
		auto mct= unit->getCurrMorphCt();
		int nextUnitTypeId= mct? mct->getMorphUnit()->getId(): -1;
		NetworkCommand networkCommand(this->world,nctGiveCommand, unit->getId(),
									commandType->getId(), pos, unitType->getId(), nextUnitTypeId,
									(targetUnit != NULL ? targetUnit->getId() : -1),
									facing, tryQueue,cst_None,-1,unitGroupCommandId);

		if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s] Line: %d took msecs: %lld\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());

		result = pushNetworkCommand(&networkCommand);
	}

	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s] Line: %d took msecs: %lld\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());

	return result;
}

std::pair<CommandResult,string> Commander::tryGiveCommand(const Selection *selection, CommandClass commandClass,
		const Vec2i &pos, const Unit *targetUnit, bool tryQueue) const{

	if(this->pauseNetworkCommands == true) {
		return std::pair<CommandResult,string>(crFailUndefined,"");
	}

	std::pair<CommandResult,string> result(crFailUndefined,"");
	if(selection->isEmpty() == false) {
		Vec2i refPos, currPos;
		CommandResultContainer results;

		refPos= world->getMap()->computeRefPos(selection);

		int unitCommandGroupId = -1;
		if(selection->getCount() > 1) {
			unitCommandGroupId = world->getNextCommandGroupId();
		}

		//give orders to all selected units
		for(int i = 0; i < selection->getCount(); ++i) {
			const Unit *unit= selection->getUnit(i);
			const CommandType *ct= unit->getType()->getFirstCtOfClass(commandClass);
			if(ct != NULL) {
				std::pair<CommandResult,string> resultCur(crFailUndefined,"");

				bool canSubmitCommand=canSubmitCommandType(unit, ct);
				if(canSubmitCommand == true) {

					int targetId= targetUnit==NULL? Unit::invalidId: targetUnit->getId();
					int unitId= selection->getUnit(i)->getId();
					Vec2i currPos= world->getMap()->computeDestPos(refPos,
							selection->getUnit(i)->getPosNotThreadSafe(), pos);
					auto mct= unit->getCurrMorphCt();
					int nextUnitTypeId= mct? mct->getMorphUnit()->getId(): -1;
					NetworkCommand networkCommand(this->world,nctGiveCommand,
							unitId, ct->getId(), currPos, -1, nextUnitTypeId, targetId, -1,
							tryQueue,cst_None,-1,unitCommandGroupId);

					//every unit is ordered to a different pos
					resultCur= pushNetworkCommand(&networkCommand);
				}
				results.push_back(resultCur);
			}
			else{
				results.push_back(std::pair<CommandResult,string>(crFailUndefined,""));
			}
		}
		return computeResult(results);
	}
	else {
		return std::pair<CommandResult,string>(crFailUndefined,"");
	}
}

std::pair<CommandResult,string> Commander::tryGiveCommand(const Selection *selection,
						const CommandType *commandType, const Vec2i &pos,
						const Unit *targetUnit, bool tryQueue) const {

	if(this->pauseNetworkCommands == true) {
		return std::pair<CommandResult,string>(crFailUndefined,"");
	}

	std::pair<CommandResult,string> result(crFailUndefined,"");

	if(!selection->isEmpty() && commandType!=NULL){
		Vec2i refPos;
		CommandResultContainer results;

		refPos= world->getMap()->computeRefPos(selection);

		int unitCommandGroupId = -1;
		if(selection->getCount() > 1) {
			unitCommandGroupId = world->getNextCommandGroupId();
		}

		//give orders to all selected units
		for(int i = 0; i < selection->getCount(); ++i) {
			const Unit *unit = selection->getUnit(i);
			assert(unit != NULL);

			std::pair<CommandResult,string> resultCur(crFailUndefined,"");

			bool canSubmitCommand=canSubmitCommandType(unit, commandType);
			if(canSubmitCommand == true) {
				int targetId= targetUnit==NULL? Unit::invalidId: targetUnit->getId();
				int unitId= unit->getId();
				Vec2i currPos= world->getMap()->computeDestPos(refPos, unit->getPosNotThreadSafe(), pos);
				auto mct= unit->getCurrMorphCt();
				int nextUnitTypeId= mct? mct->getMorphUnit()->getId(): -1;

				NetworkCommand networkCommand(this->world,nctGiveCommand, unitId,
						commandType->getId(), currPos, -1, nextUnitTypeId, targetId, -1, tryQueue,
						cst_None, -1, unitCommandGroupId);

				//every unit is ordered to a different position
				resultCur= pushNetworkCommand(&networkCommand);
			}
			results.push_back(resultCur);
		}

		return computeResult(results);
	}
	else{
		return std::pair<CommandResult,string>(crFailUndefined,"");
	}
}

//auto command
std::pair<CommandResult,string> Commander::tryGiveCommand(const Selection *selection, const Vec2i &pos,
		const Unit *targetUnit, bool tryQueue, int unitCommandGroupId) const {

	if(this->pauseNetworkCommands == true) {
		return std::pair<CommandResult,string>(crFailUndefined,"");
	}

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	std::pair<CommandResult,string> result(crFailUndefined,"");

	if(selection->isEmpty() == false){
		Vec2i refPos, currPos;
		CommandResultContainer results;

		if(unitCommandGroupId == -1 && selection->getCount() > 1) {
			unitCommandGroupId = world->getNextCommandGroupId();
		}

		//give orders to all selected units
		refPos= world->getMap()->computeRefPos(selection);
		for(int i=0; i < selection->getCount(); ++i) {
			//every unit is ordered to a different pos
			const Unit *unit = selection->getUnit(i);
			assert(unit != NULL);

			currPos= world->getMap()->computeDestPos(refPos, unit->getPosNotThreadSafe(), pos);

			//get command type
			auto mct= unit->getCurrMorphCt();
			auto nextUnitType= mct? mct->getMorphUnit(): NULL;
			const CommandType *commandType= unit->computeCommandType(pos, targetUnit, nextUnitType);

			//give commands
			if(commandType != NULL) {
				int targetId= targetUnit==NULL? Unit::invalidId: targetUnit->getId();
				int unitId= unit->getId();
				int nextUnitTypeId= nextUnitType? nextUnitType->getId(): -1;

				std::pair<CommandResult,string> resultCur(crFailUndefined,"");

				bool canSubmitCommand=canSubmitCommandType(unit, commandType);
				if(canSubmitCommand == true) {
					NetworkCommand networkCommand(this->world,nctGiveCommand,
							unitId, commandType->getId(), currPos, -1, nextUnitTypeId, targetId,
							-1, tryQueue, cst_None, -1, unitCommandGroupId);
					resultCur= pushNetworkCommand(&networkCommand);
				}
				results.push_back(resultCur);
			}
			else if(unit->isMeetingPointSettable() == true) {
				NetworkCommand command(this->world,nctSetMeetingPoint,
						unit->getId(), -1, currPos,-1,-1,-1,-1,false,
						cst_None,-1,unitCommandGroupId);

				std::pair<CommandResult,string> resultCur= pushNetworkCommand(&command);
				results.push_back(resultCur);
			}
			else {
				results.push_back(std::pair<CommandResult,string>(crFailUndefined,""));
			}
		}
		result = computeResult(results);
	}

	return result;
}

CommandResult Commander::tryCancelCommand(const Selection *selection) const {
	if(this->pauseNetworkCommands == true) {
		return crFailUndefined;
	}

	int unitCommandGroupId = -1;
	if(selection->getCount() > 1) {
		unitCommandGroupId = world->getNextCommandGroupId();
	}

	for(int i = 0; i < selection->getCount(); ++i) {
		NetworkCommand command(this->world,nctCancelCommand,
				selection->getUnit(i)->getId(),-1,Vec2i(0),-1,-1,-1,-1,false,
				cst_None,-1,unitCommandGroupId);
		pushNetworkCommand(&command);
	}

	return crSuccess;
}

void Commander::trySetMeetingPoint(const Unit* unit, const Vec2i &pos) const {
	if(this->pauseNetworkCommands == true) {
		return;
	}

	NetworkCommand command(this->world,nctSetMeetingPoint, unit->getId(), -1, pos);
	pushNetworkCommand(&command);
}

void Commander::trySwitchTeam(const Faction* faction, int teamIndex) const {
	if(this->pauseNetworkCommands == true) {
		return;
	}

	NetworkCommand command(this->world,nctSwitchTeam, faction->getIndex(), teamIndex);
	pushNetworkCommand(&command);
}

void Commander::trySwitchTeamVote(const Faction* faction, SwitchTeamVote *vote) const {
	if(this->pauseNetworkCommands == true) {
		return;
	}

	NetworkCommand command(this->world,nctSwitchTeamVote, faction->getIndex(), vote->factionIndex,Vec2i(0),vote->allowSwitchTeam);
	pushNetworkCommand(&command);
}

void Commander::tryDisconnectNetworkPlayer(const Faction* faction, int playerIndex) const {
	NetworkCommand command(this->world,nctDisconnectNetworkPlayer, faction->getIndex(), playerIndex);
	pushNetworkCommand(&command);
}

void Commander::tryPauseGame(bool joinNetworkGame, bool clearCaches) const {
	NetworkCommand command(this->world,nctPauseResume,1);
	command.commandTypeId = (clearCaches == true ? 1 : 0);
	command.unitTypeId = (joinNetworkGame == true ? 1 : 0);
	pushNetworkCommand(&command);
}

void Commander::tryResumeGame(bool joinNetworkGame, bool clearCaches) const {
	NetworkCommand command(this->world,nctPauseResume,0);
	command.commandTypeId = (clearCaches == true ? 1 : 0);
	command.unitTypeId = (joinNetworkGame == true ? 1 : 0);
	pushNetworkCommand(&command);
}

void Commander::tryNetworkPlayerDisconnected(int factionIndex) const {
	//printf("tryNetworkPlayerDisconnected factionIndex: %d\n",factionIndex);

	//if(this->pauseNetworkCommands == true) {
	//	return;
	//}

	NetworkCommand command(this->world,nctPlayerStatusChange, factionIndex, npst_Disconnected);
	pushNetworkCommand(&command);
}

// ==================== PRIVATE ====================

std::pair<CommandResult,string> Commander::computeResult(const CommandResultContainer &results) const {
	std::pair<CommandResult,string> result(crFailUndefined,"");
	switch(results.size()) {
        case 0:
            return std::pair<CommandResult,string>(crFailUndefined,"");
        case 1:
            return results.front();
        default:
            for(int i = 0; i < (int)results.size(); ++i) {
                if(results[i].first != crSuccess) {
                	return std::pair<CommandResult,string>(crSomeFailed,results[i].second);
                }
            }
            break;
	}
	return std::pair<CommandResult,string>(crSuccess,"");
}

std::pair<CommandResult,string> Commander::pushNetworkCommand(const NetworkCommand* networkCommand) const {
	GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
	std::pair<CommandResult,string> result(crSuccess,"");

	//validate unit
	const Unit* unit = NULL;
	if( networkCommand->getNetworkCommandType() != nctSwitchTeam &&
		networkCommand->getNetworkCommandType() != nctSwitchTeamVote &&
		networkCommand->getNetworkCommandType() != nctPauseResume &&
		networkCommand->getNetworkCommandType() != nctPlayerStatusChange &&
		networkCommand->getNetworkCommandType() != nctDisconnectNetworkPlayer) {
		unit= world->findUnitById(networkCommand->getUnitId());
		if(unit == NULL) {
			char szBuf[8096]="";
			snprintf(szBuf,8096,"In [%s::%s - %d] Command refers to non existent unit id = %d. Game out of synch.",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,networkCommand->getUnitId());
			GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
			if(gameNetworkInterface != NULL) {
				char szMsg[8096]="";
				snprintf(szMsg,8096,"Player detected an error: Command refers to non existent unit id = %d. Game out of synch.",networkCommand->getUnitId());
				gameNetworkInterface->sendTextMessage(szMsg,-1, true, "");
			}
			throw megaglest_runtime_error(szBuf);
		}
	}

	//add the command to the interface
	gameNetworkInterface->requestCommand(networkCommand);

	//calculate the result of the command
	if(unit != NULL && networkCommand->getNetworkCommandType() == nctGiveCommand) {
		//printf("In [%s::%s Line: %d] result.first = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,result.first);
		Command* command= buildCommand(networkCommand);
		result= unit->checkCommand(command);
		delete command;
	}
	return result;
}

void Commander::signalNetworkUpdate(Game *game) {
    updateNetwork(game);
}

bool Commander::getReplayCommandListForFrame(int worldFrameCount) {
	bool haveReplyCommands = false;
	if(replayCommandList.empty() == false) {
		if(SystemFlags::VERBOSE_MODE_ENABLED) printf("worldFrameCount = %d replayCommandList.size() = " MG_SIZE_T_SPECIFIER "\n",worldFrameCount,replayCommandList.size());

		std::vector<NetworkCommand> replayList;
		for(unsigned int i = 0; i < replayCommandList.size(); ++i) {
			std::pair<int,NetworkCommand> &cmd = replayCommandList[i];
			if(cmd.first <= worldFrameCount) {
				replayList.push_back(cmd.second);
				haveReplyCommands = true;
			}
		}
		if(haveReplyCommands == true) {
			replayCommandList.erase(replayCommandList.begin(),replayCommandList.begin() + replayList.size());

			if(SystemFlags::VERBOSE_MODE_ENABLED) printf("worldFrameCount = %d GIVING COMMANDS replayList.size() = " MG_SIZE_T_SPECIFIER "\n",worldFrameCount,replayList.size());
			for(int i= 0; i < (int)replayList.size(); ++i){
				giveNetworkCommand(&replayList[i]);
			}
			GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
			gameNetworkInterface->setKeyframe(worldFrameCount);
		}
	}
	return haveReplyCommands;
}

bool Commander::hasReplayCommandListForFrame() const {
	return (replayCommandList.empty() == false);
}

int Commander::getReplayCommandListForFrameCount() const {
	return (int)replayCommandList.size();
}

void Commander::updateNetwork(Game *game) {
	if(world == NULL) {
		return;
	}
	NetworkManager &networkManager= NetworkManager::getInstance();

	//check that this is a keyframe
	if(game != NULL) {
        GameSettings *gameSettings = game->getGameSettings();
        if( networkManager.isNetworkGame() == false ||
            (world->getFrameCount() % gameSettings->getNetworkFramePeriod()) == 0) {

        	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] networkManager.isNetworkGame() = %d,world->getFrameCount() = %d, gameSettings->getNetworkFramePeriod() = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,networkManager.isNetworkGame(),world->getFrameCount(),gameSettings->getNetworkFramePeriod());

        	if(getReplayCommandListForFrame(world->getFrameCount()) == false) {
				GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();

				if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled) perfTimer.start();
				//update the keyframe
				gameNetworkInterface->updateKeyframe(world->getFrameCount());
				if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && perfTimer.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] gameNetworkInterface->updateKeyframe for %d took %lld msecs\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,world->getFrameCount(),perfTimer.getMillis());

				if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled) perfTimer.start();
				//give pending commands
				if(SystemFlags::VERBOSE_MODE_ENABLED) printf("START process: %d network commands in frame: %d\n",gameNetworkInterface->getPendingCommandCount(),this->world->getFrameCount());
				for(int i= 0; i < gameNetworkInterface->getPendingCommandCount(); ++i){
					giveNetworkCommand(gameNetworkInterface->getPendingCommand(i));
				}
				if(SystemFlags::VERBOSE_MODE_ENABLED) printf("END process: %d network commands in frame: %d\n",gameNetworkInterface->getPendingCommandCount(),this->world->getFrameCount());
				if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && perfTimer.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] giveNetworkCommand took %lld msecs, PendingCommandCount = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,perfTimer.getMillis(),gameNetworkInterface->getPendingCommandCount());
				gameNetworkInterface->clearPendingCommands();
				if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Cleared network commands in frame: %d\n",this->world->getFrameCount());
        	}
        }
	}
}

void Commander::addToReplayCommandList(NetworkCommand &command,int worldFrameCount) {
	replayCommandList.push_back(make_pair(worldFrameCount,command));
}

void Commander::giveNetworkCommand(NetworkCommand* networkCommand) const {
	Chrono chrono;
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled) chrono.start();

	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] took msecs: %lld [START]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());


	world->getGame()->addNetworkCommandToReplayList(networkCommand,world->getFrameCount());

    networkCommand->preprocessNetworkCommand(this->world);

    if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] took msecs: %lld [after networkCommand->preprocessNetworkCommand]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());

    bool commandWasHandled = false;
    // Handle special commands first (that just use network command members as placeholders)
    switch(networkCommand->getNetworkCommandType()) {
    	case nctSwitchTeam: {
        	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found nctSwitchTeam\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

        	commandWasHandled = true;
        	int factionIndex = networkCommand->getUnitId();
        	int newTeam = networkCommand->getCommandTypeId();

        	// Auto join empty team or ask players to join
        	bool autoJoinTeam = true;
        	for(int i = 0; i < world->getFactionCount(); ++i) {
        		if(newTeam == world->getFaction(i)->getTeam()) {
        			autoJoinTeam = false;
        			break;
        		}
        	}

        	if(autoJoinTeam == true) {
        		Faction *faction = world->getFaction(factionIndex);
        		int oldTeam = faction->getTeam();
        		faction->setTeam(newTeam);
        		GameSettings *settings = world->getGameSettingsPtr();
        		settings->setTeam(factionIndex,newTeam);
        		world->getStats()->setTeam(factionIndex, newTeam);

        		if(factionIndex == world->getThisFactionIndex()) {
        			world->setThisTeamIndex(newTeam);
					GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
					if(gameNetworkInterface != NULL) {

	    		    	Lang &lang= Lang::getInstance();
	    		    	const vector<string> languageList = settings->getUniqueNetworkPlayerLanguages();
	    		    	for(unsigned int i = 0; i < languageList.size(); ++i) {
							char szMsg[8096]="";
							if(lang.hasString("PlayerSwitchedTeam",languageList[i]) == true) {
								snprintf(szMsg,8096,lang.getString("PlayerSwitchedTeam",languageList[i]).c_str(),settings->getNetworkPlayerName(factionIndex).c_str(),oldTeam,newTeam);
							}
							else {
								snprintf(szMsg,8096,"Player %s switched from team# %d to team# %d.",settings->getNetworkPlayerName(factionIndex).c_str(),oldTeam,newTeam);
							}
							bool localEcho = lang.isLanguageLocal(languageList[i]);
							gameNetworkInterface->sendTextMessage(szMsg,-1, localEcho,languageList[i]);
	    		    	}
					}
					world->getGame()->reInitGUI();
        		}
        	}
        	else {
        		for(int i = 0; i < world->getFactionCount(); ++i) {
					if(newTeam == world->getFaction(i)->getTeam()) {
						Faction *faction = world->getFaction(factionIndex);

						SwitchTeamVote vote;
						vote.factionIndex = factionIndex;
						vote.allowSwitchTeam = false;
						vote.oldTeam = faction->getTeam();
						vote.newTeam = newTeam;
						vote.voted = false;

						world->getFaction(i)->setSwitchTeamVote(vote);
					}
        		}
        	}

            if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] took msecs: %lld [after unit->setMeetingPos]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());

            if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found nctSetMeetingPoint\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
        }
            break;

        case nctSwitchTeamVote: {
        	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found nctSwitchTeamVote\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

        	commandWasHandled = true;

        	int votingFactionIndex = networkCommand->getUnitId();
        	int factionIndex = networkCommand->getCommandTypeId();
        	bool allowSwitchTeam = networkCommand->getUnitTypeId() != 0;

        	Faction *faction = world->getFaction(votingFactionIndex);

			SwitchTeamVote *vote = faction->getSwitchTeamVote(factionIndex);
			if(vote == NULL) {
				throw megaglest_runtime_error("vote == NULL");
			}
			vote->voted = true;
			vote->allowSwitchTeam = allowSwitchTeam;

        	// Join the new team if > 50 % said yes
			int newTeamTotalMemberCount=0;
			int newTeamVotedYes=0;
			int newTeamVotedNo=0;

        	for(int i = 0; i < world->getFactionCount(); ++i) {
        		if(vote->newTeam == world->getFaction(i)->getTeam()) {
        			newTeamTotalMemberCount++;

        			SwitchTeamVote *teamVote = world->getFaction(i)->getSwitchTeamVote(factionIndex);
        			if(teamVote != NULL && teamVote->voted == true) {
        				if(teamVote->allowSwitchTeam == true) {
        					newTeamVotedYes++;
        				}
        				else {
        					newTeamVotedNo++;
        				}
        			}
        		}
        	}

        	// If > 50% of team vote yes, switch th eplayers team
        	if(newTeamTotalMemberCount > 0 && newTeamVotedYes > 0 &&
        			static_cast<float>(newTeamVotedYes) / static_cast<float>(newTeamTotalMemberCount) > 0.5) {
        		Faction *faction = world->getFaction(factionIndex);
        		int oldTeam = faction->getTeam();
        		faction->setTeam(vote->newTeam);
        		GameSettings *settings = world->getGameSettingsPtr();
        		settings->setTeam(factionIndex,vote->newTeam);
        		world->getStats()->setTeam(factionIndex, vote->newTeam);

        		if(factionIndex == world->getThisFactionIndex()) {
        			world->setThisTeamIndex(vote->newTeam);
					GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
					if(gameNetworkInterface != NULL) {

	    		    	Lang &lang= Lang::getInstance();
	    		    	const vector<string> languageList = settings->getUniqueNetworkPlayerLanguages();
	    		    	for(unsigned int i = 0; i < languageList.size(); ++i) {
							char szMsg[8096]="";
							if(lang.hasString("PlayerSwitchedTeam",languageList[i]) == true) {
								snprintf(szMsg,8096,lang.getString("PlayerSwitchedTeam",languageList[i]).c_str(),settings->getNetworkPlayerName(factionIndex).c_str(),oldTeam,vote->newTeam);
							}
							else {
								snprintf(szMsg,8096,"Player %s switched from team# %d to team# %d.",settings->getNetworkPlayerName(factionIndex).c_str(),oldTeam,vote->newTeam);
							}
							bool localEcho = lang.isLanguageLocal(languageList[i]);
							gameNetworkInterface->sendTextMessage(szMsg,-1, localEcho,languageList[i]);
	    		    	}
					}
					world->getGame()->reInitGUI();
        		}
        	}
        	else if(newTeamTotalMemberCount == (newTeamVotedYes + newTeamVotedNo)) {
        		if(factionIndex == world->getThisFactionIndex()) {
        			GameSettings *settings = world->getGameSettingsPtr();
            		Faction *faction = world->getFaction(factionIndex);
            		int oldTeam = faction->getTeam();

					GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
					if(gameNetworkInterface != NULL) {

	    		    	Lang &lang= Lang::getInstance();
	    		    	const vector<string> languageList = settings->getUniqueNetworkPlayerLanguages();
	    		    	for(unsigned int i = 0; i < languageList.size(); ++i) {
							char szMsg[8096]="";
							if(lang.hasString("PlayerSwitchedTeamDenied",languageList[i]) == true) {
								snprintf(szMsg,8096,lang.getString("PlayerSwitchedTeamDenied",languageList[i]).c_str(),settings->getNetworkPlayerName(factionIndex).c_str(),oldTeam,vote->newTeam);
							}
							else {
								snprintf(szMsg,8096,"Player %s was denied the request to switch from team# %d to team# %d.",settings->getNetworkPlayerName(factionIndex).c_str(),oldTeam,vote->newTeam);
							}
							bool localEcho = lang.isLanguageLocal(languageList[i]);
							gameNetworkInterface->sendTextMessage(szMsg,-1, localEcho,languageList[i]);
	    		    	}
					}
        		}
        	}

            if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] took msecs: %lld [after unit->setMeetingPos]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());

            if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found nctSetMeetingPoint\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
        }
            break;

    	case nctDisconnectNetworkPlayer: {
        	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found nctDisconnectNetworkPlayer\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

        	commandWasHandled = true;

        	NetworkManager &networkManager= NetworkManager::getInstance();
        	NetworkRole role = networkManager.getNetworkRole();
        	//GameSettings *settings = world->getGameSettingsPtr();

        	if(role == nrServer) {
				//int factionIndex = networkCommand->getUnitId();
				int playerIndex = networkCommand->getCommandTypeId();

				GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
				if(gameNetworkInterface != NULL) {
					ServerInterface *server = networkManager.getServerInterface();
					if(server != NULL && server->isClientConnected(playerIndex) == true) {

						MutexSafeWrapper safeMutex(server->getSlotMutex(playerIndex),CODE_AT_LINE);
						ConnectionSlot *slot = server->getSlot(playerIndex,false);
						if(slot != NULL) {
							safeMutex.ReleaseLock();
							NetworkMessageQuit networkMessageQuit;
							slot->sendMessage(&networkMessageQuit);
							sleep(5);

							//printf("Sending nctDisconnectNetworkPlayer\n");
							server = networkManager.getServerInterface(false);
							if(server != NULL) {
								MutexSafeWrapper safeMutex2(server->getSlotMutex(playerIndex),CODE_AT_LINE);
								slot = server->getSlot(playerIndex,false);
								if(slot != NULL) {
									safeMutex2.ReleaseLock();
									slot->close();
								}
							}
						}
					}
				}
        	}
            if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found nctDisconnectNetworkPlayer\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
        }
            break;

        case nctPauseResume:
        	{
        	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found nctPauseResume\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

        	commandWasHandled = true;

        	bool pauseGame 			= networkCommand->getUnitId() != 0;
        	bool clearCaches 		= (networkCommand->getCommandTypeId() == 1);
        	bool joinNetworkGame 	= (networkCommand->getUnitTypeId() == 1);
       		Game *game = this->world->getGame();

       		//printf("nctPauseResume pauseGame = %d\n",pauseGame);
       		game->setPaused(pauseGame,true,clearCaches,joinNetworkGame);

            if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found nctPauseResume\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
        	}
            break;

        case nctPlayerStatusChange:
			{
			if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found nctPlayerStatusChange\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

			commandWasHandled = true;

			int factionIndex = networkCommand->getUnitId();
			int playerStatus = networkCommand->getCommandTypeId();

			if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"nctPlayerStatusChange factionIndex = %d playerStatus = %d\n",factionIndex,playerStatus);

			//printf("#1 nctPlayerStatusChange factionIndex = %d playerStatus = %d\n",factionIndex,playerStatus);

			GameSettings *settings = world->getGameSettingsPtr();
    		if(playerStatus == npst_Disconnected) {
    			//printf("Commander nctPlayerStatusChange factionIndex: %d\n",factionIndex);

    			settings->setNetworkPlayerStatuses(factionIndex,npst_Disconnected);

    			//printf("nctPlayerStatusChange -> faction->getPersonalityType() = %d index [%d] control [%d] networkstatus [%d]\n",
    			//		world->getFaction(factionIndex)->getPersonalityType(),world->getFaction(factionIndex)->getIndex(),world->getFaction(factionIndex)->getControlType(),settings->getNetworkPlayerStatuses(factionIndex));

    			//printf("#2 nctPlayerStatusChange factionIndex = %d playerStatus = %d\n",factionIndex,playerStatus);
    			settings->setFactionControl(factionIndex,ctCpuUltra);
    			settings->setResourceMultiplierIndex(factionIndex,settings->getFallbackCpuMultiplier());
    			//Game *game = this->world->getGame();
    			//game->get
    			Faction *faction = this->world->getFaction(factionIndex);
    			faction->setControlType(ctCpuUltra);

        		if(!world->getGame()->getGameOver()&& !this->world->getGame()->factionLostGame(factionIndex)){
        			// use the fallback multiplier here

        			// mark player as "leaver"
        			this->world->getStats()->setPlayerLeftBeforeEnd(factionIndex,true);
        			// set disconnect time for endgame stats
        			this->world->getStats()->setTimePlayerLeft(factionIndex,this->world->getStats()->getFramesToCalculatePlaytime());
        		}
    		}

			if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found nctPlayerStatusChange\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
			}
			break;

    }

    if(commandWasHandled == false) {
        Unit* unit= world->findUnitById(networkCommand->getUnitId());

        if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] took msecs: %lld [after world->findUnitById]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());

        if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Running command NetworkCommandType = %d, unitid = %d [%p] factionindex = %d\n",networkCommand->getNetworkCommandType(),networkCommand->getUnitId(),unit,(unit != NULL ? unit->getFactionIndex() : -1));
        //execute command, if unit is still alive
        if(unit != NULL) {
            switch(networkCommand->getNetworkCommandType()) {
                case nctGiveCommand:{
                    assert(networkCommand->getCommandTypeId() != CommandType::invalidId);

                    if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found nctGiveCommand networkCommand->getUnitId() = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,networkCommand->getUnitId());

                    Command* command= buildCommand(networkCommand);

                    if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] took msecs: %lld [after buildCommand]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());

                    if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] command = %p\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,command);

                    unit->giveCommand(command, (networkCommand->getWantQueue() != 0));

                    if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] took msecs: %lld [after unit->giveCommand]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());

                    if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found nctGiveCommand networkCommand->getUnitId() = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,networkCommand->getUnitId());
                    }
                    break;
                case nctCancelCommand: {
                	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found nctCancelCommand\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

                	unit->cancelCommand();

                	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] took msecs: %lld [after unit->cancelCommand]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());

                	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found nctCancelCommand\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
                }
                    break;
                case nctSetMeetingPoint: {
                	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found nctSetMeetingPoint\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

                    unit->setMeetingPos(networkCommand->getPosition());

                    if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] took msecs: %lld [after unit->setMeetingPos]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());

                    if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found nctSetMeetingPoint\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
                }
                    break;

                default:
                    assert(false);
                    break;
            }
        }
        else {
        	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] NULL Unit for id = %d, networkCommand->getNetworkCommandType() = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,networkCommand->getUnitId(),networkCommand->getNetworkCommandType());
        }
    }

    if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] took msecs: %lld [END]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());
}

Command* Commander::buildCommand(const NetworkCommand* networkCommand) const {
	assert(networkCommand->getNetworkCommandType()==nctGiveCommand);

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] networkCommand [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,networkCommand->toString().c_str());

	if(world == NULL) {
	    char szBuf[8096]="";
	    snprintf(szBuf,8096,"In [%s::%s Line: %d] world == NULL for unit with id: %d",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,networkCommand->getUnitId());
		throw megaglest_runtime_error(szBuf);
	}

	Unit* target= NULL;
	const CommandType* ct= NULL;
	const Unit* unit= world->findUnitById(networkCommand->getUnitId());

	//validate unit
	if(unit == NULL) {
	    char szBuf[8096]="";
	    snprintf(szBuf,8096,"In [%s::%s Line: %d] Can not find unit with id: %d. Game out of synch.",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,networkCommand->getUnitId());
	    SystemFlags::OutputDebug(SystemFlags::debugError,"%s\n",szBuf);
	    if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"%s\n",szBuf);

        GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
        if(gameNetworkInterface != NULL) {
            char szMsg[8096]="";
            snprintf(szMsg,8096,"Player detected an error: Can not find unit with id: %d. Game out of synch.",networkCommand->getUnitId());
            gameNetworkInterface->sendTextMessage(szMsg,-1, true, "");
        }

		throw megaglest_runtime_error(szBuf);
	}

	ct = unit->getType()->findCommandTypeById(networkCommand->getCommandTypeId());

	if(unit->getFaction()->getIndex() != networkCommand->getUnitFactionIndex()) {

	    char szBuf[8096]="";
	    snprintf(szBuf,8096,"In [%s::%s Line: %d]\nUnit / Faction mismatch for network command = [%s]\n%s\nfor unit = %d\n[%s]\n[%s]\nactual local factionIndex = %d.\nGame out of synch.",
            __FILE__,__FUNCTION__,__LINE__,networkCommand->toString().c_str(),unit->getType()->getCommandTypeListDesc().c_str(),unit->getId(), unit->getFullName(false).c_str(),unit->getDesc(false).c_str(),unit->getFaction()->getIndex());

	    SystemFlags::OutputDebug(SystemFlags::debugError,"%s\n",szBuf);
	    if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"%s\n",szBuf);
	    //std::string worldLog = world->DumpWorldToLog();
	    world->DumpWorldToLog();

        GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
        if(gameNetworkInterface != NULL && gameNetworkInterface->isConnected() == true) {
            char szMsg[8096]="";
            snprintf(szMsg,8096,"Player detected an error: Unit / Faction mismatch for unitId: %d",networkCommand->getUnitId());
            gameNetworkInterface->sendTextMessage(szMsg,-1, true, "");
            snprintf(szMsg,8096,"Local faction index = %d, remote index = %d. Game out of synch.",unit->getFaction()->getIndex(),networkCommand->getUnitFactionIndex());
            gameNetworkInterface->sendTextMessage(szMsg,-1, true, "");

        }
        else if(gameNetworkInterface != NULL) {
            char szMsg[8096]="";
            snprintf(szMsg,8096,"Player detected an error: Connection lost, possible Unit / Faction mismatch for unitId: %d",networkCommand->getUnitId());
            gameNetworkInterface->sendTextMessage(szMsg,-1, true,"");
            snprintf(szMsg,8096,"Local faction index = %d, remote index = %d. Game out of synch.",unit->getFaction()->getIndex(),networkCommand->getUnitFactionIndex());
            gameNetworkInterface->sendTextMessage(szMsg,-1, true,"");
        }

	    std::string sError = "Error [#1]: Game is out of sync (Unit / Faction mismatch)\nplease check log files for details.";
		throw megaglest_runtime_error(sError);
	}

	const UnitType* unitType= world->findUnitTypeById(unit->getFaction()->getType(), networkCommand->getUnitTypeId());
	
	if(networkCommand->getNextUnitTypeId() > -1 && networkCommand->getWantQueue()) { //Morph Queue
		auto nextUnitType= world->findUnitTypeById(unit->getFaction()->getType(), networkCommand->getNextUnitTypeId());
		auto mct = nextUnitType->findCommandTypeById(networkCommand->getCommandTypeId());
		if(mct) ct = mct;
	}

	// debug test!
	//throw megaglest_runtime_error("Test missing command type!");

	//validate command type

    // !!!Test out of synch behaviour
    //ct = NULL;

    // Check if the command was for the unit before it morphed, if so cancel it
    bool isCancelPreMorphCommand = false;
    if(ct == NULL && unit->getPreMorphType() != NULL) {
    	const CommandType *ctPreMorph = unit->getPreMorphType()->findCommandTypeById(networkCommand->getCommandTypeId());
    	if(ctPreMorph != NULL) {
    		ct = unit->getType()->getFirstCtOfClass(ccStop);
    		isCancelPreMorphCommand = true;
    	}
    }

	if(ct == NULL) {
	    char szBuf[8096]="";
	    snprintf(szBuf,8096,"In [%s::%s Line: %d]\nCan not find command type for network command = [%s]\n%s\nfor unit = %d\n[%s]\n[%s]\nactual local factionIndex = %d.\nUnit Type Info:\n[%s]\nNetwork unit type:\n[%s]\nisCancelPreMorphCommand: %d\nGame out of synch.",
	    	extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,networkCommand->toString().c_str(),unit->getType()->getCommandTypeListDesc().c_str(),
            unit->getId(), unit->getFullName(false).c_str(),unit->getDesc(false).c_str(),unit->getFaction()->getIndex(),unit->getType()->toString().c_str(),
            (unitType != NULL ? unitType->getName(false).c_str() : "null"),isCancelPreMorphCommand);

	    SystemFlags::OutputDebug(SystemFlags::debugSystem,"%s\n",szBuf);
	    SystemFlags::OutputDebug(SystemFlags::debugError,"%s\n",szBuf);
	    world->DumpWorldToLog();

        GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
        if(gameNetworkInterface != NULL) {
            char szMsg[8096]="";
            snprintf(szMsg,8096,"Player detected an error: Can not find command type: %d for unitId: %d [%s]. isCancelPreMorphCommand: %d Game out of synch.",networkCommand->getCommandTypeId(),networkCommand->getUnitId(),(unitType != NULL ? unitType->getName(false).c_str() : "null"),isCancelPreMorphCommand);
            gameNetworkInterface->sendTextMessage(szMsg,-1, true, "");
        }

	    std::string sError = "Error [#3]: Game is out of sync, please check log files for details.";
	    //abort();
		throw megaglest_runtime_error(sError);
	}

	CardinalDir facing;
	// get facing/target ... the target might be dead due to lag, cope with it
	if(isCancelPreMorphCommand == false) {
		if(ct->getClass() == ccBuild) {
			if(networkCommand->getTargetId() < 0 || networkCommand->getTargetId() >= 4) {
				char szBuf[8096]="";
				snprintf(szBuf,8096,"networkCommand->getTargetId() >= 0 && networkCommand->getTargetId() < 4, [%s]",networkCommand->toString().c_str());
				throw megaglest_runtime_error(szBuf);
			}
			facing = CardinalDir(networkCommand->getTargetId());
		}
		else if (networkCommand->getTargetId() != Unit::invalidId ) {
			target= world->findUnitById(networkCommand->getTargetId());
		}
	}

	//create command
	Command *command= NULL;
	if(isCancelPreMorphCommand == false) {
		if(unitType != NULL) {
			command= new Command(ct, networkCommand->getPosition(), unitType, facing);
		}
		else if(target == NULL) {
			command= new Command(ct, networkCommand->getPosition());
		}
		else {
			command= new Command(ct, target);
		}
	}
	else {
		command= new Command(ct, NULL);
	}

	// Add in any special state
	CommandStateType commandStateType 	= networkCommand->getCommandStateType();
	int commandStateValue 				= networkCommand->getCommandStateValue();

	command->setStateType(commandStateType);
	command->setStateValue(commandStateValue);
	command->setUnitCommandGroupId(networkCommand->getUnitCommandGroupId());

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	//issue command
	return command;
}

}}//end namespace
