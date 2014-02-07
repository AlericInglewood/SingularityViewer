/**
 * @file aimultigridcalculatehash.cpp
 * @brief Multi grid support.
 *
 * Copyright (c) 2014, Aleric Inglewood.
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
 *   22/01/2014
 *   Initial version, written by Aleric Inglewood @ SL
 */

#include "llviewerprecompiledheaders.h"
#include "aimultigridcalculatehash.h"
#include "llassettype.h"
#include "llmd5.h"
#include "aibvhanimdelta.h"
#include "llvoavatar.h"
#include "llviewerobjectlist.h"
#include "llkeyframemotion.h"
#include "aialert.h"
#include "llimagej2c.h"
#include "aitexturedelta.h"

namespace AIMultiGrid {

LLPointer<BVHAnimDelta> calculateHashAnimation(unsigned char* buffer, size_t size, LLMD5& source_md5, LLMD5& asset_md5)
{
  LLPointer<BVHAnimDelta> delta;

  // Create a dummy avatar needed to calculate the hash of animations.
  LLPointer<LLVOAvatar> dummy_avatar = (LLVOAvatar*)gObjectList.createObjectViewer(LL_PCODE_LEGACY_AVATAR, NULL);
  dummy_avatar->mIsDummy = TRUE;

  // Calculate the source hash.
  LLDataPackerBinaryBuffer dp(buffer, size);
  LLKeyframeMotion keyframe(dummy_avatar);
  LLKeyframeMotion::ECode error = keyframe.deserialize_motionlist(dp, true, source_md5);
  if (!error)
  {
    // Extract the Delta from the anim file too, and pass a copy back.
    delta = new AIMultiGrid::BVHAnimDelta(keyframe.getDelta());

    // Calculate asset hash from source hash and the delta.
    unsigned char source_digest[16];
    source_md5.raw_digest(source_digest);
    asset_md5.update(source_digest, 16);			// Absorb the source.
    delta->update_hash(asset_md5);				    // Absorb the delta; this ignores round off errors in the floating point settings.
    asset_md5.finalize();
  }

  // Kill the dummy, we're done with it.
  dummy_avatar->markDead();

  if (error)
  {
	THROW_ALERT(LLKeyframeMotion::errorString(error));
  }

  return delta;
}

// Calculates the asset hash of the J2C passed in the buffer, and returns an allocated TextureDelta with the decoded delta.
LLPointer<TextureDelta> calculateHashTexture(unsigned char* buffer, size_t size, LLMD5& asset_md5, LLImageJ2C* j2c)
{
  // 'validate' decodes the image in order to get the meta data (width, height, number of components and discard level).
  // 'calculateHash' partially decodes the header and calculates the hash from 'buffer' and 'size' by reading those
  // values with getData() and getDataSize() (which were initialized by the call to 'validate').
  if (!j2c->validate(buffer, size))
  {
    // validate should throw proper exceptions for errors... but for now lets just pass on the English error text.
    THROW_ALERT("AIError", AIArgs("[ERROR]", LLImage::getLastError()));
  }
  LLPointer<TextureDelta> delta;
  if (!j2c->calculateHash(asset_md5, delta))
  {
    // This means that the image format is corrupt and therefore should never happen, since validate already passed.
    THROW_ALERT("AIFailedToDecodeJ2C");
  }
  return delta;
}

// Same as above but provides its own temporary LLImageJ2C object.
LLPointer<TextureDelta> calculateHashTexture(unsigned char* buffer, size_t size, LLMD5& asset_md5)
{
  LLPointer<LLImageJ2C> j2c = new LLImageJ2C;
  return calculateHashTexture(buffer, size, asset_md5, j2c);
}

} // namespace AIMultiGrid

