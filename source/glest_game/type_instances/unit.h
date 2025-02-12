//	This file is part of Glest (www.glest.org)
//
//	Copyright (C) 2001-2008 Martiño Figueroa
//
//	You can redistribute this code and/or modify it under
//	the terms of the GNU General Public License as published
//	by the Free Software Foundation; either version 2 of the
//	License, or (at your option) any later version
// ==============================================================

#ifndef _GLEST_GAME_UNIT_H_
#define _GLEST_GAME_UNIT_H_

#ifdef WIN32
    #include <winsock2.h>
    #include <winsock.h>
#endif

#include "model.h"
#include "upgrade_type.h"
#include "particle.h"
#include "skill_type.h"
#include "game_constants.h"
#include "platform_common.h"
#include <vector>
#include "faction.h"
#include "leak_dumper.h"

//#define LEAK_CHECK_UNITS

namespace Glest { namespace Game {

using Shared::Graphics::ParticleSystem;
using Shared::Graphics::UnitParticleSystem;
using Shared::Graphics::Vec4f;
using Shared::Graphics::Vec2f;
using Shared::Graphics::Vec3f;
using Shared::Graphics::Vec2i;
using Shared::Graphics::Model;
using Shared::PlatformCommon::Chrono;
using Shared::PlatformCommon::ValueCheckerVault;

class Map;
//class Faction;
class Unit;
class Command;
class SkillType;
class ResourceType;
class CommandType;
class SkillType;
class UnitType;
class TotalUpgrade;
class UpgradeType;
class Level;
class MorphCommandType;
class Game;
class Unit;
class GameSettings;
class World;

enum CommandResult {
	crSuccess,
	crFailRes,
	crFailReqs,
	crFailUnitCount,
	crFailUndefined,
	crSomeFailed
};

enum InterestingUnitType {
	iutIdleHarvester,
	iutBuiltBuilding,
	iutProducer,
	iutDamaged,
	iutStore
};

enum CauseOfDeathType {
	ucodNone,
	ucodAttacked,
	ucodAttackBoost,
	ucodStarvedResource,
	ucodStarvedRegeneration
};

class UnitBuildInfo {
public:
	UnitBuildInfo() {
		unit = NULL;
		//pos;
		buildUnit = NULL;
	}
	const Unit *unit;
	CardinalDir facing;
	Vec2i pos;
	const UnitType *buildUnit;
};

// =====================================================
// 	class UnitObserver
// =====================================================

class UnitObserver {
public:
	enum Event{
		eKill
	};

public:
	virtual ~UnitObserver() {}
	virtual void unitEvent(Event event, const Unit *unit)=0;

	virtual void saveGame(XmlNode *rootNode) const = 0;
};

// =====================================================
// 	class UnitReference
// =====================================================

class UnitReference {
private:
	int id;
	Faction *faction;

public:
	UnitReference();

	UnitReference & operator=(const Unit *unit);
	Unit *getUnit() const;

	int getUnitId() const			{ return id; }
	Faction *getUnitFaction() const	{ return faction; }

	void saveGame(XmlNode *rootNode);
	void loadGame(const XmlNode *rootNode,World *world);
};

class UnitPathInterface {

public:
	UnitPathInterface() {}
	virtual ~UnitPathInterface() {}

	virtual bool isBlocked() const = 0;
	virtual bool isEmpty() const = 0;
	virtual bool isStuck() const = 0;

	virtual void clear() = 0;
	virtual void clearBlockCount() = 0;
	virtual void incBlockCount() = 0;
	virtual void add(const Vec2i &path) = 0;
	//virtual Vec2i pop() = 0;
	virtual int getBlockCount() const = 0;
	virtual int getQueueCount() const = 0;

	virtual vector<Vec2i> getQueue() const = 0;

	virtual std::string toString() const = 0;

	virtual void setMap(Map *value) = 0;
	virtual Map * getMap() = 0;

	virtual void saveGame(XmlNode *rootNode) = 0;
	virtual void loadGame(const XmlNode *rootNode) = 0;

	virtual void clearCaches() = 0;

	virtual Checksum getCRC() = 0;
};

class UnitPathBasic : public UnitPathInterface {
private:
	static const int maxBlockCount;
	Map *map;

#ifdef LEAK_CHECK_UNITS
	static std::map<UnitPathBasic *,bool> mapMemoryList;
#endif

private:
	int blockCount;
	vector<Vec2i> pathQueue;

public:
	UnitPathBasic();
	virtual ~UnitPathBasic();

#ifdef LEAK_CHECK_UNITS
	static void dumpMemoryList();
#endif

