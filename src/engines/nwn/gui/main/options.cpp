/* xoreos - A reimplementation of BioWare's Aurora engine
 *
 * xoreos is the legal property of its developers, whose names
 * can be found in the AUTHORS file distributed with this source
 * distribution.
 *
 * xoreos is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * xoreos is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with xoreos. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file
 *  The options menu.
 */

#include "src/common/version.h"

#include "src/engines/nwn/version.h"

#include "src/engines/nwn/gui/widgets/panel.h"
#include "src/engines/nwn/gui/widgets/label.h"

#include "src/engines/nwn/gui/options/game.h"
#include "src/engines/nwn/gui/options/video.h"
#include "src/engines/nwn/gui/options/sound.h"
#include "src/engines/nwn/gui/options/controls.h"

#include "src/engines/nwn/gui/main/options.h"

namespace Engines {

namespace NWN {

OptionsMenu::OptionsMenu(const Version &gameVersion) : _gameVersion(&gameVersion) {
	load("options_pregame");

	WidgetPanel *backdrop = new WidgetPanel(*this, "PNL_MAINMENU", "pnl_mainmenu");
	backdrop->setPosition(0.0, 0.0, 100.0);
	addWidget(backdrop);

	_game     = new OptionsGameMenu(true);
	_video    = new OptionsVideoMenu(true);
	_sound    = new OptionsSoundMenu(true);
	_controls = new OptionsControlsMenu(true);
}

OptionsMenu::~OptionsMenu() {
	delete _controls;
	delete _sound;
	delete _video;
	delete _game;
}

void OptionsMenu::initWidget(Widget &widget) {
	if (widget.getTag() == "BuildNumber") {
		Common::UString version = Common::UString(XOREOS_NAMEVERSION) + " v" + _gameVersion->getVersionString();

		dynamic_cast<WidgetLabel &>(widget).setText(version);
		dynamic_cast<WidgetLabel &>(widget).setColor(0.6, 0.6, 0.6, 1.0);
		return;
	}
}

void OptionsMenu::callbackActive(Widget &widget) {
	if ((widget.getTag() == "CloseButton") ||
	    (widget.getTag() == "XButton")) {
		_returnCode = 1;
		return;
	}

	if (widget.getTag() == "GameButton") {
		sub(*_game);
		return;
	}

	if (widget.getTag() == "VideoOptsButton") {
		sub(*_video);
		return;
	}

	if (widget.getTag() == "SoundButton") {
		sub(*_sound);
		return;
	}

	if (widget.getTag() == "ControlButton") {
		sub(*_controls);
		return;
	}
}

} // End of namespace NWN

} // End of namespace Engines
