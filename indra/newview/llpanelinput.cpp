/** 
 * @file llpanelinput.cpp
 * @brief Input preferences panel
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
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
#include "llpanelinput.h"
#include "lluictrlfactory.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llfloaterjoystick.h"
#include "llsliderctrl.h"
#include "llfloaterjoystick.h"
#include "llscrolllistctrl.h"
#include "llkeyboard.h"
#include "aifloatershortcut.h"

LLPanelInput::LLPanelInput() 
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_preferences_input.xml");
}

static void onFOVAdjust(LLUICtrl* source, void* data)
{
	LLSliderCtrl* slider = dynamic_cast<LLSliderCtrl*>(source);
	LLViewerCamera::getInstance()->setDefaultFOV(slider->getValueF32());
}

BOOL LLPanelInput::postBuild()
{
	// General TAB:
	//

	childSetAction("joystick_setup_button", onClickJoystickSetup, (void*)this);

	// cache values in case user cancels
	mPreAdjustFOV = gSavedSettings.getF32("CameraAngle");
	mPreAdjustCameraOffsetScale = gSavedSettings.getF32("CameraOffsetScale");

	childSetValue("mouse_sensitivity", gSavedSettings.getF32("MouseSensitivity"));
	childSetValue("automatic_fly", gSavedSettings.getBOOL("AutomaticFly"));
	childSetValue("invert_mouse", gSavedSettings.getBOOL("InvertMouse"));
	childSetValue("edit_camera_movement", gSavedSettings.getBOOL("EditCameraMovement"));
	childSetValue("appearance_camera_movement", gSavedSettings.getBOOL("AppearanceCameraMovement"));
	childSetValue("unsit_on_camera_reset", gSavedSettings.getBOOL("SianaUnsitOnCamReset"));
	childSetValue("first_person_avatar_visible", gSavedSettings.getBOOL("FirstPersonAvatarVisible"));

	LLSliderCtrl* fov_slider = getChild<LLSliderCtrl>("camera_fov");
	fov_slider->setCommitCallback(&onFOVAdjust);
	fov_slider->setMinValue(LLViewerCamera::getInstance()->getMinView());
	fov_slider->setMaxValue(LLViewerCamera::getInstance()->getMaxView());
	fov_slider->setValue(LLViewerCamera::getInstance()->getView());

	// Bindings TAB:
	//

	childSetCommitCallback("bindings_list", &onKeyBindingsListSelect, this);

	buildLists();
	buildCombos();

	return TRUE;
}

LLPanelInput::~LLPanelInput()
{
	// Children all cleaned up by default view destructor.
}

void LLPanelInput::apply()
{
	// any cancel after this point will use these new values
	mPreAdjustFOV = childGetValue("camera_fov").asReal();
	mPreAdjustCameraOffsetScale = childGetValue("camera_offset_scale").asReal();

	gSavedSettings.setF32("MouseSensitivity", childGetValue("mouse_sensitivity").asReal());
	gSavedSettings.setBOOL("AutomaticFly", childGetValue("automatic_fly"));
	gSavedSettings.setBOOL("InvertMouse", childGetValue("invert_mouse"));
	gSavedSettings.setBOOL("EditCameraMovement", childGetValue("edit_camera_movement"));
	gSavedSettings.setBOOL("AppearanceCameraMovement", childGetValue("appearance_camera_movement"));
	gSavedSettings.setBOOL("SianaUnsitOnCamReset", childGetValue("unsit_on_camera_reset"));
	gSavedSettings.setF32("CameraAngle", mPreAdjustFOV);
	gSavedSettings.setBOOL("FirstPersonAvatarVisible", childGetValue("first_person_avatar_visible"));
}

void LLPanelInput::cancel()
{
	LLViewerCamera::getInstance()->setDefaultFOV(mPreAdjustFOV);
	gSavedSettings.setF32("CameraAngle", LLViewerCamera::getInstance()->getView());
	gSavedSettings.setF32("CameraOffsetScale", mPreAdjustCameraOffsetScale);
}

//static
void LLPanelInput::onClickJoystickSetup(void* user_data)
{
	LLPanelInput* prefs = (LLPanelInput*)user_data;
	LLFloaterJoystick* floaterp = LLFloaterJoystick::showInstance();
	LLFloater* parent_floater = gFloaterView->getParentFloater(prefs);
	if (parent_floater)
	{
		parent_floater->addDependentFloater(floaterp, FALSE);
	}
}

//----------------------------------------------------------------------------
// Key binding

#include "aikeybindings.h"	// gAIKeyBindings

// Column names as defined in panel_preferences_input.xml
static int const scroll_list_cols = 4;
static char const* column_string[scroll_list_cols] = { "action", "inputmodes", "key1", "key2" };

void LLPanelInput::buildLists(void)
{
	LLScrollListCtrl& bindings_list = getChildRef<LLScrollListCtrl>("bindings_list");
	bindings_list.deleteAllItems();

	LLSD element;
	element["id"] = LLUUID::null;

	int row = 0;

	// Run over all input modes.
	for (int im = mKeyBindingsListFilter.mFilterInputMode ? mKeyBindingsListFilter.mInputMode : 0; im < number_of_inputmodes; ++im)
	{
		AIInputMode input_mode = static_cast<AIInputMode>(im);
		bool next_input_mode = true;

		// Run over all actions of this input mode.
		for (AIKeyBindings::iterator iter = gAIKeyBindings.begin(); iter != gAIKeyBindings.end(); ++iter)
		{
			AIKeyBinding& key_binding(*iter);
			if (key_binding.mLastInputMode != input_mode)
			{
				// Not the right input mode.
				continue;
			}
			if (next_input_mode && row > 0)
			{
				// Add a separator row.
				for(int scroll_list_col = 0; scroll_list_col < scroll_list_cols; ++scroll_list_col)
				{
					element["columns"][scroll_list_col]["column"] = column_string[scroll_list_col];
					element["columns"][scroll_list_col]["type"] = "separator";
				}
				bindings_list.addElement(element, ADD_BOTTOM);
				++row;
				llassert(row == bindings_list.getItemCount());
				LLScrollListItem* separator = bindings_list.getLastData();
				separator->setEnabled(FALSE);
			}
			// Run over all key events of this action.
			std::vector<AIKeyEvent>::iterator key_event = key_binding.mKeyEvents.begin();
			do
			{
				bool visible = false;
				bool needs_tooltip = false;

				// Run over the scroll list columns.
				for(int scroll_list_col = 0; scroll_list_col < scroll_list_cols; ++scroll_list_col)
				{
					bool filtered = false;

					// Determine value of column text.
					std::string value;
					if (scroll_list_col <= 1)
					{
						if (key_event == key_binding.mKeyEvents.begin())
						{
							visible = true;
							if (scroll_list_col == 0)
							{
								value = AIKeyBindings::stringFromAction(key_binding.mAction);
							}
							else
							{
								value = AIKeyBindings::stringFromInputMode(key_binding.mLastInputMode, AIKeyBindings::abbreviated_long);
								needs_tooltip = true;
							}
						}
					}
					else if (key_event == key_binding.mKeyEvents.end() || key_event->mKey == KEY_NONE)
					{
						value = "---";
					}
					else if (mKeyBindingsListFilter.mFilterModifiers && mKeyBindingsListFilter.mModifiers != key_event->mModifier)
					{
						filtered = true;
					}
					else
					{
						visible = true;
						value = LLKeyboard::stringFromShortcut(key_event->mModifier, key_event->mKey);
					}

					// Next  key event?
					if (key_event != key_binding.mKeyEvents.end() && scroll_list_col > 1)
					{
						++key_event;
					}

					if (!filtered)
					{
						element["columns"][scroll_list_col]["column"] = column_string[scroll_list_col];
						element["columns"][scroll_list_col]["type"] = "text";
						element["columns"][scroll_list_col]["value"] = value;
						element["columns"][scroll_list_col]["font"] = (scroll_list_col == 1) ? "MONESPACE" : "SANSSERIF";
						element["columns"][scroll_list_col]["font-style"] = (scroll_list_col == 1) ? "BOLD" : "NORMAL";
					}
					else
					{
						--scroll_list_col;
					}
				}
				if (visible)
				{
					bindings_list.addElement(element, ADD_BOTTOM);
					++row;
					if (needs_tooltip)
					{
						LLScrollListItem* item = bindings_list.getLastData();
						LLScrollListCell* cell = item->getColumn(1);
						std::string input_modes = cell->getValue().asString();
						std::string tooltip("Input modes in which this action is active:");
						std::string::iterator key = input_modes.begin();
						for (;;)
						{
							AIInputMode input_mode = AIKeyBindings::getInputMode(*key);
							tooltip += "\n";
							tooltip += *key;
							tooltip += ": ";
							tooltip += AIKeyBindings::stringFromInputMode(input_mode, AIKeyBindings::name);
							tooltip += " (";
							tooltip += AIKeyBindings::stringFromInputMode(input_mode, AIKeyBindings::description);
							tooltip += ")";
							if (++key == input_modes.end())
							{
								break;
							}
							tooltip += ",";
						}
						cell->setToolTip(tooltip);
					}
				}
			} while (key_event != key_binding.mKeyEvents.end());

			next_input_mode = false;
		}
	}
}

void LLPanelInput::buildCombos(void)
{
	LLComboBox* combo;
	
	combo = getChild<LLComboBox>("inputmodes_combobox");
	combo->add("Show all");
	for (int im = 0; im < number_of_inputmodes; ++im)
	{
		LLScrollListItem* item = combo->add(
			AIKeyBindings::stringFromInputMode(static_cast<AIInputMode>(im), AIKeyBindings::abbreviated) + ": " +
			AIKeyBindings::stringFromInputMode(static_cast<AIInputMode>(im), AIKeyBindings::name));
		item->setToolTip(AIKeyBindings::stringFromInputMode(static_cast<AIInputMode>(im), AIKeyBindings::description));
	}
	combo->setCommitCallback(boost::bind(&LLPanelInput::onInputModeFilter, this, _1, _2));

	combo = getChild<LLComboBox>("modifiers_combobox");
	combo->add("Show all");
	for (int modifiers = MASK_NONE; modifiers <= MASK_NORMALKEYS; ++modifiers)
	{
		std::string modifiers_string = (modifiers == MASK_NONE) ? std::string("none") : LLKeyboard::stringFromMask(modifiers);
		combo->add(modifiers_string);
	}
	combo->setCommitCallback(boost::bind(&LLPanelInput::onInputModifiersFilter, this, _1, _2));

	combo = getChild<LLComboBox>("shortcut_combobox");
	combo->setCommitCallback(boost::bind(&LLPanelInput::onManageShortcut, this, _1, _2));
}

//static
void LLPanelInput::onKeyBindingsListSelect(LLUICtrl* ctrl, void* user_data)
{
	DoutEntering(dc::notice, "LLPanelInput::onKeyBindingsListSelect()");
	LLScrollListCtrl* bindings_list = static_cast<LLScrollListCtrl*>(ctrl);
	LLPanelInput* panel_input = static_cast<LLPanelInput*>(user_data);
	LLTextBox* text_box = panel_input->getChild<LLTextBox>("selected_action");
	std::string text = text_box->getText();
	size_t quot1 = text.find_first_of('"');
	size_t quot2 = text.find_last_of('"');
	static std::string const none_selected = text.substr(quot1 + 1, quot2 - quot1 - 1);
	std::string const rest = text.substr(quot2);
	text = text.erase(quot1 + 1);

	LLScrollListItem* item = bindings_list->getFirstSelected();
	AIAction action;
	if (item)
	{
		std::string action_str = item->getColumn(0)->getValue().asString();
		if (action_str.empty())
		{
			// Walk to first row of this action (we are always sorted ascending).
			// Recursively calls this function.
			bindings_list->selectPrevItem(FALSE);
			return;
		}
		else
		{
			text += action_str;
		}
		action = AIKeyBindings::actionFromDescription(action_str);
		llassert_always(action < number_of_actions);
	}
	else
	{
		text += none_selected;
	}
	text += rest;
	text_box->setText(text);

	LLComboBox* combo = panel_input->getChild<LLComboBox>("shortcut_combobox");
	combo->removeall();
	if (item)
	{
		combo->add("Add Custom");
		for (AIKeyBindings::iterator action_iter = gAIKeyBindings.begin(); action_iter != gAIKeyBindings.end(); ++action_iter)
		{
			if (action_iter->mAction == action)
			{
				for (std::vector<AIKeyEvent>::iterator iter = action_iter->mKeyEvents.begin(); iter != action_iter->mKeyEvents.end(); ++iter)
				{
					combo->add(iter->asString());
				}
				break;
			}
		}
	}
}

void LLPanelInput::onInputModeFilter(LLUICtrl* ctrl, LLSD const& data)
{
	DoutEntering(dc::notice, "AIKeyBindingsListFilter::onInputModeFilter(" << data << ")");
	std::string value = data.asString();
	if (value == "Show all")
	{ 
		mKeyBindingsListFilter.mFilterInputMode = false;
	}
	else
	{
		mKeyBindingsListFilter.mInputMode = AIKeyBindings::getInputMode(value[0]);
		mKeyBindingsListFilter.mFilterInputMode = true;
	}
	buildLists();
}

void LLPanelInput::onInputModifiersFilter(LLUICtrl* ctrl, LLSD const& data)
{
	DoutEntering(dc::notice, "AIKeyBindingsListFilter::onInputModifiersFilter(" << data << ")");
	std::string value = data.asString();
	if (value == "Show all")
	{
		mKeyBindingsListFilter.mFilterModifiers = false;
	}
	else
	{
		mKeyBindingsListFilter.mModifiers = LLKeyboard::maskFromString(value);
		mKeyBindingsListFilter.mFilterModifiers = true;
	}
	buildLists();
}

void LLPanelInput::onManageShortcut(LLUICtrl* ctrl, LLSD const& data)
{
	DoutEntering(dc::notice, "LLPanelInput::onManageShortcut(" << data << ")");
	std::string value = data.asString();
	if (value == "Add Custom")
	{
	  Dout(dc::notice, "Selected: " << value);
	  AIFloaterShortcut::toggleInstance();
	}
	else
	{
	  Dout(dc::notice, "Selected: " << value);
	  AIFloaterShortcut::toggleInstance();
	}
}

