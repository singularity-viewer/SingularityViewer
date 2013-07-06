/** 
 * @file llpostprocess.cpp
 * @brief LLPostProcess class implementation
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

#include "linden_common.h"

#include "llpostprocess.h"

#include "lldir.h"
#include "llgl.h"
#include "llglslshader.h"
#include "llrender.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "llsdutil_math.h"
#include "llvertexbuffer.h"
#include "llfasttimer.h"

extern LLGLSLShader			gPostColorFilterProgram;
extern LLGLSLShader			gPostNightVisionProgram;
extern LLGLSLShader			gPostGaussianBlurProgram;
extern LLGLSLShader			gPostPosterizeProgram;
extern LLGLSLShader			gPostMotionBlurProgram;
extern LLGLSLShader			gPostVignetteProgram;

static const unsigned int NOISE_SIZE = 512;

static const char * const XML_FILENAME = "postprocesseffects.xml";

class LLPostProcessShader : public IPostProcessShader, public LLRefCount
{
public:
	LLPostProcessShader(const std::string& enable_name, LLGLSLShader& shader, bool enabled = false) :
		mShader(shader), mEnabled(enable_name,enabled)
	{
		addSetting(mEnabled);
	}
	/*virtual*/	bool isEnabled()					const	{return mShader.mProgramObject && mEnabled;}
	/*virtual*/ void bindShader()							{mShader.bind();}
	/*virtual*/ void unbindShader()							{mShader.unbind();}
	/*virtual*/ LLGLSLShader& getShader()					{return mShader;}

	/*virtual*/ LLSD getDefaults();							//See IPostProcessShader::getDefaults
	/*virtual*/ void loadSettings(const LLSD& settings);	//See IPostProcessShader::loadSettings
	/*virtual*/ void addSetting(IShaderSettingBase& setting) { mSettings.push_back(&setting); }
protected:
	template<typename T>
	struct LLShaderSetting : public IShaderSettingBase
	{
		LLShaderSetting(const std::string& name, T def) : 
			mValue(def), mDefault(def), mSettingName(name) {}
		operator T()								const	{ return mValue; }
		T get()										const	{ return mValue; }
		/*virtual*/ const std::string& getName()	const	{ return mSettingName; }	//See LLShaderSettingBase::getName
		/*virtual*/ LLSD getDefaultValue()			const	{ return mDefault; }		//See LLShaderSettingBase::getDefaultValue
		/*virtual*/ void setValue(const LLSD& value)		{ mValue = value; }			//See LLShaderSettingBase::setValue	
	private:
		const std::string mSettingName;		//LLSD key names as found in postprocesseffects.xml. eg 'contrast_base'
		T mValue;							//The member variable mentioned above.
		T mDefault;							//Set via ctor. Value is inserted into the defaults LLSD list if absent from postprocesseffects.xml
	};
private:
	std::vector<IShaderSettingBase*> mSettings;	//Contains a list of all the 'settings' this shader uses. Manually add via push_back in ctor.
	LLGLSLShader& mShader;
	LLShaderSetting<bool> mEnabled;
};

//helpers
class LLPostProcessSinglePassColorShader : public LLPostProcessShader
{
public:
	LLPostProcessSinglePassColorShader(const std::string& enable_name, LLGLSLShader& shader, bool enabled = false) :
		LLPostProcessShader(enable_name, shader, enabled) {}
	/*virtual*/ S32 getColorChannel()	const	{return 0;}
	/*virtual*/ S32 getDepthChannel()	const	{return -1;}
	/*virtual*/ bool draw(U32 pass)				{return pass == 1;}
	/*virtual*/ void postDraw()					{}
};

template<> LLSD LLPostProcessShader::LLShaderSetting<LLVector4>::getDefaultValue() const
{
	return mDefault.getValue();
}
template<> void LLPostProcessShader::LLShaderSetting<LLVector4>::setValue(const LLSD& value)
{
	mValue = ll_vector4_from_sd(value); 
}

LLSD LLPostProcessShader::getDefaults()
{
	LLSD defaults;
	for(std::vector<IShaderSettingBase*>::iterator it=mSettings.begin();it!=mSettings.end();++it)
	{
		defaults[(*it)->getName()]=(*it)->getDefaultValue();
	}
	return defaults;
}
void LLPostProcessShader::loadSettings(const LLSD& settings)
{
	for(std::vector<IShaderSettingBase*>::iterator it=mSettings.begin();it!=mSettings.end();++it)
	{
		LLSD value = settings[(*it)->getName()];
		(*it)->setValue(value);
	}
}

