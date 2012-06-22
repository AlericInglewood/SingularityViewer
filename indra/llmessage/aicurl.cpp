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
 *   copied or derived from llcurl.cpp. The code of those parts are
 *   already in their own llcurl.cpp, so they do not ever need to
 *   even look at this file; the reason I added the copyright notice
 *   is to make clear that I am not the author of 100% of this code
 *   and hence I cannot change the license of it.
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
#include "lltimer.h"		// ms_sleep
#include "llproxy.h"
#ifdef CWDEBUG
#include <libcwd/buf2str.h>
#endif

//==================================================================================
// Local variables.
//

namespace {

struct CertificateAuthority {
  std::string file;
  std::string path;
};

AIThreadSafeSimpleDC<CertificateAuthority> gCertificateAuthority;
typedef AIAccess<CertificateAuthority> CertificateAuthority_wat;
typedef AIAccessConst<CertificateAuthority> CertificateAuthority_rat;

enum gSSLlib_type {
  ssl_unknown,
  ssl_openssl,
  ssl_gnutls,
  ssl_nss
};

// No locking needed: initialized before threads are created, and subsequently only read.
gSSLlib_type gSSLlib;
bool gSetoptParamsNeedDup;

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

namespace AICurlInterface {

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
		// I don't think we ever get here, do we? --Aleric
		llassert_always(gSSLlib != ssl_gnutls);
		// If we do, then didn't curl_global_init already call gnutls_global_init?
		// It seems there is nothing to do for us here.
	  }
	  case ssl_nss:
	  {
	    break;	// No requirements.
	  }
	}

	gSetoptParamsNeedDup = (version_info->version_num < 0x071700);
	if (gSetoptParamsNeedDup)
	{
	  llwarns << "Your libcurl version is too old." << llendl;
	}
	llassert_always(!gSetoptParamsNeedDup);		// Might add support later.
  }
}

// MAIN-THREAD
void cleanupCurl(void)
{
  using AICurlPrivate::curlThreadIsRunning;

  DoutEntering(dc::curl, "AICurlInterface::cleanupCurl()");

  ssl_cleanup();

  llassert(LLThread::getRunning() <= (curlThreadIsRunning() ? 1 : 0));		// We must not call curl_global_cleanup unless we are the only thread left.
  curl_global_cleanup();
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
}

Responder::~Responder()
{
  DoutEntering(dc::curl, "AICurlInterface::Responder::~Responder() with this = " << (void*)this << "; mReferenceCount = " << mReferenceCount);
  llassert(mReferenceCount == 0);
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

} // namespace AICurlInterface
//==================================================================================


//==================================================================================
// Local implementation.
//

