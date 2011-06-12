/** 
 * @file llviewershadermgr.cpp
 * @brief Viewer shader manager implementation.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * 
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llfeaturemanager.h"
#include "llviewershadermgr.h"

#include "llfile.h"
#include "llviewerwindow.h"
#include "llviewercontrol.h"
#include "pipeline.h"
#include "llworld.h"
#include "llwlparammanager.h"
#include "llwaterparammanager.h"
#include "llsky.h"
#include "llvosky.h"
#include "llrender.h"

#if LL_DARWIN
#include "OpenGL/OpenGL.h"
#endif

#ifdef LL_RELEASE_FOR_DOWNLOAD
#define UNIFORM_ERRS LL_WARNS_ONCE("Shader")
#else
#define UNIFORM_ERRS LL_ERRS("Shader")
#endif

// Lots of STL stuff in here, using namespace std to keep things more readable
using std::vector;
using std::pair;
using std::make_pair;
using std::string;

BOOL				LLViewerShaderMgr::sInitialized = FALSE;

LLVector4			gShinyOrigin;

 // WindLight shader handles
 // Make sure WL Sky is the first program
LLGLSLShader		gWLSkyProgram(LLViewerShaderMgr::SHADER_WINDLIGHT);
LLGLSLShader		gWLCloudProgram(LLViewerShaderMgr::SHADER_WINDLIGHT);

 //object shaders
LLGLSLShader		gObjectSimpleProgram(LLViewerShaderMgr::SHADER_OBJECT);
LLGLSLShader		gObjectSimpleWaterProgram(LLViewerShaderMgr::SHADER_OBJECT);
LLGLSLShader		gObjectFullbrightProgram(LLViewerShaderMgr::SHADER_OBJECT);
LLGLSLShader		gObjectFullbrightWaterProgram(LLViewerShaderMgr::SHADER_OBJECT);
 
LLGLSLShader		gObjectFullbrightShinyProgram(LLViewerShaderMgr::SHADER_OBJECT);
LLGLSLShader		gObjectFullbrightShinyWaterProgram(LLViewerShaderMgr::SHADER_OBJECT);
LLGLSLShader		gObjectShinyProgram(LLViewerShaderMgr::SHADER_OBJECT);
LLGLSLShader		gObjectShinyWaterProgram(LLViewerShaderMgr::SHADER_OBJECT);
 
 //environment shaders
LLGLSLShader		gTerrainProgram(LLViewerShaderMgr::SHADER_ENVIRONMENT);
LLGLSLShader		gTerrainWaterProgram(LLViewerShaderMgr::SHADER_WATER); //note, water.
LLGLSLShader		gWaterProgram(LLViewerShaderMgr::SHADER_WATER);
LLGLSLShader		gUnderWaterProgram(LLViewerShaderMgr::SHADER_WATER);
 
 //interface shaders
LLGLSLShader		gHighlightProgram(LLViewerShaderMgr::SHADER_INTERFACE);			//Not in mShaderList
 
 //avatar shader handles
LLGLSLShader		gAvatarProgram(LLViewerShaderMgr::SHADER_AVATAR);
LLGLSLShader		gAvatarWaterProgram(LLViewerShaderMgr::SHADER_AVATAR);
LLGLSLShader		gAvatarEyeballProgram(LLViewerShaderMgr::SHADER_AVATAR);
LLGLSLShader		gAvatarPickProgram(LLViewerShaderMgr::SHADER_AVATAR);			//Not in mShaderList
 
 // Effects Shaders
LLGLSLShader			gGlowProgram(LLViewerShaderMgr::SHADER_EFFECT);				//Not in mShaderList
LLGLSLShader			gGlowExtractProgram(LLViewerShaderMgr::SHADER_EFFECT);		//Not in mShaderList
LLGLSLShader			gPostColorFilterProgram(LLViewerShaderMgr::SHADER_EFFECT);	//Not in mShaderList
LLGLSLShader			gPostNightVisionProgram(LLViewerShaderMgr::SHADER_EFFECT);	//Not in mShaderList
LLGLSLShader			gPostGaussianBlurProgram(LLViewerShaderMgr::SHADER_EFFECT);	//Not in mShaderList
 
 // Deferred rendering shaders
LLGLSLShader			gDeferredImpostorProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredEdgeProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredWaterProgram(LLViewerShaderMgr::SHADER_DEFERRED); //calculatesAtmospherics
LLGLSLShader			gDeferredDiffuseProgram(LLViewerShaderMgr::SHADER_DEFERRED);//Not in mShaderList
LLGLSLShader			gDeferredBumpProgram(LLViewerShaderMgr::SHADER_DEFERRED);	//Not in mShaderList
LLGLSLShader			gDeferredTerrainProgram(LLViewerShaderMgr::SHADER_DEFERRED);//Not in mShaderList
LLGLSLShader			gDeferredTreeProgram(LLViewerShaderMgr::SHADER_DEFERRED);	//Not in mShaderList
LLGLSLShader			gDeferredAvatarProgram(LLViewerShaderMgr::SHADER_DEFERRED);	//Not in mShaderList
LLGLSLShader			gDeferredAvatarAlphaProgram(LLViewerShaderMgr::SHADER_DEFERRED); //calculatesAtmospherics
LLGLSLShader			gDeferredLightProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredMultiLightProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredSpotLightProgram(LLViewerShaderMgr::SHADER_DEFERRED); //Not in mShaderList
LLGLSLShader			gDeferredMultiSpotLightProgram(LLViewerShaderMgr::SHADER_DEFERRED); //Not in mShaderList
LLGLSLShader			gDeferredSunProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredBlurLightProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredSoftenProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredShadowProgram(LLViewerShaderMgr::SHADER_DEFERRED);		//Not in mShaderList
LLGLSLShader			gDeferredAvatarShadowProgram(LLViewerShaderMgr::SHADER_DEFERRED);//Not in mShaderList
LLGLSLShader			gDeferredAlphaProgram(LLViewerShaderMgr::SHADER_DEFERRED);		//calculatesAtmospherics
LLGLSLShader			gDeferredFullbrightProgram(LLViewerShaderMgr::SHADER_DEFERRED); //calculatesAtmospherics
LLGLSLShader			gDeferredGIProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredGIFinalProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredPostGIProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredPostProgram(LLViewerShaderMgr::SHADER_DEFERRED);
LLGLSLShader			gDeferredPostNoDoFProgram(LLViewerShaderMgr::SHADER_DEFERRED);

LLGLSLShader			gLuminanceGatherProgram(LLViewerShaderMgr::SHADER_DEFERRED);
//current avatar shader parameter pointer
GLint				gAvatarMatrixParam;

LLViewerShaderMgr::LLViewerShaderMgr() :
	mVertexShaderLevel(SHADER_COUNT, 0),
	mMaxAvatarShaderLevel(0)
{}

LLViewerShaderMgr::~LLViewerShaderMgr()
{
	mVertexShaderLevel.clear();
}

// static
LLViewerShaderMgr * LLViewerShaderMgr::instance()
{
	if(NULL == sInstance)
	{
		sInstance = new LLViewerShaderMgr();
	}

	return static_cast<LLViewerShaderMgr*>(sInstance);
}

void LLViewerShaderMgr::initAttribsAndUniforms(void)
{
	if (mReservedAttribs.empty())
	{
		mReservedAttribs.push_back("materialColor");
		mReservedAttribs.push_back("specularColor");
		mReservedAttribs.push_back("binormal");
		mReservedAttribs.push_back("object_weight");

		mAvatarAttribs.reserve(5);
		mAvatarAttribs.push_back("weight");
		mAvatarAttribs.push_back("clothing");
		mAvatarAttribs.push_back("gWindDir");
		mAvatarAttribs.push_back("gSinWaveParams");
		mAvatarAttribs.push_back("gGravity");

		mAvatarUniforms.push_back("matrixPalette");

		mReservedUniforms.reserve(24);
		mReservedUniforms.push_back("diffuseMap");
		mReservedUniforms.push_back("specularMap");
		mReservedUniforms.push_back("bumpMap");
		mReservedUniforms.push_back("environmentMap");
		mReservedUniforms.push_back("cloude_noise_texture");
		mReservedUniforms.push_back("fullbright");
		mReservedUniforms.push_back("lightnorm");
		mReservedUniforms.push_back("sunlight_color");
		mReservedUniforms.push_back("ambient");
		mReservedUniforms.push_back("blue_horizon");
		mReservedUniforms.push_back("blue_density");
		mReservedUniforms.push_back("haze_horizon");
		mReservedUniforms.push_back("haze_density");
		mReservedUniforms.push_back("cloud_shadow");
		mReservedUniforms.push_back("density_multiplier");
		mReservedUniforms.push_back("distance_multiplier");
		mReservedUniforms.push_back("max_y");
		mReservedUniforms.push_back("glow");
		mReservedUniforms.push_back("cloud_color");
		mReservedUniforms.push_back("cloud_pos_density1");
		mReservedUniforms.push_back("cloud_pos_density2");
		mReservedUniforms.push_back("cloud_scale");
		mReservedUniforms.push_back("gamma");
		mReservedUniforms.push_back("scene_light_strength");

		mReservedUniforms.push_back("depthMap");
		mReservedUniforms.push_back("shadowMap0");
		mReservedUniforms.push_back("shadowMap1");
		mReservedUniforms.push_back("shadowMap2");
		mReservedUniforms.push_back("shadowMap3");
		mReservedUniforms.push_back("shadowMap4");
		mReservedUniforms.push_back("shadowMap5");

		mReservedUniforms.push_back("normalMap");
		mReservedUniforms.push_back("positionMap");
		mReservedUniforms.push_back("diffuseRect");
		mReservedUniforms.push_back("specularRect");
		mReservedUniforms.push_back("noiseMap");
		mReservedUniforms.push_back("lightFunc");
		mReservedUniforms.push_back("lightMap");
		mReservedUniforms.push_back("luminanceMap");
		mReservedUniforms.push_back("giLightMap");
		mReservedUniforms.push_back("giMip");
		mReservedUniforms.push_back("edgeMap");
		mReservedUniforms.push_back("bloomMap");
		mReservedUniforms.push_back("sunLightMap");
		mReservedUniforms.push_back("localLightMap");
		mReservedUniforms.push_back("projectionMap");
		mReservedUniforms.push_back("diffuseGIMap");
		mReservedUniforms.push_back("specularGIMap");
		mReservedUniforms.push_back("normalGIMap");
		mReservedUniforms.push_back("minpGIMap");
		mReservedUniforms.push_back("maxpGIMap");
		mReservedUniforms.push_back("depthGIMap");
		mReservedUniforms.push_back("lastDiffuseGIMap");
		mReservedUniforms.push_back("lastNormalGIMap");
		mReservedUniforms.push_back("lastMinpGIMap");
		mReservedUniforms.push_back("lastMaxpGIMap");

		mWLUniforms.push_back("camPosLocal");

		mTerrainUniforms.reserve(5);
		mTerrainUniforms.push_back("detail_0");
		mTerrainUniforms.push_back("detail_1");
		mTerrainUniforms.push_back("detail_2");
		mTerrainUniforms.push_back("detail_3");
		mTerrainUniforms.push_back("alpha_ramp");

		mGlowUniforms.push_back("glowDelta");
		mGlowUniforms.push_back("glowStrength");

		mGlowExtractUniforms.push_back("minLuminance");
		mGlowExtractUniforms.push_back("maxExtractAlpha");
		mGlowExtractUniforms.push_back("lumWeights");
		mGlowExtractUniforms.push_back("warmthWeights");
		mGlowExtractUniforms.push_back("warmthAmount");

		mShinyUniforms.push_back("origin");

		mWaterUniforms.reserve(12);
		mWaterUniforms.push_back("screenTex");
		mWaterUniforms.push_back("screenDepth");
		mWaterUniforms.push_back("refTex");
		mWaterUniforms.push_back("eyeVec");
		mWaterUniforms.push_back("time");
		mWaterUniforms.push_back("d1");
		mWaterUniforms.push_back("d2");
		mWaterUniforms.push_back("lightDir");
		mWaterUniforms.push_back("specular");
		mWaterUniforms.push_back("lightExp");
		mWaterUniforms.push_back("fogCol");
		mWaterUniforms.push_back("kd");
		mWaterUniforms.push_back("refScale");
		mWaterUniforms.push_back("waterHeight");
	}	
}
	

//============================================================================
// Set Levels

S32 LLViewerShaderMgr::getVertexShaderLevel(S32 type)
{
	return LLPipeline::sDisableShaders ? 0 : mVertexShaderLevel[type];
}

//============================================================================
// Shader Management

void LLViewerShaderMgr::setShaders()
{
	//setShaders might be called redundantly by gSavedSettings, so return on reentrance
	static bool reentrance = false;
	
	if (!gPipeline.mInitialized || !sInitialized || reentrance)
	{
		return;
	}

	reentrance = true;
	
	initAttribsAndUniforms();
	gPipeline.releaseGLBuffers();

	if (gSavedSettings.getBOOL("VertexShaderEnable"))
	{
		LLPipeline::sWaterReflections = gGLManager.mHasCubeMap;
		LLPipeline::sRenderGlow = gSavedSettings.getBOOL("RenderGlow"); 
		LLPipeline::updateRenderDeferred();
	}
	else
	{
		LLPipeline::sRenderGlow = FALSE;
		LLPipeline::sWaterReflections = FALSE;
	}
	
	//hack to reset buffers that change behavior with shaders
	gPipeline.resetVertexBuffers();

	if (gViewerWindow)
	{
		gViewerWindow->setCursor(UI_CURSOR_WAIT);
	}

	// Lighting
	gPipeline.setLightingDetail(-1);

	// Shaders
	LL_INFOS("ShaderLoading") << "\n~~~~~~~~~~~~~~~~~~\n Loading Shaders:\n~~~~~~~~~~~~~~~~~~" << LL_ENDL;
	for (S32 i = 0; i < SHADER_COUNT; i++)
	{
		mVertexShaderLevel[i] = 0;
	}
	mMaxAvatarShaderLevel = 0;

	if (LLFeatureManager::getInstance()->isFeatureAvailable("VertexShaderEnable") 
		&& gSavedSettings.getBOOL("VertexShaderEnable"))
	{
		S32 light_class = 2;
		S32 env_class = 2;
		S32 obj_class = 2;
		S32 effect_class = 2;
		S32 wl_class = 2;
		S32 water_class = 2;
		S32 deferred_class = 0;

		if (LLFeatureManager::getInstance()->isFeatureAvailable("RenderDeferred") &&
		    gSavedSettings.getBOOL("RenderDeferred") &&
			LLFeatureManager::getInstance()->isFeatureAvailable("WindLightUseAtmosShaders") &&
			gSavedSettings.getBOOL("WindLightUseAtmosShaders"))
		{
			if (gSavedSettings.getS32("RenderShadowDetail") > 0)
			{
				if (gSavedSettings.getBOOL("RenderDeferredGI"))
				{ //shadows + gi
					deferred_class = 3;
				}
				else
				{ //shadows
					deferred_class = 2;
				}
			}
			else
			{ //no shadows
				deferred_class = 1;
			}

			//make sure framebuffer objects are enabled
			gSavedSettings.setBOOL("RenderUseFBO", TRUE);

			//make sure hardware skinning is enabled
			gSavedSettings.setBOOL("RenderAvatarVP", TRUE);
		}


		if (!(LLFeatureManager::getInstance()->isFeatureAvailable("WindLightUseAtmosShaders")
			  && gSavedSettings.getBOOL("WindLightUseAtmosShaders")))
		{
			// user has disabled WindLight in their settings, downgrade
			// windlight shaders to stub versions.
			wl_class = 1;
		}

		if(!gSavedSettings.getBOOL("EnableRippleWater"))
		{
			water_class = 0;
		}

		// Trigger a full rebuild of the fallback skybox / cubemap if we've toggled windlight shaders
		if (mVertexShaderLevel[SHADER_WINDLIGHT] != wl_class && gSky.mVOSkyp.notNull())
		{
			gSky.mVOSkyp->forceSkyUpdate();
		}

		// Load lighting shaders
		mVertexShaderLevel[SHADER_LIGHTING] = light_class;
		mVertexShaderLevel[SHADER_INTERFACE] = light_class;
		mVertexShaderLevel[SHADER_ENVIRONMENT] = env_class;
		mVertexShaderLevel[SHADER_WATER] = water_class;
		mVertexShaderLevel[SHADER_OBJECT] = obj_class;
		mVertexShaderLevel[SHADER_EFFECT] = effect_class;
		mVertexShaderLevel[SHADER_WINDLIGHT] = wl_class;
		mVertexShaderLevel[SHADER_DEFERRED] = deferred_class;

		BOOL loaded = loadBasicShaders();

		if (loaded)
		{
			gPipeline.mVertexShadersEnabled = TRUE;
			gPipeline.mVertexShadersLoaded = 1;

			// Load all shaders to set max levels
			loadShadersEnvironment();
			loadShadersWater();
			loadShadersObject();
			loadShadersWindLight();
			loadShadersEffects();
			loadShadersInterface();
					
#if 0 && LL_DARWIN // force avatar shaders off for mac
			mVertexShaderLevel[SHADER_AVATAR] = 0;
			sMaxAvatarShaderLevel = 0;
#else
			if (gSavedSettings.getBOOL("RenderAvatarVP"))
			{
				BOOL avatar_cloth = gSavedSettings.getBOOL("RenderAvatarCloth");
				S32 avatar_class = 1;
				
				// cloth is a class3 shader
				if(avatar_cloth)
				{
					avatar_class = 3;
				}

				// Set the actual level
				mVertexShaderLevel[SHADER_AVATAR] = avatar_class;
				loadShadersAvatar();
				if (mVertexShaderLevel[SHADER_AVATAR] != avatar_class)
				{
					if (mVertexShaderLevel[SHADER_AVATAR] == 0)
					{
						gSavedSettings.setBOOL("RenderAvatarVP", FALSE);
					}
					if(llmax(mVertexShaderLevel[SHADER_AVATAR]-1,0) >= 3)
					{
						avatar_cloth = true;
					}
					else
					{
						avatar_cloth = false;
					}
					gSavedSettings.setBOOL("RenderAvatarCloth", avatar_cloth);
				}
			}
			else
			{
				mVertexShaderLevel[SHADER_AVATAR] = 0;
				gSavedSettings.setBOOL("RenderAvatarCloth", FALSE);
				loadShadersAvatar(); // unloads
			}

			if (!loadShadersDeferred())
			{
				gSavedSettings.setBOOL("RenderDeferred", FALSE);
			}
#endif
		}
		else
		{
			gPipeline.mVertexShadersEnabled = FALSE;
			gPipeline.mVertexShadersLoaded = 0;
			for (S32 i = 0; i < SHADER_COUNT; i++)
				mVertexShaderLevel[i] = 0;
		}

		//Flag base shader objects for deletion
		//Don't worry-- they won't be deleted until no programs refrence them.
		std::map<std::string, GLhandleARB>::iterator it = mShaderObjects.begin();
		for(; it!=mShaderObjects.end();++it)
			if(it->second)
				glDeleteObjectARB(it->second);
		mShaderObjects.clear(); 
	}
	else
	{
		gPipeline.mVertexShadersEnabled = FALSE;
		gPipeline.mVertexShadersLoaded = 0;
		for (S32 i = 0; i < SHADER_COUNT; i++)
				mVertexShaderLevel[i] = 0;
	}
	
	if (gViewerWindow)
	{
		gViewerWindow->setCursor(UI_CURSOR_ARROW);
	}
	gPipeline.createGLBuffers();
	reentrance = false;
}

void LLViewerShaderMgr::unloadShaders()
{
	//Instead of manually unloading, shaders are now automatically accumulated in a list.
	//Simply iterate and unload.
	std::vector<LLGLSLShader *> &shader_list = LLShaderMgr::getGlobalShaderList();
	for(std::vector<LLGLSLShader *>::iterator it=shader_list.begin();it!=shader_list.end();++it)
		(*it)->unload();

	for (S32 i = 0; i < SHADER_COUNT; i++)
		mVertexShaderLevel[i] = 0;

	gPipeline.mVertexShadersLoaded = 0;
}

BOOL LLViewerShaderMgr::loadBasicShaders()
{
	// Load basic dependency shaders first
	// All of these have to load for any shaders to function
	
#if LL_DARWIN // Mac can't currently handle all 8 lights, 
	S32 sum_lights_class = 2;
#else 
	S32 sum_lights_class = 3;

	// class one cards will get the lower sum lights
	// class zero we're not going to think about
	// since a class zero card COULD be a ridiculous new card
	// and old cards should have the features masked
	if(LLFeatureManager::getInstance()->getGPUClass() == GPU_CLASS_1)
	{
		sum_lights_class = 2;
	}
#endif

	// If we have sun and moon only checked, then only sum those lights.
	if (gPipeline.getLightingDetail() == 0)
	{
		sum_lights_class = 1;
	}

	// Use the feature table to mask out the max light level to use.  Also make sure it's at least 1.
	S32 max_light_class = gSavedSettings.getS32("RenderShaderLightingMaxLevel");
	sum_lights_class = llclamp(sum_lights_class, 1, max_light_class);

	// Load the Basic Vertex Shaders at the appropriate level. 
	// (in order of shader function call depth for reference purposes, deepest level first)

	vector< pair<string, S32> > shaders;
	shaders.reserve(10);
	shaders.push_back( make_pair( "windlight/atmosphericsVarsV.glsl",		mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( make_pair( "windlight/atmosphericsHelpersV.glsl",	mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( make_pair( "lighting/lightFuncV.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/sumLightsV.glsl",				sum_lights_class ) );
	shaders.push_back( make_pair( "lighting/lightV.glsl",					mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/lightFuncSpecularV.glsl",		mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/sumLightsSpecularV.glsl",		sum_lights_class ) );
	shaders.push_back( make_pair( "lighting/lightSpecularV.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "windlight/atmosphericsV.glsl",			mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( make_pair( "avatar/avatarSkinV.glsl",				1 ) );

	// We no longer have to bind the shaders to global glhandles, they are automatically added to a map now.
	for (U32 i = 0; i < shaders.size(); i++)
	{
		// Note usage of GL_VERTEX_SHADER_ARB
		if (loadShaderFile(shaders[i].first, shaders[i].second, GL_VERTEX_SHADER_ARB) == 0)
		{
			return FALSE;
		}
	}

	// Load the Basic Fragment Shaders at the appropriate level. 
	// (in order of shader function call depth for reference purposes, deepest level first)

	shaders.clear();
	shaders.reserve(13);
	shaders.push_back( make_pair( "windlight/atmosphericsVarsF.glsl",		mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( make_pair( "windlight/gammaF.glsl",					mVertexShaderLevel[SHADER_WINDLIGHT]) );
	shaders.push_back( make_pair( "windlight/atmosphericsF.glsl",			mVertexShaderLevel[SHADER_WINDLIGHT] ) );
	shaders.push_back( make_pair( "windlight/transportF.glsl",				mVertexShaderLevel[SHADER_WINDLIGHT] ) );	
	shaders.push_back( make_pair( "environment/waterFogF.glsl",				mVertexShaderLevel[SHADER_WATER] ) );
	shaders.push_back( make_pair( "lighting/lightF.glsl",					mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/lightFullbrightF.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/lightWaterF.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/lightFullbrightWaterF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/lightShinyF.glsl",				mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/lightFullbrightShinyF.glsl",	mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/lightShinyWaterF.glsl",			mVertexShaderLevel[SHADER_LIGHTING] ) );
	shaders.push_back( make_pair( "lighting/lightFullbrightShinyWaterF.glsl", mVertexShaderLevel[SHADER_LIGHTING] ) );
	
	for (U32 i = 0; i < shaders.size(); i++)
	{
		// Note usage of GL_FRAGMENT_SHADER_ARB
		if (loadShaderFile(shaders[i].first, shaders[i].second, GL_FRAGMENT_SHADER_ARB) == 0)
		{
			return FALSE;
		}
	}

	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersEnvironment()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_ENVIRONMENT] == 0)
	{
		unloadShaderClass(SHADER_ENVIRONMENT);
		return FALSE;
	}

	if (success)
	{
		gTerrainProgram.mName = "Terrain Shader";
		gTerrainProgram.mFeatures.calculatesLighting = true;
		gTerrainProgram.mFeatures.calculatesAtmospherics = true;
		gTerrainProgram.mFeatures.hasAtmospherics = true;
		gTerrainProgram.mFeatures.hasGamma = true;
		gTerrainProgram.mShaderFiles.clear();
		gTerrainProgram.mShaderFiles.push_back(make_pair("environment/terrainV.glsl", GL_VERTEX_SHADER_ARB));
		gTerrainProgram.mShaderFiles.push_back(make_pair("environment/terrainF.glsl", GL_FRAGMENT_SHADER_ARB));
		gTerrainProgram.mShaderLevel = mVertexShaderLevel[SHADER_ENVIRONMENT];
		success = gTerrainProgram.createShader(NULL, &mTerrainUniforms);
	}

	if (!success)
	{
		mVertexShaderLevel[SHADER_ENVIRONMENT] = 0;
		return FALSE;
	}
	
	LLWorld::getInstance()->updateWaterObjects();
	
	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersWater()
{
	BOOL success = TRUE;
	BOOL terrainWaterSuccess = TRUE;

	if (mVertexShaderLevel[SHADER_WATER] == 0)
	{
		unloadShaderClass(SHADER_WATER);
		return FALSE;
	}

	if (success)
	{
		// load water shader
		gWaterProgram.mName = "Water Shader";
		gWaterProgram.mFeatures.calculatesAtmospherics = true;
		gWaterProgram.mFeatures.hasGamma = true;
		gWaterProgram.mFeatures.hasTransport = true;
		gWaterProgram.mShaderFiles.clear();
		gWaterProgram.mShaderFiles.push_back(make_pair("environment/waterV.glsl", GL_VERTEX_SHADER_ARB));
		gWaterProgram.mShaderFiles.push_back(make_pair("environment/waterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_WATER];
		success = gWaterProgram.createShader(NULL, &mWaterUniforms);
	}

	if (success)
	{
		//load under water vertex shader
		gUnderWaterProgram.mName = "Underwater Shader";
		gUnderWaterProgram.mFeatures.calculatesAtmospherics = true;
		gUnderWaterProgram.mShaderFiles.clear();
		gUnderWaterProgram.mShaderFiles.push_back(make_pair("environment/waterV.glsl", GL_VERTEX_SHADER_ARB));
		gUnderWaterProgram.mShaderFiles.push_back(make_pair("environment/underWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gUnderWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_WATER];
		gUnderWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		
		success = gUnderWaterProgram.createShader(NULL, &mWaterUniforms);
	}

	if (success)
	{
		//load terrain water shader
		gTerrainWaterProgram.mName = "Terrain Water Shader";
		gTerrainWaterProgram.mFeatures.calculatesLighting = true;
		gTerrainWaterProgram.mFeatures.calculatesAtmospherics = true;
		gTerrainWaterProgram.mFeatures.hasAtmospherics = true;
		gTerrainWaterProgram.mFeatures.hasWaterFog = true;
		gTerrainWaterProgram.mShaderFiles.clear();
		gTerrainWaterProgram.mShaderFiles.push_back(make_pair("environment/terrainV.glsl", GL_VERTEX_SHADER_ARB));
		gTerrainWaterProgram.mShaderFiles.push_back(make_pair("environment/terrainWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gTerrainWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_ENVIRONMENT];
		gTerrainWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		terrainWaterSuccess = gTerrainWaterProgram.createShader(NULL, &mTerrainUniforms);
	}	

	/// Keep track of water shader levels
	if (gWaterProgram.mShaderLevel != mVertexShaderLevel[SHADER_WATER]
		|| gUnderWaterProgram.mShaderLevel != mVertexShaderLevel[SHADER_WATER])
	{
		mVertexShaderLevel[SHADER_WATER] = llmin(gWaterProgram.mShaderLevel, gUnderWaterProgram.mShaderLevel);
	}

	if (!success)
	{
		mVertexShaderLevel[SHADER_WATER] = 0;
		return FALSE;
	}

	// if we failed to load the terrain water shaders and we need them (using class2 water),
	// then drop down to class1 water.
	if (mVertexShaderLevel[SHADER_WATER] > 1 && !terrainWaterSuccess)
	{
		mVertexShaderLevel[SHADER_WATER]--;
		return loadShadersWater();
	}
	
	LLWorld::getInstance()->updateWaterObjects();

	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersEffects()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_EFFECT] == 0)
	{
		unloadShaderClass(SHADER_EFFECT);
		return FALSE;
	}

	if(LLPipeline::sRenderGlow)
	{
		if (success)
		{
			gGlowProgram.mName = "Glow Shader (Post)";
			gGlowProgram.mShaderFiles.clear();
			gGlowProgram.mShaderFiles.push_back(make_pair("effects/glowV.glsl", GL_VERTEX_SHADER_ARB));
			gGlowProgram.mShaderFiles.push_back(make_pair("effects/glowF.glsl", GL_FRAGMENT_SHADER_ARB));
			gGlowProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
			success = gGlowProgram.createShader(NULL, &mGlowUniforms);
			LLPipeline::sRenderGlow = success;
		}
		
		if (success)
		{
			gGlowExtractProgram.mName = "Glow Extract Shader (Post)";
			gGlowExtractProgram.mShaderFiles.clear();
			gGlowExtractProgram.mShaderFiles.push_back(make_pair("effects/glowExtractV.glsl", GL_VERTEX_SHADER_ARB));
			gGlowExtractProgram.mShaderFiles.push_back(make_pair("effects/glowExtractF.glsl", GL_FRAGMENT_SHADER_ARB));
			gGlowExtractProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
			success = gGlowExtractProgram.createShader(NULL, &mGlowExtractUniforms);
			LLPipeline::sRenderGlow = success;
		}
	}
	
#if 1
	// disabling loading of postprocess shaders until we fix
	// ATI sampler2DRect compatibility.
	
	//load Color Filter Shader
	//if (success)
	{
		vector<string> shaderUniforms;
		shaderUniforms.reserve(7);
		shaderUniforms.push_back("RenderTexture");
		shaderUniforms.push_back("gamma");
		shaderUniforms.push_back("brightness");
		shaderUniforms.push_back("contrast");
		shaderUniforms.push_back("contrastBase");
		shaderUniforms.push_back("saturation");
		shaderUniforms.push_back("lumWeights");

		gPostColorFilterProgram.mName = "Color Filter Shader (Post)";
		gPostColorFilterProgram.mShaderFiles.clear();
		gPostColorFilterProgram.mShaderFiles.push_back(make_pair("effects/colorFilterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gPostColorFilterProgram.mShaderFiles.push_back(make_pair("effects/drawQuadV.glsl", GL_VERTEX_SHADER_ARB));
		gPostColorFilterProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
		/*success = */gPostColorFilterProgram.createShader(NULL, &shaderUniforms);
	}

	//load Night Vision Shader
	//if (success)
	{
		vector<string> shaderUniforms;
		shaderUniforms.reserve(5);
		shaderUniforms.push_back("RenderTexture");
		shaderUniforms.push_back("NoiseTexture");
		shaderUniforms.push_back("brightMult");
		shaderUniforms.push_back("noiseStrength");
		shaderUniforms.push_back("lumWeights");

		gPostNightVisionProgram.mName = "Night Vision Shader (Post)";
		gPostNightVisionProgram.mShaderFiles.clear();
		gPostNightVisionProgram.mShaderFiles.push_back(make_pair("effects/nightVisionF.glsl", GL_FRAGMENT_SHADER_ARB));
		gPostNightVisionProgram.mShaderFiles.push_back(make_pair("effects/drawQuadV.glsl", GL_VERTEX_SHADER_ARB));
		gPostNightVisionProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
		/*success = */gPostNightVisionProgram.createShader(NULL, &shaderUniforms);
	}

	//if (success)
	{
		vector<string> shaderUniforms;
		shaderUniforms.reserve(2);
		shaderUniforms.push_back("RenderTexture");
		shaderUniforms.push_back("horizontalPass");

		gPostGaussianBlurProgram.mName = "Gaussian Blur Shader (Post)";
		gPostGaussianBlurProgram.mShaderFiles.clear();
		gPostGaussianBlurProgram.mShaderFiles.push_back(make_pair("effects/gaussBlurF.glsl", GL_FRAGMENT_SHADER_ARB));
		gPostGaussianBlurProgram.mShaderFiles.push_back(make_pair("effects/drawQuadV.glsl", GL_VERTEX_SHADER_ARB));
		gPostGaussianBlurProgram.mShaderLevel = mVertexShaderLevel[SHADER_EFFECT];
		/*success = */gPostGaussianBlurProgram.createShader(NULL, &shaderUniforms);
	}

	
	#endif

	return success;

}

