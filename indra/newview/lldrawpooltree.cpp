/** 
 * @file lldrawpooltree.cpp
 * @brief LLDrawPoolTree class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "lldrawpooltree.h"

#include "lldrawable.h"
#include "llface.h"
#include "llsky.h"
#include "llvotree.h"
#include "pipeline.h"
#include "llviewercamera.h"
#include "llviewershadermgr.h"
#include "llrender.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"

S32 LLDrawPoolTree::sDiffTex = 0;
static LLGLSLShader* shader = NULL;
static LLFastTimer::DeclareTimer FTM_SHADOW_TREE("Tree Shadow");

LLDrawPoolTree::LLDrawPoolTree(LLViewerTexture *texturep) :
	LLFacePool(POOL_TREE),
	mTexturep(texturep)
{
	mTexturep->setAddressMode(LLTexUnit::TAM_WRAP);
}

LLDrawPool *LLDrawPoolTree::instancePool()
{
	return new LLDrawPoolTree(mTexturep);
}

void LLDrawPoolTree::prerender()
{
	mVertexShaderLevel = LLViewerShaderMgr::instance()->getVertexShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

void LLDrawPoolTree::beginRenderPass(S32 pass)
{
	LLFastTimer t(FTM_RENDER_TREES);
		
	if (LLPipeline::sUnderWaterRender)
	{
		shader = &gTreeWaterProgram;
	}
	else
	{
		shader = &gTreeProgram;
	}

	if (gPipeline.canUseVertexShaders())
	{
		shader->bind();
		shader->setMinimumAlpha(0.5f);
		gGL.diffuseColor4f(1,1,1,1);
	}
	else
	{
		gPipeline.enableLightsDynamic();
		gGL.setAlphaRejectSettings(LLRender::CF_GREATER, 0.5f);
	}
}

void LLDrawPoolTree::render(S32 pass)
{
	LLFastTimer t(LLPipeline::sShadowRender ? FTM_SHADOW_TREE : FTM_RENDER_TREES);

	if (mDrawFace.empty())
	{
		return;
	}

	LLGLState test(GL_ALPHA_TEST, LLGLSLShader::sNoFixedFunction ? 0 : 1);
	LLOverrideFaceColor color(this, 1.f, 1.f, 1.f, 1.f);

	static LLCachedControl<bool> sRenderAnimateTrees("RenderAnimateTrees", false);
	if (sRenderAnimateTrees)
	{
		renderTree();
	}
	else
	gGL.getTexUnit(sDiffTex)->bind(mTexturep);
					
	for (std::vector<LLFace*>::iterator iter = mDrawFace.begin();
			 iter != mDrawFace.end(); iter++)
	{
		LLFace *face = *iter;
		LLVertexBuffer* buff = face->getVertexBuffer();

		if(buff)
		{
			LLMatrix4* model_matrix = &(face->getDrawable()->getRegion()->mRenderMatrix);

			if (model_matrix != gGLLastMatrix)
			{
				gGLLastMatrix = model_matrix;
				gGL.loadMatrix(gGLModelView);
				if (model_matrix)
				{
					llassert(gGL.getMatrixMode() == LLRender::MM_MODELVIEW);
					gGL.multMatrix((GLfloat*) model_matrix->mMatrix);
				}
				gPipeline.mMatrixOpCount++;
			}

			buff->setBuffer(LLDrawPoolTree::VERTEX_DATA_MASK);
			buff->drawRange(LLRender::TRIANGLES, 0, buff->getNumVerts()-1, buff->getNumIndices(), 0); 
			gPipeline.addTrianglesDrawn(buff->getNumIndices());
		}
	}
}

void LLDrawPoolTree::endRenderPass(S32 pass)
{
	LLFastTimer t(FTM_RENDER_TREES);
		
	if (gPipeline.canUseWindLightShadersOnObjects())
	{
		shader->unbind();
	}
	
	if (mVertexShaderLevel <= 0)
	{
		gGL.setAlphaRejectSettings(LLRender::CF_DEFAULT);
	}
}

//============================================
// deferred implementation
//============================================
void LLDrawPoolTree::beginDeferredPass(S32 pass)
{
	LLFastTimer t(FTM_RENDER_TREES);
		
	shader = &gDeferredTreeProgram;
	shader->bind();
	shader->setMinimumAlpha(0.5f);
}

void LLDrawPoolTree::renderDeferred(S32 pass)
{
	render(pass);
}

void LLDrawPoolTree::endDeferredPass(S32 pass)
{
	LLFastTimer t(FTM_RENDER_TREES);
	
	shader->unbind();
}

//============================================
// shadow implementation
//============================================
void LLDrawPoolTree::beginShadowPass(S32 pass)
{
	LLFastTimer t(FTM_SHADOW_TREE);

	static const LLCachedControl<F32> render_deferred_offset("RenderDeferredTreeShadowOffset",1.f);
	static const LLCachedControl<F32> render_deferred_bias("RenderDeferredTreeShadowBias",1.f);
	glPolygonOffset(render_deferred_offset,render_deferred_bias);
	gDeferredTreeShadowProgram.bind();
	gDeferredTreeShadowProgram.setMinimumAlpha(0.5f);
}

void LLDrawPoolTree::renderShadow(S32 pass)
{
	render(pass);
}

void LLDrawPoolTree::endShadowPass(S32 pass)
{
	LLFastTimer t(FTM_SHADOW_TREE);

	static const LLCachedControl<F32> render_deferred_offset("RenderDeferredSpotShadowOffset",1.f);
	static const LLCachedControl<F32> render_deferred_bias("RenderDeferredSpotShadowBias",1.f);
	glPolygonOffset(render_deferred_offset,render_deferred_bias);
	gDeferredTreeShadowProgram.unbind();
}

//
void LLDrawPoolTree::renderTree(BOOL selecting)
{
	LLGLState normalize(GL_NORMALIZE, TRUE);
	
	// Bind the texture for this tree.
	gGL.getTexUnit(sDiffTex)->bind(mTexturep.get(), TRUE);
		
	U32 indices_drawn = 0;

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	
	for (std::vector<LLFace*>::iterator iter = mDrawFace.begin();
		 iter != mDrawFace.end(); iter++)
	{
		LLFace *face = *iter;
		LLDrawable *drawablep = face->getDrawable();

		if (drawablep->isDead() || !face->getVertexBuffer())
		{
			continue;
		}

		face->getVertexBuffer()->setBuffer(LLDrawPoolTree::VERTEX_DATA_MASK);
		U16* indicesp = (U16*) face->getVertexBuffer()->getIndicesPointer();

		// Render each of the trees
		LLVOTree *treep = (LLVOTree *)drawablep->getVObj().get();

		LLColor4U color(255,255,255,255);

		if (!selecting || treep->mGLName != 0)
		{
			if (selecting)
			{
				S32 name = treep->mGLName;
				
				color = LLColor4U((U8)(name >> 16), (U8)(name >> 8), (U8)name, 255);
			}
			
			gGLLastMatrix = NULL;
			gGL.loadMatrix(gGLModelView);
			//gGL.pushMatrix();

			LLMatrix4 matrix(gGLModelView);
			
			// Translate to tree base  HACK - adjustment in Z plants tree underground
			const LLVector3 &pos_agent = treep->getPositionAgent();
			//gGL.translatef(pos_agent.mV[VX], pos_agent.mV[VY], pos_agent.mV[VZ] - 0.1f);
			LLMatrix4 trans_mat;
			trans_mat.setTranslation(pos_agent.mV[VX], pos_agent.mV[VY], pos_agent.mV[VZ] - 0.1f);
			trans_mat *= matrix;
			
			// Rotate to tree position and bend for current trunk/wind
			// Note that trunk stiffness controls the amount of bend at the trunk as 
			// opposed to the crown of the tree
			// 
			const F32 TRUNK_STIFF = 22.f;
			
			LLQuaternion rot = 
				LLQuaternion(treep->mTrunkBend.magVec()*TRUNK_STIFF*DEG_TO_RAD, LLVector4(treep->mTrunkBend.mV[VX], treep->mTrunkBend.mV[VY], 0)) *
				LLQuaternion(90.f*DEG_TO_RAD, LLVector4(0,0,1)) *
				treep->getRotation();

			LLMatrix4 rot_mat(rot);
			rot_mat *= trans_mat;

			F32 radius = treep->getScale().magVec()*0.05f;
			LLMatrix4 scale_mat;
			scale_mat.mMatrix[0][0] = 
				scale_mat.mMatrix[1][1] =
				scale_mat.mMatrix[2][2] = radius;

			scale_mat *= rot_mat;

			//TO-DO: Make these set-able?
			const F32 THRESH_ANGLE_FOR_BILLBOARD = 7.5f; //Made LoD now a little less aggressive here -Shyotl
			const F32 BLEND_RANGE_FOR_BILLBOARD = 1.5f;

			F32 droop = treep->mDroop + 25.f*(1.f - treep->mTrunkBend.magVec());
			
			S32 stop_depth = 0;
			F32 app_angle = treep->getAppAngle()*LLVOTree::sTreeFactor;
			F32 alpha = 1.0;
			S32 trunk_LOD = LLVOTree::sMAX_NUM_TREE_LOD_LEVELS;

			for (S32 j = 0; j < 4; j++)
			{

				if (app_angle > LLVOTree::sLODAngles[j])
				{
					trunk_LOD = j;
					break;
				}
			} 
			if(trunk_LOD >= LLVOTree::sMAX_NUM_TREE_LOD_LEVELS)
			{
				continue ; //do not render.
			}

			if (app_angle < (THRESH_ANGLE_FOR_BILLBOARD - BLEND_RANGE_FOR_BILLBOARD))
			{
				//
				//  Draw only the billboard 
				//
				//  Only the billboard, can use closer to normal alpha func.
				stop_depth = -1;
				LLFacePool::LLOverrideFaceColor clr(this, color); 
				indices_drawn += treep->drawBranchPipeline(scale_mat, indicesp, trunk_LOD, stop_depth, treep->mDepth, treep->mTrunkDepth, 1.0, treep->mTwist, droop, treep->mBranches, alpha);
			}
			else // if (app_angle > (THRESH_ANGLE_FOR_BILLBOARD + BLEND_RANGE_FOR_BILLBOARD))
			{
				//
				//  Draw only the full geometry tree
				//
				LLFacePool::LLOverrideFaceColor clr(this, color); 
				indices_drawn += treep->drawBranchPipeline(scale_mat, indicesp, trunk_LOD, stop_depth, treep->mDepth, treep->mTrunkDepth, 1.0, treep->mTwist, droop, treep->mBranches, alpha);
			}
			
			//gGL.popMatrix();
		}
	}
}//

BOOL LLDrawPoolTree::verify() const
{
/*	BOOL ok = TRUE;

	if (!ok)
	{
		printDebugInfo();
	}
	return ok;*/

	return TRUE;
}

LLViewerTexture *LLDrawPoolTree::getTexture()
{
	return mTexturep;
}

LLViewerTexture *LLDrawPoolTree::getDebugTexture()
{
	return mTexturep;
}


LLColor3 LLDrawPoolTree::getDebugColor() const
{
	return LLColor3(1.f, 0.f, 1.f);
}

