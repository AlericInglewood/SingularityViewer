/**
 * @file aimultigridwearable.h
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
 *   02/02/2014
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AIMULTIGRIDWEARABLE_H
#define AIMULTIGRIDWEARABLE_H

#include <iosfwd>
#include "llwearable.h"

class LLMD5;

namespace AIMultiGrid {

class LockedBackEnd;

// Body part of clothing asset that being manipulated for import/export.
class Wearable : public LLWearable
{
  private:
    LockedBackEnd* mBackEnd;                    // Pointer to the underlaying database.

  public:
    // Create a Wearable using a specific (already locked) BackEndAccess object.
    Wearable(LockedBackEnd* back_end) : mBackEnd(back_end) { }

    // Initialize the object by reading an asset file.
    void import(std::istream& stream);
    void import(unsigned char* buffer, size_t size);

    void calculateHash(LLMD5& asset_md5);      // Calculate asset hash.
    void translate(void);                       // Translate the embedded UUIDs to UUIDs of the current grid.

  protected:
    // Keep the base class happy.
    /*virtual*/ LLUUID const getDefaultTextureImageID(LLAvatarAppearanceDefines::ETextureIndex index) const;
    /*virtual*/ void setUpdated() const { }
    /*virtual*/ void addToBakedTextureHash(LLMD5&) const { }
};

} // namespace AIMultiGrid

#endif // AIMULTIGRIDWEARABLE_H

