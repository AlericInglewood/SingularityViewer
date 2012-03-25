/**
 * @file llcurl.cpp
 * @author Zero / Donovan
 * @date 2006-10-15
 * @brief Implementation of wrapper around libcurl.
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#if LL_WINDOWS
#define SAFE_SSL 1
#elif LL_DARWIN
#define SAFE_SSL 1
#else
#define SAFE_SSL 1
#endif

#include "linden_common.h"

#include "llcurl.h"

#include <algorithm>
#include <iomanip>
#include <curl/curl.h>
#if SAFE_SSL
#include <openssl/crypto.h>
#endif

#include "llbufferstream.h"
#include "llproxy.h"
#include "llsdserialize.h"
#include "llstl.h"
#include "llthread.h"
#include "lltimer.h"

//////////////////////////////////////////////////////////////////////////////
/*
	The trick to getting curl to do keep-alives is to reuse the
	same easy handle for the requests.  It appears that curl
	keeps a pool of connections alive for each easy handle, but
	doesn't share them between easy handles.  Therefore it is
	important to keep a pool of easy handles and reuse them,
	rather than create and destroy them with each request.  This
	code does this.

	Furthermore, it would behoove us to keep track of which
	hosts an easy handle was used for and pick an easy handle
	that matches the next request.  This code does not current
	do this.
 */

//////////////////////////////////////////////////////////////////////////////

static const U32 EASY_HANDLE_POOL_SIZE		= 5;
static const S32 MULTI_PERFORM_CALL_REPEAT	= 5;
static const S32 CURL_REQUEST_TIMEOUT = 30; // seconds per operation
static const S32 MAX_ACTIVE_REQUEST_COUNT = 100;

// DEBUG //
LLAtomicS32 gCurlEasyCount(0);
LLAtomicS32 gCurlMultiCount(0);

//////////////////////////////////////////////////////////////////////////////

//static
std::vector<LLMutex*> LLCurl::sSSLMutex;
AIThreadSafeSimpleDC<LLCurl::sCA_type> LLCurl::sCA;
AIThreadSafeSimpleDC<LLCurlThread*> LLCurl::sCurlThread;	// Global, so initialized with NULL.
AIThreadSafeSimpleDC<S32> LLCurl::sTotalHandles;			// Global, so initialized with 0.
bool     LLCurl::sNotQuitting = true;
F32      LLCurl::sCurlRequestTimeOut = 120.f; //seconds
S32      LLCurl::sMaxHandles = 256; //max number of handles, (multi handles and easy handles combined).

static void check_curl_code(CURLcode code)
{
	if (code != CURLE_OK)
	{
		// linux appears to throw a curl error once per session for a bad initialization
		// at a pretty random time (when enabling cookies).
		llinfos << "curl error detected: " << curl_easy_strerror(code) << llendl;
	}
}

static void check_curl_multi_code(CURLMcode code) 
{
	if (code != CURLM_OK)
	{
		// linux appears to throw a curl error once per session for a bad initialization
		// at a pretty random time (when enabling cookies).
		llinfos << "curl multi error detected: " << curl_multi_strerror(code) << llendl;
	}
}

//static
void LLCurl::setCAPath(const std::string& path)
{
	AIAccess<LLCurl::sCA_type>(sCA)->path = path;
}

//static
void LLCurl::setCAFile(const std::string& file)
{
	AIAccess<LLCurl::sCA_type>(sCA)->file = file;
}

//static
std::string LLCurl::getVersionString()
{
	return std::string(curl_version());
}

//////////////////////////////////////////////////////////////////////////////

LLCurl::Responder::Responder()
	: mReferenceCount(0)
{
  DoutEntering(dc::curl, "LLCurl::Responder() with this = " << (void*)this);
  BACKTRACE;
}

LLCurl::Responder::~Responder()
{
  DoutEntering(dc::curl, "LLCurl::Responder::~Responder() with this = " << (void*)this << "; mReferenceCount = " << mReferenceCount);
  BACKTRACE;
}

// virtual
void LLCurl::Responder::errorWithContent(
	U32 status,
	const std::string& reason,
	const LLSD&)
{
	error(status, reason);
}

// virtual
void LLCurl::Responder::error(U32 status, const std::string& reason)
{
	llinfos << mURL << " [" << status << "]: " << reason << llendl;
}

// virtual
void LLCurl::Responder::result(const LLSD& content)
{
}

void LLCurl::Responder::setURL(const std::string& url)
{
	mURL = url;
}

// virtual
void LLCurl::Responder::completedRaw(
	U32 status,
	const std::string& reason,
	const LLChannelDescriptors& channels,
	const LLIOPipe::buffer_ptr_t& buffer)
{
	LLSD content;
	LLBufferStream istr(channels, buffer.get());
	if (!LLSDSerialize::fromXML(content, istr))
	{
		llinfos << "Failed to deserialize LLSD. " << mURL << " [" << status << "]: " << reason << llendl;
	}

	completed(status, reason, content);
}

// virtual
void LLCurl::Responder::completed(U32 status, const std::string& reason, const LLSD& content)
{
	if (isGoodStatus(status))
	{
		result(content);
	}
	else
	{
		errorWithContent(status, reason, content);
	}
}

//virtual
void LLCurl::Responder::completedHeader(U32 status, const std::string& reason, const LLSD& content)
{

}

namespace boost
{
	void intrusive_ptr_add_ref(LLCurl::Responder* p)
	{
		++p->mReferenceCount;
	}
	
	void intrusive_ptr_release(LLCurl::Responder* p)
	{
		if (p && 0 == --p->mReferenceCount)
		{
			delete p;
		}
	}
};


//////////////////////////////////////////////////////////////////////////////

AIThreadSafeSimpleDC<LLCurl::Easy::Handles> LLCurl::Easy::sHandles;

//static
CURL* LLCurl::Easy::allocEasyHandle()
{
	llassert(*AIAccess<LLCurlThread*>(LLCurl::getCurlThread())) ;

	CURL* ret = NULL;

	//*** Multi-threaded.
	AIAccess<Handles> handles_w(sHandles);

	if (handles_w->free.empty())
	{
		ret = LLCurl::newEasyHandle();
	}
	else
	{
		ret = *(handles_w->free.begin());
		handles_w->free.erase(ret);
	}

	if (ret)
	{
		handles_w->active.insert(ret);
	}

	return ret;
}

