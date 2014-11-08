/**
 * @file aiperservice.cpp
 * @brief Implementation of AIPerService
 *
 * Copyright (c) 2012, 2013, Aleric Inglewood.
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
 *   04/11/2012
 *   Initial version, written by Aleric Inglewood @ SL
 *
 *   06/04/2013
 *   Renamed AICurlPrivate::PerHostRequestQueue[Ptr] to AIPerHostRequestQueue[Ptr]
 *   to allow public access.
 *
 *   09/04/2013
 *   Renamed everything "host" to "service" and use "hostname:port" as key
 *   instead of just "hostname".
 *
 *   20/10/2014
 *   Added HTTP pipeline support.
 */

#include "sys.h"
#include "aicurlperservice.h"
#include "aicurlthread.h"
#include "llcontrol.h"

AIPerService::threadsafe_instance_map_type AIPerService::sInstanceMap;
LLAtomicU32 AIPerService::sApprovedNonHTTPPipelineRequests;
AIThreadSafeSimpleDC<AIPerService::TotalNonHTTPPipelineQueued> AIPerService::sTotalNonHTTPPipelineQueued;
LLAtomicU32 AIPerService::sAddedConnections;

#undef AICurlPrivate

namespace AICurlPrivate {

// Cached value of CurlConcurrentConnectionsPerService.
U16 CurlConcurrentConnectionsPerService;
// Cached value of CurlMaxPipelinedRequestsPerService.
U16 CurlMaxPipelinedRequestsPerService;

// Friend functions of RefCountedThreadSafePerService

void intrusive_ptr_add_ref(RefCountedThreadSafePerService* per_service)
{
  per_service->mReferenceCount++;
}

void intrusive_ptr_release(RefCountedThreadSafePerService* per_service)
{
  if (--per_service->mReferenceCount == 0)
  {
    delete per_service;
  }
}

} // namespace AICurlPrivate

using namespace AICurlPrivate;

AIPerService::AIPerService(void) :
		mPipeliningDetected(false),
		mPipelineSupport(true),
		mIsBlackListed(false),
		mHTTPBandwidth(25),	// 25 = 1000 ms / 40 ms.
		mMaxTotalAddedEasyHandles(CurlMaxPipelinedRequestsPerService),
		mApprovedRequests(0),
		mTotalAddedEasyHandles(0),
		mEventPolls(0),
		mEstablishedConnections(0),
		mUsedCT(0),
		mCTInUse(0)
{
}

AIPerService::CapabilityType::CapabilityType(void) :
  		mApprovedRequests(0),
		mQueuedCommands(0),
		mAddedEasyHandles(0),
		mFlags(0),
		mDownloading(0),
		mUploaded(0),
		mMaxUnfinishedRequests(CurlMaxPipelinedRequestsPerService),
		mMaxAddedEasyHandles(CurlMaxPipelinedRequestsPerService)
{
}

AIPerService::CapabilityType::~CapabilityType()
{
}

// Fake copy constructor.
AIPerService::AIPerService(AIPerService const&) : mHTTPBandwidth(0)
{
}

