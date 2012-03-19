/**
 * @file aicurl.cpp
 * @brief Implementation of AICurl.
 *
 * Copyright (c) 2012, Aleric Inglewood.
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
 *   17/03/2012
 *   Initial version, written by Aleric Inglewood @ SL
 */

#include "linden_common.h"
#include "aicurl.h"
#include "llbufferstream.h"
#include "llsdserialize.h"

//==================================================================================
// External API
//

namespace AICurlInterface
{

// This function is used in indra/llmessage/llproxy.cpp.
void check_easy_code(CURLcode code)
{
  if (code != CURLE_OK)
  {
	llinfos << "curl easy error detected: " << curl_easy_strerror(code) << llendl;
  }
}

void initClass(bool multi_threaded, F32 curl_request_timeout, S32 max_number_handles)
{
}

void cleanupClass(void)
{
}

void setCAFile(std::string const& file)
{
}

std::string strerror(CURLcode errorcode)
{
  return "";
}

std::string getVersionString(void)
{
  return "";
}

CURLM* newMultiHandle(void)
{
  return NULL;
}

CURLMcode deleteMultiHandle(CURLM* handle)
{
  return (CURLMcode)0;
}

CURL* newEasyHandle(void)
{
  return NULL;
}

void deleteEasyHandle(CURL* handle)
{
}

//-----------------------------------------------------------------------------
// class Responder
//

Responder::Responder(void) : mReferenceCount(0)
{
  DoutEntering(dc::curl, "AICurlInterface::Responder() with this = " << (void*)this);
  BACKTRACE;
}

Responder::~Responder()
{
  DoutEntering(dc::curl, "AICurlInterface::Responder::~Responder() with this = " << (void*)this << "; mReferenceCount = " << mReferenceCount);
  BACKTRACE;
}

void Responder::setURL(std::string const& url)
{
  // setURL is called from llhttpclient.cpp (request()), before calling any of the below (of course).
  // We don't need locking here therefore; it's a case of initializing before use.
  mURL = url;
}

// Called with HTML header.
// virtual
void Responder::completedHeader(U32, std::string const&, LLSD const&)
{
  // Nothing.
}

// Called with HTML body.
// virtual
void Responder::completedRaw(U32 status, std::string const& reason, LLChannelDescriptors const& channels, LLIOPipe::buffer_ptr_t const& buffer)
{
  LLSD content;
  LLBufferStream istr(channels, buffer.get());
  if (!LLSDSerialize::fromXML(content, istr))
  {
	llinfos << "Failed to deserialize LLSD. " << mURL << " [" << status << "]: " << reason << llendl;
  }

  // Allow derived class to override at this point.
  completed(status, reason, content);
}

void Responder::fatalError(std::string const& reason)
{
  llwarns << "Responder::fatalError(\"" << reason << "\") is called (" << mURL << "). Passing it to Responder::completed with fake HTML error status and empty HTML body!" << llendl;
  completed(U32_MAX, reason, LLSD());
}

// virtual
void Responder::completed(U32 status, std::string const& reason, LLSD const& content)
{
  // HTML status good?
  if (200 <= status && status < 300)
  {
	// Allow derived class to override at this point.
	result(content);
  }
  else
  {
	// Allow derived class to override at this point.
	errorWithContent(status, reason, content);
  }
}

// virtual
void Responder::errorWithContent(U32 status, std::string const& reason, LLSD const&)
{
  // Allow derived class to override at this point.
  error(status, reason);
}

// virtual
void Responder::error(U32 status, std::string const& reason)
{
  llinfos << mURL << " [" << status << "]: " << reason << llendl;
}

// virtual
void Responder::result(LLSD const&)
{
  // Nothing.
}

// Friend functions.

void intrusive_ptr_add_ref(Responder* responder)
{
  responder->mReferenceCount++;
}

void intrusive_ptr_release(Responder* responder)
{
  if (--responder->mReferenceCount == 0)
  {
	delete responder;
  }
}

//-----------------------------------------------------------------------------
// class Easy
//

CURL* Easy::getCurlHandle(void) const
{
  return NULL;
}

//-----------------------------------------------------------------------------
// class EasyReqest
//

Easy* EasyRequest::getEasy(void) const
{
  return NULL;
}

void EasyRequest::setopt(CURLoption option, S32 value)
{
}

void EasyRequest::setoptString(CURLoption option, std::string const& value)
{
}

void EasyRequest::setPost(char* postdata, S32 size)
{ 
}

void EasyRequest::setHeaderCallback(curl_write_callback callback, void* userdata)
{
}

void EasyRequest::setWriteCallback(curl_write_callback callback, void* userdata)
{
}

void EasyRequest::setReadCallback(curl_read_callback callback, void* userdata)
{
}

void EasyRequest::setSSLCtxCallback(curl_ssl_ctx_callback callback, void* userdata)
{
}

void EasyRequest::slist_append(char const* str)
{
}

void EasyRequest::sendRequest(std::string const& url)
{
}

std::string EasyRequest::getErrorString(void)
{
  return "";
}

bool EasyRequest::getResult(CURLcode* result, TransferInfo* info)
{
  return true;
}

bool EasyRequest::wait(void) const
{
  return false;
}

bool EasyRequest::isValid(void) const
{
  return true;
}

//-----------------------------------------------------------------------------
// class Reqest
//

bool Request::getByteRange(std::string const& url, headers_t const& headers, S32 offset, S32 length, ResponderPtr responder)
{
  return true;
}

S32 Request::process(void)
{
  return 0;
}

} // namespace AICurlInterface
//==================================================================================


//==================================================================================
// Local implementation.
//

void check_multi_code(CURLMcode code) 
{
  if (code != CURLM_OK)
  {
	llinfos << "curl multi error detected: " << curl_multi_strerror(code) << llendl;
  }
}