//static
void LLCurl::Easy::releaseEasyHandle(CURL* handle)
{
	DoutEntering(dc::curl, "LLCurl::Easy::releaseEasyHandle(" << (void*)handle << ")");
	BACKTRACE;

	static const S32 MAX_NUM_FREE_HANDLES = 32 ;

	if (!handle)
	{
		return ; //handle allocation failed.
		//llerrs << "handle cannot be NULL!" << llendl;
	}

	//*** Multi-Threaded (logout only?)
	AIAccess<Handles> handles_w(sHandles);

	if (handles_w->active.find(handle) != handles_w->active.end())
	{
		handles_w->active.erase(handle);

		if (handles_w->free.size() < MAX_NUM_FREE_HANDLES)
		{
			curl_easy_reset(handle);
			handles_w->free.insert(handle);
		}
		else
		{
			LLCurl::deleteEasyHandle(handle) ;
		}
	}
	else
	{
		llerrs << "Invalid handle." << llendl;
	}
}

LLCurl::Easy::Easy()
	: mHeaders((curl_slist*)NULL),
	  mCurlEasyHandle((CURL*)NULL)
{
	mErrorBuffer[0] = 0;
}

LLCurl::Easy* LLCurl::Easy::getEasy()
{
	Easy* easy = new Easy();
	easy->mCurlEasyHandle = allocEasyHandle(); 
	
	if (!easy->mCurlEasyHandle)
	{
		// this can happen if we have too many open files (fails in c-ares/ares_init.c)
		llwarns << "allocEasyHandle() returned NULL! Easy handles: " << gCurlEasyCount << " Multi handles: " << gCurlMultiCount << llendl;
		delete easy;
		return NULL;
	}
	
	// set no DNS caching as default for all easy handles. This prevents them adopting a
	// multi handles cache if they are added to one.
	CURLcode result = curl_easy_setopt(easy->mCurlEasyHandle, CURLOPT_DNS_CACHE_TIMEOUT, 0);
	check_curl_code(result);
	
	gCurlEasyCount++;
	return easy;
}

LLCurl::Easy::~Easy()
{
	releaseEasyHandle(mCurlEasyHandle);
	--gCurlEasyCount;
	curl_slist_free_all(mHeaders);

	{
		AISTAccess<std::vector<char*> > strings_w(mStrings);
		for_each(strings_w->begin(), strings_w->end(), DeletePointerArray());
	}

	AISTAccess<LLCurl::ResponderPtr> responder_w(mResponder);
	if (*responder_w && LLCurl::getNotQuitting()) //aborted
	{	
		std::string reason("Request timeout, aborted.") ;
		(*responder_w)->completedRaw(408, //HTTP_REQUEST_TIME_OUT, timeout, abort
			reason, mChannels, mOutput);		
	}
	*responder_w = NULL;
}

void LLCurl::Easy::resetState()
{
 	curl_easy_reset(mCurlEasyHandle);

	if (mHeaders)
	{
		curl_slist_free_all(mHeaders);
		mHeaders = NULL;
	}

	//mRequest.str("");
	//mRequest.clear();

	AISTAccess<LLIOPipe::buffer_ptr_t>(mOutput)->reset();
	
	{
		AISTAccess<std::stringstream> input_w(mInput);
		input_w->str("");
		input_w->clear();
	}
	
	mErrorBuffer[0] = 0;
	
	{
		AISTAccess<std::stringstream> header_output_w(mHeaderOutput);
		header_output_w->str("");
		header_output_w->clear();
	}
}

void LLCurl::Easy::setErrorBuffer()
{
	setopt(CURLOPT_ERRORBUFFER, &mErrorBuffer);
}

const char* LLCurl::Easy::getErrorBuffer()
{
	return mErrorBuffer;
}

void LLCurl::Easy::setCA()
{
	AIAccess<LLCurl::sCA_type> sCA_w(sCA);
	if (!sCA_w->path.empty())
	{
		setoptString(CURLOPT_CAPATH, sCA_w->path);
	}
	if (!sCA_w->file.empty())
	{
		setoptString(CURLOPT_CAINFO, sCA_w->file);
	}
}

void LLCurl::Easy::setHeaders()
{
	setopt(CURLOPT_HTTPHEADER, mHeaders);
}

void LLCurl::Easy::getTransferInfo(LLCurl::TransferInfo* info)
{
	check_curl_code(curl_easy_getinfo(mCurlEasyHandle, CURLINFO_SIZE_DOWNLOAD, &info->mSizeDownload));
	check_curl_code(curl_easy_getinfo(mCurlEasyHandle, CURLINFO_TOTAL_TIME, &info->mTotalTime));
	check_curl_code(curl_easy_getinfo(mCurlEasyHandle, CURLINFO_SPEED_DOWNLOAD, &info->mSpeedDownload));
}

U32 LLCurl::Easy::report(CURLcode code)
{
	U32 responseCode = 0;	
	std::string responseReason;
	
	if (code == CURLE_OK)
	{
		check_curl_code(curl_easy_getinfo(mCurlEasyHandle, CURLINFO_RESPONSE_CODE, &responseCode));
		//*TODO: get reason from first line of mHeaderOutput
	}
	else
	{
		responseCode = 499;
		responseReason = strerror(code) + " : " + mErrorBuffer;
		setopt(CURLOPT_FRESH_CONNECT, TRUE);
	}

	{
		AISTAccess<LLCurl::ResponderPtr> responder_w(mResponder);
		LLCurl::ResponderPtr& responder = *responder_w;
		if (responder)
		{	
			responder->completedRaw(responseCode, responseReason, mChannels, mOutput);
			responder = NULL;
		}
	}

	resetState();
	return responseCode;
}

// Note: these all assume the caller tracks the value (i.e. keeps it persistant)
void LLCurl::Easy::setopt(CURLoption option, S32 value)
{
	CURLcode result = curl_easy_setopt(mCurlEasyHandle, option, value);
	check_curl_code(result);
}

