/** 
 * @file llviewermenufile.cpp
 * @brief "File" menu in the main menu bar.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * Second Life Viewer Source Code
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * 
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llviewermenufile.h"

// project includes
#include "llagent.h"
#include "llagentcamera.h"
#include "statemachine/aifilepicker.h"
#include "llfloaterbvhpreview.h"
#include "llfloaterimagepreview.h"
#include "llfloatermodelpreview.h"
#include "llfloaternamedesc.h"
#include "llfloatersnapshot.h"
#include "llimage.h"
#include "llimagebmp.h"
#include "llimagepng.h"
#include "llimagejpeg.h"
#include "llinventorymodel.h"	// gInventory
#include "llresourcedata.h"
#include "llfloaterperms.h"
#include "llstatusbar.h"
#include "llviewercontrol.h"	// gSavedSettings
#include "llviewertexturelist.h"
#include "lluictrlfactory.h"
#include "llviewermenu.h"	// gMenuHolder
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "llappviewer.h"
#include "lluploaddialog.h"
#include "lltrans.h"
#include "llfloaterbuycurrency.h"
// <edit>
#include "floaterlocalassetbrowse.h"
#include "llassettype.h"
#include "llinventorytype.h"
#include "aimultigridfrontend.h"
// </edit>

// linden libraries
#include "llassetuploadresponders.h"
#include "lleconomy.h"
#include "llhttpclient.h"
#include "llmemberlistener.h"
#include "llnotificationsutil.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "llstring.h"
#include "lluictrlfactory.h"
#include "lluuid.h"
#include "llvorbisencode.h"

// system libraries
#include <boost/tokenizer.hpp>

#include "hippogridmanager.h"

using namespace LLOldEvents;

typedef LLMemberListener<LLView> view_listener_t;


class LLFileEnableSaveAs : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		bool new_value = gFloaterView->getFrontmost() &&
						 gFloaterView->getFrontmost()->canSaveAs();
		gMenuHolder->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLFileEnableUpload : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		bool new_value = gStatusBar && LLGlobalEconomy::Singleton::getInstance() &&
						 gStatusBar->getBalance() >= LLGlobalEconomy::Singleton::getInstance()->getPriceUpload();
		gMenuHolder->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLFileEnableUploadModel : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		return gMeshRepo.meshUploadEnabled();
	}
};

//============================================================================

class AIFileUpload {
  public:
    AIFileUpload(void) { }
	virtual ~AIFileUpload() { }

  public:
	void filepicker_callback(ELoadFilter type, AIFilePicker* picker);
	void start_filepicker(ELoadFilter type, char const* context);
};

void AIFileUpload::start_filepicker(ELoadFilter filter, char const* context)
{
	if( gAgentCamera.cameraMouselook() )
	{
		gAgentCamera.changeCameraToDefault();
		// This doesn't seem necessary. JC
		// display();
	}

	AIFilePicker* picker = AIFilePicker::create();
	picker->open(filter, "", context);
	// Note that when the call back is called then we're still in the main loop of
	// the viewer and therefore the AIFileUpload still exists, since that is only
	// destructed at the end of main when exiting the viewer.
	picker->run(boost::bind(&AIFileUpload::filepicker_callback, this, filter, picker));
}

void AIFileUpload::filepicker_callback(ELoadFilter type, AIFilePicker* picker)
{
	if (picker->hasFilename())
	{
		LLPointer<AIMultiGrid::FrontEnd> front_end = new AIMultiGrid::FrontEnd;
		if (front_end->prepare_upload(picker->getFilename(), false))
		{
			switch(front_end->getAssetType())
			{
				case LLAssetType::AT_TEXTURE:
					LLUICtrlFactory::getInstance()->buildFloater(new LLFloaterImagePreview(front_end), "floater_image_preview.xml");
					break;
				case LLAssetType::AT_SOUND:
					LLUICtrlFactory::getInstance()->buildFloater(new LLFloaterSoundPreview(front_end), "floater_sound_preview.xml");
					break;
				case LLAssetType::AT_ANIMATION:
					if (front_end->isNativeFormat())
					{
						LLUICtrlFactory::getInstance()->buildFloater(new LLFloaterAnimPreview(front_end), "floater_animation_anim_preview.xml");
					}
					else
					{
						LLUICtrlFactory::getInstance()->buildFloater(new LLFloaterBvhPreview(front_end), "floater_animation_bvh_preview.xml");
					}
					break;
#if 0 // Not supported yet
				case LLAssetType::AT_SCRIPT:
					LLUICtrlFactory::getInstance()->buildFloater(new LLFloaterScriptPreview(front_end), "floater_script_preview.xml");
					break;
#endif
				default:
					{
						AIAlert::add_modal("AIUnsupportedFileFormat", AIArgs("[FILE]", picker->getFilename()));
						break;
					}
			}
		}
	}
}

class LLFileUploadImage : public view_listener_t, public AIFileUpload
{
  public:
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		start_filepicker(FFLOAD_IMAGE, "image");
		return true;
	}
};

class LLFileUploadSound : public view_listener_t, public AIFileUpload
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		start_filepicker(FFLOAD_SOUND, "sound");
		return true;
	}
};

class LLFileUploadAnim : public view_listener_t, public AIFileUpload
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		start_filepicker(FFLOAD_ANIM, "animations");
		return true;
	}
};

class LLFileUploadModel : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLFloaterModelPreview* fmp = new LLFloaterModelPreview("Model Preview");
		LLUICtrlFactory::getInstance()->buildFloater(fmp, "floater_model_preview.xml");
		if (fmp)
		{
			fmp->loadModel(3);
		}
		return true;
	}
};

/* Singu TODO? LL made a class to upload scripts, but never actually hooked everything up, it'd be nice for us to offer such a thing.
class LLFileUploadScript : public view_listener_t, public AIFileUpload
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		start_filepicker(FFLOAD_SCRIPT, "script");
		return true;
	}
};*/