	virtual bool isBlocked() const;
	virtual bool isEmpty() const;
	virtual bool isStuck() const;

	virtual void clear();
	virtual void clearBlockCount() { blockCount = 0; }
	virtual void incBlockCount();
	virtual void add(const Vec2i &path);
	Vec2i pop(bool removeFrontPos=true);
	virtual int getBlockCount() const { return blockCount; }
	virtual int getQueueCount() const { return (int)pathQueue.size(); }

	virtual vector<Vec2i> getQueue() const { return pathQueue; }

	virtual void setMap(Map *value) { map = value; }
	virtual Map * getMap() { return map; }

	virtual std::string toString() const;

	virtual void saveGame(XmlNode *rootNode);
	virtual void loadGame(const XmlNode *rootNode);
	virtual void clearCaches();

	virtual Checksum getCRC();
};

// =====================================================
// 	class UnitPath
// =====================================================
/** Holds the next cells of a Unit movement
  * @extends std::list<Shared::Math::Vec2i>
  */
class UnitPath : public list<Vec2i>, public UnitPathInterface {
private:
	static const int maxBlockCount = 10; /**< number of command updates to wait on a blocked path */

private:
	int blockCount;		/**< number of command updates this path has been blocked */
	Map *map;

public:
	UnitPath() : UnitPathInterface(), blockCount(0), map(NULL) {} /**< Construct path object */

	virtual bool isBlocked() const	{return blockCount >= maxBlockCount;} /**< is this path blocked	   */
	virtual bool isEmpty() const	{return list<Vec2i>::empty();}	/**< is path empty				  */
	virtual bool isStuck() const	{return false; }

	int  size() const		{return (int)list<Vec2i>::size();}	/**< size of path				 */
	virtual void clear()			{list<Vec2i>::clear(); blockCount = 0;} /**< clear the path		*/
	virtual void clearBlockCount() { blockCount = 0; }
	virtual void incBlockCount()	{++blockCount;}		   /**< increment block counter			   */
	virtual void push(Vec2i &pos)	{push_front(pos);}	  /**< push onto front of path			  */
	bool empty() const		{return list<Vec2i>::empty();}	/**< is path empty				  */
	virtual void add(const Vec2i &pos)	{ push_front(pos);}	  /**< push onto front of path			  */


#if 0
	// old style, to work with original PathFinder
	Vec2i peek()			{return back();}	 /**< peek at the next position			 */
	void pop()				{this->pop_back();}	/**< pop the next position off the path */
#else
	// new style
	Vec2i peek()			{return front();}	 /**< peek at the next position			 */
	//virtual Vec2i pop()		{ Vec2i p= front(); erase(begin()); return p; }	/**< pop the next position off the path */
	void pop()		{ erase(begin()); }	/**< pop the next position off the path */
#endif
	virtual int getBlockCount() const { return blockCount; }
	virtual int getQueueCount() const { return this->size(); }

	virtual vector<Vec2i> getQueue() const {
		vector<Vec2i> result;
		for(list<Vec2i>::const_iterator iter = this->begin(); iter != this->end(); ++iter) {
			result.push_back(*iter);
		}
		return result;
	}

	virtual void setMap(Map *value) { map = value; }
	virtual Map * getMap() { return map; }

	virtual std::string toString() const;

	virtual void saveGame(XmlNode *rootNode) {};
	virtual void loadGame(const XmlNode *rootNode) {};
	virtual void clearCaches() {};

	virtual Checksum getCRC() { return Checksum(); };
};

class WaypointPath : public list<Vec2i> {
public:
	WaypointPath() {}
	void push(const Vec2i &pos)	{ push_front(pos); }
	Vec2i peek() const			{return front();}
	void pop()					{erase(begin());}
	//void condense();
};


// ===============================
// 	class Unit
//
///	A game unit or building
// ===============================

class UnitAttackBoostEffect {
private:
	int unitId;
	const Unit *unitPtr;

	const Unit *source;

	void applyLoadedAttackBoostParticles(UnitParticleSystemType *upstPtr,const XmlNode* node, Unit* unit);
public:

	UnitAttackBoostEffect();
	virtual ~UnitAttackBoostEffect();

