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
 *  Generic Aurora engines (debug) console.
 */

#ifndef ENGINES_AURORA_CONSOLE_H
#define ENGINES_AURORA_CONSOLE_H

#include <boost/function.hpp>

#include "src/common/types.h"
#include "src/common/error.h"
#include "src/common/ustring.h"
#include "src/common/file.h"

#include "src/events/types.h"
#include "src/events/notifyable.h"

#include "src/graphics/types.h"
#include "src/graphics/guifrontelement.h"

#include "src/graphics/aurora/types.h"
#include "src/graphics/aurora/fonthandle.h"

namespace Common {
	class ReadLine;
}

namespace Engines {

class Engine;

class ConsoleWindow : public Graphics::GUIFrontElement, public Events::Notifyable {
public:
	ConsoleWindow(const Common::UString &font, uint32 lines, uint32 history,
	              int fontHeight = 0);
	~ConsoleWindow();


	void show();
	void hide();

	void showPrompt();
	void hidePrompt();

	bool isIn(float x, float y) const;
	bool isIn(float x, float y, float z) const;


	// Dimensions

	float getWidth () const;
	float getHeight() const;
	float getContentWidth () const;
	float getContentHeight() const;

	uint32 getLines  () const;
	uint32 getColumns() const;


	// Input

	void setPrompt(const Common::UString &prompt);
	void setInput(const Common::UString &input, uint32 cursorPos, bool overwrite);


	// Output

	void clear();
	void print(const Common::UString &line);
	bool setRedirect(Common::UString redirect = "");


	// Highlight

	void clearHighlight();

	void startHighlight(int x, int y);
	void stopHighlight(int x, int y);

	void highlightWord(int x, int y);
	void highlightLine(int x, int y);

	Common::UString getHighlight() const;


	// Scrolling

	void scrollUp(uint32 n = 1);
	void scrollDown(uint32 n = 1);
	void scrollTop();
	void scrollBottom();


	// Renderable
	void calculateDistance();
	void render(Graphics::RenderPass pass);


private:
	Graphics::Aurora::FontHandle _font;

	Graphics::Aurora::Text    *_prompt;
	Graphics::Aurora::GUIQuad *_cursor;
	Graphics::Aurora::GUIQuad *_highlight;

	uint32 _historySizeMax;
	uint32 _historySizeCurrent;
	std::list<Common::UString> _history;

	uint32 _historyStart;

	std::vector<Graphics::Aurora::Text *> _lines;
	Graphics::Aurora::Text *_input;


	Common::UString _inputText;
	uint32 _cursorPosition;
	bool _overwrite;


	float _lineHeight;

	float _x;
	float _y;
	float _width;
	float _height;

	bool _cursorBlinkState;
	uint32 _lastCursorBlink;

	float _scrollbarLength;
	float _scrollbarPosition;

	uint32 _highlightX;
	uint32 _highlightY;
	 int32 _highlightLength;

	Common::DumpFile _logFile;
	Common::DumpFile _redirect;


	void recalcCursor();
	void redrawLines();

	void printLine(const Common::UString &line);

	bool openLogFile();
	bool openLogFile(const Common::UString &file);
	void closeLogFile();

	void updateHighlight();
	bool getPosition(int cursorX, int cursorY, float &x, float &y);
	void highlightClip(uint32 &x, uint32 &y) const;

	void updateScrollbarLength();
	void updateScrollbarPosition();

	void notifyResized(int oldWidth, int oldHeight, int newWidth, int newHeight);

	static uint32 findWordStart(const Common::UString &line, uint32 pos);
	static uint32 findWordEnd  (const Common::UString &line, uint32 pos);
};

class Console {
public:
	Console(Engine &engine, const Common::UString &font, int fontHeight = 0);
	virtual ~Console();

	void show();
	void hide();

	bool isVisible() const;

	float  getWidth  () const;
	float  getHeight () const;
	uint32 getLines  () const;
	uint32 getColumns() const;

	bool processEvent(const Events::Event &event);

	void disableCommand(const Common::UString &cmd, const Common::UString &reason = "");
	void enableCommand (const Common::UString &cmd);

	void clear();
	void print(const Common::UString &line);
	void printf(const char *s, ...);


protected:
	struct CommandLine {
		Common::UString cmd;
		Common::UString args;
	};

	typedef boost::function<void (const CommandLine &cl)> CommandCallback;


	void printException(Common::Exception &e, const Common::UString &prefix = "ERROR: ");

	bool registerCommand(const Common::UString &cmd, const CommandCallback &callback,
	                     const Common::UString &help);

	void printCommandHelp(const Common::UString &cmd);
	void printList(const std::list<Common::UString> &list, uint32 maxSize = 0);

	void setArguments(const Common::UString &cmd, const std::list<Common::UString> &args);
	void setArguments(const Common::UString &cmd);

	virtual void updateCaches();
	virtual void showCallback();

	static void splitArguments(Common::UString argLine, std::vector<Common::UString> &args);


private:
	struct Command {
		Common::UString cmd;
		Common::UString help;

		CommandCallback callback;

		bool disabled;
		Common::UString disableReason;
	};

	typedef std::map<Common::UString, Command, Common::UString::iless> CommandMap;


	Engine *_engine;

	bool _neverShown;
	bool _visible;

	Common::ReadLine *_readLine;
	ConsoleWindow *_console;

	CommandMap _commands;

	uint32 _tabCount;
	bool _printedCompleteWarning;

	 int8  _lastClickCount;
	uint8  _lastClickButton;
	uint32 _lastClickTime;
	 int32 _lastClickX;
	 int32 _lastClickY;


	std::list<Common::UString> _videos;
	std::list<Common::UString> _sounds;

	uint32 _maxSizeVideos;
	uint32 _maxSizeSounds;


	void updateVideos();
	void updateSounds();

	void cmdHelp       (const CommandLine &cl);
	void cmdClear      (const CommandLine &cl);
	void cmdClose      (const CommandLine &cl);
	void cmdQuit       (const CommandLine &cl);
	void cmdDumpResList(const CommandLine &cl);
	void cmdDumpRes    (const CommandLine &cl);
	void cmdDumpTGA    (const CommandLine &cl);
	void cmdDump2DA    (const CommandLine &cl);
	void cmdDumpAll2DA (const CommandLine &cl);
	void cmdListVideos (const CommandLine &cl);
	void cmdPlayVideo  (const CommandLine &cl);
	void cmdListSounds (const CommandLine &cl);
	void cmdPlaySound  (const CommandLine &cl);
	void cmdSilence    (const CommandLine &cl);
	void cmdGetOption  (const CommandLine &cl);
	void cmdSetOption  (const CommandLine &cl);
	void cmdShowFPS    (const CommandLine &cl);
	void cmdListLangs  (const CommandLine &cl);
	void cmdGetLang    (const CommandLine &cl);
	void cmdSetLang    (const CommandLine &cl);
	void cmdGetString  (const CommandLine &cl);

	void updateHelpArguments();

	void printFullHelp();
	bool printHints(const Common::UString &command);

	void execute(const Common::UString &line);
};

} // End of namespace Engines

#endif // ENGINES_AURORA_CONSOLE_H
