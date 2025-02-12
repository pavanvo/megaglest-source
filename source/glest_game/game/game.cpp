//	This file is part of Glest (www.glest.org)
//
//	Copyright (C) 2001-2008 Martiño Figueroa
//
//	You can redistribute this code and/or modify it under
//	the terms of the GNU General Public License as published
//	by the Free Software Foundation; either version 2 of the
//	License, or (at your option) any later version
// ==============================================================

#include "game.h"

#include "config.h"
#include "renderer.h"
#include "particle_renderer.h"
#include "commander.h"
#include "battle_end.h"
#include "sound_renderer.h"
#include "profiler.h"
#include "core_data.h"
#include "metrics.h"
#include "faction.h"
#include "network_manager.h"
#include "checksum.h"
#include "auto_test.h"
#include "menu_state_keysetup.h"
#include "video_player.h"
#include "compression_utils.h"
#include "cache_manager.h"
#include "conversion.h"
#include "steam.h"
#include "memory.h"

#include "leak_dumper.h"

using namespace Shared::Graphics;
using namespace Shared::Util;
using namespace Shared::Platform;
using namespace Shared::CompressionUtil;

namespace Glest{ namespace Game{

string GameSettings::playerDisconnectedText = "";
Game *thisGamePtr = NULL;

// =====================================================
// 	class Game
// =====================================================

// ===================== PUBLIC ========================

const float PHOTO_MODE_MAXHEIGHT = 500.0;

const int CREATE_NEW_TEAM = -100;
const int CANCEL_SWITCH_TEAM = -1;

const int CANCEL_DISCONNECT_PLAYER = -1;

const float Game::highlightTime= 0.5f;

int fadeMusicMilliseconds = 3500;

// Check every x seconds if we should switch disconnected players to AI
const int NETWORK_PLAYER_CONNECTION_CHECK_SECONDS = 5;

int GAME_STATS_DUMP_INTERVAL = 60 * 10;

Game::Game() : ProgramState(NULL) {
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	originalDisplayMsgCallback = NULL;
	aiInterfaces.clear();
	videoPlayer = NULL;
	playingStaticVideo = false;

	mouse2d=0;
	mouseX=0;
	mouseY=0;
	updateFps=0;
	lastUpdateFps=0;
	avgUpdateFps=0;
	framesToCatchUpAsClient=0;
	framesToSlowDownAsClient=0;
	totalRenderFps=0;
	renderFps=0;
	lastRenderFps=0;
	avgRenderFps=0;
	currentAvgRenderFpsTotal=0;
	paused=false;
	networkPauseGameForLaggedClientsRequested=false;
	networkResumeGameForLaggedClientsRequested=false;
	pausedForJoinGame=false;
	pausedBeforeJoinGame=false;
	pauseRequestSent=false;
	resumeRequestSent=false;
	pauseStateChanged=false;
	gameOver=false;
	renderNetworkStatus=false;
	renderInGamePerformance=false;
	showFullConsole=false;
	setMarker=false;
	cameraDragAllowed=false;
	mouseMoved=false;
	scrollSpeed=0;
	camLeftButtonDown=false;
	camRightButtonDown=false;
	camUpButtonDown=false;
	camDownButtonDown=false;
	this->speed=1;
	weatherParticleSystem=NULL;
	isFirstRender=false;
	quitTriggeredIndicator=false;
	quitPendingIndicator=false;
	original_updateFps=0;
	original_cameraFps=0;
	captureAvgTestStatus=false;
	updateFpsAvgTest=0;
	renderFpsAvgTest=0;
	renderExtraTeamColor=0;
	photoModeEnabled=false;
	healthbarMode=hbvUndefined;
	visibleHUD=false;
	timeDisplay=false;
	withRainEffect=false;
	program=NULL;
	gameStarted=false;
	this->initialResumeSpeedLoops=false;

	highlightCellTexture=NULL;
	lastMasterServerGameStatsDump=0;
	lastMaxUnitCalcTime=0;
	lastRenderLog2d=0;
	playerIndexDisconnect=0;
	tickCount=0;
	currentCameraFollowUnit=NULL;

	popupMenu.setEnabled(false);
	popupMenu.setVisible(false);

	popupMenuSwitchTeams.setEnabled(false);
	popupMenuSwitchTeams.setVisible(false);

	popupMenuDisconnectPlayer.setEnabled(false);
	popupMenuDisconnectPlayer.setVisible(false);

	switchTeamConfirmMessageBox.setEnabled(false);
	disconnectPlayerConfirmMessageBox.setEnabled(false);

	disconnectPlayerIndexMap.clear();

	exitGamePopupMenuIndex = -1;
	joinTeamPopupMenuIndex = -1;
	pauseGamePopupMenuIndex = -1;
	saveGamePopupMenuIndex = -1;
	loadGamePopupMenuIndex = -1;
	keyboardSetupPopupMenuIndex = -1;
	disconnectPlayerPopupMenuIndex = -1;

	isMarkCellEnabled = false;
	isMarkCellTextEnabled = false;

	markCellTexture = NULL;
	isUnMarkCellEnabled = false;
	unmarkCellTexture = NULL;

	masterserverMode = false;
	currentUIState=NULL;
	currentAmbientSound=NULL;
	//printf("In [%s:%s] Line: %d currentAmbientSound = [%p]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,currentAmbientSound);

	loadGameNode = NULL;
	lastworldFrameCountForReplay = -1;
	lastNetworkPlayerConnectionCheck = time(NULL);
	inJoinGameLoading = false;
	quitGameCalled = false;
	disableSpeedChange = false;

	for( int i=0;i<GameConstants::networkSmoothInterval;i++){
		receivedTooEarlyInFrames[i]=-1;
		framesNeededToWaitForServerMessage[i]=-1;
	}

	fadeMusicMilliseconds = Config::getInstance().getInt("GameStartStopFadeSoundMilliseconds",intToStr(fadeMusicMilliseconds).c_str());
	GAME_STATS_DUMP_INTERVAL = Config::getInstance().getInt("GameStatsDumpIntervalSeconds",intToStr(GAME_STATS_DUMP_INTERVAL).c_str());
}

void Game::resetMembers() {
	Unit::setGame(this);
	gameStarted = false;
	this->initialResumeSpeedLoops = false;

	original_updateFps = GameConstants::updateFps;
	original_cameraFps = GameConstants::cameraFps;
	GameConstants::updateFps= 40;
	GameConstants::cameraFps= 100;
	captureAvgTestStatus = false;
	updateFpsAvgTest=0;
	renderFpsAvgTest=0;
	lastRenderLog2d		 = 0;
	playerIndexDisconnect=0;
	lastMasterServerGameStatsDump=0;
	highlightCellTexture=NULL;
	totalRenderFps       =0;
	lastMaxUnitCalcTime  =0;
	renderExtraTeamColor =0;

	mouseMoved= false;
	quitTriggeredIndicator = false;
	quitPendingIndicator=false;
	originalDisplayMsgCallback = NULL;
	thisGamePtr = this;

	popupMenu.setEnabled(false);
	popupMenu.setVisible(false);

	popupMenuSwitchTeams.setEnabled(false);
	popupMenuSwitchTeams.setVisible(false);

	popupMenuDisconnectPlayer.setEnabled(false);
	popupMenuDisconnectPlayer.setVisible(false);

	switchTeamConfirmMessageBox.setEnabled(false);
	disconnectPlayerConfirmMessageBox.setEnabled(false);

	disconnectPlayerIndexMap.clear();

	exitGamePopupMenuIndex = -1;
	joinTeamPopupMenuIndex = -1;
	pauseGamePopupMenuIndex = -1;
	saveGamePopupMenuIndex = -1;
	loadGamePopupMenuIndex = -1;
	keyboardSetupPopupMenuIndex = -1;
	disconnectPlayerPopupMenuIndex = -1;

	isMarkCellEnabled = false;
	isMarkCellTextEnabled = false;

	markCellTexture = NULL;
	isUnMarkCellEnabled = false;
	unmarkCellTexture = NULL;

	currentUIState = NULL;

	scrollSpeed = Config::getInstance().getFloat("UiScrollSpeed","1.5");
	photoModeEnabled = Config::getInstance().getBool("PhotoMode","false");
	healthbarMode = Config::getInstance().getInt("HealthBarMode","4");
	visibleHUD = Config::getInstance().getBool("VisibleHud","true");
	timeDisplay = Config::getInstance().getBool("TimeDisplay","true");
	withRainEffect = Config::getInstance().getBool("RainEffect","true");
	//MIN_RENDER_FPS_ALLOWED = Config::getInstance().getInt("MIN_RENDER_FPS_ALLOWED",intToStr(MIN_RENDER_FPS_ALLOWED).c_str());

	mouseX=0;
	mouseY=0;
	mouse2d= 0;
	loadingText="";
	weatherParticleSystem= NULL;
	updateFps=0;
	renderFps=0;
	lastUpdateFps=0;
	framesToCatchUpAsClient=0;
	framesToSlowDownAsClient=0;
	lastRenderFps=-1;
	avgUpdateFps=-1;
	avgRenderFps=-1;
	currentAvgRenderFpsTotal=0;
	tickCount=0;
	paused= false;
	networkPauseGameForLaggedClientsRequested=false;
	networkResumeGameForLaggedClientsRequested=false;
	pausedForJoinGame=false;
	pausedBeforeJoinGame=false;
	resumeRequestSent=false;
	pauseRequestSent=false;
	pauseStateChanged=false;
	gameOver= false;
	renderNetworkStatus= false;
	renderInGamePerformance=false;
	this->speed= 1;
	showFullConsole= false;
	setMarker = false;
	cameraDragAllowed=false;
	camLeftButtonDown=false;
	camRightButtonDown=false;
	camUpButtonDown=false;
	camDownButtonDown=false;

	currentCameraFollowUnit=NULL;
	currentAmbientSound=NULL;
	//printf("In [%s:%s] Line: %d currentAmbientSound = [%p]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,currentAmbientSound);

	loadGameNode = NULL;
	lastworldFrameCountForReplay = -1;

	lastNetworkPlayerConnectionCheck = time(NULL);

	inJoinGameLoading = false;
	quitGameCalled = false;
	disableSpeedChange = false;

	for( int i=0;i<GameConstants::networkSmoothInterval;i++){
		receivedTooEarlyInFrames[i]=-1;
		framesNeededToWaitForServerMessage[i]=-1;
	}


	fadeMusicMilliseconds = Config::getInstance().getInt("GameStartStopFadeSoundMilliseconds",intToStr(fadeMusicMilliseconds).c_str());
	GAME_STATS_DUMP_INTERVAL = Config::getInstance().getInt("GameStatsDumpIntervalSeconds",intToStr(GAME_STATS_DUMP_INTERVAL).c_str());

    Logger &logger= Logger::getInstance();
	logger.showProgress();
}

Game::Game(Program *program, const GameSettings *gameSettings,bool masterserverMode):
	ProgramState(program), lastMousePos(0), isFirstRender(true)
{
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	this->masterserverMode = masterserverMode;
	videoPlayer = NULL;
	playingStaticVideo = false;
	highlightCellTexture = NULL;
	playerIndexDisconnect=0;
	updateFpsAvgTest=0;
	renderFpsAvgTest=0;
	cameraDragAllowed=false;

	if(this->masterserverMode == true) {
		printf("Starting a new game...\n");
	}

	this->program = program;
	resetMembers();
	this->gameSettings= *gameSettings;

	Lang::getInstance().setAllowNativeLanguageTechtree(this->gameSettings.getNetworkAllowNativeLanguageTechtree());

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
}

void Game::endGame() {
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	quitGame();
	sleep(0);

	Object::setStateCallback(NULL);
	thisGamePtr = NULL;
	if(originalDisplayMsgCallback != NULL) {
		NetworkInterface::setDisplayMessageFunction(originalDisplayMsgCallback);
	}

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

    Logger &logger= Logger::getInstance();
	Renderer &renderer= Renderer::getInstance();

	logger.clearHints();
	logger.loadLoadingScreen("");
	logger.setState(Lang::getInstance().getString("Deleting"));
	//logger.add("Game", true);
	logger.add(Lang::getInstance().getString("LogScreenGameLoading","",true), false);
	logger.hideProgress();

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	// Cannot Fade because sound files will be deleted below
	SoundRenderer::getInstance().stopAllSounds(fadeMusicMilliseconds);

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

//	deleteValues(aiInterfaces.begin(), aiInterfaces.end());
//	aiInterfaces.clear();

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	gui.end();		//selection must be cleared before deleting units

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

//	world.end();	//must die before selection because of referencers

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] aiInterfaces.size() = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,aiInterfaces.size());

	// MUST DO THIS LAST!!!! Because objects above have pointers to things like
	// unit particles and fade them out etc and this end method deletes the original
	// object pointers.
	renderer.endGame(false);

	GameConstants::updateFps = original_updateFps;
	GameConstants::cameraFps = original_cameraFps;

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	Unit::setGame(NULL);

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] ==== END GAME ==== getCurrentPixelByteCount() = " MG_SIZE_T_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderer.getCurrentPixelByteCount());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugWorldSynch).enabled) SystemFlags::OutputDebug(SystemFlags::debugWorldSynch,"==== END GAME ====\n");

	FileCRCPreCacheThread * &preCacheCRCThreadPtr = CacheManager::getCachedItem< FileCRCPreCacheThread * >(GameConstants::preCacheThreadCacheLookupKey);
	if(preCacheCRCThreadPtr != NULL) {
		preCacheCRCThreadPtr->setPauseForGame(false);
	}

	//this->program->reInitGl();
	//renderer.reinitAll();
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
}

Game::~Game() {
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	quitGame();

	Object::setStateCallback(NULL);
	thisGamePtr = NULL;
	if(originalDisplayMsgCallback != NULL) {
		NetworkInterface::setDisplayMessageFunction(originalDisplayMsgCallback);
	}

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

    Logger &logger= Logger::getInstance();
	Renderer &renderer= Renderer::getInstance();

	logger.loadLoadingScreen("");
	logger.setState(Lang::getInstance().getString("Deleting"));
	//logger.add("Game", true);
	logger.add(Lang::getInstance().getString("LogScreenGameLoading","",true), false);
	logger.hideProgress();

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	// Cannot Fade because sound files will be deleted below
	SoundRenderer::getInstance().stopAllSounds();

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	masterController.clearSlaves(true);
	deleteValues(aiInterfaces.begin(), aiInterfaces.end());
	aiInterfaces.clear();

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	gui.end();		//selection must be cleared before deleting units

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	world.end();	//must die before selection because of referencers

	BaseColorPickEntity::resetUniqueColors();

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] aiInterfaces.size() = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,aiInterfaces.size());

	delete videoPlayer;
	videoPlayer = NULL;
	playingStaticVideo = false;

	// MUST DO THIS LAST!!!! Because objects above have pointers to things like
	// unit particles and fade them out etc and this end method deletes the original
	// object pointers.
	renderer.endGame(true);
	BaseColorPickEntity::cleanupPBO();

	GameConstants::updateFps = original_updateFps;
	GameConstants::cameraFps = original_cameraFps;

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	Unit::setGame(NULL);

	Lang::getInstance().setAllowNativeLanguageTechtree(true);

	FileCRCPreCacheThread * &preCacheCRCThreadPtr = CacheManager::getCachedItem< FileCRCPreCacheThread * >(GameConstants::preCacheThreadCacheLookupKey);
	if(preCacheCRCThreadPtr != NULL) {
		preCacheCRCThreadPtr->setPauseForGame(false);
	}

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] ==== END GAME ==== getCurrentPixelByteCount() = " MG_SIZE_T_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderer.getCurrentPixelByteCount());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugWorldSynch).enabled) SystemFlags::OutputDebug(SystemFlags::debugWorldSynch,"==== END GAME ====\n");

	//this->program->reInitGl();
	//renderer.reinitAll();
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
}

bool Game::showTranslatedTechTree() const {
	return this->gameSettings.getNetworkAllowNativeLanguageTechtree();
}

bool Game::quitTriggered() {
	return quitTriggeredIndicator;
}

Stats Game::quitAndToggleState() {
	//quitGame();
	//Program *program = game->getProgram();
	return quitGame();
	//Game::exitGameState(program, endStats);
}

// ==================== init and load ====================

int Game::ErrorDisplayMessage(const char *msg, bool exitApp) {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] %s\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,msg);

	if(thisGamePtr != NULL) {
		string text = msg;
		thisGamePtr->showErrorMessageBox(text, "Error detected", false);
	}

    return 0;
}

//Texture2D * Game::findFactionLogoTexture(const GameSettings *settings, Logger *logger,string factionLogoFilter, bool useTechDefaultIfFilterNotFound) {
//	Texture2D *result = NULL;
//	string logoFilename = Game::findFactionLogoFile(settings, logger,factionLogoFilter);
//	if(logoFilename == "" && factionLogoFilter != "" && useTechDefaultIfFilterNotFound == true) {
//		logoFilename = Game::findFactionLogoFile(settings, logger);
//	}
//
//	result = Renderer::findTexture(logoFilename);
//
//	return result;
//}

string Game::extractScenarioLogoFile(const GameSettings *settings, string &result,
		bool &loadingImageUsed, Logger *logger, string factionLogoFilter) {
    string scenarioDir = "";
    if(settings->getScenarioDir() != "") {
		scenarioDir = settings->getScenarioDir();
		if(EndsWith(scenarioDir, ".xml") == true) {
			scenarioDir = scenarioDir.erase(scenarioDir.size() - 4, 4);
			scenarioDir = scenarioDir.erase(scenarioDir.size() - settings->getScenario().size(), settings->getScenario().size() + 1);
		}

		//printf("!!! extractScenarioLogoFile scenarioDir [%s] factionLogoFilter [%s]\n",scenarioDir.c_str(),factionLogoFilter.c_str());

		vector<string> loadScreenList;
		string logoFullPathFilter = scenarioDir + factionLogoFilter;
		findAll(logoFullPathFilter, loadScreenList, false, false);
		if(loadScreenList.empty() == false) {
			int bestLogoIndex = 0;

			if(loadScreenList.size() > 1 && EndsWith(factionLogoFilter, ".xml") == false) {
				if(SystemFlags::VERBOSE_MODE_ENABLED) printf("\nLooking for best logo from a list of: " MG_SIZE_T_SPECIFIER " using filter: [%s]\n",loadScreenList.size(),logoFullPathFilter.c_str());

				int bestMinWidthDiff = INT_MAX;
				int bestMinHeightDiff = INT_MAX;
				// Now find the best texture for our screen
				// Texture2D *result = preloadTexture(logoFilename);
				for(unsigned int logoIndex = 0; logoIndex < loadScreenList.size(); ++logoIndex) {
					string senarioLogo = scenarioDir + loadScreenList[bestLogoIndex];
					if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] looking for loading screen '%s'\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,senarioLogo.c_str());

					if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Looking for best logo: %u [%s]\n",logoIndex,senarioLogo.c_str());

					if(fileExists(senarioLogo) == true) {
						if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found loading screen '%s'\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,senarioLogo.c_str());

						Texture2D *checkLogo = Renderer::preloadTexture(senarioLogo);
						if(checkLogo != NULL) {
							const Metrics &metrics= Metrics::getInstance();
							int minWidthDifference = abs(metrics.getScreenW() - checkLogo->getPixmapConst()->getW());
							int minHeightDifference = abs(metrics.getScreenH() - checkLogo->getPixmapConst()->getH());

							if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Logo info: %d x %d (%d,%d)\n",checkLogo->getPixmapConst()->getW(),checkLogo->getPixmapConst()->getH(),minWidthDifference,minHeightDifference);

							if(minWidthDifference < bestMinWidthDiff) {
								bestMinWidthDiff = minWidthDifference;

								bestLogoIndex = logoIndex;
								if(SystemFlags::VERBOSE_MODE_ENABLED) printf("#1 New best logo is [%s]\n",senarioLogo.c_str());
							}
							else if(minWidthDifference == bestMinWidthDiff &&
									minHeightDifference < bestMinHeightDiff) {
								bestMinHeightDiff = minHeightDifference;

								bestLogoIndex = logoIndex;
								if(SystemFlags::VERBOSE_MODE_ENABLED) printf("#2 New best logo is [%s]\n",senarioLogo.c_str());
							}
						}
					}
				}
			}

			string senarioLogo = scenarioDir + loadScreenList[bestLogoIndex];
			if(fileExists(senarioLogo) == true) {
				if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s] found scenario loading screen '%s'\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,senarioLogo.c_str());

				result = senarioLogo;
				if(logger != NULL) {
					logger->loadLoadingScreen(result);
				}
				loadingImageUsed=true;
			}
		}
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] gameSettings.getScenarioDir() = [%s] gameSettings.getScenario() = [%s] scenarioDir = [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,settings->getScenarioDir().c_str(),settings->getScenario().c_str(),scenarioDir.c_str());
	}
    return scenarioDir;
}

string Game::extractFactionLogoFile(bool &loadingImageUsed, const string &factionName,
		string scenarioDir, const string &techName, Logger *logger, string factionLogoFilter) {
	string result = "";
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] Searching for faction loading screen\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	if(factionName == formatString(GameConstants::OBSERVER_SLOTNAME)) {
		string data_path = getGameReadWritePath(GameConstants::path_data_CacheLookupKey);
		const string factionLogo = data_path + "data/core/misc_textures/observer.jpg";
		//printf("In [%s::%s Line: %d] looking for loading screen '%s'\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,factionLogo.c_str());

		if(fileExists(factionLogo) == true) {
			if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found loading screen '%s'\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,factionLogo.c_str());

			result = factionLogo;
			if(logger != NULL) {
				logger->loadLoadingScreen(result);
			}
			loadingImageUsed = true;
		}
	}
	//else if(settings->getFactionTypeName(i) == formatString(GameConstants::RANDOMFACTION_SLOTNAME)) {
	else if(factionName == formatString(GameConstants::RANDOMFACTION_SLOTNAME)) {
		string data_path = getGameReadWritePath(GameConstants::path_data_CacheLookupKey);
		const string factionLogo = data_path + "data/core/misc_textures/random.jpg";

		if(fileExists(factionLogo) == true) {
			if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found loading screen '%s'\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,factionLogo.c_str());

			result = factionLogo;
			if(logger != NULL) {
				logger->loadLoadingScreen(result);
			}
			loadingImageUsed = true;
		}
	}
	else {
		Config &config = Config::getInstance();
		vector<string> pathList = config.getPathListForType(ptTechs,scenarioDir);
		for(int idx = 0; idx < (int)pathList.size(); idx++) {
			string currentPath = pathList[idx];
			endPathWithSlash(currentPath);
			//string path = currentPath + techName + "/" + "factions" + "/" + settings->getFactionTypeName(i);
			string path = currentPath + techName + "/" + "factions" + "/" + factionName;
			if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] possible loading screen dir '%s'\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,path.c_str());
			if(isdir(path.c_str()) == true) {
				endPathWithSlash(path);

				vector<string> loadScreenList;
				string logoFullPathFilter = path + factionLogoFilter;
				findAll(logoFullPathFilter, loadScreenList, false, false);
				if(loadScreenList.empty() == false) {
					int bestLogoIndex = 0;

					if(loadScreenList.size() > 1 && EndsWith(factionLogoFilter, ".xml") == false) {
						if(SystemFlags::VERBOSE_MODE_ENABLED) printf("\nLooking for best logo from a list of: " MG_SIZE_T_SPECIFIER " using filter: [%s]\n",loadScreenList.size(),logoFullPathFilter.c_str());


						int bestMinWidthDiff = INT_MAX;
						int bestMinHeightDiff = INT_MAX;
						// Now find the best texture for our screen
						// Texture2D *result = preloadTexture(logoFilename);
						for(unsigned int logoIndex = 0; logoIndex < (unsigned int)loadScreenList.size(); ++logoIndex) {
							string factionLogo = path + loadScreenList[logoIndex];
							if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] looking for loading screen '%s'\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,factionLogo.c_str());

							if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Looking for best logo: %u [%s]\n",logoIndex,factionLogo.c_str());

							if(fileExists(factionLogo) == true) {
								if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found loading screen '%s'\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,factionLogo.c_str());

								Texture2D *checkLogo = Renderer::preloadTexture(factionLogo);
								if(checkLogo != NULL) {
									const Metrics &metrics= Metrics::getInstance();
									int minWidthDifference = abs(metrics.getScreenW() - checkLogo->getPixmapConst()->getW());
									int minHeightDifference = abs(metrics.getScreenH() - checkLogo->getPixmapConst()->getH());

									if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Logo info: %d x %d (%d,%d)\n",checkLogo->getPixmapConst()->getW(),checkLogo->getPixmapConst()->getH(),minWidthDifference,minHeightDifference);

									if(minWidthDifference < bestMinWidthDiff) {
										bestMinWidthDiff = minWidthDifference;

										bestLogoIndex = logoIndex;
										if(SystemFlags::VERBOSE_MODE_ENABLED) printf("#1 New best logo is [%s]\n",factionLogo.c_str());
									}
									else if(minWidthDifference == bestMinWidthDiff &&
											minHeightDifference < bestMinHeightDiff) {
										bestMinHeightDiff = minHeightDifference;

										bestLogoIndex = logoIndex;
										if(SystemFlags::VERBOSE_MODE_ENABLED) printf("#2 New best logo is [%s]\n",factionLogo.c_str());
									}
								}
							}
						}
					}

					string factionLogo = path + loadScreenList[bestLogoIndex];
					if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] looking for loading screen '%s'\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,factionLogo.c_str());

					if(fileExists(factionLogo) == true) {
						if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found loading screen '%s'\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,factionLogo.c_str());

						result = factionLogo;
						if(logger != NULL) {
							logger->loadLoadingScreen(result);
						}
						loadingImageUsed = true;
						break;
					}
				}
				// Check if this is a linked faction
				else {
					//!!!
					string factionXMLFile = path + factionName + ".xml";

					//printf("A factionXMLFile [%s]\n",factionXMLFile.c_str());

					if(fileExists(factionXMLFile) == true) {
						XmlTree	xmlTreeFaction(XML_RAPIDXML_ENGINE);
						std::map<string,string> mapExtraTagReplacementValues;
						xmlTreeFaction.load(factionXMLFile, Properties::getTagReplacementValues(&mapExtraTagReplacementValues),true,true);

						const XmlNode *rootNode= xmlTreeFaction.getRootNode();

						//printf("B factionXMLFile [%s] root name [%s] root first child name [%s]\n",factionXMLFile.c_str(),rootNode->getName().c_str(),rootNode->getChild(0)->getName().c_str());
						//printf("B factionXMLFile [%s] root name [%s]\n",factionXMLFile.c_str(),rootNode->getName().c_str());
						if(rootNode->hasChild("link") == true) {
							rootNode = rootNode->getChild("link");
						}
						if(rootNode->getName() == "link" && rootNode->hasChild("techtree") == true) {
							const XmlNode *linkNode = rootNode;

							//printf("C factionXMLFile [%s]\n",factionXMLFile.c_str());

							//if(linkNode->hasChild("techtree") == true) {
							const XmlNode *techtreeNode = linkNode->getChild("techtree");

							string linkedTechTreeName = techtreeNode->getAttribute("name")->getValue();

							//printf("D factionXMLFile [%s] linkedTechTreeName [%s]\n",factionXMLFile.c_str(),linkedTechTreeName.c_str());

							if(linkedTechTreeName != "") {

								string linkedTechTreePath=TechTree::findPath(linkedTechTreeName,pathList);
								string techTreePath=linkedTechTreePath;
								endPathWithSlash(techTreePath);

								string linkedCurrentPath = techTreePath + "factions/" + factionName;
								endPathWithSlash(linkedCurrentPath);
								//string linkedTmppath= linkedCurrentPath + factionName +".xml";

								path = linkedCurrentPath;
								if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] possible loading screen dir '%s'\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,path.c_str());

								//printf("D1 idx = %d\ncurrentPath [%s]\npath [%s]\npathList[idx] [%s]\n",idx,currentPath.c_str(),path.c_str(),pathList[idx].c_str());

								if(isdir(path.c_str()) == true) {
									endPathWithSlash(path);

									//printf("E path [%s]\n",path.c_str());

									loadScreenList.clear();
									findAll(path + factionLogoFilter, loadScreenList, false, false);
									if(loadScreenList.empty() == false) {
										string factionLogo = path + loadScreenList[0];
										if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] looking for loading screen '%s'\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,factionLogo.c_str());

										//printf("F factionLogo [%s]\n",factionLogo.c_str());

										if(fileExists(factionLogo) == true) {
											if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found loading screen '%s'\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,factionLogo.c_str());

											result = factionLogo;
											if(logger != NULL) {
												logger->loadLoadingScreen(result);
											}
											loadingImageUsed = true;
											break;
										}
									}
								}
							}
							//}
						}
					}
				}
			}

			if(loadingImageUsed == true) {
				break;
			}
		}
	}
		//break;
		//}
	//}
	return result;
}

string Game::extractTechLogoFile(string scenarioDir, const string &techName,
		bool &loadingImageUsed, Logger *logger,const string &factionLogoFilter) {
	string result = "";
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] Searching for tech loading screen\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
    Config &config = Config::getInstance();
    vector<string> pathList = config.getPathListForType(ptTechs, scenarioDir);
    for(int idx = 0; idx < (int)pathList.size(); idx++) {
		string currentPath = pathList[idx];
		endPathWithSlash(currentPath);
		string path = currentPath + techName;
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] possible loading screen dir '%s'\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,path.c_str());
		if(isdir(path.c_str()) == true) {
			endPathWithSlash(path);

			vector<string> loadScreenList;
			findAll(path + factionLogoFilter, loadScreenList, false, false);
			if(loadScreenList.empty() == false) {
				string factionLogo = path + loadScreenList[0];
				if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] looking for loading screen '%s'\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,factionLogo.c_str());

				if(fileExists(factionLogo) == true) {
					if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found loading screen '%s'\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,factionLogo.c_str());

					result = factionLogo;
					if(logger != NULL) {
						logger->loadLoadingScreen(result);
					}
					loadingImageUsed = true;
					break;
				}
			}
		}
		if(loadingImageUsed == true) {
			break;
		}
	}

    return result;
}

void Game::loadHudTexture(const GameSettings *settings)
{
	string factionName = "";
	string techName			= settings->getTech();
	string scenarioDir 		= extractDirectoryPathFromFile(settings->getScenarioDir());
	//printf("In loadHudTexture, scenarioDir [%s]\n",scenarioDir.c_str());

	for(int i=0; i < settings->getFactionCount(); ++i ) {
		if((settings->getFactionControl(i) == ctHuman) || (settings->getFactionControl(i) == ctNetwork
		        && settings->getThisFactionIndex() == i)){
			factionName= settings->getFactionTypeName(i);
			break;
		}
	}
	if(factionName != "") {
		bool hudFound = false;
		Config &config= Config::getInstance();
		vector<string> pathList= config.getPathListForType(ptTechs, scenarioDir);
		for(int idx= 0; hudFound == false && idx < (int)pathList.size(); idx++){
			string currentPath= pathList[idx];
			endPathWithSlash(currentPath);

			vector<string> hudList;
			string path= currentPath + techName + "/" + "factions" + "/" + factionName;
			endPathWithSlash(path);
			findAll(path + GameConstants::HUD_SCREEN_FILE_FILTER, hudList, false, false);
			if(hudList.empty() == false){
				for(unsigned int hudIdx = 0; hudFound == false && hudIdx < (unsigned int)hudList.size(); ++hudIdx) {
					string hudImageFileName= path + hudList[hudIdx];
					if(SystemFlags::VERBOSE_MODE_ENABLED) printf("In [%s::%s Line: %d] looking for a HUD [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,hudImageFileName.c_str());
					if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled)
						SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] looking for a HUD [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,hudImageFileName.c_str());

					if(fileExists(hudImageFileName) == true){
						if(SystemFlags::VERBOSE_MODE_ENABLED) printf("In [%s::%s Line: %d] found HUD image [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,hudImageFileName.c_str());
						if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled)
							SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] found HUD image [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,hudImageFileName.c_str());

						Texture2D* texture= Renderer::findTexture(hudImageFileName);
						gui.setHudTexture(texture);
						hudFound = true;
						//printf("Hud texture found! \n");
						break;
					}
				}
			}
		}
	}
}

string Game::findFactionLogoFile(const GameSettings *settings, Logger *logger,
		const string &factionLogoFilter) {
	string result = "";
	if(settings == NULL) {
		result = "";
	}
	else {
		string mapName			= settings->getMap();
		string tilesetName		= settings->getTileset();
		string techName			= settings->getTech();
		string scenarioName		= settings->getScenario();
		bool loadingImageUsed	= false;

		if(logger != NULL) {
			logger->setState(Lang::getInstance().getString("Loading"));

			if(scenarioName.empty()) {
				string scenarioDir = extractDirectoryPathFromFile(settings->getScenarioDir());
				TechTree techTree(Config::getInstance().getPathListForType(ptTechs,scenarioDir));

				logger->setSubtitle(formatString(mapName) + " - " +
						formatString(tilesetName) + " - " + formatString(techTree.getTranslatedName(techName)));
			}
			else {
				logger->setSubtitle(formatString(scenarioName));
			}
		}

		//string scenarioDir = "";
		//bool skipCustomLoadScreen = false;
		//if(skipCustomLoadScreen == false) {
		string scenarioDir = extractScenarioLogoFile(settings, result, loadingImageUsed,
					logger, factionLogoFilter);
		//}
		// try to use a faction related loading screen
		//if(skipCustomLoadScreen == false && loadingImageUsed == false) {
		if(loadingImageUsed == false) {
			for(int i=0; i < settings->getFactionCount(); ++i ) {
				if( settings->getFactionControl(i) == ctHuman ||
					(settings->getFactionControl(i) == ctNetwork && settings->getThisFactionIndex() == i)) {

					result = extractFactionLogoFile(loadingImageUsed, settings->getFactionTypeName(i),
						scenarioDir, techName, logger, factionLogoFilter);
					break;
				}
			}
		}

		// try to use a tech related loading screen
		//if(skipCustomLoadScreen == false && loadingImageUsed == false){
		if(loadingImageUsed == false) {
			result = extractTechLogoFile(scenarioDir, techName,
					loadingImageUsed, logger, factionLogoFilter);
		}
	}
	return result;
}

