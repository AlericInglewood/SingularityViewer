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

void intrusive_ptr_add_ref(Responder* p)
{
}

void intrusive_ptr_release(Responder* p)
{
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

void Responder::completedHeader(U32 status, std::string const& reason, LLSD const& content)
{
}

void Responder::errorWithContent(U32 status, std::string const& reason, LLSD const& content)
{
}

void Responder::result(LLSD const& content)
{
}

void Responder::completedRaw(U32 status, std::string const& reason, LLChannelDescriptors const& channels, LLIOPipe::buffer_ptr_t const& buffer)
{
}

void Responder::completed(U32 status, std::string const& reason, LLSD const& content)
{
}

void Responder::setURL(std::string const& url)
{
}

void Responder::error(U32 status, std::string const& reason)
{
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