class LLColorFilterShader : public LLPostProcessSinglePassColorShader
{
private:
	LLShaderSetting<F32> mGamma, mBrightness, mContrast, mSaturation;
	LLShaderSetting<LLVector4> mContrastBase;
public:
	LLColorFilterShader() : 
		LLPostProcessSinglePassColorShader("enable_color_filter",gPostColorFilterProgram),
		mGamma("gamma",1.f),
		mBrightness("brightness",1.f),
		mContrast("contrast",1.f),
		mSaturation("saturation",1.f),
		mContrastBase("contrast_base",LLVector4(1.f,1.f,1.f,0.5f))
	{
		addSetting(mGamma);
		addSetting(mBrightness);
		addSetting(mContrast);
		addSetting(mSaturation);
		addSetting(mContrastBase);
	}

	/*virtual*/ QuadType preDraw()
	{
		getShader().uniform1f("gamma", mGamma);
		getShader().uniform1f("brightness", mBrightness);
		getShader().uniform1f("contrast", mContrast);
		float baseI = (mContrastBase.get()[VX] + mContrastBase.get()[VY] + mContrastBase.get()[VZ]) / 3.0f;
		baseI = mContrastBase.get()[VW] / llmax(baseI,0.001f);
		float baseR = mContrastBase.get()[VX] * baseI;
		float baseG = mContrastBase.get()[VY] * baseI;
		float baseB = mContrastBase.get()[VZ] * baseI;
		getShader().uniform3fv("contrastBase", 1, LLVector3(baseR, baseG, baseB).mV);
		getShader().uniform1f("saturation", mSaturation);

		return QUAD_NORMAL;
	}
};

class LLNightVisionShader : public LLPostProcessSinglePassColorShader
{
private:
	LLShaderSetting<F32> mBrightnessMult, mNoiseStrength;
public:
	LLNightVisionShader() : 
		LLPostProcessSinglePassColorShader("enable_night_vision",gPostNightVisionProgram),
		mBrightnessMult("brightness_multiplier",3.f),
		mNoiseStrength("noise_strength",.4f)
	{
		addSetting(mBrightnessMult);
		addSetting(mNoiseStrength);
	}
	/*virtual*/ QuadType preDraw()
	{
		LLPostProcess::getInstance()->bindNoise(1);

		getShader().uniform1f("brightMult", mBrightnessMult);
		getShader().uniform1f("noiseStrength", mNoiseStrength);

		return QUAD_NOISE;
	}
};

class LLPosterizeShader : public LLPostProcessSinglePassColorShader
{
private:
	LLShaderSetting<S32> mNumLayers;
public:
	LLPosterizeShader() : LLPostProcessSinglePassColorShader("enable_posterize",gPostPosterizeProgram),
		mNumLayers("posterize_layers",2)
	{
		addSetting(mNumLayers);
	}
	/*virtual*/ QuadType preDraw()
	{
		getShader().uniform1i("layerCount", mNumLayers);
		return QUAD_NORMAL;
	}
};

class LLVignetteShader : public LLPostProcessSinglePassColorShader
{
private:
	LLShaderSetting<F32> mStrength, mRadius, mDarkness, mDesaturation, mChromaticAberration;
public:
	LLVignetteShader() : LLPostProcessSinglePassColorShader("enable_vignette",gPostVignetteProgram),
	mStrength("vignette_strength",.85f),
	mRadius("vignette_radius",.7f),
	mDarkness("vignette_darkness",1.f),
	mDesaturation("vignette_desaturation",1.5f),
	mChromaticAberration("vignette_chromatic_aberration",.05f)
	{
		addSetting(mStrength);
		addSetting(mRadius);
		addSetting(mDarkness);
		addSetting(mDesaturation);
		addSetting(mChromaticAberration);
	}
	/*virtual*/ QuadType preDraw()
	{
		LLVector2 screen_rect = LLPostProcess::getInstance()->getDimensions();
		getShader().uniform1f("vignette_strength", mStrength);
		getShader().uniform1f("vignette_radius", mRadius);
		getShader().uniform1f("vignette_darkness", mDarkness);
		getShader().uniform1f("vignette_desaturation", mDesaturation);
		getShader().uniform1f("vignette_chromatic_aberration", mChromaticAberration);
		getShader().uniform2fv("screen_res", 1, screen_rect.mV);
		return QUAD_NORMAL;
	}
};

