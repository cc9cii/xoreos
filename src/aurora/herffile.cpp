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
 *  BioWare's HERF file parsing.
 */

#include "src/common/util.h"
#include "src/common/file.h"
#include "src/common/filepath.h"
#include "src/common/stream.h"
#include "src/common/encoding.h"
#include "src/common/hash.h"

#include "src/aurora/herffile.h"
#include "src/aurora/error.h"
#include "src/aurora/util.h"
#include "src/aurora/resman.h"

namespace Aurora {

HERFFile::HERFFile(const Common::UString &fileName) : _fileName(fileName),
	_dictOffset(0xFFFFFFFF), _dictSize(0) {

	load();
}

HERFFile::~HERFFile() {
}

void HERFFile::clear() {
	_resources.clear();
}

void HERFFile::load() {
	Common::SeekableReadStream *herf = ResMan.getResource(TypeMan.setFileType(_fileName, kFileTypeNone), kFileTypeHERF);
	if (!herf)
		throw Common::Exception(Common::kOpenError);

	uint32 magic = herf->readUint32LE();
	if (magic != 0x00F1A5C0)
		throw Common::Exception("Invalid HERF file (0x%08X)", magic);

	uint32 resCount = herf->readUint32LE();

	_resources.resize(resCount);
	_iResources.resize(resCount);

	try {

		searchDictionary(*herf, resCount);
		readResList(*herf);

	if (herf->err())
		throw Common::Exception(Common::kReadError);

	} catch (Common::Exception &e) {
		delete herf;
		e.add("Failed reading HERF file");
		throw;
	}

	delete herf;
}

void HERFFile::searchDictionary(Common::SeekableReadStream &herf, uint32 resCount) {
	const uint32 dictHash = Common::hashStringDJB2("erf.dict");

	uint32 pos = herf.pos();

	for (uint32 i = 0; i < resCount; i++) {
		uint32 hash = herf.readUint32LE();
		if (hash == dictHash) {
			_dictSize   = herf.readUint32LE();
			_dictOffset = herf.readUint32LE();
			break;
		}

		herf.skip(8);
	}

	herf.seek(pos);
}

void HERFFile::readDictionary(Common::SeekableReadStream &herf, std::map<uint32, Common::UString> &dict) {
	if (_dictOffset == 0xFFFFFFFF)
		return;

	uint32 pos = herf.pos();

	if (!herf.seek(_dictOffset))
		return;

	uint32 magic = herf.readUint32LE();
	if (magic != 0x00F1A5C0)
		throw Common::Exception("Invalid HERF dictionary (0x%08X)", magic);

	uint32 hashCount = herf.readUint32LE();

	for (uint32 i = 0; i < hashCount; i++) {
		if ((uint32)herf.pos() >= (_dictOffset + _dictSize))
			break;

		uint32 hash = herf.readUint32LE();
		dict[hash] = Common::readStringFixed(herf, Common::kEncodingASCII, 128).toLower();
	}

	herf.seek(pos);
}

void HERFFile::readResList(Common::SeekableReadStream &herf) {
	std::map<uint32, Common::UString> dict;
	readDictionary(herf, dict);

	uint32 index = 0;
	ResourceList::iterator   res = _resources.begin();
	IResourceList::iterator iRes = _iResources.begin();
	for (; (res != _resources.end()) && (iRes != _iResources.end()); ++index, ++res, ++iRes) {
		res->index = index;

		res->hash = herf.readUint32LE();

		iRes->size   = herf.readUint32LE();
		iRes->offset = herf.readUint32LE();

		if (iRes->offset >= (uint32)herf.size())
			throw Common::Exception("HERFFile::readResList(): Resource goes beyond end of file");

		std::map<uint32, Common::UString>::const_iterator name = dict.find(res->hash);
		if (name != dict.end()) {
			res->name = Common::FilePath::getStem(name->second);
			res->type = TypeMan.getFileType(name->second);
		}
	}
}

const Archive::ResourceList &HERFFile::getResources() const {
	return _resources;
}

const HERFFile::IResource &HERFFile::getIResource(uint32 index) const {
	if (index >= _iResources.size())
		throw Common::Exception("Resource index out of range (%d/%d)", index, _iResources.size());

	return _iResources[index];
}

uint32 HERFFile::getResourceSize(uint32 index) const {
	return getIResource(index).size;
}

Common::SeekableReadStream *HERFFile::getResource(uint32 index) const {
	const IResource &res = getIResource(index);
	if (res.size == 0)
		return new Common::MemoryReadStream(0, 0);

	Common::SeekableReadStream *herf = ResMan.getResource(TypeMan.setFileType(_fileName, kFileTypeNone), kFileTypeHERF);
	if (!herf)
		throw Common::Exception(Common::kOpenError);

	if (!herf->seek(res.offset)) {
		delete herf;
		throw Common::Exception(Common::kSeekError);
	}

	Common::SeekableReadStream *resStream = herf->readStream(res.size);

	if (!resStream || (((uint32) resStream->size()) != res.size)) {
		delete herf;
		delete resStream;
		throw Common::Exception(Common::kReadError);
	}

	delete herf;
	return resStream;
}

Common::HashAlgo HERFFile::getNameHashAlgo() const {
	return Common::kHashDJB2;
}

} // End of namespace Aurora