class LLFileUploadBulk : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		if( gAgentCamera.cameraMouselook() )
		{
			gAgentCamera.changeCameraToDefault();
		}

		// TODO:
		// Iterate over all files
		// Check extensions for uploadability, cost
		// Check user balance for entire cost
		// Charge user entire cost
		// Loop, uploading
		// If an upload fails, refund the user for that one
		//
		// Also fix single upload to charge first, then refund
		// <edit>
		S32 expected_upload_cost = LLGlobalEconomy::Singleton::getInstance()->getPriceUpload();
		const char* notification_type = expected_upload_cost ? "BulkTemporaryUpload" : "BulkTemporaryUploadFree";
		LLSD args;
		args["UPLOADCOST"] = gHippoGridManager->getConnectedGrid()->getUploadFee();
		if (gHippoGridManager->getConnectedGrid()->isSecondLife())
		{
		  // SecondLife killed temporary uploads.
		  openFilePicker(false);
		  return true;
		}
		LLNotificationsUtil::add(notification_type, args, LLSD(), onConfirmBulkUploadTemp);
		return true;
	}

	static bool onConfirmBulkUploadTemp(const LLSD& notification, const LLSD& response )
	{
		S32 option = LLNotificationsUtil::getSelectedOption(notification, response);
		bool enabled;
		if (option == 0)		// yes
			enabled = true;
		else if(option == 1)	// no
			enabled = false;
		else					// cancel
			return false;

		openFilePicker(enabled);
		return true;
	}

	static bool openFilePicker(bool temporary)
	{
		AIFilePicker* filepicker = AIFilePicker::create();
		filepicker->open(FFLOAD_ALL, "", "openfile", true);
		filepicker->run(boost::bind(&LLFileUploadBulk::onConfirmBulkUploadTemp_continued, temporary, filepicker));
		return true;
	}

	static void onConfirmBulkUploadTemp_continued(bool enabled, AIFilePicker* filepicker)
	{
		if (filepicker->hasFilename())
		{
			std::vector<std::string> const& file_names(filepicker->getFilenames());
			for (std::vector<std::string>::const_iterator iter = file_names.begin(); iter != file_names.end(); ++iter)
			{
				std::string const& filename(*iter);
				if (filename.empty())
					continue;
				std::string name = gDirUtilp->getBaseFileName(filename, true);
				
				std::string asset_name = name;
				LLStringUtil::replaceNonstandardASCII( asset_name, '?' );
				LLStringUtil::replaceChar(asset_name, '|', '?');
				LLStringUtil::stripNonprintable(asset_name);
				LLStringUtil::trim(asset_name);
				
				std::string display_name = LLStringUtil::null;
				S32 expected_upload_cost = LLGlobalEconomy::Singleton::getInstance()->getPriceUpload();
				gSavedSettings.setBOOL("TemporaryUpload", enabled);
				LLPointer<AIMultiGrid::FrontEnd> mg_front_end = new AIMultiGrid::FrontEnd;
				if (mg_front_end->prepare_upload(filename, true))
				{
					mg_front_end->upload_new_resource_continued(asset_name, asset_name, 0, LLFolderType::FT_NONE, LLInventoryType::IT_NONE,
						LLFloaterPerms::getNextOwnerPerms(), LLFloaterPerms::getGroupPerms(), LLFloaterPerms::getEveryonePerms(),
						display_name, expected_upload_cost);
				}
			}
		}
		else
		{
			gSavedSettings.setBOOL("TemporaryUpload", FALSE);
		}
	}
};

