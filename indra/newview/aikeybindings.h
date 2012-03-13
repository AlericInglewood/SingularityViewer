/**
 * @file aikeybindings.h
 * @brief Implementation of AIKeyBindings
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
 *   Initial version, written by Aleric Inglewood @ SL
 */

#ifndef AIKEYBINDINGS_H
#define AIKEYBINDINGS_H

#include "stdtypes.h"
#include <string>
#include <vector>

enum AIInputMode {
  inpmode_floater,			// Any floater is active.
  inpmode_mouselook,		// Mouselook.
  inpmode_text,				// The text input bar is active.
  inpmode_sitting,			// Avatar is sitting.
  inpmode_building,			// Build floater is open (not active).
  inpmode_flying,			// Avatar is flying.
  inpmode_walking,			// Avatar is walking (or running).
  inpmode_appearance,		// Editting appearance.
  inpmode_modal,			// Modal dialog is open.
  number_of_inputmodes
};

// If you change this, then fix default_actions in aikeybindings.cpp. The order must be the same!
enum AIAction {
  action_move_forward,
  action_move_backwards,
  action_turn_left,
  action_turn_right,
  action_strafe_left,
  action_strafe_right,
  action_jump,
  action_crouch,
  action_fly_up,
  action_fly_down,
  action_toggle_fly,
  action_toggle_mouselook,
  action_fantasy,				// remove this
  number_of_actions
};

struct AIKeyEventPOD {
  MASK mModifier;
  KEY mKey;
};

class AIKeyEvent : public AIKeyEventPOD {
  public:
	AIKeyEvent(MASK modifier, KEY key) { mModifier = modifier; mKey = key; }
	MASK getModifier(void) const { return mModifier; }
	KEY getKey(void) const { return mKey; }
	std::string asString(void) const;
};

struct AIKeyBinding {
  AIAction mAction;
  AIInputMode mLastInputMode;
  std::vector<AIKeyEvent> mKeyEvents;
};

class AIKeyBindings : public std::vector<AIKeyBinding> {
  public:
	AIKeyBindings(void);
    static std::string stringFromAction(AIAction action);
	enum translation_type { abbreviated, name, description, abbreviated_long };
	static std::string stringFromInputMode(AIInputMode, translation_type kind);
	static AIInputMode getInputMode(char key);
	static AIAction actionFromDescription(std::string const& action_str);
};

extern AIKeyBindings gAIKeyBindings;

class LLUICtrl;

struct AIKeyBindingsListFilter {
  AIInputMode mInputMode;
  MASK mModifiers;
  bool mFilterInputMode;
  bool mFilterModifiers;

  AIKeyBindingsListFilter(void) : mInputMode(inpmode_floater), mModifiers(0), mFilterInputMode(false), mFilterModifiers(false) { }
};

#endif
