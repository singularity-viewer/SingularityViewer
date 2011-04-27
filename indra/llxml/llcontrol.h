/** 
 * @file llcontrol.h
 * @brief A mechanism for storing "control state" for a program
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

#ifndef LL_LLCONTROL_H
#define LL_LLCONTROL_H

#include "llevent.h"
#include "llnametable.h"
#include "llmap.h"
#include "llstring.h"
#include "llrect.h"
#include "v4color.h"
#include "v4coloru.h"

#include "llcontrolgroupreader.h"

#include <vector>

// *NOTE: boost::visit_each<> generates warning 4675 on .net 2003
// Disable the warning for the boost includes.
#if LL_WINDOWS
# if (_MSC_VER >= 1300 && _MSC_VER < 1400)
#   pragma warning(push)
#   pragma warning( disable : 4675 )
# endif
#endif

#include <boost/bind.hpp>
#include <boost/signal.hpp>

#if LL_WINDOWS
# if (_MSC_VER >= 1300 && _MSC_VER < 1400)
#   pragma warning(pop)
# endif
#endif

class LLVector3;
class LLVector3d;
class LLColor3;
class LLColor4U;

const BOOL NO_PERSIST = FALSE;

// Saved at end of session
class LLControlGroup; //Defined further down
extern LLControlGroup gSavedSettings;		//Default control group used in LLCachedControl
extern LLControlGroup gSavedPerAccountSettings;	//For ease

typedef enum e_control_type
{
	TYPE_U32 = 0,
	TYPE_S32,
	TYPE_F32,
	TYPE_BOOLEAN,
	TYPE_STRING,
	TYPE_VEC3,
	TYPE_VEC3D,
	TYPE_RECT,
	TYPE_COL4,
	TYPE_COL3,
	TYPE_COL4U,
	TYPE_LLSD,
	TYPE_COUNT
} eControlType;

class LLControlVariable : public LLRefCount
{
	friend class LLControlGroup;
	typedef boost::signal<void(const LLSD&)> signal_t;

private:
	std::string		mName;
	std::string		mComment;
	eControlType	mType;
	bool			mPersist;
	bool			mHideFromSettingsEditor;
	std::vector<LLSD> mValues;
	
	boost::shared_ptr<signal_t> mSignal;	//Signals are non-copyable. Therefore, using a pointer so vars can 'share' signals for COA

	//COA stuff:
	bool			mIsCOA;				//To have COA connection set.
	bool			mIsCOAParent;		//if true, use if settingsperaccount is false.
	LLControlVariable *mCOAConnectedVar;//Because the two vars refer to eachother, LLPointer would be a circular refrence..
public:
	LLControlVariable(const std::string& name, eControlType type,
					  LLSD initial, const std::string& comment,
					  bool persist = true, bool hidefromsettingseditor = false, bool IsCOA = false);

	virtual ~LLControlVariable();
	
	const std::string& getName() const { return mName; }
	const std::string& getComment() const { return mComment; }

	eControlType type()		{ return mType; }
	bool isType(eControlType tp) { return tp == mType; }

	void resetToDefault(bool fire_signal = false);

	signal_t* getSignal() { return mSignal.get(); }

	bool isDefault() { return (mValues.size() == 1); }
	bool isSaveValueDefault();
	bool isPersisted() { return mPersist; }
	bool isHiddenFromSettingsEditor() { return mHideFromSettingsEditor; }
	LLSD get()			const	{ return getValue(); }
	LLSD getValue()		const	{ return mValues.back(); }
	LLSD getDefault()	const	{ return mValues.front(); }
	LLSD getSaveValue() const;

	void set(const LLSD& val)	{ setValue(val); }
	void setValue(const LLSD& value, bool saved_value = TRUE);
	void setDefaultValue(const LLSD& value);
	void setPersist(bool state);
	void setHiddenFromSettingsEditor(bool hide);
	void setComment(const std::string& comment);

	void firePropertyChanged()
	{
		(*mSignal)(mValues.back());
	}

	//COA stuff
	bool isCOA()		const	{ return mIsCOA; }
	bool isCOAParent()	const	{ return mIsCOAParent; }
	LLControlVariable *getCOAConnection() const	{ return mCOAConnectedVar; }
	LLControlVariable *getCOAActive();
	void setIsCOA(bool IsCOA)  { mIsCOA=IsCOA; }
	void setCOAConnect(LLControlVariable *pConnect, bool IsParent) 
	{
		mIsCOAParent=IsParent;
		mCOAConnectedVar=pConnect;
		if(!IsParent)
			mSignal = pConnect->mSignal; //Share a single signal.
	}
private:
	LLSD getComparableValue(const LLSD& value);
	bool llsd_compare(const LLSD& a, const LLSD & b);

};

//const U32 STRING_CACHE_SIZE = 10000;
class LLControlGroup : public LLControlGroupReader
{
protected:
	typedef std::map<std::string, LLPointer<LLControlVariable> > ctrl_name_table_t;
	ctrl_name_table_t mNameTable;
	std::set<std::string> mWarnings;
	std::string mTypeString[TYPE_COUNT];

	eControlType typeStringToEnum(const std::string& typestr);
	std::string typeEnumToString(eControlType typeenum);	
	std::set<std::string> mIncludedFiles; //To prevent perpetual recursion.
public:
	LLControlGroup();
	~LLControlGroup();
	void cleanup();
	
	LLPointer<LLControlVariable> getControl(const std::string& name);

	struct ApplyFunctor
	{
		virtual ~ApplyFunctor() {};
		virtual void apply(const std::string& name, LLControlVariable* control) = 0;
	};
	void applyToAll(ApplyFunctor* func);

	BOOL declareControl(const std::string& name, eControlType type, const LLSD initial_val, const std::string& comment, BOOL persist, BOOL hidefromsettingseditor = FALSE, bool IsCOA = false);
	BOOL declareU32(const std::string& name, U32 initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareS32(const std::string& name, S32 initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareF32(const std::string& name, F32 initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareBOOL(const std::string& name, BOOL initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareString(const std::string& name, const std::string &initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareVec3(const std::string& name, const LLVector3 &initial_val,const std::string& comment,  BOOL persist = TRUE);
	BOOL declareVec3d(const std::string& name, const LLVector3d &initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareRect(const std::string& name, const LLRect &initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareColor4U(const std::string& name, const LLColor4U &initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareColor4(const std::string& name, const LLColor4 &initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareColor3(const std::string& name, const LLColor3 &initial_val, const std::string& comment, BOOL persist = TRUE);
	BOOL declareLLSD(const std::string& name, const LLSD &initial_val, const std::string& comment, BOOL persist = TRUE);
	
	std::string 	findString(const std::string& name);

	std::string 	getString(const std::string& name);
	LLWString	getWString(const std::string& name);
	std::string	getText(const std::string& name);
	LLVector3	getVector3(const std::string& name);
	LLVector3d	getVector3d(const std::string& name);
	LLRect		getRect(const std::string& name);
	BOOL		getBOOL(const std::string& name);
	S32			getS32(const std::string& name);
	F32			getF32(const std::string& name);
	U32			getU32(const std::string& name);
	LLSD        getLLSD(const std::string& name);


	// Note: If an LLColor4U control exists, it will cast it to the correct
	// LLColor4 for you.
	LLColor4	getColor(const std::string& name);
	LLColor4U	getColor4U(const std::string& name);
	LLColor4	getColor4(const std::string& name);
	LLColor3	getColor3(const std::string& name);

	void	setBOOL(const std::string& name, BOOL val);
	void	setS32(const std::string& name, S32 val);
	void	setF32(const std::string& name, F32 val);
	void	setU32(const std::string& name, U32 val);
	void	setString(const std::string&  name, const std::string& val);
	void	setVector3(const std::string& name, const LLVector3 &val);
	void	setVector3d(const std::string& name, const LLVector3d &val);
	void	setRect(const std::string& name, const LLRect &val);
	void	setColor4U(const std::string& name, const LLColor4U &val);
	void	setColor4(const std::string& name, const LLColor4 &val);
	void	setColor3(const std::string& name, const LLColor3 &val);
	void    setLLSD(const std::string& name, const LLSD& val);
	void	setValue(const std::string& name, const LLSD& val);
	
	
	BOOL    controlExists(const std::string& name);

	// Returns number of controls loaded, 0 if failed
	// If require_declaration is false, will auto-declare controls it finds
	// as the given type.
	U32	loadFromFileLegacy(const std::string& filename, BOOL require_declaration = TRUE, eControlType declare_as = TYPE_STRING);
 	U32 saveToFile(const std::string& filename, BOOL nondefault_only);
 	U32	loadFromFile(const std::string& filename, bool default_values = false);
	void	resetToDefaults();

	
	// Ignorable Warnings
	
	// Add a config variable to be reset on resetWarnings()
	void addWarning(const std::string& name);
	BOOL getWarning(const std::string& name);
	void setWarning(const std::string& name, BOOL val);
	
	// Resets all ignorables
	void resetWarnings();

	//COA stuff
	void connectToCOA(LLControlVariable *pConnecter, const std::string& name, eControlType type, const LLSD initial_val, const std::string& comment, BOOL persist);
	void connectCOAVars(LLControlGroup &OtherGroup);
	void updateCOASetting(bool coa_enabled);
	bool handleCOASettingChange(const LLSD& newvalue);
};

//! Helper function for LLCachedControl
template <class T> 
eControlType get_control_type(const T& in, LLSD& out)
{
	llerrs << "Usupported control type: " << typeid(T).name() << "." << llendl;
	return TYPE_COUNT;
}

//! Publish/Subscribe object to interact with LLControlGroups.

//! An LLCachedControl instance to connect to a LLControlVariable
//! without have to manually create and bind a listener to a local
//! object.

template <class T>
class LLCachedControl
{
private:
    T mCachedValue;
    LLPointer<LLControlVariable> mControl;
    boost::signals::connection mConnection;
	LLControlGroup *mControlGroup;

public:
	LLCachedControl(const std::string& name, const T& default_value, LLControlGroup *group, const std::string& comment = "Declared In Code") 
	{Init(name,default_value,comment,*group);} //for gSavedPerAccountSettings, etc
	LLCachedControl(const std::string& name, const T& default_value, LLControlGroup &group, const std::string& comment = "Declared In Code")
	{Init(name,default_value,comment,group);}  //for LLUI::sConfigGroup, etc
	LLCachedControl(const std::string& name,  
					const T& default_value, 
					const std::string& comment = "Declared In Code",
					LLControlGroup &group = gSavedSettings)
	{Init(name,default_value,comment,group);}  //for default (gSavedSettings)
private:
	void Init(	const std::string& name, 
				const T& default_value, 
				const std::string& comment,
				LLControlGroup &group)
	{
		mControlGroup = &group;
		mControl = mControlGroup->getControl(name);
		if(mControl.isNull())
		{
			declareTypedControl(*mControlGroup, name, default_value, comment);
			mControl = mControlGroup->getControl(name);
			if(mControl.isNull())
			{
				llerrs << "The control could not be created!!!" << llendl;
			}

			mCachedValue = default_value;
		}
		else
		{
			handleValueChange(mControl->getValue());
		}

		if(mConnection.connected())
			mConnection.disconnect();
		// Add a listener to the controls signal...
		// and store the connection...
		mConnection = mControl->getSignal()->connect(
			boost::bind(&LLCachedControl<T>::handleValueChange, this, _1)
			);
	}
public:
	~LLCachedControl()
	{
		if(mConnection.connected())
		{
			mConnection.disconnect();
		}
	}

	LLCachedControl& operator =(const T& newvalue)
	{
	   setTypeValue(*mControl, newvalue);
	   return *this;
	}
	
	operator const T&() const { return mCachedValue; }


	/*	Sometimes implicit casting doesn't work.
		For instance, something like "LLCachedControl<LLColor4> color("blah",LLColor4()); color.getValue();" 
		will not compile as it will look for the function getValue() in LLCachedControl, which doesn't exist.
			Use 'color.get().getValue()' instead if something like this happens.
		
		Manually casting to (const T) would work too, but it's ugly and requires knowledge of LLCachedControl's internals
	*/
	const T &get() const { return mCachedValue; } 

