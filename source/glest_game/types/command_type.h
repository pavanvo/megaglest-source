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

#ifndef _GLEST_GAME_COMMANDTYPE_H_
#define _GLEST_GAME_COMMANDTYPE_H_

#ifdef WIN32
    #include <winsock2.h>
    #include <winsock.h>
#endif

#include "element_type.h"
#include "resource_type.h"
#include "lang.h"
#include "skill_type.h"
#include "factory.h"
#include "xml_parser.h"
#include "sound_container.h"
#include "leak_dumper.h"

namespace Glest { namespace Game {

using Shared::Util::MultiFactory;

class UnitUpdater;
class Unit;
class UnitType;
class TechTree;
class FactionType;

enum CommandClass {
	ccStop,
	ccMove,
	ccAttack,
	ccAttackStopped,
	ccBuild,
	ccHarvest,
	ccRepair,
	ccProduce,
	ccUpgrade,
	ccMorph,
	ccSwitchTeam,
	ccHarvestEmergencyReturn,

	ccCount,
	ccNull
};

enum CommandRow {
	crCores,
	crBasics,
	crMorphs,
};

enum Clicks {
	cOne,
	cTwo
};

enum Queueability {
	qNever,
	qOnRequest,
	qOnlyLast,
	qAlways
};

class CommandHelper {// TODO put magic numbers to settings
public:
	inline static int getRowPos(CommandRow cr) { return cr * 4; } 
	static int getBasicPos(CommandClass cc);
	inline static vector<CommandClass> getBasicsCC() { return { ccAttack, ccStop, ccMove, ccAttackStopped }; }
	inline static vector<CommandClass> getCoresCC() { return { ccAttack, ccProduce, ccUpgrade, ccSwitchTeam, ccHarvest, ccRepair, ccBuild, ccAttackStopped, ccMove, ccStop }; }
	
private:
	CommandHelper(){ }
};

// =====================================================
// 	class CommandType
//
///	A complex action performed by a unit, composed by skills
// =====================================================

class CommandType: public RequirableType {
protected:
    Clicks clicks;
	int id;

	std::map<string,bool> fogOfWarSkillAttachments;
	const FogOfWarSkillType* fogOfWarSkillType;

public:
	static const int invalidId= -1;
    CommandClass commandTypeClass;

public:
	CommandType() {
	    commandTypeClass 	= ccNull;
	    clicks 				= cOne;
		id 					= -1;
		fogOfWarSkillType	= NULL;
		fogOfWarSkillAttachments.clear();
	}
    virtual void update(UnitUpdater *unitUpdater, Unit *unit, int frameIndex) const= 0;
    virtual void load(int id, const XmlNode *n, const string &dir,
    		const TechTree *tt, const FactionType *ft, const UnitType &ut,
    		std::map<string,vector<pair<string, string> > > &loadedFileList, string parentLoader);
    virtual string getDesc(const TotalUpgrade *totalUpgrade, bool translatedValue) const= 0;
	virtual string toString(bool translatedValue) const= 0;
	virtual const ProducibleType *getProduced() const	{return NULL;}
	virtual Queueability isQueuable() const						{return qOnRequest;}
	bool isQueuable(bool tryQueue) const {
		Queueability q = isQueuable();
		return (q == qAlways) || ((q == qOnRequest || q == qOnlyLast) && tryQueue);
	}
	bool isQueueAppendable() const {
		Queueability q = isQueuable();
		return (q != qNever) && (q != qOnlyLast);
	}
	virtual bool usesPathfinder() const= 0;

    //get
    CommandClass getClass() const;
	Clicks getClicks() const		{return clicks;}
	int getId() const				{return id;}

	const FogOfWarSkillType *getFogOfWarSkillType() const	{return fogOfWarSkillType;};

	bool hasFogOfWarSkillType(string name) const;
};

// ===============================
// 	class StopCommandType
// ===============================

class StopCommandType: public CommandType {
private:
    const StopSkillType* stopSkillType;

public:
    StopCommandType();
	virtual void update(UnitUpdater *unitUpdater, Unit *unit, int frameIndex) const;
    virtual void load(int id, const XmlNode *n, const string &dir, const TechTree *tt,
    		const FactionType *ft, const UnitType &ut, std::map<string,vector<pair<string, string> > > &loadedFileList,
    		string parentLoader);
    virtual string getDesc(const TotalUpgrade *totalUpgrade, bool translatedValue) const;
	virtual string toString(bool translatedValue) const;
	virtual Queueability isQueuable() const						{return qNever;}
    //get
	const StopSkillType *getStopSkillType() const	{return stopSkillType;};

