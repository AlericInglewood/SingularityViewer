/**
 * @file aimultigridcalculatehash.h
 * @brief Multi grid support API.
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

#ifndef AIMULTIGRIDCALCULATEHASH_H
#define AIMULTIGRIDCALCULATEHASH_H

#include "llassettype.h"

class LLMD5;
class LLJ2CImage;

namespace AIMultiGrid {

class BVHAnimDelta;
class TextureDelta;

// Calculates the source and asset hash of the ANIM passed in the buffer, and returns an allocated BVHAnimDelta with the decoded delta.
LLPointer<BVHAnimDelta> calculateHashAnimation(unsigned char* buffer, size_t size, LLMD5& source_md5, LLMD5& asset_md5);

// Calculates the asset hash of the J2C passed in the buffer, and returns an allocated TextureDelta with the decoded delta.
LLPointer<TextureDelta> calculateHashTexture(unsigned char* buffer, size_t size, LLMD5& asset_md5, LLJ2CImage* j2c);
LLPointer<TextureDelta> calculateHashTexture(unsigned char* buffer, size_t size, LLMD5& asset_md5);

// Calculates the asset hash of other asset types.
void calculateHash(LLAssetType::EType type, unsigned char* buffer, size_t size, LLMD5& asset_md5);

} // namespace AIMultiGrid

#endif // AIMULTIGRIDCALCULATEHASH_H