namespace AICurlPrivate {

// THREAD-SAFE
CURLcode check_easy_code(CURLcode code)
{
  if (code != CURLE_OK)
  {
	char* error_buffer = LLThreadLocalData::tldata().mCurlErrorBuffer;
	llinfos << "curl easy error detected: " << curl_easy_strerror(code);
	if (error_buffer && *error_buffer != '\0')
	{
	  llcont << ": " << error_buffer;
	}
	llcont << llendl;
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

//=============================================================================
// AICurlEasyRequest (base classes)
//

//-----------------------------------------------------------------------------
// CurlEasyHandle

LLAtomicU32 CurlEasyHandle::sTotalEasyHandles;

CurlEasyHandle::CurlEasyHandle(void) throw(AICurlNoEasyHandle) : mActiveMultiHandle(NULL), mErrorBuffer(NULL)
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
  llassert(!mActiveMultiHandle);
  curl_easy_cleanup(mEasyHandle);
  --sTotalEasyHandles;
}

//static
char* CurlEasyHandle::getTLErrorBuffer(void)
{
  LLThreadLocalData& tldata = LLThreadLocalData::tldata();
  if (!tldata.mCurlErrorBuffer)
  {
	tldata.mCurlErrorBuffer = new char[CURL_ERROR_SIZE];
  }
  return tldata.mCurlErrorBuffer;
}

void CurlEasyHandle::setErrorBuffer(void)
{
  char* error_buffer = getTLErrorBuffer();
  if (mErrorBuffer != error_buffer)
  {
	curl_easy_setopt(mEasyHandle, CURLOPT_ERRORBUFFER, error_buffer);
	mErrorBuffer = error_buffer;
  }
}

CURLcode CurlEasyHandle::getinfo(CURLINFO info, void* data)
{
  setErrorBuffer();
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
  llassert(!mActiveMultiHandle);
  setErrorBuffer();
  return check_easy_code(curl_easy_perform(mEasyHandle));
}

CURLcode CurlEasyHandle::pause(int bitmask)
{
  setErrorBuffer();
  return check_easy_code(curl_easy_pause(mEasyHandle, bitmask));
}

CURLMcode CurlEasyHandle::add_handle_to_multi(AICurlEasyRequest_wat& curl_easy_request_w, CURLM* multi)
{
  llassert_always(!mActiveMultiHandle && multi);
  mActiveMultiHandle = multi;
  CURLMcode res = check_multi_code(curl_multi_add_handle(multi, mEasyHandle));
  added_to_multi_handle(curl_easy_request_w);
  return res;
}

CURLMcode CurlEasyHandle::remove_handle_from_multi(AICurlEasyRequest_wat& curl_easy_request_w, CURLM* multi)
{
  llassert_always(mActiveMultiHandle && mActiveMultiHandle == multi);
  mActiveMultiHandle = NULL;
  CURLMcode res = check_multi_code(curl_multi_remove_handle(multi, mEasyHandle));
  removed_from_multi_handle(curl_easy_request_w);
  return res;
}

void intrusive_ptr_add_ref(AIThreadSafeCurlEasyRequest* threadsafe_curl_easy_request)
{
  threadsafe_curl_easy_request->mReferenceCount++;
}

void intrusive_ptr_release(AIThreadSafeCurlEasyRequest* threadsafe_curl_easy_request)
{
  if (--threadsafe_curl_easy_request->mReferenceCount == 0)
  {
	delete threadsafe_curl_easy_request;
  }
}

//-----------------------------------------------------------------------------
// CurlEasyReqest

void CurlEasyRequest::setoptString(CURLoption option, std::string const& value)
{
  llassert(!gSetoptParamsNeedDup);
  setopt(option, value.c_str());
}

void CurlEasyRequest::setPost(char const* postdata, S32 size)
{
  setopt(CURLOPT_POST, 1L);
  setopt(CURLOPT_POSTFIELDS, static_cast<void*>(const_cast<char*>(postdata)));
  setopt(CURLOPT_POSTFIELDSIZE, size);
}

AIThreadSafeCurlEasyRequest* CurlEasyRequest::get_lockobj(void)
{
  return static_cast<AIThreadSafeCurlEasyRequest*>(AIThreadSafeSimpleDC<CurlEasyRequest>::wrapper_cast(this));
}

//static
size_t CurlEasyRequest::headerCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
  CurlEasyRequest* self = static_cast<CurlEasyRequest*>(userdata);
  AIThreadSafeCurlEasyRequest* lockobj = self->get_lockobj();
  AICurlEasyRequest_wat lock_self(*lockobj);
  return self->mHeaderCallback(ptr, size, nmemb, self->mHeaderCallbackUserData);
}

void CurlEasyRequest::setHeaderCallback(curl_write_callback callback, void* userdata)
{
  mHeaderCallback = callback;
  mHeaderCallbackUserData = userdata;
  setopt(CURLOPT_HEADERFUNCTION, callback ? &CurlEasyRequest::headerCallback : NULL);
  setopt(CURLOPT_WRITEHEADER, userdata ? this : NULL);
}

//static
size_t CurlEasyRequest::writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
  CurlEasyRequest* self = static_cast<CurlEasyRequest*>(userdata);
  AIThreadSafeCurlEasyRequest* lockobj = self->get_lockobj();
  AICurlEasyRequest_wat lock_self(*lockobj);
  return self->mWriteCallback(ptr, size, nmemb, self->mWriteCallbackUserData);
}

