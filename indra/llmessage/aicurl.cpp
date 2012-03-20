/**
 * @file aicurl.cpp
 * @brief Implementation of AICurl.
 *
 * Copyright (c) 2012, Aleric Inglewood.
 * Copyright (C) 2010, Linden Research, Inc.
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
 *
 *   20/03/2012
 *   Added copyright notice for Linden Lab for those parts that were
 *   copied or derived llcurl.cpp.
 */

#include "linden_common.h"
#include "aicurl.h"
#include "llbufferstream.h"
#include "llsdserialize.h"
#include "aithreadsafe.h"

//==================================================================================
// Local variables.
//

namespace {

struct CertificateAuthority {
  std::string file;
  std::string path;
};

AIThreadSafeSingleThreadDC<CertificateAuthority> gCertificateAuthority;
typedef AISTAccess<CertificateAuthority> CertificateAuthority_wat;
typedef AISTAccessConst<CertificateAuthority> CertificateAuthority_rat;

} // Anonymous namespace

//==================================================================================
// External API
//

namespace AICurlInterface
{

// This function is used in indra/llmessage/llproxy.cpp.
CURLcode check_easy_code(CURLcode code)
{
  if (code != CURLE_OK)
  {
	llinfos << "curl easy error detected: " << curl_easy_strerror(code) << llendl;
  }
  return code;
}

// This function is not called from outside this compilation unit,
// but provided as part of the AICurlInterface anyway because check_easy_code is.
CURLMcode check_multi_code(CURLMcode code) 
{
  if (code != CURLM_OK)
  {
	llinfos << "curl multi error detected: " << curl_multi_strerror(code) << llendl;
  }
  return code;
}

void initCurl(F32 curl_request_timeout, S32 max_number_handles)
{
  DoutEntering(dc::curl, "AICurlInterface::initCurl(" << curl_request_timeout << ", " << max_number_handles << ")");

  llassert(LLThread::getRunning() == 0);		// We must not call curl_global_init unless we are the only thread.
  CURLcode res = check_easy_code(curl_global_init(CURL_GLOBAL_ALL));
  if (res != CURLE_OK)
  {
	llerrs << "curl_global_init(CURL_GLOBAL_ALL) failed." << llendl;
  }

  // Print version and do some feature sanity checks.
  {
	curl_version_info_data* version_info = curl_version_info(CURLVERSION_NOW);

	llassert_always(version_info->age >= 0);
	if (version_info->age < 1)
	{
	  llwarns << "libcurl's age is 0; no ares support." << llendl;
	}
	llassert_always((version_info->features & CURL_VERSION_SSL));	// SSL support, added in libcurl 7.10.
	if (!(version_info->features & CURL_VERSION_ASYNCHDNS));		// Asynchronous name lookups (added in libcurl 7.10.7).
	{
	  llwarns << "libcurl was not compiled with support for asynchronous name lookups!" << llendl;
	}

	llinfos << "Successful initialization of libcurl " <<
		version_info->version << " (" << version_info->version_num << "), (" <<
	    version_info->ssl_version << ", libz/" << version_info->libz_version << ")." << llendl;
  }
}

void cleanupCurl(void)
{
  DoutEntering(dc::curl, "AICurlInterface::cleanupCurl()");

  llassert(LLThread::getRunning() == 0);		// We must not call curl_global_cleanup unless we are the only thread left.
  curl_global_cleanup();
}

void startCurlThread(bool multiple_threads)
{
}

std::string getVersionString(void)
{
  // libcurl is thread safe, no locking needed.
  return curl_version();
}

void setCAFile(std::string const& file)
{
  CertificateAuthority_wat CertificateAuthority_w(gCertificateAuthority);
  CertificateAuthority_w->file = file;
}

// This function is not called from anywhere, but provided as part of AICurlInterface because setCAFile is.
void setCAPath(std::string const& path)
{
  CertificateAuthority_wat CertificateAuthority_w(gCertificateAuthority);
  CertificateAuthority_w->path = path;
}

std::string strerror(CURLcode errorcode)
{
  // libcurl is thread safe, no locking needed.
  return curl_easy_strerror(errorcode);
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

