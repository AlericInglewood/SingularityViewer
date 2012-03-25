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
 *   copied or derived from llcurl.cpp.
 */

#include "linden_common.h"

#define OPENSSL_THREAD_DEFINES
#include <openssl/opensslconf.h>	// OPENSSL_THREADS
#include <openssl/crypto.h>

#include "aicurl.h"
#include "llbufferstream.h"
#include "llsdserialize.h"
#include "aithreadsafe.h"
#include "llqueuedthread.h"

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

enum gSSLlib_type {
  ssl_unknown,
  ssl_openssl,
  ssl_gnutls,
  ssl_nss
};

// No locking needed: initialized before threads are created, and subsequently only read.
gSSLlib_type gSSLlib;

} // namespace

//-----------------------------------------------------------------------------------
// Needed for thread-safe openSSL operation.

// Must be defined in global namespace.
struct CRYPTO_dynlock_value
{
  AIRWLock rwlock;
};

namespace {

AIRWLock* ssl_rwlock_array;

// OpenSSL locking function.
void ssl_locking_function(int mode, int n, char const* file, int line)
{
  if ((mode & CRYPTO_LOCK))
  {
	if ((mode & CRYPTO_READ))
	  ssl_rwlock_array[n].rdlock();
	else
	  ssl_rwlock_array[n].wrlock();
  }
  else
  {
    if ((mode & CRYPTO_READ))
	  ssl_rwlock_array[n].rdunlock();
	else
	  ssl_rwlock_array[n].wrunlock();
  }
}

// OpenSSL uniq id function.
void ssl_id_function(CRYPTO_THREADID* thread_id)
{
#if 1	// apr_os_thread_current() returns an unsigned long.
  CRYPTO_THREADID_set_numeric(thread_id, apr_os_thread_current());
#else	// if it would return a pointer.
  CRYPTO_THREADID_set_pointer(thread_id, apr_os_thread_current());
#endif
}

// OpenSSL allocate and initialize dynamic crypto lock.
CRYPTO_dynlock_value* ssl_dyn_create_function(char const* file, int line)
{
  return new CRYPTO_dynlock_value;
}

// OpenSSL destroy dynamic crypto lock.
void ssl_dyn_destroy_function(CRYPTO_dynlock_value* l, char const* file, int line)
{
  delete l;
}

// OpenSSL dynamic locking function.
void ssl_dyn_lock_function(int mode, CRYPTO_dynlock_value* l, char const* file, int line)
{
  if ((mode & CRYPTO_LOCK))
  {
	if ((mode & CRYPTO_READ))
	  l->rwlock.rdlock();
	else
	  l->rwlock.wrlock();
  }
  else
  {
    if ((mode & CRYPTO_READ))
	  l->rwlock.rdunlock();
	else
	  l->rwlock.wrunlock();
  }
}

typedef void (*ssl_locking_function_type)(int, int, char const*, int);
typedef void (*ssl_id_function_type)(CRYPTO_THREADID*);
typedef CRYPTO_dynlock_value* (*ssl_dyn_create_function_type)(char const*, int);
typedef void (*ssl_dyn_destroy_function_type)(CRYPTO_dynlock_value*, char const*, int);
typedef void (*ssl_dyn_lock_function_type)(int, CRYPTO_dynlock_value*, char const*, int);

ssl_locking_function_type     old_ssl_locking_function;
ssl_id_function_type          old_ssl_id_function;
ssl_dyn_create_function_type  old_ssl_dyn_create_function;
ssl_dyn_destroy_function_type old_ssl_dyn_destroy_function;
ssl_dyn_lock_function_type    old_ssl_dyn_lock_function;

// Initialize OpenSSL library for thread-safety.
void ssl_init(void)
{
  // Static locks vector.
  ssl_rwlock_array = new AIRWLock[CRYPTO_num_locks()];
  // Static locks callbacks.
  old_ssl_locking_function = CRYPTO_get_locking_callback();
  old_ssl_id_function = CRYPTO_THREADID_get_callback();
  CRYPTO_set_locking_callback(&ssl_locking_function);
  CRYPTO_THREADID_set_callback(&ssl_id_function);		// Setting this avoids the need for a thread-local error number facility, which is hard to check.
  // Dynamic locks callbacks.
  old_ssl_dyn_create_function = CRYPTO_get_dynlock_create_callback();
  old_ssl_dyn_lock_function = CRYPTO_get_dynlock_lock_callback();
  old_ssl_dyn_destroy_function = CRYPTO_get_dynlock_destroy_callback();
  CRYPTO_set_dynlock_create_callback(&ssl_dyn_create_function);
  CRYPTO_set_dynlock_lock_callback(&ssl_dyn_lock_function);
  CRYPTO_set_dynlock_destroy_callback(&ssl_dyn_destroy_function);
}

// Cleanup OpenSLL library thread-safety.
void ssl_cleanup(void)
{
  // Dynamic locks callbacks.
  CRYPTO_set_dynlock_destroy_callback(old_ssl_dyn_destroy_function);
  CRYPTO_set_dynlock_lock_callback(old_ssl_dyn_lock_function);
  CRYPTO_set_dynlock_create_callback(old_ssl_dyn_create_function);
  // Static locks callbacks.
  CRYPTO_THREADID_set_callback(old_ssl_id_function);
  CRYPTO_set_locking_callback(old_ssl_locking_function);
  // Static locks vector.
  delete [] ssl_rwlock_array;
}

} // namespace openSSL
//-----------------------------------------------------------------------------------