void LLCurl::Easy::setopt(CURLoption option, void* value)
{
	CURLcode result = curl_easy_setopt(mCurlEasyHandle, option, value);
	check_curl_code(result);
}

void LLCurl::Easy::setopt(CURLoption option, char* value)
{
	CURLcode result = curl_easy_setopt(mCurlEasyHandle, option, value);
	check_curl_code(result);
}

// Note: this copies the string so that the caller does not have to keep it around
void LLCurl::Easy::setoptString(CURLoption option, const std::string& value)
{
	char* tstring = new char[value.length()+1];
	strcpy(tstring, value.c_str());
	AISTAccess<std::vector<char*> >(mStrings)->push_back(tstring);
	CURLcode result = curl_easy_setopt(mCurlEasyHandle, option, tstring);
	check_curl_code(result);
}

void LLCurl::Easy::slist_append(const char* str)
{
	mHeaders = curl_slist_append(mHeaders, str);
}

static size_t curlReadCallback(char* data, size_t size, size_t nmemb, void* user_data)
{
	LLCurl::Easy* easy = (LLCurl::Easy*)user_data;
	
	S32 n = size * nmemb;
	S32 startpos = easy->getInput().tellg();
	easy->getInput().seekg(0, std::ios::end);
	S32 endpos = easy->getInput().tellg();
	easy->getInput().seekg(startpos, std::ios::beg);
	S32 maxn = endpos - startpos;
	n = llmin(n, maxn);
	easy->getInput().read((char*)data, n);

	return n;
}

static size_t curlWriteCallback(char* data, size_t size, size_t nmemb, void* user_data)
{
	LLCurl::Easy* easy = (LLCurl::Easy*)user_data;
	
	S32 n = size * nmemb;
	easy->getOutput()->append(easy->getChannels().in(), (const U8*)data, n);

	return n;
}

static size_t curlHeaderCallback(void* data, size_t size, size_t nmemb, void* user_data)
{
	LLCurl::Easy* easy = (LLCurl::Easy*)user_data;
	
	size_t n = size * nmemb;
	easy->getHeaderOutput().write((const char*)data, n);

	return n;
}

void LLCurl::Easy::prepRequest(const std::string& url,
							   const std::vector<std::string>& headers,
							   ResponderPtr responder, S32 time_out, bool post)
{
	resetState();
	
	if (post) setoptString(CURLOPT_ENCODING, "");

	//setopt(CURLOPT_VERBOSE, 1); // useful for debugging
	setopt(CURLOPT_NOSIGNAL, 1);

	// Set the CURL options for either Socks or HTTP proxy
	LLProxy::getInstance()->applyProxySettings(this);

	{
		AISTAccess<LLIOPipe::buffer_ptr_t> output_w(mOutput);
		LLIOPipe::buffer_ptr_t& output(*output_w);
		output.reset(new LLBufferArray);
		output->setThreaded(true);
	}
	setopt(CURLOPT_WRITEFUNCTION, (void*)&curlWriteCallback);
	setopt(CURLOPT_WRITEDATA, (void*)this);

	setopt(CURLOPT_READFUNCTION, (void*)&curlReadCallback);
	setopt(CURLOPT_READDATA, (void*)this);
	
	setopt(CURLOPT_HEADERFUNCTION, (void*)&curlHeaderCallback);
	setopt(CURLOPT_HEADERDATA, (void*)this);

	// Allow up to five redirects
	if (responder && responder->followRedir())
	{
		setopt(CURLOPT_FOLLOWLOCATION, 1);
		setopt(CURLOPT_MAXREDIRS, MAX_REDIRECTS);
	}

	setErrorBuffer();
	setCA();

	setopt(CURLOPT_SSL_VERIFYPEER, true);
	
	//don't verify host name so urls with scrubbed host names will work (improves DNS performance)
	setopt(CURLOPT_SSL_VERIFYHOST, 0);
	setopt(CURLOPT_TIMEOUT, llmax(time_out, CURL_REQUEST_TIMEOUT));

	setoptString(CURLOPT_URL, url);

	*AISTAccess<LLCurl::ResponderPtr>(mResponder) = responder;

	if (!post)
	{
		slist_append("Connection: keep-alive");
		slist_append("Keep-alive: 300");
		// Accept and other headers
		for (std::vector<std::string>::const_iterator iter = headers.begin();
			 iter != headers.end(); ++iter)
		{
			slist_append((*iter).c_str());
		}
	}
}

////////////////////////////////////////////////////////////////////////////
LLCurl::Multi::Multi(F32 idle_time_out)
	: mQueued(0),
	  mErrorCount(0),
	  mState(STATE_READY),
	  mMutexp(NULL),
	  mEasyMutexp(NULL)
{
	*AIAccess<bool>(mDead) = false;
	mCurlMultiHandle = LLCurl::newMultiHandle();
	if (!mCurlMultiHandle)
	{
		llwarns << "curl_multi_init() returned NULL! Easy handles: " << gCurlEasyCount << " Multi handles: " << gCurlMultiCount << llendl;
		mCurlMultiHandle = LLCurl::newMultiHandle();
	}
	
	//llassert_always(mCurlMultiHandle);	
	
	if(mCurlMultiHandle)
	{
		AIAccess<LLCurlThread*> curl_thread_w(LLCurl::getCurlThread());
		if ((*curl_thread_w)->getThreaded())
		{
			mMutexp = new LLMutex;
			mEasyMutexp = new LLMutex;
		}
		(*curl_thread_w)->addMulti(this);

		mIdleTimeOut = idle_time_out ;
		if(mIdleTimeOut < LLCurl::getCurlRequestTimeOut())
		{
			mIdleTimeOut = LLCurl::getCurlRequestTimeOut();
		}

		gCurlMultiCount++;
	}
}

LLCurl::Multi::~Multi()
{
	cleanup() ;	
}

