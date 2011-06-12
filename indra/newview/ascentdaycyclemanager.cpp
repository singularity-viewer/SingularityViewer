/** 
 * @file ascentdaycyclemanager.cpp
 * @Author Duncan Garrett
 * Manager for Windlight Daycycles so we can actually save more than one
 *
 * Created August 27 2010
 * 
 * ALL SOURCE CODE IS PROVIDED "AS IS." THE CREATOR MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * k ilu bye
 */

#include "llviewerprecompiledheaders.h"

#include "ascentdaycyclemanager.h"

#include "pipeline.h"
#include "llsky.h"

#include "lldiriterator.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "llcheckboxctrl.h"
#include "lluictrlfactory.h"
#include "llviewercamera.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llsdserialize.h"

#include "v4math.h"
#include "llviewerdisplay.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"
#include "lldrawpoolwater.h"
#include "llagent.h"
#include "llviewerregion.h"

#include "llwldaycycle.h"
#include "llfloaterwindlight.h"
#include "llfloaterdaycycle.h"
#include "llfloaterenvsettings.h"

#include "curl/curl.h"

AscentDayCycleManager * AscentDayCycleManager::sInstance = NULL;

AscentDayCycleManager::AscentDayCycleManager()
{
}

AscentDayCycleManager::~AscentDayCycleManager()
{
}