class LLFileEnableCloseWindow : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		bool new_value = NULL != LLFloater::getClosableFloaterFromFocus();

		// horrendously opaque, this code
		gMenuHolder->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLFileCloseWindow : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLFloater::closeFocusedFloater();

		return true;
	}
};

class LLFileEnableCloseAllWindows : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		bool open_children = gFloaterView->allChildrenClosed();
		gMenuHolder->findControl(userdata["control"].asString())->setValue(!open_children);
		return true;
	}
};

class LLFileCloseAllWindows : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		bool app_quitting = false;
		gFloaterView->closeAllChildren(app_quitting);

		return true;
	}
};

// <edit>
class LLFileMinimizeAllWindows : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		gFloaterView->minimizeAllChildren();
		return true;
	}
};

class LLFileLocalAssetBrowser : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD&)
	{
		FloaterLocalAssetBrowser::show(0);
		return true;
	}
};
// </edit>

class LLFileSavePreview : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLFloater* top = gFloaterView->getFrontmost();
		if (top)
		{
			top->saveAs();
		}
		return true;
	}
};

class LLFileTakeSnapshot : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLFloaterSnapshot::show(NULL);
		return true;
	}
};

class LLFileTakeSnapshotToDisk : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLPointer<LLImageRaw> raw = new LLImageRaw;

		S32 width = gViewerWindow->getWindowDisplayWidth();
		S32 height = gViewerWindow->getWindowDisplayHeight();
		F32 ratio = (F32)width / height;

		F32 supersample = 1.f;
		if (gSavedSettings.getBOOL("HighResSnapshot"))
		{
#if 1//SHY_MOD // screenshot improvement
			const F32 mult = gSavedSettings.getF32("SHHighResSnapshotScale");
			width *= mult;
			height *= mult;
			static const LLCachedControl<F32> super_sample_scale("SHHighResSnapshotSuperSample",1.f);
			supersample = super_sample_scale;
#else //shy_mod
			width *= 2;
			height *= 2;
#endif //ignore
		}

		if (gViewerWindow->rawSnapshot(raw,
									   width,
									   height,
									   ratio,
									   gSavedSettings.getBOOL("RenderUIInSnapshot"),
									   FALSE,
									   LLViewerWindow::SNAPSHOT_TYPE_COLOR,
									   6144,
									   supersample))
		{
			LLPointer<LLImageFormatted> formatted;
			switch(LLFloaterSnapshot::ESnapshotFormat(gSavedSettings.getS32("SnapshotFormat")))
			{
			  case LLFloaterSnapshot::SNAPSHOT_FORMAT_JPEG:
				formatted = new LLImageJPEG(gSavedSettings.getS32("SnapshotQuality"));
				break;
			  case LLFloaterSnapshot::SNAPSHOT_FORMAT_PNG:
				formatted = new LLImagePNG;
				break;
			  case LLFloaterSnapshot::SNAPSHOT_FORMAT_BMP:
				formatted = new LLImageBMP;
				break;
			  default: 
				llwarns << "Unknown Local Snapshot format" << llendl;
				return true;
			}

			formatted->enableOverSize() ;
			formatted->encode(raw, 0);
			formatted->disableOverSize();
			gViewerWindow->saveImageNumbered(formatted, -1);
		}
		return true;
	}
};