// url must be of the form
// (see http://www.ietf.org/rfc/rfc3986.txt Appendix A for definitions not given here):
//
// url			= sheme ":" hier-part [ "?" query ] [ "#" fragment ]
// hier-part	= "//" authority path-abempty
// authority     = [ userinfo "@" ] host [ ":" port ]
// path-abempty  = *( "/" segment )
//
// That is, a hier-part of the form '/ path-absolute', '/ path-rootless' or
// '/ path-empty' is NOT allowed here. This should be safe because we only
// call this function for curl access, any file access would use APR.
//
// However, as a special exception, this function allows:
//
// url			= authority path-abempty
//
// without the 'sheme ":" "//"' parts.
//
// As follows from the ABNF (see RFC, Appendix A):
// - authority is either terminated by a '/' or by the end of the string because
//   neither userinfo, host nor port may contain a '/'.
// - userinfo does not contain a '@', and if it exists, is always terminated by a '@'.
// - port does not contain a ':', and if it exists is always prepended by a ':'.
//
// This function also needs to deal with full paths, in which case it should return
// an empty string.
//
// Full paths can have the form: "/something..."
//                           or  "C:\something..."
//               and maybe even  "C:/something..."
//
// The first form leads to an empty string being returned because the '/' signals the
// end of the authority and we'll return immediately.
// The second one will abort when hitting the backslash because that is an illegal
// character in an url (before the first '/' anyway).
// The third will abort because "C:" would be the hostname and a colon in the hostname
// is not legal.
//
//static
std::string AIPerService::extract_canonical_servicename(std::string const& url)
{
  char const* p = url.data();
  char const* const end = p + url.size();
  char const* sheme_colon = NULL;
  char const* sheme_slash = NULL;
  char const* first_ampersand = NULL;
  char const* port_colon = NULL;
  std::string servicename;
  char const* hostname = p;                 // Default in the case there is no "sheme://userinfo@".
  while (p < end)
  {
    int c = *p;
    if (c == ':')
    {
      if (!port_colon && LLStringOps::isDigit(p[1]))
      {
        port_colon = p;
      }
      else if (!sheme_colon && !sheme_slash && !first_ampersand && !port_colon)
      {
        // Found a colon before any slash or ampersand: this has to be the colon between the sheme and the hier-part.
        sheme_colon = p;
      }
    }
    else if (c == '/')
    {
      if (!sheme_slash && sheme_colon && sheme_colon == p - 1 && !first_ampersand && p[1] == '/')
      {
        // Found the first '/' in the first occurance of the sequence "://".
        sheme_slash = p;
        hostname = ++p + 1;                 // Point hostname to the start of the authority, the default when there is no "userinfo@" part.
        servicename.clear();                // Remove the sheme.
      }
      else
      {
        // Found a slash that is not part of the "sheme://" string. Signals end of authority.
        // We're done.
        if (hostname < sheme_colon)
        {
          // This happens when windows filenames are passed to this function of the form "C:/..."
          servicename.clear();
        }
        break;
      }
    }
    else if (c == '@')
    {
      if (!first_ampersand)
      {
        first_ampersand = p;
        hostname = p + 1;
        servicename.clear();                // Remove the "userinfo@"
      }
    }
    else if (c == '\\')
    {
      // Found a backslash, which is an illegal character for an URL. This is a windows path... reject it.
      servicename.clear();
      break;
    }
    if (p >= hostname)
    {
      // Convert hostname to lowercase in a way that we compare two hostnames equal iff libcurl does.
#if APR_CHARSET_EBCDIC
#error Not implemented
#else
      if (c >= 'A' && c <= 'Z')
        c += ('a' - 'A');
#endif
      servicename += c;
    }
    ++p;
  }
  // Strip off any trailing ":80".
  if (p - 3 == port_colon && p[-1] == '0' && p[-2] == '8')
  {
    return servicename.substr(0, p - hostname - 3);
  }
  return servicename;
}

//static
AIPerServicePtr AIPerService::instance(std::string const& servicename)
{
  llassert(!servicename.empty());
  instance_map_wat instance_map_w(sInstanceMap);
  AIPerService::iterator iter = instance_map_w->find(servicename);
  if (iter == instance_map_w->end())
  {
	iter = instance_map_w->insert(instance_map_type::value_type(servicename, new RefCountedThreadSafePerService)).first;
	Dout(dc::curlio, "Created new service \"" << servicename << "\" [" << (void*)&*PerService_rat(*iter->second) << "]");
  }
  // Note: the creation of AIPerServicePtr MUST be protected by the lock on sInstanceMap (see release()).
  return iter->second;
}