vector<Texture2D *> Game::processTech(string techName) {
	vector<Texture2D *> logoFiles;
	bool enableFactionTexturePreview = Config::getInstance().getBool("FactionPreview","true");
	if(enableFactionTexturePreview) {
		//string currentTechName_factionPreview = techName;

		vector<string> factions;
		vector<string> techPaths = Config::getInstance().getPathListForType(ptTechs);
		for(int idx = 0; idx < (int)techPaths.size(); idx++) {
			string &techPath = techPaths[idx];
			endPathWithSlash(techPath);
			findAll(techPath + techName + "/factions/*.", factions, false, false);

			if(factions.empty() == false) {
				for(unsigned int factionIdx = 0; factionIdx < (unsigned int)factions.size(); ++factionIdx) {
					bool loadingImageUsed = false;
					string currentFactionName_factionPreview = factions[factionIdx];

					string factionLogo = Game::extractFactionLogoFile(
							loadingImageUsed,
							currentFactionName_factionPreview,
							"",
							techName,
							NULL,
							GameConstants::PREVIEW_SCREEN_FILE_FILTER);

					if(factionLogo == "") {
						factionLogo = Game::extractFactionLogoFile(
								loadingImageUsed,
								currentFactionName_factionPreview,
								"",
								techName,
								NULL,
								GameConstants::LOADING_SCREEN_FILE_FILTER);
					}
					if(factionLogo != "") {
						Texture2D *texture = Renderer::preloadTexture(factionLogo);
						logoFiles.push_back(texture);
					}
				}
			}
		}
	}

	return logoFiles;
}

void Game::load() {
	load(lgt_All);
}

void Game::load(int loadTypes) {
	bool showPerfStats = Config::getInstance().getBool("ShowPerfStats","false");
	Chrono chronoPerf;
	if(showPerfStats) chronoPerf.start();
	char perfBuf[8096]="";
	std::vector<string> perfList;

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	FileCRCPreCacheThread * &preCacheCRCThreadPtr = CacheManager::getCachedItem< FileCRCPreCacheThread * >(GameConstants::preCacheThreadCacheLookupKey);
	if(preCacheCRCThreadPtr != NULL) {
		preCacheCRCThreadPtr->setPauseForGame(true);
	}

	std::map<string,vector<pair<string, string> > > loadedFileList;
	originalDisplayMsgCallback = NetworkInterface::getDisplayMessageFunction();
	NetworkInterface::setDisplayMessageFunction(ErrorDisplayMessage);

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] loadTypes = %d, gameSettings = [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,loadTypes,this->gameSettings.toString().c_str());

	SoundRenderer &soundRenderer= SoundRenderer::getInstance();
	soundRenderer.stopAllSounds(fadeMusicMilliseconds);

	BaseColorPickEntity::resetUniqueColors();

	Config &config = Config::getInstance();
	Logger &logger= Logger::getInstance();

	string mapName= gameSettings.getMap();
	string tilesetName= gameSettings.getTileset();
	string techName= gameSettings.getTech();
	string scenarioName= gameSettings.getScenario();
	string data_path= getGameReadWritePath(GameConstants::path_data_CacheLookupKey);
	// loadHints

	if(data_path != ""){
		endPathWithSlash(data_path);
	}

	string user_data_path = config.getString("UserData_Root","");
	if(user_data_path != "") {
		endPathWithSlash(user_data_path);
	}

	string englishFile=getGameCustomCoreDataPath(data_path, "data/lang/hint/hint_"+Lang::getInstance().getDefaultLanguage()+".lng");
	string languageFile=getGameCustomCoreDataPath(data_path, "data/lang/hint/hint_"+ Lang::getInstance().getLanguage() +".lng");
	string languageFileUserData=user_data_path + "data/lang/hint/hint_"+ Lang::getInstance().getLanguage() +".lng";

	if(fileExists(languageFileUserData) == true){
		languageFile=languageFileUserData;
	}
	if(fileExists(languageFile) == false){
		// if there is no language specific file use english instead
		languageFile=englishFile;
	}
	if(fileExists(englishFile) == false){
		SystemFlags::OutputDebug(SystemFlags::debugError,"In [%s::%s Line: %d] file [%s] not found\n",__FILE__,__FUNCTION__,__LINE__,englishFile.c_str());
	}
	else {
		logger.loadGameHints(englishFile,languageFile,true);

		::Shared::Platform::Window::handleEvent();
		SDL_PumpEvents();
	}

	if((loadTypes & lgt_FactionPreview) == lgt_FactionPreview) {
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
		Game::findFactionLogoFile(&gameSettings, &logger);

		::Shared::Platform::Window::handleEvent();
		SDL_PumpEvents();
	}

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	loadHudTexture(&gameSettings);

	const string markCellTextureFilename = data_path + "data/core/misc_textures/mark_cell.png";
	markCellTexture = Renderer::findTexture(markCellTextureFilename);
	const string unmarkCellTextureFilename = data_path + "data/core/misc_textures/unmark_cell.png";
	unmarkCellTexture = Renderer::findTexture(unmarkCellTextureFilename);
	const string highlightCellTextureFilename = data_path + "data/core/misc_textures/pointer.png";
	highlightCellTexture = Renderer::findTexture(highlightCellTextureFilename);

    string scenarioDir = "";
    if(gameSettings.getScenarioDir() != "") {
    	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
        scenarioDir = gameSettings.getScenarioDir();
        if(EndsWith(scenarioDir, ".xml") == true) {
            scenarioDir = scenarioDir.erase(scenarioDir.size() - 4, 4);
            scenarioDir = scenarioDir.erase(scenarioDir.size() - gameSettings.getScenario().size(), gameSettings.getScenario().size() + 1);
        }
    }

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

    if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	//tileset
	if((loadTypes & lgt_TileSet) == lgt_TileSet) {
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
		world.loadTileset(	config.getPathListForType(ptTilesets,scenarioDir),
    						tilesetName, &checksum, loadedFileList);
	}

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

    // give CPU time to update other things to avoid apperance of hanging
    sleep(0);
    ::Shared::Platform::Window::handleEvent();
	SDL_PumpEvents();

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

    set<string> factions;
	for ( int i=0; i < gameSettings.getFactionCount(); ++i ) {
		factions.insert(gameSettings.getFactionTypeName(i));
	}

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

    if((loadTypes & lgt_TechTree) == lgt_TechTree) {
    	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

		//tech, load before map because of resources
		world.loadTech(	config.getPathListForType(ptTechs,scenarioDir), techName,
						factions, &checksum,loadedFileList);

		if(world.getTechTree() == NULL || world.getTechTree()->getNameUntranslated() == "") {
			char szBuf[8096]="";
			snprintf(szBuf,8096,"Line ref: %d, ERROR: Cannot find techtree: [%s]",__LINE__,techName.c_str());

			throw megaglest_runtime_error(szBuf, true);
		}
    }

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

    // give CPU time to update other things to avoid apperance of hanging
    sleep(0);
    ::Shared::Platform::Window::handleEvent();
	SDL_PumpEvents();

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

    //map
    if((loadTypes & lgt_Map) == lgt_Map) {
    	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
    	world.loadMap(Config::getMapPath(mapName,scenarioDir), &checksum);
    }

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

    // give CPU time to update other things to avoid apperance of hanging
    sleep(0);
    ::Shared::Platform::Window::handleEvent();
	SDL_PumpEvents();

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

    //scenario
    if((loadTypes & lgt_Scenario) == lgt_Scenario) {
    	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
		if(scenarioName.empty() == false) {

			bool isTutorial = Scenario::isGameTutorial(gameSettings.getScenarioDir());
			//printf("Loading scenario gameSettings.getScenarioDir() [%s] scenarioName [%s] isTutorial: %d\n",gameSettings.getScenarioDir().c_str(),scenarioName.c_str(),isTutorial);

			Lang::getInstance().loadScenarioStrings(gameSettings.getScenarioDir(), scenarioName, isTutorial);

			//printf("In [%s::%s Line: %d] rootNode [%p][%s]\n",__FILE__,__FUNCTION__,__LINE__,loadGameNode,(loadGameNode != NULL ? loadGameNode->getName().c_str() : "none"));
			world.loadScenario(gameSettings.getScenarioDir(), &checksum, false,loadGameNode);
		}
    }

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

    // give CPU time to update other things to avoid apperance of hanging
    sleep(0);
	SDL_PumpEvents();

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
    //good_fpu_control_registers(NULL,extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	if(showPerfStats && chronoPerf.getMillis() >= 50) {
		for(unsigned int x = 0; x < perfList.size(); ++x) {
			printf("%s",perfList[x].c_str());
		}
	}
}

void Game::init() {
	init(false);
}

void Game::init(bool initForPreviewOnly) {
	bool showPerfStats = Config::getInstance().getBool("ShowPerfStats","false");
	Chrono chronoPerf;
	if(showPerfStats) chronoPerf.start();
	char perfBuf[8096]="";
	std::vector<string> perfList;

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] initForPreviewOnly = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,initForPreviewOnly);

	Lang &lang= Lang::getInstance();
	Logger &logger= Logger::getInstance();
	CoreData &coreData= CoreData::getInstance();
	Renderer &renderer= Renderer::getInstance();
	Map *map= world.getMap();
	NetworkManager &networkManager= NetworkManager::getInstance();

	GameSettings::playerDisconnectedText = "*" + lang.getString("AI") + "* ";

	if(map == NULL) {
		throw megaglest_runtime_error("map == NULL");
	}

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	if(initForPreviewOnly == false) {
		logger.setState(lang.getString("Initializing"));

		//message box
		mainMessageBox.init(lang.getString("Yes"), lang.getString("No"));
		mainMessageBox.setEnabled(false);

		//message box
		errorMessageBox.init(lang.getString("Ok"));
		errorMessageBox.setEnabled(false);
		errorMessageBox.setY(20);


		//init world, and place camera
		commander.init(&world);

		// give CPU time to update other things to avoid apperance of hanging
		sleep(0);
		::Shared::Platform::Window::handleEvent();
		SDL_PumpEvents();
	}

	try {
		world.init(this, gameSettings.getDefaultUnits());
	}
	catch(const megaglest_runtime_error &ex) {
		string sErrBuf = "";
		if(ex.wantStackTrace() == true) {
			char szErrBuf[8096]="";
			snprintf(szErrBuf,8096,"In [%s::%s %d]",__FILE__,__FUNCTION__,__LINE__);
			sErrBuf = string(szErrBuf) + string("\nerror [") + string(ex.what()) + string("]\n");
		}
		else {
			sErrBuf = ex.what();
		}
		SystemFlags::OutputDebug(SystemFlags::debugError,sErrBuf.c_str());
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,sErrBuf.c_str());

		if(errorMessageBox.getEnabled() == false) {
			ErrorDisplayMessage(ex.what(),true);
		}
	}
	catch(const exception &ex) {
		char szErrBuf[8096]="";
		snprintf(szErrBuf,8096,"In [%s::%s %d]",__FILE__,__FUNCTION__,__LINE__);
		string sErrBuf = string(szErrBuf) + string("\nerror [") + string(ex.what()) + string("]\n");
		SystemFlags::OutputDebug(SystemFlags::debugError,sErrBuf.c_str());
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,sErrBuf.c_str());

		if(errorMessageBox.getEnabled() == false) {
			ErrorDisplayMessage(ex.what(),true);
		}
	}

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	if(loadGameNode != NULL) {
		//world.getMapPtr()->loadGame(loadGameNode,&world);
	}

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	if(initForPreviewOnly == false) {
		// give CPU time to update other things to avoid apperance of hanging
		sleep(0);
		::Shared::Platform::Window::handleEvent();
		SDL_PumpEvents();

		gui.init(this);

		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

		// give CPU time to update other things to avoid apperance of hanging
		sleep(0);
		//SDL_PumpEvents();

		chatManager.init(&console, world.getThisTeamIndex());
		console.clearStoredLines();
	}

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	if(this->loadGameNode == NULL) {
		initCamera(map);
	}
	else {
		gui.loadGame(loadGameNode,&world);

		if(inJoinGameLoading == true) {
			initCamera(map);
		}
	}

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	NetworkRole role = nrIdle;
	if(initForPreviewOnly == false) {
		// give CPU time to update other things to avoid apperance of hanging
		sleep(0);
		::Shared::Platform::Window::handleEvent();
		SDL_PumpEvents();

		scriptManager.init(&world, &gameCamera,loadGameNode);

		//good_fpu_control_registers(NULL,extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] creating AI's\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

		//create AIs

		bool enableServerControlledAI 	= this->gameSettings.getEnableServerControlledAI();
		bool isNetworkGame 				= this->gameSettings.isNetworkGame();
		role 							= networkManager.getNetworkRole();

		masterController.clearSlaves(true);
		deleteValues(aiInterfaces.begin(), aiInterfaces.end());

		std::vector<SlaveThreadControllerInterface *> slaveThreadList;
		aiInterfaces.resize(world.getFactionCount());
		for(int i=0; i < world.getFactionCount(); ++i) {
			Faction *faction= world.getFaction(i);

			//printf("Controltype = %d for index = %d\n",faction->getControlType(),i);

			if(faction->getCpuControl(enableServerControlledAI,isNetworkGame,role) == true) {
				//printf("** Loading AI player for Controltype = %d for index = %d\n",faction->getControlType(),i);

				aiInterfaces[i]= new AiInterface(*this, i, faction->getTeam());
				if(loadGameNode != NULL) {
					aiInterfaces[i]->loadGame(loadGameNode,faction);
				}
				char szBuf[8096]="";
				snprintf(szBuf,8096,Lang::getInstance().getString("LogScreenGameLoadingCreatingAIFaction","",true).c_str(),i);
				logger.add(szBuf, true);

				slaveThreadList.push_back(aiInterfaces[i]->getWorkerThread());
			}
			else {
				aiInterfaces[i]= NULL;
			}
		}
		if(Config::getInstance().getBool("EnableNewThreadManager","false") == true) {
			masterController.setSlaves(slaveThreadList);
		}

		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

		// give CPU time to update other things to avoid apperance of hanging
		sleep(0);
		::Shared::Platform::Window::handleEvent();
		SDL_PumpEvents();

		if(world.getFactionCount() == 1 && world.getFaction(0)->getPersonalityType() == fpt_Observer) {
			withRainEffect = false;
		}

		if(withRainEffect) {
			//weather particle systems
			if(world.getTileset()->getWeather() == wRainy) {
				logger.add(Lang::getInstance().getString("LogScreenGameLoadingCreatingRainParticles","",true), true);
				weatherParticleSystem= new RainParticleSystem();
				weatherParticleSystem->setSpeed(12.f / GameConstants::updateFps);
				weatherParticleSystem->setPos(gameCamera.getPos());
				renderer.manageParticleSystem(weatherParticleSystem, rsGame);
			}
			else if(world.getTileset()->getWeather() == wSnowy) {
				logger.add(Lang::getInstance().getString("LogScreenGameLoadingCreatingSnowParticles","",true), true);
				weatherParticleSystem= new SnowParticleSystem(1200);
				weatherParticleSystem->setSpeed(1.5f / GameConstants::updateFps);
				weatherParticleSystem->setPos(gameCamera.getPos());
				weatherParticleSystem->setTexture(coreData.getSnowTexture());
				renderer.manageParticleSystem(weatherParticleSystem, rsGame);
			}
		}
		else if(world.getTileset()->getWeather() == wRainy) {
			world.getTileset()->setWeather(wSunny);
		}

		renderer.manageDeferredParticleSystems();
    }

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	//init renderer state
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s] Initializing renderer\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__);
	logger.add(Lang::getInstance().getString("LogScreenGameLoadingInitRenderer","",true), true);

	//printf("Before renderer.initGame\n");
	renderer.initGame(this,this->getGameCameraPtr());
	//printf("After renderer.initGame\n");

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	for(int i=0; i < world.getFactionCount(); ++i) {
		Faction *faction= world.getFaction(i);
		if(faction != NULL) {
			faction->deletePixels();
		}
	}

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	if(initForPreviewOnly == false) {
		//good_fpu_control_registers(NULL,extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

		// give CPU time to update other things to avoid apperance of hanging
		sleep(0);
		::Shared::Platform::Window::handleEvent();
		SDL_PumpEvents();

		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s] Waiting for network\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__);
		logger.add(Lang::getInstance().getString("LogScreenGameLoadingWaitForNetworkPlayers","",true), true);
		networkManager.getGameNetworkInterface()->waitUntilReady(&checksum);

		//std::string worldLog = world.DumpWorldToLog(true);

		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] Starting music stream\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
		logger.add(Lang::getInstance().getString("LogScreenGameLoadingStartingMusic","",true), true);

		if(this->masterserverMode == false) {
			if(world.getThisFaction() == NULL) {
				throw megaglest_runtime_error("world.getThisFaction() == NULL");
			}
			if(world.getThisFaction()->getType() == NULL) {
				throw megaglest_runtime_error("world.getThisFaction()->getType() == NULL");
			}
			//if(world.getThisFaction()->getType()->getMusic() == NULL) {
			//	throw megaglest_runtime_error("world.getThisFaction()->getType()->getMusic() == NULL");
			//}
		}

		//sounds
		SoundRenderer &soundRenderer= SoundRenderer::getInstance();
		soundRenderer.stopAllSounds(fadeMusicMilliseconds);
		soundRenderer= SoundRenderer::getInstance();

		Tileset *tileset= world.getTileset();
		AmbientSounds *ambientSounds= tileset->getAmbientSounds();

		//rain
		if(tileset->getWeather() == wRainy && ambientSounds->isEnabledRain()) {
			logger.add(Lang::getInstance().getString("LogScreenGameLoadingStartingAmbient","",true), true);
			currentAmbientSound = ambientSounds->getRain();
			//printf("In [%s:%s] Line: %d currentAmbientSound = [%p]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,currentAmbientSound);
			soundRenderer.playAmbient(currentAmbientSound);
		}

		//snow
		if(tileset->getWeather() == wSnowy && ambientSounds->isEnabledSnow()) {
			logger.add(Lang::getInstance().getString("LogScreenGameLoadingStartingAmbient","",true), true);
			currentAmbientSound = ambientSounds->getSnow();
			//printf("In [%s:%s] Line: %d currentAmbientSound = [%p]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,currentAmbientSound);
			soundRenderer.playAmbient(currentAmbientSound);
		}

		if(this->masterserverMode == false) {
			StrSound *gameMusic= world.getThisFaction()->getType()->getMusic();
			soundRenderer.playMusic(gameMusic);
		}

		logger.add(Lang::getInstance().getString("LogScreenGameLoadingLaunchGame","",true));
	}

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	//throw "test";

	logger.setCancelLoadingEnabled(false);

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"================ STARTING GAME ================\n");
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPathFinder).enabled) SystemFlags::OutputDebug(SystemFlags::debugPathFinder,"================ STARTING GAME ================\n");
	setupPopupMenus(false);

	for(int i=0; i < world.getFactionCount(); ++i) {
		Faction *faction= world.getFaction(i);

		//printf("Check for team switch to observer i = %d, team = %d [%d]\n",i,faction->getTeam(),(GameConstants::maxPlayers -1 + fpt_Observer));
		if(faction != NULL && faction->getTeam() == GameConstants::maxPlayers -1 + fpt_Observer) {
			faction->setPersonalityType(fpt_Observer);
			world.getStats()->setPersonalityType(i, faction->getPersonalityType());
		}
	}

	if(showPerfStats) {
		sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
		perfList.push_back(perfBuf);
	}

	if(role == nrClient) {
		ClientInterface *clientInterface = dynamic_cast<ClientInterface *>(networkManager.getClientInterface());
		if(clientInterface != NULL && clientInterface->getResumeInGameJoin() == true) {

			//printf("Client sending resume message to server...\n");

			clientInterface->sendResumeGameMessage();
			//this->initialResumeSpeedLoops = true;
		}
	}

	printf("Game unique identifier is: %s\n",this->gameSettings.getGameUUID().c_str());

	gameStarted = true;

	if(this->masterserverMode == true) {
		world.getStats()->setIsMasterserverMode(true);

		printf("New game has started...\n");
	}

	if(isFlagType1BitEnabled(ft1_network_synch_checks_verbose) == true) {
		printf("*Note: Monitoring Network CRC VERBOSE synchronization...\n");
	}
	else if(isFlagType1BitEnabled(ft1_network_synch_checks) == true) {
		printf("*Note: Monitoring Network CRC NORMAL synchronization...\n");
	}

	//NetworkRole role = networkManager.getNetworkRole();
	if(role == nrServer) {
		networkManager.initServerInterfaces(this);
	}

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] ==== START GAME ==== getCurrentPixelByteCount() = " MG_SIZE_T_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderer.getCurrentPixelByteCount());

	if(SystemFlags::getSystemSettingType(SystemFlags::debugWorldSynch).enabled) SystemFlags::OutputDebug(SystemFlags::debugWorldSynch,"=============================================\n");
	if(SystemFlags::getSystemSettingType(SystemFlags::debugWorldSynch).enabled) SystemFlags::OutputDebug(SystemFlags::debugWorldSynch,"==== START GAME ====\n");
	if(SystemFlags::getSystemSettingType(SystemFlags::debugWorldSynch).enabled) SystemFlags::OutputDebug(SystemFlags::debugWorldSynch,"=============================================\n");
	if(SystemFlags::getSystemSettingType(SystemFlags::debugWorldSynch).enabled) SystemFlags::OutputDebug(SystemFlags::debugWorldSynch,"Starting framecount: %d\n",world.getFrameCount());

	if(showPerfStats && chronoPerf.getMillis() >= 50) {
		for(unsigned int x = 0; x < perfList.size(); ++x) {
			printf("%s",perfList[x].c_str());
		}
	}
}

void Game::initCamera(Map *map){
	gameCamera.init(map->getW(), map->getH());

	// camera default height calculation
	if(map->getCameraHeight()>0 && gameCamera.getCalculatedDefault()<map->getCameraHeight()){
		gameCamera.setCalculatedDefault(map->getCameraHeight());
	}
	else if(gameCamera.getCalculatedDefault()<map->getMaxMapHeight()+13.0f){
		gameCamera.setCalculatedDefault(map->getMaxMapHeight()+13.0f);
	}

	if(world.getThisFaction() != NULL) {
		const Vec2i &v= map->getStartLocation(world.getThisFaction()->getStartLocationIndex());
		gameCamera.setPos(Vec2f(v.x, v.y+gameCamera.getCalculatedDefault()/2));
	}
}

// ==================== update ====================

void Game::reInitGUI() {
	gui.init(this);
}

void Game::setupPopupMenus(bool checkClientAdminOverrideOnly) {
	Lang &lang= Lang::getInstance();
	NetworkManager &networkManager= NetworkManager::getInstance();
	NetworkRole role = networkManager.getNetworkRole();
	ClientInterface *clientInterface = NULL;
	ServerInterface *serverInterface = NULL;

	bool allowAdminMenuItems = false;
	bool forceJoinInProgressUpdate = false;
	if(role == nrServer) {
		allowAdminMenuItems = true;

		if(disconnectPlayerPopupMenuIndex == -1) {
			serverInterface = dynamic_cast<ServerInterface *>(networkManager.getServerInterface());
			if(serverInterface != NULL && checkClientAdminOverrideOnly == true) {
				for(int i = 0; i < world.getFactionCount(); ++i) {
					Faction *faction = world.getFaction(i);

					MutexSafeWrapper safeMutex(serverInterface->getSlotMutex(faction->getStartLocationIndex()),CODE_AT_LINE);
					ConnectionSlot *slot =  serverInterface->getSlot(faction->getStartLocationIndex(),false);
					if(slot != NULL && slot->getConnectHasHandshaked() == true &&
							slot->getCurrentFrameCount() <= 0) {
						//printf("Connected slot can be disconnected: %d\n",slot->getPlayerIndex());

						forceJoinInProgressUpdate = true;
						break;
					}
				}
			}
		}
	}
	else if(role == nrClient) {
		clientInterface = dynamic_cast<ClientInterface *>(networkManager.getClientInterface());

		if(clientInterface != NULL &&
			(gameSettings.getMasterserver_admin() == clientInterface->getSessionKey() ||
					clientInterface->isMasterServerAdminOverride() == true)) {
			allowAdminMenuItems = true;
		}
	}

	if(checkClientAdminOverrideOnly == false ||
			forceJoinInProgressUpdate == true ||
			(clientInterface != NULL &&
			 (gameSettings.getMasterserver_admin() != clientInterface->getSessionKey() &&
					clientInterface->isMasterServerAdminOverride() == true))) {
		exitGamePopupMenuIndex = -1;
		joinTeamPopupMenuIndex = -1;
		pauseGamePopupMenuIndex = -1;
		saveGamePopupMenuIndex = -1;
		loadGamePopupMenuIndex = -1;
		keyboardSetupPopupMenuIndex = -1;
		disconnectPlayerPopupMenuIndex = -1;

		if(checkClientAdminOverrideOnly == true && clientInterface != NULL) {
			gameSettings.setMasterserver_admin(clientInterface->getSessionKey());
			gameSettings.setMasterserver_admin_faction_index(clientInterface->getPlayerIndex());
		}
		//PopupMenu popupMenu;
		std::vector<string> menuItems;
		menuItems.push_back("  " + lang.getString("ExitGameMenu") + "  ");
		exitGamePopupMenuIndex = (int)menuItems.size()-1;

		if((gameSettings.getFlagTypes1() & ft1_allow_team_switching) == ft1_allow_team_switching &&
			world.getThisFaction() != NULL && world.getThisFaction()->getPersonalityType() != fpt_Observer) {
			menuItems.push_back("  " + lang.getString("JoinOtherTeam") + "  ");
			joinTeamPopupMenuIndex = (int)menuItems.size()-1;
		}

		if(allowAdminMenuItems == true){
			menuItems.push_back("  " + lang.getString("PauseResumeGame") + "  ");
			pauseGamePopupMenuIndex= (int)menuItems.size() - 1;

			if(gameSettings.isNetworkGame() == false || gameSettings.getScenario() != "") {
				menuItems.push_back("  " + lang.getString("SaveGame") + "  ");
				saveGamePopupMenuIndex= (int)menuItems.size() - 1;
			}

			if(gameSettings.isNetworkGame() == true) {
				menuItems.push_back("  " + lang.getString("DisconnectNetorkPlayer") + "  ");
				disconnectPlayerPopupMenuIndex= (int)menuItems.size() - 1;
			}
		}
		menuItems.push_back("  " + lang.getString("KeyboardsetupL") + "  ");
		keyboardSetupPopupMenuIndex = (int)menuItems.size()-1;

		menuItems.push_back("  " + lang.getString("Cancel") + "  ");

		popupMenu.setW(100);
		popupMenu.setH(100);
		popupMenu.init("  " + lang.getString("GameMenuTitle") + "  ",menuItems);
		popupMenu.setEnabled(false);
		popupMenu.setVisible(false);

		popupMenuSwitchTeams.setEnabled(false);
		popupMenuSwitchTeams.setVisible(false);

		popupMenuDisconnectPlayer.setEnabled(false);
		popupMenuDisconnectPlayer.setVisible(false);
	}
}

void Game::processNetworkSynchChecksIfRequired() {
	bool isNetworkGame = this->gameSettings.isNetworkGame();
	if (isNetworkGame == true && NetworkManager::getInstance().getGameNetworkInterface() != NULL) {
		GameSettings *settings = world.getGameSettingsPtr();
		if(settings != NULL) {
			bool calculateNetworkCRC = false;

			if(isFlagType1BitEnabled(ft1_network_synch_checks) == true ||
				isFlagType1BitEnabled(ft1_network_synch_checks_verbose) == true) {
				calculateNetworkCRC = true;
			}

			if(calculateNetworkCRC == true) {
				NetworkManager &networkManager = NetworkManager::getInstance();
				NetworkRole role = networkManager.getNetworkRole();

				NetworkInterface *netIntf = networkManager.getGameNetworkInterface();
				for(int index = 0; index < GameConstants::maxPlayers; ++index) {
					if(index < world.getFactionCount()) {
						Faction *faction = world.getFaction(index);
						netIntf->setNetworkPlayerFactionCRC(index,faction->getCRC().getSum());

						if(settings != NULL) {
							if(isFlagType1BitEnabled(ft1_network_synch_checks_verbose) == true) {
								faction->addCRC_DetailsForWorldFrame(world.getFrameCount(),role == nrServer);
							}
							else if(isFlagType1BitEnabled(ft1_network_synch_checks) == true &&
									world.getFrameCount() % 20 == 0) {
								faction->addCRC_DetailsForWorldFrame(world.getFrameCount(),role == nrServer);
							}
						}
					}
					else {
						netIntf->setNetworkPlayerFactionCRC(index,0);
					}
				}
			}
		}
	}
}