class LLFileQuit : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent> event, const LLSD& userdata)
	{
		LLAppViewer::instance()->userQuit();
		return true;
	}
};

static void handle_compress_image_continued(AIFilePicker* filepicker);
void handle_compress_image(void*)
{
	AIFilePicker* filepicker = AIFilePicker::create();
	filepicker->open(FFLOAD_IMAGE, "", "openfile", true);
	filepicker->run(boost::bind(&handle_compress_image_continued, filepicker));
}

static void handle_compress_image_continued(AIFilePicker* filepicker)
{
	if (!filepicker->hasFilename())
		return;

	std::vector<std::string> const& filenames(filepicker->getFilenames());
	for(std::vector<std::string>::const_iterator filename = filenames.begin(); filename != filenames.end(); ++filename)
	{
		std::string const& infile(*filename);
		std::string outfile = infile + ".j2c";

		llinfos << "Input:  " << infile << llendl;
		llinfos << "Output: " << outfile << llendl;

		BOOL success;

		LLMD5 source_md5, asset_md5;
		success = LLViewerTextureList::createUploadFile(infile, outfile, IMG_CODEC_TGA, source_md5, asset_md5);

		if (success)
		{
			llinfos << "Compression complete" << llendl;
		}
		else
		{
			llinfos << "Compression failed: " << LLImage::getLastError() << llendl;
		}
	}
}

void init_menu_file()
{
	(new LLFileUploadImage())->registerListener(gMenuHolder, "File.UploadImage");
	(new LLFileUploadSound())->registerListener(gMenuHolder, "File.UploadSound");
	(new LLFileUploadAnim())->registerListener(gMenuHolder, "File.UploadAnim");
	//(new LLFileUploadScript())->registerListener(gMenuHolder, "File.UploadScript"); // Singu TODO?
	(new LLFileUploadModel())->registerListener(gMenuHolder, "File.UploadModel");
	(new LLFileUploadBulk())->registerListener(gMenuHolder, "File.UploadBulk");
	(new LLFileCloseWindow())->registerListener(gMenuHolder, "File.CloseWindow");
	(new LLFileCloseAllWindows())->registerListener(gMenuHolder, "File.CloseAllWindows");
	(new LLFileEnableCloseWindow())->registerListener(gMenuHolder, "File.EnableCloseWindow");
	(new LLFileEnableCloseAllWindows())->registerListener(gMenuHolder, "File.EnableCloseAllWindows");
	// <edit>
	(new LLFileMinimizeAllWindows())->registerListener(gMenuHolder, "File.MinimizeAllWindows");
	(new LLFileLocalAssetBrowser())->registerListener(gMenuHolder, "File.LocalAssetBrowser");
	// </edit>
	(new LLFileSavePreview())->registerListener(gMenuHolder, "File.SavePreview");
	(new LLFileTakeSnapshot())->registerListener(gMenuHolder, "File.TakeSnapshot");
	(new LLFileTakeSnapshotToDisk())->registerListener(gMenuHolder, "File.TakeSnapshotToDisk");
	(new LLFileQuit())->registerListener(gMenuHolder, "File.Quit");
	(new LLFileEnableUpload())->registerListener(gMenuHolder, "File.EnableUpload");
	(new LLFileEnableUploadModel())->registerListener(gMenuHolder, "File.EnableUploadModel");

	(new LLFileEnableSaveAs())->registerListener(gMenuHolder, "File.EnableSaveAs");
}
