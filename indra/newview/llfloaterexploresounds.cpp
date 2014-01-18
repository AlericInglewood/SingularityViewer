// <edit>

#include "llviewerprecompiledheaders.h"

#include "llfloaterexploresounds.h"

#include "llscrolllistctrl.h"
#include "llscrolllistitem.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llagentcamera.h"
#include "llfloaterblacklist.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"

static const size_t num_collision_sounds = 29;
const LLUUID collision_sounds[num_collision_sounds] =
{
    SND_FLESH_FLESH,
    SND_FLESH_PLASTIC,
    SND_FLESH_RUBBER,
    SND_GLASS_FLESH,
    SND_GLASS_GLASS,
    SND_GLASS_PLASTIC,
    SND_GLASS_RUBBER,
    SND_GLASS_WOOD,
    SND_METAL_FLESH,
    SND_METAL_GLASS,
    SND_METAL_METAL,
    SND_METAL_PLASTIC,
    SND_METAL_RUBBER,
    SND_METAL_WOOD,
    SND_PLASTIC_PLASTIC,
    SND_RUBBER_PLASTIC,
    SND_RUBBER_RUBBER,
    SND_STONE_FLESH,
    SND_STONE_GLASS,
    SND_STONE_METAL,
    SND_STONE_PLASTIC,
    SND_STONE_RUBBER,
    SND_STONE_STONE,
    SND_STONE_WOOD,
    SND_WOOD_FLESH,
    SND_WOOD_PLASTIC,
    SND_WOOD_RUBBER,
    SND_WOOD_WOOD,
	SND_STEP
};

LLFloaterExploreSounds* LLFloaterExploreSounds::sInstance;

LLFloaterExploreSounds::LLFloaterExploreSounds()
:	LLFloater(), LLEventTimer(0.25f)
{
	LLFloaterExploreSounds::sInstance = this;
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_explore_sounds.xml");
}

LLFloaterExploreSounds::~LLFloaterExploreSounds()
{
	LLFloaterExploreSounds::sInstance = NULL;
}

// static
void LLFloaterExploreSounds::toggle()
{
	if(LLFloaterExploreSounds::sInstance) LLFloaterExploreSounds::sInstance->close(FALSE);
	else LLFloaterExploreSounds::sInstance = new LLFloaterExploreSounds();
}

BOOL LLFloaterExploreSounds::visible()
{
	if (LLFloaterExploreSounds::sInstance)
		return TRUE;
	return FALSE;
}

void LLFloaterExploreSounds::close(bool app_quitting)
{
	LLFloater::close(app_quitting);
}

BOOL LLFloaterExploreSounds::postBuild(void)
{
	LLScrollListCtrl* soundlist = getChild<LLScrollListCtrl>("sound_list");
	soundlist->setDoubleClickCallback(boost::bind(&LLFloaterExploreSounds::handle_play_locally,this));
	soundlist->sortByColumn("playing", TRUE);

	childSetAction("play_locally_btn", handle_play_locally, this);
	childSetAction("look_at_btn", handle_look_at, this);
	childSetAction("stop_btn", handle_stop, this);
	childSetAction("bl_btn", blacklistSound, this);


	return TRUE;
}

LLSoundHistoryItem LLFloaterExploreSounds::getItem(LLUUID itemID)
{
	if(gSoundHistory.find(itemID) != gSoundHistory.end())
		return gSoundHistory[itemID];
	else
	{
		// If log is paused, hopefully we can find it in mLastHistory
		std::list<LLSoundHistoryItem>::iterator iter = mLastHistory.begin();
		std::list<LLSoundHistoryItem>::iterator end = mLastHistory.end();
		for( ; iter != end; ++iter)
		{
			if((*iter).mID == itemID) return (*iter);
		}
	}
	LLSoundHistoryItem item;
	item.mID = LLUUID::null;
	return item;
}

class LLSoundHistoryItemCompare
{
public:
	bool operator() (LLSoundHistoryItem first, LLSoundHistoryItem second)
	{
		if(first.isPlaying())
		{
			if(second.isPlaying())
			{
				return (first.mTimeStarted > second.mTimeStarted);
			}
			else
			{
				return true;
			}
		}
		else if(second.isPlaying())
		{
			return false;
		}
		else
		{
			return (first.mTimeStopped > second.mTimeStopped);
		}
	}
};

