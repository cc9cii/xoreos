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
 *  Button items used in WidgetListBox.
 */

#ifndef ENGINES_NWN_GUI_WIDGETS_WIDGETLISTITEMBUTTON_H
#define ENGINES_NWN_GUI_WIDGETS_WIDGETLISTITEMBUTTON_H

#include "src/engines/nwn/gui/widgets/listbox.h"

namespace Engines {

namespace NWN {

class Portrait;
class WidgetListItemBaseButton : public WidgetListItem {
public:
	WidgetListItemBaseButton(::Engines::GUI &gui, const Common::UString &button,
	                     float spacing = 0.0, const Common::UString &soundClick = "gui_button");
	~WidgetListItemBaseButton();

	void show();
	void hide();

	void setPosition(float x, float y, float z);

	void mouseDown(uint8 state, float x, float y);

	float getWidth () const;
	float getHeight() const;

	void setTag(const Common::UString &tag);

protected:
	bool activate();
	bool deactivate();

	Graphics::Aurora::Model *_button;

private:
	float _spacing;
	const Common::UString _sound;
};

class WidgetListItemButton : public WidgetListItemBaseButton {
public:
	WidgetListItemButton(::Engines::GUI &gui, const Common::UString &button,
	                     const Common::UString &text, const Common::UString &icon,
	                     const Common::UString &soundClick = "gui_button");
	~WidgetListItemButton();

	void show();
	void hide();

	void setPosition(float x, float y, float z);

	void setTextColor(float r, float g, float b, float a);

private:
	Graphics::Aurora::Text  *_text;
	Portrait *_icon;
};

} // End of namespace NWN

} // End of namespace Engines

#endif // ENGINES_NWN_GUI_WIDGETS_WIDGETLISTITEMBUTTON_H