BOOL LLViewerShaderMgr::loadShadersDeferred()
{
	if (mVertexShaderLevel[SHADER_DEFERRED] == 0)
	{
		unloadShaderClass(SHADER_DEFERRED);
		return FALSE;
	}

	mVertexShaderLevel[SHADER_AVATAR] = 1;

	BOOL success = TRUE;

	if (success)
	{
		gDeferredDiffuseProgram.mName = "Deferred Diffuse Shader";
		gDeferredDiffuseProgram.mShaderFiles.clear();
		gDeferredDiffuseProgram.mShaderFiles.push_back(make_pair("deferred/diffuseV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredDiffuseProgram.mShaderFiles.push_back(make_pair("deferred/diffuseF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredDiffuseProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredDiffuseProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredBumpProgram.mName = "Deferred Bump Shader";
		gDeferredBumpProgram.mShaderFiles.clear();
		gDeferredBumpProgram.mShaderFiles.push_back(make_pair("deferred/bumpV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredBumpProgram.mShaderFiles.push_back(make_pair("deferred/bumpF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredBumpProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredBumpProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredTreeProgram.mName = "Deferred Tree Shader";
		gDeferredTreeProgram.mShaderFiles.clear();
		gDeferredTreeProgram.mShaderFiles.push_back(make_pair("deferred/treeV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredTreeProgram.mShaderFiles.push_back(make_pair("deferred/treeF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredTreeProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredTreeProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredImpostorProgram.mName = "Deferred Impostor Shader";
		gDeferredImpostorProgram.mShaderFiles.clear();
		gDeferredImpostorProgram.mShaderFiles.push_back(make_pair("deferred/impostorV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredImpostorProgram.mShaderFiles.push_back(make_pair("deferred/impostorF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredImpostorProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredImpostorProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredLightProgram.mName = "Deferred Light Shader";
		gDeferredLightProgram.mShaderFiles.clear();
		gDeferredLightProgram.mShaderFiles.push_back(make_pair("deferred/pointLightV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredLightProgram.mShaderFiles.push_back(make_pair("deferred/pointLightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredLightProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredLightProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredMultiLightProgram.mName = "Deferred MultiLight Shader";
		gDeferredMultiLightProgram.mShaderFiles.clear();
		gDeferredMultiLightProgram.mShaderFiles.push_back(make_pair("deferred/multiPointLightV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredMultiLightProgram.mShaderFiles.push_back(make_pair("deferred/multiPointLightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredMultiLightProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredMultiLightProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredSpotLightProgram.mName = "Deferred SpotLight Shader";
		gDeferredSpotLightProgram.mShaderFiles.clear();
		gDeferredSpotLightProgram.mShaderFiles.push_back(make_pair("deferred/pointLightV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredSpotLightProgram.mShaderFiles.push_back(make_pair("deferred/multiSpotLightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredSpotLightProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredSpotLightProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredMultiSpotLightProgram.mName = "Deferred MultiSpotLight Shader";
		gDeferredMultiSpotLightProgram.mShaderFiles.clear();
		gDeferredMultiSpotLightProgram.mShaderFiles.push_back(make_pair("deferred/pointLightV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredMultiSpotLightProgram.mShaderFiles.push_back(make_pair("deferred/multiSpotLightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredMultiSpotLightProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredMultiSpotLightProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		std::string fragment;

		if (gSavedSettings.getBOOL("RenderDeferredSSAO"))
		{
			fragment = "deferred/sunLightSSAOF.glsl";
		}
		else
		{
			fragment = "deferred/sunLightF.glsl";
		}

		gDeferredSunProgram.mName = "Deferred Sun Shader";
		gDeferredSunProgram.mShaderFiles.clear();
		gDeferredSunProgram.mShaderFiles.push_back(make_pair("deferred/sunLightV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredSunProgram.mShaderFiles.push_back(make_pair(fragment, GL_FRAGMENT_SHADER_ARB));
		gDeferredSunProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredSunProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredBlurLightProgram.mName = "Deferred Blur Light Shader";
		gDeferredBlurLightProgram.mShaderFiles.clear();
		gDeferredBlurLightProgram.mShaderFiles.push_back(make_pair("deferred/blurLightV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredBlurLightProgram.mShaderFiles.push_back(make_pair("deferred/blurLightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredBlurLightProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredBlurLightProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredAlphaProgram.mName = "Deferred Alpha Shader";
		gDeferredAlphaProgram.mFeatures.calculatesLighting = true;
		gDeferredAlphaProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredAlphaProgram.mFeatures.hasGamma = true;
		gDeferredAlphaProgram.mFeatures.hasAtmospherics = true;
		gDeferredAlphaProgram.mFeatures.hasLighting = true;
		gDeferredAlphaProgram.mShaderFiles.clear();
		gDeferredAlphaProgram.mShaderFiles.push_back(make_pair("deferred/alphaV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredAlphaProgram.mShaderFiles.push_back(make_pair("deferred/alphaF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredAlphaProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredAlphaProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredFullbrightProgram.mName = "Deferred Fullbright Shader";
		gDeferredFullbrightProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredFullbrightProgram.mFeatures.hasGamma = true;
		gDeferredFullbrightProgram.mFeatures.hasTransport = true;
		gDeferredFullbrightProgram.mFeatures.isFullbright = true;
		gDeferredFullbrightProgram.mShaderFiles.clear();
		gDeferredFullbrightProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredFullbrightProgram.mShaderFiles.push_back(make_pair("deferred/fullbrightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredFullbrightProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredFullbrightProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		// load water shader
		gDeferredWaterProgram.mName = "Deferred Water Shader";
		gDeferredWaterProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredWaterProgram.mFeatures.hasGamma = true;
		gDeferredWaterProgram.mFeatures.hasTransport = true;
		gDeferredWaterProgram.mShaderFiles.clear();
		gDeferredWaterProgram.mShaderFiles.push_back(make_pair("deferred/waterV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredWaterProgram.mShaderFiles.push_back(make_pair("deferred/waterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredWaterProgram.createShader(NULL, &mWaterUniforms);
	}

	if (success)
	{
		std::string fragment;

		if (mVertexShaderLevel[SHADER_DEFERRED] < 2 && !gSavedSettings.getBOOL("RenderDeferredSSAO"))
		{
			fragment = "deferred/softenLightNoSSAOF.glsl";
		}
		else
		{
			fragment = "deferred/softenLightF.glsl";
		}

		gDeferredSoftenProgram.mName = "Deferred Soften Shader";
		gDeferredSoftenProgram.mShaderFiles.clear();
		gDeferredSoftenProgram.mShaderFiles.push_back(make_pair("deferred/softenLightV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredSoftenProgram.mShaderFiles.push_back(make_pair(fragment, GL_FRAGMENT_SHADER_ARB));
		gDeferredSoftenProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredSoftenProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredShadowProgram.mName = "Deferred Shadow Shader";
		gDeferredShadowProgram.mShaderFiles.clear();
		gDeferredShadowProgram.mShaderFiles.push_back(make_pair("deferred/shadowV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredShadowProgram.mShaderFiles.push_back(make_pair("deferred/shadowF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredShadowProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredShadowProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredAvatarShadowProgram.mName = "Deferred Avatar Shadow Shader";
		gDeferredAvatarShadowProgram.mFeatures.hasSkinning = true;
		gDeferredAvatarShadowProgram.mShaderFiles.clear();
		gDeferredAvatarShadowProgram.mShaderFiles.push_back(make_pair("deferred/avatarShadowV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredAvatarShadowProgram.mShaderFiles.push_back(make_pair("deferred/avatarShadowF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredAvatarShadowProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredAvatarShadowProgram.createShader(&mAvatarAttribs, &mAvatarUniforms);
	}

	if (success)
	{
		gTerrainProgram.mName = "Deferred Terrain Shader";
		gDeferredTerrainProgram.mShaderFiles.clear();
		gDeferredTerrainProgram.mShaderFiles.push_back(make_pair("deferred/terrainV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredTerrainProgram.mShaderFiles.push_back(make_pair("deferred/terrainF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredTerrainProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredTerrainProgram.createShader(NULL, &mTerrainUniforms);
	}

	if (success)
	{
		gDeferredAvatarProgram.mName = "Avatar Shader";
		gDeferredAvatarProgram.mFeatures.hasSkinning = true;
		gDeferredAvatarProgram.mShaderFiles.clear();
		gDeferredAvatarProgram.mShaderFiles.push_back(make_pair("deferred/avatarV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredAvatarProgram.mShaderFiles.push_back(make_pair("deferred/avatarF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredAvatarProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredAvatarProgram.createShader(&mAvatarAttribs, &mAvatarUniforms);
	}

	if (success)
	{
		gDeferredAvatarAlphaProgram.mName = "Avatar Alpha Shader";
		gDeferredAvatarAlphaProgram.mFeatures.hasSkinning = true;
		gDeferredAvatarAlphaProgram.mFeatures.calculatesLighting = true;
		gDeferredAvatarAlphaProgram.mFeatures.calculatesAtmospherics = true;
		gDeferredAvatarAlphaProgram.mFeatures.hasGamma = true;
		gDeferredAvatarAlphaProgram.mFeatures.hasAtmospherics = true;
		gDeferredAvatarAlphaProgram.mFeatures.hasLighting = true;
		gDeferredAvatarAlphaProgram.mShaderFiles.clear();
		gDeferredAvatarAlphaProgram.mShaderFiles.push_back(make_pair("deferred/avatarAlphaV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredAvatarAlphaProgram.mShaderFiles.push_back(make_pair("deferred/alphaF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredAvatarAlphaProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredAvatarAlphaProgram.createShader(&mAvatarAttribs, &mAvatarUniforms);
	}

	if (success)
	{
		gDeferredPostProgram.mName = "Deferred Post Shader";
		gDeferredPostProgram.mShaderFiles.clear();
		gDeferredPostProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredPostProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredPostProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredPostProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gDeferredPostNoDoFProgram.mName = "Deferred Post Shader";
		gDeferredPostNoDoFProgram.mShaderFiles.clear();
		gDeferredPostNoDoFProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredV.glsl", GL_VERTEX_SHADER_ARB));
		gDeferredPostNoDoFProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredNoDoFF.glsl", GL_FRAGMENT_SHADER_ARB));
		gDeferredPostNoDoFProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
		success = gDeferredPostNoDoFProgram.createShader(NULL, NULL);
	}

	if (mVertexShaderLevel[SHADER_DEFERRED] > 1)
	{
		if (success)
		{
			gDeferredEdgeProgram.mName = "Deferred Edge Shader";
			gDeferredEdgeProgram.mShaderFiles.clear();
			gDeferredEdgeProgram.mShaderFiles.push_back(make_pair("deferred/edgeV.glsl", GL_VERTEX_SHADER_ARB));
			gDeferredEdgeProgram.mShaderFiles.push_back(make_pair("deferred/edgeF.glsl", GL_FRAGMENT_SHADER_ARB));
			gDeferredEdgeProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
			success = gDeferredEdgeProgram.createShader(NULL, NULL);
		}
	}

	if (mVertexShaderLevel[SHADER_DEFERRED] > 2)
	{
		if (success)
		{
			gDeferredPostProgram.mName = "Deferred Post Shader";
			gDeferredPostProgram.mShaderFiles.clear();
			gDeferredPostProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredV.glsl", GL_VERTEX_SHADER_ARB));
			gDeferredPostProgram.mShaderFiles.push_back(make_pair("deferred/postDeferredF.glsl", GL_FRAGMENT_SHADER_ARB));
			gDeferredPostProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
			success = gDeferredPostProgram.createShader(NULL, NULL);
		}

		if (success)
		{
			gDeferredPostGIProgram.mName = "Deferred Post GI Shader";
			gDeferredPostGIProgram.mShaderFiles.clear();
			gDeferredPostGIProgram.mShaderFiles.push_back(make_pair("deferred/postgiV.glsl", GL_VERTEX_SHADER_ARB));
			gDeferredPostGIProgram.mShaderFiles.push_back(make_pair("deferred/postgiF.glsl", GL_FRAGMENT_SHADER_ARB));
			gDeferredPostGIProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
			success = gDeferredPostGIProgram.createShader(NULL, NULL);
		}

		if (success)
		{
			gDeferredGIProgram.mName = "Deferred GI Shader";
			gDeferredGIProgram.mShaderFiles.clear();
			gDeferredGIProgram.mShaderFiles.push_back(make_pair("deferred/giV.glsl", GL_VERTEX_SHADER_ARB));
			gDeferredGIProgram.mShaderFiles.push_back(make_pair("deferred/giF.glsl", GL_FRAGMENT_SHADER_ARB));
			gDeferredGIProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
			success = gDeferredGIProgram.createShader(NULL, NULL);
		}

		if (success)
		{
			gDeferredGIFinalProgram.mName = "Deferred GI Final Shader";
			gDeferredGIFinalProgram.mShaderFiles.clear();
			gDeferredGIFinalProgram.mShaderFiles.push_back(make_pair("deferred/giFinalV.glsl", GL_VERTEX_SHADER_ARB));
			gDeferredGIFinalProgram.mShaderFiles.push_back(make_pair("deferred/giFinalF.glsl", GL_FRAGMENT_SHADER_ARB));
			gDeferredGIFinalProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
			success = gDeferredGIFinalProgram.createShader(NULL, NULL);
		}

		if (success)
		{
			gLuminanceGatherProgram.mName = "Luminance Gather Shader";
			gLuminanceGatherProgram.mShaderFiles.clear();
			gLuminanceGatherProgram.mShaderFiles.push_back(make_pair("deferred/luminanceV.glsl", GL_VERTEX_SHADER_ARB));
			gLuminanceGatherProgram.mShaderFiles.push_back(make_pair("deferred/luminanceF.glsl", GL_FRAGMENT_SHADER_ARB));
			gLuminanceGatherProgram.mShaderLevel = mVertexShaderLevel[SHADER_DEFERRED];
			success = gLuminanceGatherProgram.createShader(NULL, NULL);
		}
	}

	return success;
}

BOOL LLViewerShaderMgr::loadShadersObject()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_OBJECT] == 0)
	{
		unloadShaderClass(SHADER_OBJECT);
		return FALSE;
	}

	if (success)
	{
		gObjectSimpleProgram.mName = "Simple Shader";
		gObjectSimpleProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleProgram.mFeatures.hasGamma = true;
		gObjectSimpleProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleProgram.mFeatures.hasLighting = true;
		gObjectSimpleProgram.mShaderFiles.clear();
		gObjectSimpleProgram.mShaderFiles.push_back(make_pair("objects/simpleV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectSimpleProgram.mShaderFiles.push_back(make_pair("objects/simpleF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectSimpleProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectSimpleProgram.createShader(NULL, NULL);
	}
	
	if (success)
	{
		gObjectSimpleWaterProgram.mName = "Simple Water Shader";
		gObjectSimpleWaterProgram.mFeatures.calculatesLighting = true;
		gObjectSimpleWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectSimpleWaterProgram.mFeatures.hasWaterFog = true;
		gObjectSimpleWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectSimpleWaterProgram.mFeatures.hasLighting = true;
		gObjectSimpleWaterProgram.mShaderFiles.clear();
		gObjectSimpleWaterProgram.mShaderFiles.push_back(make_pair("objects/simpleV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectSimpleWaterProgram.mShaderFiles.push_back(make_pair("objects/simpleWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectSimpleWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectSimpleWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectSimpleWaterProgram.createShader(NULL, NULL);
	}
	
	if (success)
	{
		gObjectFullbrightProgram.mName = "Fullbright Shader";
		gObjectFullbrightProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightProgram.mFeatures.hasGamma = true;
		gObjectFullbrightProgram.mFeatures.hasTransport = true;
		gObjectFullbrightProgram.mFeatures.isFullbright = true;
		gObjectFullbrightProgram.mShaderFiles.clear();
		gObjectFullbrightProgram.mShaderFiles.push_back(make_pair("objects/fullbrightV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectFullbrightProgram.mShaderFiles.push_back(make_pair("objects/fullbrightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectFullbrightProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectFullbrightProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gObjectFullbrightWaterProgram.mName = "Fullbright Water Shader";
		gObjectFullbrightWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightWaterProgram.mFeatures.isFullbright = true;
		gObjectFullbrightWaterProgram.mFeatures.hasWaterFog = true;		
		gObjectFullbrightWaterProgram.mFeatures.hasTransport = true;
		gObjectFullbrightWaterProgram.mShaderFiles.clear();
		gObjectFullbrightWaterProgram.mShaderFiles.push_back(make_pair("objects/fullbrightV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectFullbrightWaterProgram.mShaderFiles.push_back(make_pair("objects/fullbrightWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectFullbrightWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectFullbrightWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectFullbrightWaterProgram.createShader(NULL, NULL);
	}

	if (success)
	{
		gObjectShinyProgram.mName = "Shiny Shader";
		gObjectShinyProgram.mFeatures.calculatesAtmospherics = true;
		gObjectShinyProgram.mFeatures.calculatesLighting = true;
		gObjectShinyProgram.mFeatures.hasGamma = true;
		gObjectShinyProgram.mFeatures.hasAtmospherics = true;
		gObjectShinyProgram.mFeatures.isShiny = true;
		gObjectShinyProgram.mShaderFiles.clear();
		gObjectShinyProgram.mShaderFiles.push_back(make_pair("objects/shinyV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectShinyProgram.mShaderFiles.push_back(make_pair("objects/shinyF.glsl", GL_FRAGMENT_SHADER_ARB));		
		gObjectShinyProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectShinyProgram.createShader(NULL, &mShinyUniforms);
	}

	if (success)
	{
		gObjectShinyWaterProgram.mName = "Shiny Water Shader";
		gObjectShinyWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectShinyWaterProgram.mFeatures.calculatesLighting = true;
		gObjectShinyWaterProgram.mFeatures.isShiny = true;
		gObjectShinyWaterProgram.mFeatures.hasWaterFog = true;
		gObjectShinyWaterProgram.mFeatures.hasAtmospherics = true;
		gObjectShinyWaterProgram.mShaderFiles.clear();
		gObjectShinyWaterProgram.mShaderFiles.push_back(make_pair("objects/shinyWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectShinyWaterProgram.mShaderFiles.push_back(make_pair("objects/shinyV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectShinyWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectShinyWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectShinyWaterProgram.createShader(NULL, &mShinyUniforms);
	}
	
	if (success)
	{
		gObjectFullbrightShinyProgram.mName = "Fullbright Shiny Shader";
		gObjectFullbrightShinyProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightShinyProgram.mFeatures.isFullbright = true;
		gObjectFullbrightShinyProgram.mFeatures.isShiny = true;
		gObjectFullbrightShinyProgram.mFeatures.hasGamma = true;
		gObjectFullbrightShinyProgram.mFeatures.hasTransport = true;
		gObjectFullbrightShinyProgram.mShaderFiles.clear();
		gObjectFullbrightShinyProgram.mShaderFiles.push_back(make_pair("objects/fullbrightShinyV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectFullbrightShinyProgram.mShaderFiles.push_back(make_pair("objects/fullbrightShinyF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectFullbrightShinyProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		success = gObjectFullbrightShinyProgram.createShader(NULL, &mShinyUniforms);
	}

	if (success)
	{
		gObjectFullbrightShinyWaterProgram.mName = "Fullbright Shiny Water Shader";
		gObjectFullbrightShinyWaterProgram.mFeatures.calculatesAtmospherics = true;
		gObjectFullbrightShinyWaterProgram.mFeatures.isFullbright = true;
		gObjectFullbrightShinyWaterProgram.mFeatures.isShiny = true;
		gObjectFullbrightShinyWaterProgram.mFeatures.hasGamma = true;
		gObjectFullbrightShinyWaterProgram.mFeatures.hasTransport = true;
		gObjectFullbrightShinyWaterProgram.mFeatures.hasWaterFog = true;
		gObjectFullbrightShinyWaterProgram.mShaderFiles.clear();
		gObjectFullbrightShinyWaterProgram.mShaderFiles.push_back(make_pair("objects/fullbrightShinyV.glsl", GL_VERTEX_SHADER_ARB));
		gObjectFullbrightShinyWaterProgram.mShaderFiles.push_back(make_pair("objects/fullbrightShinyWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
		gObjectFullbrightShinyWaterProgram.mShaderLevel = mVertexShaderLevel[SHADER_OBJECT];
		gObjectFullbrightShinyWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;
		success = gObjectFullbrightShinyWaterProgram.createShader(NULL, &mShinyUniforms);
	}

	if( !success )
	{
		mVertexShaderLevel[SHADER_OBJECT] = 0;
		return FALSE;
	}
	
	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersAvatar()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_AVATAR] == 0)
	{
		unloadShaderClass(SHADER_AVATAR);
		return FALSE;
	}

	if (success)
	{
		gAvatarProgram.mName = "Avatar Shader";
		gAvatarProgram.mFeatures.hasSkinning = true;
		gAvatarProgram.mFeatures.calculatesAtmospherics = true;
		gAvatarProgram.mFeatures.calculatesLighting = true;
		gAvatarProgram.mFeatures.hasGamma = true;
		gAvatarProgram.mFeatures.hasAtmospherics = true;
		gAvatarProgram.mFeatures.hasLighting = true;
		gAvatarProgram.mShaderFiles.clear();
		gAvatarProgram.mShaderFiles.push_back(make_pair("avatar/avatarV.glsl", GL_VERTEX_SHADER_ARB));
		gAvatarProgram.mShaderFiles.push_back(make_pair("avatar/avatarF.glsl", GL_FRAGMENT_SHADER_ARB));
		gAvatarProgram.mShaderLevel = mVertexShaderLevel[SHADER_AVATAR];
		success = gAvatarProgram.createShader(&mAvatarAttribs, &mAvatarUniforms);
			
		if (success)
		{
			gAvatarWaterProgram.mName = "Avatar Water Shader";
			gAvatarWaterProgram.mFeatures.hasSkinning = true;
			gAvatarWaterProgram.mFeatures.calculatesAtmospherics = true;
			gAvatarWaterProgram.mFeatures.calculatesLighting = true;
			gAvatarWaterProgram.mFeatures.hasWaterFog = true;
			gAvatarWaterProgram.mFeatures.hasAtmospherics = true;
			gAvatarWaterProgram.mFeatures.hasLighting = true;
			gAvatarWaterProgram.mShaderFiles.clear();
			gAvatarWaterProgram.mShaderFiles.push_back(make_pair("avatar/avatarV.glsl", GL_VERTEX_SHADER_ARB));
			gAvatarWaterProgram.mShaderFiles.push_back(make_pair("objects/simpleWaterF.glsl", GL_FRAGMENT_SHADER_ARB));
			// Note: no cloth under water:
			gAvatarWaterProgram.mShaderLevel = llmin(mVertexShaderLevel[SHADER_AVATAR], 1);	
			gAvatarWaterProgram.mShaderGroup = LLGLSLShader::SG_WATER;				
			success = gAvatarWaterProgram.createShader(&mAvatarAttribs, &mAvatarUniforms);
		}

		/// Keep track of avatar levels
		if (gAvatarProgram.mShaderLevel != mVertexShaderLevel[SHADER_AVATAR])
		{
			mMaxAvatarShaderLevel = mVertexShaderLevel[SHADER_AVATAR] = gAvatarProgram.mShaderLevel;
		}
	}

	if (success)
	{
		gAvatarPickProgram.mName = "Avatar Pick Shader";
		gAvatarPickProgram.mFeatures.hasSkinning = true;
		gAvatarPickProgram.mShaderFiles.clear();
		gAvatarPickProgram.mShaderFiles.push_back(make_pair("avatar/pickAvatarV.glsl", GL_VERTEX_SHADER_ARB));
		gAvatarPickProgram.mShaderFiles.push_back(make_pair("avatar/pickAvatarF.glsl", GL_FRAGMENT_SHADER_ARB));
		gAvatarPickProgram.mShaderLevel = mVertexShaderLevel[SHADER_AVATAR];
		success = gAvatarPickProgram.createShader(&mAvatarAttribs, &mAvatarUniforms);
	}

	if (success)
	{
		gAvatarEyeballProgram.mName = "Avatar Eyeball Program";
		gAvatarEyeballProgram.mFeatures.calculatesLighting = true;
		gAvatarEyeballProgram.mFeatures.isSpecular = true;
		gAvatarEyeballProgram.mFeatures.calculatesAtmospherics = true;
		gAvatarEyeballProgram.mFeatures.hasGamma = true;
		gAvatarEyeballProgram.mFeatures.hasAtmospherics = true;
		gAvatarEyeballProgram.mFeatures.hasLighting = true;
		gAvatarEyeballProgram.mShaderFiles.clear();
		gAvatarEyeballProgram.mShaderFiles.push_back(make_pair("avatar/eyeballV.glsl", GL_VERTEX_SHADER_ARB));
		gAvatarEyeballProgram.mShaderFiles.push_back(make_pair("avatar/eyeballF.glsl", GL_FRAGMENT_SHADER_ARB));
		gAvatarEyeballProgram.mShaderLevel = mVertexShaderLevel[SHADER_AVATAR];
		success = gAvatarEyeballProgram.createShader(NULL, NULL);
	}

	if( !success )
	{
		mVertexShaderLevel[SHADER_AVATAR] = 0;
		mMaxAvatarShaderLevel = 0;
		return FALSE;
	}
	
	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersInterface()
{
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_INTERFACE] == 0)
	{
		unloadShaderClass(SHADER_INTERFACE);
		return FALSE;
	}
	
	if (success)
	{
		gHighlightProgram.mName = "Highlight Shader";
		gHighlightProgram.mShaderFiles.clear();
		gHighlightProgram.mShaderFiles.push_back(make_pair("interface/highlightV.glsl", GL_VERTEX_SHADER_ARB));
		gHighlightProgram.mShaderFiles.push_back(make_pair("interface/highlightF.glsl", GL_FRAGMENT_SHADER_ARB));
		gHighlightProgram.mShaderLevel = mVertexShaderLevel[SHADER_INTERFACE];		
		success = gHighlightProgram.createShader(NULL, NULL);
	}

	if( !success )
	{
		mVertexShaderLevel[SHADER_INTERFACE] = 0;
		return FALSE;
	}
	
	return TRUE;
}

BOOL LLViewerShaderMgr::loadShadersWindLight()
{	
	BOOL success = TRUE;

	if (mVertexShaderLevel[SHADER_WINDLIGHT] < 2)
	{
		unloadShaderClass(SHADER_WINDLIGHT);
		return FALSE;
	}

	if (success)
	{
		gWLSkyProgram.mName = "Windlight Sky Shader";
		//gWLSkyProgram.mFeatures.hasGamma = true;
		gWLSkyProgram.mShaderFiles.clear();
		gWLSkyProgram.mShaderFiles.push_back(make_pair("windlight/skyV.glsl", GL_VERTEX_SHADER_ARB));
		gWLSkyProgram.mShaderFiles.push_back(make_pair("windlight/skyF.glsl", GL_FRAGMENT_SHADER_ARB));
		gWLSkyProgram.mShaderLevel = mVertexShaderLevel[SHADER_WINDLIGHT];
		gWLSkyProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gWLSkyProgram.createShader(NULL, &mWLUniforms);
	}

	if (success)
	{
		gWLCloudProgram.mName = "Windlight Cloud Program";
		//gWLCloudProgram.mFeatures.hasGamma = true;
		gWLCloudProgram.mShaderFiles.clear();
		gWLCloudProgram.mShaderFiles.push_back(make_pair("windlight/cloudsV.glsl", GL_VERTEX_SHADER_ARB));
		gWLCloudProgram.mShaderFiles.push_back(make_pair("windlight/cloudsF.glsl", GL_FRAGMENT_SHADER_ARB));
		gWLCloudProgram.mShaderLevel = mVertexShaderLevel[SHADER_WINDLIGHT];
		gWLCloudProgram.mShaderGroup = LLGLSLShader::SG_SKY;
		success = gWLCloudProgram.createShader(NULL, &mWLUniforms);
	}

	return success;
}

std::string LLViewerShaderMgr::getShaderDirPrefix(void)
{
	return gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "shaders/class");
}

void LLViewerShaderMgr::updateShaderUniforms(LLGLSLShader * shader)
{
	LLWLParamManager::instance()->updateShaderUniforms(shader);
	LLWaterParamManager::instance()->updateShaderUniforms(shader);
}

/*static*/ void LLShaderMgr::unloadShaderClass(int shader_class)
{
	std::vector<LLGLSLShader *> &shader_list = getGlobalShaderList();
	for(std::vector<LLGLSLShader *>::iterator it=shader_list.begin();it!=shader_list.end();++it)
	{
		if((*it)->mShaderClass == shader_class)
			(*it)->unload();
	}
}
/*static*/ std::vector<LLGLSLShader *> &LLShaderMgr::getGlobalShaderList()
{
	static std::vector<LLGLSLShader *> sGlbShaderLst;
	return sGlbShaderLst;
}
