/** 
 * @file llstrider.h
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

#ifndef LL_LLSTRIDER_H
#define LL_LLSTRIDER_H

#include "stdtypes.h"

template <class Object> class LLStrider
{
	union
	{
		Object* mObjectp;
		U8*		mBytep;
	};
	U32     mSkip;
	U32		mTypeSize;
public:

	LLStrider()  { mObjectp = NULL; mTypeSize = mSkip = sizeof(Object); } 
	~LLStrider() { } 

	const LLStrider<Object>& operator =  (Object *first)    { mObjectp = first; return *this;}
	void setStride (S32 skipBytes)	{ mSkip = (skipBytes ? skipBytes : sizeof(Object));}
	void setTypeSize (S32 typeBytes){ mTypeSize = (typeBytes ? typeBytes : sizeof(Object)); }

	bool isStrided() const		   { return mTypeSize != mSkip; } 
	void skip(const U32 index)     { mBytep += mSkip*index;}
	U32 getSkip() const			   { return mSkip; }
	Object* get()                  { return mObjectp; }
	Object* operator->()           { return mObjectp; }
	Object& operator *()           { return *mObjectp; }
	Object* operator ++(int)       { Object* old = mObjectp; mBytep += mSkip; return old; }
	Object* operator +=(int i)     { mBytep += mSkip*i; return mObjectp; }
	Object& operator[](U32 index)  { return *(Object*)(mBytep + (mSkip * index)); }
	void assignArray(U8* __restrict source, const size_t elem_size, const size_t elem_count)
	{
		llassert_always(sizeof(Object) <= elem_size);

		U8* __restrict dest = mBytep;				//refer to dest instead of mBytep to benefit from __restrict hint
		const U32 bytes = elem_size * elem_count;	//total bytes to copy from source to dest

		//stride == sizeof(element) implies entire buffer is unstrided and thus memcpy-able, provided source buffer elements match in size.
		//Because LLStrider is often passed an LLVector3 even if the reprensentation is LLVector4 in the vertex buffer, mTypeSize is set to 
		//the TRUE vbo datatype size via VertexBufferStrider::get
		if(!isStrided() && mTypeSize == elem_size)	
		{
			if(bytes >= sizeof(LLVector4) * 4)	//Should be able to pull at least 3 16byte blocks from this. Smaller isn't really beneficial.
			{
				U8* __restrict aligned_source = LL_NEXT_ALIGNED_ADDRESS(source);
				U8* __restrict aligned_dest = LL_NEXT_ALIGNED_ADDRESS(dest);
				const U32 source_offset = aligned_source - source;	//Offset to first aligned location in source buffer.
				const U32 dest_offset = aligned_dest - dest;		//Offset to first aligned location in dest buffer.
				llassert_always(source_offset < 16);
				llassert_always(dest_offset < 16);
				if(source_offset == dest_offset)	//delta to aligned location matches between source and destination! _mm_*_ps should be viable.
				{
					const U32 end_offset = (bytes - source_offset) % sizeof(LLVector4);		//buffers may not neatly end on a 16byte alignment boundary.
					const U32 aligned_bytes = bytes - source_offset - end_offset;	//how many bytes to copy from aligned start to aligned end.

					llassert_always(aligned_bytes > 0);

					if(source_offset)	//memcpy up to the aligned location if needed
						memcpy(dest,source,source_offset);
					LLVector4a::memcpyNonAliased16((F32*) aligned_dest, (F32*) aligned_source, aligned_bytes);
					if(end_offset)		//memcpy to the very end if needed.
						memcpy(aligned_dest+aligned_bytes,aligned_source+aligned_bytes,end_offset);
				}
				else	//buffers non-uniformly offset from aligned location. Using _mm_*u_ps.
				{
					U32 end = bytes/sizeof(LLVector4);	//sizeof(LLVector4) = 16 bytes = 128 bits

					llassert_always(end > 0);

					__m128* dst = (__m128*) dest;
					__m128* src = (__m128*) source;			

					for (U32 i = 0; i < end; i++)	//copy 128bit chunks
					{
						__m128 res = _mm_loadu_ps((F32*)&src[i]);
						_mm_storeu_ps((F32*)&dst[i], res);
					}
					end*=16;//Convert to real byte offset
					if(end < bytes)	//just memcopy the rest
						memcpy(dest+end,source+end,bytes-end);
				}
			}
			else	//Too small. just do a simple memcpy.
				memcpy(dest,source,bytes);
		}
		else
		{
			for(U32 i=0;i<elem_count;i++)
			{
				memcpy(dest,source,sizeof(Object));
				dest+=mSkip;
				source+=elem_size;
			}
		}
	}
};

#endif // LL_LLSTRIDER_H
