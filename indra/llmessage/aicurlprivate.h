/**
 * @file aicurlprivate.h
 * @brief Thread safe wrapper for libcurl.
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
 *   28/04/2012
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AICURLPRIVATE_H
#define AICURLPRIVATE_H

namespace AICurlPrivate {
namespace curlthread { class MultiHandle; }

CURLcode check_easy_code(CURLcode code);
CURLMcode check_multi_code(CURLMcode code);

bool curlThreadIsRunning(void);
void wakeUpCurlThread(void);

class AIThreadSafeCurlEasyRequest;

// This class wraps CURL*'s.
// It guarantees that a pointer is cleaned up when no longer needed, as required by libcurl.
class CurlEasyHandle : public boost::noncopyable {
  public:
	CurlEasyHandle(void) throw(AICurlNoEasyHandle);
	~CurlEasyHandle();

  private:
	// Disallow assignment.
	CurlEasyHandle& operator=(CurlEasyHandle const*);

  public:
	// Reset all options of a libcurl session handle.
	void reset(void) { llassert(!mActive); curl_easy_reset(mEasyHandle); }

	// Set options for a curl easy handle.
	template<typename BUILTIN>
	  CURLcode setopt(CURLoption option, BUILTIN parameter);

	// Clone a libcurl session handle using all the options previously set.
	CurlEasyHandle(CurlEasyHandle const& orig) : mEasyHandle(curl_easy_duphandle(orig.mEasyHandle)), mActive(false), mErrorBuffer(NULL) { }

	// URL encode/decode the given string.
	char* escape(char* url, int length);
	char* unescape(char* url, int inlength , int* outlength);

	// Extract information from a curl handle.
	CURLcode getinfo(CURLINFO info, void* data);
	// Perform a file transfer (blocking).
	CURLcode perform(void);
	// Pause and unpause a connection.
	CURLcode pause(int bitmask);

  private:
	CURL* mEasyHandle;
	CURLM* mActive;
	char* mErrorBuffer;
	static LLAtomicU32 sTotalEasyHandles;

  private:
	// This should only be called from MultiHandle; add/remove an easy handle to/from a multi handle.
	friend class curlthread::MultiHandle;
	CURLMcode add_handle_to_multi(CURLM* multi_handle);
	CURLMcode remove_handle_from_multi(CURLM* multi_handle);

  public:
	// Retuns total number of existing CURL* handles (excluding ones created outside this class).
	static U32 getTotalEasyHandles(void) { return sTotalEasyHandles; }

	// Returns true if this easy handle was added to a curl multi handle.
	bool active(void) const { return mActive; }

	// Call this prior to every curl_easy function whose return value is passed to check_easy_code.
	void setErrorBuffer(void);

	// If there was an error code as result, then this returns a human readable error string.
	// Only valid when setErrorBuffer was called and the curl_easy function returned an error.
	std::string getErrorString(void) const { return mErrorBuffer; }

	// Used for debugging purposes.
	bool operator==(CURL* easy_handle) const { return mEasyHandle == easy_handle; }

  protected:
	// Return the underlaying curl easy handle.
	CURL* getEasyHandle(void) const { return mEasyHandle; }

  private:
	// Return, and possibly create, the curl (easy) error buffer used by the current thread.
	static char* getTLErrorBuffer(void);
};

template<typename BUILTIN>
CURLcode CurlEasyHandle::setopt(CURLoption option, BUILTIN parameter)
{
  llassert(!mActive);
  setErrorBuffer();
  return check_easy_code(curl_easy_setopt(mEasyHandle, option, parameter));
}

// CurlEasyRequest adds a slightly more powerful interface that can be used
// to set the options on a curl easy handle.
//
// Calling sendRequest() will then connect to the given URL and perform
// the data exchange. If an error occurs related to this handle, it can
// be read by calling getErrorString().
//
class CurlEasyRequest : public CurlEasyHandle {
  public:
	void setoptString(CURLoption option, std::string const& value);
	void setPost(char const* postdata, S32 size);
	void setHeaderCallback(curl_write_callback callback, void* userdata);
	void setWriteCallback(curl_write_callback callback, void* userdata);
	void setReadCallback(curl_read_callback callback, void* userdata);
	void setSSLCtxCallback(curl_ssl_ctx_callback callback, void* userdata);
	void addHeader(char const* str);

	// Call this if the set callbacks are about to be invalidated.
	void revokeCallbacks(void);

	// Set default options that we want applied to all curl easy handles.
	void applyDefaultOptions(void);

	// Prepare the request for adding it to a multi session, or calling perform.
	// This actually adds the headers that were collected with addHeader.
	void finalizeRequest(std::string const& url);

	// Call this to poll if the transfer already finished.
	// Returns true when result is valid.
	// If result != CURLE_FAILED_INIT then also info was filled.
	bool getResult(CURLcode* result, AICurlInterface::TransferInfo* info = NULL);

	// This is called when the viewer can't do much else without this being finished.
	// It could even block in the old implementation. The caller expects it to return
	// true when the transaction completed (possibly with an error).
	// If it returns false, you have to call it again, basically (the difference with
	// getResult() is that in this case the priority is boosted to the max, so it
	// will finish as soon as possible).
	bool wait(void) const;

  private:
	curl_slist* mHeaders;
	bool mRequestFinalized;

  public:
	CurlEasyRequest(void) : mHeaders(NULL), mRequestFinalized(false) { applyDefaultOptions(); }

	// For debugging purposes
	bool is_finalized(void) const { return mRequestFinalized; }
};

// This class wraps CurlEasyRequest for thread-safety and adds a reference counter so we can
// copy it around cheaply and it gets destructed automatically when the last instance is deleted.
// It guarantees that the CURL* handle is never used concurrently, which is not allowed by libcurl.
// As AIThreadSafeSimpleDC contains a mutex, it cannot be copied. Therefore we need a reference counter for this object.
class AIThreadSafeCurlEasyRequest : public AIThreadSafeSimpleDC<CurlEasyRequest> {
  public:
	AIThreadSafeCurlEasyRequest(void) throw(AICurlNoEasyHandle) : mReferenceCount(0) { Dout(dc::curl, "Creating AIThreadSafeCurlEasyRequest with this = " << (void*)this); }
	~AIThreadSafeCurlEasyRequest() { Dout(dc::curl, "Destructing AIThreadSafeCurlEasyRequest with this = " << (void*)this); }

  private:
	LLAtomicU32 mReferenceCount;

	friend void intrusive_ptr_add_ref(AIThreadSafeCurlEasyRequest* p);	// Called by boost::intrusive_ptr when a new copy of a boost::intrusive_ptr<AIThreadSafeCurlEasyQuest> is made.
	friend void intrusive_ptr_release(AIThreadSafeCurlEasyRequest* p);	// Called by boost::intrusive_ptr when a boost::intrusive_ptr<AIThreadSafeCurlEasyRequest> is destroyed.
};

// This class wraps CURLM*'s.
// It guarantees that a pointer is cleaned up when no longer needed, as required by libcurl.
class CurlMultiHandle : public boost::noncopyable {
  public:
	CurlMultiHandle(void) throw(AICurlNoMultiHandle);
	~CurlMultiHandle();

  private:
	// Disallow assignment.
	CurlMultiHandle& operator=(CurlMultiHandle const*);

  private:
	static LLAtomicU32 sTotalMultiHandles;

  protected:
	CURLM* mMultiHandle;

  public:
	// Set options for a curl multi handle.
	template<typename BUILTIN>
	  CURLMcode setopt(CURLMoption option, BUILTIN parameter);

	// Returns total number of existing CURLM* handles (excluding ones created outside this class).
	static U32 getTotalMultiHandles(void) { return sTotalMultiHandles; }
};

template<typename BUILTIN>
CURLMcode CurlMultiHandle::setopt(CURLMoption option, BUILTIN parameter)
{
  return check_multi_code(curl_multi_setopt(mMultiHandle, option, parameter));
}

} // namespace AICurlPrivate

#endif