void AscentDayCycleManager::loadPresets(const std::string& file_name)
{
	std::string path_name(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight/days", ""));
	LL_INFOS2("AppInit", "Shaders") << "Loading Default Day Cycle preset from " << path_name << LL_ENDL;
			
	bool found = true;			
	LLDirIterator app_settings_iter(path_name, "*.xml");
	while(found) 
	{
		std::string name;
		found = app_settings_iter.next(name);
		if(found)
		{

			name=name.erase(name.length()-4);

			// bugfix for SL-46920: preventing filenames that break stuff.
			char * curl_str = curl_unescape(name.c_str(), name.size());
			std::string unescaped_name(curl_str);
			curl_free(curl_str);
			curl_str = NULL;

			LL_DEBUGS2("AppInit", "Shaders") << "name: " << name << LL_ENDL;
			loadPreset(unescaped_name,FALSE);
		}
	}

	// And repeat for user presets, note the user presets will modify any system presets already loaded

	std::string path_name2(gDirUtilp->getExpandedFilename( LL_PATH_USER_SETTINGS , "windlight/days", ""));
	LL_INFOS2("AppInit", "Shaders") << "Loading User Daycycle preset from " << path_name2 << LL_ENDL;
			
	found = true;			
	LLDirIterator user_settings_iter(path_name2, "*.xml");
	while(found) 
	{
		std::string name;
		found = user_settings_iter.next(name);
		if(found)
		{
			name=name.erase(name.length()-4);

			// bugfix for SL-46920: preventing filenames that break stuff.
			char * curl_str = curl_unescape(name.c_str(), name.size());
			std::string unescaped_name(curl_str);
			curl_free(curl_str);
			curl_str = NULL;

			LL_DEBUGS2("AppInit", "Shaders") << "name: " << name << LL_ENDL;
			loadPreset(unescaped_name,FALSE);
		}
	}

}

void AscentDayCycleManager::savePresets(const std::string & fileName)
{
	//Nobody currently calls me, but if they did, then its reasonable to write the data out to the user's folder
	//and not over the RO system wide version.

	LLSD paramsData(LLSD::emptyMap());
	
	std::string pathName(gDirUtilp->getExpandedFilename( LL_PATH_USER_SETTINGS , "windlight", fileName));

	/*for(std::map<std::string, LLWLDayCycle>::iterator mIt = mParamList.begin();
		mIt != mParamList.end();
		++mIt) 
	{
		paramsData[mIt->first] = mIt->second.getAll();
	}*/

	llofstream presetsXML(pathName);

	LLPointer<LLSDFormatter> formatter = new LLSDXMLFormatter();

	formatter->format(paramsData, presetsXML, LLSDFormatter::OPTIONS_PRETTY);

	presetsXML.close();
}

void AscentDayCycleManager::loadPreset(const std::string & name,bool propagate)
{
	
	// bugfix for SL-46920: preventing filenames that break stuff.
	std::string escaped_filename = LLWeb::curlEscape(name);

	escaped_filename += ".xml";

	std::string pathName(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight/days", escaped_filename));
	llinfos << "Loading Day Cycle preset from " << pathName << llendl;

	llifstream presetsXML;
	presetsXML.open(pathName.c_str());

	// That failed, try loading from the users area instead.
	if(!presetsXML)
	{
		pathName=gDirUtilp->getExpandedFilename( LL_PATH_USER_SETTINGS , "windlight/days", escaped_filename);
		llinfos << "Loading User Day Cycle preset from " << pathName << llendl;
		presetsXML.open(pathName.c_str());
	}

	if (presetsXML)
	{
		LLSD paramsData(LLSD::emptyMap());

		LLPointer<LLSDParser> parser = new LLSDXMLParser();

		parser->parse(presetsXML, paramsData, LLSDSerialize::SIZE_UNLIMITED);

		std::map<std::string, LLWLDayCycle>::iterator mIt = mParamList.find(name);
		if(mIt == mParamList.end())
		{
			addParamSet(name, paramsData);
		}
		else 
		{
			setParamSet(name, paramsData);
		}
		presetsXML.close();
	} 
	else 
	{
		llwarns << "Can't find " << name << llendl;
		return;
	}

	
}	

void AscentDayCycleManager::savePreset(const std::string & name)
{
	// bugfix for SL-46920: preventing filenames that break stuff.
	std::string escaped_filename = LLWeb::curlEscape(name);

	escaped_filename += ".xml";

	// make an empty llsd
	LLSD paramsData(LLSD::emptyMap());
	std::string pathName(gDirUtilp->getExpandedFilename( LL_PATH_USER_SETTINGS , "windlight/days", escaped_filename));

	// fill it with LLSD windlight params
	//paramsData = mParamList[name].getAll();

	// write to file
	llofstream presetsXML(pathName);
	LLPointer<LLSDFormatter> formatter = new LLSDXMLFormatter();
	formatter->format(paramsData, presetsXML, LLSDFormatter::OPTIONS_PRETTY);
	presetsXML.close();
}

void AscentDayCycleManager::update(LLViewerCamera * cam)
{
	LLFastTimer ftm(LLFastTimer::FTM_UPDATE_WLPARAM);
	
	// sync menus if they exist
	if(LLFloaterWindLight::isOpen()) 
	{
		LLFloaterWindLight::instance()->syncMenu();
	}
	if(LLFloaterDayCycle::isOpen()) 
	{
		LLFloaterDayCycle::instance()->syncMenu();
	}
	if(LLFloaterEnvSettings::isOpen()) 
	{
		LLFloaterEnvSettings::instance()->syncMenu();
	}

	stop_glerror();
}

// static
void AscentDayCycleManager::initClass(void)
{
	instance();
}

// static
void AscentDayCycleManager::cleanupClass()
{
	delete sInstance;
	sInstance = NULL;
}

void AscentDayCycleManager::resetAnimator(F32 curTime, bool run)
{
	mAnimator.setTrack(mDay.mTimeMap, mDay.mDayRate, 
		curTime, run);

	return;
}
bool AscentDayCycleManager::addParamSet(const std::string& name, LLWLDayCycle& param)
{
	// add a new one if not one there already
	std::map<std::string, LLWLDayCycle>::iterator mIt = mParamList.find(name);
	if(mIt == mParamList.end()) 
	{	
		mParamList[name] = param;
		return true;
	}

	return false;
}

BOOL AscentDayCycleManager::addParamSet(const std::string& name, LLSD const & param)
{
	// add a new one if not one there already
	std::map<std::string, LLWLDayCycle>::const_iterator finder = mParamList.find(name);
	if(finder == mParamList.end())
	{
		mParamList[name].mName = name;
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

bool AscentDayCycleManager::getParamSet(const std::string& name, LLWLDayCycle& param)
{
	// find it and set it
	std::map<std::string, LLWLDayCycle>::iterator mIt = mParamList.find(name);
	if(mIt != mParamList.end()) 
	{
		param = mParamList[name];
		param.mName = name;
		return true;
	}

	return false;
}

bool AscentDayCycleManager::setParamSet(const std::string& name, LLWLDayCycle& param)
{
	mParamList[name] = param;

	return true;
}

bool AscentDayCycleManager::setParamSet(const std::string& name, const LLSD & param)
{
	// quick, non robust (we won't be working with files, but assets) check
	if(!param.isMap()) 
	{
		return false;
	}
	
	return true;
}

bool AscentDayCycleManager::removeParamSet(const std::string& name, bool delete_from_disk)
{
	// remove from param list
	std::map<std::string, LLWLDayCycle>::iterator mIt = mParamList.find(name);
	if(mIt != mParamList.end()) 
	{
		mParamList.erase(mIt);
	}

	F32 key;

	// remove all references
	bool stat = true;
	do 
	{
		// get it
		stat = mDay.getKey(name, key);
		if(stat == false) 
		{
			break;
		}

		// and remove
		stat = mDay.removeKey(key);

	} while(stat == true);
	
	if(delete_from_disk)
	{
		std::string path_name(gDirUtilp->getExpandedFilename( LL_PATH_USER_SETTINGS , "windlight/days", ""));
		
		// use full curl escaped name
		std::string escaped_name = LLWeb::curlEscape(name);
		
		gDirUtilp->deleteFilesInDir(path_name, escaped_name + ".xml");
	}

	return true;
}


// static
AscentDayCycleManager * AscentDayCycleManager::instance()
{
	if(NULL == sInstance)
	{
		sInstance = new AscentDayCycleManager();

		sInstance->loadPresets(LLStringUtil::null);

		// load the day
		sInstance->mDay.loadDayCycle(gSavedSettings.getString("AscentActiveDayCycle"));

		// *HACK - sets cloud scrolling to what we want... fix this better in the future
		sInstance->getParamSet("Default", sInstance->mCurParams);

		// set it to noon
		sInstance->resetAnimator(0.5, true);

		// but use linden time sets it to what the estate is
		sInstance->mAnimator.mUseLindenTime = true;
	}

	return sInstance;
}