//update
void Game::update() {
	try {
		if(currentUIState != NULL) {
			currentUIState->update();
		}

		bool showPerfStats = Config::getInstance().getBool("ShowPerfStats","false");
		Chrono chronoPerf;
		char perfBuf[8096]="";
		std::vector<string> perfList;
		if(showPerfStats) chronoPerf.start();

		if(showPerfStats) {
			sprintf(perfBuf,"=============== FRAME: %d In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",world.getFrameCount(),extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		Chrono chronoGamePerformanceCounts;
		Chrono chrono;
		if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled) chrono.start();

		// a) Updates non dependent on speed

		// set game stats for host
		NetworkManager &networkManager	= NetworkManager::getInstance();
		NetworkRole role 				= networkManager.getNetworkRole();
		if(role == nrServer) {
			ServerInterface *server = NetworkManager::getInstance().getServerInterface(false);
			if(server != NULL) {
				server->setGameStats(world.getStats());
			}
		}

		bool pendingQuitError = (quitPendingIndicator == true ||
								 (NetworkManager::getInstance().getGameNetworkInterface() != NULL &&
								  NetworkManager::getInstance().getGameNetworkInterface()->getQuit()));

		if(pendingQuitError == true &&
		   (this->masterserverMode == true ||
		    (mainMessageBox.getEnabled() == false && errorMessageBox.getEnabled() == false))) {
			if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

			quitTriggeredIndicator = true;
			return;
		}

		if(this->masterserverMode == false) {
			if(world.getFactionCount() > 0 && world.getThisFaction()->getFirstSwitchTeamVote() != NULL) {
				const SwitchTeamVote *vote = world.getThisFaction()->getFirstSwitchTeamVote();
				GameSettings *settings = world.getGameSettingsPtr();

				Lang &lang= Lang::getInstance();

				char szBuf[8096]="";
				if(lang.hasString("AllowPlayerJoinTeam") == true) {
					snprintf(szBuf,8096,lang.getString("AllowPlayerJoinTeam").c_str(),settings->getNetworkPlayerName(vote->factionIndex).c_str(),vote->oldTeam,vote->newTeam);
				}
				else {
					snprintf(szBuf,8096,"Allow player [%s] to join your team\n(changing from team# %d to team# %d)?",settings->getNetworkPlayerName(vote->factionIndex).c_str(),vote->oldTeam,vote->newTeam);
				}

				switchTeamConfirmMessageBox.setText(szBuf);
				switchTeamConfirmMessageBox.init(lang.getString("Yes"), lang.getString("No"));
				switchTeamConfirmMessageBox.setEnabled(true);

				world.getThisFactionPtr()->setCurrentSwitchTeamVoteFactionIndex(vote->factionIndex);
			}
		}

		//misc
		updateFps++;
		mouse2d= (mouse2d+1) % Renderer::maxMouse2dAnim;

		//console
		console.update();

		// b) Updates depandant on speed
		int updateLoops= getUpdateLoops();

		// Temp speed boost when player first joins an in progress game
		if(this->initialResumeSpeedLoops == true) {
			printf("Resume In Progress Game: %d\n",__LINE__);

			this->initialResumeSpeedLoops = false;
			//updateLoops = 80;
		}

		chronoGamePerformanceCounts.start();
		bool enableServerControlledAI 	= this->gameSettings.getEnableServerControlledAI();

		if(role == nrClient && updateLoops == 1 && world.getFrameCount() >= (gameSettings.getNetworkFramePeriod() * 2) ) {
			ClientInterface *clientInterface = dynamic_cast<ClientInterface *>(networkManager.getClientInterface());
			if(clientInterface != NULL) {
				uint64 lastNetworkFrameFromServer = clientInterface->getCachedLastPendingFrameCount();

				/////////////////////////////////
				// TTTT new attempt to make things smoother:
				///////////////

				////////////////////////////////////////////
				//get stats of received/waiting for packages
				////////////////////////////////////////////
				// calculate current receive Index slot:
				int index = ((world.getFrameCount()
						- (world.getFrameCount()
								% gameSettings.getNetworkFramePeriod()))
								/ gameSettings.getNetworkFramePeriod())
						% GameConstants::networkSmoothInterval;

				// clean the next frame slot
				receivedTooEarlyInFrames[(index+1)%GameConstants::networkSmoothInterval]=-1;
				framesNeededToWaitForServerMessage[(index+1)%GameConstants::networkSmoothInterval]=-1;

				if(receivedTooEarlyInFrames[index]==-1){
					// we need to check if we already received something for next frame
					if(lastNetworkFrameFromServer > 0 && lastNetworkFrameFromServer > (uint64)world.getFrameCount()) {
						receivedTooEarlyInFrames[index]= lastNetworkFrameFromServer-world.getFrameCount();
					}
				}
				if(framesNeededToWaitForServerMessage[index]==-1){
					// calc time waiting for message in milliseconds to frames
					int64 timeClientWaitedForLastMessage=clientInterface->getTimeClientWaitedForLastMessage();
					if(timeClientWaitedForLastMessage>0){
						if(SystemFlags::VERBOSE_MODE_ENABLED) printf("world.getFrameCount():%d index %d Client waited:%d ms\n",world.getFrameCount(),index,(int)timeClientWaitedForLastMessage);
						framesNeededToWaitForServerMessage[index]=timeClientWaitedForLastMessage*GameConstants::updateFps/1000;
						if(SystemFlags::VERBOSE_MODE_ENABLED) printf("ClienttimeClientWaitedForLastMessage:%d ms  which is %d frames \n",(int)timeClientWaitedForLastMessage,framesNeededToWaitForServerMessage[index]);
					}
					else {
						framesNeededToWaitForServerMessage[index]=0;
					}
				}

				////////////////////////////////////////////
				//use the recorded stats of received/waiting for packages
				////////////////////////////////////////////
				//lets see if the client is in front and had to wait for messages ...

				//lets see if all last recorded frames where received too early
				int minimum=0;
				int allowedMaxFallback=5;
				int countOfMessagesReceivedTooEarly=0;
				int countOfMessagesReceivedTooLate=0;
				int sumOfTooLateFrames=0;
				bool cleanupStats=false;

				for( int i=0;i<GameConstants::networkSmoothInterval;i++){
					if(receivedTooEarlyInFrames[i]>allowedMaxFallback){
						countOfMessagesReceivedTooEarly++;
						if ( minimum == 0 || minimum > receivedTooEarlyInFrames[i]  ){
							minimum=receivedTooEarlyInFrames[i];
						}
					}
					if(framesNeededToWaitForServerMessage[i]>0){
						countOfMessagesReceivedTooLate++;
						sumOfTooLateFrames+=framesNeededToWaitForServerMessage[i];
					}
				}

				if( countOfMessagesReceivedTooEarly==GameConstants::networkSmoothInterval-1 ) // -1 because slot for next frame is already initialized
				{// all packages where too early
					// we catch up the minimum-catchupInterval of what we recorded
					framesToCatchUpAsClient=minimum-allowedMaxFallback;
					framesToSlowDownAsClient=0;
					cleanupStats=true;
					if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Worldframe %d : Client will speed up: %d frames\n",world.getFrameCount(),framesToCatchUpAsClient);
				}
				else if(countOfMessagesReceivedTooLate>3){
					framesToSlowDownAsClient=sumOfTooLateFrames/countOfMessagesReceivedTooLate;
					framesToCatchUpAsClient=0;
					cleanupStats=true;
					if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Worldframe %d : Client will slow down: %d frames\n",world.getFrameCount(),framesToSlowDownAsClient);
				}

				if(cleanupStats==true) {
					// Once we decided to use the stats to do some correction, we reset/cleanup our recorded stats
					for( int i=0;i<GameConstants::networkSmoothInterval;i++){
						receivedTooEarlyInFrames[i]=-1;
						framesNeededToWaitForServerMessage[i]=-1;
					}
				}
			}
		}
		// if game is paused don't try to catch up
		if(updateLoops > 0) {
			// we catch up a bit smoother with updateLoops = 2
			if(framesToCatchUpAsClient>0)
			{
					updateLoops = 2;
					framesToCatchUpAsClient=framesToCatchUpAsClient-1;
			}
			if(framesToSlowDownAsClient>0)
			{// slowdown still the hard way.
				updateLoops = 0;
				framesToSlowDownAsClient=framesToSlowDownAsClient-1;
			}
		}

		addPerformanceCount("CalculateNetworkUpdateLoops",chronoGamePerformanceCounts.getMillis());

		if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s] Line: %d took msecs: %lld\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());
		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s] Line: %d took msecs: %lld [before ReplaceDisconnectedNetworkPlayersWithAI]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());
		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		// Check to see if we are playing a network game and if any players
		// have disconnected?
		bool isNetworkGame = this->gameSettings.isNetworkGame();

		chronoGamePerformanceCounts.start();

		ReplaceDisconnectedNetworkPlayersWithAI(isNetworkGame, role);

		addPerformanceCount("ReplaceDisconnectedNetworkPlayersWithAI",chronoGamePerformanceCounts.getMillis());

		setupPopupMenus(true);

		if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s] Line: %d took msecs: %lld [after ReplaceDisconnectedNetworkPlayersWithAI]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());
		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		if(updateLoops > 0) {
			// update the frame based timer in the stats with at least one step
			world.getStats()->addFramesToCalculatePlaytime();

			//update
			Chrono chronoReplay;
			int64 lastReplaySecond = -1;
			int replayCommandsPlayed = 0;
			int replayTotal = commander.getReplayCommandListForFrameCount();
			if(replayTotal > 0) {
				chronoReplay.start();
			}

			do {
				if(replayTotal > 0) {
					replayCommandsPlayed = (replayTotal - commander.getReplayCommandListForFrameCount());
				}
				for(int i = 0; i < updateLoops; ++i) {
					//if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled) chrono.start();
					if(showPerfStats) {
						sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
						perfList.push_back(perfBuf);
					}

					//AiInterface
					if(commander.hasReplayCommandListForFrame() == false) {
						chronoGamePerformanceCounts.start();

						processNetworkSynchChecksIfRequired();

						addPerformanceCount("CalculateNetworkCRCSynchChecks",chronoGamePerformanceCounts.getMillis());

						const bool newThreadManager = Config::getInstance().getBool("EnableNewThreadManager","false");
						if(newThreadManager == true) {
							int currentFrameCount = world.getFrameCount();
							masterController.signalSlaves(&currentFrameCount);
							//bool slavesCompleted = masterController.waitTillSlavesTrigger(20000);
							masterController.waitTillSlavesTrigger(20000);
						}
						else {
							// Signal the faction threads to do any pre-processing
							chronoGamePerformanceCounts.start();

							bool hasAIPlayer = false;
							for(int j = 0; j < world.getFactionCount(); ++j) {
								Faction *faction = world.getFaction(j);

								//printf("Faction Index = %d enableServerControlledAI = %d, isNetworkGame = %d, role = %d isCPU player = %d scriptManager.getPlayerModifiers(j)->getAiEnabled() = %d\n",j,enableServerControlledAI,isNetworkGame,role,faction->getCpuControl(enableServerControlledAI,isNetworkGame,role),scriptManager.getPlayerModifiers(j)->getAiEnabled());

								if(	faction->getCpuControl(enableServerControlledAI,isNetworkGame,role) == true &&
									scriptManager.getPlayerModifiers(j)->getAiEnabled() == true) {

									if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] [i = %d] faction = %d, factionCount = %d, took msecs: %lld [before AI updates]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,i,j,world.getFactionCount(),chrono.getMillis());
									aiInterfaces[j]->signalWorkerThread(world.getFrameCount());
									hasAIPlayer = true;
								}
							}

							if(showPerfStats) {
								sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
								perfList.push_back(perfBuf);
							}

							if(hasAIPlayer == true) {
								//sleep(0);

								Chrono chronoAI;
								chronoAI.start();

								const int MAX_FACTION_THREAD_WAIT_MILLISECONDS = 20000;
								for(;chronoAI.getMillis() < MAX_FACTION_THREAD_WAIT_MILLISECONDS;) {
									bool workThreadsFinished = true;
									for(int j = 0; j < world.getFactionCount(); ++j) {
										Faction *faction = world.getFaction(j);
										if(faction == NULL) {
											throw megaglest_runtime_error("faction == NULL");
										}
										if(	faction->getCpuControl(enableServerControlledAI,isNetworkGame,role) == true &&
											scriptManager.getPlayerModifiers(j)->getAiEnabled() == true) {
											if(aiInterfaces[j]->isWorkerThreadSignalCompleted(world.getFrameCount()) == false) {
												workThreadsFinished = false;
												break;
											}
										}
									}
									if(workThreadsFinished == false) {
										//sleep(0);
									}
									else {
										break;
									}
								}
							}

							addPerformanceCount("ProcessAIWorkerThreads",chronoGamePerformanceCounts.getMillis());
						}

						if(showPerfStats) {
							sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
							perfList.push_back(perfBuf);
						}

					}
					else {
						// Simply show a progress message while replaying commands
						if(lastReplaySecond < chronoReplay.getSeconds()) {
							lastReplaySecond = chronoReplay.getSeconds();
							Renderer &renderer= Renderer::getInstance();
							renderer.clearBuffers();
							renderer.clearZBuffer();
							renderer.reset2d();

							char szBuf[8096]="";
							snprintf(szBuf,8096,"Please wait, loading game with replay [%d / %d]...",replayCommandsPlayed,replayTotal);
							string text = szBuf;
							if(Renderer::renderText3DEnabled) {
								Font3D *font = CoreData::getInstance().getMenuFontBig3D();
								const Metrics &metrics= Metrics::getInstance();
								int w= metrics.getVirtualW();
								int renderX = (w / 2) - (font->getMetrics()->getTextWidth(text) / 2);
								int h= metrics.getVirtualH();
								int renderY = (h / 2) + (font->getMetrics()->getHeight(text) / 2);

								renderer.renderText3D(
									text, font,
									Vec3f(1.f, 1.f, 0.f),
									renderX, renderY, false);
							}
							else {
								Font2D *font = CoreData::getInstance().getMenuFontBig();
								const Metrics &metrics= Metrics::getInstance();
								int w= metrics.getVirtualW();
								int renderX = (w / 2);
								int h= metrics.getVirtualH();
								int renderY = (h / 2);

								renderer.renderText(
									text, font,
									Vec3f(1.f, 1.f, 0.f),
									renderX, renderY, true);
							}

							renderer.swapBuffers();
						}
					}

					if(showPerfStats) {
						sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
						perfList.push_back(perfBuf);
					}

					if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s] Line: %d took msecs: %lld [AI updates]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());
					if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

					//World
					chronoGamePerformanceCounts.start();

					if(pendingQuitError == false) world.update();

					addPerformanceCount("ProcessWorldUpdate",chronoGamePerformanceCounts.getMillis());

					if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s] Line: %d took msecs: %lld [world update i = %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis(),i);
					if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

					if(showPerfStats) {
						sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
						perfList.push_back(perfBuf);
					}

					if(currentCameraFollowUnit != NULL) {
						Vec3f c=currentCameraFollowUnit->getCurrMidHeightVector();
						int rotation=currentCameraFollowUnit->getRotation();
						float angle=rotation+180;

						c.z=c.z+4*std::cos(degToRad(angle));
						c.x=c.x+4*std::sin(degToRad(angle));

						c.y=c.y+currentCameraFollowUnit->getType()->getHeight()/2.f+2.0f;

						getGameCameraPtr()->setPos(c);

						rotation=(540-rotation)%360;
						getGameCameraPtr()->rotateToVH(18.0f,rotation);

						if(currentCameraFollowUnit->isAlive()==false){
							currentCameraFollowUnit=NULL;
							getGameCameraPtr()->setState(GameCamera::sGame);
						}
					}

					if(showPerfStats) {
						sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
						perfList.push_back(perfBuf);
					}

					// Commander
					chronoGamePerformanceCounts.start();

					if(pendingQuitError == false) {
						commander.signalNetworkUpdate(this);
					}

					addPerformanceCount("ProcessNetworkUpdate",chronoGamePerformanceCounts.getMillis());

					if(showPerfStats) {
						sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
						perfList.push_back(perfBuf);
					}

					if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s] Line: %d took msecs: %lld [commander updateNetwork i = %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis(),i);
					if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

					//Gui
					chronoGamePerformanceCounts.start();

					gui.update();

					addPerformanceCount("ProcessGUIUpdate",chronoGamePerformanceCounts.getMillis());

					if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s] Line: %d took msecs: %lld [gui updating i = %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis(),i);
					if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

					if(showPerfStats) {
						sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
						perfList.push_back(perfBuf);
					}

					//Particle systems
					if(weatherParticleSystem != NULL) {
						weatherParticleSystem->setPos(gameCamera.getPos());
					}

					if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s] Line: %d took msecs: %lld [weather particle updating i = %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis(),i);
					if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

					if(showPerfStats) {
						sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
						perfList.push_back(perfBuf);
					}

					Renderer &renderer= Renderer::getInstance();

					chronoGamePerformanceCounts.start();

					renderer.updateParticleManager(rsGame,avgRenderFps);

					addPerformanceCount("ProcessParticleManager",chronoGamePerformanceCounts.getMillis());

					if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s] Line: %d took msecs: %lld [particle manager updating i = %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis(),i);
					if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

					if(showPerfStats) {
						sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
						perfList.push_back(perfBuf);
					}

					//good_fpu_control_registers(NULL,extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
				}
			}
			while (commander.hasReplayCommandListForFrame() == true);
		}
		//else if(role == nrClient) {
		else {
			if(pendingQuitError == false) {
				commander.signalNetworkUpdate(this);
			}

			if(playingStaticVideo == true) {
				if(videoPlayer->isPlaying() == false) {
					playingStaticVideo = false;
					tryPauseToggle(false);
				}
			}
		}

		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		chronoGamePerformanceCounts.start();

		//call the chat manager
		chatManager.updateNetwork();
		if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s] Line: %d took msecs: %lld [chatManager.updateNetwork]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chrono.getMillis());
		if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		updateNetworkMarkedCells();
		updateNetworkUnMarkedCells();
		updateNetworkHighligtedCells();

		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		//check for quiting status
		if(NetworkManager::getInstance().getGameNetworkInterface() != NULL &&
			NetworkManager::getInstance().getGameNetworkInterface()->getQuit() &&
		   mainMessageBox.getEnabled() == false &&
		   errorMessageBox.getEnabled() == false) {
			if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
			quitTriggeredIndicator = true;
			return;
		}

		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		addPerformanceCount("ProcessMiscNetwork",chronoGamePerformanceCounts.getMillis());

		// START - Handle joining in progress games
		if(role == nrServer) {

			if(this->networkPauseGameForLaggedClientsRequested == true) {
				this->networkPauseGameForLaggedClientsRequested = false;

				if(getPaused() == false) {

					printf("[CMDR] Pausing game for lagging client(s), current world frame [%d]\n",world.getFrameCount());
					commander.tryPauseGame(false,false);
				}
			}
			else if(this->networkResumeGameForLaggedClientsRequested == true) {
				this->networkResumeGameForLaggedClientsRequested = false;

				if(getPaused() == true) {

					printf("[CMDR] Resuming game after Pause for lagging client(s), current world frame [%d]\n",world.getFrameCount());
					commander.tryResumeGame(false,false);
				}
			}

			ServerInterface *server = NetworkManager::getInstance().getServerInterface();
			if(server->getPauseForInGameConnection() == true) {

				bool clientNeedsGameSetup = false;
				for(int i = 0; i < world.getFactionCount(); ++i) {
					Faction *faction = world.getFaction(i);

					MutexSafeWrapper safeMutex(server->getSlotMutex(faction->getStartLocationIndex()),CODE_AT_LINE);
					ConnectionSlot *slot =  server->getSlot(faction->getStartLocationIndex(),false);
					if(slot != NULL && slot->getPauseForInGameConnection() == true) {
						clientNeedsGameSetup = true;
						break;
					}
				}

				if(pausedForJoinGame == false || clientNeedsGameSetup == true) {
					//printf("================= Switching player pausing game\n");

					for(int i = 0; i < world.getFactionCount(); ++i) {
						Faction *faction = world.getFaction(i);

						//printf("Switching player check %d from: %d connected: %d, startindex = %d, connected #2: %d\n",i,faction->getControlType(),server->isClientConnected(faction->getStartLocationIndex()),faction->getStartLocationIndex(),server->isClientConnected(i));
						//printf("Slot: %d faction name: %s\n",i,faction->getType()->getName().c_str());

						if(	faction->getControlType() != ctNetwork &&
							faction->getControlType() != ctHuman &&
							server->isClientConnected(faction->getStartLocationIndex()) == true) {

							//printf("Switching player %d from: %d to %d\n",i,faction->getControlType(),ctNetwork);
							//printf("Slot: %d faction name: %s GS faction: %s\n",i,faction->getType()->getName().c_str(),server->gameSettings.getFactionTypeName(i).c_str());

							server->gameSettings.setFactionControl(i,ctNetwork);

							MutexSafeWrapper safeMutex(server->getSlotMutex(faction->getStartLocationIndex()),CODE_AT_LINE);
							ConnectionSlot *slot =  server->getSlot(faction->getStartLocationIndex(),false);
							server->gameSettings.setNetworkPlayerName(i,slot->getName());
							server->gameSettings.setNetworkPlayerUUID(i,slot->getUUID());
							server->gameSettings.setNetworkPlayerPlatform(i,slot->getPlatform());
							safeMutex.ReleaseLock();
							server->gameSettings.setNetworkPlayerStatuses(i,npst_None);

							this->gameSettings.setFactionControl(i,ctNetwork);
							this->gameSettings.setNetworkPlayerName(i,server->gameSettings.getNetworkPlayerName(i));
							this->gameSettings.setNetworkPlayerUUID(i,server->gameSettings.getNetworkPlayerUUID(i));
							this->gameSettings.setNetworkPlayerPlatform(i,server->gameSettings.getNetworkPlayerPlatform(i));
							this->gameSettings.setNetworkPlayerStatuses(i,npst_None);
						}
					}
					//printf("#1 Data synch: lmap %u ltile: %d ltech: %u\n",gameSettings.getMapCRC(),gameSettings.getTilesetCRC(),gameSettings.getTechCRC());
					//printf("#2 Data synch: lmap %u ltile: %d ltech: %u\n",server->gameSettings.getMapCRC(),server->gameSettings.getTilesetCRC(),server->gameSettings.getTechCRC());
					server->broadcastGameSetup(&server->gameSettings,true);
				}

				for(int i = 0; i < world.getFactionCount(); ++i) {
					Faction *faction = world.getFaction(i);
					MutexSafeWrapper safeMutex(server->getSlotMutex(faction->getStartLocationIndex()),CODE_AT_LINE);
					ConnectionSlot *slot =  server->getSlot(faction->getStartLocationIndex(),false);
					if(slot != NULL && slot->getPauseForInGameConnection() == true) {
						slot->setPauseForInGameConnection(false);
					}
				}
			}
			else if(server->getStartInGameConnectionLaunch() == true) {
				//printf("^^^ getStartInGameConnectionLaunch triggered!\n");

				//server->setStartInGameConnectionLaunch(false);

				//this->speed = 1;

				//Lang &lang= Lang::getInstance();
				bool pauseAndSaveGameForNewClient = false;
				for(int i = 0; i < world.getFactionCount(); ++i) {
					Faction *faction = world.getFaction(i);

					MutexSafeWrapper safeMutex(server->getSlotMutex(faction->getStartLocationIndex()),CODE_AT_LINE);
					ConnectionSlot *slot =  server->getSlot(faction->getStartLocationIndex(),false);

					if(slot != NULL && slot->getStartInGameConnectionLaunch() == true) {
						//slot->setStartInGameConnectionLaunch(false);
						pauseAndSaveGameForNewClient = true;
					}
					if(slot != NULL && slot->getJoinGameInProgress() == true) {
						//printf("$$$ signalling client to start game [deleting AI player] factionIndex: %d slot: %d startlocation: %d!\n",i,slot->getPlayerIndex(),faction->getStartLocationIndex());
						safeMutex.ReleaseLock();

						this->gameSettings.setFactionControl(i,ctNetwork);
						this->gameSettings.setNetworkPlayerName(i,server->gameSettings.getNetworkPlayerName(i));
						this->gameSettings.setNetworkPlayerUUID(i,server->gameSettings.getNetworkPlayerUUID(i));
						this->gameSettings.setNetworkPlayerPlatform(i,server->gameSettings.getNetworkPlayerPlatform(i));

						if(this->gameSettings.getNetworkPlayerStatuses(i) == npst_Disconnected) {
							this->gameSettings.setNetworkPlayerStatuses(i,npst_None);
						}

						//printf("START Purging AI player for index: %d\n",i);
						masterController.clearSlaves(true);
						delete aiInterfaces[i];
						aiInterfaces[i] = NULL;
						//printf("END Purging AI player for index: %d\n",i);

						Faction *faction = world.getFaction(i);
						faction->setControlType(ctNetwork);
						//pauseAndSaveGameForNewClient = true;
					}
					else if((slot == NULL || slot->isConnected() == false) &&
							this->gameSettings.getFactionControl(i) == ctNetwork &&
							aiInterfaces[i] == NULL) {

						safeMutex.ReleaseLock();
						faction->setFactionDisconnectHandled(false);
						//this->gameSettings.setNetworkPlayerName(i,lang.getString("AI") + intToStr(i+1));
						//server->gameSettings.setNetworkPlayerName(i,lang.getString("AI") + intToStr(i+1));
					}
					else {
						safeMutex.ReleaseLock();
					}
				}

				if(pauseAndSaveGameForNewClient == true && pausedForJoinGame == false &&
						pauseRequestSent == false) {
					//printf("Pausing game for join in progress game...\n");

					commander.tryPauseGame(true,true);
					pauseRequestSent = true;
					return;
				}
			}
			//else if(server->getPauseForInGameConnection() == true && paused == true &&
			if(pausedForJoinGame == true) {
				if(pauseStateChanged == true) {
					pauseStateChanged = false;
				}

				if(server->getUnPauseForInGameConnection() == true) {
					//printf("^^^ getUnPauseForInGameConnection triggered!\n");

					for(int i = 0; i < world.getFactionCount(); ++i) {
						Faction *faction = world.getFaction(i);

						MutexSafeWrapper safeMutex(server->getSlotMutex(faction->getStartLocationIndex()),CODE_AT_LINE);
						ConnectionSlot *slot =  server->getSlot(faction->getStartLocationIndex(),false);
						if(slot != NULL && slot->getUnPauseForInGameConnection() == true) {
							slot->setUnPauseForInGameConnection(false);
							faction->setFactionDisconnectHandled(false);
						}
					}
					//printf("Resuming game for join in progress game resumeRequestSent: %d...\n",resumeRequestSent);

					if(pausedBeforeJoinGame == false && resumeRequestSent == false) {
						commander.tryResumeGame(true,true);
						resumeRequestSent = true;
					}
				}
				else if(server->getStartInGameConnectionLaunch() == true) {
					bool saveNetworkGame = false;

					ServerInterface *server = NetworkManager::getInstance().getServerInterface();
					for(int i = 0; i < world.getFactionCount(); ++i) {
						Faction *faction = world.getFaction(i);

						MutexSafeWrapper safeMutex(server->getSlotMutex(faction->getStartLocationIndex()),CODE_AT_LINE);
						ConnectionSlot *slot =  server->getSlot(faction->getStartLocationIndex(),false);
						if(slot != NULL && slot->getJoinGameInProgress() == true &&
							slot->getStartInGameConnectionLaunch() == true &&
							slot->getSentSavedGameInfo() == false) {
							slot->setStartInGameConnectionLaunch(false);

							saveNetworkGame = true;
							break;
						}
					}

					if(saveNetworkGame == true) {
						//printf("Saved network game to disk\n");

						string file = this->saveGame(GameConstants::saveNetworkGameFileServer,"temp/");

						string saveGameFilePath = "temp/";
						string saveGameFileCompressed = saveGameFilePath + string(GameConstants::saveNetworkGameFileServerCompressed);
						if(getGameReadWritePath(GameConstants::path_logs_CacheLookupKey) != "") {
							saveGameFilePath = getGameReadWritePath(GameConstants::path_logs_CacheLookupKey) + saveGameFilePath;
							saveGameFileCompressed = saveGameFilePath + string(GameConstants::saveNetworkGameFileServerCompressed);
						}
						else {
							string userData = Config::getInstance().getString("UserData_Root","");
							if(userData != "") {
								endPathWithSlash(userData);
							}
							saveGameFilePath = userData + saveGameFilePath;
							saveGameFileCompressed = saveGameFilePath + string(GameConstants::saveNetworkGameFileServerCompressed);
						}

						bool compressed_result = compressFileToZIPFile(
								file, saveGameFileCompressed);
						if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Saved game [%s] compressed to [%s] returned: %d\n",file.c_str(),saveGameFileCompressed.c_str(), compressed_result);

						char szBuf[8096]="";
						Lang &lang= Lang::getInstance();
						snprintf(szBuf,8096,lang.getString("GameSaved","",true).c_str(),file.c_str());
						console.addLine(szBuf);

						for(int i = 0; i < world.getFactionCount(); ++i) {
							Faction *faction = world.getFaction(i);

							MutexSafeWrapper safeMutex(server->getSlotMutex(faction->getStartLocationIndex()),CODE_AT_LINE);
							ConnectionSlot *slot =  server->getSlot(faction->getStartLocationIndex(),false);
							if(slot != NULL && slot->getJoinGameInProgress() == true &&
									slot->getSentSavedGameInfo() == false) {

								safeMutex.ReleaseLock();
							    NetworkMessageReady networkMessageReady(0);
								slot->sendMessage(&networkMessageReady);

								slot =  server->getSlot(faction->getStartLocationIndex(),false);
								if(slot != NULL) {
									slot->setSentSavedGameInfo(true);
								}
							}
						}
					}
				}
			}
			//else {
			// handle setting changes from clients
			Map *map= world.getMap();
			//printf("switchSetupRequests != NULL\n");

			bool switchRequested = switchSetupForSlots(server, 0, map->getMaxPlayers(), false);
			switchRequested = switchRequested || switchSetupForSlots(server, map->getMaxPlayers(), GameConstants::maxPlayers, true);

			if(switchRequested == true) {
				//printf("Send new game setup from switch: %d\n",switchRequested);

				//for(int i= 0; i < gameSettings.getFactionCount(); ++i) {
					//printf("#1 Faction Index: %d control: %d startlocation: %d\n",i,gameSettings.getFactionControl(i),gameSettings.getStartLocationIndex(i));

					//printf("#2 Faction Index: %d control: %d startlocation: %d\n",i,server->gameSettings.getFactionControl(i),server->gameSettings.getStartLocationIndex(i));
				//}

				server->broadcastGameSetup(&server->gameSettings,true);
				}
			//}

			// Make the server wait a bit for clients to start.
			if(pausedForJoinGame == false && resumeRequestSent == true) {
				resumeRequestSent = false;
				//sleep(500);
			}
		}
		// END - Handle joining in progress games

		//update auto test
		if(Config::getInstance().getBool("AutoTest")){
			AutoTest::getInstance().updateGame(this);
			return;
		}

		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		if(world.getQueuedScenario() != "") {
			string name = world.getQueuedScenario();
			bool keepFactions = world.getQueuedScenarioKeepFactions();
			world.setQueuedScenario("",false);

			//vector<string> results;
			const vector<string> &dirList = Config::getInstance().getPathListForType(ptScenarios);
			string scenarioFile = Scenario::getScenarioPath(dirList, name);


			try {
				gameStarted = false;

			//printf("\nname [%s] scenarioFile [%s] results.size() = " MG_SIZE_T_SPECIFIER "\n",name.c_str(),scenarioFile.c_str(),results.size());
			//printf("[%s:%s] Line: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

			bool isTutorial = Scenario::isGameTutorial(scenarioFile);
			ScenarioInfo scenarioInfo;
			Scenario::loadScenarioInfo(scenarioFile, &scenarioInfo, isTutorial);

			//printf("[%s:%s] Line: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
			GameSettings gameSettings;
			Scenario::loadGameSettings(dirList,&scenarioInfo, &gameSettings, scenarioFile);

			//Program *program = world->getGame()->getProgram();
			//program->setState(new Game(program, &gameSettings, false));

			//world->end();

			//world->getMapPtr()->end();
			//world.end();

			if(keepFactions == false) {
				world.end();

				world.cleanup();
				world.clearTileset();

				SoundRenderer::getInstance().stopAllSounds();

				masterController.clearSlaves(true);
				deleteValues(aiInterfaces.begin(), aiInterfaces.end());
				aiInterfaces.clear();
				gui.end();		//selection must be cleared before deleting units
				world.end();	//must die before selection because of referencers

				BaseColorPickEntity::resetUniqueColors();
				// MUST DO THIS LAST!!!! Because objects above have pointers to things like
				// unit particles and fade them out etc and this end method deletes the original
				// object pointers.
				Renderer &renderer= Renderer::getInstance();
				renderer.endGame(true);

				GameConstants::updateFps = original_updateFps;
				GameConstants::cameraFps = original_cameraFps;

				this->setGameSettings(&gameSettings);
				this->resetMembers();
				this->load();
				this->init();
			}
			else {
				SoundRenderer &soundRenderer= SoundRenderer::getInstance();
				//printf("In [%s:%s] Line: %d currentAmbientSound = [%p]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,currentAmbientSound);
				if(currentAmbientSound) {
					soundRenderer.stopAmbient(currentAmbientSound);
				}
				//soundRenderer.stopAllSounds();
				soundRenderer.stopAllSounds(fadeMusicMilliseconds);

				world.endScenario();
				BaseColorPickEntity::resetUniqueColors();

				Renderer &renderer= Renderer::getInstance();
				renderer.endScenario();
				world.clearTileset();
				this->setGameSettings(&gameSettings);
				this->load(lgt_FactionPreview | lgt_TileSet | lgt_Map | lgt_Scenario);
				try {
					world.init(this, gameSettings.getDefaultUnits(),false);
				}
				catch(const exception &ex) {
					char szBuf[8096]="";
					snprintf(szBuf,8096,"In [%s::%s Line: %d]\nError [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what());

					SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
					if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,szBuf);

					if(errorMessageBox.getEnabled() == false) {
						ErrorDisplayMessage(ex.what(),true);
					}
				}

				world.initUnitsForScenario();
				Map *map= world.getMap();
				gameCamera.init(map->getW(), map->getH());

				// camera default height calculation
				if(map->getCameraHeight()>0 && gameCamera.getCalculatedDefault()<map->getCameraHeight()){
					gameCamera.setCalculatedDefault(map->getCameraHeight());
				}
				else if(gameCamera.getCalculatedDefault()<map->getMaxMapHeight()+13.0f){
					gameCamera.setCalculatedDefault(map->getMaxMapHeight()+13.0f);
				}

				scriptManager.init(&world, &gameCamera,loadGameNode);
				renderer.initGame(this,this->getGameCameraPtr());

				//sounds
				//soundRenderer.stopAllSounds(fadeMusicMilliseconds);
				//soundRenderer.stopAllSounds();
				//soundRenderer= SoundRenderer::getInstance();

				Tileset *tileset= world.getTileset();
				AmbientSounds *ambientSounds= tileset->getAmbientSounds();

				//rain
				if(tileset->getWeather() == wRainy && ambientSounds->isEnabledRain()) {
					//logger.add("Starting ambient stream", true);
					currentAmbientSound = ambientSounds->getRain();
					//printf("In [%s:%s] Line: %d currentAmbientSound = [%p]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,currentAmbientSound);
					soundRenderer.playAmbient(currentAmbientSound);
				}

				//snow
				if(tileset->getWeather() == wSnowy && ambientSounds->isEnabledSnow()) {
					//logger.add("Starting ambient stream", true);
					currentAmbientSound = ambientSounds->getSnow();
					//printf("In [%s:%s] Line: %d currentAmbientSound = [%p]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,currentAmbientSound);
					soundRenderer.playAmbient(currentAmbientSound);
				}

				if(this->masterserverMode == false) {
					StrSound *gameMusic= world.getThisFaction()->getType()->getMusic();
					soundRenderer.playMusic(gameMusic);
				}

				gameStarted = true;
			}
			//this->init();

			//printf("[%s:%s] Line: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
			//Checksum checksum;
			//world->loadScenario(scenarioFile, &checksum, true);
			}
#if defined(WIN32)
			catch(const exception) {
#else
			catch(const exception &ex) {
#endif
				gameStarted = true;
				totalRenderFps++;

				throw;
			}
		}

		if(showPerfStats) {
			sprintf(perfBuf,"In [%s::%s] Line: %d took msecs: " MG_I64_SPECIFIER "\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,chronoPerf.getMillis());
			perfList.push_back(perfBuf);
		}

		if(showPerfStats && chronoPerf.getMillis() >= 50) {
			for(unsigned int x = 0; x < (unsigned int)perfList.size(); ++x) {
				printf("%s",perfList[x].c_str());
			}
		}
	}
	catch(const exception &ex) {
		quitPendingIndicator = true;

		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s Line: %d] Error [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what());

		SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,szBuf);

		//printf("#100 quitPendingIndicator = %d, errorMessageBox.getEnabled() = %d\n",quitPendingIndicator,errorMessageBox.getEnabled());

		NetworkManager &networkManager= NetworkManager::getInstance();
		if(networkManager.getGameNetworkInterface() != NULL) {
			GameNetworkInterface *networkInterface = NetworkManager::getInstance().getGameNetworkInterface();
			networkInterface->sendTextMessage(szBuf,-1,true,"");
			sleep(10);
			networkManager.getGameNetworkInterface()->quitGame(true);
		}
		if(errorMessageBox.getEnabled() == false) {
            ErrorDisplayMessage(ex.what(),true);
		}
	}
}

void Game::addPerformanceCount(string key,int64 value) {
	gamePerformanceCounts[key] = value + gamePerformanceCounts[key] / 2;
}

string Game::getGamePerformanceCounts(bool displayWarnings) const {
	if(gamePerformanceCounts.empty() == true) {
		return "";
	}

	bool displayWarningHeader 	= true;
	bool WARN_TO_CONSOLE 		= Config::getInstance().getBool("PerformanceWarningEnabled","false");
	int WARNING_MILLIS 			= Config::getInstance().getInt("PerformanceWarningMillis","7");
	int WARNING_RENDER_MILLIS 	= Config::getInstance().getInt("PerformanceWarningRenderMillis","40");

	string result = "";
	for(std::map<string,int64>::const_iterator iterMap = gamePerformanceCounts.begin();
			iterMap != gamePerformanceCounts.end(); ++iterMap) {
		if(iterMap->first == ProgramState::MAIN_PROGRAM_RENDER_KEY) {
			if(iterMap->second < WARNING_RENDER_MILLIS) {
				continue;
			}
			//else {
			//	printf("iterMap->second: " MG_I64_SPECIFIER " WARNING_RENDER_MILLIS = %d\n",iterMap->second,WARNING_RENDER_MILLIS);
			//}
		}
		else if(iterMap->second < WARNING_MILLIS) {
			continue;
		}

		if(result != "") {
			result += "\n";
		}
		string perfStat = iterMap->first + " = avg millis: " + intToStr(iterMap->second);

		if(displayWarnings == true && WARN_TO_CONSOLE == true) {
			if(displayWarningHeader == true) {
				displayWarningHeader = false;
				printf("=====================================\nPERFORMANCE WARNINGS for World Frame: %d\n",world.getFrameCount());
			}

			printf("*PERFORMANCE WARNING* %s\n",perfStat.c_str());
		}

		result += perfStat;
	}

	return result;
}

