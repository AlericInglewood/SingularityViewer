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
#include <boost/intrusive_ptr.hpp>
#include <curl/curl.h>		// CURL, CURLM, CURLMcode, CURLoption, curl_*_callback

#include "stdtypes.h"		// U32
#include "lliopipe.h"		// LLIOPipe::buffer_ptr_t
#include "llatomic.h"		// LLAtomicU32

class LLSD;

// Things defined in this namespace are called from elsewhere in the viewer code.
namespace AICurlInterface
{

//-----------------------------------------------------------------------------
// Global functions.

// Called once at start of application (from newview/llappviewer.cpp by main thread (before threads are created)),
// with main purpose to initialize curl.
void initCurl(F32 curl_request_timeout = 120.f, S32 max_number_handles = 256);

// Called once at start of application (from LLAppViewer::initThreads), starts AICurlThread.
void startCurlThread(bool multiple_threads);

// Called once at end of application (from newview/llappviewer.cpp by main thread),
// with purpose to stop curl threads, free curl resources and deinitialize curl.
void cleanupCurl(void);

// Called from indra/llmessage/llproxy.cpp for return values of curl_easy_setopt,
// for debug output purpose when there was an error code.
// Prints some debug output to llinfos if code is not CURLE_OK.
CURLcode check_easy_code(CURLcode code);

// Not called from anywhere.
CURLMcode check_multi_code(CURLMcode code);

// Called from indra/llmessage/llurlrequest.cpp to print debug output regarding
// an error code returned by EasyRequest::getResult.
// Just returns curl_easy_strerror(errorcode).
std::string strerror(CURLcode errorcode);

// Called from indra/newview/llfloaterabout.cpp for the About floater, and
// from newview/llappviewer.cpp in behalf of debug output.
// Just returns curl_version().
std::string getVersionString(void);

// Called newview/llappviewer.cpp from (and llcrashlogger/llcrashlogger.cpp) to set
// the Certificate Authority file used to verify HTTPS certs.
void setCAFile(std::string const& file);

// Not called from anywhere.
// Can be used to set the path to the Certificate Authority file.
void setCAPath(std::string const& file);

// LLHTTPAssetStorage calls these functions, and does a lot of curl calls directly on the returned curl handles.
CURLM* newMultiHandle(void);					// Called from constructor of LLHTTPAssetStorage.
CURLMcode deleteMultiHandle(CURLM* handle);		// Called from destructor of LLHTTPAssetStorage.

// LLHTTPAssetRequest calls these functions, and does a lot of curl calls directly on the returned curl handles.
CURL* newEasyHandle(void);						// Called from LLHTTPAssetRequest::setupCurlHandle.
void deleteEasyHandle(CURL* handle);			// Called from LLHTTPAssetRequest::cleanupCurlHandle.

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

	// A derived class should return true if curl should follow redirections.
	// The default is not to follow redirections.
	virtual bool followRedir(void) { return false; }

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
// destruct when the last of such pointers is destructed.
typedef boost::intrusive_ptr<Responder> ResponderPtr;

// Output parameter of LLCurlEasyRequest::getResult.
// Only used by LLXMLRPCTransaction::Impl.
struct TransferInfo {
  TransferInfo() : mSizeDownload(0.0), mTotalTime(0.0), mSpeedDownload(0.0) { }
  F64 mSizeDownload;
  F64 mTotalTime;
  F64 mSpeedDownload;
};

class Easy {
  public:
	CURL* getCurlHandle(void) const;
};

class EasyRequest {
  public:
	AICurlInterface::Easy* getEasy(void) const;

	void setopt(CURLoption option, S32 value);
	void setoptString(CURLoption option, std::string const& value);
	void setPost(char* postdata, S32 size);
	void setHeaderCallback(curl_write_callback callback, void* userdata);
	void setWriteCallback(curl_write_callback callback, void* userdata);
	void setReadCallback(curl_read_callback callback, void* userdata);
	void setSSLCtxCallback(curl_ssl_ctx_callback callback, void* userdata);
	void slist_append(char const* str);
	void sendRequest(std::string const& url);
	std::string getErrorString(void);
	bool getResult(CURLcode* result, AICurlInterface::TransferInfo* info = NULL);
	bool wait(void) const;
	bool isValid(void) const;
};

class Request {
  public:
	typedef std::vector<std::string> headers_t;
	bool getByteRange(std::string const& url, headers_t const& headers, S32 offset, S32 length, AICurlInterface::ResponderPtr responder);

	S32  process(void);
};

} // namespace AICurlInterface

#endif
