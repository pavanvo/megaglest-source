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

#ifdef WIN32
    #include <winsock2.h>
    #include <winsock.h>
#endif

#include "unit_type.h"
#include <cassert>
#include "util.h"
#include "upgrade_type.h"
#include "resource_type.h"
#include "sound.h"
#include "logger.h"
#include "xml_parser.h"
#include "tech_tree.h"
#include "resource.h"
#include "renderer.h"
#include "game_util.h"
#include "unit_particle_type.h"
#include "faction.h"
#include "common_scoped_ptr.h"
#include "leak_dumper.h"

using namespace Shared::Xml;
using namespace Shared::Graphics;
using namespace Shared::Util;

namespace Glest{ namespace Game{

auto_ptr<CommandType> UnitType::ctHarvestEmergencyReturnCommandType(new HarvestEmergencyReturnCommandType());
// ===============================
// 	class Level
// ===============================

void Level::init(string name, int kills){
	this->name= name;
	this->kills= kills;
}

string Level::getName(bool translatedValue) const	{
	if(translatedValue == false) return name;

	Lang &lang = Lang::getInstance();
	return lang.getTechTreeString("LevelName_" + name,name.c_str());
}

void Level::saveGame(XmlNode *rootNode) const {
	std::map<string,string> mapTagReplacements;
	XmlNode *levelNode = rootNode->addChild("Level");

	levelNode->addAttribute("name",name, mapTagReplacements);
	levelNode->addAttribute("kills",intToStr(kills), mapTagReplacements);
}

const Level * Level::loadGame(const XmlNode *rootNode,const UnitType *ut) {
	const Level *result = NULL;
	if(rootNode->hasChild("Level") == true) {
		const XmlNode *levelNode = rootNode->getChild("Level");

		result = ut->getLevel(levelNode->getAttribute("name")->getValue());
	}

	return result;
}

// =====================================================
// 	class UnitType
// =====================================================

// ===================== PUBLIC ========================

const char *UnitType::propertyNames[]= {"burnable", "rotated_climb"};

// ==================== creation and loading ====================

UnitType::UnitType() : ProducibleType() {

	countInVictoryConditions = ucvcNotSet;
	meetingPointImage = NULL;
    lightColor= Vec3f(0.f);
    light= false;
	healthbarheight= -100.0f;
	healthbarthickness=-1.0f;
	healthbarVisible=hbvUndefined;
    multiSelect= false;
    uniformSelect= false;
    commandable= true;
	armorType= NULL;
	rotatedBuildPos=0;

	field = fLand;
	id = 0;
	meetingPoint = false;
	rotationAllowed = false;

	countUnitDeathInStats=false;
	countUnitProductionInStats=false;
	countUnitKillInStats=false;
	countKillForUnitUpgrade=false;


    for(int i=0; i<ccCount; ++i){
        firstCommandTypeOfClass[i]= NULL;
    }

    for(int i=0; i<scCount; ++i){
    	firstSkillTypeOfClass[i] = NULL;
    }

	for(int i=0; i<pCount; ++i){
		properties[i]= false;
	}

	for(int i=0; i<fieldCount; ++i){
		fields[i]= false;
	}

	cellMap= NULL;
	allowEmptyCellMap=false;
	hpRegeneration= 0;
	epRegeneration= 0;
	maxUnitCount= 0;
	maxHp=0;
	startHpValue=0;
	startHpPercentage=1.0;
	startHpType=stValue;
	maxEp=0;
	startEpValue=0;
	startEpPercentage=0;
	startEpType=stValue;
	armor=0;
	sight=0;
	size=0;
	aiBuildSize=-1;
	renderSize=0;
	height=0;
	burnHeight=0;
	targetHeight=0;

	addItemToVault(&(this->maxHp),this->maxHp);
	addItemToVault(&(this->hpRegeneration),this->hpRegeneration);
	addItemToVault(&(this->maxEp),this->maxEp);
	addItemToVault(&(this->epRegeneration),this->epRegeneration);
	addItemToVault(&(this->maxUnitCount),this->maxUnitCount);
	addItemToVault(&(this->armor),this->armor);
	addItemToVault(&(this->sight),this->sight);
	addItemToVault(&(this->size),this->size);
	addItemToVault(&(this->height),this->height);
}

UnitType::~UnitType(){
	deleteValues(commandTypes.begin(), commandTypes.end());
	commandTypes.clear();
	deleteValues(skillTypes.begin(), skillTypes.end());
	skillTypes.clear();
	deleteValues(selectionSounds.getSounds().begin(), selectionSounds.getSounds().end());
	selectionSounds.clearSounds();
	deleteValues(commandSounds.getSounds().begin(), commandSounds.getSounds().end());
	commandSounds.clearSounds();
	delete [] cellMap;
	cellMap=NULL;
	//remove damageParticleSystemTypes
	while(!damageParticleSystemTypes.empty()){
		delete damageParticleSystemTypes.back();
		damageParticleSystemTypes.pop_back();
	}
}

void UnitType::preLoad(const string &dir) {
	name= lastDir(dir);
}

void UnitType::loaddd(int id,const string &dir, const TechTree *techTree,
		const string &techTreePath, const FactionType *factionType,
		Checksum* checksum, Checksum* techtreeChecksum,
		std::map<string,vector<pair<string, string> > > &loadedFileList,
		bool validationMode) {

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);

	string currentPath = dir;
	endPathWithSlash(currentPath);
	string path = currentPath + name + ".xml";
	string sourceXMLFile = path;

	this->id= id;