bool Game::switchSetupForSlots(ServerInterface *& serverInterface,
		int startIndex, int endIndex, bool onlyNetworkUnassigned) {
	bool switchRequested = false;
	if(serverInterface == NULL) {
		return switchRequested;
	}

	MutexSafeWrapper safeMutex(serverInterface->getSwitchSetupRequestsMutex(),CODE_AT_LINE);
	SwitchSetupRequest ** switchSetupRequests = serverInterface->getSwitchSetupRequests();
	if(switchSetupRequests == NULL) {
		return switchRequested;
	}

	Map *map= world.getMap();
	for(int i= startIndex; i < endIndex; ++i) {
		if(switchSetupRequests[i] != NULL) {
			//printf("Faction Index: %d Switch slot = %d to = %d current control = %d\n",i,switchSetupRequests[i]->getCurrentSlotIndex(),switchSetupRequests[i]->getToSlotIndex(),gameSettings.getFactionControl(i));

			if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] switchSetupRequests[i]->getSwitchFlags() = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,switchSetupRequests[i]->getSwitchFlags());

			if(onlyNetworkUnassigned == true && gameSettings.getFactionControl(i) != ctNetworkUnassigned) {
				if(i < map->getMaxPlayers() || (i >= map->getMaxPlayers() && gameSettings.getFactionControl(i) != ctNetwork)) {
					continue;
				}
			}

			if(gameSettings.getFactionControl(i) == ctNetwork ||
				gameSettings.getFactionControl(i) == ctNetworkUnassigned ||
				//(gameSettings.getFactionControl(i) != ctClosed && gameSettings.getFactionControl(i) != ctHuman)) {
				(gameSettings.getFactionControl(i) != ctHuman)) {
				if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] switchSetupRequests[i]->getToFactionIndex() = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,switchSetupRequests[i]->getToSlotIndex());

				if(switchSetupRequests[i]->getToSlotIndex() != -1) {
					int newSlotIdx = switchSetupRequests[i]->getToSlotIndex();

					//printf("switchSlot request from %d to %d\n",switchSetupRequests[i]->getCurrentSlotIndex(),switchSetupRequests[i]->getToSlotIndex());

					int switchSlotIdx = switchSetupRequests[i]->getCurrentSlotIndex();
					if(serverInterface->switchSlot(switchSlotIdx,newSlotIdx)) {
						//printf("switchSlot returned true\n");
						switchRequested = true;

						int oldFactionIndex = gameSettings.getFactionIndexForStartLocation(switchSlotIdx);
						int newFactionIndex = gameSettings.getFactionIndexForStartLocation(newSlotIdx);

						//printf("Switching faction for index %d [%d] to %d\n",newSlotIdx,switchSlotIdx,gameSettings.getFactionControl(newFactionIndex));

						gameSettings.setNetworkPlayerName(oldFactionIndex, "");
						serverInterface->gameSettings.setNetworkPlayerName(oldFactionIndex, "");
						gameSettings.setNetworkPlayerUUID(oldFactionIndex, "");
						serverInterface->gameSettings.setNetworkPlayerUUID(oldFactionIndex, "");

						gameSettings.setNetworkPlayerPlatform(oldFactionIndex, "");
						serverInterface->gameSettings.setNetworkPlayerPlatform(oldFactionIndex, "");

						gameSettings.setFactionControl(newFactionIndex,ctNetwork);
						serverInterface->gameSettings.setFactionControl(newFactionIndex,ctNetwork);

						//printf("#1a Faction Index: %d control: %d startlocation: %d\n",newFactionIndex,gameSettings.getFactionControl(newFactionIndex),gameSettings.getStartLocationIndex(newFactionIndex));
						//printf("#2a Faction Index: %d control: %d startlocation: %d\n",newFactionIndex,serverInterface->gameSettings.getFactionControl(newFactionIndex),serverInterface->gameSettings.getStartLocationIndex(newFactionIndex));

						try {
							//if(switchSetupRequests[i]->getSelectedFactionName() != "") {
							//	listBoxFactions[newFactionIdx].setSelectedItem(switchSetupRequests[i]->getSelectedFactionName());
							//}
							//if(switchSetupRequests[i]->getToTeam() != -1) {
							//	listBoxTeams[newFactionIdx].setSelectedItemIndex(switchSetupRequests[i]->getToTeam());
							//}
							if(switchSetupRequests[i]->getNetworkPlayerName() != "") {
								//if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line %d] i = %d, labelPlayerNames[newFactionIdx].getText() [%s] switchSetupRequests[i]->getNetworkPlayerName() [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,i,labelPlayerNames[newFactionIdx].getText().c_str(),switchSetupRequests[i]->getNetworkPlayerName().c_str());
								gameSettings.setNetworkPlayerName(newFactionIndex,switchSetupRequests[i]->getNetworkPlayerName());
								serverInterface->gameSettings.setNetworkPlayerName(newFactionIndex,switchSetupRequests[i]->getNetworkPlayerName());
							}

//							if(gameSettings.getFactionControl(switchFactionIdx) == ctNetworkUnassigned) {
//								serverInterface->removeSlot(switchFactionIdx);
//								//listBoxControls[switchFactionIdx].setSelectedItemIndex(ctClosed);
//								gameSettings.getFactionControl(switchFactionIdx)
//
//								labelPlayers[switchFactionIdx].setVisible(switchFactionIdx+1 <= mapInfo.players);
//								labelPlayerNames[switchFactionIdx].setVisible(switchFactionIdx+1 <= mapInfo.players);
//								listBoxControls[switchFactionIdx].setVisible(switchFactionIdx+1 <= mapInfo.players);
//								listBoxFactions[switchFactionIdx].setVisible(switchFactionIdx+1 <= mapInfo.players);
//								listBoxTeams[switchFactionIdx].setVisible(switchFactionIdx+1 <= mapInfo.players);
//								labelNetStatus[switchSlotIdx].setVisible(switchSlotIdx+1 <= mapInfo.players);
//							}
						}
						catch(const runtime_error &e) {
							SystemFlags::OutputDebug(SystemFlags::debugError,"In [%s::%s Line: %d] Error [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,e.what());
							if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] caught exception error = [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,e.what());
						}
					}
					//printf("AFTER switchSlot returned\n");
				}
				else {
					try {
						//if(switchSetupRequests[i]->getSelectedFactionName() != "") {
						//	listBoxFactions[i].setSelectedItem(switchSetupRequests[i]->getSelectedFactionName());
						//}
						//if(switchSetupRequests[i]->getToTeam() != -1) {
						//	listBoxTeams[i].setSelectedItemIndex(switchSetupRequests[i]->getToTeam());
						//}

						if((switchSetupRequests[i]->getSwitchFlags() & ssrft_NetworkPlayerName) == ssrft_NetworkPlayerName) {
							//if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] i = %d, switchSetupRequests[i]->getSwitchFlags() = %d, switchSetupRequests[i]->getNetworkPlayerName() [%s], labelPlayerNames[i].getText() [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,i,switchSetupRequests[i]->getSwitchFlags(),switchSetupRequests[i]->getNetworkPlayerName().c_str(),labelPlayerNames[i].getText().c_str());

							if(switchSetupRequests[i]->getNetworkPlayerName() != GameConstants::NETWORK_SLOT_UNCONNECTED_SLOTNAME) {
								//labelPlayerNames[i].setText(switchSetupRequests[i]->getNetworkPlayerName());
								gameSettings.setNetworkPlayerName(i,switchSetupRequests[i]->getNetworkPlayerName());
								serverInterface->gameSettings.setNetworkPlayerName(i,switchSetupRequests[i]->getNetworkPlayerName());
								switchRequested = true;
							}
							else {
								//labelPlayerNames[i].setText("");
								gameSettings.setNetworkPlayerName(i,"");
								serverInterface->gameSettings.setNetworkPlayerName(i,"");
								switchRequested = true;
							}
							//SetActivePlayerNameEditor();
							//switchSetupRequests[i]->clearSwitchFlag(ssrft_NetworkPlayerName);
						}
					}
					catch(const runtime_error &e) {
						SystemFlags::OutputDebug(SystemFlags::debugError,"In [%s::%s Line: %d] Error [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,e.what());
						if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] caught exception error = [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,e.what());
					}
				}
			}

			delete switchSetupRequests[i];
			switchSetupRequests[i]=NULL;
		}
	}

	return switchRequested;
}

void Game::updateNetworkMarkedCells() {
	try {
		GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();

		if(gameNetworkInterface != NULL &&
			gameNetworkInterface->getMarkedCellList(false).empty() == false) {

			std::vector<MarkedCell> chatList = gameNetworkInterface->getMarkedCellList(true);
			for(int idx = 0; idx < (int)chatList.size(); idx++) {
				MarkedCell mc = chatList[idx];
				if(mc.getFactionIndex() >= 0) {
					mc.setFaction((const Faction *)world.getFaction(mc.getFactionIndex()));
				}

				Map *map= world.getMap();
				Vec2i surfaceCellPos = map->toSurfCoords(mc.getTargetPos());
				mapMarkedCellList[surfaceCellPos] = mc;
			}
		}
	}
	catch(const std::exception &ex) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s %d] error [%s]\n",__FILE__,__FUNCTION__,__LINE__,ex.what());
		SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
		throw megaglest_runtime_error(szBuf);
	}
}

void Game::updateNetworkUnMarkedCells() {
	try {
		GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();

		if(gameNetworkInterface != NULL &&
				gameNetworkInterface->getUnMarkedCellList(false).empty() == false) {
			//Lang &lang= Lang::getInstance();

			std::vector<UnMarkedCell> chatList = gameNetworkInterface->getUnMarkedCellList(true);
			for(int idx = 0; idx < (int)chatList.size(); idx++) {
				UnMarkedCell mc = chatList[idx];
				mc.setFaction((const Faction *)world.getFaction(mc.getFactionIndex()));

				Map *map= world.getMap();
				Vec2i surfaceCellPos = map->toSurfCoords(mc.getTargetPos());
				mapMarkedCellList.erase(surfaceCellPos);
			}
		}
	}
	catch(const std::exception &ex) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s %d] error [%s]\n",__FILE__,__FUNCTION__,__LINE__,ex.what());
		SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
		throw megaglest_runtime_error(szBuf);
	}
}


void Game::updateNetworkHighligtedCells() {
	try {
		GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();

		//update the current entries
		for(int idx = (int)highlightedCells.size()-1; idx >= 0; idx--) {
			MarkedCell *mc = &highlightedCells[idx];
			mc->decrementAliveCount();
			if(mc->getAliveCount() < 0) {
				highlightedCells.erase(highlightedCells.begin()+idx);
			}
		}

		if(gameNetworkInterface != NULL &&
				gameNetworkInterface->getHighlightedCellList(false).empty() == false) {
			//Lang &lang= Lang::getInstance();
			std::vector<MarkedCell> highlighList = gameNetworkInterface->getHighlightedCellList(true);
			for(int idx = 0; idx < (int)highlighList.size(); idx++) {
				MarkedCell mc = highlighList[idx]; // I want a copy here
				if(mc.getFactionIndex() >= 0) {
					mc.setFaction((const Faction *)world.getFaction(mc.getFactionIndex())); // set faction pointer
				}
				addOrReplaceInHighlightedCells(mc);
			}
		}
	}
	catch(const std::exception &ex) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s %d] error [%s]\n",__FILE__,__FUNCTION__,__LINE__,ex.what());
		SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
		throw megaglest_runtime_error(szBuf);
	}
}

void Game::addOrReplaceInHighlightedCells(MarkedCell mc){
	if(mc.getFactionIndex() >= 0) {
		for(int i = (int)highlightedCells.size()-1; i >= 0; i--) {
			MarkedCell *currentMc = &highlightedCells[i];
			if(currentMc->getFactionIndex() == mc.getFactionIndex()) {
				highlightedCells.erase(highlightedCells.begin()+i);
			}
		}
	}
	if(mc.getAliveCount() <= 0) {
		mc.setAliveCount(200);
	}
	highlightedCells.push_back(mc);

	if (this->masterserverMode == false) {
		CoreData &coreData= CoreData::getInstance();
		SoundRenderer &soundRenderer= SoundRenderer::getInstance();

		const Faction *faction = mc.getFaction();
		if(getWorld()->getThisFaction() == NULL) {
			throw megaglest_runtime_error("getWorld()->getThisFaction() == NULL");
		}
		//printf("faction [%p][%s]\n",faction,(faction != NULL ? faction->getType()->getName().c_str() : ""));
		if((faction == NULL) ||
				(faction->getTeam() == getWorld()->getThisFaction()->getTeam())) {
			soundRenderer.playFx(coreData.getMarkerSound(),true);
		}
	}
}

void Game::ReplaceDisconnectedNetworkPlayersWithAI(bool isNetworkGame, NetworkRole role) {
	if(role == nrServer && isNetworkGame == true &&
			difftime((long int)time(NULL),lastNetworkPlayerConnectionCheck) >= NETWORK_PLAYER_CONNECTION_CHECK_SECONDS) {
		lastNetworkPlayerConnectionCheck = time(NULL);
		Logger &logger= Logger::getInstance();
		ServerInterface *server = NetworkManager::getInstance().getServerInterface();

		bool newAIPlayerCreated = false;
		for(int i = 0; i < world.getFactionCount(); ++i) {
			Faction *faction = world.getFaction(i);
			if(	faction->getFactionDisconnectHandled() == false &&
				(faction->getControlType() == ctNetwork ||
				faction->getControlType() == ctNetworkCpuEasy ||
				faction->getControlType() == ctNetworkCpu ||
				faction->getControlType() == ctNetworkCpuUltra ||
				faction->getControlType() == ctNetworkCpuMega)) {

				if(aiInterfaces[i] == NULL &&
						server->isClientConnected(faction->getStartLocationIndex()) == false) {

					if(faction->getPersonalityType() != fpt_Observer) {
						DumpCRCWorldLogIfRequired("_faction_" + intToStr(i));
					}

					faction->setFactionDisconnectHandled(true);

					Lang &lang= Lang::getInstance();

					bool isPlayerObserver = false;
					char szBuf[8096]="";
					if(faction->getPersonalityType() != fpt_Observer) {
						aiInterfaces[i] = new AiInterface(*this, i, faction->getTeam(), faction->getStartLocationIndex());

						snprintf(szBuf,8096,Lang::getInstance().getString("LogScreenGameLoadingCreatingAIFaction","",true).c_str(),i);
						logger.add(szBuf, true);

						commander.tryNetworkPlayerDisconnected(i);

						newAIPlayerCreated = true;
					}
					else {
						isPlayerObserver = true;

					}

					const vector<string> languageList = this->gameSettings.getUniqueNetworkPlayerLanguages();
					for(unsigned int j = 0; j < (unsigned int)languageList.size(); ++j) {
						if(isPlayerObserver == false) {
							string msg = "Player #%d [%s] has disconnected, switching player to AI mode!";
							if(lang.hasString("GameSwitchPlayerToAI",languageList[j],true)) {
								msg = lang.getString("GameSwitchPlayerToAI",languageList[j],true);
							}
							snprintf(szBuf,8096,msg.c_str(),i+1,this->gameSettings.getNetworkPlayerName(i).c_str());
						}
						else {
							string msg = "Player #%d [%s] has disconnected, but player was only an observer!";
							if(lang.hasString("GameSwitchPlayerObserverToAI",languageList[j],true)) {
								msg = lang.getString("GameSwitchPlayerObserverToAI",languageList[j],true);
							}
							snprintf(szBuf,8096,msg.c_str(),i+1,this->gameSettings.getNetworkPlayerName(i).c_str());
						}
						bool localEcho = (languageList[j] == lang.getLanguage());
						server->sendTextMessage(szBuf,-1,localEcho,languageList[j]);
					}
				}
			}
		}

		if(newAIPlayerCreated == true && Config::getInstance().getBool("EnableNewThreadManager","false") == true) {
			bool enableServerControlledAI 	= this->gameSettings.getEnableServerControlledAI();

			masterController.clearSlaves(true);

			std::vector<SlaveThreadControllerInterface *> slaveThreadList;
			for(int i=0; i < world.getFactionCount(); ++i) {
				Faction *faction= world.getFaction(i);
				if(faction->getCpuControl(enableServerControlledAI,isNetworkGame,role) == true) {
					slaveThreadList.push_back(aiInterfaces[i]->getWorkerThread());
				}
			}
			masterController.setSlaves(slaveThreadList);
		}
	}
}

void Game::updateCamera(){
	if(currentUIState != NULL) {
		currentUIState->updateCamera();
		return;
	}
	gameCamera.update();
}


// ==================== render ====================

//render
void Game::render() {
	// Ensure the camera starts in the right position
	if(isFirstRender == true) {
		isFirstRender = false;

		if(this->loadGameNode == NULL) {
			gameCamera.setState(GameCamera::sGame);
			this->restoreToStartXY();
		}
	}

	canRender();
	incrementFps();

	renderFps++;
	totalRenderFps++;

	updateWorldStats();

	//NetworkManager &networkManager= NetworkManager::getInstance();
	if(this->masterserverMode == false) {
		renderWorker();
	}
	else {
		// Titi, uncomment this to watch the game on the masterserver
		//renderWorker();

		// In masterserver mode quit game if no network players left
		ServerInterface *server = NetworkManager::getInstance().getServerInterface();
		int connectedClients=0;
		for(int i = 0; i < world.getFactionCount(); ++i) {
			Faction *faction = world.getFaction(i);
			if(server->isClientConnected(faction->getStartLocationIndex()) == true) {
				connectedClients++;
			}
		}

		if(connectedClients == 0) {
			quitTriggeredIndicator = true;
		}
		else {
		    string str="";
		    std::map<int,string> factionDebugInfo;

			if( difftime((long int)time(NULL),lastMasterServerGameStatsDump) >= GAME_STATS_DUMP_INTERVAL) {
				lastMasterServerGameStatsDump = time(NULL);
				str = getDebugStats(factionDebugInfo);

				printf("== Current in-game stats (interval %d) ==\n%s\n",GAME_STATS_DUMP_INTERVAL,str.c_str());
			}
		}
	}
}

void Game::renderWorker() {
	if(currentUIState != NULL) {
//		Renderer &renderer= Renderer::getInstance();
//		renderer.clearBuffers();
//
//		//3d
//		renderer.reset3dMenu();
//
//		renderer.clearZBuffer();
//		//renderer.loadCameraMatrix(menuBackground.getCamera());
//		//renderer.renderMenuBackground(&menuBackground);
//		renderer.renderParticleManager(rsMenu);
//
//		//2d
//		renderer.reset2d();
//
//		currentUIState->render();
//
//		if(renderer.isMasterserverMode() == false) {
//			renderer.renderMouse2d(mouseX, mouseY, mouse2dAnim);
//			renderer.renderFPSWhenEnabled(lastFps);
//			renderer.swapBuffers();
//		}

		currentUIState->render();
		return;
	}
	else {
		Renderer &renderer= Renderer::getInstance();
		if(renderer.getCustom3dMenu() != NULL) {
			renderer.setCustom3dMenu(NULL);
		}
	}

	Chrono chrono;
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled) chrono.start();

	render3d();

	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %d [render3d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

	render2d();

	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %d [render2d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

	Renderer::getInstance().swapBuffers();
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %d [swap buffers]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
}

// ==================== tick ====================

void Game::removeUnitFromSelection(const Unit *unit) {
	try {
		Selection *selection= gui.getSelectionPtr();
		for(int i=0; i < selection->getCount(); ++i) {
			const Unit *currentUnit = selection->getUnit(i);
			if(currentUnit == unit) {
				selection->unSelect(i);
				break;
			}
		}
	}
	catch(const exception &ex) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s Line: %d] Error [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what());

		SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,szBuf);

		if(errorMessageBox.getEnabled() == false) {
            ErrorDisplayMessage(ex.what(),true);
		}

		//abort();
	}
}

bool Game::addUnitToSelection(Unit *unit) {
	bool result = false;
	try {
		Selection *selection= gui.getSelectionPtr();
		if(selection != NULL) {
			result = selection->select(unit,true);
		}
	}
	catch(const exception &ex) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s Line: %d] Error [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what());

		SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,szBuf);

		if(errorMessageBox.getEnabled() == false) {
            ErrorDisplayMessage(ex.what(),true);
		}

		//abort();
	}

	return result;
}

void Game::addUnitToGroupSelection(Unit *unit,int groupIndex) {
	try {
		Selection *selection= gui.getSelectionPtr();
		if(selection != NULL) {
			selection->addUnitToGroup(groupIndex,unit);
		}
	}
	catch(const exception &ex) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s Line: %d] Error [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what());

		SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,szBuf);

		if(errorMessageBox.getEnabled() == false) {
            ErrorDisplayMessage(ex.what(),true);
		}

		//abort();
	}
}

void Game::removeUnitFromGroupSelection(int unitId,int groupIndex) {
	try {
		Selection *selection= gui.getSelectionPtr();
		if(selection != NULL) {
			selection->removeUnitFromGroup(groupIndex,unitId);
		}
	}
	catch(const exception &ex) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s Line: %d] Error [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what());

		SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,szBuf);

		if(errorMessageBox.getEnabled() == false) {
            ErrorDisplayMessage(ex.what(),true);
		}

		//abort();
	}
}

void Game::recallGroupSelection(int groupIndex) {
	try {
		Selection *selection= gui.getSelectionPtr();
		if(selection != NULL) {
			selection->recallGroup(groupIndex);
		}
	}
	catch(const exception &ex) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s Line: %d] Error [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what());

		SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,szBuf);

		if(errorMessageBox.getEnabled() == false) {
            ErrorDisplayMessage(ex.what(),true);
		}

		//abort();
	}
}

void Game::tick() {
	ProgramState::tick();

	tickCount++;

	if(avgUpdateFps == -1) {
		avgUpdateFps = updateFps;
	}
	else {
		avgUpdateFps = (avgUpdateFps + updateFps) / 2;
	}
	currentAvgRenderFpsTotal += renderFps;

	if(avgRenderFps == -1) {
		avgRenderFps = renderFps;
	}
	// Update the average every x game ticks
	const int CHECK_AVG_FPS_EVERY_X_TICKS = 15;
	if(tickCount % CHECK_AVG_FPS_EVERY_X_TICKS == 0) {
		avgRenderFps = currentAvgRenderFpsTotal / CHECK_AVG_FPS_EVERY_X_TICKS;
		currentAvgRenderFpsTotal = 0;
	}

	if(captureAvgTestStatus == true) {
		if(updateFpsAvgTest == -1) {
			updateFpsAvgTest = updateFps;
		}
		else {
			updateFpsAvgTest = (updateFpsAvgTest + updateFps) / 2;
		}
		if(renderFpsAvgTest == -1) {
			renderFpsAvgTest = renderFps;
		}
		else {
			renderFpsAvgTest = (renderFpsAvgTest + renderFps) / 2;
		}
	}

	lastUpdateFps= updateFps;
	lastRenderFps= renderFps;
	updateFps= 0;
	renderFps= 0;

	//Win/lose check
	checkWinner();
	gui.tick();
}


// ==================== events ====================

int Game::getFirstUnusedTeamNumber() {
	std::map<int,bool> uniqueTeamNumbersUsed;
	for(unsigned int i = 0; i < (unsigned int)world.getFactionCount(); ++i) {
		Faction *faction = world.getFaction(i);
		uniqueTeamNumbersUsed[faction->getTeam()]=true;
	}

	int result = -1;
	for(int i = 0; i < GameConstants::maxPlayers; ++i) {
		if(uniqueTeamNumbersUsed.find(i) == uniqueTeamNumbersUsed.end()) {
			result = i;
			break;
		}
	}

	return result;
}

void Game::setupRenderForVideo() {
	Renderer &renderer= Renderer::getInstance();
	//renderer.clearBuffers();
	//3d
	//renderer.reset3dMenu();
	//renderer.clearZBuffer();
	//2d
	//renderer.reset2d();
	renderer.setupRenderForVideo();
}

void Game::tryPauseToggle(bool pauseValue) {
	bool allowAdminMenuItems = false;
	NetworkManager &networkManager= NetworkManager::getInstance();
	NetworkRole role = networkManager.getNetworkRole();
	if(role == nrServer) {
		allowAdminMenuItems = true;
	}
	else if(role == nrClient) {
		ClientInterface *clientInterface = dynamic_cast<ClientInterface *>(networkManager.getClientInterface());

		if(clientInterface != NULL &&
			gameSettings.getMasterserver_admin() == clientInterface->getSessionKey()) {
			allowAdminMenuItems = true;
		}
	}

	bool isNetworkGame 				= this->gameSettings.isNetworkGame();
	//printf("Try Pause allowAdminMenuItems = %d, pauseValue = %d\n",allowAdminMenuItems,pauseValue);

	if(allowAdminMenuItems) {
		if(pauseValue == true) {
			commander.tryPauseGame(false,false);
		}
		else {
			if(isNetworkGame == false) {
				setPaused(pauseValue, true,false,false);
			}
			else {
				commander.tryResumeGame(false,false);
			}
		}
	}
}

void Game::startMarkCell() {
	int totalMarkedCellsForPlayer = 0;
	if(world.getThisFaction() != NULL) {
		for(std::map<Vec2i, MarkedCell>::iterator iterMap = mapMarkedCellList.begin();
				iterMap != mapMarkedCellList.end(); ++iterMap) {
			MarkedCell &bm = iterMap->second;
			if(bm.getPlayerIndex() == world.getThisFaction()->getStartLocationIndex()) {
				totalMarkedCellsForPlayer++;
			}
		}
	}

	const int MAX_MARKER_COUNT = 5;
	if(totalMarkedCellsForPlayer < MAX_MARKER_COUNT) {
		isMarkCellEnabled = true;
	}
	else {
		Lang &lang= Lang::getInstance();
		console.addLine(lang.getString("MaxMarkerCount") + " " + intToStr(MAX_MARKER_COUNT));
	}
}

void Game::processInputText(string text, bool cancelled) {
	isMarkCellTextEnabled = false;

	if(cancelled == false) {
		//printf("Note [%s]\n",text.c_str());

		cellMarkedData.setNote(text);
		addCellMarker(cellMarkedPos, cellMarkedData);

//		if(text.find("\\n") != text.npos) {
//			replaceAll(text, "\\n", "\n");
//		}
//		if(text.find("\\t") != text.npos) {
//			replaceAll(text, "\\t", "\t");
//		}
//
//		cellMarkedData.setNote(text);
//		mapMarkedCellList[cellMarkedPos] = cellMarkedData;
//
//		GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
//
//		int factionIndex = -1;
//		int playerIndex = -1;
//		if(cellMarkedData.getFaction() != NULL) {
//			factionIndex = cellMarkedData.getFaction()->getIndex();
//			playerIndex = cellMarkedData.getFaction()->getStartLocationIndex();
//		}
//		gameNetworkInterface->sendMarkCellMessage(
//				cellMarkedData.getTargetPos(),
//				factionIndex,
//				cellMarkedData.getNote(),
//				playerIndex);
//
//		Renderer &renderer= Renderer::getInstance();
//		renderer.forceQuadCacheUpdate();
	}
}

void Game::addCellMarker(Vec2i cellPos, MarkedCell cellData) {
	//printf("Note [%s]\n",text.c_str());

	string text = cellData.getNote();
	if(text.find("\\n") != text.npos) {
		replaceAll(text, "\\n", "\n");
	}
	if(text.find("\\t") != text.npos) {
		replaceAll(text, "\\t", "\t");
	}

	cellData.setNote(text);
	mapMarkedCellList[cellPos] = cellData;

	GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();

	int factionIndex = -1;
	int playerIndex = -1;
	if(cellData.getFaction() != NULL) {
		factionIndex = cellData.getFaction()->getIndex();
		playerIndex = cellData.getFaction()->getStartLocationIndex();
	}

	//printf("Adding Cell marker pos [%s] factionIndex [%d] note [%s] playerIndex = %d\n",cellData.getTargetPos().getString().c_str(),factionIndex,cellData.getNote().c_str(),playerIndex);

	gameNetworkInterface->sendMarkCellMessage(
			cellData.getTargetPos(),
			factionIndex,
			cellData.getNote(),
			playerIndex);

	Renderer &renderer= Renderer::getInstance();
	renderer.forceQuadCacheUpdate();
}

void Game::removeCellMarker(Vec2i surfaceCellPos, const Faction *faction) {
	//Vec2i surfaceCellPos = map->toSurfCoords(Vec2i(xCell,yCell));
	Map *map= world.getMap();
	SurfaceCell *sc = map->getSurfaceCell(surfaceCellPos);
	Vec3f vertex = sc->getVertex();
	Vec2i targetPos(vertex.x,vertex.z);

	//printf("Remove Cell marker lookup pos [%s] factionIndex [%d]\n",surfaceCellPos.getString().c_str(),(faction != NULL ? faction->getIndex() : -1));

	if(mapMarkedCellList.find(surfaceCellPos) != mapMarkedCellList.end()) {
		MarkedCell mc = mapMarkedCellList[surfaceCellPos];
		if(mc.getFaction() == faction) {
			mapMarkedCellList.erase(surfaceCellPos);
			GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();

			int factionIndex = (faction != NULL ? faction->getIndex() : -1);

			//printf("Remvoing Cell marker pos [%s] factionIndex [%d] note [%s]\n",mc.getTargetPos().getString().c_str(),factionIndex,mc.getNote().c_str());

			gameNetworkInterface->sendUnMarkCellMessage(mc.getTargetPos(),factionIndex);
		}
	}
	//printf("#1 ADDED in marked list pos [%s] markedCells.size() = " MG_SIZE_T_SPECIFIER "\n",surfaceCellPos.getString().c_str(),mapMarkedCellList.size());

	//isUnMarkCellEnabled = false;

	Renderer &renderer= Renderer::getInstance();
	//renderer.updateMarkedCellScreenPosQuadCache(surfaceCellPos);
	renderer.forceQuadCacheUpdate();
}
void Game::showMarker(Vec2i cellPos, MarkedCell cellData) {
	//setMarker = true;
	//if(setMarker) {
		//Vec2i targetPos = cellData.targetPos;
		//Vec2i screenPos(x,y-60);
		//Renderer &renderer= Renderer::getInstance();
		//renderer.computePosition(screenPos, targetPos);
		//Vec2i surfaceCellPos = map->toSurfCoords(targetPos);

		//MarkedCell mc(targetPos,world.getThisFaction(),"none",world.getThisFaction()->getStartLocationIndex());
		addOrReplaceInHighlightedCells(cellData);

		GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
		gameNetworkInterface->sendHighlightCellMessage(cellData.getTargetPos(),cellData.getFactionIndex());
	//}
}