	virtual bool usesPathfinder() const { return false; }
};


// ===============================
// 	class MoveCommandType
// ===============================

class MoveCommandType: public CommandType {
private:
    const MoveSkillType *moveSkillType;

public:
    MoveCommandType();
	virtual void update(UnitUpdater *unitUpdater, Unit *unit, int frameIndex) const;
    virtual void load(int id, const XmlNode *n, const string &dir,
    		const TechTree *tt, const FactionType *ft, const UnitType &ut,
    		std::map<string,vector<pair<string, string> > > &loadedFileList, string parentLoader);
    virtual string getDesc(const TotalUpgrade *totalUpgrade, bool translatedValue) const;
	virtual string toString(bool translatedValue) const;

    //get
	const MoveSkillType *getMoveSkillType() const	{return moveSkillType;};

	virtual bool usesPathfinder() const { return true; }
};


// ===============================
// 	class AttackCommandType
// ===============================

class AttackCommandType: public CommandType {
private:
    const MoveSkillType* moveSkillType;
    const AttackSkillType* attackSkillType;

public:
    AttackCommandType();
	virtual void update(UnitUpdater *unitUpdater, Unit *unit, int frameIndex) const;
    virtual void load(int id, const XmlNode *n, const string &dir,
    		const TechTree *tt, const FactionType *ft, const UnitType &ut,
    		std::map<string,vector<pair<string, string> > > &loadedFileList, string parentLoader);
    virtual string getDesc(const TotalUpgrade *totalUpgrade, bool translatedValue) const;
	virtual string toString(bool translatedValue) const;


    //get
	const MoveSkillType * getMoveSkillType() const			{return moveSkillType;}
	const AttackSkillType * getAttackSkillType() const		{return attackSkillType;}

	virtual bool usesPathfinder() const { return true; }
};

// =======================================
// 	class AttackStoppedCommandType
// =======================================

class AttackStoppedCommandType: public CommandType {
private:
    const StopSkillType* stopSkillType;
    const AttackSkillType* attackSkillType;

public:
    AttackStoppedCommandType();
	virtual void update(UnitUpdater *unitUpdater, Unit *unit, int frameIndex) const;
    virtual void load(int id, const XmlNode *n, const string &dir,
    		const TechTree *tt, const FactionType *ft, const UnitType &ut,
    		std::map<string,vector<pair<string, string> > > &loadedFileList, string parentLoader);
    virtual string getDesc(const TotalUpgrade *totalUpgrade, bool translatedValue) const;
	virtual Queueability isQueuable() const	{return qOnlyLast;}
	virtual string toString(bool translatedValue) const;

    //get
	const StopSkillType * getStopSkillType() const		{return stopSkillType;}
	const AttackSkillType * getAttackSkillType() const	{return attackSkillType;}

	virtual bool usesPathfinder() const { return false; }
};


// ===============================
// 	class BuildCommandType
// ===============================

class BuildCommandType: public CommandType {
private:
    const MoveSkillType* moveSkillType;
    const BuildSkillType* buildSkillType;
	vector<const UnitType*> buildings;
    SoundContainer startSounds;
    SoundContainer builtSounds;

public:
    BuildCommandType();
    ~BuildCommandType();
	virtual void update(UnitUpdater *unitUpdater, Unit *unit, int frameIndex) const;
    virtual void load(int id, const XmlNode *n, const string &dir,
    		const TechTree *tt, const FactionType *ft, const UnitType &ut,
    		std::map<string,vector<pair<string, string> > > &loadedFileList, string parentLoader);
    virtual string getDesc(const TotalUpgrade *totalUpgrade, bool translatedValue) const;
	virtual string toString(bool translatedValue) const;