	const AttackBoost *boost;
	//const Unit *source;
	const Unit * getSource();
	void setSource(const Unit *unit);
	UnitParticleSystem *ups;
	UnitParticleSystemType *upst;

	virtual void saveGame(XmlNode *rootNode);
	virtual void loadGame(const XmlNode *rootNode, Unit *unit, World *world, bool applyToOriginator);
};

class UnitAttackBoostEffectOriginator {
public:

	UnitAttackBoostEffectOriginator();
	virtual ~UnitAttackBoostEffectOriginator();

	const SkillType *skillType;
	std::vector<int> currentAttackBoostUnits;
	UnitAttackBoostEffect *currentAppliedEffect;

	virtual void saveGame(XmlNode *rootNode);
	virtual void loadGame(const XmlNode *rootNode, Unit *unit, World *world);
};

class Unit : public BaseColorPickEntity, ValueCheckerVault, public ParticleOwner {
private:
    typedef list<Command*> Commands;
	typedef list<UnitObserver*> Observers;
	typedef vector<UnitParticleSystem*> UnitParticleSystems;

#ifdef LEAK_CHECK_UNITS
	static std::map<Unit *,bool> mapMemoryList;
#endif

	static const float ANIMATION_SPEED_MULTIPLIER;
	static const int64 PROGRESS_SPEED_MULTIPLIER;

public:
	static const int speedDivider;
	static const int maxDeadCount;
	static const int invalidId;

#ifdef LEAK_CHECK_UNITS
	static std::map<UnitPathInterface *,int> mapMemoryList2;
	static void dumpMemoryList();
#endif

private:
	const int32 id;
	int32 hp;
	int32 ep;
	int32 loadCount;
	int32 deadCount;
    //float progress;			//between 0 and 1
    int64 progress;			//between 0 and 1
	int64 lastAnimProgress;	//between 0 and 1
	int64 animProgress;		//between 0 and 1
	float highlight;
	int32 progress2;
	int32 kills;
	int32 enemyKills;
	bool morphFieldsBlocked;
	int oldTotalSight;

	UnitReference targetRef;

	Field currField;
    Field targetField;
	const Level *level;

    Vec2i pos;
	Vec2i lastPos;
    Vec2i targetPos;		//absolute target pos
	Vec3f targetVec;
	Vec2i meetingPos;

	float lastRotation;		//in degrees
	float targetRotation;
	float rotation;
	float targetRotationZ;
	float targetRotationX;
	float rotationZ;
	float rotationX;

	const UnitType *preMorph_type;
    const UnitType *type;
    const ResourceType *loadType;
    const SkillType *currSkill;
    int32 lastModelIndexForCurrSkillType;
    int32 animationRandomCycleCount;

    bool toBeUndertaken;
	bool alive;
	bool showUnitParticles;

    Faction *faction;
	ParticleSystem *fire;
	TotalUpgrade totalUpgrade;
	Map *map;

	UnitPathInterface *unitPath;
	WaypointPath waypointPath;

    Commands commands;
	Observers observers;
	vector<UnitParticleSystem*> unitParticleSystems;
	vector<UnitParticleSystemType*> queuedUnitParticleSystemTypes;

	UnitParticleSystems damageParticleSystems;
	std::map<int, UnitParticleSystem *> damageParticleSystemsInUse;

	vector<ParticleSystem*> fireParticleSystems;
	vector<UnitParticleSystem*> smokeParticleSystems;
	vector<ParticleSystem*> attackParticleSystems;

	CardinalDir modelFacing;

	std::string lastSynchDataString;
	std::string lastFile;
	int32 lastLine;
	std::string lastSource;
	int32 lastRenderFrame;
	bool visible;

	int retryCurrCommandCount;

	Vec3f screenPos;
	string currentUnitTitle;

	bool inBailOutAttempt;
	// This buffer stores a list of bad harvest cells, along with the start
	// time of when it was detected. Typically this may be due to a unit
	// constantly getting blocked from getting to the resource so this
	// list may be used to tell areas of the game to ignore those cells for a
	// period of time
	//std::vector<std::pair<Vec2i,Chrono> > badHarvestPosList;
	std::map<Vec2i,int> badHarvestPosList;
	//time_t lastBadHarvestListPurge;
	std::pair<Vec2i,int> lastHarvestResourceTarget;

	//std::pair<Vec2i,std::vector<Vec2i> > currentTargetPathTaken;

	static Game *game;

	bool ignoreCheckCommand;