void Game::mouseDownLeft(int x, int y) {
	if(this->masterserverMode == true) {
		return;
	}
	cameraDragAllowed=false;
	if(currentUIState != NULL) {
		currentUIState->mouseDownLeft(x, y);
		return;
	}

	try {
		if(gameStarted == false || totalRenderFps <= 0) {
			Logger::getInstance().handleMouseClick(x, y);
			return;
		}

		Map *map= world.getMap();
		const Metrics &metrics= Metrics::getInstance();
		NetworkManager &networkManager= NetworkManager::getInstance();
		bool messageBoxClick= false;
		bool originalIsMarkCellEnabled = isMarkCellEnabled;
		bool originalIsUnMarkCellEnabled = isUnMarkCellEnabled;

		if(popupMenu.mouseClick(x, y)) {
			std::pair<int,string> result = popupMenu.mouseClickedMenuItem(x, y);
			//printf("In popup callback menuItemSelected [%s] menuIndexSelected = %d\n",result.second.c_str(),result.first);

			//printf("popupMenu.mouseClick == true result.first = %d disconnectPlayerPopupMenuIndex = %d\n",result.first,disconnectPlayerPopupMenuIndex);

			popupMenu.setEnabled(false);
			popupMenu.setVisible(false);

			// Exit game
			if(result.first == exitGamePopupMenuIndex) {
				showMessageBox(Lang::getInstance().getString("ExitBattleQuestion"), "", true);
			}
			else if(result.first == joinTeamPopupMenuIndex) {

				Lang &lang= Lang::getInstance();
				switchTeamIndexMap.clear();
				std::map<int,bool> uniqueTeamNumbersUsed;
				std::vector<string> menuItems;
				for(unsigned int i = 0; i < (unsigned int)world.getFactionCount(); ++i) {
					Faction *faction = world.getFaction(i);

					if(faction->getPersonalityType() != fpt_Observer &&
						uniqueTeamNumbersUsed.find(faction->getTeam()) == uniqueTeamNumbersUsed.end()) {
						uniqueTeamNumbersUsed[faction->getTeam()] = true;
					}

					if(faction->getPersonalityType() != fpt_Observer &&
						world.getThisFaction()->getIndex() != faction->getIndex() &&
						world.getThisFaction()->getTeam() != faction->getTeam()) {
						char szBuf[8096]="";
						if(lang.hasString("JoinPlayerTeam") == true) {
							snprintf(szBuf,8096,("  " + lang.getString("JoinPlayerTeam") + "  ").c_str(),faction->getIndex(),this->gameSettings.getNetworkPlayerName(i).c_str(),faction->getTeam());
						}
						else {
							snprintf(szBuf,8096,"  Join player #%d - %s on Team: %d  ",faction->getIndex(),this->gameSettings.getNetworkPlayerName(i).c_str(),faction->getTeam());
						}

						menuItems.push_back(szBuf);

						switchTeamIndexMap[(int)menuItems.size()-1] = faction->getTeam();
					}
				}

				if((int)uniqueTeamNumbersUsed.size() < 8) {
					menuItems.push_back("  " + lang.getString("CreateNewTeam") + "  ");
					switchTeamIndexMap[(int)menuItems.size()-1] = CREATE_NEW_TEAM;
				}
				menuItems.push_back("  " + lang.getString("Cancel") + "  ");
				switchTeamIndexMap[(int)menuItems.size()-1] = CANCEL_SWITCH_TEAM;

				popupMenuSwitchTeams.setW(100);
				popupMenuSwitchTeams.setH(100);
				popupMenuSwitchTeams.init("  " + lang.getString("SwitchTeams") + "  ",menuItems);
				popupMenuSwitchTeams.setEnabled(true);
				popupMenuSwitchTeams.setVisible(true);
			}
			else if(result.first == disconnectPlayerPopupMenuIndex) {
				Lang &lang= Lang::getInstance();

				NetworkManager &networkManager= NetworkManager::getInstance();
				NetworkRole role = networkManager.getNetworkRole();
				ServerInterface *serverInterface = NULL;
				if(role == nrServer) {
					serverInterface = dynamic_cast<ServerInterface *>(networkManager.getServerInterface());
				}
				disconnectPlayerIndexMap.clear();
				std::vector<string> menuItems;
				for(unsigned int i = 0; i < (unsigned int)world.getFactionCount(); ++i) {
					Faction *faction = world.getFaction(i);

					//printf("faction->getPersonalityType() = %d index [%d,%d] control [%d] networkstatus [%d]\n",faction->getPersonalityType(),world.getThisFaction()->getIndex(),faction->getIndex(),faction->getControlType(),this->gameSettings.getNetworkPlayerStatuses(i));

					bool isSlotJoinInProgressClient = false;
					if(serverInterface != NULL) {

						MutexSafeWrapper safeMutex(serverInterface->getSlotMutex(faction->getStartLocationIndex()),CODE_AT_LINE);
						ConnectionSlot *slot =  serverInterface->getSlot(faction->getStartLocationIndex(),false);
						if(slot != NULL && slot->getConnectHasHandshaked() == true &&
								slot->getCurrentFrameCount() <= 0) {
							isSlotJoinInProgressClient = true;
						}
					}

					//printf("isSlotJoinInProgressClient: %d [%d] [%d][%d] [%d] [%d] [%d]\n",
					//		isSlotJoinInProgressClient,faction->getPersonalityType(),faction->getIndex(),world.getThisFaction()->getIndex(),faction->getControlType(),this->gameSettings.getNetworkPlayerStatuses(i),i);

					if(isSlotJoinInProgressClient == true ||
						(faction->getPersonalityType() != fpt_Observer &&
						world.getThisFaction()->getIndex() != faction->getIndex() &&
						faction->getControlType() == ctNetwork &&
						this->gameSettings.getNetworkPlayerStatuses(i) != npst_Disconnected)) {

						char szBuf[8096]="";
						if(lang.hasString("DisconnectNetorkPlayerIndex") == true) {
							snprintf(szBuf,8096,("  " + lang.getString("DisconnectNetorkPlayerIndex") + "  ").c_str(),faction->getIndex()+1,this->gameSettings.getNetworkPlayerName(i).c_str());
						}
						else {
							snprintf(szBuf,8096,"  Disconnect player #%d - %s:  ",faction->getIndex()+1,this->gameSettings.getNetworkPlayerName(i).c_str());
						}

						menuItems.push_back(szBuf);

						//disconnectPlayerIndexMap[menuItems.size()-1] = faction->getStartLocationIndex();
						disconnectPlayerIndexMap[(int)menuItems.size()-1] = faction->getIndex();
					}
				}

				menuItems.push_back("  " + lang.getString("Cancel") + "  ");
				disconnectPlayerIndexMap[(int)menuItems.size()-1] = CANCEL_DISCONNECT_PLAYER;

				popupMenuDisconnectPlayer.setW(100);
				popupMenuDisconnectPlayer.setH(100);
				popupMenuDisconnectPlayer.init("  " + lang.getString("DisconnectNetorkPlayer") + "  ",menuItems);
				popupMenuDisconnectPlayer.setEnabled(true);
				popupMenuDisconnectPlayer.setVisible(true);
			}
			else if(result.first == keyboardSetupPopupMenuIndex) {
				MainMenu *newMenu = new MainMenu(program); // open keyboard shortcuts setup screen
				currentUIState = newMenu;
				Renderer &renderer= Renderer::getInstance();
				renderer.setCustom3dMenu(newMenu);
				//currentUIState->load();
				currentUIState->init();

				// open keyboard shortcuts setup screen
				newMenu->setState(new MenuStateKeysetup(program, newMenu, &currentUIState));
			}
			else if(result.first == pauseGamePopupMenuIndex) {
				//this->setPaused(!paused);
				//printf("popup paused = %d\n",paused);

				bool allowAdminMenuItems = false;
				NetworkRole role = networkManager.getNetworkRole();
				if(role == nrServer) {
					allowAdminMenuItems = true;
				}
				else if(role == nrClient) {
					ClientInterface *clientInterface = dynamic_cast<ClientInterface *>(networkManager.getClientInterface());

					if(clientInterface != NULL &&
						gameSettings.getMasterserver_admin() == clientInterface->getSessionKey()) {
						allowAdminMenuItems = true;
					}
				}

				if(allowAdminMenuItems) {
					if(getPaused() == false) {
						commander.tryPauseGame(false,false);
					}
					else {
						commander.tryResumeGame(false,false);
					}
				}
			}
			else if(result.first == saveGamePopupMenuIndex){
				saveGame();
			}
			//else if(result.first == markCellPopupMenuIndex) {
			//	startMarkCell();
			//}
			//else if(result.first == unmarkCellPopupMenuIndex) {
			//	isUnMarkCellEnabled = true;
			//}
		}
		else if(popupMenuSwitchTeams.mouseClick(x, y)) {
			//popupMenuSwitchTeams
			std::pair<int,string> result = popupMenuSwitchTeams.mouseClickedMenuItem(x, y);
			//printf("In popup callback menuItemSelected [%s] menuIndexSelected = %d\n",result.second.c_str(),result.first);

			popupMenuSwitchTeams.setEnabled(false);
			popupMenuSwitchTeams.setVisible(false);

			//bool isNetworkGame = this->gameSettings.isNetworkGame();

			int teamIndex = switchTeamIndexMap[result.first];
			switch(teamIndex) {
				case CREATE_NEW_TEAM:
					{
					int newTeam = getFirstUnusedTeamNumber();
					//if(isNetworkGame == true) {
						const Faction *faction = world.getThisFaction();
						commander.trySwitchTeam(faction,newTeam);
					//}
					//else {
					//	const Faction *faction = world.getThisFaction();
					//	commander.trySwitchTeam(faction,newTeam);
					//}
					}
					break;
				case CANCEL_SWITCH_TEAM:
					break;
				default:
					//if(isNetworkGame == true) {
						const Faction *faction = world.getThisFaction();
						commander.trySwitchTeam(faction,teamIndex);
					//}
					//else {
					//	const Faction *faction = world.getThisFaction();
					//	commander.trySwitchTeam(faction,teamIndex);
					//}

					break;
			}
		}
		else if(popupMenuDisconnectPlayer.mouseClick(x, y)) {
			//popupMenuSwitchTeams
			std::pair<int,string> result = popupMenuDisconnectPlayer.mouseClickedMenuItem(x, y);
			//printf("In popup callback menuItemSelected [%s] menuIndexSelected = %d\n",result.second.c_str(),result.first);

			popupMenuDisconnectPlayer.setEnabled(false);
			popupMenuDisconnectPlayer.setVisible(false);

			//bool isNetworkGame = this->gameSettings.isNetworkGame();

			//int playerIndex = disconnectPlayerIndexMap[result.first];
			int factionIndex = disconnectPlayerIndexMap[result.first];
			switch(factionIndex) {
				case CANCEL_DISCONNECT_PLAYER:
					break;
				default:
//					if(isNetworkGame == true) {
//						const Faction *faction = world.getThisFaction();
//						commander.trySwitchTeam(faction,teamIndex);
//					}
//					else {
//						const Faction *faction = world.getThisFaction();
//						commander.trySwitchTeam(faction,teamIndex);
//					}


					GameSettings *settings = world.getGameSettingsPtr();
					Lang &lang= Lang::getInstance();

					char szBuf[8096]="";
					if(lang.hasString("DisconnectNetorkPlayerIndexConfirm") == true) {
						snprintf(szBuf,8096,("  " + lang.getString("DisconnectNetorkPlayerIndexConfirm") + "  ").c_str(),factionIndex+1,settings->getNetworkPlayerName(factionIndex).c_str());
					}
					else {
						snprintf(szBuf,8096,"  Confirm disconnection for player #%d - %s?  ",factionIndex+1,settings->getNetworkPlayerName(factionIndex).c_str());
					}

					disconnectPlayerConfirmMessageBox.setText(szBuf);
					disconnectPlayerConfirmMessageBox.init(lang.getString("Yes"), lang.getString("No"));
					disconnectPlayerConfirmMessageBox.setEnabled(true);

					playerIndexDisconnect = world.getFaction(factionIndex)->getStartLocationIndex();

					GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
					if(gameNetworkInterface != NULL) {
						Lang &lang= Lang::getInstance();
						const vector<string> languageList = settings->getUniqueNetworkPlayerLanguages();
						for(unsigned int i = 0; i < (unsigned int)languageList.size(); ++i) {
							char szMsg[8096]="";
							if(lang.hasString("DisconnectNetorkPlayerIndexConfirmed",languageList[i]) == true) {
								snprintf(szMsg,8096,lang.getString("DisconnectNetorkPlayerIndexConfirmed",languageList[i]).c_str(),factionIndex+1,settings->getNetworkPlayerName(factionIndex).c_str());
							}
							else {
								snprintf(szMsg,8096,"Notice - Admin is warning to disconnect player #%d - %s!",factionIndex+1,settings->getNetworkPlayerName(factionIndex).c_str());
							}
							bool localEcho = lang.isLanguageLocal(languageList[i]);
							gameNetworkInterface->sendTextMessage(szMsg,-1, localEcho,languageList[i]);
						}

						sleep(10);
					}

					break;
			}
		}

		if(switchTeamConfirmMessageBox.getEnabled() == true) {
			int button= -1;
			if(switchTeamConfirmMessageBox.mouseClick(x,y,button)) {
				switchTeamConfirmMessageBox.setEnabled(false);

				SwitchTeamVote *vote = world.getThisFactionPtr()->getSwitchTeamVote(world.getThisFaction()->getCurrentSwitchTeamVoteFactionIndex());
				vote->voted = true;
				vote->allowSwitchTeam = (button == 0);

				const Faction *faction = world.getThisFaction();
				commander.trySwitchTeamVote(faction,vote);
			}
		}
		else if(disconnectPlayerConfirmMessageBox.getEnabled() == true) {
			int button= -1;
			if(disconnectPlayerConfirmMessageBox.mouseClick(x,y,button)) {
				disconnectPlayerConfirmMessageBox.setEnabled(false);

				if(button == 0) {
					const Faction *faction = world.getThisFaction();
					commander.tryDisconnectNetworkPlayer(faction,playerIndexDisconnect);
				}
			}
		}

		//scrip message box, only if the exit box is not enabled
		if( mainMessageBox.getEnabled() == false &&
			errorMessageBox.getEnabled() == false &&
			scriptManager.getMessageBox()->getEnabled()) {
			int button= 0;
			if(scriptManager.getMessageBox()->mouseClick(x, y, button)){
				scriptManager.onMessageBoxOk();
				messageBoxClick= true;
			}
		}

		//minimap panel
		if(messageBoxClick == false) {
			if(metrics.isInMinimap(x, y)){
				int xm= x - metrics.getMinimapX();
				int ym= y - metrics.getMinimapY();
				int xCell= static_cast<int>(xm * (static_cast<float>(map->getW()) / metrics.getMinimapW()));
				int yCell= static_cast<int>(map->getH() - ym * (static_cast<float>(map->getH()) / metrics.getMinimapH()));

				if(map->isInside(xCell, yCell) && map->isInsideSurface(map->toSurfCoords(Vec2i(xCell,yCell)))) {
					if(gui.isSelectingPos()){
						gui.mouseDownLeftGraphics(xCell, yCell, true);
					}
					else
					{
						if(!setMarker) {
							cameraDragAllowed=true;
							gameCamera.setPos(Vec2f(static_cast<float>(xCell), static_cast<float>(yCell)));
						}
					}

					if(setMarker) {
						Vec2i surfaceCellPos = map->toSurfCoords(Vec2i(xCell,yCell));
						SurfaceCell *sc = map->getSurfaceCell(surfaceCellPos);
						Vec3f vertex = sc->getVertex();
						Vec2i targetPos(vertex.x,vertex.z);

						MarkedCell mc(targetPos,world.getThisFaction(),"none",world.getThisFaction()->getStartLocationIndex());
						addOrReplaceInHighlightedCells(mc);

						GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
						gameNetworkInterface->sendHighlightCellMessage(mc.getTargetPos(),mc.getFaction()->getIndex());
					}


					if(originalIsMarkCellEnabled == true && isMarkCellEnabled == true) {
						Vec2i surfaceCellPos = map->toSurfCoords(Vec2i(xCell,yCell));
						SurfaceCell *sc = map->getSurfaceCell(surfaceCellPos);
						Vec3f vertex = sc->getVertex();
						Vec2i targetPos(vertex.x,vertex.z);

						MarkedCell mc(targetPos,world.getThisFaction(),"placeholder for note",world.getThisFaction()->getStartLocationIndex());

						//GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
						//gameNetworkInterface->sendMarkCellMessage(mc.getTargetPos(),mc.getFaction()->getIndex(),mc.getNote());

						//printf("#1 ADDED in marked list pos [%s] markedCells.size() = " MG_SIZE_T_SPECIFIER "\n",surfaceCellPos.getString().c_str(),mapMarkedCellList.size());

						isMarkCellEnabled = false;
						cellMarkedData = mc;
						cellMarkedPos = surfaceCellPos;
						isMarkCellTextEnabled = true;
						chatManager.switchOnEdit(this,500);

						Renderer &renderer= Renderer::getInstance();
						//renderer.updateMarkedCellScreenPosQuadCache(surfaceCellPos);
						renderer.forceQuadCacheUpdate();
					}
					if(originalIsUnMarkCellEnabled == true && isUnMarkCellEnabled == true) {
						Vec2i surfaceCellPos = map->toSurfCoords(Vec2i(xCell,yCell));
						SurfaceCell *sc = map->getSurfaceCell(surfaceCellPos);
						Vec3f vertex = sc->getVertex();
						Vec2i targetPos(vertex.x,vertex.z);

//						if(mapMarkedCellList.find(surfaceCellPos) != mapMarkedCellList.end()) {
//							MarkedCell mc = mapMarkedCellList[surfaceCellPos];
//							if(mc.getFaction() == world.getThisFaction()) {
//								mapMarkedCellList.erase(surfaceCellPos);
//								GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
//								gameNetworkInterface->sendUnMarkCellMessage(mc.getTargetPos(),mc.getFaction()->getIndex());
//							}
//						}

						isUnMarkCellEnabled = false;

						removeCellMarker(surfaceCellPos, world.getThisFaction());
						//printf("#1 ADDED in marked list pos [%s] markedCells.size() = " MG_SIZE_T_SPECIFIER "\n",surfaceCellPos.getString().c_str(),mapMarkedCellList.size());

						Renderer &renderer= Renderer::getInstance();
						//renderer.updateMarkedCellScreenPosQuadCache(surfaceCellPos);
						renderer.forceQuadCacheUpdate();
					}
				}
			}
			//display panel
			else if(metrics.isInDisplay(x, y) && !gui.isSelectingPos()) {
				int xd= x - metrics.getDisplayX();
				int yd= y - metrics.getDisplayY();
				if(gui.mouseValid(xd, yd)) {
					gui.mouseDownLeftDisplay(xd, yd);
				}
				else {
					gui.mouseDownLeftGraphics(x, y, false);
				}
			}
			//graphics panel
			else {
				gui.mouseDownLeftGraphics(x, y, false);

				if(setMarker) {
					Vec2i targetPos;
					Vec2i screenPos(x,y-60);
					targetPos=getMouseCellPos();
					//Vec2i surfaceCellPos = map->toSurfCoords(targetPos);


					MarkedCell mc(targetPos,world.getThisFaction(),"none",world.getThisFaction()->getStartLocationIndex());
					addOrReplaceInHighlightedCells(mc);

					GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
					gameNetworkInterface->sendHighlightCellMessage(mc.getTargetPos(),mc.getFaction()->getIndex());
				}

				if(originalIsMarkCellEnabled == true && isMarkCellEnabled == true) {
					Vec2i targetPos;
					Vec2i screenPos(x,y-60);
					targetPos=getMouseCellPos();
					Vec2i surfaceCellPos = map->toSurfCoords(targetPos);

					MarkedCell mc(targetPos,world.getThisFaction(),"placeholder for note",world.getThisFaction()->getStartLocationIndex());

					//printf("#2 ADDED in marked list pos [%s] markedCells.size() = " MG_SIZE_T_SPECIFIER "\n",surfaceCellPos.getString().c_str(),mapMarkedCellList.size());

					isMarkCellEnabled = false;
					cellMarkedData = mc;
					cellMarkedPos = surfaceCellPos;
					isMarkCellTextEnabled = true;
					chatManager.switchOnEdit(this,500);

					//renderer.updateMarkedCellScreenPosQuadCache(surfaceCellPos);
					Renderer::getInstance().forceQuadCacheUpdate();
				}

				if(originalIsUnMarkCellEnabled == true && isUnMarkCellEnabled == true) {
					Vec2i targetPos;
					Vec2i screenPos(x,y-35);
					targetPos=getMouseCellPos();
					Vec2i surfaceCellPos = map->toSurfCoords(targetPos);

//					if(mapMarkedCellList.find(surfaceCellPos) != mapMarkedCellList.end()) {
//						MarkedCell mc = mapMarkedCellList[surfaceCellPos];
//						if(mc.getFaction() == world.getThisFaction()) {
//							mapMarkedCellList.erase(surfaceCellPos);
//							GameNetworkInterface *gameNetworkInterface= NetworkManager::getInstance().getGameNetworkInterface();
//							gameNetworkInterface->sendUnMarkCellMessage(mc.getTargetPos(),mc.getFaction()->getIndex());
//						}
//					}

					isUnMarkCellEnabled = false;
					removeCellMarker(surfaceCellPos, world.getThisFaction());
					//printf("#1 ADDED in marked list pos [%s] markedCells.size() = " MG_SIZE_T_SPECIFIER "\n",surfaceCellPos.getString().c_str(),mapMarkedCellList.size());

					//Renderer &renderer= Renderer::getInstance();
					//renderer.updateMarkedCellScreenPosQuadCache(surfaceCellPos);
					Renderer::getInstance().forceQuadCacheUpdate();
				}
			}
		}

		//exit message box, has to be the last thing to do in this function
		if(errorMessageBox.getEnabled() == true) {
			if(errorMessageBox.mouseClick(x, y)) {
				if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
				//close message box
				errorMessageBox.setEnabled(false);
			}
		}
		if(mainMessageBox.getEnabled()){
			int button= 0;
			if(mainMessageBox.mouseClick(x, y, button)) {
				if(button==0) {
					if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
					if(networkManager.getGameNetworkInterface() != NULL) {
						networkManager.getGameNetworkInterface()->quitGame(true);
					}
					quitTriggeredIndicator = true;
					return;
				}
				else {
					if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
					//close message box
					mainMessageBox.setEnabled(false);
				}
			}
		}
	}
	catch(const exception &ex) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s Line: %d] Error [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what());

		SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,szBuf);

		NetworkManager &networkManager= NetworkManager::getInstance();
		if(networkManager.getGameNetworkInterface() != NULL) {
			GameNetworkInterface *networkInterface = NetworkManager::getInstance().getGameNetworkInterface();
			networkInterface->sendTextMessage(szBuf,-1,true,"");
			sleep(10);
			networkManager.getGameNetworkInterface()->quitGame(true);
		}
		ErrorDisplayMessage(ex.what(),true);
	}
}

void Game::mouseDownRight(int x, int y) {
	if(this->masterserverMode == true) {
		return;
	}

	if(currentUIState != NULL) {
		currentUIState->mouseDownRight(x, y);
		return;
	}

	try {
		if(gameStarted == false || totalRenderFps <= 0) {
			Logger::getInstance().handleMouseClick(x, y);
			return;
		}

		Map *map= world.getMap();
		const Metrics &metrics= Metrics::getInstance();

		if(metrics.isInMinimap(x, y) ){
			int xm= x - metrics.getMinimapX();
			int ym= y - metrics.getMinimapY();
			int xCell= static_cast<int>(xm * (static_cast<float>(map->getW()) / metrics.getMinimapW()));
			int yCell= static_cast<int>(map->getH() - ym * (static_cast<float>(map->getH()) / metrics.getMinimapH()));

			if(map->isInside(xCell, yCell) && map->isInsideSurface(map->toSurfCoords(Vec2i(xCell,yCell)))) {
				gui.mouseDownRightGraphics(xCell, yCell,true);
			}
		}
		else {
			Vec2i targetPos;
			Vec2i screenPos(x,y);
			targetPos=getMouseCellPos();
			if(isValidMouseCellPos() == true &&
				map->isInsideSurface(map->toSurfCoords(targetPos)) == true) {
				gui.mouseDownRightGraphics(x, y,false);
			}
		}
	}
	catch(const exception &ex) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s Line: %d] Error [%s] x = %d y = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what(),x,y);

		SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,szBuf);

		NetworkManager &networkManager= NetworkManager::getInstance();
		if(networkManager.getGameNetworkInterface() != NULL) {
			GameNetworkInterface *networkInterface = NetworkManager::getInstance().getGameNetworkInterface();
			networkInterface->sendTextMessage(szBuf,-1,true,"");
			sleep(10);
			networkManager.getGameNetworkInterface()->quitGame(true);
		}
		ErrorDisplayMessage(ex.what(),true);
	}
}

 void Game::mouseUpCenter(int x, int y) {
	if(this->masterserverMode == true) {
		return;
	}

	if(gameStarted == false || totalRenderFps <= 0) {
		return;
	}

	if(currentUIState != NULL) {
		currentUIState->mouseUpCenter(x, y);
		return;
	}

 	if(mouseMoved == false) {
 		gameCamera.setState(GameCamera::sGame);
 	}
 	else {
 		mouseMoved = false;
 	}
}

void Game::mouseUpLeft(int x, int y) {
	if(this->masterserverMode == true) {
		return;
	}

	try {
		if(gameStarted == false || totalRenderFps <= 0) {
			return;
		}

		if(currentUIState != NULL) {
			currentUIState->mouseUpLeft(x, y);
			return;
		}

		gui.mouseUpLeftGraphics(x, y);
	}
	catch(const exception &ex) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s Line: %d] Error [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what());

		SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,szBuf);

		NetworkManager &networkManager= NetworkManager::getInstance();
		if(networkManager.getGameNetworkInterface() != NULL) {
			GameNetworkInterface *networkInterface = NetworkManager::getInstance().getGameNetworkInterface();
			networkInterface->sendTextMessage(szBuf,-1,true,"");
			sleep(10);
			networkManager.getGameNetworkInterface()->quitGame(true);
		}
		ErrorDisplayMessage(ex.what(),true);
	}
}

void Game::mouseDoubleClickLeft(int x, int y) {
	if(this->masterserverMode == true) {
		return;
	}

	try {
		if(gameStarted == false || totalRenderFps <= 0) {
			return;
		}
		if(currentUIState != NULL) {
			currentUIState->mouseDoubleClickLeft(x, y);
			return;
		}

		const Metrics &metrics= Metrics::getInstance();

		if(metrics.isInMinimap(x, y)){
			// no double click on minimap
		}
		else {
			//display panel
			if(metrics.isInDisplay(x, y) && !gui.isSelectingPos()) {
				int xd= x - metrics.getDisplayX();
				int yd= y - metrics.getDisplayY();
				if(gui.mouseValid(xd, yd)){
					return;
				}
			}
			//graphics panel
			gui.mouseDoubleClickLeftGraphics(x, y);
		}
	}
	catch(const exception &ex) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s Line: %d] Error [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what());

		SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,szBuf);

		NetworkManager &networkManager= NetworkManager::getInstance();
		if(networkManager.getGameNetworkInterface() != NULL) {
			GameNetworkInterface *networkInterface = NetworkManager::getInstance().getGameNetworkInterface();
			networkInterface->sendTextMessage(szBuf,-1,true,"");
			sleep(10);
			networkManager.getGameNetworkInterface()->quitGame(true);
		}
		ErrorDisplayMessage(ex.what(),true);
	}
}

void Game::mouseMove(int x, int y, const MouseState *ms) {
	if(this->masterserverMode == true) {
		return;
	}

	try {
		if(gameStarted == false || totalRenderFps <= 0) {
			return;
		}
		if(currentUIState != NULL) {
			currentUIState->mouseMove(x, y, ms);
			return;
		}

		popupMenu.mouseMove(x, y);
		popupMenuSwitchTeams.mouseMove(x, y);
		popupMenuDisconnectPlayer.mouseMove(x, y);

		const Metrics &metrics = Metrics::getInstance();

		mouseX = x;
		mouseY = y;

		if (ms->get(mbCenter)) {
			mouseMoved = true;
			if(currentCameraFollowUnit == NULL) {
				float ymult = 0.2f;
				float xmult = 0.2f;

				Vec2i oldPos = ::Shared::Platform::Window::getOldMousePos();
				int oldx= (oldPos.x * metrics.getVirtualW() / metrics.getScreenW());
				int oldy= ((metrics.getScreenH()-oldPos.y) * metrics.getVirtualH()  /  metrics.getScreenH());
				lastMousePos.x=oldx;
				lastMousePos.y=oldy;
				gameCamera.transitionVH(-(y - oldy) * ymult, (oldx - x) * xmult);
			}
			mouseX=lastMousePos.x;
			mouseY=lastMousePos.y;
			::Shared::Platform::Window::revertMousePos();

			return;
		}
		else if(currentCameraFollowUnit==NULL) {
			//if(Window::isKeyDown() == false)
			if(!camLeftButtonDown && !camRightButtonDown && !camUpButtonDown && !camDownButtonDown)
			{
				if(ms->get(mbLeft) && metrics.isInMinimap(x, y)) {
					int xm= x - metrics.getMinimapX();
					int ym= y - metrics.getMinimapY();

					Map *map= world.getMap();
					int xCell= static_cast<int>(xm * (static_cast<float>(map->getW()) / metrics.getMinimapW()));
					int yCell= static_cast<int>(map->getH() - ym * (static_cast<float>(map->getH()) / metrics.getMinimapH()));

					if(map->isInside(xCell, yCell) && map->isInsideSurface(map->toSurfCoords(Vec2i(xCell,yCell)))) {
						if(gui.isSelectingPos()){
							gui.mouseDownLeftGraphics(xCell, yCell, true);
						}
						else
						{
							if(cameraDragAllowed == true) {
								gameCamera.setPos(Vec2f(static_cast<float>(xCell), static_cast<float>(yCell)));
							}
						}
					}
				}
				else {
					bool mouseMoveScrollsWorld = Config::getInstance().getBool("MouseMoveScrollsWorld","true");
					if(mouseMoveScrollsWorld == true) {
						if (y < 10) {
							gameCamera.setMoveZ(-scrollSpeed);
						}
						else if (y > metrics.getVirtualH() - 10) {
							gameCamera.setMoveZ(scrollSpeed);
						}
						else {
							gameCamera.setMoveZ(0);
						}

						if (x < 10) {
							gameCamera.setMoveX(-scrollSpeed);
						}
						else if (x > metrics.getVirtualW() - 10) {
							gameCamera.setMoveX(scrollSpeed);
						}
						else {
							gameCamera.setMoveX(0);
						}
					}
				}
			}

			if(switchTeamConfirmMessageBox.getEnabled() == true) {
				switchTeamConfirmMessageBox.mouseMove(x,y);
			}

			if(disconnectPlayerConfirmMessageBox.getEnabled() == true) {
				disconnectPlayerConfirmMessageBox.mouseMove(x,y);
			}

			if (mainMessageBox.getEnabled()) {
				mainMessageBox.mouseMove(x, y);
			}
			if (errorMessageBox.getEnabled()) {
				errorMessageBox.mouseMove(x, y);
			}
			if (scriptManager.getMessageBox()->getEnabled()) {
				scriptManager.getMessageBox()->mouseMove(x, y);
			}
			//else if (saveBox) {
			//	saveBox->mouseMove(x, y);
			//} else {
			//	//graphics
			gui.mouseMoveGraphics(x, y);
			//}
		}

		//display
		if ( !gui.isSelecting() && !gui.isSelectingPos()) {
			if (!gui.isSelectingPos()) {
				if(metrics.isInDisplay(x, y)){
					gui.mouseMoveDisplay(x - metrics.getDisplayX(), y - metrics.getDisplayY());
				}
				else {
					gui.mouseMoveOutsideDisplay();
				}
			}
		}

		lastMousePos.x = mouseX;
		lastMousePos.y = mouseY;

		Renderer &renderer= Renderer::getInstance();
		renderer.ccomputePosition(Vec2i(mouseX, mouseY), mouseCellPos);
	}
	catch(const exception &ex) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s Line: %d] Error [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what());

		SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,szBuf);

		NetworkManager &networkManager= NetworkManager::getInstance();
		if(networkManager.getGameNetworkInterface() != NULL) {
			GameNetworkInterface *networkInterface = NetworkManager::getInstance().getGameNetworkInterface();
			networkInterface->sendTextMessage(szBuf,-1,true,"");
			sleep(10);
			networkManager.getGameNetworkInterface()->quitGame(true);
		}
		ErrorDisplayMessage(ex.what(),true);
	}
}

bool Game::isValidMouseCellPos() const{
	if(world.getMap() == NULL){
		return false;
	}
	else {
		return world.getMap()->isInside(mouseCellPos);
	}
}

void Game::eventMouseWheel(int x, int y, int zDelta) {
	if(this->masterserverMode == true) {
		return;
	}

	if(currentUIState != NULL) {
		currentUIState->eventMouseWheel(x, y, zDelta);
		return;
	}

	try {
		gameCamera.zoom((float)zDelta / 60.0f);
	}
	catch(const exception &ex) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s Line: %d] Error [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what());

		SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,szBuf);

		NetworkManager &networkManager= NetworkManager::getInstance();
		if(networkManager.getGameNetworkInterface() != NULL) {
			GameNetworkInterface *networkInterface = NetworkManager::getInstance().getGameNetworkInterface();
			networkInterface->sendTextMessage(szBuf,-1,true,"");
			sleep(10);
			networkManager.getGameNetworkInterface()->quitGame(true);
		}
		ErrorDisplayMessage(ex.what(),true);
	}
}

void Game::startCameraFollowUnit() {
	Selection *selection= gui.getSelectionPtr();
	if(selection->getCount() == 1) {
		Unit *currentUnit = selection->getUnitPtr(0);
		if(currentUnit != NULL) {
			currentCameraFollowUnit = currentUnit;
			getGameCameraPtr()->setState(GameCamera::sUnit);
			getGameCameraPtr()->setPos(currentCameraFollowUnit->getCurrMidHeightVector());

			int rotation=currentCameraFollowUnit->getRotation();
			getGameCameraPtr()->stop();
			getGameCameraPtr()->rotateToVH(0.0f,(540-rotation)%360);
			getGameCameraPtr()->setHAng((540-rotation)%360);
			getGameCameraPtr()->setVAng(0.0f);
		}
	}
	else {
		if(currentCameraFollowUnit != NULL) {
			currentCameraFollowUnit = NULL;
		}
	}
}

bool Game::textInput(std::string text) {
	if(chatManager.getEditEnabled() == true) {
		return chatManager.textInput(text);
	}
	return false;
}

bool Game::sdlKeyDown(SDL_KeyboardEvent key) {
	if(this->masterserverMode == true) {
		return false;
	}
	if(gameStarted == false || totalRenderFps <= 0) {
		return false;
	}

	if(chatManager.getEditEnabled() == true) {
		return false;
	}
	Config &configKeys = Config::getInstance(std::pair<ConfigType,ConfigType>(cfgMainKeys,cfgUserKeys));
	//group
	for(int idx = 1; idx <= Selection::maxGroups; idx++) {
		string keyName = "GroupUnitsKey" + intToStr(idx);

		SDL_Keycode groupHotKey = configKeys.getSDLKey(keyName.c_str());
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] keyName [%s] group index = %d, key = [%c] [%d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,keyName.c_str(),idx,groupHotKey,groupHotKey);

		//if(SystemFlags::VERBOSE_MODE_ENABLED) printf("input.keysym.mod = %d groupHotKey = %d key = %d (%d) [%s] isgroup = %d\n",key.keysym.mod,groupHotKey,key.keysym.sym,key.keysym.unicode,keyName.c_str(),isKeyPressed(groupHotKey,key));
		if(SystemFlags::VERBOSE_MODE_ENABLED) printf("input.keysym.mod = %d groupHotKey = %d key = (%d) [%s] isgroup = %d\n",key.keysym.mod,groupHotKey,key.keysym.sym,keyName.c_str(),isKeyPressed(groupHotKey,key));
		//printf(" group key check %d   scancode:%d sym:%d groupHotKey=%d  \n",idx,key.keysym.scancode,key.keysym.sym,groupHotKey);
		if(key.keysym.sym==groupHotKey){
			if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
			//printf("IS GROUP KEY %d   scancode:%d sym:%d groupHotKey=%d  \n",idx,key.keysym.scancode,key.keysym.sym,groupHotKey);
			gui.groupKey(idx-1);
			return true;
		}
	}
	return false;
}