private:
	void declareTypedControl(LLControlGroup& group, 
							 const std::string& name, 
							 const T& default_value,
							 const std::string& comment)
	{
		LLSD init_value;
		eControlType type = get_control_type<T>(default_value, init_value);
		if(type < TYPE_COUNT)
		{
			group.declareControl(name, type, init_value, comment, FALSE);
		}
	}

	void setValue(const LLSD& newvalue) //default behavior
	{
		mCachedValue = (const T &)newvalue;
	}

	bool handleValueChange(const LLSD& newvalue)
	{
		setValue(newvalue);
		return true;
	}

	void setTypeValue(LLControlVariable& c, const T& v)
	{
		// Implicit conversion from T to LLSD...
		c.set(v);
	}
};

template <> inline void LLCachedControl<LLColor4>::setValue(const LLSD& newvalue)
{
	if(this->mControl->isType(TYPE_COL4U))
		//a color4u LLSD cannot be auto-converted to color4.. so do it manually.
		this->mCachedValue.set(LLColor4U(newvalue)); 
	else
		this->mCachedValue = (const LLColor4 &)newvalue;
}
template <> inline void LLCachedControl<U32>::setValue(const LLSD& newvalue)
{
	if(mControl->isType(TYPE_U32) || mControl->isType(TYPE_S32)) //LLSD does not support U32 fully
		mCachedValue = (U32)newvalue.asInteger();
	else if(this->mControl->isType(TYPE_F32))
		mCachedValue = (U32)newvalue.asReal();
	else
		mCachedValue = (U32)0; //What to do...
}