    //get
	const MoveSkillType *getMoveSkillType() const	{return moveSkillType;}
	const BuildSkillType *getBuildSkillType() const	{return buildSkillType;}
	int getBuildingCount() const					{return (int)buildings.size();}
	const UnitType * getBuilding(int i) const		{return buildings[i];}
	StaticSound *getStartSound() const				{return startSounds.getRandSound();}
	StaticSound *getBuiltSound() const				{return builtSounds.getRandSound();}

	virtual bool usesPathfinder() const { return true; }
};


// ===============================
// 	class HarvestCommandType
// ===============================

class HarvestCommandType: public CommandType {
private:
    const MoveSkillType *moveSkillType;
    const MoveSkillType *moveLoadedSkillType;
    const HarvestSkillType *harvestSkillType;
    const StopSkillType *stopLoadedSkillType;
	vector<const ResourceType*> harvestedResources;
	int maxLoad;
    int hitsPerUnit;

public:
    HarvestCommandType();
	virtual void update(UnitUpdater *unitUpdater, Unit *unit, int frameIndex) const;
    virtual void load(int id, const XmlNode *n, const string &dir,
    		const TechTree *tt, const FactionType *ft, const UnitType &ut,
    		std::map<string,vector<pair<string, string> > > &loadedFileList, string parentLoader);
    virtual string getDesc(const TotalUpgrade *totalUpgrade, bool translatedValue) const;
	virtual string toString(bool translatedValue) const;

    //get
	const MoveSkillType *getMoveSkillType() const			{return moveSkillType;}
	const MoveSkillType *getMoveLoadedSkillType() const		{return moveLoadedSkillType;}
	const HarvestSkillType *getHarvestSkillType() const		{return harvestSkillType;}
	const StopSkillType *getStopLoadedSkillType() const		{return stopLoadedSkillType;}
	int getMaxLoad() const									{return maxLoad;}
	int getHitsPerUnit() const								{return hitsPerUnit;}
	int getHarvestedResourceCount() const					{return (int)harvestedResources.size();}
	const ResourceType* getHarvestedResource(int i) const	{return harvestedResources[i];}
	bool canHarvest(const ResourceType *resourceType) const;

	virtual bool usesPathfinder() const { return true; }
};

// ===============================
// 	class HarvestEmergencyReturnCommandType
// ===============================

class HarvestEmergencyReturnCommandType: public CommandType {
private:

public:
    HarvestEmergencyReturnCommandType();
	virtual void update(UnitUpdater *unitUpdater, Unit *unit, int frameIndex) const;
    virtual void load(int id, const XmlNode *n, const string &dir,
    		const TechTree *tt, const FactionType *ft, const UnitType &ut,
    		std::map<string,vector<pair<string, string> > > &loadedFileList, string parentLoader);
    virtual string getDesc(const TotalUpgrade *totalUpgrade, bool translatedValue) const;
	virtual string toString(bool translatedValue) const;

    //get
	virtual bool usesPathfinder() const { return true; }
};


// ===============================
// 	class RepairCommandType
// ===============================

class RepairCommandType: public CommandType {
private:
    const MoveSkillType* moveSkillType;
    const RepairSkillType* repairSkillType;
    vector<const UnitType*>  repairableUnits;

public:
    RepairCommandType();
    ~RepairCommandType();
	virtual void update(UnitUpdater *unitUpdater, Unit *unit, int frameIndex) const;
    virtual void load(int id, const XmlNode *n, const string &dir,
    		const TechTree *tt, const FactionType *ft, const UnitType &ut,
    		std::map<string,vector<pair<string, string> > > &loadedFileList, string parentLoader);
    virtual string getDesc(const TotalUpgrade *totalUpgrade, bool translatedValue) const;
	virtual string toString(bool translatedValue) const;

    //get
	const MoveSkillType *getMoveSkillType() const			{return moveSkillType;};
	const RepairSkillType *getRepairSkillType() const		{return repairSkillType;};
    bool isRepairableUnitType(const UnitType *unitType) const;

	int getRepairCount() const					{return (int)repairableUnits.size();}
	const UnitType * getRepair(int i) const		{return repairableUnits[i];}