void Game::keyDown(SDL_KeyboardEvent key) {
	if(this->masterserverMode == true) {
		return;
	}

	//printf("In game checking keypress for key [%d]\n",key.keysym.sym);

	try {
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] key = [%c] [%d] gameStarted [%d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,key.keysym.sym,key.keysym.sym, gameStarted);
		if(gameStarted == false || totalRenderFps <= 0) {
			return;
		}
		if(currentUIState != NULL) {
			currentUIState->keyDown(key);
			return;
		}

		Lang &lang= Lang::getInstance();
		bool formerChatState=chatManager.getEditEnabled();
		//send key to the chat manager
		chatManager.keyDown(key);

		if( formerChatState==false && chatManager.getEditEnabled()) {
				camUpButtonDown= false;
				camDownButtonDown = false;
				camLeftButtonDown = false;
				camRightButtonDown = false;

				gameCamera.stopMove();
		}

		//printf("GAME KEYDOWN #1\n");

		if(chatManager.getEditEnabled() == false) {
			//printf("GAME KEYDOWN #2\n");
			if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] key = [%d - %c]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,key.keysym.sym,key.keysym.sym);

			Config &configKeys = Config::getInstance(std::pair<ConfigType,ConfigType>(cfgMainKeys,cfgUserKeys));
			//if(SystemFlags::VERBOSE_MODE_ENABLED) printf ("In [%s::%s Line: %d] key = [%d - %c] pausegame [%d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,key,key,configKeys.getCharKey("PauseGame"));

			//printf("SDL [%d] key [%d][%d]\n",configKeys.getSDLKey("SetMarker"),key.keysym.unicode,key.keysym.sym);
			bool setMarkerKeyAllowsModifier = false;
			if( configKeys.getSDLKey("SetMarker") == SDLK_RALT ||
				configKeys.getSDLKey("SetMarker") == SDLK_LALT) {
				setMarkerKeyAllowsModifier = true;
			}

			//printf("In game checking keypress for key [%d] camera left [%d]\n",key.keysym.sym,configKeys.getSDLKey("CameraModeLeft"));

			if(isKeyPressed(configKeys.getSDLKey("RenderInGamePerformance"),key, false) == true) {
				renderInGamePerformance = !renderInGamePerformance;

				Config::getInstance().setBool("PerformanceWarningEnabled",renderInGamePerformance, true);
			}
			//if(key == configKeys.getCharKey("RenderNetworkStatus")) {
			else if(isKeyPressed(configKeys.getSDLKey("RenderNetworkStatus"),key, false) == true) {
				renderNetworkStatus= !renderNetworkStatus;
			}
			//else if(key == configKeys.getCharKey("ShowFullConsole")) {
			else if(isKeyPressed(configKeys.getSDLKey("ShowFullConsole"),key, false) == true) {
				showFullConsole= true;
			}
			else if(isKeyPressed(configKeys.getSDLKey("SetMarker"),key, setMarkerKeyAllowsModifier) == true) {
				setMarker= true;
			}
			//else if(key == configKeys.getCharKey("TogglePhotoMode")) {
			else if(isKeyPressed(configKeys.getSDLKey("TogglePhotoMode"),key, false) == true) {
				photoModeEnabled = !photoModeEnabled;
				if(	photoModeEnabled == true &&
					this->gameSettings.isNetworkGame() == false) {
					gameCamera.setMaxHeight(PHOTO_MODE_MAXHEIGHT);
				}
				else if(photoModeEnabled == false) {
					gameCamera.setMaxHeight(-1);
				}

			}
			//Toggle Healthbars
			else if(isKeyPressed(configKeys.getSDLKey("ToggleHealthbars"),key, false) == true) {
				switch (healthbarMode) {
					case hbvUndefined:
						healthbarMode=hbvOff;
						console.addLine(lang.getString("Healthbar2")+": "+lang.getString("HealthbarsOff"));
						break;
					case hbvOff:
						healthbarMode=hbvAlways;
						console.addLine(lang.getString("Healthbar2")+": "+lang.getString("HealthbarsAlways"));
						break;
					case hbvAlways:
						healthbarMode=hbvIfNeeded;
						console.addLine(lang.getString("Healthbar2")+": "+lang.getString("HealthbarsIfNeeded"));
						break;
					case hbvIfNeeded:
						healthbarMode=hbvSelected;
						console.addLine(lang.getString("Healthbar2")+": "+lang.getString("HealthbarsSelected"));
						break;
					case hbvSelected:
						healthbarMode=hbvSelected | hbvIfNeeded;
						console.addLine(lang.getString("Healthbar2")+": "+lang.getString("HealthbarsSelectedOrNeeded"));
						break;
					case (hbvSelected | hbvIfNeeded):
						healthbarMode=hbvUndefined;
						console.addLine(lang.getString("Healthbar2")+": "+lang.getString("HealthbarsFactionDefault"));
						break;
					default:
						printf("In [%s::%s Line: %d] Toggle Healthbars Hotkey - Invalid Value. Setting to default.\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
						healthbarMode=hbvUndefined;
						break;
				}
			}
			//Toggle music
			//else if(key == configKeys.getCharKey("ToggleMusic")) {
			else if(isKeyPressed(configKeys.getSDLKey("ToggleMusic"),key, false) == true) {

				if(this->masterserverMode == false) {
					Config &config = Config::getInstance();
					StrSound *gameMusic = world.getThisFaction()->getType()->getMusic();
					if(gameMusic != NULL) {
						float configVolume = (config.getInt("SoundVolumeMusic") / 100.f);
						float currentVolume = gameMusic->getVolume();
						if(currentVolume > 0) {
							gameMusic->setVolume(0);
							console.addLine(lang.getString("GameMusic") + " " + lang.getString("Off"));
						}
						else {
							//If the config says zero, use the default music volume
							gameMusic->setVolume(configVolume ? configVolume : 0.9);
							console.addLine(lang.getString("GameMusic"));
						}
					}
				}
			}
			//move camera left
			//else if(key == configKeys.getCharKey("CameraModeLeft")) {
			else if(isKeyPressed(configKeys.getSDLKey("CameraModeLeft"),key, false) == true) {
				gameCamera.setMoveX(-1);
				camLeftButtonDown=true;
			}
			//move camera right
			//else if(key == configKeys.getCharKey("CameraModeRight")) {
			else if(isKeyPressed(configKeys.getSDLKey("CameraModeRight"),key, false) == true) {
				gameCamera.setMoveX(1);
				camRightButtonDown=true;
			}
			//move camera up
			//else if(key == configKeys.getCharKey("CameraModeUp")) {
			else if(isKeyPressed(configKeys.getSDLKey("CameraModeUp"),key, false) == true) {
				gameCamera.setMoveZ(1);
				camUpButtonDown=true;
			}
			//move camera down
			//else if(key == configKeys.getCharKey("CameraModeDown")) {
			else if(isKeyPressed(configKeys.getSDLKey("CameraModeDown"),key, false) == true) {
				gameCamera.setMoveZ(-1);
				camDownButtonDown=true;
			}
			//change camera mode
			//else if(key == configKeys.getCharKey("FreeCameraMode")) {
			else if(isKeyPressed(configKeys.getSDLKey("FreeCameraMode"),key, false) == true) {
				if(gameCamera.getState()==GameCamera::sFree)
				{
					gameCamera.setState(GameCamera::sGame);
					string stateString= gameCamera.getState()==GameCamera::sGame? lang.getString("GameCamera"): lang.getString("FreeCamera");
					console.addLine(lang.getString("CameraModeSet")+" "+ stateString);
				}
				else if(gameCamera.getState()==GameCamera::sGame)
				{
					gameCamera.setState(GameCamera::sFree);
					string stateString= gameCamera.getState()==GameCamera::sGame? lang.getString("GameCamera"): lang.getString("FreeCamera");
					console.addLine(lang.getString("CameraModeSet")+" "+ stateString);
				}
				//else ignore!
			}
			//reset camera mode to normal
			//else if(key == configKeys.getCharKey("ResetCameraMode")) {
			else if(isKeyPressed(configKeys.getSDLKey("ResetCameraMode"),key, false) == true) {
				if(currentCameraFollowUnit != NULL) {
					currentCameraFollowUnit = NULL;
				}
				gameCamera.setState(GameCamera::sGame);
			}
			//pause
			//else if(key == configKeys.getCharKey("PauseGame")) {
			else if(isKeyPressed(configKeys.getSDLKey("PauseGame"),key, false) == true) {
				//printf("Toggle pause paused = %d\n",paused);
				//setPaused(!paused);

				bool allowAdminMenuItems = false;
				NetworkManager &networkManager= NetworkManager::getInstance();
				NetworkRole role = networkManager.getNetworkRole();
				if(role == nrServer) {
					allowAdminMenuItems = true;
				}
				else if(role == nrClient) {
					ClientInterface *clientInterface = dynamic_cast<ClientInterface *>(networkManager.getClientInterface());

					if(clientInterface != NULL &&
						gameSettings.getMasterserver_admin() == clientInterface->getSessionKey()) {
						allowAdminMenuItems = true;
					}
				}

				if(allowAdminMenuItems) {
					if(getPaused() == false) {
						commander.tryPauseGame(false,false);
					}
					else {
						commander.tryResumeGame(false,false);
					}
				}
			}
			else if(isKeyPressed(configKeys.getSDLKey("ExtraTeamColorMarker"),key, false) == true) {
				//printf("Toggle ExtraTeamColorMarker\n");
				toggleTeamColorMarker();
			}
			//switch display color
			//else if(key == configKeys.getCharKey("ChangeFontColor")) {
			else if(isKeyPressed(configKeys.getSDLKey("ChangeFontColor"),key, false) == true) {
				gui.switchToNextDisplayColor();
			}
			//increment speed
			//else if(key == configKeys.getCharKey("GameSpeedIncrease")) {
			else if(isKeyPressed(configKeys.getSDLKey("GameSpeedIncrease"),key, false) == true) {
				bool speedChangesAllowed= !NetworkManager::getInstance().isNetworkGameWithConnectedClients();
				if(speedChangesAllowed){
					incSpeed();
				}
			}
			//decrement speed
			//else if(key == configKeys.getCharKey("GameSpeedDecrease")) {
			else if(isKeyPressed(configKeys.getSDLKey("GameSpeedDecrease"),key, false) == true) {
				bool speedChangesAllowed= !NetworkManager::getInstance().isNetworkGameWithConnectedClients();
				if(speedChangesAllowed){
					decSpeed();
				}
			}
			else if(isKeyPressed(configKeys.getSDLKey("BookmarkAdd"),key, false) == true) {
				startMarkCell();
			}
			else if(isKeyPressed(configKeys.getSDLKey("BookmarkRemove"),key, false) == true) {
				isUnMarkCellEnabled = true;
			}
			else if(isKeyPressed(configKeys.getSDLKey("CameraFollowSelectedUnit"),key, false) == true) {
				startCameraFollowUnit();
			}
			//exit
			else if(isKeyPressed(configKeys.getSDLKey("ExitKey"),key, false) == true) {
				popupMenu.setEnabled(!popupMenu.getEnabled());
				popupMenu.setVisible(popupMenu.getEnabled());
			}

			//hotkeys
			if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] gameCamera.getState() = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,gameCamera.getState());

			if(gameCamera.getState() != GameCamera::sFree){
				if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] key = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,key);

				gui.hotKey(key);
			}
			else {
				//rotate camera leftt
				//if(key == configKeys.getCharKey("CameraRotateLeft")) {
				if(isKeyPressed(configKeys.getSDLKey("CameraRotateLeft"),key) == true) {
					gameCamera.setRotate(-1);
				}
				//rotate camera right
				//else if(key == configKeys.getCharKey("CameraRotateRight")){
				else if(isKeyPressed(configKeys.getSDLKey("CameraRotateRight"),key) == true) {
					gameCamera.setRotate(1);
				}
				//camera up
				//else if(key == configKeys.getCharKey("CameraRotateUp")) {
				else if(isKeyPressed(configKeys.getSDLKey("CameraRotateUp"),key) == true) {
					gameCamera.setMoveY(1);
				}
				//camera down
				//else if(key == configKeys.getCharKey("CameraRotateDown")) {
				else if(isKeyPressed(configKeys.getSDLKey("CameraRotateDown"),key) == true) {
					gameCamera.setMoveY(-1);
				}
			}

			if(isKeyPressed(configKeys.getSDLKey("SaveGame"),key) == true) {
				saveGame();
			}
		}
	}
	catch(const exception &ex) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s Line: %d] Error [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what());

		SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,szBuf);

		NetworkManager &networkManager= NetworkManager::getInstance();
		if(networkManager.getGameNetworkInterface() != NULL) {
			GameNetworkInterface *networkInterface = NetworkManager::getInstance().getGameNetworkInterface();
			networkInterface->sendTextMessage(szBuf,-1,true,"");
			sleep(10);
			networkManager.getGameNetworkInterface()->quitGame(true);
		}
		ErrorDisplayMessage(ex.what(),true);
	}
}

void Game::keyUp(SDL_KeyboardEvent key) {
	if(this->masterserverMode == true) {
		return;
	}

	try {
		if(gameStarted == false || totalRenderFps <= 0) {
			return;
		}
		if(currentUIState != NULL) {
			currentUIState->keyUp(key);
			return;
		}

		if(chatManager.getEditEnabled()) {
			//send key to the chat manager
			chatManager.keyUp(key);
		}
		else {
			Config &configKeys = Config::getInstance(std::pair<ConfigType,ConfigType>(cfgMainKeys,cfgUserKeys));

			//if(key == configKeys.getCharKey("ShowFullConsole")) {
			if(isKeyPressed(configKeys.getSDLKey("ShowFullConsole"),key) == true) {
				showFullConsole= false;
			}
			else if(isKeyPressed(configKeys.getSDLKey("SetMarker"),key) == true) {
				setMarker= false;
			}
			//else if(key == configKeys.getCharKey("CameraRotateLeft") ||
			//		key == configKeys.getCharKey("CameraRotateRight")) {
			else if(isKeyPressed(configKeys.getSDLKey("CameraRotateLeft"),key) == true ||
					isKeyPressed(configKeys.getSDLKey("CameraRotateRight"),key) == true) {
				gameCamera.setRotate(0);
			}
			//else if(key == configKeys.getCharKey("CameraRotateDown") ||
			//		key == configKeys.getCharKey("CameraRotateUp")) {
			else if(isKeyPressed(configKeys.getSDLKey("CameraRotateDown"),key) == true ||
					isKeyPressed(configKeys.getSDLKey("CameraRotateUp"),key) == true) {

				gameCamera.setMoveY(0);
			}
			//else if(key == configKeys.getCharKey("CameraModeUp")){
			else if(isKeyPressed(configKeys.getSDLKey("CameraModeUp"),key) == true) {
				gameCamera.setMoveZ(0);
				camUpButtonDown= false;
				calcCameraMoveZ();
			}
			//else if(key == configKeys.getCharKey("CameraModeDown")){
			else if(isKeyPressed(configKeys.getSDLKey("CameraModeDown"),key) == true) {
				gameCamera.setMoveZ(0);
				camDownButtonDown= false;
				calcCameraMoveZ();
			}
			//else if(key == configKeys.getCharKey("CameraModeLeft")){
			else if(isKeyPressed(configKeys.getSDLKey("CameraModeLeft"),key) == true) {
				gameCamera.setMoveX(0);
				camLeftButtonDown= false;
				calcCameraMoveX();
			}
			//else if(key == configKeys.getCharKey("CameraModeRight")){
			else if(isKeyPressed(configKeys.getSDLKey("CameraModeRight"),key) == true) {
				gameCamera.setMoveX(0);
				camRightButtonDown= false;
				calcCameraMoveX();
			} else {
				gui.hotKeyReleased(key);
			}
				
		}
	}
	catch(const exception &ex) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s Line: %d] Error [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what());

		SystemFlags::OutputDebug(SystemFlags::debugError,szBuf);
		if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,szBuf);

		NetworkManager &networkManager= NetworkManager::getInstance();
		if(networkManager.getGameNetworkInterface() != NULL) {
			GameNetworkInterface *networkInterface = NetworkManager::getInstance().getGameNetworkInterface();
			networkInterface->sendTextMessage(szBuf,-1,true,"");
			sleep(10);
			networkManager.getGameNetworkInterface()->quitGame(true);
		}
		ErrorDisplayMessage(ex.what(),true);
	}
}

void Game::calcCameraMoveX(){
	//move camera left
	if(camLeftButtonDown == true){
		gameCamera.setMoveX(-1);
	}
	//move camera right
	else if(camRightButtonDown == true){
		gameCamera.setMoveX(1);
	}
}
void Game::calcCameraMoveZ(){
	//move camera up
	if(camUpButtonDown == true){
		gameCamera.setMoveZ(1);
	}
	//move camera down
	else if(camDownButtonDown == true){
		gameCamera.setMoveZ(-1);
	}

}

void Game::keyPress(SDL_KeyboardEvent c) {
	if(this->masterserverMode == true) {
		return;
	}

	if(gameStarted == false || totalRenderFps <= 0) {
		return;
	}
	if(currentUIState != NULL) {
		currentUIState->keyPress(c);
		return;
	}

	chatManager.keyPress(c);
}

Stats Game::getEndGameStats() {
	Stats endStats;
	endStats = *(world.getStats());
	//NetworkManager &networkManager= NetworkManager::getInstance();
	if (this->masterserverMode == true) {
		endStats.setIsMasterserverMode(true);
	}
	return endStats;
}

Stats Game::quitGame() {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	if(quitGameCalled == true) {
		Stats endStats = getEndGameStats();
		return endStats;
	}
	quitGameCalled = true;

	NetworkManager &networkManager= NetworkManager::getInstance();
	NetworkRole role = networkManager.getNetworkRole();
	string suffix = "_client";
	if(role == nrServer) {
		suffix = "_server";
	}
	this->DumpCRCWorldLogIfRequired(suffix);

    if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled == true) {
        world.DumpWorldToLog();
    }
	//printf("Check savegame\n");
	//printf("Saving...\n");
    if(Config::getInstance().getBool("AutoTest")){
    	this->saveGame(GameConstants::saveGameFileAutoTestDefault);
    }

	Stats endStats = getEndGameStats();

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	//printf("In [%s::%s] Line: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
	NetworkManager::getInstance().end();
	//sleep(0);

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	return endStats;
}

void Game::DumpCRCWorldLogIfRequired(string fileSuffix) {
	bool isNetworkGame = this->gameSettings.isNetworkGame();
	if(isNetworkGame == true) {
		if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Check save world CRC to log. isNetworkGame = %d fileSuffix = %s\n",isNetworkGame,fileSuffix.c_str());

		GameSettings *settings = world.getGameSettingsPtr();
		if(settings != NULL &&
				(isFlagType1BitEnabled(ft1_network_synch_checks_verbose)  == true ||
				 isFlagType1BitEnabled(ft1_network_synch_checks) 			== true)) {
			string debugCRCWorldLogFile = Config::getInstance().getString("DebugCRCWorldLogFile","debugCRCWorld.log");
			debugCRCWorldLogFile += fileSuffix;

			if(getGameReadWritePath(GameConstants::path_logs_CacheLookupKey) != "") {
				debugCRCWorldLogFile = getGameReadWritePath(GameConstants::path_logs_CacheLookupKey) + debugCRCWorldLogFile;
			}
			else {
				string userData = Config::getInstance().getString("UserData_Root","");
				if(userData != "") {
					endPathWithSlash(userData);
				}
				debugCRCWorldLogFile = userData + debugCRCWorldLogFile;
			}

			printf("Save to log debugCRCWorldLogFile = %s\n",debugCRCWorldLogFile.c_str());

		#if defined(WIN32) && !defined(__MINGW32__)
			FILE *fp = _wfopen(utf8_decode(debugCRCWorldLogFile).c_str(), L"w");
			std::ofstream logFile(fp);
		#else
			std::ofstream logFile;
			logFile.open(debugCRCWorldLogFile.c_str(), ios_base::out | ios_base::trunc);
		#endif
			logFile << "World CRC debug information:"  << std::endl;
			logFile << "============================"  << std::endl;
			logFile << "Software version: " << glestVersionString << "-" << getCompilerNameString() << "-" << getGITRevisionString() << std::endl;
			logFile << "Maximum framecount: " << world.getFaction(0)->getCRC_DetailsForWorldFrameCount() << std::endl;


			for(unsigned int worldFrameIndex = 0; worldFrameIndex < world.getFaction(0)->getCRC_DetailsForWorldFrameCount(); ++worldFrameIndex) {
				//factions (and their related info)
				for(int i = 0; i < world.getFactionCount(); ++i) {
					logFile << "Faction detail for index: " << i << std::endl;
					logFile << "--------------------------" << std::endl;

					std::pair<int,string> details = world.getFaction(i)->getCRC_DetailsForWorldFrameIndex(worldFrameIndex);

					logFile << string("** world frame: ") << details.first << std::endl;
					logFile << details.second << std::endl;
				}
			}

			logFile.close();
		#if defined(WIN32) && !defined(__MINGW32__)
			if(fp) {
				fclose(fp);
			}
		#endif

		}
	}
}

void savePlayerStats(Game* game, Stats& endStats, PlayerAchievementsInterface *playerStats) {
	const double MIN_PLAY_TIME_MINUTES = 10.0;

	// Write out achievements here
	double elapsedGameMinutes = (game->getWorld()->getStats()->getFramesToCalculatePlaytime() / GameConstants::updateFps / 60.0);

	for (int factionIndex = 0;
			factionIndex < game->getWorld()->getFactionCount(); ++factionIndex) {
		if (factionIndex == game->getWorld()->getThisFactionIndex()) {
			//printf("\nWriting out game stats for Faction Index: %d won status: %d\n",factionIndex,endStats.getVictory(factionIndex));
			if(elapsedGameMinutes >= MIN_PLAY_TIME_MINUTES) {
				int gamesPlayedCount = playerStats->getStatAsInt(EnumParser<SteamStatName>::getString(games_played).c_str()) + 1;
				playerStats->setStatAsInt(EnumParser<SteamStatName>::getString(games_played).c_str(),gamesPlayedCount);

				if(gamesPlayedCount >= 50 && playerStats->isUnlocked(EnumParser<SteamAchievementName>::getString(ACH_PLAY_FIFTY_GAMES).c_str()) == false) {
					playerStats->unlock(EnumParser<SteamAchievementName>::getString(ACH_PLAY_FIFTY_GAMES).c_str());
				}
				if(gamesPlayedCount >= 100 && playerStats->isUnlocked(EnumParser<SteamAchievementName>::getString(ACH_PLAY_ONE_HUNDRED_GAMES).c_str()) == false) {
					playerStats->unlock(EnumParser<SteamAchievementName>::getString(ACH_PLAY_ONE_HUNDRED_GAMES).c_str());
				}
				if(gamesPlayedCount >= 250 && playerStats->isUnlocked(EnumParser<SteamAchievementName>::getString(ACH_PLAY_TWO_HUNDRED_FIFTY_GAMES).c_str()) == false) {
					playerStats->unlock(EnumParser<SteamAchievementName>::getString(ACH_PLAY_TWO_HUNDRED_FIFTY_GAMES).c_str());
				}
				if(gamesPlayedCount >= 500 && playerStats->isUnlocked(EnumParser<SteamAchievementName>::getString(ACH_PLAY_FIVE_HUNDRED_GAMES).c_str()) == false) {
					playerStats->unlock(EnumParser<SteamAchievementName>::getString(ACH_PLAY_FIVE_HUNDRED_GAMES).c_str());
				}
				if(gamesPlayedCount > 1000 && playerStats->isUnlocked(EnumParser<SteamAchievementName>::getString(ACH_PLAY_OVER_THOUSAND_GAMES).c_str()) == false) {
					playerStats->unlock(EnumParser<SteamAchievementName>::getString(ACH_PLAY_OVER_THOUSAND_GAMES).c_str());
				}

				if (NetworkManager::getInstance().isNetworkGame()) {
					int networkGamesPlayedCount = playerStats->getStatAsInt(EnumParser<SteamStatName>::getString(network_games_played).c_str()) + 1;
					playerStats->setStatAsInt(EnumParser<SteamStatName>::getString(network_games_played).c_str(),networkGamesPlayedCount);

					if(networkGamesPlayedCount >= 50 && playerStats->isUnlocked(EnumParser<SteamAchievementName>::getString(ACH_PLAY_FIFTY_GAMES_ONLINE).c_str()) == false) {
						playerStats->unlock(EnumParser<SteamAchievementName>::getString(ACH_PLAY_FIFTY_GAMES_ONLINE).c_str());
					}
					if(networkGamesPlayedCount >= 100 && playerStats->isUnlocked(EnumParser<SteamAchievementName>::getString(ACH_PLAY_ONE_HUNDRED_GAMES_ONLINE).c_str()) == false) {
						playerStats->unlock(EnumParser<SteamAchievementName>::getString(ACH_PLAY_ONE_HUNDRED_GAMES_ONLINE).c_str());
					}
					if(networkGamesPlayedCount >= 250 && playerStats->isUnlocked(EnumParser<SteamAchievementName>::getString(ACH_PLAY_TWO_HUNDRED_FIFTY_GAMES_ONLINE).c_str()) == false) {
						playerStats->unlock(EnumParser<SteamAchievementName>::getString(ACH_PLAY_TWO_HUNDRED_FIFTY_GAMES_ONLINE).c_str());
					}
					if(networkGamesPlayedCount >= 500 && playerStats->isUnlocked(EnumParser<SteamAchievementName>::getString(ACH_PLAY_FIVE_HUNDRED_GAMES_ONLINE).c_str()) == false) {
						playerStats->unlock(EnumParser<SteamAchievementName>::getString(ACH_PLAY_FIVE_HUNDRED_GAMES_ONLINE).c_str());
					}
					if(networkGamesPlayedCount > 1000 && playerStats->isUnlocked(EnumParser<SteamAchievementName>::getString(ACH_PLAY_OVER_THOUSAND_GAMES_ONLINE).c_str()) == false) {
						playerStats->unlock(EnumParser<SteamAchievementName>::getString(ACH_PLAY_OVER_THOUSAND_GAMES_ONLINE).c_str());
					}
				}

				if (endStats.getVictory(factionIndex)) {
					if(playerStats->isUnlocked(EnumParser<SteamAchievementName>::getString(ACH_WIN_ONE_GAME).c_str()) == false) {
						playerStats->unlock(EnumParser<SteamAchievementName>::getString(ACH_WIN_ONE_GAME).c_str());
					}

					if (NetworkManager::getInstance().isNetworkGame()) {
						int networkGamesWonCount = playerStats->getStatAsInt("network-games-won") + 1;
						playerStats->setStatAsInt("network-games-won",networkGamesWonCount);
					}

					//printf("\nPlayer won the game with at least 10 minutes of play: %f!\n",elapsedGameMinutes);
				}
			}
			else {
				//printf("\nPlayer won the game BUT NOT with at least 10 minutes of play: %f!\n",elapsedGameMinutes);
			}
		}
	}
	playerStats->storeStats();
}

void saveStatsToSteam(Game* game, Stats& endStats) {
	Steam* steamInstance = CacheManager::getCachedItem<Steam*>(GameConstants::steamCacheInstanceKey);
	if (steamInstance != NULL) {
		printf("\nSTEAM detected, writing out end game stats for player!\n");

		// Write out stats here
		if (NetworkManager::getInstance().isNetworkGame()) {
			//printf("\nSTEAM Refresh Stats!\n");
			steamInstance->requestRefreshStats();
			for (int factionIndex = 0;
					factionIndex < game->getWorld()->getFactionCount(); ++factionIndex) {
				if (factionIndex == game->getWorld()->getThisFactionIndex()) {
					//printf("\nWriting out game stats for Faction Index: %d!\n",factionIndex);
					if (endStats.getVictory(factionIndex)) {
						steamInstance->setStatAsInt(EnumParser<SteamStatName>::getString(stat_online_wins).c_str(),
								steamInstance->getStatAsInt(EnumParser<SteamStatName>::getString(stat_online_wins).c_str())
										+ 1);
					} else {
						steamInstance->setStatAsInt(EnumParser<SteamStatName>::getString(stat_online_loses).c_str(),
								steamInstance->getStatAsInt(EnumParser<SteamStatName>::getString(stat_online_loses).c_str())
										+ 1);
					}
					steamInstance->setStatAsInt(EnumParser<SteamStatName>::getString(stat_online_kills).c_str(),
							steamInstance->getStatAsInt(EnumParser<SteamStatName>::getString(stat_online_kills).c_str())
									+ endStats.getKills(factionIndex));
					steamInstance->setStatAsInt(EnumParser<SteamStatName>::getString(stat_online_kills_enemy).c_str(),
							steamInstance->getStatAsInt(
									EnumParser<SteamStatName>::getString(stat_online_kills_enemy).c_str())
									+ endStats.getEnemyKills(factionIndex));
					steamInstance->setStatAsInt(EnumParser<SteamStatName>::getString(stat_online_deaths).c_str(),
							steamInstance->getStatAsInt(EnumParser<SteamStatName>::getString(stat_online_deaths).c_str())
									+ endStats.getDeaths(factionIndex));
					steamInstance->setStatAsInt(EnumParser<SteamStatName>::getString(stat_online_units).c_str(),
							steamInstance->getStatAsInt(EnumParser<SteamStatName>::getString(stat_online_units).c_str())
									+ endStats.getUnitsProduced(factionIndex));
					steamInstance->setStatAsInt(
							EnumParser<SteamStatName>::getString(stat_online_resources_harvested).c_str(),
							steamInstance->getStatAsInt(
									EnumParser<SteamStatName>::getString(stat_online_resources_harvested).c_str())
									+ endStats.getResourcesHarvested(
											factionIndex));
					if (endStats.getPlayerLeftBeforeEnd(factionIndex)) {
						steamInstance->setStatAsInt(
								EnumParser<SteamStatName>::getString(stat_online_quit_before_end).c_str(),
								steamInstance->getStatAsInt(
										EnumParser<SteamStatName>::getString(stat_online_quit_before_end).c_str()) + 1);
					}
					steamInstance->setStatAsDouble(EnumParser<SteamStatName>::getString(stat_online_minutes_played).c_str(),
							steamInstance->getStatAsDouble(
									EnumParser<SteamStatName>::getString(stat_online_minutes_played).c_str())
									+ getTimeDuationMinutes(
											endStats.getFramesToCalculatePlaytime(),
											GameConstants::updateFps));
				}
			}
		}

		// Write out achievements here
		savePlayerStats(game, endStats, steamInstance);

		//printf("\nSTEAM Store Stats!\n");
		//steamInstance->storeStats();
		//printf("\nSTEAM Refresh Stats!\n");
		steamInstance->requestRefreshStats();
	}
	// Local Player stats for now just write out to player-stats.ini
	// We'll clean this up to use an interface that both steam and lcoal comply with so the 'update' logic
	// doesn't need to be duplicated.
	else {
		printf("\n**STEAM NOT detected, writing out NON STEAM end game stats for player!\n");
		string saveFilePlayerLocalStats = "player-stats.ini";
		if(getGameReadWritePath(GameConstants::path_logs_CacheLookupKey) != "") {
			saveFilePlayerLocalStats = getGameReadWritePath(GameConstants::path_logs_CacheLookupKey) + saveFilePlayerLocalStats;
		}
		else {
	        string userData = Config::getInstance().getString("UserData_Root","");
	        if(userData != "") {
	        	endPathWithSlash(userData);
	        }
	        saveFilePlayerLocalStats = userData + saveFilePlayerLocalStats;
		}
		SteamLocal playerLocalStats(saveFilePlayerLocalStats);
		PlayerAchievementsInterface *playerStats = &playerLocalStats;

		// Write out achievements here
		savePlayerStats(game, endStats, playerStats);
		//playerLocalStats->save(saveFilePlayerLocalStats);
	}
}

void Game::exitGameState(Program *program, Stats &endStats) {
	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	Game *game = dynamic_cast<Game *>(program->getState());

	//printf("game = %p\n",game);

	if(game) {
		game->setEndGameTeamWinnersAndLosers();
		game->endGame();
	}

	if((game != NULL && game->isMasterserverMode() == true) ||
		Config::getInstance().getBool("AutoTest") == true) {
		printf("Game ending with stats:\n");
		printf("-----------------------\n");

		string gameStats = endStats.getStats();
		printf("%s",gameStats.c_str());

		printf("-----------------------\n");
	}

	saveStatsToSteam(game, endStats);

	ProgramState *newState = new BattleEnd(program, &endStats, game);

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	program->setState(newState, false);

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
}

// ==================== PRIVATE ====================

void Game::highlightUnit(int unitId,float radius, float thickness, Vec4f color) {
	HighlightSpecialUnitInfo info;
	info.radius = radius;
	info.thickness = thickness;
	info.color = color;
	unitHighlightList[unitId] = info;
}

void Game::unhighlightUnit(int unitId) {
	unitHighlightList.erase(unitId);
}

// ==================== render ====================

void Game::render3d(){
	Chrono chrono;
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled) chrono.start();

	Renderer &renderer= Renderer::getInstance();

	//init
	renderer.reset3d();
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [reset3d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

//	renderer.computeVisibleQuad();
//	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [computeVisibleQuad]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
//	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

	renderer.loadGameCameraMatrix();
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [loadGameCameraMatrix]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

	renderer.computeVisibleQuad();
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [computeVisibleQuad]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

	renderer.setupLighting();
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [setupLighting]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

	//shadow map
	renderer.renderShadowsToTexture(avgRenderFps);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [renderShadowsToTexture]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

	//clear buffers
	renderer.clearBuffers();
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s] Line: %d renderFps = %d took msecs: %lld\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

	//surface
	renderer.renderSurface(avgRenderFps);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [renderSurface]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

	//selection circles
	renderer.renderSelectionEffects(healthbarMode);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [renderSelectionEffects]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

	// renderTeamColorCircle
	if((renderExtraTeamColor&renderTeamColorCircleBit)>0){
		renderer.renderTeamColorCircle();
		if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [renderObjects]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
		if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();
	}

	renderer.renderSpecialHighlightUnits(unitHighlightList);

	// renderTeamColorCircle
	renderer.renderMorphEffects();
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [renderObjects]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

	//objects
	renderer.renderObjects(avgRenderFps);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [renderObjects]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

	//ground units
	renderer.renderUnits(false, avgRenderFps);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [renderUnits]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

	//water
	renderer.renderWater();
	renderer.renderWaterEffects();
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [renderWater]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

	//air units
	renderer.renderUnits(true, avgRenderFps);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [renderUnits]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

	//particles
	renderer.renderParticleManager(rsGame);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [renderParticleManager]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

	//renderOnTopBars (aka Healthbars)
	if(photoModeEnabled == false) {
		renderer.renderHealthBars(healthbarMode);
	}

	// renderTeamColorPlane
	if((renderExtraTeamColor&renderTeamColorPlaneBit)>0){
		renderer.renderTeamColorPlane();
		if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [renderObjects]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
		if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();
	}

	//mouse 3d
	renderer.renderMouse3d();
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [renderMouse3d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());

	renderer.renderUnitsToBuild(avgRenderFps);
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] renderFps = %d took msecs: %lld [renderUnitsToBuild]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,renderFps,chrono.getMillis());
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled && chrono.getMillis() > 0) chrono.start();

	renderer.setLastRenderFps(lastRenderFps);
}

