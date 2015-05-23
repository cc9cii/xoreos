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

// Largely based on the stream implementation found in ScummVM.

/** @file
 *  Basic stream interfaces.
 */

#include "src/common/stream.h"
#include "src/common/error.h"
#include "src/common/util.h"
#include "src/common/ustring.h"

namespace Common {

uint32 WriteStream::writeStream(ReadStream &stream, uint32 n) {
	uint32 haveRead = 0;

	byte buf[4096];
	while (!stream.eos() && (n > 0)) {
		uint32 toRead  = MIN<uint32>(4096, n);
		uint32 bufRead = stream.read(buf, toRead);

		write(buf, bufRead);

		n        -= bufRead;
		haveRead += bufRead;
	}

	return haveRead;
}

uint32 WriteStream::writeStream(ReadStream &stream) {
	return writeStream(stream, 0xFFFFFFFF);
}

void WriteStream::writeString(const UString &str) {
	write(str.c_str(), strlen(str.c_str()));
}


MemoryReadStream *ReadStream::readStream(uint32 dataSize) {
	byte *buf = new byte[dataSize];

	try {

		if (read(buf, dataSize) != dataSize)
			throw Exception(kReadError);

	} catch (...) {
		delete[] buf;
		throw;
	}

	return new MemoryReadStream(buf, dataSize, true);
}


uint32 MemoryReadStream::read(void *dataPtr, uint32 dataSize) {
	// Read at most as many bytes as are still available...
	if (dataSize > _size - _pos) {
		dataSize = _size - _pos;
		_eos = true;
	}
	std::memcpy(dataPtr, _ptr, dataSize);

	if (_encbyte) {
		byte *p = (byte *)dataPtr;
		byte *end = p + dataSize;
		while (p < end)
			*p++ ^= _encbyte;
	}

	_ptr += dataSize;
	_pos += dataSize;

	return dataSize;
}

void MemoryReadStream::seek(int32 offs, int whence) {
	assert(_pos <= _size);

	uint32 newPos = _pos;

	switch (whence) {
	case SEEK_END:
		// SEEK_END works just like SEEK_SET, only 'reversed',
		// i.e. from the end.
		offs = _size + offs;
		// Fall through
	case SEEK_SET:
		newPos = offs;
		break;

	case SEEK_CUR:
		newPos += offs;
		break;
	}

	if (newPos > _size)
		throw Exception(kSeekError);

	_pos = newPos;
	_ptr = _ptrOrig + newPos;

	// Reset end-of-stream flag on a successful seek
	_eos = false;
}


uint32 SeekableReadStream::seekTo(uint32 offset) {
	uint32 curPos = pos();

	seek(offset);

	return curPos;
}


uint32 SubReadStream::read(void *dataPtr, uint32 dataSize) {
	if (dataSize > _end - _pos) {
		dataSize = _end - _pos;
		_eos = true;
	}

	dataSize = _parentStream->read(dataPtr, dataSize);
	_pos += dataSize;

	return dataSize;
}

SeekableSubReadStream::SeekableSubReadStream(SeekableReadStream *parentStream, uint32 begin, uint32 end, bool disposeParentStream)
	: SubReadStream(parentStream, end, disposeParentStream),
	_parentStream(parentStream),
	_begin(begin) {
	assert(_begin <= _end);
	_pos = _begin;
	_parentStream->seek(_pos);
	_eos = false;
}

void SeekableSubReadStream::seek(int32 offset, int whence) {
	assert(_pos >= _begin);
	assert(_pos <= _end);

	uint32 newPos = _pos;

	switch (whence) {
	case SEEK_END:
		offset = size() + offset;
		// fallthrough
	case SEEK_SET:
		newPos = _begin + offset;
		break;
	case SEEK_CUR:
		newPos += offset;
	}

	if ((newPos < _begin) || (newPos > _end))
		throw Exception(kSeekError);

	_pos = newPos;

	_parentStream->seek(_pos);
	_eos = false; // reset eos on successful seek
}

BufferedReadStream::BufferedReadStream(ReadStream *parentStream, uint32 bufSize, bool disposeParentStream)
	: _parentStream(parentStream),
	_disposeParentStream(disposeParentStream),
	_pos(0),
	_bufSize(0),
	_realBufSize(bufSize) {

	assert(parentStream);
	_buf = new byte[bufSize];
	assert(_buf);
}

BufferedReadStream::~BufferedReadStream() {
	if (_disposeParentStream)
		delete _parentStream;
	delete[] _buf;
}

uint32 BufferedReadStream::read(void *dataPtr, uint32 dataSize) {
	uint32 alreadyRead = 0;
	const uint32 bufBytesLeft = _bufSize - _pos;

	// Check whether the data left in the buffer suffices....
	if (dataSize > bufBytesLeft) {
		// Nope, we need to read more data

		// First, flush the buffer, if it is non-empty
		if (0 < bufBytesLeft) {
			std::memcpy(dataPtr, _buf + _pos, bufBytesLeft);
			_pos = _bufSize;
			alreadyRead += bufBytesLeft;
			dataPtr = (byte *)dataPtr + bufBytesLeft;
			dataSize -= bufBytesLeft;
		}

		// At this point the buffer is empty. Now if the read request
		// exceeds the buffer size, just satisfy it directly.
		if (dataSize > _bufSize)
			return alreadyRead + _parentStream->read(dataPtr, dataSize);

		// Refill the buffer.
		// If we didn't read as many bytes as requested, the reason
		// is EOF or an error. In that case we truncate the buffer
		// size, as well as the number of  bytes we are going to
		// return to the caller.
		_bufSize = _parentStream->read(_buf, _realBufSize);
		_pos = 0;
		if (dataSize > _bufSize)
			dataSize = _bufSize;
	}

	// Satisfy the request from the buffer
	std::memcpy(dataPtr, _buf + _pos, dataSize);
	_pos += dataSize;
	return alreadyRead + dataSize;
}

BufferedSeekableReadStream::BufferedSeekableReadStream(SeekableReadStream *parentStream, uint32 bufSize, bool disposeParentStream)
	: BufferedReadStream(parentStream, bufSize, disposeParentStream),
	_parentStream(parentStream) {
}

void BufferedSeekableReadStream::seek(int32 offset, int whence) {
	// If it is a "local" seek, we may get away with "seeking" around
	// in the buffer only.
	// Note: We could try to handle SEEK_END and SEEK_SET, too, but
	// since they are rarely used, it seems not worth the effort.
	if (whence == SEEK_CUR && (int)_pos + offset >= 0 && _pos + offset <= _bufSize) {
		_pos += offset;
	} else {
		// Seek was not local enough, so we reset the buffer and
		// just seeks normally in the parent stream.
		if (whence == SEEK_CUR)
			offset -= (_bufSize - _pos);
		_pos = _bufSize;
		_parentStream->seek(offset, whence);
	}
}

} // End of namespace Common
