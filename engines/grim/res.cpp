/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common/archive.h"
#include "common/file.h"
#include "common/fs.h"
#include "common/stream.h"
#include "common/textconsole.h"

#include "engines/grim/grim.h"
#include "engines/grim/imuse/imuse_engine.h"
#include "engines/grim/lab.h"
#include "engines/grim/res.h"
#include "engines/grim/render/sputrender.h"

namespace Grim {

static const int kLabPriorityBase = -1000;
static const int kDirectoryPriorityBase = 2;

Res::Res(bool isDemo) : _resHS(nullptr), _resSysInitialized(0), _started(0), _nextLabPriority(kLabPriorityBase),
		_nextDirectoryPriority(kDirectoryPriorityBase), _nextRemasteredPriority(kDirectoryPriorityBase),
		_tagHead(nullptr), _nodeHead(nullptr), _nodeTail(nullptr), _defaultHandler(nullptr),
		_logFile(nullptr), _memoryLimit(0x1800000), _sputRender(nullptr), _demo(isDemo) {
}

Res::~Res() {
	delete _logFile;
	_logFile = nullptr;
}

int Res::sysInitialize(SysStruct *hs) {
	_resHS = hs;

	if (_resSysInitialized)
		return 1;

	_resSysInitialized = 1;
	_tagHead = nullptr;
	_nodeTail = nullptr;
	_nodeHead = nullptr;

	return 0;
}

void Res::sysTerminate() {
	_resSysInitialized = 0;
}

int Res::sysStartup() {
	if (!_resSysInitialized)
		return 1;

	if (_started)
		return 1;

	_started = 1;

	return 0;
}

int Res::sysShutdown() {
	if (_started) {
		fileCloseAll();
		freeAll();

		while (_tagHead) {
			ResTag *next = _tagHead->next;
			delete _tagHead;
			_tagHead = next;
		}
	}

	_started = 0;

	return 0;
}

int Res::sysRegisterHandler(int tag, ResHandler *handler) {
	if (tag == kResTagDefaultHandler) {
		_defaultHandler = handler;
		return 1;
	}

	ResTag *tagPointer = _tagHead;

	while (tagPointer) {
		if (tagPointer->tag == tag)
			break;

		tagPointer = tagPointer->next;
	}

	if (tagPointer)
		return 2;

	tagPointer = new ResTag();
	tagPointer->tag = tag;
	tagPointer->handler = handler;
	tagPointer->next = _tagHead;

	if (_tagHead)
		_tagHead->prev = tagPointer;

	tagPointer->prev = nullptr;
	_tagHead = tagPointer;

	return 1;
}

int Res::fileOpen(const char *path) {
	if (!*path || path[strlen(path) - 1] == '\\' || path[strlen(path) - 1] == '/')
		return fileOpenSearch(path);

	return openLabFile(path);
}

int Res::fileClose(const char *path) {
	int index = lookupFileHandle(path);

	if (index >= 0) {
		SearchMan.remove(_labFiles[index].archiveName);
		_labFiles.remove_at(index);
	}

	return 1;
}

void Res::fileCloseAll() {
	while (!_labFiles.empty()) {
		Common::String path(_labFiles[0].path);

		fileClose(path.c_str());
	}

	_nextLabPriority = kLabPriorityBase;
	_nextDirectoryPriority = kDirectoryPriorityBase;
	_nextRemasteredPriority = kDirectoryPriorityBase;
}

int Res::fileMoveToFront(const char *path) {
	int index = lookupFileHandle(path);

	if (index < 0)
		return 0;

	if (g_grim->isRemastered()) {
		SearchMan.setPriority(_labFiles[index].archiveName, _nextRemasteredPriority);
		_nextRemasteredPriority++;
	} else if (_labFiles[index].flags == kResLabFileLab) {
		SearchMan.setPriority(_labFiles[index].archiveName, _nextLabPriority);
		_nextLabPriority++;
	} else {
		SearchMan.setPriority(_labFiles[index].archiveName, _nextDirectoryPriority);
		_nextDirectoryPriority++;
	}

	return 1;
}

int Res::openLabFile(const char *path) {
	Common::Path labPath(path, Common::Path::kNativeSeparator);
	Common::String name(labPath.baseName());

	name.toLowercase();

	if (SearchMan.hasArchive(name))
		return 1;

	if (!SearchMan.hasFile(Common::Path(name))) {
		warning("Res::openLabFile(): %s is not reachable", path);
		return 0;
	}

	Lab *lab = new Lab();

	if (!lab->open(Common::Path(name), false)) {
		delete lab;
		lab = nullptr;

		return 0;
	}

	int labPriority = _nextLabPriority;

	if (g_grim->isRemastered()) {
		labPriority = _nextRemasteredPriority;
		_nextRemasteredPriority++;
	}

	SearchMan.add(name, lab, labPriority, true);

	ResLabFile labFile;

	labFile.path = path;
	labFile.archiveName = name;
	labFile.flags = kResLabFileLab;
	_labFiles.push_back(labFile);

	return 1;
}

int Res::fileOpenSearch(const char *dirPath) {
	Common::String directoryName(dirPath);

	if (!directoryName.empty() && (directoryName.lastChar() == '\\' || directoryName.lastChar() == '/'))
		directoryName.deleteLastChar();

	if (directoryName.empty())
		return 0;

	Common::FSNode directory(Common::Path(directoryName, Common::Path::kNativeSeparator));
	Common::FSList files;

	if (!directory.exists() || !directory.isDirectory() || !directory.getChildren(files, Common::FSNode::kListFilesOnly) || files.empty())
		return 0;

	Common::String name(directory.getPath().toString());

	if (SearchMan.hasArchive(name))
		return 1;

	int directoryPriority = _nextDirectoryPriority;

	if (g_grim->isRemastered()) {
		directoryPriority = _nextRemasteredPriority;
		_nextRemasteredPriority++;
	}

	SearchMan.add(name, new Common::FSDirectory(directory, 1), directoryPriority, true);

	ResLabFile labFile;

	labFile.path = dirPath;
	labFile.archiveName = name;
	labFile.flags = kResLabFileDirectory;
	_labFiles.push_back(labFile);

	return 1;
}

int Res::lookupFileHandle(const char *path) {
	for (uint i = 0; i < _labFiles.size(); i++) {
		if (_labFiles[i].path == path)
			return i;
	}

	return -1;
}

int Res::allocCompletion(int tag, const char *name, void *handle) {
	if (checkAllocatedRes(handle))
		return 4;

	ResNode *node = load(tag, name, handle);

	if (!node)
		return 0;

	if (_logFile)
		_logFile->writeString(Common::String::format("Resource <%-12s> allocated: %8d bytes\n", name, getUserField(node)));

	return 1;
}

void *Res::getResource(int tag, const char *name) {
	int result;
	ResNode *node = checkForNeedLoadRes(tag, name, &result);

	if (node) {
		checkAnotherLists(node);
		return node->handle;
	}

	return nullptr;
}

int Res::setPurgeOnFree(void *handle, int purge) {
	int result = 0;
	ResNode *node = checkAllocatedRes(handle);

	if (node) {
		if (!(node->flags & 8))
			result = 1;

		if (purge)
			node->flags &= 0xFFF7;
		else
			node->flags |= 8;
	}

	return result;
}

int Res::freeResources(uint bytesWanted, uint *bytesFreed) {
	int freedAny = 1;
	int numVisited = 0;

	*bytesFreed = 0;

	if (bytesWanted) {
		while (freedAny) {
			ResNode *node = _nodeTail;
			freedAny = 0;

			if (node) {
				int listEnded = 0;

				while (*bytesFreed < bytesWanted) {
					numVisited++;

					if (g_grim->getImuse() && !(numVisited % 5))
						g_grim->getImuse()->soundsPeriodic();

					ResNode *prev = node->prev;

					if (!node->refs && !(node->flags & 8)) {
						*bytesFreed += getUserField(node);
						unlinkNode(node);
						freeNode(node);
						freedAny = 1;
					}

					node = prev;

					if (!node) {
						listEnded = 1;
						break;
					}
				}

				if (!listEnded)
					return 1;
			}

			if (*bytesFreed >= bytesWanted)
				break;
		}
	}

	if (*bytesFreed >= bytesWanted)
		return 1;

	return 0;
}

int Res::dump(const char *path, int tag) {
	int totalBytes = 0;
	int numItems = 0;
	Common::DumpFile file;

	if (!file.open(path))
		return 0;

	for (ResNode *node = _nodeHead; node; node = node->next) {
		if (!tag || node->tag == tag) {
			const char *status = "";

			if (node->refs)
				status = "REFERENCED";

			file.writeString(Common::String::format("Resource %-12s,\t\tsize = %7d\t%s\n", node->name, getUserField(node), status));
			totalBytes += getUserField(node);
			numItems++;
		}
	}

	file.writeString(Common::String::format("Total: %d items, %d bytes\n", numItems, totalBytes));
	file.close();

	return 1;
}

int Res::log(const char *path) {
	if (path) {
		delete _logFile;
		_logFile = new Common::DumpFile();

		if (!_logFile->open(path)) {
			delete _logFile;
			_logFile = nullptr;
		}

		return 1;
	}

	if (_logFile) {
		delete _logFile;
		_logFile = nullptr;
	}

	return 1;
}

ResTag *Res::getResTagStruct(int tag) {
	ResTag *tagPointer = _tagHead;

	while (tagPointer) {
		if (tagPointer->tag == tag)
			return tagPointer;

		tagPointer = tagPointer->next;
	}

	return nullptr;
}

ResNode *Res::checkAllocatedRes(void *handle) {
	ResNode *node = _nodeHead;

	while (node) {
		if (node->handle == handle)
			return node;

		node = node->next;
	}

	return nullptr;
}

void *Res::handleFromSerialNumber(int serial) {
	ResNode *node = resourceHandleFromSerialNumber(serial);

	if (node)
		return node->handle;

	return nullptr;
}

int Res::serialFromHandle(void *handle) {
	ResNode *node = checkAllocatedRes(handle);

	if (node)
		return node->serial;

	return (int)(intptr)handle;
}

ResNode *Res::resourceHandleFromSerialNumber(int serial) {
	ResNode *node = _nodeHead;

	while (node) {
		if (node->serial == serial)
			return node;

		node = node->next;
	}

	return nullptr;
}

int Res::loaded(int tag, const char *name) {
	return isResourceLoaded(tag, name) != nullptr;
}

ResNode *Res::isResourceLoaded(int tag, const char *name) {
	char upperName[260];

	if (strlen(name) >= sizeof(upperName)) {
		warning("Res::isResourceLoaded(): name too long");
		return nullptr;
	}

	uint hash = changeFileNameToUpper(name, upperName);
	ResNode *node = _nodeHead;

	while (node) {
		if (node->tag == tag && node->hash == hash && !(node->flags & 2) && !strcmp(node->name, upperName))
			return node;

		node = node->next;
	}

	return nullptr;
}

ResNode *Res::checkForNeedLoadRes(int tag, const char *name, int *result) {
	ResNode *node = isResourceLoaded(tag, name);

	if (node) {
		*result = 4;
		return node;
	}

	ResTag *tagPointer = getResTagStruct(tag);
	ResHandlerMsg msg;

	msg.op = 1;
	msg.name = name;
	msg.tag = tag;
	msg.data = nullptr;
	msg.result = 0;

	// The demo has no loading image and does not update the sounds while loading
	if (!_demo)
		_sputRender->drawLoadingImage();

	if (tagPointer)
		*result = tagPointer->handler->handler(&msg);
	else if (_defaultHandler)
		*result = _defaultHandler->handler(&msg);
	else
		*result = loadFileData(&msg);

	if (g_grim->getImuse() && !_demo)
		g_grim->getImuse()->soundsPeriodic();

	if (*result == 1) {
		node = checkList1(tag, name);

		// The demo asks the handler for the size every time it is needed
		if (node && !_demo) {
			ResHandlerMsg sizeMsg;

			sizeMsg.op = 3;
			sizeMsg.name = node->name;
			sizeMsg.tag = node->tag;
			sizeMsg.data = node->handle;
			sizeMsg.result = 0;

			if (tagPointer) {
				tagPointer->handler->handler(&sizeMsg);
			} else {
				warning("Res::checkForNeedLoadRes(): no handler registered for tag %d", tag);

				if (_defaultHandler)
					_defaultHandler->handler(&sizeMsg);
				else
					loadFileData(&sizeMsg);
			}

			node->size = sizeMsg.result;
		}
	}

	return node;
}

ResNode *Res::checkList1(int tag, const char *name) {
	return locateResByName(tag, name, _nodeHead);
}

ResNode *Res::locateResByName(int tag, const char *name, ResNode *start) {
	char upperName[260];

	if (strlen(name) >= sizeof(upperName)) {
		warning("Res::locateResByName(): name too long");
		return nullptr;
	}

	uint hash = changeFileNameToUpper(name, upperName);
	ResNode *node = start;

	while (node) {
		if (node->tag == tag && node->hash == hash && !strcmp(node->name, upperName))
			return node;

		node = node->next;
	}

	return nullptr;
}

ResNode *Res::load(int tag, const char *name, void *handle) {
	ResNode *node = new ResNode();

	node->tag = tag;
	node->handle = handle;
	node->serial = (int)(intptr)handle;
	node->flags = 0;
	node->refs = nullptr;
	node->name = nullptr;

	node->name = new char[strlen(name) + 1];
	node->hash = changeFileNameToUpper(name, node->name);
	node->flags |= 4;

	node->next = _nodeHead;

	if (_nodeHead)
		_nodeHead->prev = node;

	node->prev = nullptr;
	_nodeHead = node;

	if (!_nodeTail)
		_nodeTail = node;

	checkAnotherLists(node);

	return node;
}

void Res::unlinkNode(ResNode *node) {
	int numRefs = 0;
	ResTag *tag = getResTagStruct(node->tag);
	ResRef *ref = node->refs;

	while (ref) {
		numRefs++;

		if (!ref->valid)
			warning("Res::unlinkNode(): reference of %s is not valid", node->name);

		ref->valid = 0;
		ref = ref->next;
	}

	if (tag) {
		ResHandlerMsg msg;

		msg.op = 2;
		msg.name = node->name;
		msg.tag = node->tag;
		msg.data = node->handle;
		msg.result = 0;

		if (tag->handler->handler(&msg) == 1) {
			if (_logFile)
				_logFile->writeString(Common::String::format("Resource <%-12s> freed (%d references)\n", node->name, numRefs));
		}
	}
}

int Res::freeNode(ResNode *node) {
	if (node->prev)
		node->prev->next = node->next;

	if (node->next)
		node->next->prev = node->prev;

	if (_nodeHead == node)
		_nodeHead = node->next;

	if (_nodeTail == node)
		_nodeTail = node->prev;

	if (node->flags & 4) {
		delete[] node->name;
		node->name = nullptr;
	}

	delete node;

	return 1;
}

int Res::loadFileData(ResHandlerMsg *msg) {
	if (msg->op != 1) {
		if (msg->op == 2)
			delete[] static_cast<byte *>(msg->data);

		return 0;
	}

	if (msg->tag == kResTagResf)
		return 0;

	Common::SeekableReadStream *file = _resHS->openFile(msg->name);

	if (!file)
		return 4;

	uint size = (uint)file->size();
	byte *data = new byte[size];

	file->read(data, size);
	allocCompletion(msg->tag, msg->name, data);
	delete file;

	return 1;
}

int Res::getUserField(ResNode *node) {
	if (!node)
		return 0;

	if (_demo) {
		ResTag *tag = getResTagStruct(node->tag);

		if (!tag)
			return 0;

		ResHandlerMsg sizeMsg;

		sizeMsg.op = 3;
		sizeMsg.name = node->name;
		sizeMsg.tag = node->tag;
		sizeMsg.data = node->handle;
		sizeMsg.result = 0;

		tag->handler->handler(&sizeMsg);

		return sizeMsg.result;
	}

	return node->size;
}

int Res::totalAllocated() {
	// TODO: the EXE returns the bytes allocated by the whole memory manager, here only the resources are counted
	int totalBytes = 0;

	for (ResNode *node = _nodeHead; node; node = node->next)
		totalBytes += getUserField(node);

	return totalBytes;
}

void Res::checkAnotherLists(ResNode *node) {
	if (_nodeHead == node)
		return;

	if (node->prev)
		node->prev->next = node->next;

	if (node->next)
		node->next->prev = node->prev;

	if (_nodeTail == node)
		_nodeTail = node->prev;

	node->next = _nodeHead;
	_nodeHead->prev = node;
	node->prev = nullptr;
	_nodeHead = node;
}

uint Res::changeFileNameToUpper(const char *src, char *dst) {
	uint length = strlen(src);
	uint hash = (((length | ((length | (length << 8)) << 8)) << 8) | length);
	const char *srcPointer = src + 1;
	char c = toupper((byte)*src);

	*dst = c;
	char *dstPointer = dst + 1;

	while (c) {
		int next = *srcPointer++;
		hash = c ^ ((hash >> 31) | (2 * hash));
		c = toupper((byte)next);
		*dstPointer++ = c;
	}

	return hash;
}

int Res::findExternalFile(const char *name, char *outPath, uint size) {
	char upperName[260];

	if (strlen(name) >= sizeof(upperName)) {
		warning("Res::findExternalFile(): name too long");
		return 0;
	}

	if (!size) {
		warning("Res::findExternalFile(): empty output buffer");
		return 0;
	}

	changeFileNameToUpper(name, upperName);

	if (!SearchMan.hasFile(Common::Path(upperName)))
		return 0;

	memset(outPath, 0, size);
	strncpy(outPath, upperName, size - 1);

	return 1;
}

void Res::setReference(ResRef *ref, void *handle) {
	ResNode *node = checkAllocatedRes(handle);

	if (!ref) {
		warning("Res::setReference(): no reference given");
		return;
	}

	if (ref->valid)
		warning("Res::setReference(): reference is already valid");

	if (node) {
		ref->valid = 1;
		ref->serial = (int)(intptr)handle;
		ref->handle = handle;
		ref->prev = nullptr;
		ref->next = node->refs;

		if (ref->next)
			ref->next->prev = ref;

		node->refs = ref;
		checkAnotherLists(node);
	} else {
		ref->valid = 0;
	}
}

void Res::removeReference(ResRef *ref) {
	if (!ref->valid)
		return;

	ResNode *node = checkAllocatedRes(ref->handle);

	if (node) {
		if (node->refs == ref)
			node->refs = ref->next;

		if (ref->prev)
			ref->prev->next = ref->next;

		if (ref->next)
			ref->next->prev = ref->prev;

		ref->valid = 0;
		ref->next = nullptr;
		ref->prev = nullptr;
	}
}

void Res::allocatedIterFirst(ResNode **next, void **handle, int tag) {
	ResNode *node = _nodeHead;

	while (node) {
		if (node->tag == tag) {
			*next = node->next;
			*handle = node->handle;
			return;
		}

		node = node->next;
	}

	*handle = nullptr;
	*next = nullptr;
}

void Res::allocatedIterNext(ResNode **next, void **handle, int tag) {
	ResNode *node = *next;

	while (node) {
		if (node->tag == tag) {
			*next = node->next;
			*handle = node->handle;
			return;
		}

		node = node->next;
	}

	*handle = nullptr;
	*next = nullptr;
}

void Res::touch(void *handle) {
	ResNode *node = checkAllocatedRes(handle);

	if (node)
		checkAnotherLists(node);
}

void Res::resolveToSerialNumber(ResRef *ref) {
	ResNode *node = resourceHandleFromSerialNumber(ref->serial);

	if (node) {
		ref->valid = 0;
		ref->next = nullptr;
		ref->prev = nullptr;
		setReference(ref, node->handle);
	}
}

int Res::sysSave(Common::WriteStream *stream) {
	int numNodes = 0;

	for (ResNode *node = _nodeHead; node; node = node->next) {
		if ((node->refs && !strstr(node->name, "TEMP")) || (node->flags & 8))
			numNodes++;
	}

	stream->writeSint32LE(numNodes);

	if (stream->err())
		return 1;

	for (ResNode *node = _nodeHead; node; node = node->next) {
		if ((node->refs && !strstr(node->name, "TEMP")) || (node->flags & 8)) {
			int length = strlen(node->name);

			stream->writeSint32LE(length);

			if (stream->err())
				return 1;

			stream->write(node->name, length);

			if (stream->err())
				return 1;

			stream->writeSint32LE(node->tag);

			if (stream->err())
				return 1;

			// The EXE overwrote the serial with the handle here, which supposedly
			// worked only because its heap addresses are the same on every run
			// node->serial = (int)(intptr)node->handle;
			stream->writeSint32LE(node->serial);

			if (stream->err())
				return 1;
		}
	}

	return 0;
}

int Res::sysLoad(Common::ReadStream *stream) {
	char fileName[256];

	int numNodes = stream->readSint32LE();

	if (stream->err() || stream->eos())
		return 1;

	if (numNodes <= 0)
		return 0;

	for (int i = 0; i < numNodes; i++) {
		int length = stream->readSint32LE();

		if (stream->err() || stream->eos())
			return 1;

		if (length > 255)
			return 1;

		if (length < 0) {
			warning("Res::sysLoad(): negative name length");
			return 1;
		}

		if (stream->read(fileName, length) != (uint32)length)
			return 1;

		fileName[length] = 0;

		int tag = stream->readSint32LE();

		if (stream->err() || stream->eos())
			return 1;

		int result;
		ResNode *node = checkForNeedLoadRes(tag, fileName, &result);

		if (!result)
			return 1;

		if (!node) {
			warning("Res::sysLoad(): %s was not loaded", fileName);
			return 1;
		}

		node->serial = stream->readSint32LE();

		if (stream->err() || stream->eos())
			return 1;
	}

	return 0;
}

int Res::sysSaveRemastered(Common::WriteStream *stream) {
	int numNodes = 0;

	for (ResNode *node = _nodeHead; node; node = node->next) {
		if ((node->refs && !strstr(node->name, "TEMP")) || (node->flags & 8))
			numNodes++;
	}

	stream->writeSint32LE(numNodes);

	for (ResNode *node = _nodeHead; node; node = node->next) {
		if ((node->refs && !strstr(node->name, "TEMP")) || (node->flags & 8)) {
			int length = strlen(node->name);

			stream->writeSint32LE(length);
			stream->write(node->name, length);
			stream->writeSint32LE(node->tag);
			stream->writeSint32LE(node->serial);
			stream->writeSint32LE(0);

			if (stream->err())
				return 1;
		}
	}

	return stream->err() ? 1 : 0;
}

int Res::sysLoadRemastered(Common::ReadStream *stream) {
	char fileName[256];

	int numNodes = stream->readSint32LE();

	if (stream->err() || stream->eos())
		return 1;

	for (int i = 0; i < numNodes; i++) {
		int length = stream->readSint32LE();

		if (stream->err() || stream->eos() || length > 255)
			return 1;

		if (length < 0) {
			warning("Res::sysLoadRemastered(): negative name length");
			return 1;
		}

		if (stream->read(fileName, length) != (uint32)length)
			return 1;

		fileName[length] = 0;

		int tag = stream->readSint32LE();

		if (stream->err() || stream->eos())
			return 1;

		int result;
		ResNode *node = checkForNeedLoadRes(tag, fileName, &result);

		if (!result)
			return 1;

		if (!node) {
			warning("Res::sysLoadRemastered(): %s was not loaded", fileName);
			return 1;
		}

		node->serial = stream->readSint32LE();
		stream->readSint32LE();

		if (stream->err() || stream->eos())
			return 1;
	}

	return 0;
}

void Res::freeAll() {
	ResNode *result = _nodeHead;
	ResNode *node = _nodeHead;

	while (node) {
		ResNode *next = node->next;

		if (!(node->flags & 8)) {
			unlinkNode(node);
			freeNode(node);
			result = _nodeHead;
		}

		node = next;
	}

	if (result) {
		do {
			unlinkNode(result);
			freeNode(_nodeHead);
			result = _nodeHead;
		} while (_nodeHead);
	}
}

int Res::countDependents(void *handle) {
	int count = 0;
	ResNode *node = checkAllocatedRes(handle);

	if (node) {
		for (ResRef *ref = node->refs; ref; ref = ref->next)
			count++;
	}

	return count;
}

int Res::initLists(int tag, const char *name) {
	int count = 0;
	ResNode *node = checkList1(tag, name);

	if (node) {
		for (ResRef *ref = node->refs; ref; ref = ref->next)
			count++;
	}

	return count;
}

void Res::setMemoryLimitMB(int megabytes) {
	_memoryLimit = (int)((uint)megabytes << 20);
}

int Res::setLogFile(const char *path) {
	return log(path) == 1;
}

} // End of namespace Grim