void Game::updateWorldStats() {
    world.getStats()->setWorldTimeElapsed(world.getTimeFlow()->getTime());

    if(difftime((long int)time(NULL),lastMaxUnitCalcTime) >= 1) {
    	lastMaxUnitCalcTime = time(NULL);

		int totalUnitcount = 0;
		for(int i = 0; i < world.getFactionCount(); ++i) {
			Faction *faction= world.getFaction(i);
			totalUnitcount += faction->getUnitCount();
		}

		if(world.getStats()->getMaxConcurrentUnitCount() < totalUnitcount) {
			world.getStats()->setMaxConcurrentUnitCount(totalUnitcount);
		}
		world.getStats()->setTotalEndGameConcurrentUnitCount(totalUnitcount);
		world.getStats()->setFramesPlayed(world.getFrameCount());
    }
}

string Game::getDebugStats(std::map<int,string> &factionDebugInfo) {
	string str = "";

	if(this->masterserverMode == false) {
		str+= "MouseXY: "        + intToStr(mouseX) + "," + intToStr(mouseY)+"\n";

		if(world.getMap()->isInsideSurface(world.getMap()->toSurfCoords(mouseCellPos)) == true) {
			str+= "MouseXY cell coords: "        + intToStr(mouseCellPos.x) + "," + intToStr(mouseCellPos.y)+"\n";
		}

		str+= "PosObjWord: "     + intToStr(gui.getPosObjWorld().x) + "," + intToStr(gui.getPosObjWorld().y)+"\n";
	}

	str+= "Render FPS: "     + intToStr(lastRenderFps) + "[" + intToStr(avgRenderFps) + "]\n";
	str+= "Update FPS: "     + intToStr(lastUpdateFps) + "[" + intToStr(avgUpdateFps) + "]\n";

	if(this->masterserverMode == false) {
		str+= "GameCamera pos: " + floatToStr(gameCamera.getPos().x)+","+floatToStr(gameCamera.getPos().y)+","+floatToStr(gameCamera.getPos().z)+"\n";
		//str+= "Cached surfacedata: " +  intToStr(renderer.getCachedSurfaceDataSize())+"\n";
	}

	//intToStr(stats.getFramesToCalculatePlaytime()/GameConstants::updateFps/60
	str+= "Time: "           + floatToStr(world.getTimeFlow()->getTime(),2) + " [" + floatToStr((double)world.getStats()->getFramesToCalculatePlaytime() / (float)GameConstants::updateFps / 60.0,2) + "]\n";

	if(SystemFlags::getThreadedLoggerRunning() == true) {
		str+= "Log buffer count: " + intToStr(SystemFlags::getLogEntryBufferCount())+"\n";
	}

	str+= "UnitRangeCellsLookupItemCache: " + world.getUnitUpdater()->getUnitRangeCellsLookupItemCacheStats()+"\n";
	str+= "ExploredCellsLookupItemCache: " 	+ world.getExploredCellsLookupItemCacheStats()+"\n";
	str+= "FowAlphaCellsLookupItemCache: "  + world.getFowAlphaCellsLookupItemCacheStats()+"\n";

	const string selectionType = toLower(Config::getInstance().getString("SelectionType",Config::colorPicking));
	str += "Selection type: " + toLower(selectionType) + "\n";

	if(selectionType == Config::colorPicking) {
		str += "Color picking used color list size: " + intToStr(BaseColorPickEntity::getUsedColorIDListSize()) +"\n";
	}

	//str+= "AllFactionsCacheStats: "			+ world.getAllFactionsCacheStats()+"\n";
	//str+= "AttackWarningCount: " + intToStr(world.getUnitUpdater()->getAttackWarningCount()) + "\n";

	str+= "Map: " + gameSettings.getMap() +"\n";
	str+= "Tileset: " + gameSettings.getTileset() +"\n";
	str+= "Techtree: " + gameSettings.getTech() +"\n";

	if(this->masterserverMode == false) {
		Renderer &renderer= Renderer::getInstance();
		str+= "Triangle count: " + intToStr(renderer.getTriangleCount())+"\n";
		str+= "Vertex count: "   + intToStr(renderer.getPointCount())+"\n";
	}

	str+= "Frame count:"     + intToStr(world.getFrameCount())+"\n";

	//visible quad
	if(this->masterserverMode == false) {
		Renderer &renderer= Renderer::getInstance();
		Quad2i visibleQuad= renderer.getVisibleQuad();
		//Quad2i visibleQuadCamera= renderer.getVisibleQuadFromCamera();

		str+= "Visible quad:        ";
		for(int i= 0; i<4; ++i){
			str+= "(" + intToStr(visibleQuad.p[i].x) + "," +intToStr(visibleQuad.p[i].y) + ") ";
		}
	//		str+= "\n";
	//		str+= "Visible quad camera: ";
	//		for(int i= 0; i<4; ++i){
	//			str+= "(" + intToStr(visibleQuadCamera.p[i].x) + "," +intToStr(visibleQuadCamera.p[i].y) + ") ";
	//		}
		str+= "\n";

		str+= "Visible quad area:        " + floatToStr(visibleQuad.area()) +"\n";
	//		str+= "Visible quad camera area: " + floatToStr(visibleQuadCamera.area()) +"\n";

	//		Rect2i boundingRect= visibleQuad.computeBoundingRect();
	//		Rect2i scaledRect= boundingRect/Map::cellScale;
	//		scaledRect.clamp(0, 0, world.getMap()->getSurfaceW()-1, world.getMap()->getSurfaceH()-1);
	//		renderer.renderText3D("#1", coreData.getMenuFontNormal3D(), Vec3f(1.0f), scaledRect.p[0].x, scaledRect.p[0].y, false);
	//		renderer.renderText3D("#2", coreData.getMenuFontNormal3D(), Vec3f(1.0f), scaledRect.p[1].x, scaledRect.p[1].y, false);
	}

	int totalUnitcount = 0;
	for(int i = 0; i < world.getFactionCount(); ++i) {
		Faction *faction= world.getFaction(i);
		totalUnitcount += faction->getUnitCount();
	}

	if(this->masterserverMode == false) {
		Renderer &renderer= Renderer::getInstance();
		VisibleQuadContainerCache &qCache =renderer.getQuadCache();
		int visibleUnitCount = (int)qCache.visibleQuadUnitList.size();
		str+= "Visible unit count: " + intToStr(visibleUnitCount) + " total: " + intToStr(totalUnitcount) + "\n";

		int visibleObjectCount = (int)qCache.visibleObjectList.size();
		str+= "Visible object count: " + intToStr(visibleObjectCount) +"\n";
	}
	else {
		str+= "Total unit count: " + intToStr(totalUnitcount) + "\n";
	}

	// resources
	for(int i = 0; i < world.getFactionCount(); ++i) {
		string factionInfo = this->gameSettings.getNetworkPlayerName(i);
		//factionInfo += " [" + this->gameSettings.getNetworkPlayerUUID(i) + "]";
		float multi=world.getStats()->getResourceMultiplier(i);
		string multiplier="["+floatToStr(multi,1)+"]";
		switch(this->gameSettings.getFactionControl(i)) {
			case ctCpuEasy:
				factionInfo += " CPU Easy"+multiplier;
				break;
			case ctCpu:
				factionInfo += " CPU Normal"+multiplier;
				break;
			case ctCpuUltra:
				factionInfo += " CPU Ultra"+multiplier;
				break;
			case ctCpuMega:
				factionInfo += " CPU Mega"+multiplier;
				break;
		}

		factionInfo +=	" [" + formatString(this->gameSettings.getFactionTypeName(i)) +
				" team: " + intToStr(this->gameSettings.getTeam(i)) + "]";

//		bool showResourceDebugInfo = true;
//		if(showResourceDebugInfo == true) {
//			factionInfo +=" res: ";
//			for(int j = 0; j < world.getTechTree()->getResourceTypeCount(); ++j) {
//				factionInfo += world.getFaction(i)->getResource(j)->getType()->getName()+":"+intToStr(world.getFaction(i)->getResource(j)->getAmount());
//				factionInfo += " ";
//			}
//		}

		factionDebugInfo[i] = factionInfo;
	}

	return str;
}

void Game::render2d() {
	Renderer &renderer= Renderer::getInstance();
	//Config &config= Config::getInstance();
	CoreData &coreData= CoreData::getInstance();

	//init
	renderer.reset2d();

	//HUD
	if(visibleHUD == true && photoModeEnabled == false) {
		renderer.renderHud();
	}
	//display
	renderer.renderDisplay();

	//minimap
	if(photoModeEnabled == false) {
        renderer.renderMinimap();
	}

	renderer.renderVisibleMarkedCells();
	renderer.renderVisibleMarkedCells(true,lastMousePos.x,lastMousePos.y);

    //selection
	renderer.renderSelectionQuad();

	//highlighted Cells
	renderer.renderHighlightedCellsOnMinimap();

	if(switchTeamConfirmMessageBox.getEnabled() == true) {
		renderer.renderMessageBox(&switchTeamConfirmMessageBox);
	}

	if(disconnectPlayerConfirmMessageBox.getEnabled() == true) {
		renderer.renderMessageBox(&disconnectPlayerConfirmMessageBox);
	}

	//exit message box
	if(errorMessageBox.getEnabled()) {
		renderer.renderMessageBox(&errorMessageBox);
	}
	if(mainMessageBox.getEnabled()) {
		renderer.renderMessageBox(&mainMessageBox);
	}

	//script message box
	if( mainMessageBox.getEnabled() == false &&
		errorMessageBox.getEnabled() == false &&
		scriptManager.getMessageBoxEnabled()) {
		renderer.renderMessageBox(scriptManager.getMessageBox());
	}

	//script display text
	if(!scriptManager.getDisplayText().empty() && !scriptManager.getMessageBoxEnabled()){
		Vec4f fontColor = getGui()->getDisplay()->getColor();

		if(Renderer::renderText3DEnabled == true) {
			renderer.renderText3D(
				scriptManager.getDisplayText(), coreData.getMenuFontNormal3D(),
				Vec3f(fontColor.x,fontColor.y,fontColor.z), 200, 660, false);
		}
		else {
			renderer.renderText(
				scriptManager.getDisplayText(), coreData.getMenuFontNormal(),
				Vec3f(fontColor.x,fontColor.y,fontColor.z), 200, 660, false);
		}
	}

	renderVideoPlayer();

	renderer.renderPopupMenu(&popupMenu);
	renderer.renderPopupMenu(&popupMenuSwitchTeams);
	renderer.renderPopupMenu(&popupMenuDisconnectPlayer);

	if(program != NULL) program->renderProgramMsgBox();

	renderer.renderChatManager(&chatManager);

    //debug info
	bool perfLogging = false;
	if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled == true ||
	   SystemFlags::getSystemSettingType(SystemFlags::debugWorldSynch).enabled == true) {
		perfLogging = true;
	}

    string str="";
    std::map<int,string> factionDebugInfo;

	if( renderer.getShowDebugUI() == true ||
		(perfLogging == true && difftime((long int)time(NULL),lastRenderLog2d) >= 1)) {
		str = getDebugStats(factionDebugInfo);
	}

	if(this->getRenderInGamePerformance() == true) {
		renderer.renderPerformanceStats();
	}
	else {
		static time_t lastGamePerfCheck = time(NULL);
		if(difftime((long int)time(NULL),lastGamePerfCheck) > 3) {
			lastGamePerfCheck = time(NULL);

			getGamePerformanceCounts(true);
		}
	}

	if(renderer.getShowDebugUI() == true) {
		const Metrics &metrics= Metrics::getInstance();
		int mh= metrics.getMinimapH();

		if(this->getRenderInGamePerformance() == true) {
			mh = mh + ((int)gamePerformanceCounts.size() * 14);
		}

		const Vec4f fontColor=getGui()->getDisplay()->getColor();

		if(Renderer::renderText3DEnabled == true) {
			renderer.renderTextShadow3D(str, coreData.getMenuFontNormal3D(),
					fontColor, 10, metrics.getVirtualH() - mh - 60, false);
		}
		else {
			renderer.renderTextShadow(str, coreData.getMenuFontNormal(),
					fontColor, 10, metrics.getVirtualH() - mh - 60, false);
		}

        vector<string> lineTokens;
        Tokenize(str,lineTokens,"\n");
        int fontHeightNormal = (Renderer::renderText3DEnabled == true ? coreData.getMenuFontNormal3D()->getMetrics()->getHeight("W") : coreData.getMenuFontNormal()->getMetrics()->getHeight("W"));
        int fontHeightBig 	 = (Renderer::renderText3DEnabled == true ? coreData.getMenuFontBig3D()->getMetrics()->getHeight("W") : coreData.getMenuFontBig()->getMetrics()->getHeight("W"));
        int playerPosY = (int)lineTokens.size() * fontHeightNormal;

        //printf("lineTokens.size() = %d\n",lineTokens.size());

		for(int i = 0; i < world.getFactionCount(); ++i) {
			string factionInfo = factionDebugInfo[i];
			Vec3f playerColor = world.getFaction(i)->getTexture()->getPixmapConst()->getPixel3f(0, 0);

			if(Renderer::renderText3DEnabled == true) {
				renderer.renderText3D(factionInfo, coreData.getMenuFontBig3D(),
						Vec4f(playerColor.x,playerColor.y,playerColor.z,1.0),
						10,
						//metrics.getVirtualH() - mh - 90 - 280 - (i * 16),
						metrics.getVirtualH() - mh - 60 - playerPosY - fontHeightBig - (fontHeightBig * i),
						false);
			}
			else {
				renderer.renderText(factionInfo, coreData.getMenuFontBig(),
						Vec4f(playerColor.x,playerColor.y,playerColor.z,1.0),
						10,
						//metrics.getVirtualH() - mh - 90 - 280 - (i * 16),
						metrics.getVirtualH() - mh - 60 - playerPosY - fontHeightBig - (fontHeightBig * i),
						false);
			}
		}

		if((renderer.getShowDebugUILevel() & debugui_unit_titles) == debugui_unit_titles) {
			if(renderer.getAllowRenderUnitTitles() == false) {
				renderer.setAllowRenderUnitTitles(true);
			}

			if(Renderer::renderText3DEnabled == true) {
				renderer.renderUnitTitles3D(coreData.getMenuFontNormal3D(),Vec3f(1.0f));
			}
			else {
				renderer.renderUnitTitles(coreData.getMenuFontNormal(),Vec3f(1.0f));
			}
		}
	}

	//network status
	if(renderNetworkStatus == true) {
		if(NetworkManager::getInstance().getGameNetworkInterface() != NULL) {
			const Metrics &metrics= Metrics::getInstance();
			int mx= metrics.getMinimapX();
			//int my= metrics.getMinimapY();
			int mw= metrics.getMinimapW();
			//int mh= metrics.getMinimapH();
			const Vec4f fontColor=getGui()->getDisplay()->getColor();

			if(Renderer::renderText3DEnabled == true) {
				renderer.renderTextShadow3D(
								NetworkManager::getInstance().getGameNetworkInterface()->getNetworkStatus(),
								coreData.getMenuFontNormal3D(),
								fontColor, mx + mw + 5 , metrics.getVirtualH()-30-20, false);
			}
			else {
				renderer.renderTextShadow(
					NetworkManager::getInstance().getGameNetworkInterface()->getNetworkStatus(),
					coreData.getMenuFontNormal(),
					fontColor, mx + mw + 5 , metrics.getVirtualH()-30-20, false);
			}
		}
	}

	// clock
	if(photoModeEnabled == false && timeDisplay == true) {
		renderer.renderClock();
	}

    //resource info
	if(photoModeEnabled == false) {
		if(this->masterserverMode == false) {
			renderer.renderResourceStatus();
		}
		renderer.renderConsole(&console,showFullConsole?consoleFull:consoleNormal);
    }

    //2d mouse
	renderer.renderMouse2d(mouseX, mouseY, mouse2d, gui.isSelectingPos()? 1.f: 0.f);

	if(perfLogging == true && difftime((long int)time(NULL),lastRenderLog2d) >= 1) {
		lastRenderLog2d = time(NULL);

		if(SystemFlags::getSystemSettingType(SystemFlags::debugPerformance).enabled) SystemFlags::OutputDebug(SystemFlags::debugPerformance,"In [%s::%s Line: %d] Statistics: %s\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,str.c_str());
	}
}


// ==================== misc ====================

void Game::checkWinner() {
	// lookup int is team #, value is players alive on team
	std::map<int, int> teamsAlive = getTeamsAlive();

	if(gameOver == false || teamsAlive.size() > 1) {
		if(gameSettings.getDefaultVictoryConditions()) {
			checkWinnerStandard();
		}
		else {
			checkWinnerScripted();
		}
	}
}

void Game::setEndGameTeamWinnersAndLosers() {
	//bool lose= false;
	bool checkTeamIndex = !(this->masterserverMode == false && world.getThisFaction()->getPersonalityType() != fpt_Observer);

	// lookup int is team #, value is players alive on team
	std::map<int,int> teamsAlive;
	for(int i = 0; i < world.getFactionCount(); ++i) {
		if(checkTeamIndex == false || i != world.getThisFactionIndex()) {
			if(factionLostGame(world.getFaction(i)) == false) {
				teamsAlive[world.getFaction(i)->getTeam()] = teamsAlive[world.getFaction(i)->getTeam()] + 1;
			}
		}
	}

	// did some team win
	if(teamsAlive.size() <= 1) {
		for(int i=0; i< world.getFactionCount(); ++i) {
			if(checkTeamIndex == false || i != world.getThisFactionIndex()) {
				if(teamsAlive.find(world.getFaction(i)->getTeam()) != teamsAlive.end()) {
					world.getStats()->setVictorious(i);
				}
				else if(i == world.getThisFactionIndex()) {
					//lose= true;
				}
			}
		}
		bool firstGameOverTrigger = false;
		if(gameOver == false) {
			firstGameOverTrigger = true;
			gameOver= true;
		}
		if( this->gameSettings.isNetworkGame() == false ||
			this->gameSettings.getEnableObserverModeAtEndGame() == true) {
			// Let the happy winner view everything left in the world

			// This caused too much LAG for network games
			if(this->gameSettings.isNetworkGame() == false) {
				Renderer::getInstance().setPhotoMode(true);
				gameCamera.setMaxHeight(PHOTO_MODE_MAXHEIGHT);
			}
			// END
		}

		if(firstGameOverTrigger == true) {
			scriptManager.onGameOver(true);
		}

		if(world.getFactionCount() == 1 && world.getFaction(0)->getPersonalityType() == fpt_Observer) {
			//printf("!!!!!!!!!!!!!!!!!!!!");

			//gameCamera.setMoveY(100.0);
			gameCamera.zoom(-300);
			//gameCamera.update();
		}
//		else {
//			if(lose == true) {
//				showLoseMessageBox();
//			}
//			else {
//				showWinMessageBox();
//			}
//		}
	}
}

std::map<int, int> Game::getTeamsAlive() {
	std::map<int, int> teamsAlive;
	for (int factionIndex = 0; factionIndex < world.getFactionCount(); ++factionIndex) {
		if (factionIndex != world.getThisFactionIndex()) {
			if (factionLostGame(world.getFaction(factionIndex)) == false) {
				teamsAlive[world.getFaction(factionIndex)->getTeam()] =
						teamsAlive[world.getFaction(factionIndex)->getTeam()] + 1;
			}
		}
	}
	// did some team win
	return teamsAlive;
}

void Game::checkWinnerStandardHeadlessOrObserver() {
	// lookup int is team #, value is players alive on team
	std::map<int, int> teamsAlive = getTeamsAlive();

	// did some team win
	if (teamsAlive.size() <= 1) {
		if (this->masterserverMode == true) {
			printf("Game finished...\n");
		}
		for (int factionIndex = 0; factionIndex < world.getFactionCount(); ++factionIndex) {

			Faction* faction = world.getFaction(factionIndex);
			if (factionIndex != world.getThisFactionIndex() &&
					teamsAlive.find(faction->getTeam()) != teamsAlive.end()) {

				world.getStats()->setVictorious(factionIndex);
				if (this->masterserverMode == true) {

					printf("Player: %s is on the winning team #: %d\n",
							this->gameSettings.getNetworkPlayerName(factionIndex).c_str(),
							faction->getTeam());
				}
			}
		}
		bool wasGameOverAlready = gameOver;
		gameOver 				= true;

		// Only need to process this once
		if(wasGameOverAlready == false) {
			if (this->gameSettings.isNetworkGame() == false ||
				this->gameSettings.getEnableObserverModeAtEndGame() == true) {

				// Let the happy winner view everything left in the world
				// This caused too much LAG for network games
				if (this->gameSettings.isNetworkGame() == false) {

					Renderer::getInstance().setPhotoMode(true);
					gameCamera.setMaxHeight(PHOTO_MODE_MAXHEIGHT);
				}
				// END
			}
			scriptManager.onGameOver(true);
			if (world.getFactionCount() == 1 &&
					world.getFaction(0)->getPersonalityType() == fpt_Observer) {
				gameCamera.zoom(-300);
			}
			else {
				showWinMessageBox();
			}
		}
	}
}

void Game::checkWinnerStandardPlayer() {
	//lose
	bool lose = false;
	if (factionLostGame(world.getThisFaction()) == true) {

		bool playerLostGame = true;
		// Team Shared units enabled?
		if(isFlagType1BitEnabled(ft1_allow_shared_team_units) == true) {

			// Check if all team members have lost?
			for (int factionIndex = 0; factionIndex < world.getFactionCount(); ++factionIndex) {

				if (world.getFaction(factionIndex)->getPersonalityType() != fpt_Observer) {
					if (world.getFaction(factionIndex)->isAlly(world.getThisFaction()) == true &&
							factionLostGame(world.getFaction(factionIndex)) == false) {

						playerLostGame = false;
						break;
					}
				}
			}
		}

		if(playerLostGame == true) {
			lose = true;
			for (int factionIndex = 0; factionIndex < world.getFactionCount(); ++factionIndex) {

				if (world.getFaction(factionIndex)->getPersonalityType() != fpt_Observer) {
					if (world.getFaction(factionIndex)->isAlly(world.getThisFaction()) == false &&
							factionLostGame(world.getFaction(factionIndex)) == false) {

						world.getStats()->setVictorious(factionIndex);
					}
				}
			}
			bool wasGameOverAlready = gameOver;
			gameOver 				= true;

			// Only need to process losing once
			if(wasGameOverAlready == false) {
				if (this->gameSettings.isNetworkGame() == false ||
					this->gameSettings.getEnableObserverModeAtEndGame()
								== true) {
					// Let the poor user watch everything unfold
					// This caused too much LAG for network games
					if (this->gameSettings.isNetworkGame() == false) {
						Renderer::getInstance().setPhotoMode(true);
						gameCamera.setMaxHeight(PHOTO_MODE_MAXHEIGHT);
					}
					// END
					// but don't let him cheat via teamchat
					chatManager.setDisableTeamMode(true);
				}
				scriptManager.onGameOver(!lose);
				showLoseMessageBox();
			}
		}
	}

	//win
	if (lose == false) {
		bool win = true;
		for (int factionIndex = 0; factionIndex < world.getFactionCount(); ++factionIndex) {

			if (factionIndex != world.getThisFactionIndex()) {
				if (world.getFaction(factionIndex)->getPersonalityType() != fpt_Observer) {

					if (factionLostGame(world.getFaction(factionIndex)) == false &&
						world.getFaction(factionIndex)->isAlly(world.getThisFaction()) == false) {

						win = false;
					}
				}
			}
		}

		if (win) {
			for (int factionIndex = 0; factionIndex < world.getFactionCount(); ++factionIndex) {
				if (world.getFaction(factionIndex)->getPersonalityType() != fpt_Observer) {
					if (world.getFaction(factionIndex)->isAlly(world.getThisFaction())) {

						world.getStats()->setVictorious(factionIndex);
					}
				}
			}

			bool wasGameOverAlready = gameOver;
			gameOver 				= true;

			// Only need to process winning once
			if(wasGameOverAlready == false) {
				if (this->gameSettings.isNetworkGame() == false ||
					this->gameSettings.getEnableObserverModeAtEndGame() == true) {
					// Let the happy winner view everything left in the world

					// This caused too much LAG for network games
					if (this->gameSettings.isNetworkGame() == false) {
						Renderer::getInstance().setPhotoMode(true);
						gameCamera.setMaxHeight(PHOTO_MODE_MAXHEIGHT);
					}
					// END
				}
				scriptManager.onGameOver(win);
				showWinMessageBox();
			}
		}
	}
}

void Game::checkWinnerStandard() {
	if(world.getFactionCount() <= 0) {
		return;
	}
	if(this->masterserverMode == true ||
		world.getThisFaction()->getPersonalityType() == fpt_Observer) {
		checkWinnerStandardHeadlessOrObserver();
	}
	else {
		checkWinnerStandardPlayer();
	}
}

void Game::checkWinnerScripted() {
	if(scriptManager.getIsGameOver()) {
		bool wasGameOverAlready = gameOver;
		gameOver				= true;


		for(int index = 0; index < world.getFactionCount(); ++index) {
			if(scriptManager.getPlayerModifiers(index)->getWinner()) {

				world.getStats()->setVictorious(index);
			}
		}

		// Only need to process winning once
		if(wasGameOverAlready == false) {
			if( this->gameSettings.isNetworkGame() == false ||
				this->gameSettings.getEnableObserverModeAtEndGame() == true) {

				// Let the happy winner view everything left in the world
				// This caused too much LAG for network games
				if(this->gameSettings.isNetworkGame() == false) {
					Renderer::getInstance().setPhotoMode(true);
					gameCamera.setMaxHeight(PHOTO_MODE_MAXHEIGHT);
				}
				// END
			}

			if(this->masterserverMode == true ||
					world.getThisFaction()->getPersonalityType() == fpt_Observer) {
				showWinMessageBox();
			}
			else {
				scriptManager.onGameOver(scriptManager.getPlayerModifiers(world.getThisFactionIndex())->getWinner());

				if(scriptManager.getPlayerModifiers(world.getThisFactionIndex())->getWinner()){
					showWinMessageBox();
				}
				else {
					showLoseMessageBox();
				}
			}
		}
	}
}

bool Game::isFlagType1BitEnabled(FlagTypes1 type) const {
	return ((gameSettings.getFlagTypes1() & (uint32)type) == (uint32)type);
}

bool Game::factionLostGame(int factionIndex) {
	return factionLostGame(world.getFaction(factionIndex));
}

bool Game::factionLostGame(const Faction *faction) {
	if(faction != NULL) {
		for(int factionIndex=0; factionIndex<faction->getUnitCount(); ++factionIndex) {
			const UnitType *ut = faction->getUnit(factionIndex)->getType();
			if(ut->getCountInVictoryConditions() == ucvcNotSet) {
				if(faction->getUnit(factionIndex)->getType()->hasSkillClass(scBeBuilt)) {
					return false;
				}
			}
			else if(ut->getCountInVictoryConditions() == ucvcTrue) {
				return false;
			}
		}
	}
	return true;
}

//bool Game::hasBuilding(const Faction *faction) {
//	if(faction != NULL) {
//		for(int i=0; i<faction->getUnitCount(); ++i) {
//			if(faction->getUnit(i)->getType()->hasSkillClass(scBeBuilt)) {
//				return true;
//			}
//		}
//	}
//	return false;
//}

void Game::incSpeed() {
	if(disableSpeedChange == true) {
		return;
	}

	Lang &lang= Lang::getInstance();

	if(this->speed < Config::getInstance().getInt("FastSpeedLoops")) {
		if(this->speed == 0) {
			this->speed = 1;
		}
		else {
			this->speed++;
		}
		console.addLine(lang.getString("GameSpeedSet")+" "+((this->speed == 0)?lang.getString("Slow") : (this->speed == 1)?lang.getString("Normal"):"x"+intToStr(this->speed)));
	}
}

void Game::decSpeed() {
	if(disableSpeedChange == true) {
		return;
	}

	Lang &lang= Lang::getInstance();
	if(this->speed > 0) {
		this->speed--;
		console.addLine(lang.getString("GameSpeedSet")+" "+((this->speed == 0)?lang.getString("Slow") : (this->speed == 1)?lang.getString("Normal"):"x"+intToStr(this->speed)));
	}
}

void Game::setPaused(bool value,bool forceAllowPauseStateChange,bool clearCaches, bool joinNetworkGame) {
	bool speedChangesAllowed= !NetworkManager::getInstance().isNetworkGame();
	//printf("Toggle pause value = %d, speedChangesAllowed = %d, forceAllowPauseStateChange = %d\n",value,speedChangesAllowed,forceAllowPauseStateChange);

	if(SystemFlags::getSystemSettingType(SystemFlags::debugWorldSynch).enabled) SystemFlags::OutputDebug(SystemFlags::debugWorldSynch,"game.cpp line: %d setPaused value: %d clearCaches: %d forceAllowPauseStateChange: %d speedChangesAllowed: %d pausedForJoinGame: %d joinNetworkGame: %d\n",__LINE__,value,clearCaches,forceAllowPauseStateChange,speedChangesAllowed,pausedForJoinGame,joinNetworkGame);
	//printf("Line: %d setPaused value: %d clearCaches: %d forceAllowPauseStateChange: %d speedChangesAllowed: %d pausedForJoinGame: %d joinNetworkGame: %d\n",__LINE__,value,clearCaches,forceAllowPauseStateChange,speedChangesAllowed,pausedForJoinGame,joinNetworkGame);

	if(forceAllowPauseStateChange == true || speedChangesAllowed == true) {
		//printf("setPaused paused = %d, value = %d\n",paused,value);

		NetworkManager &networkManager= NetworkManager::getInstance();

		// Cannot change pause state while client is joining in progress game
		if(pausedForJoinGame == true && joinNetworkGame == false &&
			networkManager.getNetworkRole() == nrServer) {

			ServerInterface *server = NetworkManager::getInstance().getServerInterface();
			Lang &lang= Lang::getInstance();
			const vector<string> languageList = this->gameSettings.getUniqueNetworkPlayerLanguages();

			bool haveClientConnectedButNoReady = false;
			for(int i = 0; i < world.getFactionCount(); ++i) {
				Faction *faction = world.getFaction(i);

				MutexSafeWrapper safeMutex(server->getSlotMutex(faction->getStartLocationIndex()),CODE_AT_LINE);
				ConnectionSlot *slot =  server->getSlot(faction->getStartLocationIndex(),false);
				if(slot != NULL && slot->isConnected() == true && slot->isReady() == false) {
					for(unsigned int i = 0; i < languageList.size(); ++i) {

						char szMsg[8096]="";
						if(lang.hasString("JoinPlayerToCurrentGameLaunch",languageList[i]) == true) {
							snprintf(szMsg,8096,lang.getString("JoinPlayerToCurrentGameLaunch",languageList[i]).c_str(), slot->getName().c_str());
						}
						else {
							snprintf(szMsg,8096,"Player: %s is about to join the game, please wait...",slot->getName().c_str());
						}

						safeMutex.ReleaseLock();

						bool localEcho = lang.isLanguageLocal(languageList[i]);
						server->sendTextMessage(szMsg,-1, localEcho,languageList[i]);
						sleep(1);

						haveClientConnectedButNoReady = true;
					}
				}
			}

			if(haveClientConnectedButNoReady == true) {
				return;
			}
		}

		Lang &lang= Lang::getInstance();
		if(value == false) {
			console.addLine(lang.getString("GameResumed"));

			bool wasPausedForJoinGame = pausedForJoinGame;
			paused= false;
			pausedForJoinGame = false;
			pausedBeforeJoinGame = false;
			pauseStateChanged = true;

			if(clearCaches == true) {
				if(SystemFlags::getSystemSettingType(SystemFlags::debugWorldSynch).enabled) SystemFlags::OutputDebug(SystemFlags::debugWorldSynch,"game.cpp line: %d Clear Caches for resume in progress game\n",__LINE__);
				//printf("Line: %d Clear Caches for resume in progress game\n",__LINE__);

				world.clearCaches();
				for(int i = 0; i < world.getFactionCount(); ++i) {
					Faction *faction = world.getFaction(i);
					faction->clearCaches();
				}
				world.refreshAllUnitExplorations();
			}
			setupPopupMenus(false);

			if(networkManager.getNetworkRole() == nrClient &&
				wasPausedForJoinGame == true) {
				initialResumeSpeedLoops = true;
			}

			commander.setPauseNetworkCommands(false);
		}
		else {
			console.addLine(lang.getString("GamePaused"));

			if(joinNetworkGame == true) {
				pausedBeforeJoinGame = paused;
			}
			paused= true;
			pausedForJoinGame = joinNetworkGame;
			pauseStateChanged = true;

			if(clearCaches == true) {
				//printf("Line: %d Clear Caches for resume in progress game\n",__LINE__);
				if(SystemFlags::getSystemSettingType(SystemFlags::debugWorldSynch).enabled) SystemFlags::OutputDebug(SystemFlags::debugWorldSynch,"game.cpp line: %d Clear Caches for resume in progress game\n",__LINE__);

				world.clearCaches();
				for(int i = 0; i < world.getFactionCount(); ++i) {
					Faction *faction = world.getFaction(i);
					faction->clearCaches();
				}
				world.refreshAllUnitExplorations();
			}
			pauseRequestSent=false;

			commander.setPauseNetworkCommands(joinNetworkGame);
		}
		//printf("setPaused new paused = %d\n",paused);
	}
}

bool Game::getPaused()
{
	bool speedChangesAllowed= !NetworkManager::getInstance().isNetworkGame();
	if(speedChangesAllowed){
		if(popupMenu.getVisible() == true || popupMenuSwitchTeams.getVisible() == true){
			return true;
		}
//		if(mainMessageBox.getEnabled() == true || errorMessageBox.getEnabled() == true){
//			return true;
//		}
		if(currentUIState != NULL) {
			return true;
		}
	}
	return paused;
}

int Game::getUpdateLoops() {
	if(commander.hasReplayCommandListForFrame() == true) {
		return 1;
	}

	if(getPaused()) {
		return 0;
	}
	else if(this->speed == 0) {
		return updateFps % 2 == 0? 1: 0;
	}
	else
		return this->speed;
}

