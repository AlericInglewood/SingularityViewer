/**
 * @file llfloaterdaycycle.cpp
 * @brief LLFloaterDayCycle class definition
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 * 
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llfloaterdaycycle.h"

#include "pipeline.h"
#include "llsky.h"

#include "llsliderctrl.h"
#include "llmultislider.h"
#include "llmultisliderctrl.h"
#include "llnotificationsutil.h"
#include "llspinctrl.h"
#include "llcheckboxctrl.h"
#include "lluictrlfactory.h"
#include "llviewercamera.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llwlanimator.h"

#include "v4math.h"
#include "llviewerdisplay.h"
#include "llviewercontrol.h"
#include "llviewerwindow.h"

#include "lldaycyclemanager.h"
#include "llwlparamset.h"
#include "llwlparammanager.h"
#include "llpostprocess.h"
#include "llfloaterwindlight.h"


LLFloaterDayCycle* LLFloaterDayCycle::sDayCycle = NULL;
std::map<std::string, LLWLSkyKey> LLFloaterDayCycle::sSliderToKey;
const F32 LLFloaterDayCycle::sHoursPerDay = 24.0f;

LLFloaterDayCycle::LLFloaterDayCycle() : LLFloater(std::string("Day Cycle Floater"))
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_day_cycle_options.xml");
	
	// add the combo boxes
	LLComboBox* keyCombo = getChild<LLComboBox>("WLKeyPresets");

	if(keyCombo != NULL) 
	{
		LLWLParamManager::preset_name_list_t local_presets;
		LLWLParamManager::getInstance()->getLocalPresetNames(local_presets);

		for (LLWLParamManager::preset_name_list_t::const_iterator it = local_presets.begin(); it != local_presets.end(); ++it)
		{
			keyCombo->add(*it);
		}

		// set defaults on combo boxes
		keyCombo->selectFirstItem();
	}

	// add the time slider
	LLMultiSliderCtrl* sldr = getChild<LLMultiSliderCtrl>("WLTimeSlider");

	sldr->addSlider();

	// add the combo boxes
	LLComboBox* comboBox = getChild<LLComboBox>("DayCyclePresetsCombo");

	if(comboBox != NULL) {

		LLDayCycleManager::preset_name_list_t day_presets;
		LLDayCycleManager::getInstance()->getPresetNames(day_presets);
		LLDayCycleManager::preset_name_list_t::const_iterator it;
		for(it = day_presets.begin(); it != day_presets.end(); ++it)
		{
			comboBox->add(*it);
		}

		// entry for when we're in estate time
		comboBox->add(LLStringUtil::null);

		// set defaults on combo boxes
		//comboBox->selectByValue(LLSD("Default"));
	}

	// load it up
	initCallbacks();
}

LLFloaterDayCycle::~LLFloaterDayCycle()
{
}

void LLFloaterDayCycle::onClickHelp(void* data)
{
	LLFloaterDayCycle* self = LLFloaterDayCycle::instance();

	std::string xml_alert = *(std::string *) data;
	LLNotifications::instance().add(self->contextualNotification(xml_alert));
}

void LLFloaterDayCycle::initHelpBtn(const std::string& name, const std::string& xml_alert)
{
	childSetAction(name, onClickHelp, new std::string(xml_alert));
}

void LLFloaterDayCycle::initCallbacks(void) 
{
	initHelpBtn("WLDayCycleHelp", "HelpDayCycle");

	// WL Day Cycle
	childSetCommitCallback("WLTimeSlider", onTimeSliderMoved, NULL);
	childSetCommitCallback("WLDayCycleKeys", onKeyTimeMoved, NULL);
	childSetCommitCallback("WLCurKeyHour", onKeyTimeChanged, NULL);
	childSetCommitCallback("WLCurKeyMin", onKeyTimeChanged, NULL);
	childSetCommitCallback("WLKeyPresets", onKeyPresetChanged, NULL);

	childSetCommitCallback("WLLengthOfDayHour", onTimeRateChanged, NULL);
	childSetCommitCallback("WLLengthOfDayMin", onTimeRateChanged, NULL);
	childSetCommitCallback("WLLengthOfDaySec", onTimeRateChanged, NULL);
	childSetAction("WLUseLindenTime", onUseLindenTime, NULL);
	childSetAction("WLAnimSky", onRunAnimSky, NULL);
	childSetAction("WLStopAnimSky", onStopAnimSky, NULL);

	LLComboBox* comboBox = getChild<LLComboBox>("DayCyclePresetsCombo");

	//childSetAction("WLLoadPreset", onLoadPreset, comboBox);
	childSetAction("DayCycleNewPreset", onNewPreset, comboBox);
	childSetAction("DayCycleSavePreset", onSavePreset, comboBox);
	childSetAction("DayCycleDeletePreset", onDeletePreset, comboBox);

	comboBox->setCommitCallback(onChangePresetName);


	childSetAction("WLAddKey", onAddKey, NULL);
	childSetAction("WLDeleteKey", onDeleteKey, NULL);
}

void LLFloaterDayCycle::syncMenu()
{
//	std::map<std::string, LLVector4> & currentParams = LLWLParamManager::getInstance()->mCurParams.mParamValues;
	
	// set time
	LLMultiSliderCtrl* sldr = LLFloaterDayCycle::sDayCycle->getChild<LLMultiSliderCtrl>("WLTimeSlider");
	sldr->setCurSliderValue((F32)LLWLParamManager::getInstance()->mAnimator.getDayTime() * sHoursPerDay);

	LLSpinCtrl* secSpin = sDayCycle->getChild<LLSpinCtrl>("WLLengthOfDaySec");
	LLSpinCtrl* minSpin = sDayCycle->getChild<LLSpinCtrl>("WLLengthOfDayMin");
	LLSpinCtrl* hourSpin = sDayCycle->getChild<LLSpinCtrl>("WLLengthOfDayHour");

	F32 curRate;
	F32 hours, min, sec;

	// get the current rate
	curRate = LLWLParamManager::getInstance()->mDay.mDayRate;
	hours = (F32)((int)(curRate / 60 / 60));
	curRate -= (hours * 60 * 60);
	min = (F32)((int)(curRate / 60));
	curRate -= (min * 60);
	sec = curRate;

	hourSpin->setValue(hours);
	minSpin->setValue(min);
	secSpin->setValue(sec);

	// turn off Use Estate Time button if it's already being used
	if(	LLWLParamManager::getInstance()->mAnimator.getUseLindenTime())
	{
		LLFloaterDayCycle::sDayCycle->childDisable("WLUseLindenTime");
	} 
	else 
	{
		LLFloaterDayCycle::sDayCycle->childEnable("WLUseLindenTime");
	}
}

void LLFloaterDayCycle::syncSliderTrack()
{
	// clear the slider
	LLMultiSliderCtrl* kSldr = sDayCycle->getChild<LLMultiSliderCtrl>("WLDayCycleKeys");

	kSldr->clear();
	sSliderToKey.clear();

	// add sliders
	std::map<F32, LLWLParamKey>::iterator mIt = 
		LLWLParamManager::getInstance()->mDay.mTimeMap.begin();
	for(; mIt != LLWLParamManager::getInstance()->mDay.mTimeMap.end(); mIt++)
	{
		addSliderKey(mIt->first * sHoursPerDay, mIt->second.name);
	}
}

void LLFloaterDayCycle::syncTrack()
{
	// if no keys, do nothing
	if(sSliderToKey.size() == 0) 
	{
		return;
	}
	
	LLMultiSliderCtrl* sldr;
	sldr = sDayCycle->getChild<LLMultiSliderCtrl>( 
		"WLDayCycleKeys");
	llassert_always(sSliderToKey.size() == sldr->getValue().size());
	
	LLMultiSliderCtrl* tSldr;
	tSldr = sDayCycle->getChild<LLMultiSliderCtrl>( 
		"WLTimeSlider");

	// create a new animation track
	LLWLParamManager::getInstance()->mDay.clearKeys();
	
	// add the keys one by one
	std::map<std::string, LLWLSkyKey>::iterator mIt = sSliderToKey.begin();
	for(; mIt != sSliderToKey.end(); mIt++) 
	{
		LLWLParamManager::getInstance()->mDay.addKey(mIt->second.time / sHoursPerDay,
			mIt->second.presetName);
	}
	
	// set the param manager's track to the new one
	LLWLParamManager::getInstance()->resetAnimator(
		tSldr->getCurSliderValue() / sHoursPerDay, false);

	LLWLParamManager::getInstance()->mAnimator.update(
		LLWLParamManager::getInstance()->mCurParams);
}

// static
LLFloaterDayCycle* LLFloaterDayCycle::instance()
{
	if (!sDayCycle)
	{
		sDayCycle = new LLFloaterDayCycle();
		sDayCycle->open();
		sDayCycle->setFocus(TRUE);
	}
	return sDayCycle;
}

bool LLFloaterDayCycle::isOpen()
{
	if (sDayCycle != NULL) 
	{
		return sDayCycle->getVisible();
	}
	return false;
}

void LLFloaterDayCycle::show()
{
	LLFloaterDayCycle* dayCycle = instance();
	dayCycle->syncMenu();
	syncSliderTrack();

	// comment in if you want the menu to rebuild each time
	//LLUICtrlFactory::getInstance()->buildFloater(dayCycle, "floater_day_cycle_options.xml");
	//dayCycle->initCallbacks();

	dayCycle->open();
}

// virtual
void LLFloaterDayCycle::onClose(bool app_quitting)
{
	if (sDayCycle)
	{
		sDayCycle->setVisible(FALSE);
	}
}

void LLFloaterDayCycle::onNewPreset(void* userData)
{
	LLNotificationsUtil::add("NewDaycyclePreset", LLSD(), LLSD(), newPromptCallback);
}

void LLFloaterDayCycle::onSavePreset(void* userData)
{
	// get the name
	LLComboBox* comboBox = sDayCycle->getChild<LLComboBox>(
							   "DayCyclePresetsCombo");

	std::string name = comboBox->getSelectedItemLabel();

	// don't save the empty name
	if(name == "")
	{
		return;
	}

	// check to see if it's a default and shouldn't be overwritten

	if(LLDayCycleManager::getInstance()->isSystemPreset(name))
	{
		LLNotificationsUtil::add("WLNoEditDefault");
		return;
	}

	LLWLParamManager::getInstance()->mCurParams.mName = name;

	LLNotificationsUtil::add("WLSavePresetAlert", LLSD(), LLSD(), saveAlertCallback);
}


bool LLFloaterDayCycle::saveAlertCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	// if they choose save, do it.  Otherwise, don't do anything
	if(option == 0)
	{
		LLComboBox* combo_box = sDayCycle->getChild<LLComboBox>("DayCyclePresetsCombo");
		// comment this back in to save to file
		LLWLParamManager::getInstance()->mDay.saveDayCycle(combo_box->getSelectedValue().asString());
	}
	return false;
}

void LLFloaterDayCycle::onDeletePreset(void* userData)
{
	LLComboBox* combo_box = sDayCycle->getChild<LLComboBox>(
								"DayCyclePresetsCombo");

	if(combo_box->getSelectedValue().asString() == "")
	{
		return;
	}

	LLSD args;
	args["SKY"] = combo_box->getSelectedValue().asString();
	LLNotificationsUtil::add("WLDeletePresetAlert", args, LLSD(),
							 boost::bind(&LLFloaterDayCycle::deleteAlertCallback, sDayCycle, _1, _2));
}

bool LLFloaterDayCycle::deleteAlertCallback(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	// if they choose delete, do it.  Otherwise, don't do anything
	if(option == 0)
	{
		LLComboBox* combo_box = getChild<LLComboBox>(
									"DayCyclePresetsCombo");
		LLFloaterDayCycle* day_cycle = NULL;
		LLComboBox* key_combo = NULL;
		LLMultiSliderCtrl* mult_sldr = NULL;

		if(LLFloaterDayCycle::isOpen())
		{
			day_cycle = LLFloaterDayCycle::instance();
			key_combo = day_cycle->getChild<LLComboBox>(
							"WLKeyPresets");
			mult_sldr = day_cycle->getChild<LLMultiSliderCtrl>("WLDayCycleKeys");
		}

		std::string name(combo_box->getSelectedValue().asString());

		// check to see if it's a default and shouldn't be deleted

		if(LLDayCycleManager::getInstance()->isSystemPreset(name))
		{
			LLNotificationsUtil::add("WLNoEditDefault");
			return false;
		}

		LLDayCycleManager::getInstance()->deletePreset(name);

		// remove and choose another
		S32 new_index = combo_box->getCurrentIndex();

		combo_box->remove(name);
		if(key_combo != NULL)
		{
			key_combo->remove(name);

			// remove from slider, as well
			day_cycle->deletePreset(name);
		}

		// pick the previously selected index after delete
		if(new_index > 0)
		{
			new_index--;
		}

		if(combo_box->getItemCount() > 0)
		{
			combo_box->setCurrentByIndex(new_index);
		}
	}
	return false;
}

bool LLFloaterDayCycle::newPromptCallback(const LLSD& notification, const LLSD& response)
{
	std::string text = response["message"].asString();
	S32 option = LLNotification::getSelectedOption(notification, response);

	if(text == "")
	{
		return false;
	}

	if(option == 0) {
		LLComboBox* comboBox = sDayCycle->getChild<LLComboBox>("DayCyclePresetsCombo");

		LLFloaterDayCycle* sDayCycle = NULL;
		LLComboBox* keyCombo = NULL;
		if(LLFloaterDayCycle::isOpen())
		{
			sDayCycle = LLFloaterDayCycle::instance();
			keyCombo = sDayCycle->getChild<LLComboBox>("WLKeyPresets");
		}


		// add the current parameters to the list
		// see if it's there first
		// if not there, add a new one
		if(LLDayCycleManager::getInstance()->findPreset(text).empty())
		{
			//AscentDayCycleManager::instance()->addParamSet(text,
			//	AscentDayCycleManager::instance()->mCurParams);

			LLDayCycleManager::getInstance()->savePreset(text,
						LLWLParamManager::getInstance()->mDay.asLLSD());

			comboBox->add(text);
			comboBox->sortByName();

			// add a blank to the bottom
			comboBox->selectFirstItem();
			if(comboBox->getSimple() == "")
			{
				comboBox->remove(0);
			}
			comboBox->add(LLStringUtil::null);

			comboBox->setSelectedByValue(text, true);
			if(LLFloaterDayCycle::isOpen())
			{
				keyCombo->add(text);
				keyCombo->sortByName();
			}
		}
		else // otherwise, send a message to the user
		{
			LLNotificationsUtil::add("ExistsSkyPresetAlert");
		}
	}
	return false;
}


void LLFloaterDayCycle::onChangePresetName(LLUICtrl* ctrl, void * userData)
{
	LLComboBox * combo_box = static_cast<LLComboBox*>(ctrl);

	if(combo_box->getSimple() == "")
	{
		return;
	}

	LLEnvManagerNew::getInstance()->useDayCycle(combo_box->getSelectedValue().asString(), LLEnvKey::SCOPE_LOCAL);

	gSavedSettings.setString("AscentActiveDayCycle", combo_box->getSelectedValue().asString());
	// sync it all up
	syncSliderTrack();
	syncMenu();

	// set the param manager's track to the new one
	LLMultiSliderCtrl* tSldr;
	tSldr = sDayCycle->getChild<LLMultiSliderCtrl>("WLTimeSlider");
	LLWLParamManager::getInstance()->resetAnimator(tSldr->getCurSliderValue() / sHoursPerDay, false);

	// and draw it
	LLWLParamManager::getInstance()->mAnimator.update(LLWLParamManager::getInstance()->mCurParams);
}


void LLFloaterDayCycle::onRunAnimSky(void* userData)
{
	// if no keys, do nothing
	if(sSliderToKey.size() == 0)
	{
		return;
	}
	
	LLMultiSliderCtrl* sldr;
	sldr = sDayCycle->getChild<LLMultiSliderCtrl>("WLDayCycleKeys");
	llassert_always(sSliderToKey.size() == sldr->getValue().size());

	LLMultiSliderCtrl* tSldr;
	tSldr = sDayCycle->getChild<LLMultiSliderCtrl>("WLTimeSlider");

	// turn off linden time
	//LLWLParamManager::getInstance()->mAnimator.mUseLindenTime = false;

	// set the param manager's track to the new one
	LLWLParamManager::getInstance()->resetAnimator(
		tSldr->getCurSliderValue() / sHoursPerDay, true);

	llassert_always(LLWLParamManager::getInstance()->mAnimator.mTimeTrack.size() == sldr->getValue().size());
}

void LLFloaterDayCycle::onStopAnimSky(void* userData)
{
	// if no keys, do nothing
	if(sSliderToKey.size() == 0) {
		return;
	}

	// turn off animation and using linden time
	LLWLParamManager::getInstance()->mAnimator.deactivate();
}

void LLFloaterDayCycle::onUseLindenTime(void* userData)
{
	LLFloaterWindLight* wl = LLFloaterWindLight::instance();
	LLComboBox* box = wl->getChild<LLComboBox>("WLPresetsCombo");
	box->selectByValue("");	

	LLWLParamManager::getInstance()->mAnimator.activate(LLWLAnimator::TIME_LINDEN);
	LLEnvManagerNew::instance().setUseDayCycle(LLEnvManagerNew::instance().getDayCycleName());
}

void LLFloaterDayCycle::onTimeSliderMoved(LLUICtrl* ctrl, void* userData)
{
	LLMultiSliderCtrl* sldr = sDayCycle->getChild<LLMultiSliderCtrl>( 
		"WLTimeSlider");

	/// get the slider value
	F32 val = sldr->getCurSliderValue() / sHoursPerDay;
	
	// set the value, turn off animation
	LLWLParamManager::getInstance()->mAnimator.setDayTime((F64)val);
	LLWLParamManager::getInstance()->mAnimator.deactivate();

	// then call update once
	LLWLParamManager::getInstance()->mAnimator.update(
		LLWLParamManager::getInstance()->mCurParams);
}

void LLFloaterDayCycle::onKeyTimeMoved(LLUICtrl* ctrl, void* userData)
{
	LLComboBox* comboBox = sDayCycle->getChild<LLComboBox>("WLKeyPresets");
	LLMultiSliderCtrl* sldr = sDayCycle->getChild<LLMultiSliderCtrl>("WLDayCycleKeys");
	LLSpinCtrl* hourSpin = sDayCycle->getChild<LLSpinCtrl>("WLCurKeyHour");
	LLSpinCtrl* minSpin = sDayCycle->getChild<LLSpinCtrl>("WLCurKeyMin");

	if(sldr->getValue().size() == 0) {
		return;
	}

	// make sure we have a slider
	const std::string& curSldr = sldr->getCurSlider();
	if(curSldr == "") {
		return;
	}

	F32 time = sldr->getCurSliderValue();

	// check to see if a key exists
	std::string presetName = sSliderToKey[curSldr].presetName;
	sSliderToKey[curSldr].time = time;

	// if it exists, turn on check box
	comboBox->selectByValue(presetName);

	// now set the spinners
	F32 hour = (F32)((S32)time);
	F32 min = (time - hour) * 60;

	// handle imprecision
	if(min >= 59) {
		min = 0;
		hour += 1;
	}

	hourSpin->set(hour);
	minSpin->set(min);

	syncTrack();

}

void LLFloaterDayCycle::onKeyTimeChanged(LLUICtrl* ctrl, void* userData)
{
	// if no keys, skipped
	if(sSliderToKey.size() == 0) {
		return;
	}

	LLMultiSliderCtrl* sldr = sDayCycle->getChild<LLMultiSliderCtrl>( 
		"WLDayCycleKeys");
	LLSpinCtrl* hourSpin = sDayCycle->getChild<LLSpinCtrl>( 
		"WLCurKeyHour");
	LLSpinCtrl* minSpin = sDayCycle->getChild<LLSpinCtrl>( 
		"WLCurKeyMin");

	F32 hour = hourSpin->get();
	F32 min = minSpin->get();
	F32 val = hour + min / 60.0f;

	const std::string& curSldr = sldr->getCurSlider();
	sldr->setCurSliderValue(val, TRUE);
	F32 time = sldr->getCurSliderValue() / sHoursPerDay;

	// now set the key's time in the sliderToKey map
	std::string presetName = sSliderToKey[curSldr].presetName;
	sSliderToKey[curSldr].time = time;

	syncTrack();
}

void LLFloaterDayCycle::onKeyPresetChanged(LLUICtrl* ctrl, void* userData)
{
	// get the time
	LLComboBox* comboBox = sDayCycle->getChild<LLComboBox>( 
		"WLKeyPresets");
	LLMultiSliderCtrl* sldr = sDayCycle->getChild<LLMultiSliderCtrl>( 
		"WLDayCycleKeys");

	// do nothing if no sliders
	if(sldr->getValue().size() == 0) {
		return;
	}

	// change the map
	std::string newPreset(comboBox->getSelectedValue().asString());
	const std::string& curSldr = sldr->getCurSlider();

	// if null, don't use
	if(curSldr == "") {
		return;
	}

	sSliderToKey[curSldr].presetName = newPreset;

	syncTrack();
}

void LLFloaterDayCycle::onTimeRateChanged(LLUICtrl* ctrl, void* userData)
{
	// get the time
	LLSpinCtrl* secSpin = sDayCycle->getChild<LLSpinCtrl>( 
		"WLLengthOfDaySec");

	LLSpinCtrl* minSpin = sDayCycle->getChild<LLSpinCtrl>( 
		"WLLengthOfDayMin");

	LLSpinCtrl* hourSpin = sDayCycle->getChild<LLSpinCtrl>( 
		"WLLengthOfDayHour");

	F32 hour;
	hour = (F32)hourSpin->getValue().asReal();
	F32 min;
	min = (F32)minSpin->getValue().asReal();
	F32 sec;
	sec = (F32)secSpin->getValue().asReal();

	F32 time = 60.0f * 60.0f * hour + 60.0f * min + sec;
	if(time <= 0) {
		time = 1;
	}
	LLWLParamManager::getInstance()->mDay.mDayRate = time;

	syncTrack();
}

void LLFloaterDayCycle::onAddKey(void* userData)
{
	LLComboBox* comboBox = sDayCycle->getChild<LLComboBox>( 
		"WLKeyPresets");
	LLMultiSliderCtrl* kSldr = sDayCycle->getChild<LLMultiSliderCtrl>( 
		"WLDayCycleKeys");
	LLMultiSliderCtrl* tSldr = sDayCycle->getChild<LLMultiSliderCtrl>( 
		"WLTimeSlider");
	
	llassert_always(sSliderToKey.size() == kSldr->getValue().size());

	// get the values
	std::string newPreset(comboBox->getSelectedValue().asString());

	// add the slider key
	addSliderKey(tSldr->getCurSliderValue(), newPreset);

	syncTrack();
}

void LLFloaterDayCycle::addSliderKey(F32 time, const std::string & presetName)
{
	LLMultiSliderCtrl* kSldr = sDayCycle->getChild<LLMultiSliderCtrl>( 
		"WLDayCycleKeys");

	// make a slider
	const std::string& sldrName = kSldr->addSlider(time);
	if(sldrName == "") {
		return;
	}

	// set the key
	LLWLSkyKey newKey;
	newKey.presetName = presetName;
	newKey.time = kSldr->getCurSliderValue();

	llassert_always(sldrName != LLStringUtil::null);

	// add to map
	sSliderToKey.insert(std::pair<std::string, LLWLSkyKey>(sldrName, newKey));

	llassert_always(sSliderToKey.size() == kSldr->getValue().size());

}

void LLFloaterDayCycle::deletePreset(std::string& presetName)
{
	LLMultiSliderCtrl* sldr = sDayCycle->getChild<LLMultiSliderCtrl>("WLDayCycleKeys");

	/// delete any reference
	std::map<std::string, LLWLSkyKey>::iterator curr_preset, next_preset;
	for(curr_preset = sSliderToKey.begin(); curr_preset != sSliderToKey.end(); curr_preset = next_preset)
	{
		next_preset = curr_preset;
		++next_preset;
		if (curr_preset->second.presetName == presetName)
		{
			sldr->deleteSlider(curr_preset->first);
			sSliderToKey.erase(curr_preset);
		}
	}
}

void LLFloaterDayCycle::onDeleteKey(void* userData)
{
	if(sSliderToKey.size() == 0) {
		return;
	}

	LLComboBox* comboBox = sDayCycle->getChild<LLComboBox>( 
		"WLKeyPresets");
	LLMultiSliderCtrl* sldr = sDayCycle->getChild<LLMultiSliderCtrl>("WLDayCycleKeys");

	// delete from map
	const std::string& sldrName = sldr->getCurSlider();
	std::map<std::string, LLWLSkyKey>::iterator mIt = sSliderToKey.find(sldrName);
	sSliderToKey.erase(mIt);

	sldr->deleteCurSlider();

	if(sSliderToKey.size() == 0) {
		return;
	}

	const std::string& name = sldr->getCurSlider();
	comboBox->selectByValue(sSliderToKey[name].presetName);
	F32 time = sSliderToKey[name].time;

	LLSpinCtrl* hourSpin = sDayCycle->getChild<LLSpinCtrl>("WLCurKeyHour");
	LLSpinCtrl* minSpin = sDayCycle->getChild<LLSpinCtrl>("WLCurKeyMin");

	// now set the spinners
	F32 hour = (F32)((S32)time);
	F32 min = (time - hour) / 60;
	hourSpin->set(hour);
	minSpin->set(min);

	syncTrack();

}