class LLGaussBlurShader : public LLPostProcessShader
{
private:
	LLShaderSetting<S32> mNumPasses;
	GLint mPassLoc;
public:
	LLGaussBlurShader() : LLPostProcessShader("enable_gauss_blur",gPostGaussianBlurProgram),
		mNumPasses("gauss_blur_passes",2),
		mPassLoc(0)
	{
		addSetting(mNumPasses);
	}
	/*virtual*/ bool isEnabled()		const	{ return LLPostProcessShader::isEnabled() && mNumPasses.get(); }
	/*virtual*/ S32 getColorChannel()	const	{ return 0; }
	/*virtual*/ S32 getDepthChannel()	const	{ return -1; }
	/*virtual*/ QuadType preDraw()
	{
		mPassLoc = getShader().getUniformLocation("horizontalPass");
		return QUAD_NORMAL;
	}
	/*virtual*/ bool draw(U32 pass) 
	{
		if((S32)pass > mNumPasses*2)
			return false;
		glUniform1iARB(mPassLoc, (pass-1) % 2);
		return true;
	}
	/*virtual*/ void postDraw()					{}
};

class LLMotionShader : public LLPostProcessShader
{
private:
	LLShaderSetting<S32> mStrength;
public:
	LLMotionShader() : LLPostProcessShader("enable_motionblur",gPostMotionBlurProgram),
		mStrength("blur_strength",1)
	{
		addSetting(mStrength);
	}
	/*virtual*/ bool isEnabled()		const	{ return LLPostProcessShader::isEnabled() && llabs(gGLModelView[0] - gGLPreviousModelView[0]) > .0000001; }
	/*virtual*/ S32 getColorChannel()	const	{ return 0; }
	/*virtual*/ S32 getDepthChannel()	const	{ return 1; }
	/*virtual*/ QuadType preDraw()
	{
		glh::matrix4f inv_proj(gGLModelView);
		inv_proj.mult_left(gGLProjection);
		inv_proj = inv_proj.inverse();
		glh::matrix4f prev_proj(gGLPreviousModelView);
		prev_proj.mult_left(gGLProjection);

		LLVector2 screen_rect = LLPostProcess::getInstance()->getDimensions();

		getShader().uniformMatrix4fv("prev_proj", 1, GL_FALSE, prev_proj.m);
		getShader().uniformMatrix4fv("inv_proj", 1, GL_FALSE, inv_proj.m);
		getShader().uniform2fv("screen_res", 1, screen_rect.mV);
		getShader().uniform1i("blur_strength", mStrength);
		
		return QUAD_NORMAL;
	}
	/*virtual*/ bool draw(U32 pass) 
	{
		return pass == 1;
	}
	/*virtual*/ void postDraw()					{}
};

