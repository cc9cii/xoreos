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
 *  A cursor as used in the Aurora engines.
 */

#include "src/common/util.h"
#include "src/common/error.h"
#include "src/common/stream.h"

#include "src/aurora/types.h"
#include "src/aurora/resman.h"

#include "src/graphics/images/decoder.h"
#include "src/graphics/images/txi.h"
#include "src/graphics/images/tga.h"
#include "src/graphics/images/dds.h"
#include "src/graphics/images/winiconimage.h"

#include "src/graphics/aurora/cursor.h"
#include "src/graphics/aurora/textureman.h"
#include "src/graphics/aurora/texture.h"
#include "src/graphics/aurora/cursorman.h"

namespace Graphics {

namespace Aurora {

Cursor::Cursor(const Common::UString &name, int hotspotX, int hotspotY) :
	_name(name), _hotspotX(hotspotX), _hotspotY(hotspotY) {

	load();
}

Cursor::~Cursor() {
}

void Cursor::render() {
	TextureMan.activeTexture(0);
	TextureMan.set(_texture);

	int x, y;
	CursorMan.getPosition(x, y);

	glTranslatef(x - _hotspotX, -y - _height + _hotspotY, 0.0);

	glBegin(GL_QUADS);
		glTexCoord2f(0.0, 0.0);
		glVertex2f(0.0, 0.0);
		glTexCoord2f(1.0, 0.0);
		glVertex2f(_height, 0.0);
		glTexCoord2f(1.0, 1.0);
		glVertex2f(_height, _width);
		glTexCoord2f(0.0, 1.0);
		glVertex2f(0.0, _width);
	glEnd();
}

void Cursor::load() {
	::Aurora::FileType type;

	Common::SeekableReadStream *img = ResMan.getResource(::Aurora::kResourceCursor, _name, &type);
	if (!img)
		throw Common::Exception("No such cursor resource \"%s\"", _name.c_str());

	_hotspotX = 0;
	_hotspotY = 0;

	ImageDecoder *image = 0;

	try {

		// Loading the different image formats
		if      (type == ::Aurora::kFileTypeTGA)
			image = new TGA(*img);
		else if (type == ::Aurora::kFileTypeDDS)
			image = new DDS(*img);
		else if (type == ::Aurora::kFileTypeCUR) {
			WinIconImage *cursor = new WinIconImage(*img);

			if (_hotspotX < 0)
				_hotspotX = cursor->getHotspotX();
			if (_hotspotY < 0)
				_hotspotY = cursor->getHotspotY();

			image = cursor;
		} else
			throw Common::Exception("Unsupported cursor resource type %d", (int) type);

	} catch (...) {
		delete img;
		throw;
	}

	delete img;

	_width  = image->getMipMap(0).width;
	_height = image->getMipMap(0).height;

	TXI *txi = new TXI();
	txi->getFeatures().filter = false;

	_texture = TextureMan.add(Texture::create(image, type, txi), _name);

	_hotspotX = CLIP(_hotspotX, 0, _width  - 1);
	_hotspotY = CLIP(_hotspotY, 0, _height - 1);
}

} // End of namespace Aurora

} // End of namespace Graphics
