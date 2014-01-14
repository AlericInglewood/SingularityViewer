/**
 * @file aibvhanimdelta.h
 * @brief Declaration of class BVHAnimDelta
 *
 * Copyright (c) 2013, Aleric Inglewood.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution.
 *
 * CHANGELOG
 *   and additional copyright holders.
 *
 *   30/09/2013
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AIBVHANIMDELTA_H
#define AIBVHANIMDELTA_H

#include <string>
#include <iosfwd>
#include "stdtypes.h"
#include "aimultigriddelta.h"

namespace AIMultiGrid {

//-----------------------------------------------------------------------------
// class BVHAnimDelta

// The extra data in an ANIM asset file that is not in the corresponding BVH source file.

class BVHAnimDelta : public Delta
{
  protected:
	S32			mBasePriority;			// LLJoint::JointPriority
	BOOL		mLoop;
	F32			mLoopInPoint;
	F32			mLoopOutPoint;
	F32			mEaseInDuration;
	F32			mEaseOutDuration;
	S32			mHandPose;				// LLHandMotion::eHandPose
	std::string	mEmoteName;

  public:
	BVHAnimDelta(void);
	BVHAnimDelta(S32 base_priority, bool loop, F32 loop_in_point, F32 loop_out_point, F32 ease_in_duration, F32 ease_out_duration, S32 hand_pose, std::string const& emote_name);
	BVHAnimDelta(AIXMLElementParser const& parser);

	// Compare.
	bool equals(Delta const* delta) const;

	// Accessors.
	S32 getBasePriority(void) const { return mBasePriority; }
	bool getLoop(void) const { return mLoop; }
	F32 getLoopInPoint(void) const { return mLoopInPoint; }
	F32 getLoopOutPoint(void) const { return mLoopOutPoint; }
	F32 getEaseInDuration(void) const { return mEaseInDuration; }
	F32 getEaseOutDuration(void) const { return mEaseOutDuration; }
	S32 getHandPose(void) const { return mHandPose; }
	std::string const& getEmoteName(void) const { return mEmoteName; }

	// Manipulators.
	void setBasePriority(S32 base_priority) { mBasePriority = base_priority; }
	void setLoop(bool loop) { mLoop = loop ? TRUE : FALSE; }
	void setLoopInPoint(F32 loop_in_point) { mLoopInPoint = loop_in_point; }
	void setLoopOutPoint(F32 loop_out_point) { mLoopOutPoint = loop_out_point; }
	void setEaseInDuration(F32 ease_in_duration) { mEaseInDuration = ease_in_duration; }
	void setEaseOutDuration(F32 ease_out_duration) { mEaseOutDuration = ease_out_duration; }
	void setHandPose(S32 hand_pose) { mHandPose = hand_pose; }
	void setEmoteName(std::string const& emote_name) { mEmoteName = emote_name; }

	/*virtual*/ void toXML(std::ostream& os, int indentation) const;
	/*virtual*/ void update_hash(LLMD5& hash_out);
};

} // namespace AIMultiGrid

#endif // AIBVHANIMDELTA_H

