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
 *  The context holding an NWN2 campaign.
 */

#include "src/common/util.h"
#include "src/common/error.h"
#include "src/common/file.h"
#include "src/common/filepath.h"
#include "src/common/filelist.h"

#include "src/aurora/gfffile.h"

#include "src/engines/aurora/resources.h"

#include "src/engines/nwn2/campaign.h"
#include "src/engines/nwn2/console.h"

namespace Engines {

namespace NWN2 {

Campaign::Campaign(Console &console) : _console(&console), _running(false),
	_module(console, *this), _newCampaign(0) {

	findCampaigns();

	_console->setCampaign(this);
}

Campaign::~Campaign() {
	_console->setCampaign();

	clear();
}

const std::list<CampaignDescription> &Campaign::getCampaigns() const {
	return _campaigns;
}

void Campaign::findCampaigns() {
	Common::UString baseDir = ResMan.getDataBaseDir();

	Common::UString campaignBaseDir = Common::FilePath::findSubDirectory(baseDir, "campaigns", true);
	if (campaignBaseDir.empty())
		return;

	Common::FileList campaignFiles;
	if (!campaignFiles.addDirectory(campaignBaseDir, -1))
		return;

	Common::FileList camFiles;
	if (!campaignFiles.getSubList("campaign.cam", true, camFiles))
		return;

	for (Common::FileList::const_iterator c = camFiles.begin(); c != camFiles.end(); ++c) {
		CampaignDescription desc;

		desc.directory = Common::FilePath::relativize(baseDir, Common::FilePath::getDirectory(*c));
		if (!readCampaign(*c, desc))
			continue;

		_campaigns.push_back(desc);
	}
}

bool Campaign::readCampaign(const Common::UString &camFile, CampaignDescription &desc) {
	Common::File *file = new Common::File;
	if (!file->open(camFile)) {
		delete file;
		return false;
	}

	Aurora::GFFFile *gff = 0;
	try {
		gff = new Aurora::GFFFile(file, MKTAG('C', 'A', 'M', ' '));
	} catch (...) {
		return false;
	}

	gff->getTopLevel().getLocString("DisplayName", desc.name);
	gff->getTopLevel().getLocString("Description", desc.description);

	delete gff;

	return true;
}

void Campaign::clear() {
	_console->setModule();

	_module.clear();

	_currentCampaign.directory.clear();
	_currentCampaign.name.clear();
	_currentCampaign.description.clear();

	_modules.clear();
	_startModule.clear();

	_newCampaign = 0;

	ResMan.undo(_resCampaign);
}

void Campaign::load(const CampaignDescription &desc) {
	if (isRunning()) {
		// We are currently running a campaign. Schedule a safe change instead

		changeCampaign(desc);
		return;
	}

	// We are not currently running a campaign. Directly load the new campaign
	loadCampaign(desc);
}

void Campaign::loadCampaignResource(const CampaignDescription &desc) {
	if (desc.directory.empty())
		throw Common::Exception("Campaign path is empty");

	indexMandatoryDirectory(desc.directory, 0, -1, 1000, &_resCampaign);

	Aurora::GFFFile *gff = 0;
	try {
		gff = new Aurora::GFFFile("campaign", Aurora::kFileTypeCAM, MKTAG('C', 'A', 'M', ' '));
	} catch (Common::Exception &e) {
		clear();

		e.add("Failed to load campaign information file");
		throw;
	}

	if (!gff->getTopLevel().hasField("ModNames") || !gff->getTopLevel().hasField("StartModule")) {
		delete gff;
		clear();

		throw Common::Exception("Campaign information file is missing modules");
	}

	_startModule = gff->getTopLevel().getString("StartModule") + ".mod";

	const Aurora::GFFList &modules = gff->getTopLevel().getList("ModNames");
	for (Aurora::GFFList::const_iterator m = modules.begin(); m != modules.end(); ++m)
		_modules.push_back((*m)->getString("ModuleName") + ".mod");

	delete gff;
}

void Campaign::loadCampaign(const CampaignDescription &desc) {
	clear();
	loadCampaignResource(desc);

	_currentCampaign = desc;

	try {
		_module.load(_startModule);
	} catch (Common::Exception &e) {
		clear();

		e.add("Failed to load campaign's starting module");
		throw;
	}

	_console->setModule(&_module);
}

void Campaign::run() {
	_running = true;

	try {
		_module.run();
	} catch (...) {
		_running = false;
		throw;
	}

	_running = false;
}

bool Campaign::isRunning() const {
	return _running;
}

void Campaign::changeCampaign(const CampaignDescription &desc) {
	_newCampaign = &desc;
}

void Campaign::replaceCampaign() {
	if (!_newCampaign)
		return;

	const CampaignDescription *campaign = _newCampaign;

	clear();
	loadCampaignResource(*campaign);

	_module.load(_startModule);
	_console->setModule(&_module);
}

const Common::UString &Campaign::getName() const {
	return _currentCampaign.name.getString();
}

const Common::UString &Campaign::getDescription() const {
	return _currentCampaign.description.getString();
}

} // End of namespace NWN2

} // End of namespace Engines