	uint32 lastStuckFrame;
	Vec2i lastStuckPos;

	uint32 lastPathfindFailedFrame;
	Vec2i lastPathfindFailedPos;
	bool usePathfinderExtendedMaxNodes;
	int32 maxQueuedCommandDisplayCount;

	UnitAttackBoostEffectOriginator currentAttackBoostOriginatorEffect;

	std::vector<UnitAttackBoostEffect *> currentAttackBoostEffects;

	Mutex *mutexCommands;

	//static Mutex mutexDeletedUnits;
	//static std::map<void *,bool> deletedUnits;

	bool changedActiveCommand;
	uint32 lastChangedActiveCommandFrame;
	uint32 changedActiveCommandFrame;

	int32 lastAttackerUnitId;
	int32 lastAttackedUnitId;
	CauseOfDeathType causeOfDeath;

	uint32 pathfindFailedConsecutiveFrameCount;
	Vec2i currentPathFinderDesiredFinalPos;

	RandomGen random;
	int32 pathFindRefreshCellCount;

	FowAlphaCellsLookupItem cachedFow;
	Vec2i cachedFowPos;

	ExploredCellsLookupItem cacheExploredCells;
	std::pair<Vec2i, int> cacheExploredCellsKey;

	Vec2i lastHarvestedResourcePos;

	string networkCRCLogInfo;
	string networkCRCParticleLogInfo;
	vector<string> networkCRCDecHpList;
	vector<string> networkCRCParticleInfoList;

public:
    Unit(int id, UnitPathInterface *path, const Vec2i &pos, const UnitType *type, Faction *faction, Map *map, CardinalDir placeFacing);
    virtual ~Unit();

    //static bool isUnitDeleted(void *unit);

    static void setGame(Game *value) { game=value;}

    inline int getPathFindRefreshCellCount() const { return pathFindRefreshCellCount; }

    void setCurrentPathFinderDesiredFinalPos(const Vec2i &finalPos) { currentPathFinderDesiredFinalPos = finalPos; }
    Vec2i getCurrentPathFinderDesiredFinalPos() const { return currentPathFinderDesiredFinalPos; }

    const UnitAttackBoostEffectOriginator & getAttackBoostOriginatorEffect() const { return currentAttackBoostOriginatorEffect; }
    bool unitHasAttackBoost(const AttackBoost *boost, const Unit *source);

    inline uint32 getPathfindFailedConsecutiveFrameCount() const { return pathfindFailedConsecutiveFrameCount; }
    inline void incrementPathfindFailedConsecutiveFrameCount() { pathfindFailedConsecutiveFrameCount++; }
    inline void resetPathfindFailedConsecutiveFrameCount() { pathfindFailedConsecutiveFrameCount=0; }

    const FowAlphaCellsLookupItem & getCachedFow() const { return cachedFow; }
    FowAlphaCellsLookupItem getFogOfWarRadius(bool useCache) const;
    void calculateFogOfWarRadius(bool forceRefresh=false);

    //queries
    Command *getCurrrentCommandThreadSafe();
    void setIgnoreCheckCommand(bool value)      { ignoreCheckCommand=value;}
    inline bool getIgnoreCheckCommand() const			{return ignoreCheckCommand;}
    inline int getId() const							{return id;}
    inline Field getCurrField() const					{return currField;}
    inline int getLoadCount() const					{return loadCount;}

    //inline int getLastAnimProgress() const			{return lastAnimProgress;}
    //inline int getAnimProgress() const				{return animProgress;}
    inline float getLastAnimProgressAsFloat() const	{return static_cast<float>(lastAnimProgress) / ANIMATION_SPEED_MULTIPLIER;}
    inline float getAnimProgressAsFloat() const		{return static_cast<float>(animProgress) / ANIMATION_SPEED_MULTIPLIER;}

    inline float getHightlight() const					{return highlight;}
    inline int getProgress2() const					{return progress2;}
	inline int getFactionIndex() const {
		return faction->getIndex();
	}
	inline int getTeam() const {
		return faction->getTeam();
	}
	inline int getHp() const							{return hp;}
	inline int getEp() const							{return ep;}
	int getProductionPercent() const;
	float getProgressRatio() const;
	float getHpRatio() const;
	float getEpRatio() const;
	inline bool getToBeUndertaken() const				{return toBeUndertaken;}
	inline Vec2i getTargetPos() const					{return targetPos;}
	inline Vec3f getTargetVec() const					{return targetVec;}
	inline Field getTargetField() const				{return targetField;}
	inline Vec2i getMeetingPos() const					{return meetingPos;}
	inline Faction *getFaction() const					{return faction;}
	inline const ResourceType *getLoadType() const		{return loadType;}

