/**
 * @file aioggvorbisverifier.cpp
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

#include "linden_common.h"
#include <cstdio>
#include "aioggvorbisverifier.h"
#include "aifile.h"
#include "aialert.h"
#include "llerror.h"
#include "llmd5.h"

struct AIOggPage
{
  unsigned char* mBuffer;
  unsigned int mDataLen;
  bool mSawFirstPage;
  bool mSawLastPage;
  U64 mGranulePosition;
  U32 mSerialNumber;
  U32 mSequenceNumber;

  AIOggPage(void);
  ~AIOggPage();

  void read(void);
  void updateHash(LLMD5& md5);

  template<typename T>
  static T read(unsigned char* buffer, int pos);

  virtual size_t read(unsigned char* buffer, size_t size) = 0;
};

struct AIOggPageFile : public AIOggPage
{
  AIFile mFile;

  AIOggPageFile(std::string const& filename) : mFile(filename, "rb") { }

  virtual size_t read(unsigned char* buffer, size_t size)
  {
    return fread(buffer, 1, size, mFile);
  }
};

struct AIOggPageBuffer : public AIOggPage
{
  unsigned char* mHead;
  unsigned char const* mBufferEnd;

  AIOggPageBuffer(unsigned char* buffer, size_t size) : mHead(buffer), mBufferEnd(buffer + size) { }

  virtual size_t read(unsigned char* buffer, size_t size)
  {
    size_t s = llmin(size, (size_t)(mBufferEnd - mHead));
    std::memcpy(buffer, mHead, s);
    mHead += s;
    return s;
  }
};

AIOggPage::AIOggPage(void) :
  mBuffer(new unsigned char[255 * 255]),	// The maximum Ogg packet data that fits in one page.
  mDataLen(0),
  mSawFirstPage(false),
  mSawLastPage(false),
  mGranulePosition(0),
  mSerialNumber(0),
  mSequenceNumber(0)
{
}

AIOggPage::~AIOggPage()
{
  delete [] mBuffer;
}

//static
template<typename T>
T AIOggPage::read(unsigned char* buffer, int pos)
{
  T value = 0;
  for (int i = pos + sizeof(T) - 1; i >= pos; --i)
  {
	value <<= 8;
	value |= buffer[i];
  }
  return value;
}

// Read and decode an Ogg page.
//
// NOTE: Comments that start with a '*' are quotes from http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-130000A.2
//       The layout and meaning of the Ogg header is specified on http://www.xiph.org/ogg/doc/framing.html
//
void AIOggPage::read(void)
{
  // Read a part of the Ogg header.
  unsigned char header[27];
  size_t len = read(header, sizeof(header));

  //-----------------------------------------------------------------------------------------------
  // capture_pattern
  if (len < 4 || header[0] != 0x4f || header[1] != 0x67 || header[2] != 0x67 || header[3] != 0x53)
  {
	THROW_ALERT("AIOggPage_read_Wrong_magic");
  }
  if (len != sizeof(header))
  {
	// Truncated file.
	THROW_ALERT("AIOggPage_read_Truncated");
  }

  // stream_structure_version
  if (header[4] != 0)
  {
	// Wrong version.
	THROW_ALERT("AIOggPage_read_Wrong_version");
  }

  //-----------------------------------------------------------------------------------------------
  // header_type_flag
  unsigned int flags = header[5];
  if ((flags & 0x2))
  {
	// First page of logical bitstream (bos).
	if (mSawFirstPage)
	{
	  // Stream may not contain more than one logical bitstreams.
	  THROW_ALERT("AIOggPage_read_Format_error");
	}
	mSawFirstPage = true;
	mSequenceNumber = 0;
  }
  else
  {
	// This should be our sequence number.
	++mSequenceNumber;
  }
  if ((flags & 0x4))
  {
	mSawLastPage = true;
  }

  // Check flags.
  if (!mSawFirstPage)
  {
	// File must start with the first page of the logical bitstream.
	// * This first page is marked ’beginning of stream’ in the page flags.
	THROW_ALERT("AIOggPage_read_Corrupted");
  }

  //-----------------------------------------------------------------------------------------------
  // absolute granule position
  mGranulePosition = read<U64>(header, 6);

  // Check the duration.
  // * The granule position of pages containing Vorbis audio is in units of PCM audio samples (per channel;
  //   a stereo stream’s granule position does not increment at twice the speed of a mono stream).
  // * The granule position of a page represents the end PCM sample position of the last packet completed on that page.
  //   The ’last PCM sample’ is the last complete sample returned by decode, not an internal sample awaiting lapping
  //   with a subsequent block. A page that is entirely spanned by a single packet (that completes on a subsequent page)
  //   has no granule position, and the granule position is set to ’-1’.
  if ((S64)mGranulePosition != -1 && mGranulePosition > 441000)
  {
	// The granule position is the bitclock in our case, so at a bit rate of 44100 Hz, this is more than 10 seconds.
	THROW_ALERT("AIOggPage_read_Too_long");
  }

  //-----------------------------------------------------------------------------------------------
  // stream serial number
  U32 serial = read<U32>(header, 14);

  // Check serial number.
  if (!mSerialNumber)
  {
	mSerialNumber = serial;
  }
  else if (serial != mSerialNumber)
  {
	// Since we have only one logical bitstream, every page must have the same serial number.
	THROW_ALERT("AIOggPage_read_Corrupted");
  }

  //-----------------------------------------------------------------------------------------------
  // page sequence no
  U32 sequence_number = read<U32>(header, 18);

  // Check sequence number.
  if (sequence_number != mSequenceNumber)
  {
	// Page sequence numbers don't match.
	THROW_ALERT("AIOggPage_read_Corrupted");
  }

  //-----------------------------------------------------------------------------------------------
  // page_segments
  int segments = header[26];

  // Check size of first page.
  if (mSequenceNumber == 0 && segments != 1)
  {
	// * The first Vorbis packet (the identification header), which uniquely identifies a stream as Vorbis audio,
	//   is placed alone in the first page of the logical Ogg stream. This results in a first Ogg page of exactly
	//   58 bytes at the very beginning of the logical stream.
	// 28 bytes of Ogg header and 30 bytes of Vorbis identification header.
	// Since 30 < 255, we only have a single segment (bringing the Ogg header to a size of 27 + 1 = 28).
	THROW_ALERT("AIOggPage_read_Not_Ogg_Vorbis");
  }

  //-----------------------------------------------------------------------------------------------
  // segment_table (containing packet lacing values)
  unsigned char segment_table[255];
  len = read(segment_table, segments);
  if (len != segments)
  {
	// File truncated?
	THROW_ALERT("AIOggPage_read_Corrupted");
  }

  // Calculate the length of the Ogg data by adding up the size of all packet lacing values.
  mDataLen = 0;
  for (int i = 0; i < segments; ++i)
  {
	mDataLen += segment_table[i];
  }

  // Check size of first page.
  if (mSequenceNumber == 0 && mDataLen != 30)
  {
	THROW_ALERT("AIOggPage_read_Not_Ogg_Vorbis");
  }

  //-----------------------------------------------------------------------------------------------
  // Read the Ogg data into memory so that the file pointer points to the next page,
  // and the we can optionally calculate the md5 hash of this data.
  llassert_always(mDataLen <= 255 * 255);
  len = read(mBuffer, mDataLen);
  if (len != mDataLen)
  {
	// File truncated?
	THROW_ALERT("AIOggPage_read_Corrupted");
  }
  if (mSequenceNumber == 0)
  {
	// We just read the Vorbis identification header.
	// The layout and format of this header is specified on http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-620004.2.2

	//--------------------------
	// packet_type
	unsigned char packet_type = mBuffer[0];
	if (packet_type != 1)
	{
	  // - The identification header is type 1.
	  THROW_ALERT("AIOggPage_read_Not_Ogg_Vorbis");
	}

	//--------------------------
	// magic - 0x76, 0x6f, 0x72, 0x62, 0x69, 0x73: the characters ’v’,’o’,’r’,’b’,’i’,’s’ as six octets
	if (mBuffer[1] != 0x76 || mBuffer[2] != 0x6f || mBuffer[3] != 0x72 || mBuffer[4] != 0x62 || mBuffer[5] != 0x69 || mBuffer[6] != 0x73)
	{
	  THROW_ALERT("AIOggPage_read_Not_Ogg_Vorbis");
	}

	U32 vorbis_version = read<U32>(mBuffer, 7);
	if (vorbis_version != 0)
	{
	  THROW_ALERT("AIOggPage_read_Not_Ogg_Vorbis");
	}
	int audio_channels = mBuffer[11];
	if (audio_channels != 1)
	{
	  // SecondLife only accepts a single channel.
	  THROW_ALERT("AIOggPage_read_Format_error");
	}
	U32 audio_sample_rate = read<U32>(mBuffer, 12);
	if (audio_sample_rate != 44100)
	{
	  // SecondLife only accepts a 44100 Hz sample rate.
	  THROW_ALERT("AIOggPage_read_Format_error");
	}
	unsigned char block_size = mBuffer[28];
	unsigned char block_size_0 = block_size & 0xf;
	unsigned char block_size_1 = block_size >> 4;
	if (block_size_0 > block_size_1 || block_size_1 > 13 || block_size_0 < 6)
	{
	  // - Allowed final blocksize values are 64, 128, 256, 512, 1024, 2048, 4096 and 8192 in Vorbis I.
	  // [blocksize_0] must be less than or equal to [blocksize_1].
	  THROW_ALERT("AIOggPage_read_Corrupted");
	}
	unsigned char framing_flag = mBuffer[29];
	if (!(framing_flag & 1))
	{
	  // The framing bit must be nonzero.
	  THROW_ALERT("AIOggPage_read_Corrupted");
	}
  }
}

void AIOggPage::updateHash(LLMD5& md5)
{
  md5.update(mBuffer, mDataLen);
}

void AIOggVorbisVerifier::calculateHash(LLMD5& md5)
{
  // Initialize the verifier.
  AIOggPage* page = mBuffer ? (AIOggPage*) new AIOggPageBuffer(mBuffer, mSize) : (AIOggPage*) new AIOggPageFile(mFilename);

  // Read Ogg pages until the data stream starts.
  // From http://www.xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-130000A.2 :
  // * The first Vorbis packet (the identification header), which uniquely identifies a
  //   stream as Vorbis audio, is placed alone in the first page of the logical Ogg stream.
  // * The second and third vorbis packets (comment and setup headers) may span one or more
  //   pages beginning on the second page of the logical stream. However many pages they span,
  //   the third header packet finishes the page on which it ends. The next (first audio)
  //   packet must begin on a fresh page.
  // * The granule position of these first pages containing only headers is zero.
  do
  {
	page->read();
  }
  while (page->mGranulePosition == 0);

  // Calculate the hash of the data portion (skipping the ogg headers).
  // We can start with the last page read because that has to be
  // entirely audio data: audio data must start on a fresh page.
  while(1)
  {
	page->updateHash(md5);
	if (page->mSawLastPage)
	{
	  break;
	}
	page->read();
  }

  md5.finalize();
}

