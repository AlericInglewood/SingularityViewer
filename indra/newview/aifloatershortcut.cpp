/**
 * @file aifloatershortcut.cpp
 * @brief Implementation of AIFloaterShortcut
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
 *   10/03/2012
 *   Initial version, written by Aleric Inglewood @ SL
 */

#include "linden_common.h"
#include "aifloatershortcut.h"
#include "lluictrlfactory.h"

AIFloaterShortcut::AIFloaterShortcut(LLSD const& seed)
{
  DoutEntering(dc::notice, "AIFloaterShortcut::AIFloaterShortcut(" << seed << ")");
  LLUICtrlFactory::getInstance()->buildFloater(this, "floater_shortcut.xml");
}

BOOL AIFloaterShortcut::postBuild(void)
{
  DoutEntering(dc::notice, "AIFloaterShortcut::postBuild()");
  return TRUE;
}

void AIFloaterShortcut::onClose(bool app_quitting)
{
  DoutEntering(dc::notice, "AIFloaterShortcut::onClose(" << (app_quitting ? "true" : "false") << ")");
  // Cancel any unsaved changes before closing. 
  // Note: when closed due to the OK button this amounts to a no-op.
  cancel();
  LLFloater::onClose(app_quitting);
}

void AIFloaterShortcut::cancel(void)
{
}