	virtual bool usesPathfinder() const { return true; }
};


// ===============================
// 	class ProduceCommandType
// ===============================

class ProduceCommandType: public CommandType {
private:
    const ProduceSkillType* produceSkillType;
	const UnitType *producedUnit;

public:
    ProduceCommandType();
	virtual void update(UnitUpdater *unitUpdater, Unit *unit, int frameIndex) const;
    virtual void load(int id, const XmlNode *n, const string &dir,
    		const TechTree *tt, const FactionType *ft, const UnitType &ut,
    		std::map<string,vector<pair<string, string> > > &loadedFileList, string parentLoader);
    virtual string getDesc(const TotalUpgrade *totalUpgrade, bool translatedValue) const;
    virtual string getReqDesc(bool translatedValue) const;
	virtual string toString(bool translatedValue) const;
	virtual const ProducibleType *getProduced() const;
	virtual Queueability isQueuable() const						{return qAlways;}

    //get
	const ProduceSkillType *getProduceSkillType() const	{return produceSkillType;}
	const UnitType *getProducedUnit() const				{return producedUnit;}

	virtual bool usesPathfinder() const { return false; }
};


// ===============================
// 	class UpgradeCommandType
// ===============================

class UpgradeCommandType: public CommandType {
private:
    const UpgradeSkillType* upgradeSkillType;
    const UpgradeType* producedUpgrade;

public:
    UpgradeCommandType();
	virtual void update(UnitUpdater *unitUpdater, Unit *unit, int frameIndex) const;
    virtual void load(int id, const XmlNode *n, const string &dir,
    		const TechTree *tt, const FactionType *ft, const UnitType &ut,
    		std::map<string,vector<pair<string, string> > > &loadedFileList, string parentLoader);
    virtual string getDesc(const TotalUpgrade *totalUpgrade, bool translatedValue) const;
	virtual string toString(bool translatedValue) const;
	virtual string getReqDesc(bool translatedValue) const;
	virtual const ProducibleType *getProduced() const;
	virtual Queueability isQueuable() const						{return qAlways;}

    //get
	const UpgradeSkillType *getUpgradeSkillType() const		{return upgradeSkillType;}
	const UpgradeType *getProducedUpgrade() const			{return producedUpgrade;}

	virtual bool usesPathfinder() const { return false; }
};

// ===============================
// 	class MorphCommandType
// ===============================

class MorphCommandType: public CommandType {
private:
    const MorphSkillType* morphSkillType;
    const UnitType* morphUnit;
	int discount;
	bool ignoreResourceRequirements;

public:
    MorphCommandType();
	virtual void update(UnitUpdater *unitUpdater, Unit *unit, int frameIndex) const;
    virtual void load(int id, const XmlNode *n, const string &dir,
    		const TechTree *tt, const FactionType *ft, const UnitType &ut,
    		std::map<string,vector<pair<string, string> > > &loadedFileList, string parentLoader);
    virtual string getDesc(const TotalUpgrade *totalUpgrade, bool translatedValue) const;
	virtual string toString(bool translatedValue) const;
	virtual string getReqDesc(bool translatedValue) const;
	virtual const ProducibleType *getProduced() const;

    //get
	const MorphSkillType *getMorphSkillType() const		{return morphSkillType;}
	const UnitType *getMorphUnit() const				{return morphUnit;}
	int getDiscount() const								{return discount;}
	bool getIgnoreResourceRequirements() const			{return ignoreResourceRequirements;}

	virtual bool usesPathfinder() const { return false; }
};

// ===============================
// 	class SwitchTeamCommandType
// ===============================

class SwitchTeamCommandType: public CommandType {
private:


public:
	SwitchTeamCommandType();
	virtual void update(UnitUpdater *unitUpdater, Unit *unit, int frameIndex) const;
    virtual void load(int id, const XmlNode *n, const string &dir,
    		const TechTree *tt, const FactionType *ft, const UnitType &ut,
    		std::map<string,vector<pair<string, string> > > &loadedFileList, string parentLoader);
    virtual string getDesc(const TotalUpgrade *totalUpgrade, bool translatedValue) const;
	virtual string toString(bool translatedValue) const;

	virtual bool usesPathfinder() const { return false; }
};

// ===============================
// 	class CommandFactory
// ===============================

class CommandTypeFactory: public MultiFactory<CommandType>{
private:
	CommandTypeFactory();

public:
	static CommandTypeFactory &getInstance();
};

}}//end namespace

#endif
