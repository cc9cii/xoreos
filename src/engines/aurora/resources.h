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
 *  Generic Aurora engines resource utility functions.
 */

#ifndef ENGINES_AURORA_RESOURCES_H
#define ENGINES_AURORA_RESOURCES_H

#include "src/aurora/types.h"

namespace Common {
	class ChangeID;
	class UString;
}

namespace Engines {

/** Add an archive file to the resource manager, erroring out if it does not exist. */
void indexMandatoryArchive(const Common::UString &file, uint32 priority, Common::ChangeID *changeID = 0);

/** Add an archive file to the resource manager, if it exists. */
bool indexOptionalArchive(const Common::UString &file, uint32 priority, Common::ChangeID *changeID = 0);

/** Add a directory to the resource manager, erroring out if it does not exist. */
void indexMandatoryDirectory(const Common::UString &dir, const char *glob, int depth,
                             uint32 priority, Common::ChangeID *changeID = 0);

/** Add a directory to the resource manager, if it exists. */
bool indexOptionalDirectory(const Common::UString &dir, const char *glob, int depth,
                            uint32 priority, Common::ChangeID *changeID = 0);

/** Remove previously added resources from the ResourceManager. */
void deindexResources(Common::ChangeID &changeID);

} // End of namespace Engines

#endif // ENGINES_AURORA_RESOURCES_H