void CurlEasyRequest::setWriteCallback(curl_write_callback callback, void* userdata)
{
  mWriteCallback = callback;
  mWriteCallbackUserData = userdata;
  setopt(CURLOPT_WRITEFUNCTION, callback ? &CurlEasyRequest::writeCallback : NULL);
  setopt(CURLOPT_WRITEDATA, userdata ? this : NULL);
}

//static
size_t CurlEasyRequest::readCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
  CurlEasyRequest* self = static_cast<CurlEasyRequest*>(userdata);
  AIThreadSafeCurlEasyRequest* lockobj = self->get_lockobj();
  AICurlEasyRequest_wat lock_self(*lockobj);
  return self->mReadCallback(ptr, size, nmemb, self->mReadCallbackUserData);
}

void CurlEasyRequest::setReadCallback(curl_read_callback callback, void* userdata)
{
  mReadCallback = callback;
  mReadCallbackUserData = userdata;
  setopt(CURLOPT_READFUNCTION, callback ? &CurlEasyRequest::readCallback : NULL);
  setopt(CURLOPT_READDATA, userdata ? this : NULL);
}

//static
CURLcode CurlEasyRequest::SSLCtxCallback(CURL* curl, void* sslctx, void* userdata)
{
  CurlEasyRequest* self = static_cast<CurlEasyRequest*>(userdata);
  AIThreadSafeCurlEasyRequest* lockobj = self->get_lockobj();
  AICurlEasyRequest_wat lock_self(*lockobj);
  return self->mSSLCtxCallback(curl, sslctx, self->mSSLCtxCallbackUserData);
}

void CurlEasyRequest::setSSLCtxCallback(curl_ssl_ctx_callback callback, void* userdata)
{
  mSSLCtxCallback = callback;
  mSSLCtxCallbackUserData = userdata;
  setopt(CURLOPT_SSL_CTX_FUNCTION, callback ? &CurlEasyRequest::SSLCtxCallback : NULL);
  setopt(CURLOPT_SSL_CTX_DATA, userdata ? this : NULL);
}

static size_t noHeaderCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
  llwarns << "Calling noHeaderCallback(); curl session aborted." << llendl;
  return 0;							// Cause a CURL_WRITE_ERROR
}

static size_t noWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
  llwarns << "Calling noWriteCallback(); curl session aborted." << llendl;
  return 0;							// Cause a CURL_WRITE_ERROR
}

static size_t noReadCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
  llwarns << "Calling noReadCallback(); curl session aborted." << llendl;
  return CURL_READFUNC_ABORT;		// Cause a CURLE_ABORTED_BY_CALLBACK
}

static CURLcode noSSLCtxCallback(CURL* curl, void* sslctx, void* parm)
{
  llwarns << "Calling noSSLCtxCallback(); curl session aborted." << llendl;
  return CURLE_ABORTED_BY_CALLBACK;
}

void CurlEasyRequest::revokeCallbacks(void)
{
  mHeaderCallback = &noHeaderCallback;
  mWriteCallback = &noWriteCallback;
  mReadCallback = &noReadCallback;
  mSSLCtxCallback = &noSSLCtxCallback;
  if (active())
  {
	llwarns << "Revoking callbacks on a still active CurlEasyRequest object!" << llendl;
  }
  curl_easy_setopt(getEasyHandle(), CURLOPT_HEADERFUNCTION, &noHeaderCallback);
  curl_easy_setopt(getEasyHandle(), CURLOPT_WRITEHEADER, &noWriteCallback);
  curl_easy_setopt(getEasyHandle(), CURLOPT_READFUNCTION, &noReadCallback);
  curl_easy_setopt(getEasyHandle(), CURLOPT_SSL_CTX_FUNCTION, &noSSLCtxCallback);
}

void CurlEasyRequest::resetState(void)
{
  revokeCallbacks();
  reset();
  curl_slist_free_all(mHeaders);
  mHeaders = NULL;
  mRequestFinalized = false;
  mParent = NULL;
  mResult = CURLE_FAILED_INIT;
  applyDefaultOptions();
}

