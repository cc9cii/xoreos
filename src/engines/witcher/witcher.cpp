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
 *  Engine class handling The Witcher
 */

#include "src/common/util.h"
#include "src/common/error.h"
#include "src/common/filelist.h"
#include "src/common/filepath.h"
#include "src/common/stream.h"
#include "src/common/configman.h"

#include "src/aurora/resman.h"
#include "src/aurora/talkman.h"

#include "src/graphics/aurora/cursorman.h"
#include "src/graphics/aurora/fontman.h"

#include "src/events/events.h"

#include "src/engines/aurora/util.h"
#include "src/engines/aurora/language.h"
#include "src/engines/aurora/loadprogress.h"
#include "src/engines/aurora/resources.h"
#include "src/engines/aurora/model.h"

#include "src/engines/witcher/witcher.h"
#include "src/engines/witcher/modelloader.h"
#include "src/engines/witcher/console.h"
#include "src/engines/witcher/campaign.h"

namespace Engines {

namespace Witcher {

const WitcherEngineProbe kWitcherEngineProbe;

const Common::UString WitcherEngineProbe::kGameName = "The Witcher";

WitcherEngineProbe::WitcherEngineProbe() {
}

WitcherEngineProbe::~WitcherEngineProbe() {
}

Aurora::GameID WitcherEngineProbe::getGameID() const {
	return Aurora::kGameIDWitcher;
}

const Common::UString &WitcherEngineProbe::getGameName() const {
	return kGameName;
}

bool WitcherEngineProbe::probe(const Common::UString &directory,
                               const Common::FileList &UNUSED(rootFiles)) const {

	// There should be a system directory
	Common::UString systemDir = Common::FilePath::findSubDirectory(directory, "system", true);
	if (systemDir.empty())
		return false;

	// The system directory has to be readable
	Common::FileList systemFiles;
	if (!systemFiles.addDirectory(systemDir))
		return false;

	// If either witcher.ini or witcher.exe exists, this should be a valid path
	return systemFiles.containsGlob(".*/witcher.(exe|ini)", true);
}

bool WitcherEngineProbe::probe(Common::SeekableReadStream &UNUSED(stream)) const {
	return false;
}

Engines::Engine *WitcherEngineProbe::createEngine() const {
	return new WitcherEngine;
}


WitcherEngine::WitcherEngine() :
	_languageText(Aurora::kLanguageInvalid), _languageVoice(Aurora::kLanguageInvalid),
	_campaign(0) {

	_console = new Console(*this);
}

WitcherEngine::~WitcherEngine() {
	delete _campaign;
}

bool WitcherEngine::detectLanguages(Aurora::GameID game, const Common::UString &target,
                                    Aurora::Platform UNUSED(platform),
                                    std::vector<Aurora::Language> &languagesText,
                                    std::vector<Aurora::Language> &languagesVoice) const {
	try {
		Common::UString dataDir = Common::FilePath::findSubDirectory(target, "data", true);
		if (dataDir.empty())
			return true;

		Common::FileList files;
		if (!files.addDirectory(dataDir))
			return true;

		for (uint i = 0; i < Aurora::kLanguageMAX; i++) {
			const uint32 langID = getLanguageID(game, (Aurora::Language) i);

			const Common::UString v1 = Common::UString::sprintf("lang_%d.key"  , langID);
			const Common::UString v2 = Common::UString::sprintf("M1_%d.key"    , langID);
			const Common::UString v3 = Common::UString::sprintf("M2_%d.key"    , langID);
			const Common::UString t  = Common::UString::sprintf("dialog_%d.tlk", langID);

			if (files.contains(v1, true) && files.contains(v2, true) && files.contains(v3, true))
				languagesVoice.push_back((Aurora::Language) i);
			if (files.contains(t, true))
				languagesText.push_back((Aurora::Language) i);
		}

	} catch (...) {
	}

	return true;
}

Campaign *WitcherEngine::getCampaign() {
	return _campaign;
}

Module *WitcherEngine::getModule() {
	if (!_campaign)
		return 0;

	return _campaign->getModule();
}

bool WitcherEngine::getLanguage(Aurora::Language &languageText, Aurora::Language &languageVoice) const {
	languageText  = _languageText;
	languageVoice = _languageVoice;

	return true;
}

bool WitcherEngine::changeLanguage() {
	Aurora::Language languageText, languageVoice;
	if (!evaluateLanguage(false, languageText, languageVoice))
		return false;

	if ((_languageText == languageText) && (_languageVoice == languageVoice))
		return true;

	try {

		loadLanguageFiles(languageText, languageVoice);

		if (_campaign)
			_campaign->refreshLocalized();

		_languageText  = languageText;
		_languageVoice = languageVoice;

	} catch (...) {

		// Roll back
		loadLanguageFiles(_languageText, _languageVoice);
		return false;

	}

	return true;
}

void WitcherEngine::run() {
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

void WitcherEngine::init() {
	LoadProgress progress(14);

	if (evaluateLanguage(true, _languageText, _languageVoice))
		status("Setting the language to %s text + %s voices",
				Aurora::getLanguageName(_languageText).c_str(),
				Aurora::getLanguageName(_languageVoice).c_str());
	else
		throw Common::Exception("Failed to detect this game's language");

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

void WitcherEngine::declareEncodings() {
	static const LanguageEncoding kLanguageEncodings[] = {
		{ Aurora::kLanguageEnglish           , Common::kEncodingUTF8 },
		{ Aurora::kLanguagePolish            , Common::kEncodingUTF8 },
		{ Aurora::kLanguageGerman            , Common::kEncodingUTF8 },
		{ Aurora::kLanguageFrench            , Common::kEncodingUTF8 },
		{ Aurora::kLanguageSpanish           , Common::kEncodingUTF8 },
		{ Aurora::kLanguageItalian           , Common::kEncodingUTF8 },
		{ Aurora::kLanguageRussian           , Common::kEncodingUTF8 },
		{ Aurora::kLanguageCzech             , Common::kEncodingUTF8 },
		{ Aurora::kLanguageHungarian         , Common::kEncodingUTF8 },
		{ Aurora::kLanguageKorean            , Common::kEncodingUTF8 },
		{ Aurora::kLanguageChineseTraditional, Common::kEncodingUTF8 },
		{ Aurora::kLanguageChineseSimplified , Common::kEncodingUTF8 }
	};

	Engines::declareEncodings(_game, kLanguageEncodings, ARRAYSIZE(kLanguageEncodings));
}

void WitcherEngine::initResources(LoadProgress &progress) {
	progress.step("Setting base directory");
	ResMan.registerDataBase(_target);

	progress.step("Adding extra archive directories");
	indexMandatoryDirectory("system"      , 0,  0, 2);
	indexMandatoryDirectory("data"        , 0,  0, 3);
	indexMandatoryDirectory("data/voices" , 0,  0, 4);

	indexMandatoryDirectory("data/modules", 0, -1, 5);

	progress.step("Loading main KEY");
	indexMandatoryArchive("main.key", 10);

	progress.step("Loading the localized base KEY");
	indexMandatoryArchive("localized.key", 50);

	// Language files at 100-102

	progress.step("Indexing extra resources");
	indexOptionalDirectory("data/movies"   , 0, -1, 150);
	indexOptionalDirectory("data/music"    , 0, -1, 151);
	indexOptionalDirectory("data/sounds"   , 0, -1, 152);
	indexOptionalDirectory("data/cutscenes", 0, -1, 153);
	indexOptionalDirectory("data/dialogues", 0, -1, 154);
	indexOptionalDirectory("data/fx"       , 0, -1, 155);
	indexOptionalDirectory("data/meshes"   , 0, -1, 156);
	indexOptionalDirectory("data/quests"   , 0, -1, 157);
	indexOptionalDirectory("data/scripts"  , 0, -1, 158);
	indexOptionalDirectory("data/templates", 0, -1, 159);
	indexOptionalDirectory("data/textures" , 0, -1, 160);

	progress.step("Indexing Windows-specific resources");
	indexMandatoryArchive("witcher.exe", 250);

	progress.step("Indexing override files");
	indexOptionalDirectory("data/override", 0, 0, 500);

	loadLanguageFiles(progress, _languageText, _languageVoice);

	progress.step("Registering file formats");
	registerModelLoader(new WitcherModelLoader);
	FontMan.setFormat(Graphics::Aurora::kFontFormatTTF);
}

void WitcherEngine::initCursors() {
	CursorMan.add("cursor0" , "default"  , "up"  );
	CursorMan.add("cursor1" , "default"  , "down");

	CursorMan.setDefault("default", "up");
}

void WitcherEngine::initConfig() {
}

void WitcherEngine::initGameConfig() {
	ConfigMan.setString(Common::kConfigRealmGameTemp, "WITCHER_moduleDir",
		Common::FilePath::findSubDirectory(_target, "data/modules", true));
}

void WitcherEngine::unloadLanguageFiles() {
	TalkMan.removeTable(_languageTLK);

	std::list<Common::ChangeID>::iterator res;
	for (res = _languageResources.begin(); res != _languageResources.end(); ++res)
		deindexResources(*res);

	_languageResources.clear();
}

void WitcherEngine::loadLanguageFiles(LoadProgress &progress,
		Aurora::Language langText, Aurora::Language langVoice) {

	progress.step(Common::UString::sprintf("Indexing language files (%s text + %s voices)",
				Aurora::getLanguageName(langText).c_str(), Aurora::getLanguageName(langVoice).c_str()));

	loadLanguageFiles(langText, langVoice);
}

void WitcherEngine::loadLanguageFiles(Aurora::Language langText, Aurora::Language langVoice) {
	unloadLanguageFiles();
	declareTalkLanguage(_game, langText);

	Common::UString archive;

	Common::ChangeID change;

	_languageResources.push_back(Common::ChangeID());
	archive = Common::UString::sprintf("lang_%d.key", getLanguageID(_game, langVoice));
	indexMandatoryArchive(archive, 100, &_languageResources.back());

	_languageResources.push_back(Common::ChangeID());
	archive = Common::UString::sprintf("M1_%d.key", getLanguageID(_game, langVoice));
	indexMandatoryArchive(archive, 101, &_languageResources.back());

	_languageResources.push_back(Common::ChangeID());
	archive = Common::UString::sprintf("M2_%d.key", getLanguageID(_game, langVoice));
	indexMandatoryArchive(archive, 102, &_languageResources.back());

	archive = Common::UString::sprintf("dialog_%d", getLanguageID(_game, langText));
	TalkMan.addTable(archive, "", false, 0, &_languageTLK);
}

void WitcherEngine::deinit() {
}

void WitcherEngine::playIntroVideos() {
	playVideo("publisher");
	playVideo("developer");
	playVideo("engine");
	playVideo("intro");
	playVideo("title");
}

void WitcherEngine::main() {
	_campaign = new Campaign(*_console);

	const std::list<CampaignDescription> &campaigns = _campaign->getCampaigns();
	if (campaigns.empty())
		error("No campaigns found");

	// Find the original The Witcher campaign
	const CampaignDescription *witcherCampaign = 0;
	for (std::list<CampaignDescription>::const_iterator c = campaigns.begin(); c != campaigns.end(); ++c)
		if (c->tag == "thewitcher")
			witcherCampaign = &*c;

	// If that's not available, load the first one found
	if (!witcherCampaign)
		witcherCampaign = &*campaigns.begin();

	_campaign->load(*witcherCampaign);
	_campaign->run();
	_campaign->clear();

	delete _campaign;
	_campaign = 0;
}

} // End of namespace Witcher

} // End of namespace Engines
