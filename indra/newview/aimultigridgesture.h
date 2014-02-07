/**
 * @file aimultigridgesture.h
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

#ifndef AIMULTIGRIDGESTURE_H
#define AIMULTIGRIDGESTURE_H

#include <iosfwd>
#include "llmultigesture.h"

class LLMD5;

namespace AIMultiGrid {

class LockedBackEnd;

class Gesture : public LLMultiGesture
{
  private:
    LockedBackEnd* mBackEnd;                    // Pointer to the underlaying database.

  public:
    Gesture(LockedBackEnd* back_end) : mBackEnd(back_end) { }

    void import(std::istream& stream);
    void import(unsigned char* buffer, size_t size);
    void calculateHash(LLMD5& asset_md5);
    void translate(void);
};

} // namespace AIMultiGrid

#endif // AIMULTIGRIDGESTURE_H