//Following is actually defined in newview/llviewercontrol.cpp, but extern access is fine (Unless GCC bites me)
template <> eControlType get_control_type<U32>(const U32& in, LLSD& out);
template <> eControlType get_control_type<S32>(const S32& in, LLSD& out);
template <> eControlType get_control_type<F32>(const F32& in, LLSD& out);
template <> eControlType get_control_type<bool> (const bool& in, LLSD& out); 
// Yay BOOL, its really an S32.
//template <> eControlType get_control_type<BOOL> (const BOOL& in, LLSD& out) 
template <> eControlType get_control_type<std::string>(const std::string& in, LLSD& out);
template <> eControlType get_control_type<LLVector3>(const LLVector3& in, LLSD& out);
template <> eControlType get_control_type<LLVector3d>(const LLVector3d& in, LLSD& out); 
template <> eControlType get_control_type<LLRect>(const LLRect& in, LLSD& out);
template <> eControlType get_control_type<LLColor4>(const LLColor4& in, LLSD& out);
template <> eControlType get_control_type<LLColor3>(const LLColor3& in, LLSD& out);
template <> eControlType get_control_type<LLColor4U>(const LLColor4U& in, LLSD& out); 
template <> eControlType get_control_type<LLSD>(const LLSD& in, LLSD& out);
#endif
