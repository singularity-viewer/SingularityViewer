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
#include <typeinfo>

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

	/// Find or create a STATICDATA instance for the specified TRACKED class.
	/// STATICDATA must be default-constructible.
	template<typename STATICDATA, class TRACKED>
	static STATICDATA& getStatic()
	{
		void *& instances = getInstances(typeid(TRACKED));
		if (! instances)
		{
			instances = new STATICDATA;
		}
		return *static_cast<STATICDATA*>(instances);
	}

    /// It's not essential to derive your STATICDATA (for use with
    /// getStatic()) from StaticBase; it's just that both known
    /// implementations do.
    struct StaticBase
    {
        StaticBase():
            sIterationNestDepth(0)
        {}
        S32 sIterationNestDepth;
    };
};

/// This mix-in class adds support for tracking all instances of the specified class parameter T
/// The (optional) key associates a value of type KEY with a given instance of T, for quick lookup
/// If KEY is not provided, then instances are stored in a simple set
/// @NOTE: see explicit specialization below for default KEY==T* case
template<typename T, typename KEY = T*>
class LLInstanceTracker : public LLInstanceTrackerBase
{
	typedef LLInstanceTracker<T, KEY> MyT;
	typedef typename std::map<KEY, T*> InstanceMap;
	struct StaticData: public StaticBase
	{
		InstanceMap sMap;
	};
	static StaticData& getStatic() { return LLInstanceTrackerBase::getStatic<StaticData, MyT>(); }
	static InstanceMap& getMap_() { return getStatic().sMap; }

public:
	class instance_iter : public boost::iterator_facade<instance_iter, T, boost::forward_traversal_tag>
	{
	public:
		typedef boost::iterator_facade<instance_iter, T, boost::forward_traversal_tag> super_t;
		
		instance_iter(const typename InstanceMap::iterator& it)
		:	mIterator(it)
		{
			++getStatic().sIterationNestDepth;
		}

		~instance_iter()
		{
			--getStatic().sIterationNestDepth;
		}


	private:
		friend class boost::iterator_core_access;

		void increment() { mIterator++; }
		bool equal(instance_iter const& other) const
		{
			return mIterator == other.mIterator;
		}

		T& dereference() const
		{
			return *(mIterator->second);
		}

		typename InstanceMap::iterator mIterator;
	};

	class key_iter : public boost::iterator_facade<key_iter, KEY, boost::forward_traversal_tag>
	{
	public:
		typedef boost::iterator_facade<key_iter, KEY, boost::forward_traversal_tag> super_t;

		key_iter(typename InstanceMap::iterator it)
			:	mIterator(it)
		{
			++getStatic().sIterationNestDepth;
		}

		key_iter(const key_iter& other)
			:	mIterator(other.mIterator)
		{
			++getStatic().sIterationNestDepth;
		}

		~key_iter()
		{
			--getStatic().sIterationNestDepth;
		}


	private:
		friend class boost::iterator_core_access;

		void increment() { mIterator++; }
		bool equal(key_iter const& other) const
		{
			return mIterator == other.mIterator;
		}

		KEY& dereference() const
		{
			return const_cast<KEY&>(mIterator->first);
		}

		typename InstanceMap::iterator mIterator;
	};

	static T* getInstance(const KEY& k)
	{
		typename InstanceMap::const_iterator found = getMap_().find(k);
		return (found == getMap_().end()) ? NULL : found->second;
	}

	static instance_iter beginInstances() 
	{	
		return instance_iter(getMap_().begin()); 
	}

	static instance_iter endInstances() 
	{
		return instance_iter(getMap_().end());
	}

	static S32 instanceCount() { return getMap_().size(); }

	static key_iter beginKeys()
	{
		return key_iter(getMap_().begin());
	}
	static key_iter endKeys()
	{
		return key_iter(getMap_().end());
	}

protected:
	LLInstanceTracker(KEY key) { add_(key); }
	virtual ~LLInstanceTracker() 
	{ 
		// it's unsafe to delete instances of this type while all instances are being iterated over.
		llassert_always(getStatic().sIterationNestDepth == 0);
		remove_();		
	}
	virtual void setKey(KEY key) { remove_(); add_(key); }
	virtual const KEY& getKey() const { return mInstanceKey; }

private:
	void add_(KEY key) 
	{ 
		mInstanceKey = key; 
		getMap_()[key] = static_cast<T*>(this); 
	}
	void remove_()
	{
		getMap_().erase(mInstanceKey);
	}

private:
	KEY mInstanceKey;
};

/// explicit specialization for default case where KEY is T*
/// use a simple std::set<T*>
template<typename T>
class LLInstanceTracker<T, T*> : public LLInstanceTrackerBase
{
	typedef LLInstanceTracker<T, T*> MyT;
	typedef typename std::set<T*> InstanceSet;
	struct StaticData: public StaticBase
	{
		InstanceSet sSet;
	};
	static StaticData& getStatic() { return LLInstanceTrackerBase::getStatic<StaticData, MyT>(); }
	static InstanceSet& getSet_() { return getStatic().sSet; }

public:

	/// for completeness of analogy with the generic implementation
	static T* getInstance(T* k) { return k; }
	static S32 instanceCount() { return getSet_().size(); }

	class instance_iter : public boost::iterator_facade<instance_iter, T, boost::forward_traversal_tag>
	{
	public:
		instance_iter(const typename InstanceSet::iterator& it)
		:	mIterator(it)
		{
			++getStatic().sIterationNestDepth;
		}

		instance_iter(const instance_iter& other)
		:	mIterator(other.mIterator)
		{
			++getStatic().sIterationNestDepth;
		}

		~instance_iter()
		{
			--getStatic().sIterationNestDepth;
		}

	private:
		friend class boost::iterator_core_access;

		void increment() { mIterator++; }
		bool equal(instance_iter const& other) const
		{
			return mIterator == other.mIterator;
		}

		T& dereference() const
		{
			return **mIterator;
		}

		typename InstanceSet::iterator mIterator;
	};

	static instance_iter beginInstances() {	return instance_iter(getSet_().begin()); }
	static instance_iter endInstances() { return instance_iter(getSet_().end()); }

protected:
	LLInstanceTracker()
	{
		// it's safe but unpredictable to create instances of this type while all instances are being iterated over.  I hate unpredictable.  This assert will probably be turned on early in the next development cycle.
		getSet_().insert(static_cast<T*>(this));
	}
	virtual ~LLInstanceTracker()
	{
		// it's unsafe to delete instances of this type while all instances are being iterated over.
		llassert_always(getStatic().sIterationNestDepth == 0);
		getSet_().erase(static_cast<T*>(this));
	}

	LLInstanceTracker(const LLInstanceTracker& other)
	{
		getSet_().insert(static_cast<T*>(this));
	}
};

#endif
