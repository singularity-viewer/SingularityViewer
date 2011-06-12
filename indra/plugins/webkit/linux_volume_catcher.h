/** 
 * @file linux_volume_catcher.h
 * @brief A Linux-specific, PulseAudio-specific hack to detect and volume-adjust new audio sources
 *
 * @cond
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 * 
 * Copyright (c) 2010, Linden Research, Inc.
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
 * @endcond
 */

#ifndef LINUX_VOLUME_CATCHER_H
#define LINUX_VOLUME_CATCHER_H

#include "linden_common.h"

class LinuxVolumeCatcherImpl;

class LinuxVolumeCatcher
{
 public:
	LinuxVolumeCatcher();
	~LinuxVolumeCatcher();

	void setVolume(F32 volume); // 0.0 - 1.0
	void pump(); // call this at least a few times a second if you can - it affects how quickly we can 'catch' a new audio source and adjust its volume
	
 private:
	LinuxVolumeCatcherImpl *pimpl;
};

#endif // LINUX_VOLUME_CATCHER_H