void LLCurl::Multi::cleanup()
{
	if(!mCurlMultiHandle)
	{
		return ; //nothing to clean.
	}

	// Clean up active
	AISTAccess<easy_active_list_t> easy_active_list_w(mEasyActiveList);
	for(easy_active_list_t::iterator iter = easy_active_list_w->begin(); iter != easy_active_list_w->end(); ++iter)
	{
		Easy* easy = *iter;
		check_curl_multi_code(curl_multi_remove_handle(mCurlMultiHandle, easy->getCurlHandle()));
		delete easy;
	}
	easy_active_list_w->clear();

	AISTAccess<easy_active_map_t> easy_active_map_w(mEasyActiveMap);
	easy_active_map_w->clear();
	
	// Clean up freed
	AISTAccess<easy_free_list_t> easy_free_list_w(mEasyFreeList);
	for_each(easy_free_list_w->begin(), easy_free_list_w->end(), DeletePointer());
	easy_free_list_w->clear();

	AISTAccess<CURLM*> curl_multi_handle_w(mCurlMultiHandle);
	check_curl_multi_code(LLCurl::deleteMultiHandle(*curl_multi_handle_w));
	*curl_multi_handle_w = NULL ;

	delete mMutexp ;
	mMutexp = NULL ;
	delete mEasyMutexp ;
	mEasyMutexp = NULL ;
	
	*AISTAccess<S32>(mQueued) = 0;
	*AIAccess<ePerformState>(mState) = STATE_COMPLETED;
	
	--gCurlMultiCount;

	return ;
}

void LLCurl::Multi::lock()
{
	if(mMutexp)
	{
		mMutexp->lock() ;
	}
}

void LLCurl::Multi::unlock()
{
	if(mMutexp)
	{
		mMutexp->unlock() ;
	}
}

void LLCurl::Multi::markDead()
{
	//*** Multi-threaded.
	AIAccess<bool> dead_w(mDead);
	*dead_w = true;
	(*AIAccess<LLCurlThread*>(LLCurl::getCurlThread()))->setPriority(mHandle, LLQueuedThread::PRIORITY_URGENT);
}

void LLCurl::Multi::setState(LLCurl::Multi::ePerformState state)
{
	*AIAccess<ePerformState>(mState) = state ;

	if (state == STATE_READY)
	{
		(*AIAccess<LLCurlThread*>(LLCurl::getCurlThread()))->setPriority(mHandle, LLQueuedThread::PRIORITY_NORMAL);
	}	
}

LLCurl::Multi::ePerformState LLCurl::Multi::getState()
{
	return *AIAccess<ePerformState>(mState);
}
	
bool LLCurl::Multi::isCompleted() 
{
	return STATE_COMPLETED == getState() ;
}

bool LLCurl::Multi::waitToComplete()
{
	if(!isValid())
	{
		return true ;
	}

	if(!mMutexp) //not threaded
	{
		doPerform() ;
		return true ;
	}

	bool completed = (STATE_COMPLETED == *AIAccess<ePerformState>(mState)) ;
	if(!completed)
	{
		(*AIAccess<LLCurlThread*>(LLCurl::getCurlThread()))->setPriority(mHandle, LLQueuedThread::PRIORITY_HIGH);
	}
	
	return completed;
}

CURLMsg* LLCurl::Multi::info_read(S32* msgs_in_queue)
{
	//*** Multi-threaded.
	LLMutexLock lock(mMutexp) ;

	CURLMsg* curlmsg = curl_multi_info_read(mCurlMultiHandle, msgs_in_queue);
	return curlmsg;
}

//return true if dead
bool LLCurl::Multi::doPerform()
{
	//*** Multi-threaded.
	bool dead = *AIAccess<bool>(mDead);

	if (dead)
	{
		setState(STATE_COMPLETED);
		mQueued = 0 ;
	}
	else if(getState() != STATE_COMPLETED)
	{		
		setState(STATE_PERFORMING);

		S32 q = 0;
		for (S32 call_count = 0;
				call_count < MULTI_PERFORM_CALL_REPEAT;
				call_count++)
		{
			LLMutexLock lock(mMutexp) ;

			//WARNING: curl_multi_perform will block for many hundreds of milliseconds
			// NEVER call this from the main thread, and NEVER allow the main thread to 
			// wait on a mutex held by this thread while curl_multi_perform is executing
			CURLMcode code = curl_multi_perform(mCurlMultiHandle, &q);
			if (CURLM_CALL_MULTI_PERFORM != code || q == 0)
			{
				check_curl_multi_code(code);
			
				break;
			}
		}

		mQueued = q;	
		setState(STATE_COMPLETED) ;		
		AISTAccess<LLFrameTimer>(mIdleTimer)->reset();
	}
	else if (AISTAccess<LLFrameTimer>(mIdleTimer)->getElapsedTimeF32() > mIdleTimeOut) //idle for too long, remove it.
	{
		dead = true ;
	}

	return dead ;
}

S32 LLCurl::Multi::process()
{
	//*** Multi-threaded.
	if(!isValid())
	{
		return 0 ;
	}

	waitToComplete() ;

	if (getState() != STATE_COMPLETED)
	{
		return 0;
	}

	CURLMsg* msg;
	int msgs_in_queue;

	S32 processed = 0;
	while ((msg = info_read(&msgs_in_queue)))
	{
		++processed;
		if (msg->msg == CURLMSG_DONE)
		{
			U32 response = 0;
			Easy* easy = NULL ;

			{
				LLMutexLock lock(mEasyMutexp) ;
				AISTAccess<easy_active_map_t> easy_active_map_w(mEasyActiveMap);
				easy_active_map_t::iterator iter = easy_active_map_w->find(msg->easy_handle);
				if (iter != easy_active_map_w->end())
				{
					easy = iter->second;
				}
			}

			if(easy)
			{
				response = easy->report(msg->data.result);
				removeEasy(easy);
			}
			else
			{
				response = 499;
				//*TODO: change to llwarns
				llerrs << "cleaned up curl request completed!" << llendl;
			}
			if (response >= 400)
			{
				// failure of some sort, inc mErrorCount for debugging and flagging multi for destruction
				++mErrorCount;
			}
		}
	}

	setState(STATE_READY);

	return processed;
}

