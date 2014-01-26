/**
 * @file aianimverifier.cpp
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

#include "linden_common.h"
#include "aianimverifier.h"
#include "aibvhanimdelta.h"
#include "aifile.h"
#include "aialert.h"
#include "llmd5.h"
#include "llerror.h"
#include "llkeyframemotion.h"
#include "lldatapacker.h"
#include "llcharacter.h"

namespace AIMultiGrid {

// Defined in newview.
LLPointer<BVHAnimDelta> calculateHashAnimation(unsigned char* buffer, size_t size, LLMD5& source_md5, LLMD5& asset_md5);

} // namespace AIMultiGrid

AIAnimVerifier::AIAnimVerifier(std::string const& filename) :
  mFilename(filename)
{
}

void AIAnimVerifier::calculateHash(LLMD5& source_md5, LLMD5& asset_md5, LLPointer<AIMultiGrid::BVHAnimDelta>& delta)
{
  // Open the file.
  AIFile fp(mFilename, "rb");

  // Default error thrown when seek or read fails. It doesn't really matter
  // because since we succeeded to open the file for reading, such error
  // just won't happen anyway.
  LLKeyframeMotion::ECode error = LLKeyframeMotion::invalid_anim_file;

  // Get the size of the file.
  long filesize;
  if (fseek(fp, 0, SEEK_END) || (filesize = ftell(fp)) == -1)
  {
	// It's impossible to get an error here, but whatever!
	THROW_ALERT(LLKeyframeMotion::errorString(error));
  }
  rewind(fp);

  // Read it into memory.
  std::vector<unsigned char> buffer((std::vector<unsigned char>::size_type)filesize);
  if (!fread(&buffer[0], 1, filesize, fp) == filesize)
  {
	THROW_ALERT(LLKeyframeMotion::errorString(error));
  }

  // If the read was successful, calculate asset_md5 and source_md5 hash values of it.
  delta = AIMultiGrid::calculateHashAnimation(&buffer[0], filesize, source_md5, asset_md5);
}