//static
void AIPerService::release(AIPerServicePtr& instance)
{
  if (instance->exactly_two_left())		// Being 'instance' and the one in sInstanceMap.
  {
	// The viewer can be have left main() we can't access the global sInstanceMap anymore.
	if (LLApp::isStopped())
	{
	  return;
	}
	instance_map_wat instance_map_w(sInstanceMap);
	// It is possible that 'exactly_two_left' is not up to date anymore.
	// Therefore, recheck the condition now that we have locked sInstanceMap.
	if (!instance->exactly_two_left())
	{
	  // Some other thread added this service in the meantime.
	  return;
	}
	bool is_blacklisted;
	{
	  PerService_rat per_service_r(*instance);
	  is_blacklisted = per_service_r->is_blacklisted();
#ifdef SHOW_ASSERT
	  // The reference in the map is the last one; that means there can't be any curl easy requests queued for this service.
	  for (int i = 0; i < number_of_capability_types; ++i)
	  {
	  	llassert(per_service_r->mCapabilityType[i].mQueuedRequests.empty());
	  }
#endif
	}
	// Find the service and erase it from the map.
	iterator const end = instance_map_w->end();
	for(iterator iter = instance_map_w->begin(); iter != end; ++iter)
	{
	  if (instance == iter->second)
	  {
		if (is_blacklisted)
		{
		  Dout(dc::curlio, "Removing \"" << iter->first << "\" from pipelining blacklist because the AIPerService is destructed.");
		  removePipeliningBlacklist(iter->first);
		}
		instance_map_w->erase(iter);
		instance.reset();
		return;
	  }
	}
	// We should always find the service.
	llassert(false);
  }
  instance.reset();
}

void AIPerService::redivide_easy_handle_slots(void)
{
  // Priority order.
  static AICapabilityType order[number_of_capability_types] = { cap_inventory, cap_texture, cap_mesh, cap_other };
  // Count the number of capability types that are currently in use and store the types in an array.
  AICapabilityType used_order[number_of_capability_types];
  int number_of_capability_types_in_use = 0;
  for (int i = 0; i < number_of_capability_types; ++i)
  {
	U32 const mask = CT2mask(order[i]);
	if ((mCTInUse & mask))
	{
	  used_order[number_of_capability_types_in_use++] = order[i];
	}
	else
	{
	  // Give every other type (that is not in use) one-ish easy handles, so they can be used (at which point they'll get more).
	  // Note that this is overhead - theoretically allowing us to go over the maximum that is set with the respective debug settings.
	  mCapabilityType[order[i]].mMaxAddedEasyHandles = llmax(mMaxTotalAddedEasyHandles / 8, 1);
	  if (order[i] == cap_other)
	  {
		// Add one extra for the long poll.
	    mCapabilityType[order[i]].mMaxAddedEasyHandles += 1;
	  }
	}
  }
  // Can happen when called from AIPerService::set_http_pipeline.
  if (number_of_capability_types_in_use == 0)
  {
	return;
  }
  // Keep one easy handle slot in reserve for currently unused capability types (that have been used before).
  int reserve = (mUsedCT != mCTInUse) ? 1 : 0;
  // Distribute (mMaxAddedEasyHandles - reserve) over number_of_capability_types_in_use.
  U16 max_slots_per_CT = (mMaxTotalAddedEasyHandles - reserve) / number_of_capability_types_in_use + 1;
  // The first count CTs get max_slots_per_CT easy handle slots.
  int count = (mMaxTotalAddedEasyHandles - reserve) % number_of_capability_types_in_use;
  for(int i = 1, j = 0;; --i)
  {
	while (j < count)
	{
	  mCapabilityType[used_order[j++]].mMaxAddedEasyHandles = max_slots_per_CT;
	}
	if (i == 0)
	{
	  break;
	}
	// Finish the loop till all used CTs are assigned.
	count = number_of_capability_types_in_use;
	// Never assign 0 as maximum.
	if (max_slots_per_CT > 1)
	{
	  // The remaining CTs get one easy handle slot less so that the sum of all assigned slots is mMaxAddedEasyHandles - reserve.
	  --max_slots_per_CT;
	}
  }
}

bool AIPerService::throttled(AICapabilityType capability_type) const
{
  return mTotalAddedEasyHandles >= (mPipeliningDetected ? mMaxTotalAddedEasyHandles : 1) ||
		 mCapabilityType[capability_type].mAddedEasyHandles >= mCapabilityType[capability_type].mMaxAddedEasyHandles;
}