	inline const UnitType *getType() const				{return type;}
	void setType(const UnitType *newType);
	inline const UnitType *getPreMorphType() const		{return preMorph_type;}

	inline const SkillType *getCurrSkill() const		{return currSkill;}
	inline const TotalUpgrade *getTotalUpgrade() const	{return &totalUpgrade;}
	inline float getRotation() const					{return rotation;}
	float getRotationX() const;
	float getRotationZ() const;
	ParticleSystem *getFire() const;
	inline int getKills() const						{return kills;}
	inline int getEnemyKills() const					{return enemyKills;}
	inline const Level *getLevel() const				{return level;}
	const Level *getNextLevel() const;
	string getFullName(bool translatedValue) const;
	inline const UnitPathInterface *getPath() const	{return unitPath;}
	inline UnitPathInterface *getPath()				{return unitPath;}
	inline WaypointPath *getWaypointPath()				{return &waypointPath;}

	inline int getLastAttackerUnitId() const { return lastAttackerUnitId; }
	inline void setLastAttackerUnitId(int unitId) { lastAttackerUnitId = unitId; }

	inline int getLastAttackedUnitId() const { return lastAttackedUnitId; }
	inline void setLastAttackedUnitId(int unitId) { lastAttackedUnitId = unitId; }

	inline CauseOfDeathType getCauseOfDeath() const { return causeOfDeath; }
	inline void setCauseOfDeath(CauseOfDeathType cause) { causeOfDeath = cause; }

    //pos
	inline Vec2i getPosNotThreadSafe() const				{return pos;}
	Vec2i getPos();
	Vec2i getPosWithCellMapSet() const;
	inline Vec2i getLastPos() const			{return lastPos;}
	Vec2i getCenteredPos() const;
    Vec2f getFloatCenteredPos() const;
	Vec2i getCellPos() const;

    //is
	inline bool isHighlighted() const			{return highlight>0.f;}
	inline bool isDead() const					{return !alive;}
	inline bool isAlive() const				{return alive;}
    bool isOperative() const;
    bool isBeingBuilt() const;
    bool isBuilt() const;
    bool isBuildCommandPending() const;
    UnitBuildInfo getBuildCommandPendingInfo() const;

    bool isAnimProgressBound() const;
    bool isPutrefacting() const {
    	return deadCount!=0;
    }
	bool isAlly(const Unit *unit) const;
	bool isDamaged() const;
	bool isInteresting(InterestingUnitType iut) const;

    //set
	//void setCurrField(Field currField);
    void setCurrSkill(const SkillType *currSkill);
    void setCurrSkill(SkillClass sc);

    void setMorphFieldsBlocked ( bool value ) {this->morphFieldsBlocked=value;}
	bool getMorphFieldsBlocked() const { return morphFieldsBlocked; }

	inline void setLastHarvestedResourcePos(Vec2i pos)		{ this->lastHarvestedResourcePos = pos; }
	inline Vec2i getLastHarvestedResourcePos() const	{ return this->lastHarvestedResourcePos; }

    inline void setLoadCount(int loadCount)					{this->loadCount= loadCount;}
    inline void setLoadType(const ResourceType *loadType)		{this->loadType= loadType;}
    inline void setProgress2(int progress2)					{this->progress2= progress2;}
	void setPos(const Vec2i &pos,bool clearPathFinder=false, bool threaded=false);
	void refreshPos(bool forceRefresh=false);
	void setTargetPos(const Vec2i &targetPos, bool threaded=false);
	void setTarget(const Unit *unit);
	//void setTargetVec(const Vec3f &targetVec);
	void setMeetingPos(const Vec2i &meetingPos);
	void setVisible(const bool visible);
	inline bool getVisible() const { return visible; }

	//render related
    //const Model *getCurrentModel();
    Model *getCurrentModelPtr();
	Vec3f getCurrMidHeightVector() const;
	Vec3f getCurrVectorForParticlesystems() const;
	Vec3f getCurrVectorAsTarget() const;
	Vec3f getCurrBurnVector() const;
	Vec3f getCurrVectorFlat() const;
	Vec3f getVectorFlat(const Vec2i &lastPosValue, const Vec2i &curPosValue) const;

