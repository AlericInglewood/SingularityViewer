/**
 * @file aibvhanimdelta.cpp
 * @brief Implementation of class BVHAnimDelta
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
 *   90/09/2013
 *   Initial version, written by Aleric Inglewood @ SL
 */

#include "linden_common.h"
#include "aibvhanimdelta.h"
#include "aixml.h"
#include <iostream>
#include "llmd5.h"
#include "llmath.h"
#include "lldatapacker.h"

namespace AIMultiGrid {

BVHAnimDelta::BVHAnimDelta(void) :
  mBasePriority(0),
  mLoop(false),
  mLoopInPoint(0.f),
  mLoopOutPoint(0.f),
  mEaseInDuration(0.f),
  mEaseOutDuration(0.f),
  mHandPose(0)
{
}

BVHAnimDelta::BVHAnimDelta(S32 base_priority, bool loop, F32 loop_in_point, F32 loop_out_point,
	F32 ease_in_duration, F32 ease_out_duration, S32 hand_pose, std::string const& emote_name) :
  mBasePriority(base_priority),
  mLoop(loop),
  mLoopInPoint(loop_in_point),
  mLoopOutPoint(loop_out_point),
  mEaseInDuration(ease_in_duration),
  mEaseOutDuration(ease_out_duration),
  mHandPose(hand_pose),
  mEmoteName(emote_name)
{
}

bool BVHAnimDelta::equals(BVHAnimDelta const& ad) const
{
  return
	  mBasePriority == ad.mBasePriority &&
	  mLoop == ad.mLoop &&
	  std::fabs(mLoopInPoint - ad.mLoopInPoint) < 0.00001 &&
	  std::fabs(mLoopOutPoint - ad.mLoopOutPoint) < 0.00001 &&
	  std::fabs(mEaseInDuration - ad.mEaseInDuration) < 0.00001 &&
	  std::fabs(mEaseOutDuration - ad.mEaseOutDuration) < 0.00001 &&
	  mHandPose == ad.mHandPose &&
	  mEmoteName == ad.mEmoteName;
}

// This produces the same hash value for two almost equal BVHAnimDelta objects
// as long as BVHAnimDelta::equals returns zero (edge cases are not important).
void BVHAnimDelta::update_hash(LLMD5& hash_out)
{
  U32 v;
  U8 loop = mLoop ? 1 : 0;
  S32 size = 4 + 1 + 4 + 4 + 4 + 4 + 4 + mEmoteName.length();
  U8* buffer = new U8 [size];
  LLDataPackerBinaryBuffer buf(buffer, sizeof(buffer));
  buf.packS32(mBasePriority, "mBasePriority");
  buf.packU8(loop, "mLoop");
  v = llfloor(mLoopInPoint * 100000.f);
  buf.packU32(v, "mLoopInPoint");
  v = llfloor(mLoopOutPoint * 100000.f);
  buf.packU32(v, "mLoopOutPoint");
  v = llfloor(mEaseInDuration * 100000.f);
  buf.packU32(v, "mEaseInDuration");
  v = llfloor(mEaseOutDuration * 100000.f);
  buf.packU32(v, "mEaseOutDuration");
  buf.packS32(mHandPose, "mHandPose");
  buf.packString(mEmoteName, "mEmoteName");
  hash_out.update(buffer, sizeof(buffer));
  delete [] buffer;
}

void BVHAnimDelta::toXML(std::ostream& os, int indentation) const
{
  AIXMLElement tag(os, "bvhanimdelta", indentation);
  tag.child("base_priority", mBasePriority);
  tag.child("loop", mLoop);
  tag.child("loop_in_point", mLoopInPoint);
  tag.child("loop_out_point", mLoopOutPoint);
  tag.child("ease_in_duration", mEaseInDuration);
  tag.child("ease_out_duration", mEaseOutDuration);
  tag.child("hand_pose", mHandPose);
  tag.child("emote_name", mEmoteName);
}

BVHAnimDelta::BVHAnimDelta(AIXMLElementParser const& parser)
{
  parser.child("base_priority", mBasePriority);
  parser.child("loop", mLoop);
  parser.child("loop_in_point", mLoopInPoint);
  parser.child("loop_out_point", mLoopOutPoint);
  parser.child("ease_in_duration", mEaseInDuration);
  parser.child("ease_out_duration", mEaseOutDuration);
  parser.child("hand_pose", mHandPose);
  parser.child("emote_name", mEmoteName);
}

} // namespace AIMultiGrid
