/**
 * @file aianimverifier.h
 * @brief AIMultiGrid support: calculate hash values for an animation asset.
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
 *   20/09/2013
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AIANIMVERIFIER_H
#define AIANIMVERIFIER_H

#include <string>
#include "llpointer.h"

class LLMD5;

namespace AIMultiGrid {
  class BVHAnimDelta;
}

class AIAnimVerifier {
  public:
	AIAnimVerifier(std::string const& filename);

	void calculateHash(LLMD5& source_md5, LLMD5& asset_md5, LLPointer<AIMultiGrid::BVHAnimDelta>& delta);

  private:
	std::string mFilename;
};

#endif // AIANIMVERIFIER_H

