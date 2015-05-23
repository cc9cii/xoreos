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
 *  The race chooser in NWN CharGen.
 */
#include "src/aurora/talkman.h"

#include "src/engines/nwn/types.h"
#include "src/engines/nwn/creature.h"

#include "src/engines/nwn/gui/widgets/button.h"
#include "src/engines/nwn/gui/widgets/buttonsgroup.h"
#include "src/engines/nwn/gui/widgets/editbox.h"

#include "src/engines/nwn/gui/chargen/chargenchoices.h"
#include "src/engines/nwn/gui/chargen/charrace.h"

namespace Engines {

namespace NWN {

CharRace::CharRace(CharGenChoices &choices, ::Engines::Console *console) : CharGenBase(console) {
	_choices = &choices;
	load("cg_race");

	// TODO Implement subrace.
	getWidget("SubRaceButton", true)->setDisabled(true);

	// Init buttons and helpbox.
	_buttons = new ButtonsGroup(getEditBox("HelpBox", true));

	uint textID = 251;
	uint titleID = 1985;

	_buttons->addButton(getButton("DwarfButton", true),
	                    TalkMan.getString(titleID), TalkMan.getString(textID));
	_buttons->addButton(getButton("ElfButton", true),
	                    TalkMan.getString(++titleID), TalkMan.getString(++textID));
	_buttons->addButton(getButton("GnomeButton", true),
	                    TalkMan.getString(++titleID), TalkMan.getString(++textID));
	_buttons->addButton(getButton("HalflingButton", true),
	                    TalkMan.getString(++titleID), TalkMan.getString(++textID));
	_buttons->addButton(getButton("HalfElfButton", true),
	                    TalkMan.getString(++titleID), TalkMan.getString(++textID));
	_buttons->addButton(getButton("HalfOrcButton", true),
	                    TalkMan.getString(++titleID), TalkMan.getString(++textID));
	_buttons->addButton(getButton("HumanButton", true),
	                    TalkMan.getString(++titleID), TalkMan.getString(++textID));

	reset();
}

CharRace::~CharRace() {
	delete _buttons;
}

void CharRace::reset() {
	_buttons->setActive(getButton("HumanButton", true));

	getEditBox("HelpBox", true)->setTitle("fnt_galahad14", TalkMan.getString(481));
	getEditBox("HelpBox", true)->setText("fnt_galahad14", TalkMan.getString(485), 1.0);

	// Set human as default race.
	_choices->setCharRace(6);
}

void CharRace::hide() {
	Engines::GUI::hide();

	if (_returnCode == 1) {
		// Set previous choice if any.
		if (_choices->getCharacter().getRace() < kRaceInvalid)
			_buttons->setActive(_choices->getCharacter().getRace());
	}
}

void CharRace::callbackActive(Widget &widget) {
	if (widget.getTag() == "OkButton") {
		_choices->setCharRace(_buttons->getChoice());
		_returnCode = 2;
		return;
	}

	if (widget.getTag() == "CancelButton") {
		_returnCode = 1;
		return;
	}

	_buttons->setActive((WidgetButton *) &widget);
}

} // End of namespace NWN

} // End of namespace Engines