void Game::showLoseMessageBox() {
	Lang &lang= Lang::getInstance();

	NetworkManager &networkManager= NetworkManager::getInstance();
	if(networkManager.isNetworkGame() == true && networkManager.getNetworkRole() == nrServer) {
		showMessageBox(lang.getString("YouLose")+" "+lang.getString("ExitBattleServerQuestion"), lang.getString("BattleOver"), false);
	}
	else {
		showMessageBox(lang.getString("YouLose")+" "+lang.getString("ExitBattleQuestion"), lang.getString("BattleOver"), false);
	}
}

void Game::showWinMessageBox() {
	Lang &lang= Lang::getInstance();

	if(this->masterserverMode == true || world.getThisFaction()->getPersonalityType() == fpt_Observer) {
		showMessageBox(lang.getString("GameOver")+" "+lang.getString("ExitBattleQuestion"), lang.getString("BattleOver"), false);
	}
	else {
		showMessageBox(lang.getString("YouWin")+" "+lang.getString("ExitBattleQuestion"), lang.getString("BattleOver"), false);
	}
}

void Game::showMessageBox(const string &text, const string &header, bool toggle) {
	if(toggle == false) {
		mainMessageBox.setEnabled(false);
	}

	if(mainMessageBox.getEnabled() == false) {
		mainMessageBox.setText(text);
		mainMessageBox.setHeader(header);
		mainMessageBox.setEnabled(true);
	}
	else {
		mainMessageBox.setEnabled(false);
	}
}

void Game::showErrorMessageBox(const string &text, const string &header, bool toggle) {
	if(toggle == false) {
		errorMessageBox.setEnabled(false);
	}

	if(errorMessageBox.getEnabled() == false) {
		errorMessageBox.setText(text);
		errorMessageBox.setHeader(header);
		errorMessageBox.setEnabled(true);
	}
	else {
		errorMessageBox.setEnabled(false);
	}
}

void Game::startPerformanceTimer() {
	captureAvgTestStatus = true;
	updateFpsAvgTest = -1;
	renderFpsAvgTest = -1;
}

void Game::endPerformanceTimer() {
	captureAvgTestStatus = false;
}

Vec2i Game::getPerformanceTimerResults() {
	Vec2i results(this->updateFpsAvgTest,this->renderFpsAvgTest);
	return results;
}

void Game::consoleAddLine(string line) {
	console.addLine(line);
}
void Game::toggleTeamColorMarker() {
	renderExtraTeamColor++;
	renderExtraTeamColor=renderExtraTeamColor%4;
}

void Game::addNetworkCommandToReplayList(NetworkCommand* networkCommand, int worldFrameCount) {
	Config &config= Config::getInstance();
	if(config.getBool("SaveCommandsForReplay","false") == true) {
		replayCommandList.push_back(make_pair(worldFrameCount,*networkCommand));
	}
}

void Game::renderVideoPlayer() {
	if(videoPlayer != NULL) {
		if(videoPlayer->isPlaying() == true) {
			videoPlayer->playFrame(false);
		}
		else {
			delete videoPlayer;
			videoPlayer = NULL;
		}
	}
}

void Game::playStaticVideo(const string &playVideo) {
	if(GlobalStaticFlags::getIsNonGraphicalModeEnabled() == false &&
			::Shared::Graphics::VideoPlayer::hasBackEndVideoPlayer() == true) {

		//togglePauseGame(true,true);
		tryPauseToggle(true);
		setupRenderForVideo();


//		Context *c= GraphicsInterface::getInstance().getCurrentContext();
//		SDL_Surface *screen = static_cast<ContextGl*>(c)->getPlatformContextGlPtr()->getScreen();
//
//		string vlcPluginsPath = Config::getInstance().getString("VideoPlayerPluginsPath","");
//		//printf("screen->w = %d screen->h = %d screen->format->BitsPerPixel = %d\n",screen->w,screen->h,screen->format->BitsPerPixel);
//		Shared::Graphics::VideoPlayer player(playVideo.c_str(),
//							screen,
//							0,0,
//							screen->w,
//							screen->h,
//							screen->format->BitsPerPixel,
//							vlcPluginsPath,
//							SystemFlags::VERBOSE_MODE_ENABLED);
//		player.PlayVideo();
		//}
		//tryPauseToggle(false);

		playStreamingVideo(playVideo);
		playingStaticVideo = true;
	}
}
void Game::playStreamingVideo(const string &playVideo) {
	if(videoPlayer == NULL) {
		if(GlobalStaticFlags::getIsNonGraphicalModeEnabled() == false &&
				::Shared::Graphics::VideoPlayer::hasBackEndVideoPlayer() == true) {
			Context *c= GraphicsInterface::getInstance().getCurrentContext();
			PlatformContextGl *glCtx = static_cast<ContextGl*>(c)->getPlatformContextGlPtr();
			SDL_Window *window = glCtx->getScreenWindow();
			SDL_Surface *screen = glCtx->getScreenSurface();

			string vlcPluginsPath = Config::getInstance().getString("VideoPlayerPluginsPath","");

			videoPlayer = new ::Shared::Graphics::VideoPlayer(
					&Renderer::getInstance(),
					playVideo,
					"",
					window,
					0,0,
					screen->w,
					screen->h,
					screen->format->BitsPerPixel,
					false,
					vlcPluginsPath,
					SystemFlags::VERBOSE_MODE_ENABLED);
		}
	}
	else {
		if(videoPlayer->isPlaying() == false) {
			delete videoPlayer;
			videoPlayer = NULL;

			if(GlobalStaticFlags::getIsNonGraphicalModeEnabled() == false &&
					::Shared::Graphics::VideoPlayer::hasBackEndVideoPlayer() == true) {
				Context *c= GraphicsInterface::getInstance().getCurrentContext();
				PlatformContextGl *glCtx = static_cast<ContextGl*>(c)->getPlatformContextGlPtr();
				SDL_Window *window = glCtx->getScreenWindow();
				SDL_Surface *screen = glCtx->getScreenSurface();

				string vlcPluginsPath = Config::getInstance().getString("VideoPlayerPluginsPath","");

				videoPlayer = new ::Shared::Graphics::VideoPlayer(
						&Renderer::getInstance(),
						playVideo,
						"",
						window,
						0,0,
						screen->w,
						screen->h,
						screen->format->BitsPerPixel,
						false,
						vlcPluginsPath,
						SystemFlags::VERBOSE_MODE_ENABLED);
			}
		}
	}

	if(videoPlayer != NULL) {
		videoPlayer->initPlayer();
	}
}
void Game::stopStreamingVideo(const string &playVideo) {
	if(videoPlayer != NULL) {
		videoPlayer->StopVideo();
	}
}

void Game::stopAllVideo() {
	if(videoPlayer != NULL) {
		videoPlayer->StopVideo();
	}
}

bool Game::clientLagHandler(int slotIndex,bool networkPauseGameForLaggedClients) {
	if(networkPauseGameForLaggedClients == true) {
		printf("**WARNING** Detected lag from client: %d networkPauseGameForLaggedClients: %d\n",slotIndex,networkPauseGameForLaggedClients);
	}
	else {
		printf("==> Requested Resume Game after Pause for lagging client(s)...\n");
	}

	this->networkPauseGameForLaggedClientsRequested  = networkPauseGameForLaggedClients;
	this->networkResumeGameForLaggedClientsRequested = !networkPauseGameForLaggedClients;
	return true;
}

void Game::saveGame(){
	string file = this->saveGame(GameConstants::saveGameFilePattern);
	char szBuf[8096]="";
	Lang &lang= Lang::getInstance();
	snprintf(szBuf,8096,lang.getString("GameSaved","",true).c_str(),file.c_str());
	console.addLine(szBuf);

	Config &config= Config::getInstance();
	config.setString("LastSavedGame",file);
	config.save();
}

string Game::saveGame(string name, const string &path) {
	Config &config= Config::getInstance();
	// auto name file if using saved file pattern string
	if(name == GameConstants::saveGameFilePattern) {
		//time_t curTime = time(NULL);
	    //struct tm *loctime = localtime (&curTime);
		struct tm loctime = threadsafe_localtime(systemtime_now());
	    char szBuf2[100]="";
	    strftime(szBuf2,100,"%Y%m%d_%H%M%S",&loctime);

		char szBuf[8096]="";
		snprintf(szBuf,8096,name.c_str(),szBuf2);
		name = szBuf;
	}
	else if(name == GameConstants::saveGameFileAutoTestDefault) {
		//time_t curTime = time(NULL);
	    //struct tm *loctime = localtime (&curTime);
		struct tm loctime = threadsafe_localtime(systemtime_now());
	    char szBuf2[100]="";
	    strftime(szBuf2,100,"%Y%m%d_%H%M%S",&loctime);

		char szBuf[8096]="";
		snprintf(szBuf,8096,name.c_str(),szBuf2);
		name = szBuf;
	}

	// Save the file now
	string saveGameFile = path + name;
	if(getGameReadWritePath(GameConstants::path_logs_CacheLookupKey) != "") {
		saveGameFile = getGameReadWritePath(GameConstants::path_logs_CacheLookupKey) + saveGameFile;
	}
	else {
        string userData = config.getString("UserData_Root","");
        if(userData != "") {
        	endPathWithSlash(userData);
        }
        saveGameFile = userData + saveGameFile;
	}
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Saving game to [%s]\n",saveGameFile.c_str());

	// This condition will re-play all the commands from a replay file
	// INSTEAD of saving from a saved game.
	if(config.getBool("SaveCommandsForReplay","false") == true) {
		std::map<string,string> mapTagReplacements;
		XmlTree xmlTreeSaveGame(XML_RAPIDXML_ENGINE);

		xmlTreeSaveGame.init("megaglest-saved-game");
		XmlNode *rootNodeReplay = xmlTreeSaveGame.getRootNode();

		//std::map<string,string> mapTagReplacements;
		//time_t now = time(NULL);
		//struct tm *loctime = localtime (&now);
		struct tm loctime = threadsafe_localtime(systemtime_now());
		char szBuf[4096]="";
		strftime(szBuf,4095,"%Y-%m-%d %H:%M:%S",&loctime);

		rootNodeReplay->addAttribute("version",glestVersionString, mapTagReplacements);
		rootNodeReplay->addAttribute("timestamp",szBuf, mapTagReplacements);

		XmlNode *gameNodeReplay = rootNodeReplay->addChild("Game");
		gameSettings.saveGame(gameNodeReplay);

		gameNodeReplay->addAttribute("LastWorldFrameCount",intToStr(world.getFrameCount()), mapTagReplacements);

		for(unsigned int i = 0; i < replayCommandList.size(); ++i) {
			std::pair<int,NetworkCommand> &cmd = replayCommandList[i];
			XmlNode *networkCommandNode = cmd.second.saveGame(gameNodeReplay);
			networkCommandNode->addAttribute("worldFrameCount",intToStr(cmd.first), mapTagReplacements);
		}

		string replayFile = saveGameFile + ".replay";
		if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Saving game replay commands to [%s]\n",replayFile.c_str());
		xmlTreeSaveGame.save(replayFile);
	}

	XmlTree xmlTree;
	xmlTree.init("megaglest-saved-game");
	XmlNode *rootNode = xmlTree.getRootNode();

	std::map<string,string> mapTagReplacements;
	//time_t now = time(NULL);
    //struct tm *loctime = localtime (&now);
	struct tm loctime = threadsafe_localtime(systemtime_now());
    char szBuf[4096]="";
    strftime(szBuf,4095,"%Y-%m-%d %H:%M:%S",&loctime);

	rootNode->addAttribute("version",glestVersionString, mapTagReplacements);
	rootNode->addAttribute("timestamp",szBuf, mapTagReplacements);

	XmlNode *gameNode = rootNode->addChild("Game");
	//World world;
	world.saveGame(gameNode);
    //AiInterfaces aiInterfaces;
	for(unsigned int i = 0; i < aiInterfaces.size(); ++i) {
		AiInterface *aiIntf = aiInterfaces[i];
		if(aiIntf != NULL) {
			aiIntf->saveGame(gameNode);
		}
	}
    //Gui gui;
	gui.saveGame(gameNode);
    //GameCamera gameCamera;
	gameCamera.saveGame(gameNode);
    //Commander commander;
    //Console console;
	//ChatManager chatManager;
	//ScriptManager scriptManager;
	scriptManager.saveGame(gameNode);

	//misc
	//Checksum checksum;
	gameNode->addAttribute("checksum",intToStr(checksum.getSum()), mapTagReplacements);
    //string loadingText;
//    int mouse2d;
	gameNode->addAttribute("mouse2d",intToStr(mouse2d), mapTagReplacements);
//    int mouseX;
	gameNode->addAttribute("mouseX",intToStr(mouseX), mapTagReplacements);
//    int mouseY; //coords win32Api
	gameNode->addAttribute("mouseY",intToStr(mouseY), mapTagReplacements);
//    Vec2i mouseCellPos;
	gameNode->addAttribute("mouseCellPos",mouseCellPos.getString(), mapTagReplacements);
//	int updateFps, lastUpdateFps, avgUpdateFps;
//	int totalRenderFps, renderFps, lastRenderFps, avgRenderFps,currentAvgRenderFpsTotal;
	//Uint64 tickCount;
	gameNode->addAttribute("tickCount",intToStr(tickCount), mapTagReplacements);

	//bool paused;
	gameNode->addAttribute("paused",intToStr(paused), mapTagReplacements);
	//bool gameOver;
	gameNode->addAttribute("gameOver",intToStr(gameOver), mapTagReplacements);
	//bool renderNetworkStatus;
	//bool showFullConsole;
	//bool mouseMoved;
	//float scrollSpeed;
	gameNode->addAttribute("scrollSpeed",floatToStr(scrollSpeed,6), mapTagReplacements);
	//bool camLeftButtonDown;
	//bool camRightButtonDown;
	//bool camUpButtonDown;
	//bool camDownButtonDown;

	//Speed speed;
	gameNode->addAttribute("speed",intToStr(speed), mapTagReplacements);

	//GraphicMessageBox mainMessageBox;
	//GraphicMessageBox errorMessageBox;

	//misc ptr
	//ParticleSystem *weatherParticleSystem;
	if(weatherParticleSystem != NULL) {
		weatherParticleSystem->saveGame(gameNode);
	}
	//GameSettings gameSettings;
	gameSettings.saveGame(gameNode);
	//Vec2i lastMousePos;
	gameNode->addAttribute("lastMousePos",lastMousePos.getString(), mapTagReplacements);
	//time_t lastRenderLog2d;
	gameNode->addAttribute("lastRenderLog2d",intToStr(lastRenderLog2d), mapTagReplacements);
	//DisplayMessageFunction originalDisplayMsgCallback;
	//bool isFirstRender;
	gameNode->addAttribute("isFirstRender",intToStr(isFirstRender), mapTagReplacements);

	//bool quitTriggeredIndicator;
	//int original_updateFps;
	gameNode->addAttribute("original_updateFps",intToStr(original_updateFps), mapTagReplacements);
	//int original_cameraFps;
	gameNode->addAttribute("original_cameraFps",intToStr(original_cameraFps), mapTagReplacements);

	//bool captureAvgTestStatus;
	gameNode->addAttribute("captureAvgTestStatus",intToStr(captureAvgTestStatus), mapTagReplacements);
	//int updateFpsAvgTest;
	gameNode->addAttribute("updateFpsAvgTest",intToStr(updateFpsAvgTest), mapTagReplacements);
	//int renderFpsAvgTest;
	gameNode->addAttribute("renderFpsAvgTest",intToStr(renderFpsAvgTest), mapTagReplacements);

	//int renderExtraTeamColor;
	gameNode->addAttribute("renderExtraTeamColor",intToStr(renderExtraTeamColor), mapTagReplacements);

	//static const int renderTeamColorCircleBit=1;
	//static const int renderTeamColorPlaneBit=2;

	//bool photoModeEnabled;
	gameNode->addAttribute("photoModeEnabled",intToStr(photoModeEnabled), mapTagReplacements);
	//bool visibleHUD;
	gameNode->addAttribute("visibleHUD",intToStr(visibleHUD), mapTagReplacements);

	//bool timeDisplay
	gameNode->addAttribute("timeDisplay",intToStr(timeDisplay), mapTagReplacements);

	//bool withRainEffect;
	gameNode->addAttribute("withRainEffect",intToStr(withRainEffect), mapTagReplacements);
	//Program *program;

	//bool gameStarted;
	gameNode->addAttribute("gameStarted",intToStr(gameStarted), mapTagReplacements);

	//time_t lastMaxUnitCalcTime;
	gameNode->addAttribute("lastMaxUnitCalcTime",intToStr(lastMaxUnitCalcTime), mapTagReplacements);

	//PopupMenu popupMenu;
	//PopupMenu popupMenuSwitchTeams;

	//std::map<int,int> switchTeamIndexMap;
	//GraphicMessageBox switchTeamConfirmMessageBox;

	//int exitGamePopupMenuIndex;
	//int joinTeamPopupMenuIndex;
	//int pauseGamePopupMenuIndex;
	//int keyboardSetupPopupMenuIndex;
	//GLuint statelist3dMenu;
	//ProgramState *currentUIState;

	//bool masterserverMode;

	//StrSound *currentAmbientSound;

	//time_t lastNetworkPlayerConnectionCheck;
	gameNode->addAttribute("lastNetworkPlayerConnectionCheck",intToStr(lastNetworkPlayerConnectionCheck), mapTagReplacements);

	//time_t lastMasterServerGameStatsDump;
	gameNode->addAttribute("lastMasterServerGameStatsDump",intToStr(lastMasterServerGameStatsDump), mapTagReplacements);

	XmlNode *unitHighlightListNode = gameNode->addChild("unitHighlightList");
	//for(unsigned int i = 0; i < unitHighlightList.size(); ++i) {
	for(std::map<int,HighlightSpecialUnitInfo>::iterator iterMap = unitHighlightList.begin();
			iterMap != unitHighlightList.end(); ++iterMap) {
		HighlightSpecialUnitInfo &info = iterMap->second;
		XmlNode *infoNode = unitHighlightListNode->addChild("info");
		infoNode->addAttribute("unitid",intToStr(iterMap->first), mapTagReplacements);
		infoNode->addAttribute("radius",floatToStr(info.radius,6), mapTagReplacements);
		infoNode->addAttribute("thickness",floatToStr(info.thickness,6), mapTagReplacements);
		infoNode->addAttribute("color",info.color.getString(), mapTagReplacements);
	}

	gameNode->addAttribute("disableSpeedChange",intToStr(disableSpeedChange), mapTagReplacements);

	xmlTree.save(saveGameFile);

	if(masterserverMode == false) {
		// take Screenshot
		string jpgFileName=saveGameFile+".jpg";
		// menu is already disabled, last rendered screen is still with enabled one. Lets render again:
		render3d();
		render2d();
		Renderer::getInstance().saveScreen(jpgFileName,config.getInt("SaveGameScreenshotWidth","800"),config.getInt("SaveGameScreenshotHeight","600"));
	}

	return saveGameFile;
}

void Game::loadGame(string name,Program *programPtr,bool isMasterserverMode,const GameSettings *joinGameSettings) {
	Config &config= Config::getInstance();
	// This condition will re-play all the commands from a replay file
	// INSTEAD of saving from a saved game.
	if(joinGameSettings == NULL && config.getBool("SaveCommandsForReplay","false") == true) {
		XmlTree	xmlTreeReplay(XML_RAPIDXML_ENGINE);
		std::map<string,string> mapExtraTagReplacementValues;
		xmlTreeReplay.load(name + ".replay", Properties::getTagReplacementValues(&mapExtraTagReplacementValues),true);

		const XmlNode *rootNode= xmlTreeReplay.getRootNode();

		if(rootNode->hasChild("megaglest-saved-game") == true) {
			rootNode = rootNode->getChild("megaglest-saved-game");
		}

		//const XmlNode *versionNode= rootNode->getChild("megaglest-saved-game");
		const XmlNode *versionNode= rootNode;

		Lang &lang= Lang::getInstance();
		string gameVer = versionNode->getAttribute("version")->getValue();
		if(gameVer != glestVersionString && checkVersionComptability(gameVer, glestVersionString) == false){
			char szBuf[8096]="";
			snprintf(szBuf,8096,lang.getString("SavedGameBadVersion").c_str(),gameVer.c_str(),glestVersionString.c_str());
			throw megaglest_runtime_error(szBuf,true);
		}

		if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Found saved game version that matches your application version: [%s] --> [%s]\n",gameVer.c_str(),glestVersionString.c_str());

		XmlNode *gameNode = rootNode->getChild("Game");

		GameSettings newGameSettingsReplay;
		newGameSettingsReplay.loadGame(gameNode);
		//printf("Loading scenario [%s]\n",newGameSettingsReplay.getScenarioDir().c_str());
		if(newGameSettingsReplay.getScenarioDir() != "" && fileExists(newGameSettingsReplay.getScenarioDir()) == false) {
			newGameSettingsReplay.setScenarioDir(Scenario::getScenarioPath(Config::getInstance().getPathListForType(ptScenarios),newGameSettingsReplay.getScenario()));

			//printf("Loading scenario #2 [%s]\n",newGameSettingsReplay.getScenarioDir().c_str());
		}

		//GameSettings newGameSettings;
		//newGameSettings.loadGame(gameNode);
		//if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Game settings loaded\n");

		NetworkManager &networkManager= NetworkManager::getInstance();
		networkManager.end();
		networkManager.init(nrServer,true);

		Game *newGame = new Game(programPtr, &newGameSettingsReplay, isMasterserverMode);
		newGame->lastworldFrameCountForReplay = gameNode->getAttribute("LastWorldFrameCount")->getIntValue();

		vector<XmlNode *> networkCommandNodeList = gameNode->getChildList("NetworkCommand");
		if(SystemFlags::VERBOSE_MODE_ENABLED) printf("networkCommandNodeList.size() = " MG_SIZE_T_SPECIFIER "\n",networkCommandNodeList.size());
		for(unsigned int i = 0; i < networkCommandNodeList.size(); ++i) {
			XmlNode *node = networkCommandNodeList[i];
			int worldFrameCount = node->getAttribute("worldFrameCount")->getIntValue();
			NetworkCommand command;
			command.loadGame(node);
			newGame->commander.addToReplayCommandList(command,worldFrameCount);
		}

		programPtr->setState(newGame);
		return;
	}

	XmlTree	xmlTree(XML_RAPIDXML_ENGINE);

	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Before load of XML\n");
	std::map<string,string> mapExtraTagReplacementValues;
	xmlTree.load(name, Properties::getTagReplacementValues(&mapExtraTagReplacementValues),true);
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("After load of XML\n");

	const XmlNode *rootNode= xmlTree.getRootNode();
	if(rootNode->hasChild("megaglest-saved-game") == true) {
		rootNode = rootNode->getChild("megaglest-saved-game");
	}

	//const XmlNode *versionNode= rootNode->getChild("megaglest-saved-game");
	const XmlNode *versionNode= rootNode;

	Lang &lang= Lang::getInstance();
	string gameVer = versionNode->getAttribute("version")->getValue();
	// this is the version check for loading normal save games from menu_state_load_game
	if (gameVer != glestVersionString
			&& (compareMajorMinorVersion(gameVer, lastCompatibleSaveGameVersionString) < 0
					|| compareMajorMinorVersion(glestVersionString, gameVer) < 0)) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,lang.getString("SavedGameBadVersion").c_str(),gameVer.c_str(),glestVersionString.c_str());
		throw megaglest_runtime_error(szBuf,true);
	}

	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Found saved game version that matches your application version: [%s] --> [%s]\n",gameVer.c_str(),glestVersionString.c_str());

	XmlNode *gameNode = rootNode->getChild("Game");
	GameSettings newGameSettings;
	if(joinGameSettings != NULL) {
		newGameSettings = *joinGameSettings;

		XmlNode *worldNode = gameNode->getChild("World");
		XmlNode *guiNode = gameNode->getChild("Gui");
		XmlNode *selectionNode = guiNode->getChild("Selection");
		XmlNode *statsNode = worldNode->getChild("Stats");
		XmlNode *minimapNode = worldNode->getChild("Minimap");

		if(gameVer != glestVersionString && checkVersionComptability(gameVer, glestVersionString) == false){
			char szBuf[8096]="";
			snprintf(szBuf,8096,lang.getString("SavedGameBadVersion").c_str(),gameVer.c_str(),glestVersionString.c_str());
			throw megaglest_runtime_error(szBuf,true);
		}
		// This is explored fog of war for the host player, clear it
		minimapNode->clearChild("fowPixmap1");

		NetworkManager &networkManager= NetworkManager::getInstance();
		//NetworkRole role = networkManager.getNetworkRole();
		ClientInterface *clientInterface = dynamic_cast<ClientInterface *>(networkManager.getClientInterface());

		for(int i= 0; i<newGameSettings.getFactionCount(); ++i) {
			//replace by network
			if(newGameSettings.getFactionControl(i)==ctHuman) {
				newGameSettings.setFactionControl(i, ctNetwork);
			}

			//set the faction index
			if(newGameSettings.getStartLocationIndex(i) == clientInterface->getPlayerIndex()) {
				newGameSettings.setThisFactionIndex(i);
				newGameSettings.setFactionControl(i, ctNetwork);

				worldNode->getAttribute("thisFactionIndex")->setValue(intToStr(i));
				//worldNode->getAttribute("thisTeamIndex")->setValue(intToStr(newGameSettings.getTeam(i)));

				XmlNode *factionNode = worldNode->getChild("Faction",i);
				factionNode->getAttribute("thisFaction")->setValue(intToStr(i));
				factionNode->getAttribute("control")->setValue(intToStr(ctNetwork));

				selectionNode->getAttribute("factionIndex")->setValue(intToStr(i));
				//selectionNode->getAttribute("teamIndex")->setValue(intToStr(newGameSettings.getTeam(i)));

				statsNode->getAttribute("thisFactionIndex")->setValue(intToStr(i));
			}
			else {
				XmlNode *factionNode = worldNode->getChild("Faction",i);
				factionNode->getAttribute("thisFaction")->setValue(intToStr(0));
			}
		}
	}
	else {
		newGameSettings.loadGame(gameNode);
		//printf("Loading scenario [%s]\n",newGameSettings.getScenarioDir().c_str());
		if(newGameSettings.getScenarioDir() != "" && fileExists(newGameSettings.getScenarioDir()) == false) {
			newGameSettings.setScenarioDir(Scenario::getScenarioPath(Config::getInstance().getPathListForType(ptScenarios),newGameSettings.getScenario()));

			//printf("Loading scenario #2 [%s]\n",newGameSettings.getScenarioDir().c_str());
		}
	}

	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Game settings loaded\n");

	if(joinGameSettings == NULL) {
		NetworkManager &networkManager= NetworkManager::getInstance();
		networkManager.end();
		networkManager.init(nrServer,true);
	}

	Game *newGame = new Game(programPtr, &newGameSettings, isMasterserverMode);

	newGame->loadGameNode = gameNode;
	newGame->inJoinGameLoading = (joinGameSettings != NULL);

//	newGame->mouse2d = gameNode->getAttribute("mouse2d")->getIntValue();
//    int mouseX;
//	newGame->mouseX = gameNode->getAttribute("mouseX")->getIntValue();
//    int mouseY; //coords win32Api
//	newGame->mouseY = gameNode->getAttribute("mouseY")->getIntValue();
//    Vec2i mouseCellPos;
//	newGame->mouseCellPos = Vec2i::strToVec2(gameNode->getAttribute("mouseCellPos")->getValue());
//	int updateFps, lastUpdateFps, avgUpdateFps;
//	int totalRenderFps, renderFps, lastRenderFps, avgRenderFps,currentAvgRenderFpsTotal;
	//Uint64 tickCount;
	newGame->tickCount = gameNode->getAttribute("tickCount")->getIntValue();

	//bool paused;
	if(newGame->inJoinGameLoading==true){
		newGame->paused = gameNode->getAttribute("paused")->getIntValue() != 0;
	}else{
		//newGame->paused = gameNode->getAttribute("paused")->getIntValue() != 0;
		newGame->paused = true;
	}
	if(newGame->paused) newGame->console.addLine(lang.getString("GamePaused"));
	//bool gameOver;
	newGame->gameOver = gameNode->getAttribute("gameOver")->getIntValue() != 0;
	//bool renderNetworkStatus;
	//bool showFullConsole;
	//bool mouseMoved;
	//float scrollSpeed;
//	newGame->scrollSpeed = gameNode->getAttribute("scrollSpeed")->getFloatValue();
	//bool camLeftButtonDown;
	//bool camRightButtonDown;
	//bool camUpButtonDown;
	//bool camDownButtonDown;

	//Speed speed;
	//gameNode->addAttribute("speed",intToStr(speed), mapTagReplacements);

	//GraphicMessageBox mainMessageBox;
	//GraphicMessageBox errorMessageBox;

	//misc ptr
	//ParticleSystem *weatherParticleSystem;
//	if(weatherParticleSystem != NULL) {
//		weatherParticleSystem->saveGame(gameNode);
//	}
	//GameSettings gameSettings;
//	gameSettings.saveGame(gameNode);
	//Vec2i lastMousePos;
//	gameNode->addAttribute("lastMousePos",lastMousePos.getString(), mapTagReplacements);
	//time_t lastRenderLog2d;
//	gameNode->addAttribute("lastRenderLog2d",intToStr(lastRenderLog2d), mapTagReplacements);
	//DisplayMessageFunction originalDisplayMsgCallback;
	//bool isFirstRender;
//	gameNode->addAttribute("isFirstRender",intToStr(isFirstRender), mapTagReplacements);

	//bool quitTriggeredIndicator;
	//int original_updateFps;
//	gameNode->addAttribute("original_updateFps",intToStr(original_updateFps), mapTagReplacements);
	//int original_cameraFps;
//	gameNode->addAttribute("original_cameraFps",intToStr(original_cameraFps), mapTagReplacements);

	//bool captureAvgTestStatus;
//	gameNode->addAttribute("captureAvgTestStatus",intToStr(captureAvgTestStatus), mapTagReplacements);
	//int updateFpsAvgTest;
//	gameNode->addAttribute("updateFpsAvgTest",intToStr(updateFpsAvgTest), mapTagReplacements);
	//int renderFpsAvgTest;
//	gameNode->addAttribute("renderFpsAvgTest",intToStr(renderFpsAvgTest), mapTagReplacements);

	//int renderExtraTeamColor;
	newGame->renderExtraTeamColor = gameNode->getAttribute("renderExtraTeamColor")->getIntValue();

	//static const int renderTeamColorCircleBit=1;
	//static const int renderTeamColorPlaneBit=2;

	//bool photoModeEnabled;
	//gameNode->addAttribute("photoModeEnabled",intToStr(photoModeEnabled), mapTagReplacements);
	newGame->photoModeEnabled = gameNode->getAttribute("photoModeEnabled")->getIntValue() != 0;
	//bool visibleHUD;
	//gameNode->addAttribute("visibleHUD",intToStr(visibleHUD), mapTagReplacements);
	newGame->visibleHUD = gameNode->getAttribute("visibleHUD")->getIntValue() != 0;
	newGame->timeDisplay = gameNode->getAttribute("timeDisplay")->getIntValue() != 0;
	//bool withRainEffect;
	//gameNode->addAttribute("withRainEffect",intToStr(withRainEffect), mapTagReplacements);
	newGame->withRainEffect = gameNode->getAttribute("withRainEffect")->getIntValue() != 0;
	//Program *program;

	if(joinGameSettings == NULL) {
		if(gameNode->hasChild("unitHighlightList") == true) {
			XmlNode *unitHighlightListNode = gameNode->getChild("unitHighlightList");
			vector<XmlNode *> infoNodeList = unitHighlightListNode->getChildList("info");
			for(unsigned int i = 0; i < infoNodeList.size(); ++i) {
				XmlNode *infoNode = infoNodeList[i];

				int unitId = infoNode->getAttribute("radius")->getIntValue();
				HighlightSpecialUnitInfo info;
				info.radius = infoNode->getAttribute("radius")->getFloatValue();
				info.thickness = infoNode->getAttribute("thickness")->getFloatValue();
				info.color = Vec4f::strToVec4(infoNode->getAttribute("color")->getValue());

				newGame->unitHighlightList[unitId] = info;
			}
		}
	}

	newGame->timeDisplay = gameNode->getAttribute("timeDisplay")->getIntValue() != 0;

	if(gameNode->hasAttribute("disableSpeedChange") == true) {
		newGame->disableSpeedChange = gameNode->getAttribute("disableSpeedChange")->getIntValue() != 0;
	}

	//bool gameStarted;
	//gameNode->addAttribute("gameStarted",intToStr(gameStarted), mapTagReplacements);
//	newGame->gameStarted = gameNode->getAttribute("gameStarted")->getIntValue();

	//time_t lastMaxUnitCalcTime;
	//gameNode->addAttribute("lastMaxUnitCalcTime",intToStr(lastMaxUnitCalcTime), mapTagReplacements);

	//PopupMenu popupMenu;
	//PopupMenu popupMenuSwitchTeams;

	//std::map<int,int> switchTeamIndexMap;
	//GraphicMessageBox switchTeamConfirmMessageBox;

	//int exitGamePopupMenuIndex;
	//int joinTeamPopupMenuIndex;
	//int pauseGamePopupMenuIndex;
	//int keyboardSetupPopupMenuIndex;
	//GLuint statelist3dMenu;
	//ProgramState *currentUIState;

	//bool masterserverMode;

	//StrSound *currentAmbientSound;

	//time_t lastNetworkPlayerConnectionCheck;
	//gameNode->addAttribute("lastNetworkPlayerConnectionCheck",intToStr(lastNetworkPlayerConnectionCheck), mapTagReplacements);

	//time_t lastMasterServerGameStatsDump;
	//gameNode->addAttribute("lastMasterServerGameStatsDump",intToStr(lastMasterServerGameStatsDump), mapTagReplacements);

	if(joinGameSettings == NULL) {
		newGame->gameCamera.loadGame(gameNode);
	}

	const XmlNode *worldNode = gameNode->getChild("World");
	newGame->world.loadGame(worldNode);
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Starting Game ...\n");
	programPtr->setState(newGame);
}

}}//end namespace
