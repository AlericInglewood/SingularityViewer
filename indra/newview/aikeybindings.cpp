/**
 * @file aikeybindings.cpp
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
 *   22/02/2012
 *   - Initial version, written by Aleric Inglewood @ SL
 */

// This design of key bindings and (input)modes is intended as follows:
//
// An 'event' is a key (keyboard or mouse button) together with one or
// more modifier keys (there are three (or four if we add the Super key)
// modifiers, so each key can represent eight (sixteen) events).
//
// There are several, ordered, input modes. An event is bound to an
// action for a given input mode. The first input mode (top down)
// that has a given event bound is used, starting at the active mode.
//
// For example, let Event1, Event2 etc represent some given modifier(s)+key,
// and InputMode1, InputMode2 etc represent input modes. Let the following
// table be the binding for ActionXY, where X is the event and Y is the
// input mode.
//
//            Event1    Event2
// InputMode1 Action11
// InputMode2           Action22
// InputMode3 Action13
//
// Then in input mode 1, Event1 results in Action11, and Event2 results in Action22.
// In input mode 2, Event 1 result in Action13 and Event2 results in Action22,
// while in input mode 3, Event1 results in Action13 and Event2 is not bound.
//
// The reason that this is powerful is because this way the top input modes
// are guaranteed to eat events if they are active: their binding prevails.
// It allows one to set 'general' bindings by starting at the bottom and
// then overriding special cases for the higher input modes.
//
// Obviously, the order of the input modes is very important here.
// This order is (see AIInputMode):
//
// inpmode_floater
// inpmode_mouselook
// inpmode_text
// inpmode_sitting
// inpmode_building
// inpmode_flying
// inpmode_walking
// inpmode_appearance
// inpmode_modal
//
// This means that general movement bindings should be set for inpmode_walking,
// causing inpmode_flying to automatically inherit them, with the possibility
// to override them. In text input mode, any non-modifier (except shift) key
// that isn't a special key (like mouse, or home, pageup, F3 etc) is caught
// by default by inpmode_text. In mouselook there is never input, so it's ok
// that inpmode_mouselook shadows inpmode_text. This order however allows to
// bind non-typing events in mouselook that aren't inherited during text
// input, which is correct, because at that moment the mode is never perceived
// as mouselook. inpmode_appearance is mostly used for camera movements that
// should still work even in appearance mode. Most of those actions will
// be bound to modifier-events, so not caught by inpmode_floater, even if
// the appearance floater is active. It could be argued that inpmode_floater
// should be at the bottom, as you shouldn't be able to do any movements
// or camera movements while a (modal) floater is active, but in some cases
// it might be cool to BE able to move the avatar / camera while a floater
// is open - as long as the floater doesn't catch the events.
//
#include "linden_common.h"
#include "aikeybindings.h"
#include "indra_constants.h"
#include "lluictrl.h"
#include "llkeyboard.h"

struct InputMode {
  AIInputMode mInputMode;
  char mAbbreviation;
  char const* mName;
  char const* mDescription;

  std::string abbreviation(void) const { return std::string(1, mAbbreviation); }
  std::string name(void) const { return mName; }
  std::string description(void) const { return mDescription; }
  std::string long_abbreviation(void) const;
};

static InputMode const input_modes[number_of_inputmodes] = {
  { inpmode_floater,    'U', "User Interface", "A floater is active", },
  { inpmode_mouselook,  'M', "Mouselook",      "In Mouselook", },
  { inpmode_text,       'T', "Text input",     "Active Text input", },
  { inpmode_sitting,    'S', "Sitting",        "Avatar is Sitting", },
  { inpmode_building,   'B', "Building",       "Build floater is open", },
  { inpmode_flying,     'F', "Flying",         "Avatar is Flying", },
  { inpmode_walking,    'W', "Walking",        "Avatar is Walking/running", },
  { inpmode_appearance, 'A', "Appearance",     "Appearance floater is open", },
  { inpmode_modal,      'D', "Dialog",         "A modal Dialog is open", }
};

int const max_default_events_per_action = 4;

// Note: this is a POD because we use it to initialize gAIKeyBindings from it before reaching main()
// and this way it's guaranteed that default_actions[] is fully initialized before calling
// AIKeyBindings::AIKeyBindings().
struct ActionDefaultPOD {
  char const* mDescription;									// Human readable description (ie for the preferences floater).
  AIInputMode mLastInputMode;								// The default input mode below which this action is not bound.
  AIKeyEventPOD mKeyEvent[max_default_events_per_action];	// Default events bound to this action.
};