    //command related
	bool anyCommand(bool validateCommandtype=false) const;
	inline Command *getCurrCommand() const {
		if(commands.empty() == false) {
			return commands.front();
		}
		return NULL;
	}
	const MorphCommandType* getCurrMorphCt() const;
	void replaceCurrCommand(Command *cmd);
	int getCountOfProducedUnitsPreExistence(const UnitType *ut) const;
	unsigned int getCommandSize() const;
	std::pair<CommandResult,string> giveCommand(Command *command, bool tryQueue = false);		//give a command
	CommandResult finishCommand();						//command finished
	CommandResult cancelCommand();						//cancel canceled

	//lifecycle
	void create(bool startingUnit= false);
	void born(const CommandType *ct);
	void kill();
	void undertake();

	//observers
	void addObserver(UnitObserver *unitObserver) ;
	//void removeObserver(UnitObserver *unitObserver);
	void notifyObservers(UnitObserver::Event event);

    //other
	void resetHighlight();
	const CommandType *computeCommandType(const Vec2i &pos, const Unit *targetUnit= NULL, const UnitType* unitType= NULL) const;
	string getDesc(bool translatedValue) const;
	string getDescExtension(bool translatedValue) const;
    bool computeEp();
    //bool computeHp();
    bool repair();
    bool decHp(int i);
    int update2();
    bool update();
	void tick();
	RandomGen* getRandom(bool threadAccessAllowed=false);

	bool applyAttackBoost(const AttackBoost *boost, const Unit *source);
	void deapplyAttackBoost(const AttackBoost *boost, const Unit *source);

	void applyUpgrade(const UpgradeType *upgradeType);
	void computeTotalUpgrade();
	void incKills(int team);
	bool morph(const MorphCommandType *mct, int frameIndex);
	std::pair<CommandResult,string> checkCommand(Command *command) const;
	void applyCommand(Command *command);

	void setModelFacing(CardinalDir value);
	inline CardinalDir getModelFacing() const { return modelFacing; }

	bool isMeetingPointSettable() const;

	inline int getLastRenderFrame() const { return lastRenderFrame; }
	inline void setLastRenderFrame(int value) { lastRenderFrame = value; }

	inline int getRetryCurrCommandCount() const { return retryCurrCommandCount; }
	inline void setRetryCurrCommandCount(int value) { retryCurrCommandCount = value; }

	inline Vec3f getScreenPos() const { return screenPos; }
	void setScreenPos(Vec3f value) { screenPos = value; }

	inline string getCurrentUnitTitle() const {return currentUnitTitle;}
	void setCurrentUnitTitle(string value) { currentUnitTitle = value;}

	void exploreCells(bool forceRefresh=false);

	inline bool getInBailOutAttempt() const { return inBailOutAttempt; }
	inline void setInBailOutAttempt(bool value) { inBailOutAttempt = value; }

	//std::vector<std::pair<Vec2i,Chrono> > getBadHarvestPosList() const { return badHarvestPosList; }
	//void setBadHarvestPosList(std::vector<std::pair<Vec2i,Chrono> > value) { badHarvestPosList = value; }
	void addBadHarvestPos(const Vec2i &value);
	//void removeBadHarvestPos(const Vec2i &value);
	inline bool isBadHarvestPos(const Vec2i &value,bool checkPeerUnits=true) const {
		bool result = false;
		if(badHarvestPosList.empty() == true) {
			return result;
		}

		std::map<Vec2i,int>::const_iterator iter = badHarvestPosList.find(value);
		if(iter != badHarvestPosList.end()) {
			result = true;
		}
		else if(checkPeerUnits == true) {
			// Check if any other units of similar type have this position tagged
			// as bad?
			for(int i = 0; i < this->getFaction()->getUnitCount(); ++i) {
				Unit *peerUnit = this->getFaction()->getUnit(i);
				if( peerUnit != NULL && peerUnit->getId() != this->getId() &&
					peerUnit->getType()->hasCommandClass(ccHarvest) == true &&
					peerUnit->getType()->getSize() <= this->getType()->getSize()) {
					if(peerUnit->isBadHarvestPos(value,false) == true) {
						result = true;
						break;
					}
				}
			}
		}

		return result;
	}
	void cleanupOldBadHarvestPos();