//==================================================================================
// External API
//

namespace AICurlInterface
{

#undef AICurlPrivate
using AICurlPrivate::check_easy_code;

// MAIN-THREAD
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

	// Detect SSL library used.
	gSSLlib = ssl_unknown;
	std::string ssl_version(version_info->ssl_version);
	if (ssl_version.find("OpenSSL") != std::string::npos)
	  gSSLlib = ssl_openssl;										// See http://www.openssl.org/docs/crypto/threads.html#DESCRIPTION
	else if (ssl_version.find("GnuTLS") != std::string::npos)
	  gSSLlib = ssl_gnutls;											// See http://www.gnu.org/software/gnutls/manual/html_node/Thread-safety.html
	else if (ssl_version.find("NSS") != std::string::npos)
	  gSSLlib = ssl_nss;											// Supposedly thread-safe without any requirements.

	// Set up thread-safety requirements of underlaying SSL library.
	// See http://curl.haxx.se/libcurl/c/libcurl-tutorial.html
	switch (gSSLlib)
	{
	  case ssl_unknown:
	  {
		llerrs << "Unknown SSL library \"" << version_info->ssl_version << "\", required actions for thread-safe handling are unknown! Bailing out." << llendl;
	  }
	  case ssl_openssl:
	  {
#ifndef OPENSSL_THREADS
		llerrs << "OpenSSL was not configured with thread support! Bailing out." << llendl;
#endif
		ssl_init();
	  }
	  case ssl_gnutls:
	  {
		// FIXME. I don't think we ever get here, do we?
		llassert_always(gSSLlib != ssl_gnutls);
		// If we do, then didn't curl_global_init already call gnutls_global_init?
		// It seems there is nothing to do for us here.
	  }
	  case ssl_nss:
	  {
	    break;	// No requirements.
	  }
	}
  }
}

// MAIN-THREAD
void cleanupCurl(void)
{
  DoutEntering(dc::curl, "AICurlInterface::cleanupCurl()");

  ssl_cleanup();

  llassert(LLThread::getRunning() == 0);		// We must not call curl_global_cleanup unless we are the only thread left.
  curl_global_cleanup();
}

// MAIN-THREAD
void startCurlThread(bool multiple_threads)
{
  llassert(is_main_thread());
}

// THREAD-SAFE
std::string getVersionString(void)
{
  // libcurl is thread safe, no locking needed.
  return curl_version();
}

// THREAD-SAFE
void setCAFile(std::string const& file)
{
  CertificateAuthority_wat CertificateAuthority_w(gCertificateAuthority);
  CertificateAuthority_w->file = file;
}

// This function is not called from anywhere, but provided as part of AICurlInterface because setCAFile is.
// THREAD-SAFE
void setCAPath(std::string const& path)
{
  CertificateAuthority_wat CertificateAuthority_w(gCertificateAuthority);
  CertificateAuthority_w->path = path;
}

