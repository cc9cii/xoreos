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
 *  Utility templates and functions for working with strings.
 */

#include <cctype>
#include <climits>
#include <cerrno>
#include <cstdlib>
#include <cstdio>

#include "src/common/system.h"
#include "src/common/strutil.h"
#include "src/common/util.h"
#include "src/common/error.h"
#include "src/common/ustring.h"
#include "src/common/stream.h"

namespace Common {

void printDataHex(SeekableReadStream &stream) {
	uint32 pos  = stream.pos();
	uint32 size = stream.size() - pos;

	if (size == 0)
		return;

	uint32 offset = 0;
	byte rowData[16];

	while (size > 0) {
		// At max 16 bytes printed per row
		uint32 n = MIN<uint32>(size, 16);
		if (stream.read(rowData, n) != n)
			throw Exception(kReadError);

		// Print an offset
		std::fprintf(stderr, "%08X  ", offset);

		// 2 "blobs" of each 8 bytes per row
		for (uint32 i = 0; i < 2; i++) {
			for (uint32 j = 0; j < 8; j++) {
				uint32 m = i * 8 + j;

				if (m < n)
					// Print the data
					std::fprintf(stderr, "%02X ", rowData[m]);
				else
					// Last row, data count not aligned to 16
					std::fprintf(stderr, "   ");
			}

			// Separate the blobs by an extra space
			std::fprintf(stderr, " ");
		}

		std::fprintf(stderr, "|");

		// If the data byte is a printable character, print it. If not, substitute a '.'
		for (uint32 i = 0; i < n; i++)
			std::fprintf(stderr, "%c", std::isprint(rowData[i]) ? rowData[i] : '.');

		std::fprintf(stderr, "|\n");

		size   -= n;
		offset += n;
	}

	// Seek back
	stream.seek(pos);
}

void printDataHex(const byte *data, uint32 size) {
	if (!data || (size == 0))
		return;

	MemoryReadStream stream(data, size);
	printDataHex(stream);
}

static bool tagToString(uint32 tag, bool trim, Common::UString &str) {
	tag = TO_BE_32(tag);

	const char *tS = (const char *) &tag;
	if (!std::isprint(tS[0]) || !std::isprint(tS[1]) || !std::isprint(tS[2]) || !std::isprint(tS[3]))
		return false;

	str = UString::sprintf("%c%c%c%c", tS[0], tS[1], tS[2], tS[3]);
	if (trim)
		str.trim();

	return true;
}

UString debugTag(uint32 tag, bool trim) {
	Common::UString str;
	if (tagToString(tag, trim, str))
		return UString::sprintf("0x%08X ('%s')", FROM_BE_32(tag), str.c_str());

	return UString::sprintf("0x%08X", FROM_BE_32(tag));
}

// Helper functions for parseString()

static inline void parse(const char *nptr, char **endptr, signed long long &value) {
	value = strtoll(nptr, endptr, 0);
}

static inline void parse(const char *nptr, char **endptr, unsigned long long &value) {
	value = strtoull(nptr, endptr, 0);
}

static inline void parse(const char *nptr, char **endptr, signed long &value) {
	value = strtol(nptr, endptr, 0);
}

static inline void parse(const char *nptr, char **endptr, unsigned long &value) {
	value = strtoul(nptr, endptr, 0);
}

static inline void parse(const char *nptr, char **endptr, signed int &value) {
	signed long tmp = strtol(nptr, endptr, 0);
	if ((tmp < INT_MIN) || (tmp > INT_MAX))
		errno = ERANGE;

	value = (signed int) tmp;
}

static inline void parse(const char *nptr, char **endptr, unsigned int &value) {
	unsigned long tmp = strtoul(nptr, endptr, 0);
	if (tmp > UINT_MAX)
		errno = ERANGE;

	value = (unsigned int) tmp;
}

static inline void parse(const char *nptr, char **endptr, signed short &value) {
	signed long tmp = strtol(nptr, endptr, 0);
	if ((tmp < SHRT_MIN) || (tmp > SHRT_MAX))
		errno = ERANGE;

	value = (signed short) tmp;
}

static inline void parse(const char *nptr, char **endptr, unsigned short &value) {
	unsigned long tmp = strtoul(nptr, endptr, 0);
	if (tmp > USHRT_MAX)
		errno = ERANGE;

	value = (unsigned short) tmp;
}

static inline void parse(const char *nptr, char **endptr, signed char &value) {
	signed long tmp = strtol(nptr, endptr, 0);
	if ((tmp < SCHAR_MIN) || (tmp > SCHAR_MAX))
		errno = ERANGE;

	value = (signed char) tmp;
}

static inline void parse(const char *nptr, char **endptr, unsigned char &value) {
	unsigned long tmp = strtoul(nptr, endptr, 0);
	if (tmp > UCHAR_MAX)
		errno = ERANGE;

	value = (unsigned char) tmp;
}

static inline void parse(const char *nptr, char **endptr, float &value) {
	value = strtof(nptr, endptr);
}

static inline void parse(const char *nptr, char **endptr, double &value) {
	value = strtod(nptr, endptr);
}


template<typename T> void parseString(const UString &str, T &value) {
	const char *nptr = str.c_str();
	char *endptr = 0;

	T oldValue = value;

	errno = 0;
	parse(nptr, &endptr, value);

	while (endptr && isspace(*endptr))
		endptr++;

	try {
		if (endptr && (*endptr != '\0'))
			throw Exception("Can't convert \"%s\" to type of size %d", str.c_str(), sizeof(T));
		if (errno == ERANGE)
			throw Exception("\"%s\" out of range for type of size %d", str.c_str(), sizeof(T));
	} catch (...) {
		value = oldValue;
		throw;
	}
}

template<> void parseString(const UString &str, bool &value) {
	// Valid true values are "true", "yes", "y", "on" and "1"

	bool oldValue = value;

	try {
		value = (str.equalsIgnoreCase("true") ||
		         str.equalsIgnoreCase("yes")  ||
		         str.equalsIgnoreCase("y")    ||
		         str.equalsIgnoreCase("on")   ||
		         str == "1") ?
			true : false;
	} catch (...) {
		value = oldValue;
		throw;
	}
}

template void parseString<bool              >(const UString &str, bool               &value);

template void parseString<  signed char     >(const UString &str,   signed char      &value);
template void parseString<unsigned char     >(const UString &str, unsigned char      &value);
template void parseString<  signed short    >(const UString &str,   signed short     &value);
template void parseString<unsigned short    >(const UString &str, unsigned short     &value);
template void parseString<  signed int      >(const UString &str,   signed int       &value);
template void parseString<unsigned int      >(const UString &str, unsigned int       &value);
template void parseString<  signed long     >(const UString &str,   signed long      &value);
template void parseString<unsigned long     >(const UString &str, unsigned long      &value);
template void parseString<  signed long long>(const UString &str,   signed long long &value);
template void parseString<unsigned long long>(const UString &str, unsigned long long &value);

template void parseString<float             >(const UString &str, float              &value);
template void parseString<double            >(const UString &str, double             &value);


template<typename T> UString composeString(T value) {
	char buf[64], *bufEnd = buf + sizeof(buf) - 1;

	char *strStart = buf, *strEnd = buf;

	// Write the sign, if negative
	if (value < 0) {
		*strStart = '-';
		value = -value;

		strStart++;
		strEnd++;
	}

	// Collect all the digits (least significant ones first)
	do {
		*strEnd++ = (value % 10) + '0';
	} while ((value /= 10) && (strEnd < bufEnd));

	*strEnd-- = '\0';

	// Reverse the digits
	while (strStart < strEnd) {
		SWAP(*strStart, *strEnd);

		strStart++;
		strEnd--;
	}

	return UString(buf);
}

template<> UString composeString(bool value) {
	return value ? "true" : "false";
}

template<> UString composeString(float value) {
	return UString::sprintf("%f", value);
}

template<> UString composeString(double value) {
	return UString::sprintf("%lf", value);
}

template UString composeString<bool              >(bool               value);

template UString composeString<  signed char     >(  signed char      value);
template UString composeString<unsigned char     >(unsigned char      value);
template UString composeString<  signed short    >(  signed short     value);
template UString composeString<unsigned short    >(unsigned short     value);
template UString composeString<  signed int      >(  signed int       value);
template UString composeString<unsigned int      >(unsigned int       value);
template UString composeString<  signed long     >(  signed long      value);
template UString composeString<unsigned long     >(unsigned long      value);
template UString composeString<  signed long long>(  signed long long value);
template UString composeString<unsigned long long>(unsigned long long value);

template UString composeString<float             >(float              value);
template UString composeString<double            >(double             value);

} // End of namespace Common