	void setLastHarvestResourceTarget(const Vec2i *pos);
	inline std::pair<Vec2i,int> getLastHarvestResourceTarget() const { return lastHarvestResourceTarget;}

	//std::pair<Vec2i,std::vector<Vec2i> > getCurrentTargetPathTaken() const { return currentTargetPathTaken; }
	//void addCurrentTargetPathTakenCell(const Vec2i &target,const Vec2i &cell);

	void logSynchData(string file,int line,string source="");
	void logSynchDataThreaded(string file,int line,string source="");

	std::string toString(bool crcMode=false) const;
	bool needToUpdate();
	float getProgressAsFloat() const;
	int64 getUpdateProgress();
	int64 getDiagonalFactor();
	int64 getHeightFactor(int64 speedMultiplier=PROGRESS_SPEED_MULTIPLIER);
	int64 getSpeedDenominator(int64 updateFPS);
	bool isChangedActiveCommand() const { return changedActiveCommand; }

	bool isLastStuckFrameWithinCurrentFrameTolerance(bool evalMode);
	inline uint32 getLastStuckFrame() const { return lastStuckFrame; }
	//inline void setLastStuckFrame(uint32 value) { lastStuckFrame = value; }
	void setLastStuckFrameToCurrentFrame();

	inline Vec2i getLastStuckPos() const { return lastStuckPos; }
	inline void setLastStuckPos(Vec2i pos) { lastStuckPos = pos; }

	bool isLastPathfindFailedFrameWithinCurrentFrameTolerance() const;
	inline uint32 getLastPathfindFailedFrame() const { return lastPathfindFailedFrame; }
	inline void setLastPathfindFailedFrame(uint32 value) { lastPathfindFailedFrame = value; }
	void setLastPathfindFailedFrameToCurrentFrame();

	inline Vec2i getLastPathfindFailedPos() const { return lastPathfindFailedPos; }
	inline void setLastPathfindFailedPos(Vec2i pos) { lastPathfindFailedPos = pos; }

	inline bool getUsePathfinderExtendedMaxNodes() const { return usePathfinderExtendedMaxNodes; }
	inline void setUsePathfinderExtendedMaxNodes(bool value) { usePathfinderExtendedMaxNodes = value; }

	void updateTimedParticles();
	void setMeshPosInParticleSystem(UnitParticleSystem *ups);

	virtual string getUniquePickName() const;
	void saveGame(XmlNode *rootNode);
	static Unit * loadGame(const XmlNode *rootNode,GameSettings *settings,Faction *faction, World *world);

	void clearCaches();
	bool showTranslatedTechTree() const;

	void addAttackParticleSystem(ParticleSystem *ps);

	Checksum getCRC();

	virtual void end(ParticleSystem *particleSystem);
	virtual void logParticleInfo(string info);
	void setNetworkCRCParticleLogInfo(string networkCRCParticleLogInfo) { this->networkCRCParticleLogInfo = networkCRCParticleLogInfo; }
	void clearParticleInfo();
	void addNetworkCRCDecHp(string info);
	void clearNetworkCRCDecHpList();

private:

	void cleanupAllParticlesystems();
	bool isNetworkCRCEnabled();
	string getNetworkCRCDecHpList() const;
	string getParticleInfo() const;

	float computeHeight(const Vec2i &pos) const;
	void calculateXZRotation();
	void AnimCycleStarts();
	void updateTarget();
	void clearCommands();
	void deleteQueuedCommand(Command *command);
	CommandResult undoCommand(Command *command);
	void stopDamageParticles(bool force);
	void startDamageParticles();

	uint32 getFrameCount() const;

	void checkCustomizedParticleTriggers(bool force);
	void checkCustomizedUnitParticleTriggers();
	void checkCustomizedUnitParticleListTriggers(const UnitParticleSystemTypes &unitParticleSystemTypesList,
												 bool applySkillChangeParticles);
	void queueTimedParticles(const UnitParticleSystemTypes &unitParticleSystemTypesList);

	bool checkModelStateInfoForNewHpValue();
	void checkUnitLevel();

	void morphAttackBoosts(Unit *unit);

	int64 getUpdatedProgress(int64 currentProgress, int64 updateFPS, int64 speed, int64 diagonalFactor, int64 heightFactor);

	void logSynchDataCommon(string file,int line,string source="",bool threadedMode=false);
	void updateAttackBoostProgress(const Game* game);

	void setAlive(bool value);
};

}}// end namespace

#endif