LLCurl::Easy* LLCurl::Multi::allocEasy()
{
	//*** Multi-threaded.
	Easy* easy = 0;	

	if (AISTAccess<easy_free_list_t>(mEasyFreeList)->empty())
	{		
		easy = Easy::getEasy();
	}
	else
	{
		LLMutexLock lock(mEasyMutexp) ;
		easy = *(AISTAccess<easy_free_list_t>(mEasyFreeList)->begin());
		AISTAccess<easy_free_list_t>(mEasyFreeList)->erase(easy);
	}
	if (easy)
	{
		LLMutexLock lock(mEasyMutexp) ;
		AISTAccess<easy_active_list_t>(mEasyActiveList)->insert(easy);
		(*AISTAccess<easy_active_map_t>(mEasyActiveMap))[easy->getCurlHandle()] = easy;
	}
	return easy;
}

bool LLCurl::Multi::addEasy(Easy* easy)
{
	//*** Multi-threaded.
	LLMutexLock lock(mMutexp) ;
	CURLMcode mcode = curl_multi_add_handle(mCurlMultiHandle, easy->getCurlHandle());
	check_curl_multi_code(mcode);
	//if (mcode != CURLM_OK)
	//{
	//	llwarns << "Curl Error: " << curl_multi_strerror(mcode) << llendl;
	//	return false;
	//}
	return true;
}

void LLCurl::Multi::easyFree(Easy* easy)
{
	//*** Multi-threaded.
	if(mEasyMutexp)
	{
		mEasyMutexp->lock() ;
	}

	AISTAccess<easy_active_list_t>(mEasyActiveList)->erase(easy);
	AISTAccess<easy_active_map_t>(mEasyActiveMap)->erase(easy->getCurlHandle());

	if (AISTAccess<easy_free_list_t>(mEasyFreeList)->size() < EASY_HANDLE_POOL_SIZE)
	{		
		AISTAccess<easy_free_list_t>(mEasyFreeList)->insert(easy);
		
		if(mEasyMutexp)
		{
			mEasyMutexp->unlock() ;
		}

		easy->resetState();
	}
	else
	{
		if(mEasyMutexp)
		{
			mEasyMutexp->unlock() ;
		}
		delete easy;
	}
}

void LLCurl::Multi::removeEasy(Easy* easy)
{
	//*** Multi-threaded.
	{
		LLMutexLock lock(mMutexp) ;
		check_curl_multi_code(curl_multi_remove_handle(mCurlMultiHandle, easy->getCurlHandle()));
	}
	easyFree(easy);
}

LLAtomicS32 total_curl_requests;

//------------------------------------------------------------
//LLCurlThread
LLCurlThread::CurlRequest::CurlRequest(handle_t handle, LLCurl::Multi* multi, LLCurlThread* curl_thread) :
	LLQueuedThread::QueuedRequest(handle, LLQueuedThread::PRIORITY_NORMAL, FLAG_AUTO_COMPLETE),
	mMulti(multi),
	mCurlThread(curl_thread)
{	
	total_curl_requests++;
	Dout(dc::curl, "Created CurlRequest for handle " << handle << "; total number of CurlRequest objects now " << total_curl_requests);
}

LLCurlThread::CurlRequest::~CurlRequest()
{	
	--total_curl_requests;
	if(mMulti)
	{
		mCurlThread->deleteMulti(mMulti) ;
		mMulti = NULL ;
	}
	Dout(dc::curl, "Deleted CurlRequest for handle " << getHashKey() << "; total number of CurlRequest objects now " << total_curl_requests);
}

bool LLCurlThread::CurlRequest::processRequest()
{
	bool completed = true ;
	if(mMulti)
	{
		completed = mCurlThread->doMultiPerform(mMulti) ;

		if(!completed)
		{
			setPriority(LLQueuedThread::PRIORITY_LOW) ;
		}
	}

	return completed ;
}

void LLCurlThread::CurlRequest::finishRequest(bool completed)
{
	// The lock on LLCurl::Multi::mDead must be released before we delete mMulti. So just make a copy.
	bool dead = *AIAccessConst<bool>(mMulti->isDead());
	// The lock is now released again.
	if (dead)
	{
		mCurlThread->deleteMulti(mMulti) ;
	}
	else
	{
		mCurlThread->cleanupMulti(mMulti) ; //being idle too long, remove the request.
	}

	mMulti = NULL ;
}

LLCurlThread::LLCurlThread(bool threaded) :
	LLQueuedThread("curlthread", threaded)
{
}
	
//virtual 
LLCurlThread::~LLCurlThread() 
{
}

S32 LLCurlThread::update(F32 max_time_ms)
{	
	return LLQueuedThread::update(max_time_ms);
}

void LLCurlThread::addMulti(LLCurl::Multi* multi)
{
	DoutEntering(dc::curl, "LLCurlThread::addMulti(" << (void*)multi << ")");
	multi->mHandle = generateHandle() ;
	Dout(dc::curl, "Generated handle " << multi->mHandle);

	CurlRequest* req = new CurlRequest(multi->mHandle, multi, this) ;

	if (!addRequest(req))
	{
		llwarns << "curl request added when the thread is quitted" << llendl;
		req->deleteRequest();
	}
}
	
void LLCurlThread::killMulti(LLCurl::Multi* multi)
{
	if(!multi)
	{
		return ;
	}

	if(multi->isValid())
	{
		multi->markDead() ;
	}
	else
	{
		deleteMulti(multi) ;
	}
}

//private
bool LLCurlThread::doMultiPerform(LLCurl::Multi* multi) 
{
	return multi->doPerform() ;
}

//private
void LLCurlThread::deleteMulti(LLCurl::Multi* multi) 
{
	delete multi ;
}

//private
void LLCurlThread::cleanupMulti(LLCurl::Multi* multi) 
{
	multi->cleanup() ;
}

//------------------------------------------------------------

//static
std::string LLCurl::strerror(CURLcode errorcode)
{
	return std::string(curl_easy_strerror(errorcode));
}

////////////////////////////////////////////////////////////////////////////
// For generating a simple request for data
// using one multi and one easy per request 

LLCurlRequest::LLCurlRequest() :
	mActiveMulti(NULL),
	mActiveRequestCount(0)
{
	mProcessing = FALSE;
}