void CurlEasyRequest::addHeader(char const* header)
{
  llassert(!mRequestFinalized);
  mHeaders = curl_slist_append(mHeaders, header);
}

#ifdef CWDEBUG
static int curl_debug_callback(CURL*, curl_infotype infotype, char* buf, size_t size, void* user_ptr)
{
  using namespace ::libcwd;

  CurlEasyRequest* request = (CurlEasyRequest*)user_ptr;
  std::ostringstream marker;
  marker << (void*)request->get_lockobj();
  libcw_do.push_marker();
  libcw_do.marker().assign(marker.str().data(), marker.str().size());
  LibcwDoutScopeBegin(LIBCWD_DEBUGCHANNELS, libcw_do, dc::curl|cond_nonewline_cf(infotype == CURLINFO_TEXT))
  switch (infotype)
  {
	case CURLINFO_TEXT:
	  LibcwDoutStream << "* ";
	  break;
	case CURLINFO_HEADER_IN:
	  LibcwDoutStream << "H> ";
	  break;
	case CURLINFO_HEADER_OUT:
	  LibcwDoutStream << "H< ";
	  break;
	case CURLINFO_DATA_IN:
	  LibcwDoutStream << "D> ";
	  break;
	case CURLINFO_DATA_OUT:
	  LibcwDoutStream << "D< ";
	  break;
	case CURLINFO_SSL_DATA_IN:
	  LibcwDoutStream << "S> ";
	  break;
	case CURLINFO_SSL_DATA_OUT:
	  LibcwDoutStream << "S< ";
	  break;
	default:
	  LibcwDoutStream << "?? ";
  }
  if (infotype == CURLINFO_TEXT)
	LibcwDoutStream.write(buf, size);
  else if (infotype == CURLINFO_HEADER_IN || infotype == CURLINFO_HEADER_OUT)
	LibcwDoutStream << libcwd::buf2str(buf, size);
  else if (infotype == CURLINFO_DATA_IN || infotype == CURLINFO_DATA_OUT)
	LibcwDoutStream << size << " bytes: \"" << libcwd::buf2str(buf, size) << '"';
  else
	LibcwDoutStream << size << " bytes";
  LibcwDoutScopeEnd;
  libcw_do.pop_marker();
  return 0;
}
#endif

void CurlEasyRequest::applyDefaultOptions(void)
{
  CertificateAuthority_rat CertificateAuthority_r(gCertificateAuthority);
  setoptString(CURLOPT_CAINFO, CertificateAuthority_r->file);
  // This option forces openssl to use TLS version 1.
  // The Linden Lab servers don't support later TLS versions, and libopenssl-1.0.1c has
  // a bug were renegotion fails (see http://rt.openssl.org/Ticket/Display.html?id=2828),
  // causing the connection to fail completely without this hack.
  // For a commandline test of the same, observe the difference between:
  // openssl s_client       -connect login.agni.lindenlab.com:443 -CAfile packaged/app_settings/CA.pem -debug
  // and
  // openssl s_client -tls1 -connect login.agni.lindenlab.com:443 -CAfile packaged/app_settings/CA.pem -debug
  setopt(CURLOPT_SSLVERSION, (long)CURL_SSLVERSION_TLSv1);
  setopt(CURLOPT_NOSIGNAL, 1);
  //setopt(CURLOPT_DNS_CACHE_TIMEOUT, 0);
  Debug(
	if (dc::curl.is_on())
	{
	  setopt(CURLOPT_VERBOSE, 1);					// Usefull for debugging.
	  setopt(CURLOPT_DEBUGFUNCTION, &curl_debug_callback);
	  setopt(CURLOPT_DEBUGDATA, this);
	}
  );
}

void CurlEasyRequest::finalizeRequest(std::string const& url)
{
  llassert(!mRequestFinalized);
  mResult = CURLE_FAILED_INIT;		// General error code, the final code is written here in MultiHandle::check_run_count when msg is CURLMSG_DONE.
  mRequestFinalized = true;
  lldebugs << url << llendl;
  setopt(CURLOPT_HTTPHEADER, mHeaders);
  setoptString(CURLOPT_URL, url);
  setopt(CURLOPT_PRIVATE, get_lockobj());
}