	try {
		//Lang &lang= Lang::getInstance();

		char szBuf[8096]="";
		snprintf(szBuf,8096,Lang::getInstance().getString("LogScreenGameLoadingUnitType","",true).c_str(),formatString(this->getName(true)).c_str());
		Logger::getInstance().add(szBuf, true);

		//file load
		checksum->addFile(path);
		techtreeChecksum->addFile(path);

		XmlTree xmlTree;
		std::map<string,string> mapExtraTagReplacementValues;
		mapExtraTagReplacementValues["$COMMONDATAPATH"] = techTreePath + "/commondata/";
		xmlTree.load(path, Properties::getTagReplacementValues(&mapExtraTagReplacementValues));
		loadedFileList[path].push_back(make_pair(dir,dir));

		const XmlNode *unitNode= xmlTree.getRootNode();

		const XmlNode *parametersNode= unitNode->getChild("parameters");

		if(parametersNode->hasChild("count-in-victory-conditions") == true) {
			bool countUnit = parametersNode->getChild("count-in-victory-conditions")->getAttribute("value")->getBoolValue();
			if(countUnit == true) {
				countInVictoryConditions = ucvcTrue;
			}
			else {
				countInVictoryConditions = ucvcFalse;
			}
		}

		//size
		//checkItemInVault(&(this->size),this->size);
		size= parametersNode->getChild("size")->getAttribute("value")->getIntValue();
		addItemToVault(&(this->size),this->size);
		renderSize=size;
		if(parametersNode->hasChild("render-size")){
			renderSize=parametersNode->getChild("render-size")->getAttribute("value")->getIntValue();
		}
		aiBuildSize=size;
		if(parametersNode->hasChild("ai-build-size")){
			aiBuildSize= parametersNode->getChild("ai-build-size")->getAttribute("value")->getIntValue();
		}

		//height
		//checkItemInVault(&(this->height),this->height);
		height= parametersNode->getChild("height")->getAttribute("value")->getIntValue();
		addItemToVault(&(this->height),this->height);

		//targetHeight
		if(parametersNode->hasChild("target-height")){
			targetHeight= parametersNode->getChild("target-height")->getAttribute("value")->getIntValue();
			addItemToVault(&(this->targetHeight),this->targetHeight);
		} else {
			targetHeight=height;
		}
		//burnHeight
		if(parametersNode->hasChild("target-height")){
			burnHeight= parametersNode->getChild("burn-height")->getAttribute("value")->getIntValue();
			addItemToVault(&(this->burnHeight),this->burnHeight);
		} else {
			burnHeight=height;
		}


		//maxHp
		//checkItemInVault(&(this->maxHp),this->maxHp);
		maxHp = parametersNode->getChild("max-hp")->getAttribute("value")->getIntValue();
		addItemToVault(&(this->maxHp),this->maxHp);

		//hpRegeneration
		//checkItemInVault(&(this->hpRegeneration),this->hpRegeneration);
		hpRegeneration= parametersNode->getChild("max-hp")->getAttribute("regeneration")->getIntValue();
		addItemToVault(&(this->hpRegeneration),this->hpRegeneration);

		//maxEp
		//checkItemInVault(&(this->maxEp),this->maxEp);
		maxEp= parametersNode->getChild("max-ep")->getAttribute("value")->getIntValue();
		addItemToVault(&(this->maxEp),this->maxEp);

		if(maxEp != 0) {
			//epRegeneration
			//checkItemInVault(&(this->epRegeneration),this->epRegeneration);
			epRegeneration= parametersNode->getChild("max-ep")->getAttribute("regeneration")->getIntValue();
		}
		addItemToVault(&(this->epRegeneration),this->epRegeneration);

		// Check that we don't use both start-value and start-percentage, as they are mutually
		// exclusive
		if(parametersNode->getChild("max-hp")->hasAttribute("start-value") &&
				parametersNode->getChild("max-hp")->hasAttribute("start-percentage")) {
					throw megaglest_runtime_error("Unit " + name +
							" has both start-value and start-percentage for HP", true);
		}

		//startHpValue -- the *absolute* value to use for starting HP
		if(parametersNode->getChild("max-hp")->hasAttribute("start-value")) {
			//checkItemInVault(&(this->startEp),this->startEp);
			startHpValue= parametersNode->getChild("max-hp")->getAttribute("start-value")->getIntValue();
			startHpType= stValue;
		}
		addItemToVault(&(this->startHpValue),this->startHpValue);

		//startHpPercentage -- the *relative* value to use for starting HP
		if(parametersNode->getChild("max-hp")->hasAttribute("start-percentage")) {
			startHpPercentage= parametersNode->getChild("max-hp")->getAttribute("start-percentage")->getIntValue();
			startHpType= stPercentage;
		}

		// No start value is set; use 100% of the current Max
		if(!parametersNode->getChild("max-hp")->hasAttribute("start-value") &&
				!parametersNode->getChild("max-hp")->hasAttribute("start-percentage")) {
			startHpPercentage=100;
			startHpType= stPercentage;
		}
		addItemToVault(&(this->startHpPercentage),this->startHpPercentage);

		// Check that we don't use both start-value and start-percentage, as they are mutually
		// exclusive
		if(parametersNode->getChild("max-ep")->hasAttribute("start-value") &&
				parametersNode->getChild("max-ep")->hasAttribute("start-percentage")) {
					throw megaglest_runtime_error("Unit " + name +
							" has both start-value and start-percentage for EP", true);
		}

		//startEpValue -- the *absolute* value to use for starting EP
		if(parametersNode->getChild("max-ep")->hasAttribute("start-value")) {
			//checkItemInVault(&(this->startEp),this->startEp);
			startEpValue= parametersNode->getChild("max-ep")->getAttribute("start-value")->getIntValue();
			startEpType= stValue;
		}
		addItemToVault(&(this->startEpValue),this->startEpValue);

		//startEpPercentage -- the *relative* value to use for starting EP
		if(parametersNode->getChild("max-ep")->hasAttribute("start-percentage")) {
			startEpPercentage= parametersNode->getChild("max-ep")->getAttribute("start-percentage")->getIntValue();
			startEpType= stPercentage;
		}
		addItemToVault(&(this->startEpPercentage),this->startEpPercentage);

		//maxUnitCount
		if(parametersNode->hasChild("max-unit-count")) {
			//checkItemInVault(&(this->maxUnitCount),this->maxUnitCount);
			maxUnitCount= parametersNode->getChild("max-unit-count")->getAttribute("value")->getIntValue();
		}
		addItemToVault(&(this->maxUnitCount),this->maxUnitCount);

		//armor
		//checkItemInVault(&(this->armor),this->armor);
		armor= parametersNode->getChild("armor")->getAttribute("value")->getIntValue();
		addItemToVault(&(this->armor),this->armor);

		//armor type string
		string armorTypeName= parametersNode->getChild("armor-type")->getAttribute("value")->getRestrictedValue();
		armorType= techTree->getArmorType(armorTypeName);

		//sight
		//checkItemInVault(&(this->sight),this->sight);
		sight= parametersNode->getChild("sight")->getAttribute("value")->getIntValue();
		addItemToVault(&(this->sight),this->sight);

		//prod time
		productionTime= parametersNode->getChild("time")->getAttribute("value")->getIntValue();

		//multi selection
		multiSelect= parametersNode->getChild("multi-selection")->getAttribute("value")->getBoolValue();

		//uniform selection
		if(parametersNode->hasChild("uniform-selection")) {
			uniformSelect= parametersNode->getChild("uniform-selection")->getAttribute("value")->getBoolValue();
		}

		//commandable
		if(parametersNode->hasChild("commandable")){
			commandable= parametersNode->getChild("commandable")->getAttribute("value")->getBoolValue();
		}
		//cellmap
		allowEmptyCellMap = false;
		const XmlNode *cellMapNode= parametersNode->getChild("cellmap");
		bool hasCellMap= cellMapNode->getAttribute("value")->getBoolValue();
		if(hasCellMap == true) {
			if(cellMapNode->getAttribute("allowEmpty",false) != NULL) {
				allowEmptyCellMap = cellMapNode->getAttribute("allowEmpty")->getBoolValue();
			}

			cellMap= new bool[size*size];
			for(int i=0; i<size; ++i){
				const XmlNode *rowNode= cellMapNode->getChild("row", i);
				string row= rowNode->getAttribute("value")->getRestrictedValue();
				if((int)row.size() != size){
					throw megaglest_runtime_error("Cellmap row has not the same length as unit size",true);
				}
				for(int j=0; j < (int)row.size(); ++j){
					cellMap[i*size+j]= row[j]=='0'? false: true;
				}
			}
		}

		//levels
		const XmlNode *levelsNode= parametersNode->getChild("levels");
		levels.resize(levelsNode->getChildCount());
		for(int i=0; i < (int)levels.size(); ++i){
			const XmlNode *levelNode= levelsNode->getChild("level", i);

			levels[i].init(
				levelNode->getAttribute("name")->getRestrictedValue(),
				levelNode->getAttribute("kills")->getIntValue());
		}

		//fields
		const XmlNode *fieldsNode= parametersNode->getChild("fields");
		for(int i=0; i < (int)fieldsNode->getChildCount(); ++i){
			const XmlNode *fieldNode= fieldsNode->getChild("field", i);
			string fieldName= fieldNode->getAttribute("value")->getRestrictedValue();
			if(fieldName=="land"){
				fields[fLand]= true;
			}
			else if(fieldName=="air"){
				fields[fAir]= true;
			}
			else{
				throw megaglest_runtime_error("Not a valid field: "+fieldName+": "+ path, true);
			}
		}

		if (fields[fLand]) {
			field = fLand;
		}
		else if (fields[fAir]) {
			field = fAir;
		}
		else {
			throw megaglest_runtime_error("Unit has no field: " + path, true);
		}

		//properties
		const XmlNode *propertiesNode= parametersNode->getChild("properties");
		for(int i = 0; i < (int)propertiesNode->getChildCount(); ++i) {
			const XmlNode *propertyNode= propertiesNode->getChild("property", i);
			string propertyName= propertyNode->getAttribute("value")->getRestrictedValue();
			bool found= false;
			for(int i = 0; i < pCount; ++i) {
				if(propertyName == propertyNames[i]) {
					properties[i]= true;
					found= true;
					break;
				}
			}
			if(!found) {
				throw megaglest_runtime_error("Unknown property: " + propertyName, true);
			}
		}
		//damage-particles
		if(parametersNode->hasChild("damage-particles")) {
			const XmlNode *particleNode= parametersNode->getChild("damage-particles");
			bool particleEnabled= particleNode->getAttribute("value")->getBoolValue();

			if(particleEnabled) {
				for(int i = 0; i < (int)particleNode->getChildCount(); ++i) {
					const XmlNode *particleFileNode= particleNode->getChild("particle-file", i);
					string path= particleFileNode->getAttribute("path")->getRestrictedValue();
					UnitParticleSystemType *unitParticleSystemType= new UnitParticleSystemType();

					unitParticleSystemType->load(particleFileNode, dir, currentPath + path,
							&Renderer::getInstance(),loadedFileList, sourceXMLFile,
							techTree->getPath());
					loadedFileList[currentPath + path].push_back(make_pair(sourceXMLFile,particleFileNode->getAttribute("path")->getRestrictedValue()));

					if(particleFileNode->getAttribute("minHp",false) != NULL && particleFileNode->getAttribute("maxHp",false) != NULL) {
						unitParticleSystemType->setMinmaxEnabled(true);
						unitParticleSystemType->setMinHp(particleFileNode->getAttribute("minHp")->getIntValue());
						unitParticleSystemType->setMaxHp(particleFileNode->getAttribute("maxHp")->getIntValue());

						if(particleFileNode->getAttribute("ispercentbased",false) != NULL) {
							unitParticleSystemType->setMinmaxIsPercent(particleFileNode->getAttribute("ispercentbased")->getBoolValue());
						}

					}

					damageParticleSystemTypes.push_back(unitParticleSystemType);
				}
			}
		}

		//healthbar
		if(parametersNode->hasChild("healthbar")) {
			const XmlNode *healthbarNode= parametersNode->getChild("healthbar");
			if(healthbarNode->hasChild("height")) {
				healthbarheight= healthbarNode->getChild("height")->getAttribute("value")->getFloatValue();
			}
			if(healthbarNode->hasChild("thickness")) {
				healthbarthickness= healthbarNode->getChild("thickness")->getAttribute("value")->getFloatValue(0.f, 1.f);
			}
			if(healthbarNode->hasChild("visible")) {
				string healthbarVisibleString=healthbarNode->getChild("visible")->getAttribute("value")->getValue();
				vector<string> v=split(healthbarVisibleString,"|");
				for (int i = 0; i < (int)v.size(); ++i) {
					string current=trim(v[i]);
					if(current=="always") {
						healthbarVisible=healthbarVisible|hbvAlways;
					} else if(current=="selected") {
						healthbarVisible=healthbarVisible|hbvSelected;
					} else if(current=="ifNeeded") {
						healthbarVisible=healthbarVisible|hbvIfNeeded;
					} else if(current=="off") {
						healthbarVisible=healthbarVisible|hbvOff;
					} else {
						throw megaglest_runtime_error("Unknown Healthbar Visible Option: " + current, true);
					}
				}
			}
		}

		//light
		const XmlNode *lightNode= parametersNode->getChild("light");
		light= lightNode->getAttribute("enabled")->getBoolValue();
		if(light){
			lightColor.x= lightNode->getAttribute("red")->getFloatValue(0.f, 1.f);
			lightColor.y= lightNode->getAttribute("green")->getFloatValue(0.f, 1.f);
			lightColor.z= lightNode->getAttribute("blue")->getFloatValue(0.f, 1.f);
		}

		//rotationAllowed
		if(parametersNode->hasChild("rotationAllowed")) {
			const XmlNode *rotationAllowedNode= parametersNode->getChild("rotationAllowed");
			rotationAllowed= rotationAllowedNode->getAttribute("value")->getBoolValue();
		}
		else
		{
			rotationAllowed=true;
		}

		std::map<string,int> sortedItems;

		//unit requirements
		bool hasDup = false;
		const XmlNode *unitRequirementsNode= parametersNode->getChild("unit-requirements");
		for(int i=0; i < (int)unitRequirementsNode->getChildCount(); ++i){
			const XmlNode *unitNode= 	unitRequirementsNode->getChild("unit", i);
			string name= unitNode->getAttribute("name")->getRestrictedValue();

			if(sortedItems.find(name) != sortedItems.end()) {
				hasDup = true;
			}

			sortedItems[name] = 0;
		}
		if(hasDup) {
			printf("WARNING, unit type [%s] has one or more duplicate unit requirements\n",this->getName(false).c_str());
		}

		for(std::map<string,int>::iterator iterMap = sortedItems.begin();
				iterMap != sortedItems.end(); ++iterMap) {
			unitReqs.push_back(factionType->getUnitType(iterMap->first));
		}
		sortedItems.clear();
		hasDup = false;

		//upgrade requirements
		const XmlNode *upgradeRequirementsNode= parametersNode->getChild("upgrade-requirements");
		for(int i=0; i < (int)upgradeRequirementsNode->getChildCount(); ++i){
			const XmlNode *upgradeReqNode= upgradeRequirementsNode->getChild("upgrade", i);
			string name= upgradeReqNode->getAttribute("name")->getRestrictedValue();

			if(sortedItems.find(name) != sortedItems.end()) {
				hasDup = true;
			}

			sortedItems[name] = 0;
		}

		if(hasDup) {
			printf("WARNING, unit type [%s] has one or more duplicate upgrade requirements\n",this->getName(false).c_str());
		}

		for(std::map<string,int>::iterator iterMap = sortedItems.begin();
				iterMap != sortedItems.end(); ++iterMap) {
			upgradeReqs.push_back(factionType->getUpgradeType(iterMap->first));
		}
		sortedItems.clear();
		hasDup = false;

		//resource requirements
		const XmlNode *resourceRequirementsNode= parametersNode->getChild("resource-requirements");
		costs.resize(resourceRequirementsNode->getChildCount());
		for(int i = 0; i < (int)costs.size(); ++i) {
			const XmlNode *resourceNode= resourceRequirementsNode->getChild("resource", i);
			string name= resourceNode->getAttribute("name")->getRestrictedValue();
			int amount= resourceNode->getAttribute("amount")->getIntValue();

			if(sortedItems.find(name) != sortedItems.end()) {
				hasDup = true;
			}
			sortedItems[name] = amount;
		}
		//if(hasDup || sortedItems.size() != costs.size()) printf("Found duplicate resource requirement, costs.size() = %d sortedItems.size() = %d\n",costs.size(),sortedItems.size());

		if(hasDup) {
			printf("WARNING, unit type [%s] has one or more duplicate resource requirements\n",this->getName(false).c_str());
		}

		if(sortedItems.size() < costs.size()) {
			costs.resize(sortedItems.size());
		}
		int index = 0;
		for(std::map<string,int>::iterator iterMap = sortedItems.begin();
				iterMap != sortedItems.end(); ++iterMap) {
			try {
				costs[index].init(techTree->getResourceType(iterMap->first), iterMap->second);
				index++;
			}
			catch(megaglest_runtime_error& ex) {
				if(validationMode == false) {
					throw;
				}
				else {
					SystemFlags::OutputDebug(SystemFlags::debugError,"In [%s::%s Line: %d] Error [%s]\nFor UnitType: %s Cost: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what(),name.c_str(),iterMap->second);
				}
			}
		}
		sortedItems.clear();
		hasDup = false;

		//resources stored
		const XmlNode *resourcesStoredNode= parametersNode->getChild("resources-stored");
		storedResources.resize(resourcesStoredNode->getChildCount());
		for(int i=0; i < (int)storedResources.size(); ++i){
			const XmlNode *resourceNode= resourcesStoredNode->getChild("resource", i);
			string name= resourceNode->getAttribute("name")->getRestrictedValue();
			int amount= resourceNode->getAttribute("amount")->getIntValue();

			if(sortedItems.find(name) != sortedItems.end()) {
				hasDup = true;
			}

			sortedItems[name] = amount;
		}

		if(hasDup) {
			printf("WARNING, unit type [%s] has one or more duplicate stored resources\n",this->getName(false).c_str());
		}

		if(sortedItems.size() < storedResources.size()) {
			storedResources.resize(sortedItems.size());
		}

		index = 0;
		for(std::map<string,int>::iterator iterMap = sortedItems.begin();
				iterMap != sortedItems.end(); ++iterMap) {
			try {
				storedResources[index].init(techTree->getResourceType(iterMap->first), iterMap->second);
				index++;
			}
			catch(megaglest_runtime_error& ex) {
				if(validationMode == false) {
					throw;
				}
				else {
					SystemFlags::OutputDebug(SystemFlags::debugError,"In [%s::%s Line: %d] Error [%s]\nFor UnitType: %s Store: %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what(),name.c_str(),iterMap->second);
				}
			}
		}
		sortedItems.clear();
		//hasDup = false;

		// Lootable resources (resources given/lost on death)
		if(parametersNode->hasChild("resources-death")) {
			const XmlNode *deathResourcesNode= parametersNode->getChild("resources-death");

			for(unsigned int i = 0; i < deathResourcesNode->getChildCount(); ++i){
				const XmlNode *resourceNode= deathResourcesNode->getChild("resource", i);
				string name= resourceNode->getAttribute("name")->getRestrictedValue();

				LootableResource resource;
				resource.setResourceType(techTree->getResourceType(name));

				// All attributes are optional, although nothing happens if they aren't used. They can
				// be combined freely. Percentages will take affect before absolute values.
				if(resourceNode->hasAttribute("amount-value")) {
					resource.setAmountValue(resourceNode->getAttribute("amount-value")->getIntValue());
				}
				else {
					resource.setAmountValue(0);
				}
				
				if(resourceNode->hasAttribute("amount-faction-percent")) {
					resource.setAmountFactionPercent(resourceNode->getAttribute("amount-faction-percent")->getIntValue());
				}
				else {
					resource.setAmountFactionPercent(0);
				}

				if(resourceNode->hasAttribute("loss-value")) {
					resource.setLossValue(resourceNode->getAttribute("loss-value")->getIntValue());
				}
				else {
					resource.setLossValue(0);
				}
				
				if(resourceNode->hasAttribute("loss-faction-percent")) {
					resource.setLossFactionPercent(resourceNode->getAttribute("loss-faction-percent")->getIntValue());
				}
				else {
					resource.setLossFactionPercent(0);
				}
				
				if(resourceNode->hasAttribute("allow-negative")) {
					resource.setNegativeAllowed(resourceNode->getAttribute("allow-negative")->getBoolValue());
				}
				else {
					resource.setNegativeAllowed(false);
				}

				// Figure out if there are duplicate resources. The value stored in the map is arbitrary,
				// and exists solely because 
				if(std::find(lootableResources.begin(), lootableResources.end(), resource) != lootableResources.end()) {
					printf("WARNING, unit type [%s] has one or more duplicate lootable resources\n", this->getName(false).c_str());
				}

				lootableResources.push_back(resource);
			}
		}

		// Tags
		if(parametersNode->hasChild("tags")) {
			const XmlNode *tagsNode= parametersNode->getChild("tags");

			for(unsigned int i = 0; i < tagsNode->getChildCount(); ++i){
				const XmlNode *resourceNode= tagsNode->getChild("tag", i);
				string tag= resourceNode->getAttribute("value")->getRestrictedValue();
				tags.insert(tag);
			}
		}

		//image
		const XmlNode *imageNode= parametersNode->getChild("image");
		image= Renderer::getInstance().newTexture2D(rsGame);
		if(image) {
			image->load(imageNode->getAttribute("path")->getRestrictedValue(currentPath));
		}
		loadedFileList[imageNode->getAttribute("path")->getRestrictedValue(currentPath)].push_back(make_pair(sourceXMLFile,imageNode->getAttribute("path")->getRestrictedValue()));

		//image cancel
		const XmlNode *imageCancelNode= parametersNode->getChild("image-cancel");
		cancelImage= Renderer::getInstance().newTexture2D(rsGame);
		if(cancelImage) {
			cancelImage->load(imageCancelNode->getAttribute("path")->getRestrictedValue(currentPath));
		}
		loadedFileList[imageCancelNode->getAttribute("path")->getRestrictedValue(currentPath)].push_back(make_pair(sourceXMLFile,imageCancelNode->getAttribute("path")->getRestrictedValue()));

		//meeting point
		const XmlNode *meetingPointNode= parametersNode->getChild("meeting-point");
		meetingPoint= meetingPointNode->getAttribute("value")->getBoolValue();
		if(meetingPoint) {
			meetingPointImage= Renderer::getInstance().newTexture2D(rsGame);
			if(meetingPointImage) {
				meetingPointImage->load(meetingPointNode->getAttribute("image-path")->getRestrictedValue(currentPath));
			}
			loadedFileList[meetingPointNode->getAttribute("image-path")->getRestrictedValue(currentPath)].push_back(make_pair(sourceXMLFile,meetingPointNode->getAttribute("image-path")->getRestrictedValue()));
		}

		//countUnitDeathInStats
		if(parametersNode->hasChild("count-unit-death-in-stats")){
			const XmlNode *countUnitDeathInStatsNode= parametersNode->getChild("count-unit-death-in-stats");
			countUnitDeathInStats= countUnitDeathInStatsNode->getAttribute("value")->getBoolValue();
		}
		else {
			countUnitDeathInStats=true;
		}
		//countUnitProductionInStats
		if(parametersNode->hasChild("count-unit-production-in-stats")){
			const XmlNode *countUnitProductionInStatsNode= parametersNode->getChild("count-unit-production-in-stats");
			countUnitProductionInStats= countUnitProductionInStatsNode->getAttribute("value")->getBoolValue();
		}
		else {
			countUnitProductionInStats=true;
		}
		//countUnitKillInStats
		if(parametersNode->hasChild("count-unit-kill-in-stats")){
			const XmlNode *countUnitKillInStatsNode= parametersNode->getChild("count-unit-kill-in-stats");
			countUnitKillInStats= countUnitKillInStatsNode->getAttribute("value")->getBoolValue();
		}
		else {
			countUnitKillInStats=true;
		}

		//countKillForUnitUpgrade
		if(parametersNode->hasChild("count-kill-for-unit-upgrade")){
			const XmlNode *countKillForUnitUpgradeNode= parametersNode->getChild("count-kill-for-unit-upgrade");
			countKillForUnitUpgrade= countKillForUnitUpgradeNode->getAttribute("value")->getBoolValue();
		} else {
			countKillForUnitUpgrade=true;
		}

		if(countKillForUnitUpgrade == false){
			// it makes no sense if we count it in stats but not for upgrades
			countUnitKillInStats=false;
		}

		//selection sounds
		const XmlNode *selectionSoundNode= parametersNode->getChild("selection-sounds");
		if(selectionSoundNode->getAttribute("enabled")->getBoolValue()){
			selectionSounds.resize((int)selectionSoundNode->getChildCount());
			for(int i = 0; i < (int)selectionSounds.getSounds().size(); ++i) {
				const XmlNode *soundNode= selectionSoundNode->getChild("sound", i);
				string path= soundNode->getAttribute("path")->getRestrictedValue(currentPath);
				StaticSound *sound= new StaticSound();
				sound->load(path);
				loadedFileList[path].push_back(make_pair(sourceXMLFile,soundNode->getAttribute("path")->getRestrictedValue()));
				selectionSounds[i]= sound;
			}
		}

		//command sounds
		const XmlNode *commandSoundNode= parametersNode->getChild("command-sounds");
		if(commandSoundNode->getAttribute("enabled")->getBoolValue()) {
			commandSounds.resize((int)commandSoundNode->getChildCount());
			for(int i = 0; i < (int)commandSoundNode->getChildCount(); ++i) {
				const XmlNode *soundNode= commandSoundNode->getChild("sound", i);
				string path= soundNode->getAttribute("path")->getRestrictedValue(currentPath);
				StaticSound *sound= new StaticSound();
				sound->load(path);
				loadedFileList[path].push_back(make_pair(sourceXMLFile,soundNode->getAttribute("path")->getRestrictedValue()));
				commandSounds[i]= sound;
			}
		}

		//skills

		const XmlNode *attackBoostsNode= NULL;
		if(unitNode->hasChild("attack-boosts") == true) {
			attackBoostsNode=unitNode->getChild("attack-boosts");
		}

		const XmlNode *skillsNode= unitNode->getChild("skills");
		skillTypes.resize(skillsNode->getChildCount());

		snprintf(szBuf,8096,Lang::getInstance().getString("LogScreenGameLoadingUnitTypeSkills","",true).c_str(),formatString(this->getName(true)).c_str(),skillTypes.size());
		Logger::getInstance().add(szBuf, true);

		for(int i = 0; i < (int)skillTypes.size(); ++i) {
			const XmlNode *sn= skillsNode->getChild("skill", i);
			const XmlNode *typeNode= sn->getChild("type");
			string classId= typeNode->getAttribute("value")->getRestrictedValue();
			SkillType *skillType= SkillTypeFactory::getInstance().newInstance(classId);

			skillTypes[i]=NULL;
			try {
				skillType->load(sn, attackBoostsNode, dir, techTree, factionType, loadedFileList,sourceXMLFile);
				skillTypes[i]= skillType;
			}
			catch(megaglest_runtime_error& ex) {
				if(validationMode == false) {
					throw;
				}
				else {
					SystemFlags::OutputDebug(SystemFlags::debugError,"In [%s::%s Line: %d] Error [%s]\nFor UnitType: %s SkillType: %s\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what(),name.c_str(),classId.c_str());
				}
			}
		}

		//commands
		const XmlNode *commandsNode= unitNode->getChild("commands");
		commandTypes.resize(commandsNode->getChildCount());
		for(int i = 0; i < (int)commandTypes.size(); ++i) {
			const XmlNode *commandNode= commandsNode->getChild("command", i);
			const XmlNode *typeNode= commandNode->getChild("type");
			string classId= typeNode->getAttribute("value")->getRestrictedValue();
			CommandType *commandType= CommandTypeFactory::getInstance().newInstance(classId);

			commandTypes[i]=NULL;
			try {
				commandType->load(i, commandNode, dir, techTree, factionType, *this,
						loadedFileList,sourceXMLFile);
				commandTypes[i]= commandType;
			}
			catch(megaglest_runtime_error& ex) {
				if(validationMode == false) {
					throw;
				}
				else {
					SystemFlags::OutputDebug(SystemFlags::debugError,"In [%s::%s Line: %d] Error [%s]\nFor UnitType: %s CommandType:%s\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what(),name.c_str(),classId.c_str());
				}
			}
		}
		sortCommandTypes(commandTypes);

		computeFirstStOfClass();
		computeFirstCtOfClass();

		if(getFirstStOfClass(scStop)==NULL){
			throw megaglest_runtime_error("Every unit must have at least one stop skill: "+ path,true);
		}
		if(getFirstStOfClass(scDie)==NULL){
			throw megaglest_runtime_error("Every unit must have at least one die skill: "+ path,true);
		}

	}
	//Exception handling (conversions and so on);
    catch(megaglest_runtime_error& ex) {
		SystemFlags::OutputDebug(SystemFlags::debugError,"In [%s::%s Line: %d] Error [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,ex.what());
		throw megaglest_runtime_error("Error loading UnitType: " + path + "\nMessage: " + ex.what(),!ex.wantStackTrace());
    }
	catch(const exception &e){
		SystemFlags::OutputDebug(SystemFlags::debugError,"In [%s::%s Line: %d] Error [%s]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,e.what());
		throw megaglest_runtime_error("Error loading UnitType: " + path + "\nMessage: " + e.what());
	}

	if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
}

// ==================== get ====================

const Level *UnitType::getLevel(string name) const {
	const Level *result = NULL;
	for(unsigned int i = 0; i < levels.size(); ++i) {
		const Level &level = levels[i];
		if(level.getName() == name) {
			result = &level;
			break;
		}
	}

	return result;
}

const CommandType *UnitType::getFirstCtOfClass(CommandClass commandClass) const{
	if(firstCommandTypeOfClass[commandClass] == NULL) {
		return NULL;
		//if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d] commandClass = %d\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,commandClass);

		/*
	    for(int j=0; j<ccCount; ++j){
	        for(int i=0; i<commandTypes.size(); ++i){
	            if(commandTypes[i]->getClass()== CommandClass(j)){
	                return commandTypes[i];
	            }
	        }
	    }
	    */

		//if(SystemFlags::getSystemSettingType(SystemFlags::debugSystem).enabled) SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
	}
    return firstCommandTypeOfClass[commandClass];
}

const SkillType *UnitType::getFirstStOfClass(SkillClass skillClass) const{
	if(firstSkillTypeOfClass[skillClass] == NULL) {
		/*
		for(int j= 0; j<scCount; ++j){
	        for(int i= 0; i<skillTypes.size(); ++i){
	            if(skillTypes[i]->getClass()== SkillClass(j)){
	                return skillTypes[i];
	            }
	        }
	    }
	    */
		//SystemFlags::OutputDebug(SystemFlags::debugSystem,"In [%s::%s Line: %d]\n",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__);
	}
    return firstSkillTypeOfClass[skillClass];
}

const HarvestCommandType *UnitType::getFirstHarvestCommand(const ResourceType *resourceType, const Faction *faction) const {
	for(int i = 0; i < (int)commandTypes.size(); ++i) {
		if(commandTypes[i]->getClass() == ccHarvest) {
			const HarvestCommandType *hct = static_cast<const HarvestCommandType*>(commandTypes[i]);

			if (faction->reqsOk(hct) == false) {
			 	continue;
			}

			if(hct->canHarvest(resourceType)) {
				return hct;
			}
		}
	}
	return NULL;
}

const HarvestEmergencyReturnCommandType *UnitType::getFirstHarvestEmergencyReturnCommand() const {
	const HarvestEmergencyReturnCommandType *result = dynamic_cast<const HarvestEmergencyReturnCommandType *>(ctHarvestEmergencyReturnCommandType.get());
	return result;
}

const AttackCommandType *UnitType::getFirstAttackCommand(Field field) const{
	//printf("$$$ Unit [%s] commandTypes.size() = %d\n",this->getName().c_str(),(int)commandTypes.size());

	for(int i = 0; i < (int)commandTypes.size(); ++i){
		if(commandTypes[i] == NULL) {
			throw megaglest_runtime_error("commandTypes[i] == NULL");
		}

		//printf("$$$ Unit [%s] i = %d, commandTypes[i] [%s]\n",this->getName().c_str(),(int)i, commandTypes[i]->toString().c_str());
		if(commandTypes[i]->getClass()== ccAttack){
			const AttackCommandType *act= dynamic_cast<const AttackCommandType*>(commandTypes[i]);
			if(act != NULL && act->getAttackSkillType()->getAttackField(field)) {
				//printf("## Unit [%s] i = %d, is found\n",this->getName().c_str(),(int)i);
				return act;
			}
		}
	}

	return NULL;
}

const AttackStoppedCommandType *UnitType::getFirstAttackStoppedCommand(Field field) const{
	//printf("$$$ Unit [%s] commandTypes.size() = %d\n",this->getName().c_str(),(int)commandTypes.size());

	for(int i = 0; i < (int)commandTypes.size(); ++i){
		if(commandTypes[i] == NULL) {
			throw megaglest_runtime_error("commandTypes[i] == NULL");
		}

		//printf("$$$ Unit [%s] i = %d, commandTypes[i] [%s]\n",this->getName().c_str(),(int)i, commandTypes[i]->toString().c_str());
		if(commandTypes[i]->getClass()== ccAttackStopped){
			const AttackStoppedCommandType *act= dynamic_cast<const AttackStoppedCommandType*>(commandTypes[i]);
			if(act != NULL && act->getAttackSkillType()->getAttackField(field)) {
				//printf("## Unit [%s] i = %d, is found\n",this->getName().c_str(),(int)i);
				return act;
			}
		}
	}

	return NULL;
}

const RepairCommandType *UnitType::getFirstRepairCommand(const UnitType *repaired) const{
	for(int i=0; i < (int)commandTypes.size(); ++i){
		if(commandTypes[i]->getClass()== ccRepair){
			const RepairCommandType *rct= static_cast<const RepairCommandType*>(commandTypes[i]);
			if(rct->isRepairableUnitType(repaired)){
				return rct;
			}
		}
	}
	return NULL;
}

bool UnitType::hasEmptyCellMap() const {
	//checkItemInVault(&(this->size),this->size);
	bool result = (size > 0);
	for(int i = 0; result == true && i < size; ++i) {
		for(int j = 0; j < size; ++j){
			if(cellMap[i*size+j] == true) {
				result = false;
				break;
			}
		}
	}

	return result;
}

Vec2i UnitType::getFirstOccupiedCellInCellMap(Vec2i currentPos) const {
	Vec2i cell = currentPos;
	//printf("\n\n\n\n^^^^^^^^^^ currentPos [%s] size [%d]\n",currentPos.getString().c_str(),size);

	//checkItemInVault(&(this->size),this->size);
	if(hasCellMap() == true) {
		for(int i = 0; i < size; ++i) {
			for(int j = 0; j < size; ++j){
				if(cellMap[i*size+j] == true) {
					cell.x += i;
					cell.y += j;
					//printf("\n^^^^^^^^^^ cell [%s] i [%d] j [%d]\n",cell.getString().c_str(),i,j);
					return cell;
				}
			}
		}
	}
	return cell;
}

bool UnitType::getCellMapCell(int x, int y, CardinalDir facing) const {
	assert(cellMap);
	if(cellMap == NULL) {
		throw megaglest_runtime_error("cellMap == NULL");
	}

	//checkItemInVault(&(this->size),this->size);
	int tmp=0;
	switch (facing) {
		case CardinalDir::EAST:
			tmp = y;
			y = x;
			x = size - tmp - 1;
			break;
		case CardinalDir::SOUTH:
			x = size - x - 1;
			y = size - y - 1;
			break;
		case CardinalDir::WEST:
			tmp = x;
			x = y;
			y = size - tmp - 1;
			break;
		default:
			break;
	}
	return cellMap[y * size + x];
}

int UnitType::getStore(const ResourceType *rt) const{
    for(int i=0; i < (int)storedResources.size(); ++i){
		if(storedResources[i].getType()==rt){
            return storedResources[i].getAmount();
		}
    }
    return 0;
}

const SkillType *UnitType::getSkillType(const string &skillName, SkillClass skillClass) const{
	for(int i=0; i < (int)skillTypes.size(); ++i){
		if(skillTypes[i]->getName()==skillName){
			if(skillTypes[i]->getClass()==skillClass){
				return skillTypes[i];
			}
			else{
				throw megaglest_runtime_error("Skill \""+skillName+"\" is not of class \""+SkillType::skillClassToStr(skillClass));
			}
		}
	}
	throw megaglest_runtime_error("No skill named \""+skillName+"\"");
}

// ==================== totals ====================

int UnitType::getTotalMaxHp(const TotalUpgrade *totalUpgrade) const {
	checkItemInVault(&(this->maxHp),this->maxHp);
	int result = maxHp + totalUpgrade->getMaxHp();
	result = max(0,result);
	return result;
}

int UnitType::getTotalMaxHpRegeneration(const TotalUpgrade *totalUpgrade) const {
	checkItemInVault(&(this->hpRegeneration),this->hpRegeneration);
	int result = hpRegeneration + totalUpgrade->getMaxHpRegeneration();
	//result = max(0,result);
	return result;
}

int UnitType::getTotalMaxEp(const TotalUpgrade *totalUpgrade) const {
	checkItemInVault(&(this->maxEp),this->maxEp);
	int result = maxEp + totalUpgrade->getMaxEp();
	result = max(0,result);
	return result;
}

int UnitType::getTotalMaxEpRegeneration(const TotalUpgrade *totalUpgrade) const {
	checkItemInVault(&(this->epRegeneration),this->epRegeneration);
	int result = epRegeneration + totalUpgrade->getMaxEpRegeneration();
	//result = max(0,result);
	return result;
}

int UnitType::getTotalArmor(const TotalUpgrade *totalUpgrade) const {
	checkItemInVault(&(this->armor),this->armor);
	int result = armor + totalUpgrade->getArmor();
	result = max(0,result);
	return result;
}

int UnitType::getTotalSight(const TotalUpgrade *totalUpgrade) const {
	checkItemInVault(&(this->sight),this->sight);
	int result = sight + totalUpgrade->getSight();
	result = max(0,result);
	return result;
}

// ==================== has ====================

bool UnitType::hasSkillClass(SkillClass skillClass) const {
    return firstSkillTypeOfClass[skillClass]!=NULL;
}

bool UnitType::hasCommandType(const CommandType *commandType) const {
    assert(commandType!=NULL);

	const HarvestEmergencyReturnCommandType *result = dynamic_cast<const HarvestEmergencyReturnCommandType *>(ctHarvestEmergencyReturnCommandType.get());
	if(commandType == result) {
		return true;
	}

    for(int i=0; i < (int)commandTypes.size(); ++i) {
        if(commandTypes[i]==commandType) {
            return true;
        }
    }
    return false;
}

//bool UnitType::hasSkillType(const SkillType *skillType) const {
//    assert(skillType!=NULL);
//    for(int i=0; i < (int)skillTypes.size(); ++i) {
//        if(skillTypes[i]==skillType) {
//            return true;
//        }
//    }
//    return false;
//}

bool UnitType::isOfClass(UnitClass uc) const{
	switch(uc){
	case ucWarrior:
		return hasSkillClass(scAttack) && !hasSkillClass(scHarvest);
	case ucWorker:
		return hasSkillClass(scBuild) || hasSkillClass(scRepair)|| hasSkillClass(scHarvest);
	case ucBuilding:
		return hasSkillClass(scBeBuilt);
	default:
		assert(false);
		break;
	}
	return false;
}

// ==================== PRIVATE ====================

void UnitType::computeFirstStOfClass() {
	for(int j= 0; j < scCount; ++j) {
        firstSkillTypeOfClass[j]= NULL;
        for(int i= 0; i < (int)skillTypes.size(); ++i) {
            if(skillTypes[i] != NULL && skillTypes[i]->getClass()== SkillClass(j)) {
                firstSkillTypeOfClass[j]= skillTypes[i];
                break;
            }
        }
    }
}

void UnitType::computeFirstCtOfClass() {
    for(int j = 0; j < ccCount; ++j) {
        firstCommandTypeOfClass[j]= NULL;
        for(int i = 0; i < (int)commandTypes.size(); ++i) {
            if(commandTypes[i] != NULL && commandTypes[i]->getClass() == CommandClass(j)) {
                firstCommandTypeOfClass[j] = commandTypes[i];
                break;
            }
        }
    }
}

void UnitType::sortCommandTypes(CommandTypes cts){
	try{
		CommandTypes ctCores, ctBasics = {NULL,NULL,NULL,NULL}, ctMorphs;
		vector<int>	 basicNulls = {0,1,2,3};
		map<CommandClass, int>	basicIndexes = {};

		//Morphs
		for(int i = (int)cts.size(); i --> 0; ) {
			if(cts[i]->getClass() == ccMorph) {
				ctMorphs.insert(ctMorphs.begin(), cts[i]);
				cts.erase(cts.begin() + i);
			}
		}

		//Basics
		for(int i= 0; i < (int)cts.size(); i++) {
			for(auto &&cc : CommandHelper::getBasicsCC()) {
				if(cts[i]->getClass() == cc && basicIndexes.find(cc) == basicIndexes.end()) {
					basicIndexes[cc]= i;
				}
			}
		}
		for(int i = (int)cts.size(); i --> 0; ) {
			for(auto kv : basicIndexes ) {
				if(kv.second == i) {
					auto ccPos = CommandHelper::getBasicPos(kv.first);
					ctBasics[ccPos] = cts[i];
					cts.erase(cts.begin() + i);
					basicNulls.erase(std::remove(basicNulls.begin(), basicNulls.end(), ccPos), basicNulls.end());
				}
			}
		}
		
		//Cores
		for(auto &&cc : CommandHelper::getCoresCC()){
			std::copy_if(cts.begin(), cts.end(), std::back_inserter(ctCores), [cc](CommandType* i) {
				return i->getClass() == cc;
			});
		}
		int nullCount = 4 - ctCores.size();
		for(int i=0; i<nullCount; i++){
			ctCores.push_back(NULL);
		}
		
		if(nullCount < 0) {//magic: if we cant push all commands to cores
			CommandTypes ctToBasics(ctCores.end() + nullCount, ctCores.end());
			ctCores.resize(4);
			for(int i = (int)ctToBasics.size(); i --> 0; ) {// we push it to basics
				if(i < (int)basicNulls.size()){
				ctBasics[basicNulls[i]] = ctToBasics[i];
				ctToBasics.erase(ctToBasics.begin() + i);
				basicNulls.erase(basicNulls.begin() + i);
				}
			}
			//everything else to Morphs and so on untill cancel image 
			ctMorphs.insert(ctMorphs.end(), ctToBasics.begin(), ctToBasics.end());
		}

		commandTypesSorted.insert(commandTypesSorted.end(), ctCores.begin(), ctCores.end());
		commandTypesSorted.insert(commandTypesSorted.end(), ctBasics.begin(), ctBasics.end());
		commandTypesSorted.insert(commandTypesSorted.end(), ctMorphs.begin(), ctMorphs.end());
	} catch(exception &ex){
		
	}
}

const CommandType* UnitType::findCommandTypeById(int id) const{
	const HarvestEmergencyReturnCommandType *result = dynamic_cast<const HarvestEmergencyReturnCommandType *>(ctHarvestEmergencyReturnCommandType.get());
	if(result != NULL && id == result->getId()) {
		return result;
	}

	for(int i=0; i < getCommandTypeCount(); ++i) {
		const CommandType *commandType= getCommandType(i);
		if(commandType->getId() == id){
			return commandType;
		}
	}
	return NULL;
}

const CommandType *UnitType::getCommandType(int i) const {
	if(i >= (int)commandTypes.size()) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s Line: %d] i >= commandTypes.size(), i = %d, commandTypes.size() = " MG_SIZE_T_SPECIFIER "",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,i,commandTypes.size());
		throw megaglest_runtime_error(szBuf);
	}
	return commandTypes[i];
}

const CommandType *UnitType::getCommandTypeSorted(int i) const {
	if(i >= (int)commandTypesSorted.size()) {
		char szBuf[8096]="";
		snprintf(szBuf,8096,"In [%s::%s Line: %d] i >= commandTypesSorted.size(), i = %d, commandTypesSorted.size() = " MG_SIZE_T_SPECIFIER "",extractFileFromDirectoryPath(__FILE__).c_str(),__FUNCTION__,__LINE__,i,commandTypesSorted.size());
		throw megaglest_runtime_error(szBuf);
	}
	return commandTypesSorted[i];
}

string UnitType::getCommandTypeListDesc() const {
	string desc = "Commands: ";
	for(int i=0; i<getCommandTypeCount(); ++i){
		const CommandType* commandType= getCommandType(i);
		desc += " id = " + intToStr(commandType->getId()); + " toString: " + commandType->toString(false);
	}
	return desc;

}

string UnitType::getReqDesc(bool translatedValue) const{
	Lang &lang= Lang::getInstance();
	//string desc = "Limits: ";
	string resultTxt="";

	checkItemInVault(&(this->maxUnitCount),this->maxUnitCount);
	if(getMaxUnitCount() > 0) {
		resultTxt += "\n" + lang.getString("MaxUnitCount") + " " + intToStr(getMaxUnitCount());
	}
	if(resultTxt == "")
		return ProducibleType::getReqDesc(translatedValue);
	else
		return ProducibleType::getReqDesc(translatedValue)+"\n" + lang.getString("Limits") + " " + resultTxt;
}

string UnitType::getName(bool translatedValue) const {
	if(translatedValue == false) return name;

	Lang &lang = Lang::getInstance();
	return lang.getTechTreeString("UnitTypeName_" + name,name.c_str());
}

std::string UnitType::toString() const {
	std::string result = "Unit Name: [" + name + "] id = " + intToStr(id);
	result += " maxHp = " + intToStr(maxHp);
	result += " hpRegeneration = " + intToStr(hpRegeneration);
	result += " maxEp = " + intToStr(maxEp);
	result += " startEpValue = " + intToStr(startEpValue);
	result += " startEpPercentage = " + intToStr(startEpPercentage);
	result += " epRegeneration = " + intToStr(epRegeneration);
	result += " maxUnitCount = " + intToStr(getMaxUnitCount());


	for(int i = 0; i < fieldCount; i++) {
		result += " fields index = " + intToStr(i) + " value = " + intToStr(fields[i]);
	}
	for(int i = 0; i < pCount; i++) {
		result += " properties index = " + intToStr(i) + " value = " + intToStr(properties[i]);
	}

	result += " armor = " + intToStr(armor);

	if(armorType != NULL) {
		result += " armorType Name: [" + armorType->getName(false) + " id = " +  intToStr(armorType->getId());
	}

	result += " light = " + intToStr(light);
	result += " lightColor = " + lightColor.getString();
	result += " multiSelect = " + intToStr(multiSelect);
	result += " uniformSelect = " + intToStr(uniformSelect);
	result += " commandable = " + intToStr(commandable);
	result += " sight = " + intToStr(sight);
	result += " size = " + intToStr(size);
	result += " height = " + intToStr(height);
	result += " rotatedBuildPos = " + floatToStr(rotatedBuildPos,6);
	result += " rotationAllowed = " + intToStr(rotationAllowed);

	if(cellMap != NULL) {
		result += " cellMap: [" + intToStr(size) + "]";
		for(int i = 0; i < size; ++i) {
			for(int j = 0; j < size; ++j){
				result += " i = " + intToStr(i) + " j = " + intToStr(j) + " value = " + intToStr(cellMap[i*size+j]);
			}
		}
	}

	result += " skillTypes: [" + intToStr(skillTypes.size()) + "]";
	for(int i = 0; i < (int)skillTypes.size(); ++i) {
		result += " i = " + intToStr(i) + " " + skillTypes[i]->toString(false);
	}

	result += " commandTypes: [" + intToStr(commandTypes.size()) + "]";
	for(int i = 0; i < (int)commandTypes.size(); ++i) {
		result += " i = " + intToStr(i) + " " + commandTypes[i]->toString(false);
	}

	result += " storedResources: [" + intToStr(storedResources.size()) + "]";
	for(int i = 0; i < (int)storedResources.size(); ++i) {
		result += " i = " + intToStr(i) + " " + storedResources[i].getDescription(false);
	}

	result += " levels: [" + intToStr(levels.size()) + "]";
	for(int i = 0; i < (int)levels.size(); ++i) {
		result += " i = " + intToStr(i) + " " + levels[i].getName();
	}

	result += " meetingPoint = " + intToStr(meetingPoint);

	result += " countInVictoryConditions = " + intToStr(countInVictoryConditions);

	return result;
}

}}//end namespace