LLCurlRequest::~LLCurlRequest()
{
	//stop all Multi handle background threads
	AIAccess<LLCurlThread*> curl_thread_w(LLCurl::getCurlThread());
	for (curlmulti_set_t::iterator iter = mMultiSet.begin(); iter != mMultiSet.end(); ++iter)
	{
		(*curl_thread_w)->killMulti(*iter);
	}
	mMultiSet.clear() ;
}

void LLCurlRequest::addMulti()
{
	LLCurl::Multi* multi = new LLCurl::Multi();
	if(!multi->isValid())
	{
		(*AIAccess<LLCurlThread*>(LLCurl::getCurlThread()))->killMulti(multi) ;
		mActiveMulti = NULL ;
		mActiveRequestCount = 0 ;
		return;
	}

	mMultiSet.insert(multi);
	mActiveMulti = multi;
	mActiveRequestCount = 0;
}

LLCurl::Easy* LLCurlRequest::allocEasy()
{
	if (!mActiveMulti ||
		mActiveRequestCount	>= MAX_ACTIVE_REQUEST_COUNT ||
		mActiveMulti->mErrorCount > 0)
	{
		addMulti();
	}
	if(!mActiveMulti)
	{
		return NULL ;
	}

	//llassert_always(mActiveMulti);
	++mActiveRequestCount;
	LLCurl::Easy* easy = mActiveMulti->allocEasy();
	return easy;
}

bool LLCurlRequest::addEasy(LLCurl::Easy* easy)
{
	llassert_always(mActiveMulti);
	
	if (mProcessing)
	{
		llerrs << "Posting to a LLCurlRequest instance from within a responder is not allowed (causes DNS timeouts)." << llendl;
	}
	bool res = mActiveMulti->addEasy(easy);
	return res;
}

void LLCurlRequest::get(const std::string& url, LLCurl::ResponderPtr responder)
{
	getByteRange(url, headers_t(), 0, -1, responder);
}
	
bool LLCurlRequest::getByteRange(const std::string& url,
								 const headers_t& headers,
								 S32 offset, S32 length,
								 LLCurl::ResponderPtr responder)
{
	LLCurl::Easy* easy = allocEasy();
	if (!easy)
	{
		return false;
	}
	easy->prepRequest(url, headers, responder);
	easy->setopt(CURLOPT_HTTPGET, 1);
	if (length > 0)
	{
		std::string range = llformat("Range: bytes=%d-%d", offset,offset+length-1);
		easy->slist_append(range.c_str());
	}
	easy->setHeaders();
	bool res = addEasy(easy);
	return res;
}

bool LLCurlRequest::post(const std::string& url,
						 const headers_t& headers,
						 const LLSD& data,
						 LLCurl::ResponderPtr responder, S32 time_out)
{
	LLCurl::Easy* easy = allocEasy();
	if (!easy)
	{
		return false;
	}
	easy->prepRequest(url, headers, responder, time_out);

	LLSDSerialize::toXML(data, easy->getInput());
	S32 bytes = easy->getInput().str().length();
	
	easy->setopt(CURLOPT_POST, 1);
	easy->setopt(CURLOPT_POSTFIELDS, (void*)NULL);
	easy->setopt(CURLOPT_POSTFIELDSIZE, bytes);

	easy->slist_append("Content-Type: application/llsd+xml");
	easy->setHeaders();

	lldebugs << "POSTING: " << bytes << " bytes." << llendl;
	bool res = addEasy(easy);
	return res;
}

bool LLCurlRequest::post(const std::string& url,
						 const headers_t& headers,
						 const std::string& data,
						 LLCurl::ResponderPtr responder, S32 time_out)
{
	LLCurl::Easy* easy = allocEasy();
	if (!easy)
	{
		return false;
	}
	easy->prepRequest(url, headers, responder, time_out);

	easy->getInput().write(data.data(), data.size());
	S32 bytes = easy->getInput().str().length();
	
	easy->setopt(CURLOPT_POST, 1);
	easy->setopt(CURLOPT_POSTFIELDS, (void*)NULL);
	easy->setopt(CURLOPT_POSTFIELDSIZE, bytes);

	easy->slist_append("Content-Type: application/octet-stream");
	easy->setHeaders();

	lldebugs << "POSTING: " << bytes << " bytes." << llendl;
	bool res = addEasy(easy);
	return res;
}

// Note: call once per frame
S32 LLCurlRequest::process()
{
	S32 res = 0;

	mProcessing = TRUE;
	for (curlmulti_set_t::iterator iter = mMultiSet.begin();
		 iter != mMultiSet.end(); )
	{
		curlmulti_set_t::iterator curiter = iter++;
		LLCurl::Multi* multi = *curiter;

		if(!multi->isValid())
		{
			if(multi == mActiveMulti)
			{				
				mActiveMulti = NULL ;
				mActiveRequestCount = 0 ;
			}
			mMultiSet.erase(curiter) ;
			(*AIAccess<LLCurlThread*>(LLCurl::getCurlThread()))->killMulti(multi) ;
			continue ;
		}

		S32 tres = multi->process();
		res += tres;
		if (multi != mActiveMulti && tres == 0 && multi->mQueued == 0)
		{
			mMultiSet.erase(curiter);
			(*AIAccess<LLCurlThread*>(LLCurl::getCurlThread()))->killMulti(multi);
		}
	}
	mProcessing = FALSE;
	return res;
}

S32 LLCurlRequest::getQueued()
{
	S32 queued = 0;
	for (curlmulti_set_t::iterator iter = mMultiSet.begin();
		 iter != mMultiSet.end(); )
	{
		curlmulti_set_t::iterator curiter = iter++;
		LLCurl::Multi* multi = *curiter;
		
		if(!multi->isValid())
		{
			if(multi == mActiveMulti)
			{				
				mActiveMulti = NULL ;
				mActiveRequestCount = 0 ;
			}
			(*AIAccess<LLCurlThread*>(LLCurl::getCurlThread()))->killMulti(multi);
			mMultiSet.erase(curiter) ;
			continue ;
		}

		queued += multi->mQueued;
		if (multi->getState() != LLCurl::Multi::STATE_READY)
		{
			++queued;
		}
	}
	return queued;
}