void CurlEasyRequest::getTransferInfo(AICurlInterface::TransferInfo* info)
{
  // Curl explicitly demands a double for these info's.
  double size, total_time, speed;
  getinfo(CURLINFO_SIZE_DOWNLOAD, &size);
  getinfo(CURLINFO_TOTAL_TIME, &total_time);
  getinfo(CURLINFO_SPEED_DOWNLOAD, &speed);
  // Convert to F64.
  info->mSizeDownload = size;
  info->mTotalTime = total_time;
  info->mSpeedDownload = speed;
}

void CurlEasyRequest::getResult(CURLcode* result, AICurlInterface::TransferInfo* info)
{
  *result = mResult;
  if (info && mResult != CURLE_FAILED_INIT)
  {
	getTransferInfo(info);
  }
}

//-----------------------------------------------------------------------------
// CurlResponderBuffer

static unsigned int const MAX_REDIRECTS = 5;
static S32 const CURL_REQUEST_TIMEOUT = 30;		// Seconds per operation.

LLChannelDescriptors const CurlResponderBuffer::sChannels;

void CurlResponderBuffer::resetState(AICurlEasyRequest_wat& curl_easy_request_w)
{
  curl_easy_request_w->resetState();

  mOutput.reset();
  
  mInput.str("");
  mInput.clear();
  
  mHeaderOutput.str("");
  mHeaderOutput.clear();
}

AIThreadSafeBufferedCurlEasyRequest* CurlResponderBuffer::get_lockobj(void)
{
  return static_cast<AIThreadSafeBufferedCurlEasyRequest*>(AIThreadSafeSimple<CurlResponderBuffer>::wrapper_cast(this));
}