LLPostProcess::LLPostProcess(void) : 
					mVBO(NULL),
					mDepthTexture(0),
					mNoiseTexture(NULL),
					mScreenWidth(0),
					mScreenHeight(0),
					mNoiseTextureScale(0.f),
					mSelectedEffectInfo(LLSD::emptyMap()),
					mAllEffectInfo(LLSD::emptyMap())
{
	mShaders.push_back(new LLMotionShader());
	mShaders.push_back(new LLVignetteShader());
	mShaders.push_back(new LLColorFilterShader());
	mShaders.push_back(new LLNightVisionShader());
	mShaders.push_back(new LLGaussBlurShader());
	mShaders.push_back(new LLPosterizeShader());

	/*  Do nothing.  Needs to be updated to use our current shader system, and to work with the move into llrender.*/
	std::string pathName(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight", XML_FILENAME));
	LL_DEBUGS2("AppInit", "Shaders") << "Loading PostProcess Effects settings from " << pathName << LL_ENDL;

	llifstream effectsXML(pathName);

	if (effectsXML)
	{
		LLPointer<LLSDParser> parser = new LLSDXMLParser();

		parser->parse(effectsXML, mAllEffectInfo, LLSDSerialize::SIZE_UNLIMITED);
	}

	if (!mAllEffectInfo.has("default"))
		mAllEffectInfo["default"] = LLSD::emptyMap();

	LLSD& defaults = mAllEffectInfo["default"];

	for(std::list<LLPointer<LLPostProcessShader> >::iterator it=mShaders.begin();it!=mShaders.end();++it)
	{
		LLSD shader_defaults = (*it)->getDefaults();
		for (LLSD::map_const_iterator it2 = defaults.beginMap();it2 != defaults.endMap();++it2)
		{
			if(!defaults.has(it2->first))
				defaults[it2->first]=it2->second;
		}
	}
	for(std::list<LLPointer<LLPostProcessShader> >::iterator it=mShaders.begin();it!=mShaders.end();++it)
	{
		(*it)->loadSettings(defaults);
	}
	setSelectedEffect("default");
}

LLPostProcess::~LLPostProcess(void)
{
	destroyGL() ;
}

void LLPostProcess::initialize(unsigned int width, unsigned int height)
{
	destroyGL();
	mScreenWidth = width;
	mScreenHeight = height;

	createScreenTextures();
	createNoiseTexture();

	//Setup our VBO.
	{
		mVBO = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 | LLVertexBuffer::MAP_TEXCOORD1,3);
		mVBO->allocateBuffer(4,0,TRUE);

		LLStrider<LLVector3> v;
		LLStrider<LLVector2> uv1;
		LLStrider<LLVector2> uv2;

		mVBO->getVertexStrider(v);
		mVBO->getTexCoord0Strider(uv1);
		mVBO->getTexCoord1Strider(uv2);
	
		v[0] = LLVector3( uv2[0] = uv1[0] = LLVector2(0, 0) );
		v[1] = LLVector3( uv2[1] = uv1[1] = LLVector2(0, mScreenHeight) );
		v[2] = LLVector3( uv2[2] = uv1[2] = LLVector2(mScreenWidth, 0) );
		v[3] = LLVector3( uv2[3] = uv1[3] = LLVector2(mScreenWidth, mScreenHeight) );

		mVBO->flush();
	}
	stop_glerror();
}

void LLPostProcess::createScreenTextures()
{
	const LLTexUnit::eTextureType type = LLTexUnit::TT_RECT_TEXTURE;

	mRenderTarget[0].allocate(mScreenWidth,mScreenHeight,GL_RGBA,FALSE,FALSE,type,FALSE);
	if(mRenderTarget[0].getFBO())//Only need pingpong between two rendertargets if FBOs are supported.
		mRenderTarget[1].allocate(mScreenWidth,mScreenHeight,GL_RGBA,FALSE,FALSE,type,FALSE);
	stop_glerror();

	if(mDepthTexture)
		LLImageGL::deleteTextures(type, 0, 0, 1, &mDepthTexture, true);

	for(std::list<LLPointer<LLPostProcessShader> >::iterator it=mShaders.begin();it!=mShaders.end();++it)
	{
		if((*it)->getDepthChannel()>=0)
		{
			LLImageGL::generateTextures(type, GL_DEPTH_COMPONENT24, 1, &mDepthTexture);
			gGL.getTexUnit(0)->bindManual(type, mDepthTexture);
			LLImageGL::setManualImage(LLTexUnit::getInternalType(type), 0, GL_DEPTH_COMPONENT24, mScreenWidth, mScreenHeight, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL, false);
			stop_glerror();
			gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_POINT);
			gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
			stop_glerror();
			break;
		}
	}
}

void LLPostProcess::createNoiseTexture()
{	
	std::vector<GLubyte> buffer(NOISE_SIZE * NOISE_SIZE);
	for (unsigned int i = 0; i < NOISE_SIZE; i++){
		for (unsigned int k = 0; k < NOISE_SIZE; k++){
			buffer[(i * NOISE_SIZE) + k] = (GLubyte)((double) rand() / ((double) RAND_MAX + 1.f) * 255.f);
		}
	}

	mNoiseTexture = new LLImageGL(FALSE) ;
	if(mNoiseTexture->createGLTexture())
	{
		gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_TEXTURE, mNoiseTexture->getTexName());
		LLImageGL::setManualImage(GL_TEXTURE_2D, 0, GL_LUMINANCE, NOISE_SIZE, NOISE_SIZE, GL_LUMINANCE, GL_UNSIGNED_BYTE, &buffer[0]);
		stop_glerror();
		gGL.getTexUnit(0)->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
		gGL.getTexUnit(0)->setTextureAddressMode(LLTexUnit::TAM_WRAP);
		stop_glerror();
	}
}

