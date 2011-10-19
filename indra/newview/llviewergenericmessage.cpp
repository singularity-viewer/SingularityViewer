/** 
 * @file llviewergenericmessage.cpp
 * @brief Handle processing of "generic messages" which contain short lists of strings.
 * @author James Cook
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llviewergenericmessage.h"
#include "meta7windlight.h"
#include "lldispatcher.h"
#include "lluuid.h"
#include "message.h"
#include "llagent.h"
#include "llwaterparamset.h"
#include "llwaterparammanager.h"
#include "llwlparamset.h"
#include "llwlparammanager.h"
#include "lluuid.h"

LLDispatcher gGenericDispatcher;


void send_generic_message(const std::string& method,
						  const std::vector<std::string>& strings,
						  const LLUUID& invoice)
{
	LLMessageSystem* msg = gMessageSystem;
	msg->newMessage("GenericMessage");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgent.getID());
	msg->addUUIDFast(_PREHASH_SessionID, gAgent.getSessionID());
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); //not used
	msg->nextBlock("MethodData");
	msg->addString("Method", method);
	msg->addUUID("Invoice", invoice);
	if(strings.empty())
	{
		msg->nextBlock("ParamList");
		msg->addString("Parameter", NULL);
	}
	else
	{
		std::vector<std::string>::const_iterator it = strings.begin();
		std::vector<std::string>::const_iterator end = strings.end();
		for(; it != end; ++it)
		{
			msg->nextBlock("ParamList");
			msg->addString("Parameter", *it);
		}
	}
	gAgent.sendReliableMessage();
}



void process_generic_message(LLMessageSystem* msg, void**)
{
	LLUUID agent_id;
	msg->getUUID("AgentData", "AgentID", agent_id);

	std::string method;
	msg->getStringFast(_PREHASH_MethodData, _PREHASH_Method, method);
	
	//This needs to be handled by a dispatcher really, but I'm not sure where is the best place to put it
	if (method == "Windlight")
	{	
		//Meta7 WindLight packet
		//We are delivering with an agentID of NULL_KEY so as to be
		//friendly and not trigger a warning for unsupporting clients.
		S32 count = msg->getNumberOfBlocksFast(_PREHASH_ParamList);
		for (S32 i = 0; i < count; ++i)
		{
			// our param is binary data)
			S32 size = msg->getSizeFast(_PREHASH_ParamList, i, _PREHASH_Parameter);
			if (size >= 0)
			{
				char buf[250];
				msg->getBinaryDataFast(
					_PREHASH_ParamList, _PREHASH_Parameter,
					buf, size, i, 249);
				
				Meta7WindlightPacket* wl = (Meta7WindlightPacket*)buf;
			
				LLWaterParamManager * param_mgr = LLWaterParamManager::getInstance();
				LLWaterParamSet & param_set = param_mgr->mCurParams;
				LLWaterParamSet backup;
				if(!param_mgr->getParamSet("LightShare-Backup", backup)) {
					param_mgr->addParamSet("LightShare-Backup", param_set);
				}

				param_set.set("waterFogColor", wl->waterColor.red / 256.f, wl->waterColor.green / 256.f, wl->waterColor.blue / 256.f);
				param_set.set("waterFogDensity", pow(2.0f, wl->waterFogDensityExponent));
				param_set.set("underWaterFogMod", wl->underwaterFogModifier);
				param_set.set("normScale", wl->reflectionWaveletScale.X,wl->reflectionWaveletScale.Y,wl->reflectionWaveletScale.Z);
				param_set.set("fresnelScale", wl->fresnelScale);
				param_set.set("fresnelOffset", wl->fresnelOffset);
				param_set.set("scaleAbove", wl->refractScaleAbove);
				param_set.set("scaleBelow", wl->refractScaleBelow);
				param_set.set("blurMultiplier", wl->blurMultiplier);
				param_set.set("wave1Dir", wl->littleWaveDirection.X, wl->littleWaveDirection.Y);
				param_set.set("wave2Dir", wl->bigWaveDirection.X, wl->bigWaveDirection.Y);

				LLUUID normalMapTexture;

				std::string out = llformat(
					"%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
					(U8)(wl->normalMapTexture[0]),
					(U8)(wl->normalMapTexture[1]),
					(U8)(wl->normalMapTexture[2]),
					(U8)(wl->normalMapTexture[3]),
					(U8)(wl->normalMapTexture[4]),
					(U8)(wl->normalMapTexture[5]),
					(U8)(wl->normalMapTexture[6]),
					(U8)(wl->normalMapTexture[7]),
					(U8)(wl->normalMapTexture[8]),
					(U8)(wl->normalMapTexture[9]),
					(U8)(wl->normalMapTexture[10]),
					(U8)(wl->normalMapTexture[11]),
					(U8)(wl->normalMapTexture[12]),
					(U8)(wl->normalMapTexture[13]),
					(U8)(wl->normalMapTexture[14]),
					(U8)(wl->normalMapTexture[15]));

				normalMapTexture.set(out);

				param_mgr->setParamSet(	"LightShare-CurrentRegion", param_set);
				param_mgr->setNormalMapID(normalMapTexture);

				LLWLParamManager * wl_param_mgr = LLWLParamManager::getInstance();
				LLWLParamSet & wl_param_set = wl_param_mgr->mCurParams;
				LLWLParamSet wl_backup;
				if(!wl_param_mgr->getParamSet("LightShare-Backup", wl_backup)) {
					wl_param_mgr->addParamSet("LightShare-Backup", wl_param_set);
				}
				
				wl_param_set.setSunAngle(F_TWO_PI * wl->sunMoonPosiiton);
				wl_param_set.setEastAngle(F_TWO_PI * wl->eastAngle);
				wl_param_set.set("sunlight_color", wl->sunMoonColor.red * 3.0f, wl->sunMoonColor.green * 3.0f, wl->sunMoonColor.blue * 3.0f, wl->sunMoonColor.alpha * 3.0f);
				wl_param_set.set("ambient", wl->ambient.red * 3.0f, wl->ambient.green * 3.0f, wl->ambient.blue * 3.0f, wl->ambient.alpha * 3.0f);
				wl_param_set.set("blue_horizon", wl->horizon.red * 2.0f, wl->horizon.green *2.0f, wl->horizon.blue * 2.0f, wl->horizon.alpha * 2.0f);
				wl_param_set.set("blue_density", wl->blueDensity.red * 2.0f, wl->blueDensity.green * 2.0f, wl->blueDensity.blue * 2.0f, wl->blueDensity.alpha * 2.0f);
				wl_param_set.set("haze_horizon", wl->hazeHorizon, wl->hazeHorizon, wl->hazeHorizon, 1.f);
				wl_param_set.set("haze_density", wl->hazeDensity, wl->hazeDensity, wl->hazeDensity, 1.f);
				wl_param_set.set("cloud_shadow", wl->cloudCoverage, wl->cloudCoverage, wl->cloudCoverage, wl->cloudCoverage);
				wl_param_set.set("density_multiplier", wl->densityMultiplier / 1000.0f);
				wl_param_set.set("distance_multiplier", wl->distanceMultiplier, wl->distanceMultiplier, wl->distanceMultiplier, wl->distanceMultiplier);
				wl_param_set.set("max_y",(F32)wl->maxAltitude);
				wl_param_set.set("cloud_color", wl->cloudColor.red, wl->cloudColor.green, wl->cloudColor.blue, wl->cloudColor.alpha);
				wl_param_set.set("cloud_pos_density1", wl->cloudXYDensity.X, wl->cloudXYDensity.Y, wl->cloudXYDensity.Z);
				wl_param_set.set("cloud_pos_density2", wl->cloudDetailXYDensity.X, wl->cloudDetailXYDensity.Y, wl->cloudDetailXYDensity.Z);
				wl_param_set.set("cloud_scale", wl->cloudScale, 0.f, 0.f, 1.f);
				wl_param_set.set("gamma", wl->sceneGamma, wl->sceneGamma, wl->sceneGamma, 0.0f);
				wl_param_set.set("glow",(2 - wl->sunGlowSize) * 20 , 0.f, -wl->sunGlowFocus * 5);
				wl_param_set.setCloudScrollX(wl->cloudScrollX + 10.0f);
				wl_param_set.setCloudScrollY(wl->cloudScrollY + 10.0f);
				wl_param_set.setEnableCloudScrollX(!wl->cloudScrollXLock);
				wl_param_set.setEnableCloudScrollY(!wl->cloudScrollYLock);
				wl_param_set.setStarBrightness(wl->starBrightness);
				wl_param_mgr->removeParamSet("LightShare-CurrentRegion",true);
				wl_param_mgr->addParamSet(	"LightShare-CurrentRegion", wl_param_set);
				wl_param_mgr->savePreset( "LightShare-CurrentRegion");
				LLWLParamManager::getInstance()->mAnimator.mIsRunning = false;
				LLWLParamManager::getInstance()->mAnimator.mUseLindenTime = false;
				wl_param_mgr->loadPreset( "LightShare-CurrentRegion",true);	
			}
		}
	}
	else if (agent_id != gAgent.getID())
	{
		llwarns << "GenericMessage for wrong agent" << llendl;
		return;
	}
	else
	{
		std::string request;
		LLUUID invoice;
		LLDispatcher::sparam_t strings;
		LLDispatcher::unpackMessage(msg, request, invoice, strings);

		if(!gGenericDispatcher.dispatch(request, invoice, strings))
		{
			llwarns << "GenericMessage " << request << " failed to dispatch" 
				<< llendl;
		}
	}
}