// THREAD-SAFE
std::string strerror(CURLcode errorcode)
{
  // libcurl is thread safe, no locking needed.
  return curl_easy_strerror(errorcode);
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

class AICurlMulti {
};

class AICurlThread : public LLQueuedThread
{
  class CurlRequest : public LLQueuedThread::QueuedRequest
  {
	protected:
	  /*virtual*/ ~CurlRequest();			// Use deleteRequest().
	  
	public:
	  CurlRequest(handle_t handle, AICurlMulti* multi, AICurlThread* curl_thread);

	  /*virtual*/ bool processRequest(void);
	  /*virtual*/ void finishRequest(bool completed);
  };

  public:
	AICurlThread(bool threaded = true);
	virtual ~AICurlThread();
};

namespace AICurlPrivate
{

// THREAD-SAFE
CURLcode check_easy_code(CURLcode code)
{
  if (code != CURLE_OK)
  {
	llinfos << "curl easy error detected: " << curl_easy_strerror(code) << llendl;
  }
  return code;
}

// THREAD-SAFE
CURLMcode check_multi_code(CURLMcode code) 
{
  if (code != CURLM_OK)
  {
	llinfos << "curl multi error detected: " << curl_multi_strerror(code) << llendl;
  }
  return code;
}

//-----------------------------------------------------------------------------
// AICurlEasyHandle (base classes)
//

LLAtomicU32 CurlEasyHandle::sTotalEasyHandles;

CurlEasyHandle::CurlEasyHandle(void) throw(AICurlNoEasyHandle) : mActive(NULL)
{
  mEasyHandle = curl_easy_init();
  if (!mEasyHandle)
  {
	throw AICurlNoEasyHandle("curl_easy_init() returned NULL");
  }
  sTotalEasyHandles++;
}

CurlEasyHandle::~CurlEasyHandle()
{
  llassert(!mActive);
  curl_easy_cleanup(mEasyHandle);
  --sTotalEasyHandles;
}

CURLcode CurlEasyHandle::getinfo(CURLINFO info, void* data)
{
  return check_easy_code(curl_easy_getinfo(mEasyHandle, info, data));
}

char* CurlEasyHandle::escape(char* url, int length)
{
  return curl_easy_escape(mEasyHandle, url, length);
}

char* CurlEasyHandle::unescape(char* url, int inlength , int* outlength)
{
  return curl_easy_unescape(mEasyHandle, url, inlength, outlength);
}

CURLcode CurlEasyHandle::perform(void)
{
  llassert(!mActive);
  return check_easy_code(curl_easy_perform(mEasyHandle));
}

CURLcode CurlEasyHandle::pause(int bitmask)
{
  return check_easy_code(curl_easy_pause(mEasyHandle, bitmask));
}

CURLMcode CurlEasyHandle::add_handle_to_multi(CURLM* multi)
{
  llassert_always(!mActive && multi);
  mActive = multi;
  return check_multi_code(curl_multi_add_handle(multi, mEasyHandle));
}

CURLMcode CurlEasyHandle::remove_handle_from_multi(CURLM* multi)
{
  llassert_always(mActive && mActive == multi);
  mActive = NULL;
  return check_multi_code(curl_multi_remove_handle(multi, mEasyHandle));
}

void intrusive_ptr_add_ref(AIThreadSafeCurlEasyHandle* threadsafe_curl_easy_handle)
{
  threadsafe_curl_easy_handle->mReferenceCount++;
}

void intrusive_ptr_release(AIThreadSafeCurlEasyHandle* threadsafe_curl_easy_handle)
{
  if (--threadsafe_curl_easy_handle->mReferenceCount == 0)
  {
	delete threadsafe_curl_easy_handle;
  }
}

//-----------------------------------------------------------------------------
// AICurlMultiHandle (base classes)
//

LLAtomicU32 CurlMultiHandle::sTotalMultiHandles;

CurlMultiHandle::CurlMultiHandle(void) throw(AICurlNoMultiHandle)
{
  mMultiHandle = curl_multi_init();
  if (!mMultiHandle)
  {
	throw AICurlNoMultiHandle("curl_multi_init() returned NULL");
  }
  sTotalMultiHandles++;
}

CurlMultiHandle::~CurlMultiHandle()
{
  // This thread was terminated.
  // Curl demands that all handles are removed from the multi session handle before calling curl_multi_cleanup.
  for(addedEasyHandles_type::iterator iter = mAddedEasyHandles.begin(); iter != mAddedEasyHandles.end(); iter = mAddedEasyHandles.begin())
  {
	remove_easy_handle(*iter);
  }
  curl_multi_cleanup(mMultiHandle);
  --sTotalMultiHandles;
}

CURLMcode CurlMultiHandle::perform(int* running_handles)
{
  return check_multi_code(curl_multi_perform(mMultiHandle, running_handles));
}

CURLMsg* CurlMultiHandle::info_read(int* msgs_in_queue)
{
  return curl_multi_info_read(mMultiHandle, msgs_in_queue);
}

CURLMcode CurlMultiHandle::add_easy_handle(AICurlEasyHandle const& easy_handle)
{
  std::pair<addedEasyHandles_type::iterator, bool> res = mAddedEasyHandles.insert(easy_handle);
  llassert(res.second);		// May not have been added before.
  return AICurlEasyHandle_wat(*easy_handle)->add_handle_to_multi(mMultiHandle);
}

CURLMcode CurlMultiHandle::remove_easy_handle(AICurlEasyHandle const& easy_handle)
{
  addedEasyHandles_type::iterator iter = mAddedEasyHandles.find(easy_handle);
  llassert(iter != mAddedEasyHandles.end());	// Must have been added before.
  CURLMcode res = AICurlEasyHandle_wat(**iter)->remove_handle_from_multi(mMultiHandle);
  mAddedEasyHandles.erase(iter);
  return res;
}

} // namespace AICurlPrivate

//static
AICurlMultiHandle& AICurlMultiHandle::getInstance(void) throw(AICurlNoMultiHandle)
{
  LLThreadLocalData& tldata = LLThreadLocalData::tldata();
  if (!tldata.mCurlMultiHandle)
  {
	llinfos << "Creating AICurlMultiHandle for thread \"" << tldata.mName << "\"." << llendl;
	tldata.mCurlMultiHandle = new AICurlMultiHandle;
  }
  return *static_cast<AICurlMultiHandle*>(tldata.mCurlMultiHandle);
}