// static
BOOL LLFloaterExploreSounds::tick()
{
	//if(childGetValue("pause_chk").asBoolean()) return FALSE;

	bool show_collision_sounds = childGetValue("collision_chk").asBoolean();
	bool show_repeated_assets = childGetValue("repeated_asset_chk").asBoolean();
	bool show_avatars = childGetValue("avatars_chk").asBoolean();
	bool show_objects = childGetValue("objects_chk").asBoolean();

	std::list<LLSoundHistoryItem> history;
	if(childGetValue("pause_chk").asBoolean())
	{
		history = mLastHistory;
	}
	else
	{
		std::map<LLUUID, LLSoundHistoryItem>::iterator map_iter = gSoundHistory.begin();
		std::map<LLUUID, LLSoundHistoryItem>::iterator map_end = gSoundHistory.end();
		for( ; map_iter != map_end; ++map_iter)
		{
			history.push_back((*map_iter).second);
		}
		LLSoundHistoryItemCompare c;
		history.sort(c);
		mLastHistory = history;
	}

	LLScrollListCtrl* list = getChild<LLScrollListCtrl>("sound_list");

	// Save scroll pos and selection so they can be restored
	S32 scroll_pos = list->getScrollPos();
	uuid_vec_t selected_ids = list->getSelectedIDs();
	list->clearRows();

	std::list<LLUUID> unique_asset_list;

	std::list<LLSoundHistoryItem>::iterator iter = history.begin();
	std::list<LLSoundHistoryItem>::iterator end = history.end();
	for( ; iter != end; ++iter)
	{
		LLSoundHistoryItem item = (*iter);

		bool is_avatar = item.mOwnerID == item.mSourceID;
		if(is_avatar && !show_avatars) continue;

		bool is_object = !is_avatar;
		if(is_object && !show_objects) continue;

		bool is_repeated_asset = std::find(unique_asset_list.begin(), unique_asset_list.end(), item.mAssetID) != unique_asset_list.end();
		if(is_repeated_asset && !show_repeated_assets) continue;

		if(!item.mReviewed)
		{
			item.mReviewedCollision	= std::find(&collision_sounds[0], &collision_sounds[num_collision_sounds], item.mAssetID) != &collision_sounds[num_collision_sounds];
			item.mReviewed = true;
		}
		bool is_collision_sound = item.mReviewedCollision;
		if(is_collision_sound && !show_collision_sounds) continue;

		unique_asset_list.push_back(item.mAssetID);

		LLSD element;
		element["id"] = item.mID;

		LLSD& playing_column = element["columns"][0];
		playing_column["column"] = "playing";
		if(item.isPlaying())
		{
			playing_column["value"] = item.mIsLooped ? " Looping" : " Playing";
		}
		else
		{
			S32 time = (LLTimer::getElapsedSeconds() - item.mTimeStopped);
			S32 hours = time / 3600;
			S32 mins = time / 60;
			S32 secs = time % 60;
			playing_column["value"] = llformat("%d:%02d:%02d", hours,mins,secs);
		}

		LLSD& type_column = element["columns"][1];
		type_column["column"] = "type";
		type_column["type"] = "icon";
		if(item.mType == LLAudioEngine::AUDIO_TYPE_UI)
		{
			// this shouldn't happen for now, as UI is forbidden in the log
			type_column["value"] = "UI";
		}
		else
		{
			std::string type;
			std::string icon;
			if(is_avatar)
			{
				type = "Avatar";
				icon = "inv_item_gesture.tga";
			}
			else
			{
				if(item.mIsTrigger)
				{
					type = "llTriggerSound";
					icon = "inv_item_sound.tga";
				}
				else
				{
					if(item.mIsLooped)
						type = "llLoopSound";
					else
						type = "llPlaySound";
					icon = "inv_item_object.tga";
				}
			}

			type_column["value"] = icon;
			type_column["tool_tip"] = type;
		}

		LLSD& owner_column = element["columns"][2];
		owner_column["column"] = "owner";
		std::string fullname;
		BOOL is_group;
		if(gCacheName->getIfThere(item.mOwnerID, fullname, is_group))
		{
			if(is_group) fullname += " (Group)";
			owner_column["value"] = fullname;
		}
		else
			owner_column["value"] = item.mOwnerID.asString();

		LLSD& sound_column = element["columns"][3];
		sound_column["column"] = "sound";

		std::string uuid = item.mAssetID.asString();
		sound_column["value"] = uuid.substr(0, uuid.find_first_of("-")) + "...";

		list->addElement(element, ADD_BOTTOM);
	}

	list->selectMultiple(selected_ids);
	list->setScrollPos(scroll_pos);

	return FALSE;
}