bool AIPerService::added_to_multi_handle(AICapabilityType capability_type, bool event_poll)
{
  if (event_poll)
  {
	llassert(capability_type == cap_other);
	// We want to mark this service as unused when only long polls have been added, because they
	// are not counted towards the maximum number of easy handle slots for this service and therefore
	// should not cause another capability type to get less easy handle slots.
	// For example, if - like on opensim, without pipelining - Textures and Other capability types use
	// the same service then it is nonsense to reserve 4 connections for Other and only give 4 connections
	// to Textures, only because there is a long poll connection (or any number of long poll
	// connections). What we want is to see: 0-0-0,{0/7,0} for textures when Other is ONLY in
	// use for the Event Poll.
	//
	// This translates to that, since we're adding an event_poll and are about to remove it from
	// either the command queue OR the request queue, that when mAddedEasyHandles == 1 at the end of this function
	// (and the rest of the pipeline is empty) we want to mark this capability type as unused.
	//
	// If mEventPolls > 0 at this point then mAddedEasyHandles will not be incremented.
	// If mEventPolls == 0 then mAddedEasyHandles will be incremented and thus should be 0 now.
	// In other words, if the number of mAddedEasyHandles requests is equal to the number of (counted)
	// mEventPoll requests right now, then that will still be the case after we added another
	// event poll request (the transition from used to unused only being necessary because
	// event poll requests in the pipe line ARE counted; not because that is necessary but
	// because it would be more complex to not do so).
	//
	// Moreover, when we get here then the request that is being added is still counted as being in
	// the command queue, or the request queue, so that pipelined_requests() will return 1 more than
	// the actual count.
	U16 counted_event_polls = (mEventPolls == 0) ? 0 : 1;
	if (mCapabilityType[capability_type].mAddedEasyHandles == counted_event_polls &&
		mCapabilityType[capability_type].unfinished_requests() == counted_event_polls + 1)
	{
	  mark_unused(capability_type);
	}
	if (++mEventPolls > 1)
	{
	  // This only happens on megaregions. Do not count the additional long poll connections against the maximum handles for this service.
	  return false;
	}
  }
  ++mCapabilityType[capability_type].mAddedEasyHandles;
  if (mTotalAddedEasyHandles++ == 0 || !mPipelineSupport)
  {
	sAddedConnections++;
  }
  return true;
}

void AIPerService::removed_from_multi_handle(AICapabilityType capability_type, bool event_poll, bool downloaded_something, bool upload_finished, bool success)
{
  CapabilityType& ct(mCapabilityType[capability_type]);
  llassert(mTotalAddedEasyHandles > 0 && ct.mAddedEasyHandles > 0 && (!event_poll || mEventPolls));
  if (!event_poll || --mEventPolls == 0)
  {
	--ct.mAddedEasyHandles;
	if (--mTotalAddedEasyHandles == 0 || !mPipelineSupport)
	{
	  --sAddedConnections;
	}
  }
  if (downloaded_something)
  {
	llassert(ct.mDownloading > 0);
	--ct.mDownloading;
  }
  if (upload_finished)
  {
	llassert_always(ct.mUploaded > 0);
	--ct.mUploaded;
  }
  // If the number of added request handles is equal to the number of counted event poll handles,
  // in other words, when there are only long poll handles left, then mark the capability type
  // as unused.
  U16 counted_event_polls = (capability_type != cap_other || mEventPolls == 0) ? 0 : 1;
  if (ct.mAddedEasyHandles == counted_event_polls && ct.unfinished_requests() == counted_event_polls)
  {
	mark_unused(capability_type);
  }
  if (success)
  {
	ct.mFlags |= ctf_success;
  }
}

// Returns true if the request was queued.
bool AIPerService::queue(AICurlEasyRequest const& easy_request, AICapabilityType capability_type, bool force_queuing)
{
  CapabilityType::queued_request_type& queued_requests(mCapabilityType[capability_type].mQueuedRequests);
  bool needs_queuing = force_queuing || !queued_requests.empty();
  if (needs_queuing)
  {
	queued_requests.push_back(easy_request.get_ptr());
	if (!mPipelineSupport && is_approved(capability_type))
	{
	  TotalNonHTTPPipelineQueued_wat(sTotalNonHTTPPipelineQueued)->approved++;
	}
  }
  return needs_queuing;
}

