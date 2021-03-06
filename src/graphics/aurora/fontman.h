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
 *  The Aurora font manager.
 */

#ifndef GRAPHICS_AURORA_FONTMAN_H
#define GRAPHICS_AURORA_FONTMAN_H

#include <map>

#include "src/graphics/types.h"

#include "src/common/types.h"
#include "src/common/singleton.h"
#include "src/common/mutex.h"
#include "src/common/ustring.h"

namespace Graphics {

class Font;

namespace Aurora {

/** Identifier used for the monospaced system font. */
extern const char *kSystemFontMono;

/** The format of a font. */
enum FontFormat {
	kFontFormatUnknown = 0, ///< Unknown font format.
	kFontFormatTexture    , ///< Textured font, used by NWN and KotOR/KotOR2
	kFontFormatABC        , ///< ABC/SBM font, used by Jade Empire.
	kFontFormatTTF          ///< TTF font, used by NWN2.
};

/** A managed font, storing how often it's referenced. */
struct ManagedFont {
	Font *font;
	uint32 referenceCount;

	ManagedFont(Font *f);
	~ManagedFont();
};

typedef std::map<Common::UString, ManagedFont *> FontMap;

/** A handle to a font. */
struct FontHandle {
	bool empty;
	FontMap::iterator it;

	FontHandle();
	FontHandle(FontMap::iterator &i);
	FontHandle(const FontHandle &right);
	~FontHandle();

	FontHandle &operator=(const FontHandle &right);

	void clear();

	const Common::UString &getFontName() const;
	Font &getFont() const;
};

/** The global Aurora font manager. */
class FontManager : public Common::Singleton<FontManager> {
public:
	FontManager();
	~FontManager();

	void clear();

	void setFormat(FontFormat format);

	/** Add an alias for a specific font name. */
	void addAlias(const Common::UString &alias, const Common::UString &realName);

	FontHandle get(FontFormat format, Common::UString name, int height = 0);
	FontHandle get(Common::UString name, int height = 0);

	void release(FontHandle &handle);

private:
	FontFormat _format;

	std::map<Common::UString, Common::UString> _aliases;

	FontMap _fonts;

	Common::Mutex _mutex;

	static ManagedFont *createFont(FontFormat format,
			const Common::UString &name, int height);
};

} // End of namespace Aurora

} // End of namespace Graphics

/** Shortcut for accessing the font manager. */
#define FontMan Graphics::Aurora::FontManager::instance()

#endif // GRAPHICS_AURORA_FONTMAN_H