////////////////////////////////////////////////////////////////////////////
// For generating one easy request
// associated with a single multi request

LLCurlEasyRequest::LLCurlEasyRequest()
	: mRequestSent(false),
	  mResultReturned(false)
{
	mMulti = new LLCurl::Multi();
	
	if(mMulti->isValid())
	{
		mEasy = mMulti->allocEasy();
		if (mEasy)
		{
			mEasy->setErrorBuffer();
			mEasy->setCA();
			// Set proxy settings if configured to do so.
			LLProxy::getInstance()->applyProxySettings(mEasy);
		}
	}
	else
	{
		(*AIAccess<LLCurlThread*>(LLCurl::getCurlThread()))->killMulti(mMulti) ;
		mEasy = NULL ;
		mMulti = NULL ;
	}
}

LLCurlEasyRequest::~LLCurlEasyRequest()
{
	(*AIAccess<LLCurlThread*>(LLCurl::getCurlThread()))->killMulti(mMulti) ;
}
	
void LLCurlEasyRequest::setopt(CURLoption option, S32 value)
{
	if (isValid() && mEasy)
	{
		mEasy->setopt(option, value);
	}
}

void LLCurlEasyRequest::setoptString(CURLoption option, const std::string& value)
{
	if (isValid() && mEasy)
	{
		mEasy->setoptString(option, value);
	}
}

void LLCurlEasyRequest::setPost(char* postdata, S32 size)
{
	if (isValid() && mEasy)
	{
		mEasy->setopt(CURLOPT_POST, 1);
		mEasy->setopt(CURLOPT_POSTFIELDS, postdata);
		mEasy->setopt(CURLOPT_POSTFIELDSIZE, size);
	}
}

void LLCurlEasyRequest::setHeaderCallback(curl_write_callback callback, void* userdata)
{
	if (isValid() && mEasy)
	{
		mEasy->setopt(CURLOPT_HEADERFUNCTION, (void*)callback);
		mEasy->setopt(CURLOPT_HEADERDATA, userdata); // aka CURLOPT_WRITEHEADER
	}
}

void LLCurlEasyRequest::setWriteCallback(curl_write_callback callback, void* userdata)
{
	if (isValid() && mEasy)
	{
		mEasy->setopt(CURLOPT_WRITEFUNCTION, (void*)callback);
		mEasy->setopt(CURLOPT_WRITEDATA, userdata);
	}
}

void LLCurlEasyRequest::setReadCallback(curl_read_callback callback, void* userdata)
{
	if (isValid() && mEasy)
	{
		mEasy->setopt(CURLOPT_READFUNCTION, (void*)callback);
		mEasy->setopt(CURLOPT_READDATA, userdata);
	}
}

void LLCurlEasyRequest::setSSLCtxCallback(curl_ssl_ctx_callback callback, void* userdata)
{
	if (isValid() && mEasy)
	{
		mEasy->setopt(CURLOPT_SSL_CTX_FUNCTION, (void*)callback);
		mEasy->setopt(CURLOPT_SSL_CTX_DATA, userdata);
	}
}

void LLCurlEasyRequest::slist_append(const char* str)
{
	if (isValid() && mEasy)
	{
		mEasy->slist_append(str);
	}
}

void LLCurlEasyRequest::sendRequest(const std::string& url)
{
	llassert_always(!mRequestSent);
	mRequestSent = true;
	lldebugs << url << llendl;
	if (isValid() && mEasy)
	{
		mEasy->setHeaders();
		mEasy->setoptString(CURLOPT_URL, url);
		mMulti->addEasy(mEasy);
	}
}

void LLCurlEasyRequest::requestComplete()
{
	llassert_always(mRequestSent);
	mRequestSent = false;
	if (isValid() && mEasy)
	{
		mMulti->removeEasy(mEasy);
	}
}

// Usage: Call getRestult until it returns false (no more messages)
bool LLCurlEasyRequest::getResult(CURLcode* result, LLCurl::TransferInfo* info)
{
	if(!isValid())
	{
		return false ;
	}
	if (!mMulti->isCompleted())
	{ //we're busy, try again later
		return false;
	}
	mMulti->setState(LLCurl::Multi::STATE_READY) ;

	if (!mEasy)
	{
		// Special case - we failed to initialize a curl_easy (can happen if too many open files)
		//  Act as though the request failed to connect
		if (mResultReturned)
		{
			return false;
		}
		else
		{
			*result = CURLE_FAILED_INIT;
			mResultReturned = true;
			return true;
		}
	}
	// In theory, info_read might return a message with a status other than CURLMSG_DONE
	// In practice for all messages returned, msg == CURLMSG_DONE
	// Ignore other messages just in case
	while(1)
	{
		S32 q;
		CURLMsg* curlmsg = info_read(&q, info);
		if (curlmsg)
		{
			if (curlmsg->msg == CURLMSG_DONE)
			{
				*result = curlmsg->data.result;			
				return true;
			}
			// else continue
		}
		else
		{
			return false;
		}
	}
}

// private
CURLMsg* LLCurlEasyRequest::info_read(S32* q, LLCurl::TransferInfo* info)
{
	if (mEasy)
	{
		CURLMsg* curlmsg = mMulti->info_read(q);
		if (curlmsg && curlmsg->msg == CURLMSG_DONE)
		{
			if (info)
			{
				mEasy->getTransferInfo(info);
			}
		}
		return curlmsg;
	}
	return NULL;
}

std::string LLCurlEasyRequest::getErrorString()
{
	return isValid() &&  mEasy ? std::string(mEasy->getErrorBuffer()) : std::string();
}

////////////////////////////////////////////////////////////////////////////

#if SAFE_SSL
//static
void LLCurl::ssl_locking_callback(int mode, int type, const char *file, int line)
{
	if (mode & CRYPTO_LOCK)
	{
		LLCurl::sSSLMutex[type]->lock();
	}
	else
	{
		LLCurl::sSSLMutex[type]->unlock();
	}
}

