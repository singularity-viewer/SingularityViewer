/** 
 * @file llpolymorph.cpp
 * @brief Implementation of LLPolyMesh class
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 * 
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

//-----------------------------------------------------------------------------
// Header Files
//-----------------------------------------------------------------------------
#include "llviewerprecompiledheaders.h"

#include "llpolymorph.h"
#include "llvoavatar.h"
#include "llxmltree.h"
#include "llendianswizzle.h"

//#include "../tools/imdebug/imdebug.h"

const F32 NORMAL_SOFTEN_FACTOR = 0.65f;
const F32 SIGNIFICANT_DELTA    = 0.0001f;

//-----------------------------------------------------------------------------
// LLPolyMorphData()
//-----------------------------------------------------------------------------
LLPolyMorphData::LLPolyMorphData(const std::string& morph_name)
	: mName(morph_name)
{
	mNumIndices = 0;
	mCurrentIndex = 0;
	mTotalDistortion = 0.f;
	mAvgDistortion.zeroVec();
	mMaxDistortion = 0.f;
	mVertexIndices = NULL;
	mCoords = NULL;
	mNormals = NULL;
	mBinormals = NULL;
	mTexCoords = NULL;

	mMesh = NULL;
}

LLPolyMorphData::LLPolyMorphData(const LLPolyMorphData &rhs) :
	mName(rhs.mName),
	mNumIndices(rhs.mNumIndices),
	mTotalDistortion(rhs.mTotalDistortion),
	mAvgDistortion(rhs.mAvgDistortion),
	mMaxDistortion(rhs.mMaxDistortion),
	mVertexIndices(NULL),
	mCoords(NULL),
	mNormals(NULL),
	mBinormals(NULL),
	mTexCoords(NULL)
{
	const S32 numVertices = mNumIndices;

	mCoords = new LLVector3[numVertices];
	mNormals = new LLVector3[numVertices];
	mBinormals = new LLVector3[numVertices];
	mTexCoords = new LLVector2[numVertices];
	mVertexIndices = new U32[numVertices];
	
	for (S32 v=0; v < numVertices; v++)
	{
		mCoords[v] = rhs.mCoords[v];
		mNormals[v] = rhs.mNormals[v];
		mBinormals[v] = rhs.mBinormals[v];
		mTexCoords[v] = rhs.mTexCoords[v];
		mVertexIndices[v] = rhs.mVertexIndices[v];
	}
}

//-----------------------------------------------------------------------------
// ~LLPolyMorphData()
//-----------------------------------------------------------------------------
LLPolyMorphData::~LLPolyMorphData()
{
	delete [] mVertexIndices;
	delete [] mCoords;
	delete [] mNormals;
	delete [] mBinormals;
	delete [] mTexCoords;
}

//-----------------------------------------------------------------------------
// loadBinary()
//-----------------------------------------------------------------------------
BOOL LLPolyMorphData::loadBinary(LLFILE *fp, LLPolyMeshSharedData *mesh)
{
	S32 numVertices;
	S32 numRead;

	numRead = fread(&numVertices, sizeof(S32), 1, fp);
	llendianswizzle(&numVertices, sizeof(S32), 1);
	if (numRead != 1)
	{
		llwarns << "Can't read number of morph target vertices" << llendl;
		return FALSE;
	}

	//-------------------------------------------------------------------------
	// allocate vertices
	//-------------------------------------------------------------------------
	mCoords = new LLVector3[numVertices];
	mNormals = new LLVector3[numVertices];
	mBinormals = new LLVector3[numVertices];
	mTexCoords = new LLVector2[numVertices];
	// Actually, we are allocating more space than we need for the skiplist
	mVertexIndices = new U32[numVertices];
	mNumIndices = 0;
	mTotalDistortion = 0.f;
	mMaxDistortion = 0.f;
	mAvgDistortion.zeroVec();
	mMesh = mesh;

	//-------------------------------------------------------------------------
	// read vertices
	//-------------------------------------------------------------------------
	for(S32 v = 0; v < numVertices; v++)
	{
		numRead = fread(&mVertexIndices[v], sizeof(U32), 1, fp);
		llendianswizzle(&mVertexIndices[v], sizeof(U32), 1);
		if (numRead != 1)
		{
			llwarns << "Can't read morph target vertex number" << llendl;
			return FALSE;
		}

		if (mVertexIndices[v] > 10000)
		{
			llerrs << "Bad morph index: " << mVertexIndices[v] << llendl;
		}


		numRead = fread(&mCoords[v].mV, sizeof(F32), 3, fp);
		llendianswizzle(&mCoords[v].mV, sizeof(F32), 3);
		if (numRead != 3)
		{
			llwarns << "Can't read morph target vertex coordinates" << llendl;
			return FALSE;
		}

		F32 magnitude = mCoords[v].magVec();
		
		mTotalDistortion += magnitude;
		mAvgDistortion.mV[VX] += fabs(mCoords[v].mV[VX]);
		mAvgDistortion.mV[VY] += fabs(mCoords[v].mV[VY]);
		mAvgDistortion.mV[VZ] += fabs(mCoords[v].mV[VZ]);
		
		if (magnitude > mMaxDistortion)
		{
			mMaxDistortion = magnitude;
		}

		numRead = fread(&mNormals[v].mV, sizeof(F32), 3, fp);
		llendianswizzle(&mNormals[v].mV, sizeof(F32), 3);
		if (numRead != 3)
		{
			llwarns << "Can't read morph target normal" << llendl;
			return FALSE;
		}

		numRead = fread(&mBinormals[v].mV, sizeof(F32), 3, fp);
		llendianswizzle(&mBinormals[v].mV, sizeof(F32), 3);
		if (numRead != 3)
		{
			llwarns << "Can't read morph target binormal" << llendl;
			return FALSE;
		}


		numRead = fread(&mTexCoords[v].mV, sizeof(F32), 2, fp);
		llendianswizzle(&mTexCoords[v].mV, sizeof(F32), 2);
		if (numRead != 2)
		{
			llwarns << "Can't read morph target uv" << llendl;
			return FALSE;
		}

		mNumIndices++;
	}

	mAvgDistortion = mAvgDistortion * (1.f/(F32)mNumIndices);
	mAvgDistortion.normVec();

	return TRUE;
}

//-----------------------------------------------------------------------------
// LLPolyMesh::saveLLM()
//-----------------------------------------------------------------------------
BOOL LLPolyMorphData::saveLLM(LLFILE *fp)
{
	if (!fp)
		return FALSE;

	S32 numVertices = mNumIndices;

	llendianswizzle(&numVertices, sizeof(S32), 1);
	if (fwrite(&numVertices, sizeof(S32), 1, fp) != 1)
	{
		llwarns << "Short write" << llendl;
	}
	numVertices = mNumIndices;	// without the swizzle again

	//-------------------------------------------------------------------------
	// write vertices
	//-------------------------------------------------------------------------
	for(S32 v = 0; v < numVertices; v++)
	{
		llendianswizzle(&mVertexIndices[v], sizeof(U32), 1);
		if (fwrite(&mVertexIndices[v], sizeof(U32), 1, fp) != 1)
		{
			llwarns << "Short write" << llendl;
		}
		llendianswizzle(&mVertexIndices[v], sizeof(U32), 1);

		llendianswizzle(&mCoords[v].mV, sizeof(F32), 3);
		if (fwrite(&mCoords[v].mV, sizeof(F32), 3, fp) != 3)
		{
			llwarns << "Short write" << llendl;
		}
		llendianswizzle(&mCoords[v].mV, sizeof(F32), 3);

		llendianswizzle(&mNormals[v].mV, sizeof(F32), 3);
		if (fwrite(&mNormals[v].mV, sizeof(F32), 3, fp) != 3)
		{
			llwarns << "Short write" << llendl;
		}
		llendianswizzle(&mNormals[v].mV, sizeof(F32), 3);

		llendianswizzle(&mBinormals[v].mV, sizeof(F32), 3);
		if (fwrite(&mBinormals[v].mV, sizeof(F32), 3, fp) != 3)
		{
			llwarns << "Short write" << llendl;
		}
		llendianswizzle(&mBinormals[v].mV, sizeof(F32), 3);

		llendianswizzle(&mTexCoords[v].mV, sizeof(F32), 2);
		if (fwrite(&mTexCoords[v].mV, sizeof(F32), 2, fp) != 2)
		{
			llwarns << "Short write" << llendl;
		}
		llendianswizzle(&mTexCoords[v].mV, sizeof(F32), 2);
	}

	return TRUE;
}

//-----------------------------------------------------------------------------
// saveOBJ()
//-----------------------------------------------------------------------------
BOOL LLPolyMorphData::saveOBJ(LLFILE *fp)
{
	if (!fp)
		return FALSE;

	LLPolyMesh mesh(mMesh, NULL);

	LLVector4 *coords     = mesh.getWritableCoords();
	LLVector4 *normals    = mesh.getWritableNormals();
	LLVector2 *tex_coords = mesh.getWritableTexCoords();

	for(U32 vert_index_morph = 0; vert_index_morph < mNumIndices; vert_index_morph++)
	{
		S32 vert_index_mesh = mVertexIndices[vert_index_morph];

		coords[vert_index_mesh]     += LLVector4(mCoords[vert_index_morph]);
		normals[vert_index_mesh]    += LLVector4(mNormals[vert_index_morph]);
		normals[vert_index_mesh].normVec();
		tex_coords[vert_index_mesh] += mTexCoords[vert_index_morph];
	}

	return mesh.saveOBJ(fp);
}

//-----------------------------------------------------------------------------
// setMorphFromMesh()
//-----------------------------------------------------------------------------
BOOL LLPolyMorphData::setMorphFromMesh(LLPolyMesh *morph)
{
	if (!morph)
		return FALSE;

	LLVector4 *morph_coords     = morph->getWritableCoords();
	LLVector4 *morph_normals    = morph->getWritableNormals();
	LLVector3 *morph_binormals  = morph->getWritableBinormals();
	LLVector2 *morph_tex_coords = morph->getWritableTexCoords();

	// We now have the morph loaded as a mesh.  We have to subtract the
	// base mesh to get the delta morph.

	LLPolyMesh delta(mMesh, NULL);
	U32 nverts = delta.getNumVertices();

	LLVector4 *delta_coords     = delta.getWritableCoords();
	LLVector4 *delta_normals    = delta.getWritableNormals();
	LLVector3 *delta_binormals  = delta.getWritableBinormals();
	LLVector2 *delta_tex_coords = delta.getWritableTexCoords();

	U32 num_significant = 0;
	U32 vert_index;
	for( vert_index = 0; vert_index < nverts; vert_index++)
	{
		delta_coords[vert_index]     = morph_coords[vert_index] - delta_coords[vert_index];
		delta_normals[vert_index]    = morph_normals[vert_index] - delta_normals[vert_index];
		delta_binormals[vert_index]  = morph_binormals[vert_index] - delta_binormals[vert_index];
		delta_tex_coords[vert_index] = morph_tex_coords[vert_index] - delta_tex_coords[vert_index];

		// For the normals and binormals, we really want the deltas
		// to be perpendicular to the mesh (bi)normals in the plane
		// that contains both the mesh and morph (bi)normals, such
		// that the morph (bi)normals form the hypotenuses of right
		// triangles. Right now, we just compute the difference vector.

		if (delta_coords[vert_index].length()     > SIGNIFICANT_DELTA
		||  delta_normals[vert_index].length()    > SIGNIFICANT_DELTA
		||  delta_binormals[vert_index].length()  > SIGNIFICANT_DELTA
		||  delta_tex_coords[vert_index].length() > SIGNIFICANT_DELTA)
		{
			num_significant++;
		}
	}

	//-------------------------------------------------------------------------
	// compute new morph
	//-------------------------------------------------------------------------

	// If the morph matches the base mesh, we store one vertex to prevent
	// zero length vectors.

	U32 nindices = num_significant;
	if (num_significant == 0)
		nindices = 1;

	LLVector3* new_coords     = new LLVector3[nindices];
	LLVector3* new_normals    = new LLVector3[nindices];
	LLVector3* new_binormals  = new LLVector3[nindices];
	LLVector2* new_tex_coords = new LLVector2[nindices];
	U32* new_vertex_indices   = new U32[nindices];

    // We'll set the distortion directly
	mTotalDistortion = 0.f;
	mMaxDistortion   = 0.f;
	mAvgDistortion.zeroVec();

	U32 morph_index = 0;
	for( vert_index = 0; vert_index < nverts; vert_index++)
	{
		if (delta_coords[vert_index].length()     > SIGNIFICANT_DELTA
		||  delta_normals[vert_index].length()    > SIGNIFICANT_DELTA
		||  delta_binormals[vert_index].length()  > SIGNIFICANT_DELTA
		||  delta_tex_coords[vert_index].length() > SIGNIFICANT_DELTA
		||  num_significant == 0)
		{
			new_vertex_indices[morph_index] = vert_index;

			new_coords[morph_index]     = LLVector3(delta_coords[vert_index]);
			new_normals[morph_index]    = LLVector3(delta_normals[vert_index]);
			new_binormals[morph_index]  = delta_binormals[vert_index];
			new_tex_coords[morph_index] = delta_tex_coords[vert_index];

			F32 magnitude = new_coords[morph_index].magVec();

			mTotalDistortion += magnitude;
			mAvgDistortion.mV[VX] += fabs(new_coords[morph_index].mV[VX]);
			mAvgDistortion.mV[VY] += fabs(new_coords[morph_index].mV[VY]);
			mAvgDistortion.mV[VZ] += fabs(new_coords[morph_index].mV[VZ]);

			if (magnitude > mMaxDistortion)
			{
				mMaxDistortion = magnitude;
			}

			morph_index++;
			num_significant = 1;
		}
	}

	mAvgDistortion = mAvgDistortion * (1.f/(F32)nindices);
	mAvgDistortion.normVec();

	//-------------------------------------------------------------------------
	// compute the change in the morph
	//-------------------------------------------------------------------------

	// Because meshes are set by continually updating morph weights
	// there is no easy way to reapply the morphs, so we just compute
	// the change in this morph and apply that appropriately weighted.

	for( morph_index = 0; morph_index < mNumIndices; morph_index++)
	{
		vert_index = mVertexIndices[morph_index];

		delta_coords[vert_index]     -= LLVector4(mCoords[morph_index]);
		delta_normals[vert_index]    -= LLVector4(mNormals[morph_index]);
		delta_binormals[vert_index]  -= mBinormals[morph_index];
		delta_tex_coords[vert_index] -= mTexCoords[morph_index];
	}

	//-------------------------------------------------------------------------
	// Update all avatars
	//-------------------------------------------------------------------------

	std::vector< LLCharacter* >::iterator avatar_it;
	for(avatar_it = LLCharacter::sInstances.begin(); avatar_it != LLCharacter::sInstances.end(); ++avatar_it)
	{
		LLVOAvatar* avatarp = (LLVOAvatar*)*avatar_it;

		LLPolyMorphTarget* param = (LLPolyMorphTarget*) avatarp->getVisualParam(mName.c_str());
		if (!param)
		{
			continue;
		}

		F32 weight = param->getLastWeight();
		if (weight == 0.0f)
		{
			continue;
		}

		LLPolyMesh* mesh = avatarp->getMesh(mMesh);
		if (!mesh)
		{
			continue;
		}

		// If we have a vertex mask, just remove it.  It will be recreated.
		if (param->undoMask(TRUE))
		{
			continue;
		}

		LLVector4 *mesh_coords           = mesh->getWritableCoords();
		LLVector4 *mesh_normals          = mesh->getWritableNormals();
		LLVector3 *mesh_binormals        = mesh->getWritableBinormals();
		LLVector2 *mesh_tex_coords       = mesh->getWritableTexCoords();
		LLVector3 *mesh_scaled_normals   = mesh->getScaledNormals();
		LLVector3 *mesh_scaled_binormals = mesh->getScaledBinormals();

		for( vert_index = 0; vert_index < nverts; vert_index++)
		{
			mesh_coords[vert_index]           += delta_coords[vert_index] * weight;
			mesh_tex_coords[vert_index]       += delta_tex_coords[vert_index] * weight;

			mesh_scaled_normals[vert_index]   += LLVector3(delta_normals[vert_index] * weight * NORMAL_SOFTEN_FACTOR);
			LLVector3 normalized_normal        = mesh_scaled_normals[vert_index];
			normalized_normal.normVec();
			mesh_normals[vert_index]           = LLVector4(normalized_normal);

			mesh_scaled_binormals[vert_index] += delta_binormals[vert_index] * weight * NORMAL_SOFTEN_FACTOR;
			LLVector3 tangent                  = mesh_scaled_binormals[vert_index] % normalized_normal;
			LLVector3 normalized_binormal      = normalized_normal % tangent;
			normalized_binormal.normVec();
			mesh_binormals[vert_index]         = normalized_binormal;
		}

		avatarp->dirtyMesh();
	}

	//-------------------------------------------------------------------------
	// reallocate vertices
	//-------------------------------------------------------------------------
	delete [] mVertexIndices;
	delete [] mCoords;
	delete [] mNormals;
	delete [] mBinormals;
	delete [] mTexCoords;

	mVertexIndices = new_vertex_indices;
	mCoords        = new_coords;
	mNormals       = new_normals;
	mBinormals     = new_binormals;
	mTexCoords     = new_tex_coords;
	mNumIndices    = nindices;

	return TRUE;
}

//-----------------------------------------------------------------------------
// LLPolyMorphTargetInfo()
//-----------------------------------------------------------------------------
LLPolyMorphTargetInfo::LLPolyMorphTargetInfo()
	: mIsClothingMorph(FALSE)
{
}

BOOL LLPolyMorphTargetInfo::parseXml(LLXmlTreeNode* node)
{
	llassert( node->hasName( "param" ) && node->getChildByName( "param_morph" ) );

	if (!LLViewerVisualParamInfo::parseXml(node))
		return FALSE;

	// Get mixed-case name
	static LLStdStringHandle name_string = LLXmlTree::addAttributeString("name");
	if( !node->getFastAttributeString( name_string, mMorphName ) )
	{
		llwarns << "Avatar file: <param> is missing name attribute" << llendl;
		return FALSE;  // Continue, ignoring this tag
	}

	static LLStdStringHandle clothing_morph_string = LLXmlTree::addAttributeString("clothing_morph");
	node->getFastAttributeBOOL(clothing_morph_string, mIsClothingMorph);

	LLXmlTreeNode *paramNode = node->getChildByName("param_morph");

        if (NULL == paramNode)
        {
                llwarns << "Failed to getChildByName(\"param_morph\")"
                        << llendl;
                return FALSE;
        }

	for (LLXmlTreeNode* child_node = paramNode->getFirstChild();
		 child_node;
		 child_node = paramNode->getNextChild())
	{
		static LLStdStringHandle name_string = LLXmlTree::addAttributeString("name");
		if (child_node->hasName("volume_morph"))
		{
			std::string volume_name;
			if (child_node->getFastAttributeString(name_string, volume_name))
			{
				LLVector3 scale;
				static LLStdStringHandle scale_string = LLXmlTree::addAttributeString("scale");
				child_node->getFastAttributeVector3(scale_string, scale);
				
				LLVector3 pos;
				static LLStdStringHandle pos_string = LLXmlTree::addAttributeString("pos");
				child_node->getFastAttributeVector3(pos_string, pos);

				mVolumeInfoList.push_back(LLPolyVolumeMorphInfo(volume_name,scale,pos));
			}
		}
	}
	
	return TRUE;
}

//-----------------------------------------------------------------------------
// LLPolyMorphTarget()
//-----------------------------------------------------------------------------
LLPolyMorphTarget::LLPolyMorphTarget(LLPolyMesh *poly_mesh)
	: mMorphData(NULL), mMesh(poly_mesh),
	  mVertMask(NULL),
	  mLastSex(SEX_FEMALE),
	  mNumMorphMasksPending(0)
{
}

//-----------------------------------------------------------------------------
// ~LLPolyMorphTarget()
//-----------------------------------------------------------------------------
LLPolyMorphTarget::~LLPolyMorphTarget()
{
	if (mVertMask)
	{
		delete mVertMask;
	}
}

//-----------------------------------------------------------------------------
// setInfo()
//-----------------------------------------------------------------------------
BOOL LLPolyMorphTarget::setInfo(LLPolyMorphTargetInfo* info)
{
	llassert(mInfo == NULL);
	if (info->mID < 0)
		return FALSE;
	mInfo = info;
	mID = info->mID;
	setWeight(getDefaultWeight(), FALSE );

	LLVOAvatar* avatarp = mMesh->getAvatar();
	LLPolyMorphTargetInfo::volume_info_list_t::iterator iter;
	for (iter = getInfo()->mVolumeInfoList.begin(); iter != getInfo()->mVolumeInfoList.end(); iter++)
	{
		LLPolyVolumeMorphInfo *volume_info = &(*iter);
		for (S32 i = 0; i < avatarp->mNumCollisionVolumes; i++)
		{
			if (avatarp->mCollisionVolumes[i].getName() == volume_info->mName)
			{
				mVolumeMorphs.push_back(LLPolyVolumeMorph(&avatarp->mCollisionVolumes[i],
														  volume_info->mScale,
														  volume_info->mPos));
				break;
			}
		}
	}

	std::string morph_param_name = getInfo()->mMorphName;
	
	mMorphData = mMesh->getMorphData(morph_param_name);
	if (!mMorphData)
	{
		const std::string driven_tag = "_Driven";
		U32 pos = morph_param_name.find(driven_tag);
		if (pos > 0)
		{
			morph_param_name = morph_param_name.substr(0,pos);
			mMorphData = mMesh->getMorphData(morph_param_name);
		}
	}
	if (!mMorphData)
	{
		llwarns << "No morph target named " << morph_param_name << " found in mesh." << llendl;
		return FALSE;  // Continue, ignoring this tag
	}
	return TRUE;
}

#if 0 // obsolete
//-----------------------------------------------------------------------------
// parseData()
//-----------------------------------------------------------------------------
BOOL LLPolyMorphTarget::parseData(LLXmlTreeNode* node)
{
	LLPolyMorphTargetInfo* info = new LLPolyMorphTargetInfo;

	info->parseXml(node);
	if (!setInfo(info))
	{
		delete info;
		return FALSE;
	}
	return TRUE;
}
#endif

//-----------------------------------------------------------------------------
// getVertexDistortion()
//-----------------------------------------------------------------------------
LLVector3 LLPolyMorphTarget::getVertexDistortion(S32 requested_index, LLPolyMesh *mesh)
{
	if (!mMorphData || mMesh != mesh) return LLVector3::zero;

	for(U32 index = 0; index < mMorphData->mNumIndices; index++)
	{
		if (mMorphData->mVertexIndices[index] == (U32)requested_index)
		{
			return mMorphData->mCoords[index];
		}
	}

	return LLVector3::zero;
}

//-----------------------------------------------------------------------------
// getFirstDistortion()
//-----------------------------------------------------------------------------
const LLVector3 *LLPolyMorphTarget::getFirstDistortion(U32 *index, LLPolyMesh **poly_mesh)
{
	if (!mMorphData) return &LLVector3::zero;

	LLVector3* resultVec;
	mMorphData->mCurrentIndex = 0;
	if (mMorphData->mNumIndices)
	{
		resultVec = &mMorphData->mCoords[mMorphData->mCurrentIndex];
		if (index != NULL)
		{
			*index = mMorphData->mVertexIndices[mMorphData->mCurrentIndex];
		}
		if (poly_mesh != NULL)
		{
			*poly_mesh = mMesh;
		}

		return resultVec;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// getNextDistortion()
//-----------------------------------------------------------------------------
const LLVector3 *LLPolyMorphTarget::getNextDistortion(U32 *index, LLPolyMesh **poly_mesh)
{
	if (!mMorphData) return &LLVector3::zero;

	LLVector3* resultVec;
	mMorphData->mCurrentIndex++;
	if (mMorphData->mCurrentIndex < mMorphData->mNumIndices)
	{
		resultVec = &mMorphData->mCoords[mMorphData->mCurrentIndex];
		if (index != NULL)
		{
			*index = mMorphData->mVertexIndices[mMorphData->mCurrentIndex];
		}
		if (poly_mesh != NULL)
		{
			*poly_mesh = mMesh;
		}
		return resultVec;
	}
	return NULL;
}

//-----------------------------------------------------------------------------
// getTotalDistortion()
//-----------------------------------------------------------------------------
F32	LLPolyMorphTarget::getTotalDistortion() 
{ 
	if (mMorphData) 
	{
		return mMorphData->mTotalDistortion; 
	}
	else 
	{
		return 0.f;
	}
}

//-----------------------------------------------------------------------------
// getAvgDistortion()
//-----------------------------------------------------------------------------
const LLVector3& LLPolyMorphTarget::getAvgDistortion()	
{
	if (mMorphData) 
	{
		return mMorphData->mAvgDistortion; 
	}
	else 
	{
		return LLVector3::zero;
	}
}

//-----------------------------------------------------------------------------
// getMaxDistortion()
//-----------------------------------------------------------------------------
F32	LLPolyMorphTarget::getMaxDistortion() 
{
	if (mMorphData) 
	{
		return mMorphData->mMaxDistortion; 
	}
	else
	{
		return 0.f;
	}
}

//-----------------------------------------------------------------------------
// apply()
//-----------------------------------------------------------------------------
void LLPolyMorphTarget::apply( ESex avatar_sex )
{
	if (!mMorphData || mNumMorphMasksPending > 0)
	{
		return;
	}

	mLastSex = avatar_sex;

	// Check for NaN condition (NaN is detected if a variable doesn't equal itself.
	if (mCurWeight != mCurWeight)
	{
		mCurWeight = 0.0;
	}
	if (mLastWeight != mLastWeight)
	{
		mLastWeight = mCurWeight+.001;
	}

	// perform differential update of morph
	F32 delta_weight = ( getSex() & avatar_sex ) ? (mCurWeight - mLastWeight) : (getDefaultWeight() - mLastWeight);
	// store last weight
	mLastWeight += delta_weight;

	if (delta_weight != 0.f)
	{
		llassert(!mMesh->isLOD());
		LLVector4 *coords = mMesh->getWritableCoords();

		LLVector3 *scaled_normals = mMesh->getScaledNormals();
		LLVector4 *normals = mMesh->getWritableNormals();

		LLVector3 *scaled_binormals = mMesh->getScaledBinormals();
		LLVector3 *binormals = mMesh->getWritableBinormals();

		LLVector4 *clothing_weights = mMesh->getWritableClothingWeights();
		LLVector2 *tex_coords = mMesh->getWritableTexCoords();

		F32 *maskWeightArray = (mVertMask) ? mVertMask->getMorphMaskWeights() : NULL;

		for(U32 vert_index_morph = 0; vert_index_morph < mMorphData->mNumIndices; vert_index_morph++)
		{
			S32 vert_index_mesh = mMorphData->mVertexIndices[vert_index_morph];

			F32 maskWeight = 1.f;
			if (maskWeightArray)
			{
				maskWeight = maskWeightArray[vert_index_morph];
			}

			coords[vert_index_mesh] += LLVector4(mMorphData->mCoords[vert_index_morph] * delta_weight * maskWeight);

			if (getInfo()->mIsClothingMorph && clothing_weights)
			{
				LLVector3 clothing_offset = mMorphData->mCoords[vert_index_morph] * delta_weight * maskWeight;
				LLVector4* clothing_weight = &clothing_weights[vert_index_mesh];
				clothing_weight->mV[VX] += clothing_offset.mV[VX];
				clothing_weight->mV[VY] += clothing_offset.mV[VY];
				clothing_weight->mV[VZ] += clothing_offset.mV[VZ];
				clothing_weight->mV[VW] = maskWeight;
			}

			// calculate new normals based on half angles
			scaled_normals[vert_index_mesh] += mMorphData->mNormals[vert_index_morph] * delta_weight * maskWeight * NORMAL_SOFTEN_FACTOR;
			LLVector3 normalized_normal = scaled_normals[vert_index_mesh];
			normalized_normal.normVec();
			normals[vert_index_mesh] = LLVector4(normalized_normal);

			// calculate new binormals
			scaled_binormals[vert_index_mesh] += mMorphData->mBinormals[vert_index_morph] * delta_weight * maskWeight * NORMAL_SOFTEN_FACTOR;
			LLVector3 tangent = scaled_binormals[vert_index_mesh] % normalized_normal;
			LLVector3 normalized_binormal = normalized_normal % tangent; 
			normalized_binormal.normVec();
			binormals[vert_index_mesh] = normalized_binormal;

			tex_coords[vert_index_mesh] += mMorphData->mTexCoords[vert_index_morph] * delta_weight * maskWeight;
		}

		// now apply volume changes
		for( volume_list_t::iterator iter = mVolumeMorphs.begin(); iter != mVolumeMorphs.end(); iter++ )
		{
			LLPolyVolumeMorph* volume_morph = &(*iter);
			LLVector3 scale_delta = volume_morph->mScale * delta_weight;
			LLVector3 pos_delta = volume_morph->mPos * delta_weight;
			
			volume_morph->mVolume->setScale(volume_morph->mVolume->getScale() + scale_delta);
			volume_morph->mVolume->setPosition(volume_morph->mVolume->getPosition() + pos_delta);
		}
	}

	if (mNext)
	{
		mNext->apply(avatar_sex);
	}
}

//-----------------------------------------------------------------------------
// applyMask()
//-----------------------------------------------------------------------------
void	LLPolyMorphTarget::applyMask(U8 *maskTextureData, S32 width, S32 height, S32 num_components, BOOL invert)
{
	LLVector4 *clothing_weights = getInfo()->mIsClothingMorph ? mMesh->getWritableClothingWeights() : NULL;

	if (!mVertMask)
	{
		mVertMask = new LLPolyVertexMask(mMorphData);
		mNumMorphMasksPending--;
	}
	else
	{
		// remove effect of previous mask
		undoMask(FALSE);
	}

	mLastWeight = 0.f;

	mVertMask->generateMask(maskTextureData, width, height, num_components, invert, clothing_weights);

	apply(mLastSex);
}

//-----------------------------------------------------------------------------
// undoMask()
//-----------------------------------------------------------------------------
BOOL	LLPolyMorphTarget::undoMask(BOOL delete_mask)
{
	if (!mVertMask)
		{
	    return FALSE;
	}

	// remove effect of previous mask

	LLVector4 *clothing_weights = getInfo()->mIsClothingMorph ? mMesh->getWritableClothingWeights() : NULL;

	F32 *mask_weights = mVertMask->getMorphMaskWeights();

			LLVector4 *coords = mMesh->getWritableCoords();
			LLVector3 *scaled_normals = mMesh->getScaledNormals();
			LLVector3 *scaled_binormals = mMesh->getScaledBinormals();
			LLVector2 *tex_coords = mMesh->getWritableTexCoords();

			for(U32 vert = 0; vert < mMorphData->mNumIndices; vert++)
			{
				F32 mask_weight = 1.f;
				if (mask_weights)
				{
					mask_weight = mask_weights[vert];
				}

				F32 lastMaskWeight = mLastWeight * mask_weights[vert];
				S32 out_vert = mMorphData->mVertexIndices[vert];

				// remove effect of existing masked morph
				coords[out_vert] -= LLVector4(mMorphData->mCoords[vert]) * lastMaskWeight;
				scaled_normals[out_vert] -= mMorphData->mNormals[vert] * lastMaskWeight * NORMAL_SOFTEN_FACTOR;
				scaled_binormals[out_vert] -= mMorphData->mBinormals[vert] * lastMaskWeight * NORMAL_SOFTEN_FACTOR;
				tex_coords[out_vert] -= mMorphData->mTexCoords[vert] * lastMaskWeight;

				if (clothing_weights)
				{
					LLVector3 clothing_offset = mMorphData->mCoords[vert] * lastMaskWeight;
					LLVector4* clothing_weight = &clothing_weights[out_vert];
					clothing_weight->mV[VX] -= clothing_offset.mV[VX];
					clothing_weight->mV[VY] -= clothing_offset.mV[VY];
					clothing_weight->mV[VZ] -= clothing_offset.mV[VZ];
				}
			}

	// set last weight to 0, since we've removed the effect of this morph
	mLastWeight = 0.f;

	if (delete_mask)
	{
		delete mVertMask;
		mVertMask = NULL;
		addPendingMorphMask();
	}

	return TRUE;
}


//-----------------------------------------------------------------------------
// LLPolyVertexMask()
//-----------------------------------------------------------------------------
LLPolyVertexMask::LLPolyVertexMask(LLPolyMorphData* morph_data)
{
	mWeights = new F32[morph_data->mNumIndices];
	mMorphData = morph_data;
	mWeightsGenerated = FALSE;
}

//-----------------------------------------------------------------------------
// ~LLPolyVertexMask()
//-----------------------------------------------------------------------------
LLPolyVertexMask::~LLPolyVertexMask()
{
	delete[] mWeights;
}

//-----------------------------------------------------------------------------
// generateMask()
//-----------------------------------------------------------------------------
void LLPolyVertexMask::generateMask(U8 *maskTextureData, S32 width, S32 height, S32 num_components, BOOL invert, LLVector4 *clothing_weights)
{
// RN debug output that uses Image Debugger (http://www.cs.unc.edu/~baxter/projects/imdebug/)
//	BOOL debugImg = FALSE; 
//	if (debugImg)
//	{
//		if (invert)
//		{
//			imdebug("lum rbga=rgba b=8 w=%d h=%d *-1 %p", width, height, maskTextureData);
//		}
//		else
//		{
//			imdebug("lum rbga=rgba b=8 w=%d h=%d %p", width, height, maskTextureData);
//		}
//	}
	for (U32 index = 0; index < mMorphData->mNumIndices; index++)
	{
		S32 vertIndex = mMorphData->mVertexIndices[index];
		const S32 *sharedVertIndex = mMorphData->mMesh->getSharedVert(vertIndex);
		LLVector2 uvCoords;

		if (sharedVertIndex)
		{
			uvCoords = mMorphData->mMesh->getUVs(*sharedVertIndex);
		}
		else
		{
			uvCoords = mMorphData->mMesh->getUVs(vertIndex);
		}
		U32 s = llclamp((U32)(uvCoords.mV[VX] * (F32)(width - 1)), (U32)0, (U32)width - 1);
		U32 t = llclamp((U32)(uvCoords.mV[VY] * (F32)(height - 1)), (U32)0, (U32)height - 1);
		
		mWeights[index] = ((F32) maskTextureData[((t * width + s) * num_components) + (num_components - 1)]) / 255.f;
		
		if (invert) 
		{
			mWeights[index] = 1.f - mWeights[index];
		}

		// now apply step function
		// mWeights[index] = mWeights[index] > 0.95f ? 1.f : 0.f;

		if (clothing_weights)
		{
			clothing_weights[vertIndex].mV[VW] = mWeights[index];
		}
	}
	mWeightsGenerated = TRUE;
}

//-----------------------------------------------------------------------------
// getMaskForMorphIndex()
//-----------------------------------------------------------------------------
F32* LLPolyVertexMask::getMorphMaskWeights()
{
	if (!mWeightsGenerated)
	{
		return NULL;
	}
	
	return mWeights;
}