bool AIPerService::cancel(AICurlEasyRequest const& easy_request, AICapabilityType capability_type)
{
  CapabilityType::queued_request_type::iterator const end = mCapabilityType[capability_type].mQueuedRequests.end();
  CapabilityType::queued_request_type::iterator cur = std::find(mCapabilityType[capability_type].mQueuedRequests.begin(), end, easy_request.get_ptr());

  if (cur == end)
	return false;		// Not found.

  // We can't use erase because that uses assignment to move elements,
  // because it isn't thread-safe. Therefore, move the element that we found to 
  // the back with swap (could just swap with the end immediately, but I don't
  // want to break the order in which requests where added). Swap is also not
  // thread-safe, but OK here because it only touches the objects in the deque,
  // and the deque is protected by the lock on the AIPerService object.
  CapabilityType::queued_request_type::iterator prev = cur;
  while (++cur != end)
  {
	prev->swap(*cur);				// This is safe,
	prev = cur;
  }
  mCapabilityType[capability_type].mQueuedRequests.pop_back();		// if this is safe.
  if (!mPipelineSupport && is_approved(capability_type))
  {
	TotalNonHTTPPipelineQueued_wat total_non_http_pipeline_queued_w(sTotalNonHTTPPipelineQueued);
	llassert(total_non_http_pipeline_queued_w->approved > 0);
	total_non_http_pipeline_queued_w->approved--;
  }
  return true;
}

