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

#include <cstdarg>
#include <cstdio>

#include <boost/bind.hpp>

#include "src/common/util.h"
#include "src/common/strutil.h"
#include "src/common/filepath.h"
#include "src/common/readline.h"
#include "src/common/configman.h"

#include "src/aurora/resman.h"
#include "src/aurora/talkman.h"

#include "src/graphics/graphics.h"
#include "src/graphics/font.h"

#include "src/sound/sound.h"

#include "src/events/events.h"

#include "src/graphics/aurora/textureman.h"
#include "src/graphics/aurora/cursorman.h"
#include "src/graphics/aurora/fontman.h"
#include "src/graphics/aurora/text.h"
#include "src/graphics/aurora/guiquad.h"

#include "src/engines/engine.h"

#include "src/engines/aurora/console.h"
#include "src/engines/aurora/util.h"


static const uint32 kDoubleClickTime = 500;

static const char *kPrompt = "> ";

static const uint32 kCommandHistorySize = 100;
static const uint32 kConsoleHistory     = 500;
static const uint32 kConsoleLines       =  25;

namespace Engines {

ConsoleWindow::ConsoleWindow(const Common::UString &font, uint32 lines, uint32 history,
                             int fontHeight) : _font(FontMan.get(font, fontHeight)),
	_historySizeMax(history), _historySizeCurrent(0), _historyStart(0),
	_cursorPosition(0), _overwrite(false),
	_cursorBlinkState(false), _lastCursorBlink(0) {

	assert(lines >= 2);
	assert(history >= lines);

	setTag("ConsoleWindow");
	setClickable(true);

	_lineHeight = _font.getFont().getHeight() + _font.getFont().getLineSpacing();
	_height     = floorf(lines * _lineHeight);

	_prompt = new Graphics::Aurora::Text(_font, "");
	_input  = new Graphics::Aurora::Text(_font, "");

	_prompt->disableColorTokens(true);
	_input->disableColorTokens(true);

	const float cursorHeight = _font.getFont().getHeight();
	_cursor = new Graphics::Aurora::GUIQuad("", 0.0, 1.0, 0.0, cursorHeight);
	_cursor->setXOR(true);

	_highlight = new Graphics::Aurora::GUIQuad("", 0.0, 0.0, 0.0, cursorHeight);
	_highlight->setColor(1.0, 1.0, 1.0, 0.0);
	_highlight->setXOR(true);

	_lines.reserve(lines - 1);
	for (uint32 i = 0; i < (lines - 1); i++) {
		_lines.push_back(new Graphics::Aurora::Text(_font, ""));
		_lines.back()->disableColorTokens(true);
	}

	notifyResized(0, 0, GfxMan.getScreenWidth(), GfxMan.getScreenHeight());

	updateScrollbarLength();
	updateScrollbarPosition();

	clearHighlight();

	calculateDistance();

	openLogFile();
}

ConsoleWindow::~ConsoleWindow() {
	_redirect.flush();
	_redirect.close();

	for (std::vector<Graphics::Aurora::Text *>::iterator l = _lines.begin();
	     l != _lines.end(); ++l)
		delete *l;

	delete _highlight;
	delete _cursor;
	delete _prompt;
	delete _input;
}

void ConsoleWindow::show() {
	GfxMan.lockFrame();

	for (std::vector<Graphics::Aurora::Text *>::iterator l = _lines.begin();
	     l != _lines.end(); ++l)
		(*l)->show();

	_highlight->show();
	_cursor->show();
	_prompt->show();
	_input->show();

	Graphics::GUIFrontElement::show();

	GfxMan.unlockFrame();
}

void ConsoleWindow::hide() {
	GfxMan.lockFrame();

	for (std::vector<Graphics::Aurora::Text *>::iterator l = _lines.begin();
	     l != _lines.end(); ++l)
		(*l)->hide();

	_highlight->hide();
	_cursor->hide();
	_prompt->hide();
	_input->hide();

	Graphics::GUIFrontElement::hide();

	GfxMan.unlockFrame();
}

void ConsoleWindow::showPrompt() {
	if (!isVisible())
		return;

	GfxMan.lockFrame();

	_cursor->show();
	_prompt->show();
	_input->show();

	GfxMan.unlockFrame();
}

void ConsoleWindow::hidePrompt() {
	if (!isVisible())
		return;

	GfxMan.lockFrame();

	_cursor->hide();
	_prompt->hide();
	_input->hide();

	GfxMan.unlockFrame();
}

bool ConsoleWindow::isIn(float x, float y) const {
	if ((x < _x) || (x > (_x + _width)))
		return false;
	if ((y < _y) || (y > (_y + _height)))
		return false;

	return true;
}

bool ConsoleWindow::isIn(float x, float y, float UNUSED(z)) const {
	return isIn(x, y);
}

float ConsoleWindow::getWidth() const {
	return _width;
}

float ConsoleWindow::getHeight() const {
	return _height;
}

float ConsoleWindow::getContentWidth() const {
	return _width - 15.0;
}

float ConsoleWindow::getContentHeight() const {
	return _height - _lineHeight;
}

uint32 ConsoleWindow::getLines() const {
	return _lines.size();
}

uint32 ConsoleWindow::getColumns() const {
	return floorf(getContentWidth() / _font.getFont().getWidth('m'));
}

void ConsoleWindow::setPrompt(const Common::UString &prompt) {
	GfxMan.lockFrame();

	_prompt->set(prompt);

	_input->setPosition(_x + _prompt->getWidth(), _y, -1001.0);
	recalcCursor();

	GfxMan.unlockFrame();
}

void ConsoleWindow::setInput(const Common::UString &input, uint32 cursorPos,
		bool overwrite) {

	GfxMan.lockFrame();

	_inputText      = input;
	_cursorPosition = cursorPos;
	_overwrite      = overwrite;

	_cursorBlinkState = false;
	_lastCursorBlink  = 0;

	_input->set(input);
	recalcCursor();

	GfxMan.unlockFrame();
}

void ConsoleWindow::clear() {
	GfxMan.lockFrame();

	_history.clear();
	_historySizeCurrent = 0;

	_historyStart = 0;

	updateScrollbarLength();
	updateScrollbarPosition();

	for (std::vector<Graphics::Aurora::Text *>::iterator l = _lines.begin();
	     l != _lines.end(); ++l)
		(*l)->set("");
	GfxMan.unlockFrame();
}

void ConsoleWindow::print(const Common::UString &line) {
	std::vector<Common::UString> lines;

	_font.getFont().split(line, lines, _width - 15.0, 0.0, false);
	for (std::vector<Common::UString>::iterator l = lines.begin(); l != lines.end(); ++l)
		printLine(*l);
}

void ConsoleWindow::printLine(const Common::UString &line) {
	if (_redirect.isOpen()) {
		_redirect.writeString(line);
		_redirect.writeByte('\n');
		return;
	}

	if (_logFile.isOpen()) {
		_logFile.writeString(line);
		_logFile.writeByte('\n');
		_logFile.flush();
	}

	_history.push_back(line);
	if (_historySizeCurrent >= _historySizeMax)
		_history.pop_front();
	else
		_historySizeCurrent++;

	updateScrollbarLength();
	redrawLines();
}

bool ConsoleWindow::setRedirect(Common::UString redirect) {
	_redirect.flush();
	_redirect.close();

	if (redirect.empty())
		return true;

	redirect = Common::FilePath::getUserDataFile(redirect);
	if (!_redirect.open(redirect)) {
		Common::UString error =
			Common::UString::sprintf("Failed opening file \"%s\" for writing.", redirect.c_str());

		print(error);
		return false;
	}

	return true;
}

bool ConsoleWindow::openLogFile() {
	/* Open the log file.
	 *
	 * NOTE: A log is opened by default, unless the consolelog config value
	 *       is set to an empty string or noconsolelog is set to true.
	 */
	Common::UString logFile = Common::FilePath::getUserDataDirectory() + "/console.log";
	if (ConfigMan.hasKey("consolelog"))
		logFile = ConfigMan.getString("consolelog");
	if (ConfigMan.getBool("noconsolelog", false))
		logFile.clear();

	if (logFile.empty())
		return true;

	return openLogFile(logFile);
}

bool ConsoleWindow::openLogFile(const Common::UString &file) {
	closeLogFile();

	// Create the directories in the path, if necessary
	Common::UString path = Common::FilePath::canonicalize(file);
	Common::FilePath::createDirectories(Common::FilePath::getDirectory(path));

	return _logFile.open(path);
}

void ConsoleWindow::closeLogFile() {
	_logFile.close();
}

void ConsoleWindow::updateHighlight() {
	if ((_highlightLength == 0) || (_highlightY >= kConsoleLines)) {
		_highlight->setColor(1.0, 1.0, 1.0, 0.0);
		return;
	}

	const float charWidth = _font.getFont().getWidth(' ');

	const int32 start = _highlightX;
	const int32 end   = _highlightX + _highlightLength;

	const  int32 x      = MIN(start, end);
	const uint32 length = ABS(start - end);

	_highlight->setWidth(length * charWidth);
	_highlight->setPosition(_x + x * charWidth, _y + _highlightY * _lineHeight, -1002.0);
	_highlight->setColor(1.0, 1.0, 1.0, 1.0);
}

bool ConsoleWindow::getPosition(int cursorX, int cursorY, float &x, float &y) {
	float realX, realY;
	CursorMan.toScreenCoordinates(cursorX, cursorY, realX, realY);

	x = (realX - _x) / _font.getFont().getWidth(' ');
	y = (realY - _y) / _lineHeight;

	if ((x < _x) || (x > (_x + _width)))
		return false;
	if ((y < _y) || (y > (_y + _height)))
		return false;

	return true;
}

void ConsoleWindow::highlightClip(uint32 &x, uint32 &y) const {
	y = CLIP<uint32>(y, 0, _lines.size());

	uint32 minX, maxX;
	if        (y == 0) {
		minX = _prompt->get().size();
		maxX = _prompt->get().size() + _input->get().size();
	} else {
		minX = 0;
		maxX = _lines[_lines.size() - y]->get().size();
	}

	x = CLIP(x, minX, maxX);
}

void ConsoleWindow::startHighlight(int x, int y) {
	clearHighlight();

	float lineX, lineY;
	if (!getPosition(x, y, lineX, lineY))
		return;

	_highlightX = floor(lineX);
	_highlightY = floor(lineY);

	highlightClip(_highlightX, _highlightY);

	updateHighlight();
}

void ConsoleWindow::stopHighlight(int x, int y) {
	float lineX, lineY;
	if (!getPosition(x, y, lineX, lineY))
		return;

	uint32 endX = floor(lineX);

	highlightClip(endX, _highlightY);

	_highlightLength = ((int32) endX) - ((int32) _highlightX);

	updateHighlight();
}

void ConsoleWindow::highlightWord(int x, int y) {
	clearHighlight();

	float lineX, lineY;
	if (!getPosition(x, y, lineX, lineY))
		return;

	uint32 wX = floor(lineX);
	uint32 wY = floor(lineY);

	highlightClip(wX, wY);

	const Common::UString &line = (wY == 0) ? _input->get() :
	                                          _lines[_lines.size() - wY]->get();
	const uint32 pos = (wY == 0) ? (wX - _prompt->get().size()) : wX;

	uint32 wordStart = findWordStart(line, pos);
	uint32 wordEnd   = findWordEnd  (line, pos);

	_highlightX      = (wY == 0) ? (wordStart + _prompt->get().size()) : wordStart;
	_highlightY      =  wY;
	_highlightLength = wordEnd - wordStart;

	updateHighlight();
}

void ConsoleWindow::highlightLine(int x, int y) {
	clearHighlight();

	float lineX, lineY;
	if (!getPosition(x, y, lineX, lineY))
		return;

	_highlightX = 0;
	_highlightY = floor(lineY);

	highlightClip(_highlightX, _highlightY);

	const Common::UString &line = (_highlightY == 0) ?
		_input->get() : _lines[_lines.size() - _highlightY]->get();
	_highlightLength = line.size();

	updateHighlight();
}

void ConsoleWindow::clearHighlight() {
	_highlightX      = 0;
	_highlightY      = 0;
	_highlightLength = 0;

	updateHighlight();
}

Common::UString ConsoleWindow::getHighlight() const {
	if ((_highlightLength == 0) || (_highlightY >= kConsoleLines))
		return "";

	int32 start = _highlightX;
	int32 end   = _highlightX + _highlightLength;

	if (start > end)
		SWAP(start, end);

	Common::UString line;
	if (_highlightY == 0) {
		start = start - _prompt->get().size();
		end   = end   - _prompt->get().size();
		line  = _input->get();
	} else
		line = _lines[_lines.size() - _highlightY]->get();

	start = MAX(0, start);
	end   = MAX(0, end  );

	return line.substr(line.getPosition(start), line.getPosition(end));
}

void ConsoleWindow::scrollUp(uint32 n) {
	if ((_historyStart + _lines.size()) >= _historySizeCurrent)
		return;

	_historyStart += MIN<uint32>(n, _historySizeCurrent - _lines.size() - _historyStart);

	updateScrollbarPosition();
	redrawLines();
}

void ConsoleWindow::scrollDown(uint32 n) {
	if (_historyStart == 0)
		return;

	_historyStart -= MIN(n, _historyStart);

	updateScrollbarPosition();
	redrawLines();
}

void ConsoleWindow::scrollTop() {
	if (_historySizeCurrent <= _lines.size())
		return;

	const uint32 bottom = _historySizeCurrent - _lines.size();
	if (bottom == _historyStart)
		return;

	_historyStart = bottom;

	updateScrollbarPosition();
	redrawLines();
}

void ConsoleWindow::scrollBottom() {
	if (_historyStart == 0)
		return;

	_historyStart = 0;

	updateScrollbarPosition();
	redrawLines();
}

void ConsoleWindow::calculateDistance() {
	_distance = -1000.0;
}

void ConsoleWindow::render(Graphics::RenderPass pass) {
	if (pass == Graphics::kRenderPassOpaque)
		return;

	uint32 now = EventMan.getTimestamp();
	if ((now - _lastCursorBlink) > 500) {
		_cursorBlinkState = !_cursorBlinkState;
		_lastCursorBlink = now;

		_cursor->setColor(1.0, 1.0, 1.0, _cursorBlinkState ? 1.0 : 0.0);
	}

	TextureMan.reset();
	glColor4f(0.0, 0.0, 0.0, 0.75);


	// Backdrop
	glBegin(GL_QUADS);
		glVertex2f(_x         , _y          );
		glVertex2f(_x + _width, _y          );
		glVertex2f(_x + _width, _y + _height);
		glVertex2f(_x         , _y + _height);
	glEnd();

	// Bottom edge
	glColor4f(0.0, 0.0, 0.0, 1.0);
	glBegin(GL_QUADS);
		glVertex2f(_x         , _y - 3.0);
		glVertex2f(_x + _width, _y - 3.0);
		glVertex2f(_x + _width, _y      );
		glVertex2f(_x         , _y      );
	glEnd();

	// Scrollbar background
	glColor4f(0.0, 0.0, 0.0, 1.0);
	glBegin(GL_QUADS);
		glVertex2f(_x + _width - 12.0, _y          );
		glVertex2f(_x + _width       , _y          );
		glVertex2f(_x + _width       , _y + _height);
		glVertex2f(_x + _width - 12.0, _y + _height);
	glEnd();

	// Scrollbar
	glColor4f(0.5, 0.5, 0.5, 0.5);
	glBegin(GL_QUADS);
		glVertex2f(_x + _width - 10.0, _y + 2.0 + _scrollbarPosition);
		glVertex2f(_x + _width -  2.0, _y + 2.0 + _scrollbarPosition);
		glVertex2f(_x + _width -  2.0, _y + 2.0 + _scrollbarPosition + _scrollbarLength);
		glVertex2f(_x + _width - 10.0, _y + 2.0 + _scrollbarPosition + _scrollbarLength);
	glEnd();

	glColor4f(1.0, 1.0, 1.0, 1.0);
}

void ConsoleWindow::notifyResized(int UNUSED(oldWidth), int UNUSED(oldHeight),
                                  int newWidth, int newHeight) {

	_width = newWidth;

	_x = -(newWidth  / 2.0);
	_y =  (newHeight / 2.0) - _height;

	float textY = (newHeight / 2.0) - _lineHeight;
	for (uint32 i = 0; i < _lines.size(); i++, textY -= _lineHeight)
		_lines[i]->setPosition(_x, textY, -1001.0);

	_prompt->setPosition(_x                      , _y, -1001.0);
	_input ->setPosition(_x + _prompt->getWidth(), _y, -1001.0);

	recalcCursor();
}

uint32 ConsoleWindow::findWordStart(const Common::UString &line, uint32 pos) {
	Common::UString::iterator it = line.getPosition(pos);
	if ((it == line.end()) || (*it == ' '))
		return 0;

	while ((it != line.begin()) && (*it != ' '))
		--it;

	if (*it == ' ')
		++it;

	return line.getPosition(it);
}

uint32 ConsoleWindow::findWordEnd(const Common::UString &line, uint32 pos) {
	Common::UString::iterator it = line.getPosition(pos);
	if ((it == line.end()) || (*it == ' '))
		return 0;

	while ((it != line.end()) && (*it != ' '))
		++it;

	return line.getPosition(it);
}

void ConsoleWindow::recalcCursor() {
	Common::UString input = _inputText;
	input.truncate(_cursorPosition);

	const float cursorX = _x + _prompt->getWidth() + _font.getFont().getWidth(input) - 1.0;
	_cursor->setPosition(cursorX, _y, -1002.0);

	const float cursorWidth = 1.0 + (_overwrite ? _font.getFont().getWidth(' ') : 0.0);
	_cursor->setWidth(cursorWidth);
}

void ConsoleWindow::redrawLines() {
	GfxMan.lockFrame();

	std::list<Common::UString>::reverse_iterator h = _history.rbegin();
	for (uint32 i = 0; (i < _historyStart) && (h != _history.rend()); i++, ++h);

	for (int i = _lines.size() - 1; (i >= 0) && (h != _history.rend()); i--, ++h)
		_lines[i]->set(*h);

	GfxMan.unlockFrame();
}

void ConsoleWindow::updateScrollbarLength() {
	float length = 1.0;

	if (_historySizeCurrent > 0)
		length = ((float) _lines.size()) / _historySizeCurrent;

	const float height = _height - 4.0;
	_scrollbarLength = floorf(CLIP(length * height, 8.0f, height));
}

void ConsoleWindow::updateScrollbarPosition() {
	float position = 0.0;

	int max = _historySizeCurrent - _lines.size();
	if (max > 0)
		position = ((float) _historyStart) / max;

	const float span = (_height - 4.0) - _scrollbarLength;
	_scrollbarPosition = floorf(CLIP(position * span, 0.0f, span));
}


Console::Console(Engine &engine, const Common::UString &font, int fontHeight) :
	_engine(&engine), _neverShown(true), _visible(false), _tabCount(0),
	_printedCompleteWarning(false), _lastClickCount(-1),
	_lastClickButton(0), _lastClickTime(0), _lastClickX(0), _lastClickY(0),
	_maxSizeVideos(0), _maxSizeSounds(0) {

	_readLine = new Common::ReadLine(kCommandHistorySize);
	_console  = new ConsoleWindow(font, kConsoleLines, kConsoleHistory, fontHeight);

	_readLine->historyIgnoreDups(true);

	registerCommand("help"       , boost::bind(&Console::cmdHelp       , this, _1),
			"Usage: help [<command>]\nPrint help text");
	registerCommand("clear"      , boost::bind(&Console::cmdClear      , this, _1),
			"Usage: clear\nClear the console window");
	registerCommand("close"      , boost::bind(&Console::cmdClose      , this, _1),
			"Usage: close\nClose the console window, returning to the game");
	registerCommand("quit"       , boost::bind(&Console::cmdQuit    , this, _1),
			"Usage: quit\nQuit xoreos entirely");
	registerCommand("dumpreslist", boost::bind(&Console::cmdDumpResList, this, _1),
			"Usage: dumpreslist <file>\nDump the current list of resources to file");
	registerCommand("dumpres"    , boost::bind(&Console::cmdDumpRes    , this, _1),
			"Usage: dumpres <resource>\nDump a resource to file");
	registerCommand("dumptga"    , boost::bind(&Console::cmdDumpTGA    , this, _1),
			"Usage: dumptga <resource>\nDump an image resource into a TGA");
	registerCommand("dump2da"    , boost::bind(&Console::cmdDump2DA    , this, _1),
			"Usage: dump2da <2da>\nDump a 2DA to file");
	registerCommand("dumpall2da" , boost::bind(&Console::cmdDumpAll2DA , this, _1),
			"Usage: dumpall2da\nDump all 2DA to file");
	registerCommand("listvideos" , boost::bind(&Console::cmdListVideos , this, _1),
			"Usage: listvideos\nList all available videos");
	registerCommand("playvideo"  , boost::bind(&Console::cmdPlayVideo  , this, _1),
			"Usage: playvideo <video>\nPlay the specified video");
	registerCommand("listsounds" , boost::bind(&Console::cmdListSounds , this, _1),
			"Usage: listsounds\nList all available sounds");
	registerCommand("playsound"  , boost::bind(&Console::cmdPlaySound  , this, _1),
			"Usage: playsound <sound>\nPlay the specified sound");
	registerCommand("silence"    , boost::bind(&Console::cmdSilence    , this, _1),
			"Usage: silence\nStop all playing sounds and music");
	registerCommand("getoption"  , boost::bind(&Console::cmdGetOption  , this, _1),
			"Usage: getoption <option>\nPrint the value of a config options");
	registerCommand("setoption"  , boost::bind(&Console::cmdSetOption  , this, _1),
			"Usage: setoption <option> <value>\nSet the value of a config option for this session");
	registerCommand("showfps"    , boost::bind(&Console::cmdShowFPS    , this, _1),
			"Usage: showfps <true/false>\nShow/Hide the frames-per-second display");
	registerCommand("listlangs"  , boost::bind(&Console::cmdListLangs  , this, _1),
			"Usage: listlangs\nLists all languages supported by this game version");
	registerCommand("getlang"    , boost::bind(&Console::cmdGetLang    , this, _1),
			"Usage: getlang\nPrint the current language settings");
	registerCommand("setlang"    , boost::bind(&Console::cmdSetLang    , this, _1),
			"Usage: setlang <language>\n       setlang <language_text> <language_voice>\n"
			"Change the game's current language");
	registerCommand("getstring"  , boost::bind(&Console::cmdGetString  , this, _1),
			"Usage: getstring <strref>\nGet a string from the talk manager and print it");

	_console->setPrompt(kPrompt);

	_console->print("Console ready...");
}

Console::~Console() {
	hide();

	delete _console;
	delete _readLine;
}

void Console::show() {
	if (_visible)
		return;

	if (_neverShown)
		_console->print("Type 'close' to return to the game. Type 'help' for a list of commands.");

	_console->show();
	_visible    = true;
	_neverShown = false;

	updateCaches();
	showCallback();

	EventMan.enableTextInput(true);
}

void Console::hide() {
	if (!_visible)
		return;

	_console->hide();
	_visible = false;

	EventMan.enableTextInput(false);
}

bool Console::isVisible() const {
	return _visible;
}

float Console::getWidth() const {
	return _console->getContentWidth();
}

float Console::getHeight() const {
	return _console->getContentHeight();
}

uint32 Console::getLines() const {
	return _console->getLines();
}

uint32 Console::getColumns() const {
	return _console->getColumns();
}

bool Console::processEvent(const Events::Event &event) {
	if (!isVisible())
		return false;

	if (event.type == Events::kEventMouseDown) {

		const uint8 button     = event.button.button;
		const uint8 pasteMask1 = SDL_BUTTON_MMASK;
		const uint8 pasteMask2 = SDL_BUTTON_LMASK | SDL_BUTTON_RMASK;

		// Pasting the current buffer with the middle (or left+right) mouse button
		if (((button & pasteMask1) == pasteMask1) || ((button & pasteMask2) == pasteMask2)) {
			_readLine->addInput(_console->getHighlight());
			_console->setInput(_readLine->getCurrentLine(),
					_readLine->getCursorPosition(), _readLine->getOverwrite());
			return true;
		}

		// Highlight while dragging the left mouse button
		if (button & SDL_BUTTON_LMASK) {
			_console->startHighlight(event.button.x, event.button.y);
			return true;
		}
	}

	if (event.type == Events::kEventMouseMove) {

		// Highlight while dragging the left mouse button
		if (event.motion.state & SDL_BUTTON_LMASK) {
			_console->stopHighlight(event.button.x, event.button.y);
			return true;
		}
	}

	if (event.type == Events::kEventMouseUp) {

		uint32 curTime = EventMan.getTimestamp();

		if (((curTime - _lastClickTime) < kDoubleClickTime) &&
		    (_lastClickButton == event.button.button) &&
		    (_lastClickX == event.button.x) && (_lastClickY == event.button.y))
			_lastClickCount = (_lastClickCount + 1) % 3;
		else
			_lastClickCount = 0;

		_lastClickButton = event.button.button;
		_lastClickTime   = curTime;
		_lastClickX      = event.button.x;
		_lastClickY      = event.button.y;

		if (event.button.button & SDL_BUTTON_LMASK) {
			if      (_lastClickCount == 0)
				// Stop highlighting when release the mouse
				_console->stopHighlight(event.button.x, event.button.y);
			else if (_lastClickCount == 1)
				// Click twice to highlight a word
				_console->highlightWord(event.button.x, event.button.y);
			else if (_lastClickCount == 2)
				// Click thrice to highlight the whole line
				_console->highlightLine(event.button.x, event.button.y);

			return true;
		}

	}

	if (event.type == Events::kEventKeyDown) {
		_console->clearHighlight();

		// Autocomplete with tab
		if (event.key.keysym.sym != SDLK_TAB) {
			_tabCount = 0;
			_printedCompleteWarning = false;
		} else
			_tabCount++;

		// Close the console with escape or Ctrl-D
		if ((event.key.keysym.sym == SDLK_ESCAPE) ||
		   ((event.key.keysym.sym == SDLK_d) && (event.key.keysym.mod & KMOD_CTRL))) {
			hide();
			return true;
		}

		// Ctrl-L clear the console
		if ((event.key.keysym.sym == SDLK_l) && (event.key.keysym.mod & KMOD_CTRL)) {
			clear();
			return true;
		}

		// Scroll up half a screen with Shift-PageUp
		if ((event.key.keysym.sym == SDLK_PAGEUP) && (event.key.keysym.mod & KMOD_SHIFT)) {
			_console->scrollUp(kConsoleLines / 2);
			return true;
		}

		// Scroll down half a screen with Shift-PageUp
		if ((event.key.keysym.sym == SDLK_PAGEDOWN) && (event.key.keysym.mod & KMOD_SHIFT)) {
			_console->scrollDown(kConsoleLines / 2);
			return true;
		}

		// Scroll up a line with PageUp
		if (event.key.keysym.sym == SDLK_PAGEUP) {
			_console->scrollUp();
			return true;
		}

		// Scroll down a line with PageUp
		if (event.key.keysym.sym == SDLK_PAGEDOWN) {
			_console->scrollDown();
			return true;
		}

		// Shift-Home scrolls to the top
		if ((event.key.keysym.sym == SDLK_HOME) && (event.key.keysym.mod & KMOD_SHIFT)) {
			_console->scrollTop();
			return true;
		}

		// Shift-Home scrolls to the bottom
		if ((event.key.keysym.sym == SDLK_END) && (event.key.keysym.mod & KMOD_SHIFT)) {
			_console->scrollBottom();
			return true;
		}


	} else if (event.type == Events::kEventMouseWheel) {
		// Scroll up / down using the mouse wheel

		if (event.wheel.y > 0) {
			_console->scrollUp();
			return true;
		}

		if (event.wheel.y < 0) {
			_console->scrollDown();
			return true;
		}
	}

	Common::UString command;
	if (!_readLine->processEvent(event, command))
		return false;

	_console->setInput(_readLine->getCurrentLine(),
			_readLine->getCursorPosition(), _readLine->getOverwrite());

	// Check whether we have tab-completion hints
	if (printHints(command))
		return true;

	execute(command);
	return true;
}

void Console::disableCommand(const Common::UString &cmd, const Common::UString &reason) {
	CommandMap::iterator c = _commands.find(cmd);
	if (c == _commands.end()) {
		throw Common::Exception("No such command \"%s\"", cmd.c_str());
		return;
	}

	c->second.disabled      = true;
	c->second.disableReason = reason;
}

void Console::enableCommand(const Common::UString &cmd) {
	CommandMap::iterator c = _commands.find(cmd);
	if (c == _commands.end()) {
		throw Common::Exception("No such command \"%s\"", cmd.c_str());
		return;
	}

	c->second.disabled = false;
}

void Console::execute(const Common::UString &line) {
	if (line.empty())
		return;

	// Add the line to console
	_console->print(Common::UString(kPrompt) + line);


	// Split command from redirect target

	Common::UString command;
	Common::UString redirect;
	line.split(line.findFirst('>'), command, redirect, true);

	command.trim();
	redirect.trim();


	// Split command from arguments

	CommandLine cl;

	command.split(command.findFirst(' '), cl.cmd, cl.args, true);

	cl.cmd.trim();
	cl.args.trim();


	// Find the command
	CommandMap::iterator cmd = _commands.find(cl.cmd);
	if (cmd == _commands.end()) {
		printf("Unknown command \"%s\". Type 'help' for a list of available commands.",
				cl.cmd.c_str());
		return;
	}

	if (cmd->second.disabled) {
		if (cmd->second.disableReason.empty())
			printf("Command \"%s\" is currently disabled.", cl.cmd.c_str());
		else
			printf("Command \"%s\" is currently disabled: %s.", cl.cmd.c_str(),
					cmd->second.disableReason.c_str());

		return;
	}

	// Set redirect
	if (!_console->setRedirect(redirect))
		return;

	// Execute
	_console->hidePrompt();
	cmd->second.callback(cl);
	_console->showPrompt();

	// Reset redirect
	_console->setRedirect();
}

bool Console::printHints(const Common::UString &command) {
	if (_tabCount < 2)
		return false;

	uint32 maxSize;
	uint32 count;
	const std::list<Common::UString> &hints = _readLine->getCompleteHint(maxSize, count);
	if (count == 0)
		return false;

	maxSize = MAX<uint32>(maxSize, 3) + 2;
	uint32 lineSize = getColumns() / maxSize;
	uint32 lines = count / lineSize;

	if (lines >= (kConsoleLines - 3)) {
		if (!_printedCompleteWarning)
			printf("%d completion candidates", count);

		_printedCompleteWarning = true;

		if (_tabCount < 4)
			return true;
	}

	_console->scrollBottom();
	_console->print(Common::UString(kPrompt) + " " + command);
	printList(hints, maxSize);

	_tabCount = 0;
	_printedCompleteWarning = false;

	return true;
}

void Console::clear() {
	_console->clear();
}

void Console::print(const Common::UString &line) {
	_console->print(line);
}

void Console::printf(const char *s, ...) {
	char buf[STRINGBUFLEN];
	va_list va;

	va_start(va, s);
	vsnprintf(buf, STRINGBUFLEN, s, va);
	va_end(va);

	print(buf);
}

void Console::printException(Common::Exception &e, const Common::UString &prefix) {
	Common::Exception::Stack &stack = e.getStack();

	if (stack.empty()) {
		print("FATAL ERROR");
		return;
	}

	printf("%s%s", prefix.c_str(), stack.top().c_str());

	stack.pop();

	while (!stack.empty()) {
		printf("'- Because: %s", stack.top().c_str());
		stack.pop();
	}
}

void Console::updateCaches() {
	updateVideos();
	updateSounds();
}

void Console::updateVideos() {
	_videos.clear();
	_maxSizeVideos = 0;

	std::list<Aurora::ResourceManager::ResourceID> videos;
	ResMan.getAvailableResources(Aurora::kResourceVideo, videos);

	for (std::list<Aurora::ResourceManager::ResourceID>::const_iterator v = videos.begin();
	     v != videos.end(); ++v) {

		_videos.push_back(v->name);

		_maxSizeVideos = MAX(_maxSizeVideos, _videos.back().size());
	}

	setArguments("playvideo", _videos);
}

void Console::updateSounds() {
	_sounds.clear();
	_maxSizeSounds = 0;

	std::list<Aurora::ResourceManager::ResourceID> sounds;
	ResMan.getAvailableResources(Aurora::kFileTypeWAV, sounds);

	for (std::list<Aurora::ResourceManager::ResourceID>::const_iterator s = sounds.begin();
	     s != sounds.end(); ++s) {

		_sounds.push_back(s->name);

		_maxSizeSounds = MAX(_maxSizeSounds, _sounds.back().size());
	}

	setArguments("playsound", _sounds);
}

void Console::cmdHelp(const CommandLine &cli) {
	if (cli.args.empty()) {
		printFullHelp();
		return;
	}

	printCommandHelp(cli.args);
}

void Console::cmdClear(const CommandLine &UNUSED(cl)) {
	clear();
}

void Console::cmdClose(const CommandLine &UNUSED(cl)) {
	hide();
}

void Console::cmdQuit(const CommandLine &UNUSED(cl)) {
	print("Bye...");
	EventMan.requestQuit();
}

void Console::cmdDumpResList(const CommandLine &cl) {
	if (cl.args.empty()) {
		printCommandHelp(cl.cmd);
		return;
	}

	Common::UString file = Common::FilePath::getUserDataFile(cl.args);

	if (dumpResList(file))
		printf("Dumped list of resources to file \"%s\"", file.c_str());
	else
		printf("Failed dumping list of resources to file \"%s\"", file.c_str());
}

void Console::cmdDumpRes(const CommandLine &cl) {
	if (cl.args.empty()) {
		printCommandHelp(cl.cmd);
		return;
	}

	Common::UString file = Common::FilePath::getUserDataFile(cl.args);

	if (dumpResource(cl.args, file))
		printf("Dumped resource \"%s\"to \"%s\"", cl.args.c_str(), file.c_str());
	else
		printf("Failed dumping resource \"%s\"", cl.args.c_str());
}

void Console::cmdDumpTGA(const CommandLine &cl) {
	if (cl.args.empty()) {
		printCommandHelp(cl.cmd);
		return;
	}

	Common::UString file = Common::FilePath::getUserDataFile(cl.args) + ".tga";

	if (dumpTGA(cl.args, file))
		printf("Dumped TGA \"%s\" to \"%s\"", cl.args.c_str(), file.c_str());
	else
		printf("Failed dumping TGA \"%s\"", cl.args.c_str());
}

void Console::cmdDump2DA(const CommandLine &cl) {
	if (cl.args.empty()) {
		printCommandHelp(cl.cmd);
		return;
	}

	Common::UString file = Common::FilePath::getUserDataFile(cl.args) + ".2da";

	if (dump2DA(cl.args, file))
		printf("Dumped 2DA \"%s\" to \"%s\"", cl.args.c_str(), file.c_str());
	else
		printf("Failed dumping 2DA \"%s\"", cl.args.c_str());
}

void Console::cmdDumpAll2DA(const CommandLine &UNUSED(cl)) {
	std::list<Aurora::ResourceManager::ResourceID> twoda;
	ResMan.getAvailableResources(Aurora::kFileType2DA, twoda);

	std::list<Aurora::ResourceManager::ResourceID>::const_iterator t;
	for (t = twoda.begin(); t != twoda.end(); ++t) {
		Common::UString file = Common::FilePath::getUserDataFile(t->name) + ".2da";

		if (dump2DA(t->name, file))
			printf("Dumped 2DA \"%s\" to \"%s\"", t->name.c_str(), file.c_str());
		else
			printf("Failed dumping 2DA \"%s\"", t->name.c_str());
	}
}

void Console::cmdListVideos(const CommandLine &UNUSED(cl)) {
	updateVideos();
	printList(_videos, _maxSizeVideos);
}

void Console::cmdPlayVideo(const CommandLine &cl) {
	if (cl.args.empty()) {
		printCommandHelp(cl.cmd);
		return;
	}

	playVideo(cl.args);
}

void Console::cmdListSounds(const CommandLine &UNUSED(cl)) {
	updateSounds();
	printList(_sounds, _maxSizeSounds);
}

void Console::cmdPlaySound(const CommandLine &cl) {
	if (cl.args.empty()) {
		printCommandHelp(cl.cmd);
		return;
	}

	playSound(cl.args, Sound::kSoundTypeSFX);
}

void Console::cmdSilence(const CommandLine &UNUSED(cl)) {
	SoundMan.stopAll();
}

void Console::cmdGetOption(const CommandLine &cl) {
	std::vector<Common::UString> args;
	splitArguments(cl.args, args);

	if (args.empty()) {
		printCommandHelp(cl.cmd);
		return;
	}

	printf("\"%s\" = \"%s\"", args[0].c_str(), ConfigMan.getString(args[0]).c_str());
}

void Console::cmdSetOption(const CommandLine &cl) {
	std::vector<Common::UString> args;
	splitArguments(cl.args, args);

	if (args.size() < 2) {
		printCommandHelp(cl.cmd);
		return;
	}

	ConfigMan.setCommandlineKey(args[0], args[1]);
	_engine->showFPS();

	printf("\"%s\" = \"%s\"", args[0].c_str(), ConfigMan.getString(args[0]).c_str());
}

void Console::cmdShowFPS(const CommandLine &cl) {
	if (cl.args.empty()) {
		printCommandHelp(cl.cmd);
		return;
	}

	ConfigMan.setCommandlineKey("showfps", cl.args);
	_engine->showFPS();
}

void Console::cmdListLangs(const CommandLine &UNUSED(cl)) {
	std::vector<Aurora::Language> langs;
	if (_engine->detectLanguages(langs)) {
		if (!langs.empty()) {
			printf("Available languages:");
			for (std::vector<Aurora::Language>::iterator l = langs.begin(); l != langs.end(); ++l)
				printf("- %s", Aurora::getLanguageName(*l).c_str());
		}
	}

	std::vector<Aurora::Language> langsT, langsV;
	if (_engine->detectLanguages(langsT, langsV)) {
		if (!langsT.empty()) {
			printf("Available text languages:");
			for (std::vector<Aurora::Language>::iterator l = langsT.begin(); l != langsT.end(); ++l)
				printf("- %s", Aurora::getLanguageName(*l).c_str());
		}

		if (!langsV.empty()) {
			printf("Available voice languages:");
			for (std::vector<Aurora::Language>::iterator l = langsV.begin(); l != langsV.end(); ++l)
				printf("- %s", Aurora::getLanguageName(*l).c_str());
		}
	}
}

void Console::cmdGetLang(const CommandLine &UNUSED(cl)) {
	Aurora::Language lang;
	if (_engine->getLanguage(lang))
		printf("%s", Aurora::getLanguageName(lang).c_str());

	Aurora::Language langT, langV;
	if (_engine->getLanguage(langT, langV))
		printf("%s text + %s voices", Aurora::getLanguageName(langT).c_str(),
				Aurora::getLanguageName(langV).c_str());
}

void Console::cmdSetLang(const CommandLine &cl) {
	std::vector<Common::UString> args;
	splitArguments(cl.args, args);

	if (args.size() == 1) {
		ConfigMan.setCommandlineKey("lang"     , args[0]);
		ConfigMan.setCommandlineKey("langtext" , args[0]);
		ConfigMan.setCommandlineKey("langvoice", args[0]);
	} else if (args.size() == 2) {
		ConfigMan.setCommandlineKey("langtext" , args[0]);
		ConfigMan.setCommandlineKey("langvoice", args[1]);
	} else {
		printCommandHelp(cl.cmd);
		return;
	}

	if (_engine->changeLanguage()) {
		Aurora::Language lang;
		if (_engine->getLanguage(lang))
			printf("Changed language to %s", Aurora::getLanguageName(lang).c_str());

		Aurora::Language langT, langV;
		if (_engine->getLanguage(langT, langV))
			printf("Change language to %s text + %s voices", Aurora::getLanguageName(langT).c_str(),
					Aurora::getLanguageName(langV).c_str());
	} else
		printf("Failed to change the language");
}

void Console::printCommandHelp(const Common::UString &cmd) {
	CommandMap::const_iterator c = _commands.find(cmd);
	if (c == _commands.end()) {
		printFullHelp();
		return;
	}

	print(c->second.help);
}

void Console::cmdGetString(const CommandLine &cl) {
	if (cl.args.empty()) {
		printCommandHelp(cl.cmd);
		return;
	}

	uint32 strRef = 0xFFFFFFFF;
	try {
		Common::parseString(cl.args, strRef);
	} catch (...) {
		printCommandHelp(cl.cmd);
		return;
	}

	printf("\"%s\"", TalkMan.getString(strRef).c_str());
}

void Console::printFullHelp() {
	print("Available commands (help <command> for further help on each command):");

	uint32 maxSize = 0;
	std::list<Common::UString> commands;
	for (CommandMap::const_iterator c = _commands.begin(); c != _commands.end(); ++c) {
		commands.push_back(c->second.cmd);

		maxSize = MAX(maxSize, commands.back().size());
	}

	printList(commands, maxSize);
}

void Console::printList(const std::list<Common::UString> &list, uint32 maxSize) {
	const uint32 columns = getColumns();

	// If no max size is given, go through the whole list to find it ourselves
	if (maxSize == 0)
		for (std::list<Common::UString>::const_iterator l = list.begin(); l != list.end(); ++l)
			maxSize = MAX<uint32>(maxSize, l->size());

	maxSize = MAX<uint32>(maxSize, 3);

	// Calculate the number of items per line
	uint32 lineSize = 1;
	if      (maxSize >= (columns - 2))
		maxSize  = columns;
	else if (maxSize > 0)
		lineSize = columns / (maxSize + 2);

	// Calculate the number of number of lines that won't fit into the history
	uint32 toPrint  = MIN<uint32>((kConsoleHistory - 1) * lineSize, list.size());
	uint32 linesCut = list.size() - toPrint;

	// Print a message when we cut items
	if (linesCut > 0) {
		Common::UString cutMsg = Common::UString::sprintf("(%d items cut due to history overflow)", linesCut);

		print(cutMsg);
	}

	// Move past the items we're cutting
	std::list<Common::UString>::const_iterator l = list.begin();
	for (uint32 i = 0; i < linesCut; i++)
		++l;

	// Print the lines
	while (l != list.end()) {
		Common::UString line;

		// Attach the items together that go onto one line
		for (uint32 i = 0; (i < lineSize) && (l != list.end()); i++, ++l) {
			Common::UString item = *l;

			uint32 itemSize = item.size();

			if (itemSize > maxSize) {
				item.truncate(maxSize - 3);
				item += "...";
				itemSize = maxSize;
			}

			uint32 pad = (maxSize + 2) - itemSize;
			while (pad-- > 0)
				item += ' ';

			line += item;
		}

		print(line);
	}
}

void Console::splitArguments(Common::UString argLine, std::vector<Common::UString> &args) {
	bool inQuote = false;

	if (args.empty())
		args.push_back(Common::UString());

	for (Common::UString::iterator c = argLine.begin(); c != argLine.end(); ++c) {
		if (*c == '"') {
			inQuote = !inQuote;
			continue;
		}

		if (*c == ' ' && !inQuote) {
			if (!args.back().empty())
				args.push_back(Common::UString());

			continue;
		}

		args.back() += *c;
	}

	if (args.back().empty())
		args.pop_back();
}

void Console::setArguments(const Common::UString &cmd,
		const std::list<Common::UString> &args) {

	_readLine->setArguments(cmd, args);
}

void Console::setArguments(const Common::UString &cmd) {
	_readLine->setArguments(cmd);
}

void Console::showCallback() {
}

bool Console::registerCommand(const Common::UString &cmd, const CommandCallback &callback,
                              const Common::UString &help) {


	std::pair<CommandMap::iterator, bool> result;

	result = _commands.insert(std::make_pair(cmd, Command()));
	if (!result.second)
		return false;

	result.first->second.cmd  = cmd;
	result.first->second.help = help;

	result.first->second.callback = callback;

	result.first->second.disabled = false;

	_readLine->addCommand(cmd);

	updateHelpArguments();

	return true;
}

void Console::updateHelpArguments() {
	std::list<Common::UString> commands;
	for (CommandMap::const_iterator c = _commands.begin(); c != _commands.end(); ++c)
		commands.push_back(c->second.cmd);

	_readLine->setArguments("help", commands);
}

} // End of namespace Engines