void LLPostProcess::destroyGL()
{
	mRenderTarget[0].release();
	mRenderTarget[1].release();
	if(mDepthTexture)
		LLImageGL::deleteTextures(LLTexUnit::TT_RECT_TEXTURE, 0, 0, 1, &mDepthTexture, true);
	mDepthTexture=0;
	mNoiseTexture = NULL ;
	mVBO = NULL ;
}

/*static*/void LLPostProcess::cleanupClass()
{
	if(instanceExists())
		getInstance()->destroyGL() ;
}

void LLPostProcess::copyFrameBuffer()
{
	mRenderTarget[!!mRenderTarget[0].getFBO()].bindTexture(0,0);
	glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,0,0,0,0,mScreenWidth, mScreenHeight);

	if(mDepthTexture)
	{
		for(std::list<LLPointer<LLPostProcessShader> >::iterator it=mShaders.begin();it!=mShaders.end();++it)
		{
			if((*it)->isEnabled() && (*it)->getDepthChannel()>=0)
			{
				gGL.getTexUnit(0)->bindManual(LLTexUnit::TT_RECT_TEXTURE, mDepthTexture);
				glCopyTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,0,0,0,0,mScreenWidth, mScreenHeight);
				break;
			}
		}
	}

}

void LLPostProcess::bindNoise(U32 channel)
{
	gGL.getTexUnit(channel)->bind(mNoiseTexture);
}

void LLPostProcess::renderEffects(unsigned int width, unsigned int height)
{
	for(std::list<LLPointer<LLPostProcessShader> >::iterator it=mShaders.begin();it!=mShaders.end();++it)
	{
		if((*it)->isEnabled())
		{
			if (mVBO.isNull() || width != mScreenWidth || height != mScreenHeight)
			{
				initialize(width, height);
			}
			doEffects();
			return;
		}
	}
}

void LLPostProcess::doEffects(void)
{
	LLVertexBuffer::unbind();

	mNoiseTextureScale = 0.001f + ((100.f - mSelectedEffectInfo["noise_size"].asFloat()) / 100.f);
	mNoiseTextureScale *= (mScreenHeight / NOISE_SIZE);

	/// Copy the screen buffer to the render texture
	copyFrameBuffer();
	stop_glerror();

	//Disable depth. Set blendmode to replace.
	LLGLDepthTest depth(GL_FALSE,GL_FALSE);
	LLGLEnable blend(GL_BLEND);
	gGL.setSceneBlendType(LLRender::BT_REPLACE);

	/// Change to an orthogonal view
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadIdentity();
	gGL.ortho( 0.f, (GLdouble) mScreenWidth, 0.f, (GLdouble) mScreenHeight, -1.f, 1.f );
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadIdentity();
	
	applyShaders();

	LLGLSLShader::bindNoShader();

	/// Change to a perspective view
	gGL.flush();
	gGL.matrixMode( LLRender::MM_PROJECTION );
	gGL.popMatrix();
	gGL.matrixMode( LLRender::MM_MODELVIEW );
	gGL.popMatrix();

	gGL.setSceneBlendType(LLRender::BT_ALPHA);	//Restore blendstate. Alpha is ASSUMED for hud/ui render, etc.

	gGL.getTexUnit(1)->disable();
}