void AIPerService::add_queued_to(curlthread::MultiHandle* multi_handle, bool only_this_service)
{
  U32 success = 0;									// The CTs that we successfully added a request for from the queue.
  bool success_this_pass = false;
  int i = 0;
  // The first pass we only look at CTs with 0 requests added to the multi handle. Subsequent passes only non-zero ones.
  for (int pass = 0;; ++i)
  {
	if (i == number_of_capability_types)
	{
	  i = 0;
	  // Keep trying until we couldn't add anything anymore.
	  if (pass++ && !success_this_pass)
	  {
		// Done.
		break;
	  }
	  success_this_pass = false;
	}
	CapabilityType& ct(mCapabilityType[i]);
	if (!pass != !ct.mAddedEasyHandles)						// Does mAddedEasyHandles match what we're looking for (first mAddedEasyHandles == 0, then mAddedEasyHandles != 0)?
	{
	  continue;
	}
	if (!mPipelineSupport && curlthread::MultiHandle::added_maximum())
	{
	  // We hit the maximum number of global connections. Abort every attempt to add anything.
	  only_this_service = true;
	  break;
	}
	if (mTotalAddedEasyHandles >= mMaxTotalAddedEasyHandles)
	{
	  // We hit the maximum number of easy handles for this service. Abort any attempt to add anything to this service.
	  break;
	}
	if (ct.mAddedEasyHandles >= ct.mMaxAddedEasyHandles)
	{
	  // We hit the maximum number of easy handles for this capability type. Try the next one.
	  continue;
	}
	U32 mask = CT2mask((AICapabilityType)i);
	if (ct.mQueuedRequests.empty())					// Is there anything in the queue (left) at all?
	{
	  // We could add a new request, but there is none in the queue!
	  // Note that if this service does not serve this capability type,
	  // then obviously this queue was empty; however, in that case
	  // this variable will never be looked at, so it's ok to set it.
	  ct.mFlags |= ((success & mask) ? ctf_empty : ctf_starvation);
	}
	else
	{
	  // Attempt to add the front of the queue.
	  if (!multi_handle->add_easy_request(ct.mQueuedRequests.front(), true))
	  {
		// If that failed then we got throttled on bandwidth because the maximum number of connections were not reached yet.
		// Therefore this will keep failing for this service, we abort any additional attempt to add something for this service.
		break;
	  }
	  // Request was added, remove it from the queue.
	  // Note: AIPerService::added_to_multi_handle (called from add_easy_request above) relies on the fact that
	  // we first add the easy handle and then remove it from the request queue (which is necessary to avoid
	  // that another thread adds one just in between).
	  ct.mQueuedRequests.pop_front();
	  // Mark that at least one request of this CT was successfully added.
	  success |= mask;
	  success_this_pass = true;
	  // Update approved count.
	  if (!mPipelineSupport && is_approved((AICapabilityType)i))
	  {
		TotalNonHTTPPipelineQueued_wat total_non_http_pipeline_queued_w(sTotalNonHTTPPipelineQueued);
		llassert(total_non_http_pipeline_queued_w->approved > 0);
		total_non_http_pipeline_queued_w->approved--;
	  }
	}
  }

  size_t queuedapproved_size = 0;
  for (int i = 0; i < number_of_capability_types; ++i)
  {
	CapabilityType& ct(mCapabilityType[i]);
	U32 mask = CT2mask((AICapabilityType)i);
	// Add up the size of all queues with approved requests.
	if ((approved_mask & mask))
	{
	  queuedapproved_size += ct.mQueuedRequests.size();
	}
	// Skip CTs that we didn't add anything for.
	if (!(success & mask))
	{
	  continue;
	}
	if (!ct.mQueuedRequests.empty())
	{
	  // We obtained one or more requests from the queue, and even after that there was at least one more request in the queue of this CT.
	  ct.mFlags |= ctf_full;
	}
  }

  // Update the flags of sTotalNonHTTPPipelineQueued.
  if (!mPipelineSupport)
  {
	TotalNonHTTPPipelineQueued_wat total_non_http_pipeline_queued_w(sTotalNonHTTPPipelineQueued);
	if (total_non_http_pipeline_queued_w->approved == 0)
	{
	  if ((success & approved_mask))
	  {
		// We obtained an approved request from the queue, and after that there were no more requests in any (approved) queue.
		total_non_http_pipeline_queued_w->empty = true;
	  }
	  else
	  {
		// Every queue of every approved CT is empty!
		total_non_http_pipeline_queued_w->starvation = true;
	  }
	}
	else if ((success & approved_mask))
	{
	  // We obtained an approved request from the queue, and even after that there was at least one more request in some (approved) queue.
	  total_non_http_pipeline_queued_w->full = true;
	}
  }
 
  // Don't try other services if anything was added successfully.
  if (success || only_this_service)
  {
	return;
  }

  // Nothing from this service could be added, try other services.
  instance_map_wat instance_map_w(sInstanceMap);
  for (iterator service = instance_map_w->begin(); service != instance_map_w->end(); ++service)
  {
	PerService_wat per_service_w(*service->second);
	if (&*per_service_w == this)
	{
	  continue;
	}
	per_service_w->add_queued_to(multi_handle, true);
  }
}

//static
void AIPerService::purge(void)
{
  instance_map_wat instance_map_w(sInstanceMap);
  for (iterator service = instance_map_w->begin(); service != instance_map_w->end(); ++service)
  {
	Dout(dc::curl, "Purging queues of service \"" << service->first << "\".");
	PerService_wat per_service_w(*service->second);
	TotalNonHTTPPipelineQueued_wat total_non_http_pipeline_queued_w(sTotalNonHTTPPipelineQueued);
	for (int i = 0; i < number_of_capability_types; ++i)
	{
	  size_t s = per_service_w->mCapabilityType[i].mQueuedRequests.size();
	  per_service_w->mCapabilityType[i].mQueuedRequests.clear();
	  if (!per_service_w->mPipelineSupport && is_approved((AICapabilityType)i))
	  {
		llassert(total_non_http_pipeline_queued_w->approved >= (S32)s);
		total_non_http_pipeline_queued_w->approved -= s;
	  }
	}
  }
}

