/**
 * @file aicurl.h
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
 *   17/03/2012
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AICURL_H
#define AICURL_H

#include <string>
#include <vector>
#include <set>
#include <stdexcept>
#include <boost/intrusive_ptr.hpp>
#include <boost/utility.hpp>
#include <curl/curl.h>		// CURL, CURLM, CURLMcode, CURLoption, curl_*_callback

// Make sure we don't use this option: it is not thread-safe.
#undef CURLOPT_DNS_USE_GLOBAL_CACHE

#include "stdtypes.h"		// U32
#include "lliopipe.h"		// LLIOPipe::buffer_ptr_t
#include "llatomic.h"		// LLAtomicU32
#include "aithreadsafe.h"

class LLSD;

//-----------------------------------------------------------------------------
// Exceptions.
//

// A general curl exception.
//
class AICurlError : public std::runtime_error {
  public:
	AICurlError(std::string const& message) : std::runtime_error(message) { }
};

class AICurlNoEasyHandle : public AICurlError {
  public:
	AICurlNoEasyHandle(std::string const& message) : AICurlError(message) { }
};

class AICurlNoMultiHandle : public AICurlError {
  public:
	AICurlNoMultiHandle(std::string const& message) : AICurlError(message) { }
};

// End Exceptions.
//-----------------------------------------------------------------------------

// Things defined in this namespace are called from elsewhere in the viewer code.
namespace AICurlInterface {

// Output parameter of AICurlPrivate::CurlEasyRequest::getResult.
// Only used by LLXMLRPCTransaction::Impl.
struct TransferInfo {
  TransferInfo() : mSizeDownload(0.0), mTotalTime(0.0), mSpeedDownload(0.0) { }
  F64 mSizeDownload;
  F64 mTotalTime;
  F64 mSpeedDownload;
};

//-----------------------------------------------------------------------------
// Global functions.

// Called once at start of application (from newview/llappviewer.cpp by main thread (before threads are created)),
// with main purpose to initialize curl.
void initCurl(F32 curl_request_timeout = 120.f, S32 max_number_handles = 256);

// Called once at start of application (from LLAppViewer::initThreads), starts AICurlThread.
void startCurlThread(void);

// Called once at end of application (from newview/llappviewer.cpp by main thread),
// with purpose to stop curl threads, free curl resources and deinitialize curl.
void cleanupCurl(void);

// Called from indra/llmessage/llurlrequest.cpp to print debug output regarding
// an error code returned by EasyRequest::getResult.
// Just returns curl_easy_strerror(errorcode).
std::string strerror(CURLcode errorcode);

// Called from indra/newview/llfloaterabout.cpp for the About floater, and
// from newview/llappviewer.cpp in behalf of debug output.
// Just returns curl_version().
std::string getVersionString(void);

// Called from newview/llappviewer.cpp (and llcrashlogger/llcrashlogger.cpp) to set
// the Certificate Authority file used to verify HTTPS certs.
void setCAFile(std::string const& file);

// Not called from anywhere.
// Can be used to set the path to the Certificate Authority file.
void setCAPath(std::string const& file);

//-----------------------------------------------------------------------------
// Global classes.

class Responder {
  protected:
	Responder(void);
	virtual ~Responder();

  private:
	// Associated URL, used for debug output.
	std::string mURL;

  public:
	// Called to set the url of the current request for this responder,
	// used only when printing debug output regarding activity of the responder.
	void setURL(std::string const& url);

  public:
	// Called from LLHTTPClientURLAdaptor::complete():

	// Derived classes can override this to get the HTML header that was received, when the message is completed.
	// The default does nothing.
	virtual void completedHeader(U32 status, std::string const& reason, LLSD const& content);

	// Derived classes can override this to get the raw data of the body of the HTML message that was received.
	// The default is to interpret the content as LLSD and call completed().
	virtual void completedRaw(U32 status, std::string const& reason, LLChannelDescriptors const& channels, LLIOPipe::buffer_ptr_t const& buffer);

	// Called from LLHTTPClient request calls, if an error occurs even before we can call one of the above.
	// It calls completed() with a fake status U32_MAX, as that is what some derived clients expect (bad design).
	// This means that if a derived class overrides completedRaw() it now STILL has to override completed() to catch this error.
	void fatalError(std::string const& reason);

	// A derived class should return true if curl should follow redirections.
	// The default is not to follow redirections.
	virtual bool followRedir(void) { return false; }

  protected:
	// ... or, derived classes can override this to get the LLSD content when the message is completed.
	// The default is to call result() (or errorWithContent() in case of a HTML status indicating an error).
	virtual void completed(U32 status, std::string const& reason, LLSD const& content);

	// ... or, derived classes can override this to received the content of a body upon success.
	// The default does nothing.
	virtual void result(LLSD const& content);

	// Derived classes can override this to get informed when a bad HTML status code is received.
	// The default calls error().
	virtual void errorWithContent(U32 status, std::string const& reason, LLSD const& content);

	// ... or, derived classes can override this to get informed when a bad HTML statis code is received.
	// The default prints the error to llinfos.
	virtual void error(U32 status, std::string const& reason);

  public:
	// Called from LLSDMessage::ResponderAdapter::listener.
	// LLSDMessage::ResponderAdapter is a hack, showing among others by fact that these function needs to be public.

	void pubErrorWithContent(U32 status, std::string const& reason, LLSD const& content) { errorWithContent(status, reason, content); }
	void pubResult(LLSD const& content) { result(content); }

  private:
	// Used by ResponderPtr. Object is deleted when reference count reaches zero.
	LLAtomicU32 mReferenceCount;

	friend void intrusive_ptr_add_ref(Responder* p);		// Called by boost::intrusive_ptr when a new copy of a boost::intrusive_ptr<Responder> is made.
	friend void intrusive_ptr_release(Responder* p);		// Called by boost::intrusive_ptr when a boost::intrusive_ptr<Responder> is destroyed.
															// This function must delete the Responder object when the reference count reaches zero.
};

// A Responder is passed around as ResponderPtr, which causes it to automatically
// destruct when there are no pointers left pointing to it.
typedef boost::intrusive_ptr<Responder> ResponderPtr;

class Request {
  public:
	typedef std::vector<std::string> headers_t;
	
	void get(std::string const& url, ResponderPtr responder);
	bool getByteRange(std::string const& url, headers_t const& headers, S32 offset, S32 length, ResponderPtr responder);
	bool post(std::string const& url, headers_t const& headers, std::string const& data, ResponderPtr responder, S32 time_out = 0);
	bool post(std::string const& url, headers_t const& headers,        LLSD const& data, ResponderPtr responder, S32 time_out = 0);

	S32  process(void);
};

} // namespace AICurlInterface

// Forward declaration (see aicurlprivate.h).
namespace AICurlPrivate {
  class CurlEasyRequest;
} // namespace AICurlPrivate

// Define access types (_crat = Const Read Access Type, _rat = Read Access Type, _wat = Write Access Type).
// Typical usage is:
// AICurlEasyRequest h1;				// Create easy handle.
// AICurlEasyRequest h2(h1);			// Make lightweight copies.
// AICurlEasyRequest_wat h2_w(*h2);	// Lock and obtain write access to the easy handle.
// Use *h2_w, which is a reference to the locked CurlEasyRequest instance.
// Note: As it is not allowed to use curl easy handles in any way concurrently,
// read access would at most give access to a CURL const*, which will turn out
// to be completely useless; therefore it is sufficient and efficient to use
// an AIThreadSafeSimple and it's unlikely that AICurlEasyRequest_rat will be used.
typedef AIAccessConst<AICurlPrivate::CurlEasyRequest> AICurlEasyRequest_rat;
typedef AIAccess<AICurlPrivate::CurlEasyRequest> AICurlEasyRequest_wat;

#include "aicurlprivate.h"

// The curl easy request type wrapped in a reference counting pointer.
typedef boost::intrusive_ptr<AICurlPrivate::ThreadSafeCurlEasyRequest> AICurlEasyRequestPtr;

// boost::intrusive_ptr is no more threadsafe than a builtin type, but wrapping it in AIThreadSafe is obviously not going to help here.
// Therefore we use the following trick: we wrap the boost::intrusive_ptr too, and only allow read accesses on it.

// Thread safe, reference counting, auto cleaning curl easy handle.
class AICurlEasyRequest : protected CurlEasyHandleEvents {
  public:
	// Initial construction is allowed (thread-safe).
	// Note: If ThreadSafeCurlEasyRequest() throws then the memory allocated is still freed.
	// 'new' never returned however and the constructor nor destructor of mCurlEasyRequest is called in this case.
	AICurlEasyRequest(bool buffered) throw(AICurlNoEasyHandle) :
	    mCurlEasyRequest(buffered ? new AICurlPrivate::ThreadSafeBufferedCurlEasyRequest : new AICurlPrivate::ThreadSafeCurlEasyRequest) { }
	AICurlEasyRequest(AICurlEasyRequest const& orig) : mCurlEasyRequest(orig.mCurlEasyRequest) { }
	AICurlEasyRequest(AICurlEasyRequestPtr const& ptr) : mCurlEasyRequest(ptr) { }

	// For the rest, only allow read operations.
	AIThreadSafeSimple<AICurlPrivate::CurlEasyRequest>& operator*(void) const { return *mCurlEasyRequest; }
	AIThreadSafeSimple<AICurlPrivate::CurlEasyRequest>* operator->(void) const { return mCurlEasyRequest.get(); }
	AIThreadSafeSimple<AICurlPrivate::CurlEasyRequest>* get(void) const { return mCurlEasyRequest.get(); }

	// It's also OK to automatically convert this object to a const boost::intrusive_ptr.
	operator AICurlEasyRequestPtr const&() const { return mCurlEasyRequest; }

	// Returns true if this object points to the same CurlEasyRequest object.
	bool operator==(AICurlEasyRequest const& cer) const { return mCurlEasyRequest == cer.mCurlEasyRequest; }
	
	// Returns true if this object points to a different CurlEasyRequest object.
	bool operator!=(AICurlEasyRequest const& cer) const { return mCurlEasyRequest != cer.mCurlEasyRequest; }
	
	// Queue this request for insertion in the multi session.
	void addRequest(void);

	// Queue a command to remove this request from the multi session (or cancel a queued command to add it).
	void removeRequest(void);

  private:
	// The actual pointer to the ThreadSafeCurlEasyRequest instance.
	AICurlEasyRequestPtr mCurlEasyRequest;

  protected:
	void get_events(void) { AICurlEasyRequest_wat(*mCurlEasyRequest)->send_events_to(this); }
	void kill_events(void) { AICurlEasyRequest_wat(*mCurlEasyRequest)->send_events_to(NULL); }
	/*virtual*/ void added_to_multi_handle(AICurlEasyRequest_wat&) { Dout(dc::warning, "Unhandled event added_to_multi_handle()"); }
	/*virtual*/ void finished(AICurlEasyRequest_wat&) { Dout(dc::warning, "Unhandled event finished()"); }
	/*virtual*/ void removed_from_multi_handle(AICurlEasyRequest_wat&) { Dout(dc::warning, "Unhandled event removed_from_multi_handle()"); }

  private:
	// Assignment would not be thread-safe; we may create this object and read from it.
	// Note: Destruction is implicitly assumed thread-safe, as it would be a logic error to
	// destruct it while another thread still needs it, concurrent or not.
	AICurlEasyRequest& operator=(AICurlEasyRequest const&) { return *this; }
};

// Write Access Type for the buffer.
struct AICurlResponderBuffer_wat : public AIAccess<AICurlPrivate::CurlResponderBuffer> {
  explicit AICurlResponderBuffer_wat(AICurlPrivate::ThreadSafeBufferedCurlEasyRequest& lockobj) :
	  AIAccess<AICurlPrivate::CurlResponderBuffer>(lockobj) { }
  AICurlResponderBuffer_wat(AIThreadSafeSimple<AICurlPrivate::CurlEasyRequest>& lockobj) :
	  AIAccess<AICurlPrivate::CurlResponderBuffer>(static_cast<AICurlPrivate::ThreadSafeBufferedCurlEasyRequest&>(lockobj)) { }
};

#define AICurlPrivate DONTUSE_AICurlPrivate

#endif
