/** 
 * @file llinstancetracker.h
 * @brief LLInstanceTracker is a mixin class that automatically tracks object
 *        instances with or without an associated key
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 * 
 * Copyright (c) 2000-2010, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlife.com/developers/opensource/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlife.com/developers/opensource/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 * 
 */

#ifndef LL_LLINSTANCETRACKER_H
#define LL_LLINSTANCETRACKER_H

#include <map>

#include "string_table.h"
#include <boost/utility.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/iterator/indirect_iterator.hpp>

class LL_COMMON_API LLInstanceTrackerBase : public boost::noncopyable
{
	protected:
		static void * & getInstances(std::type_info const & info);
};
/// This mix-in class adds support for tracking all instances of the specified class parameter T
/// The (optional) key associates a value of type KEY with a given instance of T, for quick lookup
/// If KEY is not provided, then instances are stored in a simple set
/// @NOTE: see explicit specialization below for default KEY==T* case
template<typename T, typename KEY = T*>
class LLInstanceTracker : public LLInstanceTrackerBase
{
	typedef typename std::map<KEY, T*> InstanceMap;
	typedef LLInstanceTracker<T, KEY> MyT;
	typedef boost::function<const KEY&(typename InstanceMap::value_type&)> KeyGetter;
	typedef boost::function<T*(typename InstanceMap::value_type&)> InstancePtrGetter;
public:
	/// Dereferencing key_iter gives you a const KEY&
	typedef boost::transform_iterator<KeyGetter, typename InstanceMap::iterator> key_iter;
	/// Dereferencing instance_iter gives you a T&
	typedef boost::indirect_iterator< boost::transform_iterator<InstancePtrGetter, typename InstanceMap::iterator> > instance_iter;

	static T* getInstance(const KEY& k)
	{
		typename InstanceMap::const_iterator found = getMap_().find(k);
		return (found == getMap_().end()) ? NULL : found->second;
	}

	static key_iter beginKeys()
	{
		return boost::make_transform_iterator(getMap_().begin(),
											  boost::bind(&InstanceMap::value_type::first, _1));
	}
	static key_iter endKeys()
	{
		return boost::make_transform_iterator(getMap_().end(),
											  boost::bind(&InstanceMap::value_type::first, _1));
	}
	static instance_iter beginInstances()
	{
		return instance_iter(boost::make_transform_iterator(getMap_().begin(),
															boost::bind(&InstanceMap::value_type::second, _1)));
	}
	static instance_iter endInstances()
	{
		return instance_iter(boost::make_transform_iterator(getMap_().end(),
															boost::bind(&InstanceMap::value_type::second, _1)));
	}
	static S32 instanceCount() { return getMap_().size(); }
protected:
	LLInstanceTracker(KEY key) { add_(key); }
	virtual ~LLInstanceTracker() { remove_(); }
	virtual void setKey(KEY key) { remove_(); add_(key); }
	virtual const KEY& getKey() const { return mKey; }

private:
	void add_(KEY key) 
	{ 
		mKey = key; 
		getMap_()[key] = static_cast<T*>(this); 
	}
	void remove_()
	{
		getMap_().erase(mKey);
	}

    static InstanceMap& getMap_()
    {
		void * & instances = getInstances(typeid(MyT));
        if (! instances)
        {
            instances = new InstanceMap;
        }
        return * static_cast<InstanceMap*>(instances);
    }

private:

	KEY mKey;
};

/// explicit specialization for default case where KEY is T*
/// use a simple std::set<T*>
template<typename T>
class LLInstanceTracker<T, T*> : public LLInstanceTrackerBase
{
	typedef typename std::set<T*> InstanceSet;
	typedef LLInstanceTracker<T, T*> MyT;
public:
	/// Dereferencing key_iter gives you a T* (since T* is the key)
	typedef typename InstanceSet::iterator key_iter;
	/// Dereferencing instance_iter gives you a T&
	typedef boost::indirect_iterator<key_iter> instance_iter;

	/// for completeness of analogy with the generic implementation
	static T* getInstance(T* k) { return k; }
	static S32 instanceCount() { return getSet_().size(); }

	// Instantiate this to get access to iterators for this type.  It's a 'guard' in the sense
	// that it treats deletes of this type as errors as long as there is an instance of
	// this class alive in scope somewhere (i.e. deleting while iterating is bad).
	class LLInstanceTrackerScopedGuard
	{
	public:
		LLInstanceTrackerScopedGuard()
		{
			++sIterationNestDepth;
		}

		~LLInstanceTrackerScopedGuard()
		{
			--sIterationNestDepth;
		}

		static instance_iter beginInstances() {	return instance_iter(getSet_().begin()); }
		static instance_iter endInstances() { return instance_iter(getSet_().end()); }
		static key_iter beginKeys() { return getSet_().begin(); }
		static key_iter endKeys()   { return getSet_().end(); }
	};

protected:
	LLInstanceTracker()
	{
		// it's safe but unpredictable to create instances of this type while all instances are being iterated over.  I hate unpredictable.  This assert will probably be turned on early in the next development cycle.
		//llassert(sIterationNestDepth == 0);
		getSet_().insert(static_cast<T*>(this));
	}
	virtual ~LLInstanceTracker()
	{
		// it's unsafe to delete instances of this type while all instances are being iterated over.
		llassert(sIterationNestDepth == 0);
		getSet_().erase(static_cast<T*>(this));
	}

	LLInstanceTracker(const LLInstanceTracker& other)
	{
		//llassert(sIterationNestDepth == 0);
		getSet_().insert(static_cast<T*>(this));
	}

	static InstanceSet& getSet_()
	{
		void * & instances = getInstances(typeid(MyT));
		if (! instances)
		{
			instances = new InstanceSet;
		}
		return * static_cast<InstanceSet *>(instances);
	}

	static S32 sIterationNestDepth;
};

template <typename T> S32 LLInstanceTracker<T, T*>::sIterationNestDepth = 0;

#endif
