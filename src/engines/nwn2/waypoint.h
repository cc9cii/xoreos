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

/** @file engines/nwn2/waypoint.h
 *  NWN2 waypoint.
 */

#ifndef ENGINES_NWN2_WAYPOINT_H
#define ENGINES_NWN2_WAYPOINT_H

#include "common/types.h"
#include "common/ustring.h"

#include "aurora/types.h"

#include "engines/nwn2/types.h"
#include "engines/nwn2/object.h"

namespace Engines {

namespace NWN2 {

class Waypoint : public Object {
public:
	/** Load from a waypoint instance. */
	Waypoint(const Aurora::GFFStruct &waypoint);
	~Waypoint();

	/** Does this waypoint have a map note? */
	bool hasMapNote() const;
	/** Return the waypoint's map note text. */
	Common::UString getMapNote() const;

	/** Enable/Disable the waypoint's map note. */
	void enableMapNote(bool enabled);

private:
	bool _hasMapNote;         ///< Does this waypoint have a map note?
	Common::UString _mapNote; ///< The waypoint's map note text.

	/** Load from a waypoint instance. */
	void load(const Aurora::GFFStruct &waypoint);
	/** Load the waypoint from an instance and its blueprint. */
	void load(const Aurora::GFFStruct &instance, const Aurora::GFFStruct *blueprint);

	/** Load general waypoint properties. */
	void loadProperties(const Aurora::GFFStruct &gff);
};

} // End of namespace NWN2

} // End of namespace Engines

#endif // ENGINES_NWN2_WAYPOINT_H