void CurlResponderBuffer::prepRequest(AICurlEasyRequest_wat& curl_easy_request_w, std::vector<std::string> const& headers, AICurlInterface::ResponderPtr responder, S32 time_out, bool post)
{
  // The old code did this, but it seems nonsense; prepRequest is only called for freshly created curl easy requests.
  //resetState(curl_easy_request_w);
 
  if (post)
  {
	curl_easy_request_w->setoptString(CURLOPT_ENCODING, "");
  }

  // Set the CURL options for either Socks or HTTP proxy.
  LLProxy::getInstance()->applyProxySettings(curl_easy_request_w);

  mOutput.reset(new LLBufferArray);
  mOutput->setThreaded(true);

  AIThreadSafeBufferedCurlEasyRequest* lockobj = get_lockobj();
  curl_easy_request_w->setWriteCallback(&curlWriteCallback, lockobj);
  curl_easy_request_w->setReadCallback(&curlReadCallback, lockobj);
  curl_easy_request_w->setHeaderCallback(&curlHeaderCallback, lockobj);

  // Allow up to five redirects.
  if (responder && responder->followRedir())
  {
	curl_easy_request_w->setopt(CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_request_w->setopt(CURLOPT_MAXREDIRS, MAX_REDIRECTS);
  }

  curl_easy_request_w->setopt(CURLOPT_SSL_VERIFYPEER, true);
  // Don't verify host name so urls with scrubbed host names will work (improves DNS performance).
  curl_easy_request_w->setopt(CURLOPT_SSL_VERIFYHOST, 0);

  curl_easy_request_w->setopt(CURLOPT_TIMEOUT, llmax(time_out, CURL_REQUEST_TIMEOUT));

  mResponder = responder;

  if (!post)
  {
	curl_easy_request_w->addHeader("Connection: keep-alive");
	curl_easy_request_w->addHeader("Keep-alive: 300");
	// Add other headers.
	for (std::vector<std::string>::const_iterator iter = headers.begin(); iter != headers.end(); ++iter)
	{
	  curl_easy_request_w->addHeader((*iter).c_str());
	}
  }

  // Register for events.
  curl_easy_request_w->set_parent(this);
}

//static
size_t CurlResponderBuffer::curlWriteCallback(char* data, size_t size, size_t nmemb, void* user_data)
{
  AIThreadSafeBufferedCurlEasyRequest* lockobj = static_cast<AIThreadSafeBufferedCurlEasyRequest*>(user_data);

  // We need to lock the curl easy request object too, because that lock is used
  // to make sure that callbacks and destruction aren't done simulaneously.
  AICurlEasyRequest_wat buffered_easy_request_w(*lockobj);

  AICurlResponderBuffer_wat buffer_w(*lockobj);
  S32 n = size * nmemb;
  buffer_w->getOutput()->append(sChannels.in(), (U8 const*)data, n);
  return n;
}

//static
size_t CurlResponderBuffer::curlReadCallback(char* data, size_t size, size_t nmemb, void* user_data)
{
  AIThreadSafeBufferedCurlEasyRequest* lockobj = static_cast<AIThreadSafeBufferedCurlEasyRequest*>(user_data);

  // We need to lock the curl easy request object too, because that lock is used
  // to make sure that callbacks and destruction aren't done simulaneously.
  AICurlEasyRequest_wat buffered_easy_request_w(*lockobj);

  AICurlResponderBuffer_wat buffer_w(*lockobj);
  S32 n = size * nmemb;
  S32 startpos = buffer_w->getInput().tellg();
  buffer_w->getInput().seekg(0, std::ios::end);
  S32 endpos = buffer_w->getInput().tellg();
  buffer_w->getInput().seekg(startpos, std::ios::beg);
  S32 maxn = endpos - startpos;
  n = llmin(n, maxn);
  buffer_w->getInput().read(data, n);
  return n;
}

//static
size_t CurlResponderBuffer::curlHeaderCallback(char* data, size_t size, size_t nmemb, void* user_data)
{
  AIThreadSafeBufferedCurlEasyRequest* lockobj = static_cast<AIThreadSafeBufferedCurlEasyRequest*>(user_data);

  // We need to lock the curl easy request object too, because that lock is used
  // to make sure that callbacks and destruction aren't done simulaneously.
  AICurlEasyRequest_wat buffered_easy_request_w(*lockobj);

  AICurlResponderBuffer_wat buffer_w(*lockobj);
  size_t n = size * nmemb;
  buffer_w->getHeaderOutput().write(data, n);
  return n;
}

void CurlResponderBuffer::added_to_multi_handle(AICurlEasyRequest_wat& curl_easy_request_w)
{
  Dout(dc::curl, "Calling CurlResponderBuffer::added_to_multi_handle(@" << (void*)&*curl_easy_request_w << ") for this = " << (void*)this);
}

void CurlResponderBuffer::finished(AICurlEasyRequest_wat& curl_easy_request_w)
{
  Dout(dc::curl, "Calling CurlResponderBuffer::finished(@" << (void*)&*curl_easy_request_w << ") for this = " << (void*)this);
}

void CurlResponderBuffer::removed_from_multi_handle(AICurlEasyRequest_wat& curl_easy_request_w)
{
  Dout(dc::curl, "Calling CurlResponderBuffer::removed_from_multi_handle(@" << (void*)&*curl_easy_request_w << ") for this = " << (void*)this);

  // Lock self.
  AIThreadSafeBufferedCurlEasyRequest* lockobj = get_lockobj();
  AICurlResponderBuffer_wat buffer_w(*lockobj);

  llassert(dynamic_cast<AIThreadSafeBufferedCurlEasyRequest*>(static_cast<AIThreadSafeCurlEasyRequest*>(AIThreadSafeCurlEasyRequest::wrapper_cast(&*curl_easy_request_w))) == lockobj);
  llassert(&*buffer_w == this);

  U32 responseCode = 0;	
  std::string responseReason;
  
  CURLcode code;
  curl_easy_request_w->getResult(&code);
  if (code == CURLE_OK)
  {
	curl_easy_request_w->getinfo(CURLINFO_RESPONSE_CODE, &responseCode);
	//*TODO: get reason from first line of mHeaderOutput
  }
  else
  {
	responseCode = 499;
	responseReason = AICurlInterface::strerror(code) + " : ";
	if (code == CURLE_FAILED_INIT)
	{
	  responseReason += "Curl Easy Handle initialization failed.";
	}
	else
	{
	  responseReason += curl_easy_request_w->getErrorString();
	}
	curl_easy_request_w->setopt(CURLOPT_FRESH_CONNECT, TRUE);
  }

  if (mResponder)
  {	
	mResponder->completedRaw(responseCode, responseReason, sChannels, mOutput);
	mResponder = NULL;
  }

  resetState(curl_easy_request_w);
}

//-----------------------------------------------------------------------------
// CurlMultiHandle

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
  curl_multi_cleanup(mMultiHandle);
  --sTotalMultiHandles;
}

} // namespace AICurlPrivate

