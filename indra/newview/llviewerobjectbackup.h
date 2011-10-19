/** 
 * @file llviewerobjectbackup.h
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
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

#include "llviewerinventory.h"

enum export_states {
	EXPORT_INIT,
	EXPORT_STRUCTURE,
	EXPORT_TEXTURES,
	EXPORT_LLSD,
	EXPORT_DONE,
	EXPORT_FAILED
};

class LLObjectBackup : public LLFloater
{
public:
	virtual ~LLObjectBackup();

	// Floater stuff
	virtual void show(bool exporting);
	virtual void draw();
	virtual void onClose(bool app_quitting);

	// Static accessor
	static LLObjectBackup* getInstance();

	// Export idle callback
	static void exportWorker(void *userdata);

	// Import entry point
	void importObject(bool upload=FALSE);
	void importObject_continued(AIFilePicker* filepicker);

	// Export entry point
	void exportObject();
	void exportObject_continued(AIFilePicker* filepicker);

	// Update map from texture worker
	void updateMap(LLUUID uploaded_asset);

	// Move to next texture upload
	void uploadNextAsset();

	// Folder public geter
	std::string getfolder() { return mFolder; };

	// Prim updated callback
	void primUpdate(LLViewerObject* object);

	// New prim call back
	bool newPrim(LLViewerObject* pobject);

	static const U32 TEXTURE_OK = 0x00;
	static const U32 TEXTURE_BAD_PERM = 0x01;
	static const U32 TEXTURE_MISSING = 0x02;
	static const U32 TEXTURE_BAD_ENCODING = 0x04;
	static const U32 TEXTURE_IS_NULL = 0x08;
	static const U32 TEXTURE_SAVED_FAILED = 0x10;

	// Is ready for next texture?
	bool mNextTextureReady;

	// Export state machine
	enum export_states mExportState; 

	// Export result flags for textures.
	U32 mNonExportedTextures;

private:
	// Static singleton stuff
	LLObjectBackup();	
	static LLObjectBackup* sInstance;

	// Update the floater with status numbers
	void updateImportNumbers();
	void updateExportNumbers();

	// Permissions stuff.
	bool validatePerms(const LLPermissions* item_permissions);
	LLUUID validateTextureID(LLUUID asset_id);

	// Convert a selection list of objects to LLSD
	LLSD primsToLLSD(LLViewerObject::child_list_t child_list, bool is_attachment);

	// Start the import process
	void importFirstObject();

	// Move to the next import group
	void importNextObject();

	// Export the next texture in list
	void exportNextTexture();

	// Apply LLSD to object
	void xmlToPrim(LLSD prim_llsd, LLViewerObject* pobject);

	// Rez a prim at a given position (note not agent offset X/Y screen for raycast)
	void rezAgentOffset(LLVector3 offset);	

	// Get an offset from the agent based on rotation and current pos
	LLVector3 offsetAgent(LLVector3 offset);

	// Are we active flag
	bool mRunning;

	// File and folder name control 
	std::string mFileName;
	std::string mFolder;

	// True if we need to rebase the assets
	bool mRetexture;

	// Counts of import and export objects and prims
	U32 mObjects;
	U32 mCurObject;
	U32 mPrims;
	U32 mCurPrim;

	// No prims rezed
	U32 mRezCount;

	// Rebase map
	std::map<LLUUID,LLUUID> mAssetMap;

	// Export texture list
	std::list<LLUUID> mTexturesList;

	// Import object tracking
	std::vector<LLViewerObject*> mToSelect;
	std::vector<LLViewerObject*>::iterator mProcessIter;

	// Working LLSD holders
	LLUUID mCurrentAsset;
	LLSD mLLSD;
	LLSD mThisGroup;
	LLUUID mExpectingUpdate;

	// Working llsd itterators for objects and linksets
	LLSD::map_const_iterator mPrimImportIter;
	LLSD::array_const_iterator mGroupPrimImportIter;

	// Root pos and rotation and central root pos for link set
	LLVector3 mRootPos;
	LLQuaternion mRootRot;
	LLVector3 mRootRootPos;
	LLVector3 mGroupOffset;

	// Agent inital pos and rot when starting import
	LLVector3 mAgentPos;
	LLQuaternion mAgentRot;
};