//static
void AIPerService::adjust_max_added_easy_handles(int increment, bool for_pipeline_support)
{
  instance_map_wat instance_map_w(sInstanceMap);
  for (AIPerService::iterator iter = instance_map_w->begin(); iter != instance_map_w->end(); ++iter)
  {
	PerService_wat per_service_w(*iter->second);
	if (per_service_w->mPipelineSupport != for_pipeline_support)
	  continue;
	U16 old_max_added_easy_handles = per_service_w->mMaxTotalAddedEasyHandles;
	int new_max_added_easy_handles = llclamp(old_max_added_easy_handles + increment, 1, (int)(per_service_w->mPipelineSupport ? CurlMaxPipelinedRequestsPerService : CurlConcurrentConnectionsPerService));
	per_service_w->mMaxTotalAddedEasyHandles = (U16)new_max_added_easy_handles;
	increment = per_service_w->mMaxTotalAddedEasyHandles - old_max_added_easy_handles;
	for (int i = 0; i < number_of_capability_types; ++i)
	{
	  per_service_w->mCapabilityType[i].mMaxUnfinishedRequests = llmax(per_service_w->mCapabilityType[i].mMaxUnfinishedRequests + increment, 0);
	  int new_max_added_easy_handles_per_capability_type =
		  llclamp((new_max_added_easy_handles * per_service_w->mCapabilityType[i].mMaxAddedEasyHandles + old_max_added_easy_handles / 2) / old_max_added_easy_handles, 1, new_max_added_easy_handles);
	  per_service_w->mCapabilityType[i].mMaxAddedEasyHandles = (U16)new_max_added_easy_handles_per_capability_type;
	}
  }
}

void AIPerService::ResetUsed::operator()(AIPerService::instance_map_type::value_type const& service) const
{
  PerService_wat(*service.second)->resetUsedCt();
}

void AIPerService::Approvement::honored(void)
{
  if (!mHonored)
  {
	mHonored = true;
	PerService_wat per_service_w(*mPerServicePtr);
	llassert(per_service_w->mCapabilityType[mCapabilityType].mApprovedRequests > 0 && per_service_w->mApprovedRequests > 0);
	per_service_w->mCapabilityType[mCapabilityType].mApprovedRequests--;
	per_service_w->mApprovedRequests--;
	if (!per_service_w->is_http_pipeline())
	{
	  --sApprovedNonHTTPPipelineRequests;
	}
  }
}

void AIPerService::Approvement::not_honored(void)
{
  honored();
  llwarns << "Approvement for has not been honored." << llendl;
}

void AIPerService::set_http_pipeline(bool enable)
{
  DoutEntering(dc::curlio, "AIPerService::set_http_pipeline(" << enable << ") [" << (void*)this << "]");
  if (mPipelineSupport != enable)
  {
	Dout(dc::curlio, "Pipeline support changed from " << mPipelineSupport << " to " << enable);
	llassert(!mPipeliningDetected);		// Would be kinda weird if the detection changed its mind.
	mPipelineSupport = enable;
	int const sign = enable ? -1 : 1;
	sApprovedNonHTTPPipelineRequests += sign * mApprovedRequests;
	TotalNonHTTPPipelineQueued_wat total_non_http_pipeline_queued_w(sTotalNonHTTPPipelineQueued);
	for (int i = 0; i < number_of_capability_types; ++i)
	{
	  AICapabilityType capability_type = (AICapabilityType)i;
	  if (is_approved(capability_type))
	  {
		sApprovedNonHTTPPipelineRequests += sign * mCapabilityType[capability_type].mQueuedCommands;
		total_non_http_pipeline_queued_w->approved += sign * mCapabilityType[capability_type].mQueuedRequests.size();
	  }
	  mCapabilityType[i].mMaxUnfinishedRequests = enable ? CurlMaxPipelinedRequestsPerService : CurlConcurrentConnectionsPerService;
	}
	sAddedConnections += sign * (mTotalAddedEasyHandles ? mTotalAddedEasyHandles - 1 : 0);
	mMaxTotalAddedEasyHandles = enable ? CurlMaxPipelinedRequestsPerService : CurlConcurrentConnectionsPerService;
	redivide_easy_handle_slots();
  }
  mPipeliningDetected = true;
}