static ActionDefaultPOD const default_actions[] = {
  // Description        Input mode          Event1                        Event2                        Event3                        Event4
  { "Move forward",     inpmode_walking,  { { MASK_NONE, KEY_UP },        { MASK_NONE, 'w' },           { MASK_SHIFT, KEY_UP },       { MASK_SHIFT, 'w' } } },           // action_move_forward
  { "Move backwards",   inpmode_walking,  { { MASK_NONE, KEY_DOWN },      { MASK_NONE, 's' },           { MASK_SHIFT, KEY_DOWN },     { MASK_SHIFT, 's' } } },           // action_move_backwards
  { "Turn left",        inpmode_walking,  { { MASK_NONE, KEY_LEFT },      { MASK_NONE, 'a' } } },           // action_turn_left
  { "Turn right",       inpmode_walking,  { { MASK_NONE, KEY_RIGHT },     { MASK_NONE, 'd' } } },           // action_turn_right
  { "Strafe left",      inpmode_walking,  { { MASK_SHIFT, KEY_LEFT },     { MASK_SHIFT, 'a' } } },          // action_strafe_left
  { "Strafe right",     inpmode_walking,  { { MASK_SHIFT, KEY_RIGHT },    { MASK_SHIFT, 'd' } } },          // action_strafe_right
  { "Jump",             inpmode_walking,  { { MASK_NONE, 'e' },           { MASK_NONE, KEY_PAGE_UP } } },   // action_jump
  { "Crouch down",      inpmode_walking,  { { MASK_NONE, 'c' },           { MASK_NONE, KEY_PAGE_DOWN } } }, // action_crouch
  { "Fly up",           inpmode_flying,   { { MASK_NONE, 'e' },           { MASK_NONE, KEY_PAGE_UP } } },   // action_fly_up
  { "Fly down",         inpmode_flying,   { { MASK_NONE, 'c' },           { MASK_NONE, KEY_PAGE_DOWN } } }, // action_fly_down
  { "Toggle flying",    inpmode_walking,  { { MASK_NONE, 'f' } } },                                         // action_toggle_fly
  { "Toggle mouselook", inpmode_walking,  { { MASK_NONE, 'm' } } },                                         // action_toggle_mouselook
  { "Fantasy action",   inpmode_floater,  { { MASK_ALT|MASK_CONTROL, KEY_HYPHEN }, {MASK_NORMALKEYS, KEY_BACKSPACE}}},  // action_fantasy
};

AIKeyBindings::AIKeyBindings(void)
{
  llassert(gAIKeyBindings.empty());
  for (int action = 0; action < number_of_actions; ++action)
  {
	AIKeyBinding key_binding;
	key_binding.mAction = static_cast<AIAction>(action);
	key_binding.mLastInputMode = default_actions[action].mLastInputMode;
	for (int key = 0; key < max_default_events_per_action; ++key)
	{
	  AIKeyEventPOD const& default_key_event(default_actions[action].mKeyEvent[key]);
	  MASK modifier = default_key_event.mModifier;
	  KEY key = default_key_event.mKey ? default_key_event.mKey : KEY_NONE;
	  if (modifier || key != KEY_NONE)
	  {
		  key_binding.mKeyEvents.push_back(AIKeyEvent(modifier, key));
	  }
	}
	gAIKeyBindings.push_back(key_binding);
  }
}

// Global key bindings.
AIKeyBindings gAIKeyBindings;

//static
std::string AIKeyBindings::stringFromAction(AIAction action)
{
  //TODO: Add translation
  return default_actions[action].mDescription;
}

//static
AIAction AIKeyBindings::actionFromDescription(std::string const& description)
{
  int action;
  for (action = 0; action < number_of_actions; ++action)
	if (default_actions[action].mDescription == description)
	  break;
  return static_cast<AIAction>(action);
}

//static
std::string AIKeyBindings::stringFromInputMode(AIInputMode input_mode, translation_type kind)
{
  switch(kind)
  {
	case abbreviated:
	  return std::string(1, input_modes[input_mode].mAbbreviation);
	case name:
	  return input_modes[input_mode].mName;
	case description:
	  return input_modes[input_mode].mDescription;
	case abbreviated_long:
	{
	  std::string result;
	  for (int i = 0; i <= input_mode; ++i)
	  {
		result += input_modes[i].mAbbreviation;
	  }
	  return result;
	}
  }
  // Never reached, but needed for stupid compiler warning.
  return "UNKNOWN";
}

//static
AIInputMode AIKeyBindings::getInputMode(char key)
{
  for (int i = 0; i < number_of_inputmodes; ++i)
	if (input_modes[i].mAbbreviation == key)
	  return static_cast<AIInputMode>(i);
  // Never reached.
  llassert(false);
  return inpmode_floater;
}

std::string AIKeyEvent::asString(void) const
{
  return LLKeyboard::stringFromShortcut(mModifier, mKey);
}