void LLPostProcess::applyShaders(void)
{
	bool primary_rendertarget = 1;
	for(std::list<LLPointer<LLPostProcessShader> >::iterator it=mShaders.begin();it!=mShaders.end();++it)
	{
		if((*it)->isEnabled())
		{
			S32 color_channel = (*it)->getColorChannel();
			S32 depth_channel = (*it)->getDepthChannel();
			if(depth_channel >= 0)
				gGL.getTexUnit(depth_channel)->bindManual(LLTexUnit::TT_RECT_TEXTURE, mDepthTexture);
			U32 pass = 1;

			(*it)->bindShader();
			QuadType quad = (*it)->preDraw();
			while((*it)->draw(pass++))
			{
				mRenderTarget[!primary_rendertarget].bindTarget();

				if(color_channel >= 0)
					mRenderTarget[mRenderTarget[0].getFBO() ? primary_rendertarget : !primary_rendertarget].bindTexture(0,color_channel);

				drawOrthoQuad(quad);
				mRenderTarget[!primary_rendertarget].flush();
				if(mRenderTarget[0].getFBO())
					primary_rendertarget = !primary_rendertarget;
			}
			(*it)->postDraw();
			(*it)->unbindShader();
		}
	}
	//Only need to copy to framebuffer if FBOs are supported, else we've already been drawing to the framebuffer to begin with.
	if(mRenderTarget[0].getFBO())
	{
		//copyContentsToFramebuffer also binds the main framebuffer.
		LLRenderTarget::copyContentsToFramebuffer(mRenderTarget[primary_rendertarget],0,0,mScreenWidth,mScreenHeight,0,0,mScreenWidth,mScreenHeight,GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
	stop_glerror();
}

void LLPostProcess::drawOrthoQuad(QuadType type)
{
	if(type == QUAD_NOISE)
	{
		//This could also be done through uniforms.
		LLStrider<LLVector2> uv2;
		mVBO->getTexCoord1Strider(uv2);

		float offs[2] = {(float) rand() / (float) RAND_MAX, (float) rand() / (float) RAND_MAX};
		float scale[2] = {mScreenWidth * mNoiseTextureScale / mScreenHeight, mNoiseTextureScale};
		uv2[0] = LLVector2(offs[0],offs[1]);
		uv2[1] = LLVector2(offs[0],offs[1]+scale[1]);
		uv2[2] = LLVector2(offs[0]+scale[0],offs[1]);
		uv2[3] = LLVector2(uv2[2].mV[0],uv2[1].mV[1]);
		mVBO->flush();
	}

	U32 mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_TEXCOORD0 | (type == QUAD_NOISE ? LLVertexBuffer::MAP_TEXCOORD1 : 0);
	mVBO->setBuffer(mask);
	mVBO->drawArrays(LLRender::TRIANGLE_STRIP, 0, 4);
}

void LLPostProcess::setSelectedEffect(std::string const & effectName)
{
	mSelectedEffectName = effectName;
	mSelectedEffectInfo = mAllEffectInfo[effectName];
	for(std::list<LLPointer<LLPostProcessShader> >::iterator it=mShaders.begin();it!=mShaders.end();++it)
	{
		(*it)->loadSettings(mSelectedEffectInfo);
	}
}

void LLPostProcess::setSelectedEffectValue(std::string const & setting, LLSD value)
{
	char buf[256];
	S32 elem=0;
	if(sscanf(setting.c_str(),"%255[^[][%d]", buf, &elem) == 2)
	{
		mSelectedEffectInfo[static_cast<const char*>(buf)][elem] = value;
	}
	else
	{
		mSelectedEffectInfo[setting] = value;
	}
	for(std::list<LLPointer<LLPostProcessShader> >::iterator it=mShaders.begin();it!=mShaders.end();++it)
	{
		(*it)->loadSettings(mSelectedEffectInfo);
	}
}

void LLPostProcess::resetSelectedEffect()
{
	if(!llsd_equals(mAllEffectInfo[mSelectedEffectName], mSelectedEffectInfo))
	{
		mSelectedEffectInfo = mAllEffectInfo[mSelectedEffectName];
		for(std::list<LLPointer<LLPostProcessShader> >::iterator it=mShaders.begin();it!=mShaders.end();++it)
		{
			(*it)->loadSettings(mSelectedEffectInfo);
		}
	}
}

void LLPostProcess::saveEffectAs(std::string const & effectName)
{
	mAllEffectInfo[effectName] = mSelectedEffectInfo;

	std::string pathName(gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "windlight", XML_FILENAME));
	//llinfos << "Saving PostProcess Effects settings to " << pathName << llendl;

	llofstream effectsXML(pathName);

	LLPointer<LLSDFormatter> formatter = new LLSDXMLFormatter();

	formatter->format(mAllEffectInfo, effectsXML);
}



