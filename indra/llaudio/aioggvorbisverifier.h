/**
 * @file aioggvorbisverifier.h
 * @brief Verify an Ogg/Vorbis file for upload.
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
 *   15/09/2013
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AIOGGVORBISVERIFIER_H
#define AIOGGVORBISVERIFIER_H

#include <string>

class LLMD5;

class AIOggVorbisVerifier {
  public:
	AIOggVorbisVerifier(std::string const& filename) : mFilename(filename), mBuffer(NULL), mSize(0) { }
	AIOggVorbisVerifier(unsigned char* buffer, size_t size) : mBuffer(buffer), mSize(size) { }

	void calculateHash(LLMD5& md5);

  private:
	std::string mFilename;          // Set when constructed using the first constructor,

    unsigned char* mBuffer;         // or using mBuffer and mSize when constructed with
    unsigned char* mHead;
    size_t mSize;                   // the second constructor.
};

#endif // AIOGGVORBISVERIFIER_H