//==================================================================================
// External API
//

namespace AICurlInterface {

//-----------------------------------------------------------------------------
// class Request
//

bool Request::getByteRange(std::string const& url, headers_t const& headers, S32 offset, S32 length, ResponderPtr responder)
{
  DoutEntering(dc::curl, "Request::getByteRange(" << url << ", ...)");

  AICurlEasyRequest buffered_easy_request(true);

  {
    AICurlEasyRequest_wat buffered_easy_request_w(*buffered_easy_request);

	AICurlResponderBuffer_wat(*buffered_easy_request)->prepRequest(buffered_easy_request_w, headers, responder);

	buffered_easy_request_w->setopt(CURLOPT_HTTPGET, 1);
	if (length > 0)
	{
	  std::string range = llformat("Range: bytes=%d-%d", offset, offset + length - 1);
	  buffered_easy_request_w->addHeader(range.c_str());
	}

	buffered_easy_request_w->finalizeRequest(url);
  }

  buffered_easy_request.addRequest();

  return true;	// FIXME
}

bool Request::post(std::string const& url, headers_t const& headers, std::string const& data, ResponderPtr responder, S32 time_out)
{
  DoutEntering(dc::curl, "Request::post(" << url << ", ...)");

  AICurlEasyRequest buffered_easy_request(true);

  {
    AICurlEasyRequest_wat buffered_easy_request_w(*buffered_easy_request);
	AICurlResponderBuffer_wat buffer_w(*buffered_easy_request);

	buffer_w->prepRequest(buffered_easy_request_w, headers, responder);

	buffer_w->getInput().write(data.data(), data.size());
	S32 bytes = buffer_w->getInput().str().length();
	buffered_easy_request_w->setPost(NULL, bytes);
	buffered_easy_request_w->addHeader("Content-Type: application/octet-stream");
	buffered_easy_request_w->finalizeRequest(url);

	lldebugs << "POSTING: " << bytes << " bytes." << llendl;
  }

  buffered_easy_request.addRequest();

  return true;	// FIXME
}

bool Request::post(std::string const& url, headers_t const& headers, LLSD const& data, ResponderPtr responder, S32 time_out)
{
  DoutEntering(dc::curl, "Request::post(" << url << ", ...)");

  AICurlEasyRequest buffered_easy_request(true);

  {
    AICurlEasyRequest_wat buffered_easy_request_w(*buffered_easy_request);
	AICurlResponderBuffer_wat buffer_w(*buffered_easy_request);

	buffer_w->prepRequest(buffered_easy_request_w, headers, responder);

	LLSDSerialize::toXML(data, buffer_w->getInput());
	S32 bytes = buffer_w->getInput().str().length();
	buffered_easy_request_w->setPost(NULL, bytes);
	buffered_easy_request_w->addHeader("Content-Type: application/llsd+xml");
	buffered_easy_request_w->finalizeRequest(url);

	lldebugs << "POSTING: " << bytes << " bytes." << llendl;
  }

  buffered_easy_request.addRequest();

  return true;	// FIXME
}

S32 Request::process(void)
{
  //FIXME: needs implementation
  //DoutEntering(dc::warning, "Request::process()");
  return 0;
}

} // namespace AICurlInterface
//==================================================================================

