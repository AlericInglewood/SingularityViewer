/**
 * @file aimultigridgesture.cpp
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
 *   02/02/2014
 *   Initial version, written by Aleric Inglewood @ SL
 */

#include "llviewerprecompiledheaders.h"
#include "lltrans.h"
#include "llmd5.h"
#include "aimultigridgesture.h"
#include "aimultigridbackend.h"
#include "aialert.h"

namespace AIMultiGrid {

void Gesture::import(unsigned char* buffer, size_t size)
{
  LLDataPackerAsciiBuffer packer((char*)buffer, size);
  if (!deserialize(packer))
  {
    THROW_ALERT("AIMultiGrid_Error_decoding_NAME_asset", AIArgs("[NAME]", LLTrans::getString("gesture")));
  }
}

void Gesture::calculateHash(LLMD5& asset_md5)
{
  S32 const buffer_size = sizeof(U8) + sizeof(U32);
  U8 v[buffer_size];
  LLDataPackerBinaryBuffer buffer(v, buffer_size);
  buffer.packU8(mKey, "Gesture::mKey");
  buffer.packU32(mMask, "Gesture::mMask");
  asset_md5.update(v, buffer_size);
  asset_md5.update(mTrigger);
  asset_md5.update(mReplaceText);
  for (std::vector<LLGestureStep*>::iterator step = mSteps.begin(); step != mSteps.end(); ++step)
  {
    (*step)->update_hash(asset_md5, mBackEnd);
  }
  asset_md5.finalize();
}

void Gesture::translate(void)
{
  for (std::vector<LLGestureStep*>::iterator step = mSteps.begin(); step != mSteps.end(); ++step)
  {
    switch((*step)->getType())
    {
      case STEP_ANIMATION:
      {
        LLGestureStepAnimation* animation_step = static_cast<LLGestureStepAnimation*>(*step);
        mBackEnd->translate(animation_step->mAnimAssetID);
        break;
      }
      case STEP_SOUND:
      {
        LLGestureStepSound* sound_step = static_cast<LLGestureStepSound*>(*step);
        mBackEnd->translate(sound_step->mSoundAssetID);
        break;
      }
      default:
        break;
    }
  }
}

} // namespace AIMultiGrid