// static
void LLFloaterExploreSounds::handle_play_locally(void* user_data)
{
	LLFloaterExploreSounds* floater = (LLFloaterExploreSounds*)user_data;
	LLScrollListCtrl* list = floater->getChild<LLScrollListCtrl>("sound_list");
	std::vector<LLScrollListItem*> selection = list->getAllSelected();
	std::vector<LLScrollListItem*>::iterator selection_iter = selection.begin();
	std::vector<LLScrollListItem*>::iterator selection_end = selection.end();
	std::vector<LLUUID> asset_list;
	for( ; selection_iter != selection_end; ++selection_iter)
	{
		LLSoundHistoryItem item = floater->getItem((*selection_iter)->getValue());
		if(item.mID.isNull()) continue;
		// Unique assets only
		if(std::find(asset_list.begin(), asset_list.end(), item.mAssetID) == asset_list.end())
		{
			asset_list.push_back(item.mAssetID);
			if(gAudiop)
			gAudiop->triggerSound(item.mAssetID, LLUUID::null, 1.0f, LLAudioEngine::AUDIO_TYPE_UI);
		}
	}
}

// static
void LLFloaterExploreSounds::handle_look_at(void* user_data)
{
	LLFloaterExploreSounds* floater = (LLFloaterExploreSounds*)user_data;
	LLScrollListCtrl* list = floater->getChild<LLScrollListCtrl>("sound_list");
	LLUUID selection = list->getSelectedValue().asUUID();
	LLSoundHistoryItem item = floater->getItem(selection); // Single item only
	if(item.mID.isNull()) return;

	LLVector3d pos_global = item.mPosition;

	// Try to find object position
	if(item.mSourceID.notNull())
	{
		LLViewerObject* object = gObjectList.findObject(item.mSourceID);
		if(object)
		{
			pos_global = object->getPositionGlobal();
		}
	}

	// Move the camera
	// Find direction to self (reverse)
	LLVector3d cam = gAgent.getPositionGlobal() - pos_global;
	cam.normalize();
	// Go 4 meters back and 3 meters up
	cam *= 4.0f;
	cam += pos_global;
	cam += LLVector3d(0.f, 0.f, 3.0f);

	gAgentCamera.setFocusOnAvatar(FALSE, FALSE);
	gAgentCamera.setCameraPosAndFocusGlobal(cam, pos_global, item.mSourceID);
	gAgentCamera.setCameraAnimating(FALSE);
}

// static
void LLFloaterExploreSounds::handle_stop(void* user_data)
{
	LLFloaterExploreSounds* floater = (LLFloaterExploreSounds*)user_data;
	LLScrollListCtrl* list = floater->getChild<LLScrollListCtrl>("sound_list");
	std::vector<LLScrollListItem*> selection = list->getAllSelected();
	std::vector<LLScrollListItem*>::iterator selection_iter = selection.begin();
	std::vector<LLScrollListItem*>::iterator selection_end = selection.end();
	std::vector<LLUUID> asset_list;
	for( ; selection_iter != selection_end; ++selection_iter)
	{
		LLSoundHistoryItem item = floater->getItem((*selection_iter)->getValue());
		if(item.mID.isNull()) continue;
		if(item.isPlaying())
		{
			item.mAudioSource->play(LLUUID::null);
		}
	}
}

void LLFloaterExploreSounds::blacklistSound(void* user_data)
{
	LLFloaterBlacklist::show();
	LLFloaterExploreSounds* floater = (LLFloaterExploreSounds*)user_data;
	LLScrollListCtrl* list = floater->getChild<LLScrollListCtrl>("sound_list");

	typedef std::vector<LLScrollListItem*> item_map_t;
	item_map_t selection = list->getAllSelected();
	item_map_t::iterator selection_end = selection.end();
	
	for(item_map_t::iterator selection_iter = selection.begin(); selection_iter != selection_end; ++selection_iter)
	{
		LLSoundHistoryItem item = floater->getItem((*selection_iter)->getValue());
		if(item.mID.isNull()) continue;

		LLSD sound_data;
		std::string agent;
		gCacheName->getFullName(item.mOwnerID, agent);
		LLViewerRegion* cur_region = gAgent.getRegion();

		if(cur_region)
		  sound_data["entry_name"] = llformat("Sound played by %s in region %s",agent.c_str(),cur_region->getName().c_str());
		else
		  sound_data["entry_name"] = llformat("Sound played by %s",agent.c_str());
		sound_data["entry_type"] = (LLAssetType::EType)item.mType;
		sound_data["entry_agent"] = gAgent.getID();
		LLFloaterBlacklist::addEntry(item.mAssetID,sound_data);
	}
}

// </edit>
