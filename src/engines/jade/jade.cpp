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
 *  Engine class handling Jade Empire
 */

#include "src/common/util.h"
#include "src/common/filelist.h"
#include "src/common/filepath.h"
#include "src/common/configman.h"

#include "src/aurora/util.h"
#include "src/aurora/resman.h"
#include "src/aurora/talkman.h"
#include "src/aurora/talktable_tlk.h"

#include "src/graphics/camera.h"

#include "src/graphics/aurora/cursorman.h"
#include "src/graphics/aurora/model.h"
#include "src/graphics/aurora/fontman.h"

#include "src/sound/sound.h"

#include "src/events/events.h"

#include "src/engines/aurora/util.h"
#include "src/engines/aurora/language.h"
#include "src/engines/aurora/loadprogress.h"
#include "src/engines/aurora/resources.h"
#include "src/engines/aurora/model.h"
#include "src/engines/aurora/camera.h"

#include "src/engines/jade/gui/main/main.h"

#include "src/engines/jade/jade.h"
#include "src/engines/jade/modelloader.h"
#include "src/engines/jade/console.h"
#include "src/engines/jade/module.h"

namespace Engines {

namespace Jade {

const JadeEngineProbe kJadeEngineProbe;

const Common::UString JadeEngineProbe::kGameName = "Jade Empire";

JadeEngineProbe::JadeEngineProbe() {
}

JadeEngineProbe::~JadeEngineProbe() {
}

Aurora::GameID JadeEngineProbe::getGameID() const {
	return Aurora::kGameIDJade;
}

const Common::UString &JadeEngineProbe::getGameName() const {
	return kGameName;
}

bool JadeEngineProbe::probe(const Common::UString &UNUSED(directory),
                            const Common::FileList &rootFiles) const {

	// If the launcher binary is found, this should be a valid path
	if (rootFiles.contains("/JadeEmpire.exe", true))
		return true;

	return false;
}

bool JadeEngineProbe::probe(Common::SeekableReadStream &UNUSED(stream)) const {
	return false;
}

Engines::Engine *JadeEngineProbe::createEngine() const {
	return new JadeEngine;
}


JadeEngine::JadeEngine() : _language(Aurora::kLanguageInvalid),
	_module(0) {

	_console = new Console(*this);
}

JadeEngine::~JadeEngine() {
	delete _module;
}

bool JadeEngine::detectLanguages(Aurora::GameID game, const Common::UString &target,
                                 Aurora::Platform UNUSED(platform),
                                 std::vector<Aurora::Language> &languages) const {
	try {
		Common::FileList files;
		if (!files.addDirectory(target))
			return true;

		Common::UString tlk = files.findFirst("dialog.tlk", true);
		if (tlk.empty())
			return true;

		uint32 languageID = Aurora::TalkTable_TLK::getLanguageID(tlk);
		if (languageID == Aurora::kLanguageInvalid)
			return true;

		Aurora::Language language = Aurora::getLanguage(game, languageID);
		if (language == Aurora::kLanguageInvalid)
			return true;

		languages.push_back(language);

	} catch (...) {
	}

	return true;
}

bool JadeEngine::getLanguage(Aurora::Language &language) const {
	language = _language;
	return true;
}

bool JadeEngine::changeLanguage() {
	Aurora::Language language;
	if (!evaluateLanguage(false, language) || (_language != language))
		return false;

	return true;
}

Module *JadeEngine::getModule() {
	return _module;
}

void JadeEngine::run() {
	init();
	if (EventMan.quitRequested())
		return;

	CursorMan.hideCursor();
	CursorMan.set();

	playIntroVideos();
	if (EventMan.quitRequested())
		return;

	CursorMan.showCursor();

	main();

	deinit();
}

void JadeEngine::init() {
	LoadProgress progress(16);

	if (evaluateLanguage(true, _language))
		status("Setting the language to %s", Aurora::getLanguageName(_language).c_str());
	else
		warning("Failed to detect this game's language");

	progress.step("Loading user game config");
	initConfig();

	progress.step("Declare string encodings");
	declareEncodings();

	initResources(progress);
	if (EventMan.quitRequested())
		return;

	progress.step("Loading game cursors");
	initCursors();
	if (EventMan.quitRequested())
		return;

	progress.step("Initializing internal game config");
	initGameConfig();

	progress.step("Successfully initialized the engine");
}

void JadeEngine::declareEncodings() {
	static const LanguageEncoding kLanguageEncodings[] = {
		{ Aurora::kLanguageEnglish           , Common::kEncodingCP1252 },
		{ Aurora::kLanguageFrench            , Common::kEncodingCP1252 },
		{ Aurora::kLanguageGerman            , Common::kEncodingCP1252 },
		{ Aurora::kLanguageItalian           , Common::kEncodingCP1252 },
		{ Aurora::kLanguageSpanish           , Common::kEncodingCP1252 },
		{ Aurora::kLanguagePolish            , Common::kEncodingCP1250 },
		{ Aurora::kLanguageKorean            , Common::kEncodingCP949  },
		{ Aurora::kLanguageChineseTraditional, Common::kEncodingCP950  },
		{ Aurora::kLanguageChineseSimplified , Common::kEncodingCP936  },
		{ Aurora::kLanguageJapanese          , Common::kEncodingCP932  }
	};

	Engines::declareEncodings(_game, kLanguageEncodings, ARRAYSIZE(kLanguageEncodings));
	Engines::declareTalkLanguage(_game, _language);
}

void JadeEngine::initResources(LoadProgress &progress) {
	// Some new file types with the same function as old ones re-use the type ID
	ResMan.addTypeAlias(Aurora::kFileTypeBTC, Aurora::kFileTypeCRE);
	ResMan.addTypeAlias(Aurora::kFileTypeBTP, Aurora::kFileTypePLA);
	ResMan.addTypeAlias(Aurora::kFileTypeBTT, Aurora::kFileTypeTRG);

	progress.step("Setting base directory");
	ResMan.registerDataBase(_target);

	progress.step("Adding extra archive directories");
	indexMandatoryDirectory("data", 0, -1, 2);

	progress.step("Loading main KEY");
	indexMandatoryArchive("chitin.key", 10);

	progress.step("Loading global auxiliary resources");
	indexMandatoryArchive("loadscreens.mod"   , 50);
	indexMandatoryArchive("players.mod"       , 51);
	indexMandatoryArchive("global-a.rim"      , 52);
	indexMandatoryArchive("ingamemenu-a.rim"  , 53);
	indexMandatoryArchive("globalunload-a.rim", 54);
	indexMandatoryArchive("minigame-a.rim"    , 55);
	indexMandatoryArchive("miniglobal-a.rim"  , 56);
	indexMandatoryArchive("mmenu-a.rim"       , 57);

	progress.step("Indexing extra font resources");
	indexMandatoryDirectory("fonts"   , 0, -1, 100);
	progress.step("Indexing extra sound resources");
	indexMandatoryDirectory("sound"   , 0, -1, 101);
	progress.step("Indexing extra movie resources");
	indexMandatoryDirectory("movies"  , 0, -1, 102);
	progress.step("Indexing extra shader resources");
	indexMandatoryDirectory("shaderpc", 0, -1, 103);

	progress.step("Indexing override files");
	indexOptionalDirectory("override", 0, 0, 150);

	if (EventMan.quitRequested())
		return;

	progress.step("Loading main talk table");
	TalkMan.addTable("dialog", "dialogf", false, 0);

	progress.step("Registering file formats");
	registerModelLoader(new JadeModelLoader);
	FontMan.setFormat(Graphics::Aurora::kFontFormatABC);
	FontMan.addAlias("sava"   , "asian");
	FontMan.addAlias("cerigo" , "asian");
	FontMan.addAlias("fnt_gui", "asian");
}

void JadeEngine::initCursors() {
	CursorMan.add("ui_cursor32", "default");

	CursorMan.setDefault("default");
}

void JadeEngine::initConfig() {
}

void JadeEngine::initGameConfig() {
	ConfigMan.setString(Common::kConfigRealmGameTemp, "JADE_moduleDir",
		Common::FilePath::findSubDirectory(_target, "data", true));
}

void JadeEngine::deinit() {
}

void JadeEngine::playIntroVideos() {
	playVideo("black");
	playVideo("publisher");
	playVideo("bwlogo");
	playVideo("graymatr");
	playVideo("attract");
}

void JadeEngine::main() {
	_module = new Module(*_console);

	_module->load("j01_town");
	_module->run();
	_module->clear();

	delete _module;
	_module = 0;
}

} // End of namespace Jade

} // End of namespace Engines