//static
unsigned long LLCurl::ssl_thread_id(void)
{
	return LLThread::currentID();
}
#endif

void LLCurl::initClass(F32 curl_reuest_timeout, S32 max_number_handles, bool multi_threaded)
{
	DoutEntering(dc::curl, "LLCurl::initClass(" << curl_reuest_timeout << ", " << max_number_handles << ", " << (multi_threaded ? "true" : "false") << ")");

	sCurlRequestTimeOut = curl_reuest_timeout ; //seconds
	sMaxHandles = max_number_handles ; //max number of handles, (multi handles and easy handles combined).

	// Do not change this "unless you are familiar with and mean to control 
	// internal operations of libcurl"
	// - http://curl.haxx.se/libcurl/c/curl_global_init.html
	CURLcode code = curl_global_init(CURL_GLOBAL_ALL);

	check_curl_code(code);
	
#if SAFE_SSL
	S32 mutex_count = CRYPTO_num_locks();
	for (S32 i=0; i<mutex_count; i++)
	{
		sSSLMutex.push_back(new LLMutex);
	}
	CRYPTO_set_id_callback(&LLCurl::ssl_thread_id);
	CRYPTO_set_locking_callback(&LLCurl::ssl_locking_callback);
#endif

	*AIAccess<LLCurlThread*>(sCurlThread) = new LLCurlThread(multi_threaded) ;
}

void LLCurl::cleanupClass()
{
	sNotQuitting = false; //set quitting

	//shut down curl thread
	AIAccess<LLCurlThread*> curl_thread_w(sCurlThread);
	LLCurlThread*& curl_thread_p = *curl_thread_w;
	while(1)
	{
		if(!curl_thread_p->update(1)) //finish all tasks
		{
			break ;
		}
	}
	curl_thread_p->shutdown() ;
	delete curl_thread_p;
	curl_thread_p = NULL;

#if SAFE_SSL
	CRYPTO_set_locking_callback(NULL);
	for_each(sSSLMutex.begin(), sSSLMutex.end(), DeletePointer());
#endif

	AIAccess<Easy::Handles> handles_w(Easy::sHandles);
	for (std::set<CURL*>::iterator iter = handles_w->free.begin(); iter != handles_w->free.end(); ++iter)
	{
		CURL* curl = *iter;
		LLCurl::deleteEasyHandle(curl);
	}
	handles_w->free.clear();
	llassert(handles_w->active.empty());
}

//static 
CURLM* LLCurl::newMultiHandle()
{
	DoutEntering(dc::curl, "LLCurl::newMultiHandle()");
	BACKTRACE;
	//*** Multi-threaded.
	CURLM* ret;
	{
		AIAccess<S32> sTotalHandles_w(sTotalHandles);
		if (*sTotalHandles_w + 1 > LLCurl::getMaxHandles())
		{
			//llwarns << "LLCurl::newMultiHandle: limit of curl handles reached. Returning NULL." << llendl;
			Dout(dc::warning, "LLCurl::newMultiHandle: limit of curl handles reached. Returning NULL.");
			return NULL; // Failed.
		}
		ret = curl_multi_init() ;
		if (ret)
		{
			++*sTotalHandles_w;
		}
		Dout(dc::curl, "Called curl_multi_int() returning " << (void*)ret << ", sTotalHandles is now " << *sTotalHandles_w);
		BACKTRACE;
	}

	if(!ret)
	{
		//llwarns << "curl_multi_init failed." << llendl ;
		Dout(dc::warning, "curl_multi_init failed.");
	}

	return ret ;
}

//static 
CURLMcode  LLCurl::deleteMultiHandle(CURLM* handle)
{
	DoutEntering(dc::curl, "LLCurl::deleteMultiHandle(" << (void*)handle << ")");
	BACKTRACE;
	if(handle)
	{
		AIAccess<S32> sTotalHandles_w(sTotalHandles);
		--*sTotalHandles_w;
		Dout(dc::curl, "Calling curl_multi_cleanup(" << (void*)handle << "), sTotalHandles is now " << *sTotalHandles_w);
		return curl_multi_cleanup(handle) ;
	}
	return CURLM_OK ;
}

//static 
CURL*  LLCurl::newEasyHandle()
{
	DoutEntering(dc::curl, "LLCurl::newEasyHandle()");
	BACKTRACE;
	//*** Multi-threaded.
	CURLM* ret;
	{
		AIAccess<S32> sTotalHandles_w(sTotalHandles);
		if(*sTotalHandles_w + 1 > LLCurl::getMaxHandles())
		{
			//llwarns << "LLCurl::newEasyHandle: limit of curl handles reached. Returning NULL." << llendl;
			Dout(dc::warning, "LLCurl::newEasyHandle: limit of curl handles reached. Returning NULL.");
			return NULL ; // Failed.
		}
		ret = curl_easy_init();
		if (ret)
		{
		  ++*sTotalHandles_w;
		}
		Dout(dc::curl, "Called curl_easy_init() returning " << (void*)ret << ", sTotalHandles is now " << *sTotalHandles_w);
		BACKTRACE;
	}

	if(!ret)
	{
		llwarns << "curl_easy_init failed." << llendl ;
	}

	return ret ;
}

//static 
void  LLCurl::deleteEasyHandle(CURL* handle)
{
	DoutEntering(dc::curl, "LLCurl::deleteEasyHandle(" << (void*)handle << ")");
	BACKTRACE;
	//*** Multi-threaded (logout only?).
	if(handle)
	{
		AIAccess<S32> sTotalHandles_w(sTotalHandles);
		--*sTotalHandles_w;
		Dout(dc::curl, "Calling curl_easy_cleanup(" << (void*)handle << "), sTotalHandles is now " << *sTotalHandles_w);
		curl_easy_cleanup(handle) ;
	}
}

const unsigned int LLCurl::MAX_REDIRECTS = 5;

// Provide access to LLCurl free functions outside of llcurl.cpp without polluting the global namespace.
void LLCurlFF::check_easy_code(CURLcode code)
{
	check_curl_code(code);
}
void LLCurlFF::check_multi_code(CURLMcode code)
{
	check_curl_multi_code(code);
}
