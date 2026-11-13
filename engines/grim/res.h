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

#ifndef GRIM_RES_H
#define GRIM_RES_H

#include "common/array.h"
#include "common/endian.h"
#include "common/scummsys.h"
#include "common/str.h"

#include "engines/grim/std/std_sys.h"

namespace Common {
class DumpFile;
class ReadStream;
class WriteStream;
}

namespace Grim {

enum ResTags {
	kResTagDefaultHandler = MKTAG('D', 'E', 'F', 'H'),
	kResTagResf = MKTAG('R', 'E', 'S', 'F')
};

struct ResHandlerMsg {
	int op;
	const char *name;
	int tag;
	void *data;
	int result;
};

class ResHandler {
public:
	virtual ~ResHandler() {}

	virtual int handler(ResHandlerMsg *msg) = 0;
};

struct ResRef {
	int valid;
	int serial;
	ResRef *prev;
	ResRef *next;
	void *handle;
};

struct ResNode {
	ResNode *next;
	ResNode *prev;
	char *name;
	uint hash;
	int tag;
	void *handle;
	int serial;
	uint16 flags;
	ResRef *refs;
	int size;
};

struct ResTag {
	ResTag *next;
	ResTag *prev;
	int tag;
	ResHandler *handler;
};

enum ResLabFileFlags {
	kResLabFileDirectory = 1,
	kResLabFileLab = 2
};

struct ResLabFile {
	Common::String path;
	Common::String archiveName;
	int flags;
};

class SputRender;

class Res {
public:
	explicit Res(bool isDemo);
	~Res();

	void setSputRender(SputRender *sputRender) { _sputRender = sputRender; }

	int sysInitialize(SysStruct *hs);
	void sysTerminate();
	int sysStartup();
	int sysShutdown();
	int sysRegisterHandler(int tag, ResHandler *handler);
	int fileOpen(const char *path);
	int fileClose(const char *path);
	void fileCloseAll();
	int fileMoveToFront(const char *path);
	int sysSave(Common::WriteStream *stream);
	int sysLoad(Common::ReadStream *stream);
	int sysSaveRemastered(Common::WriteStream *stream);
	int sysLoadRemastered(Common::ReadStream *stream);
	int allocCompletion(int tag, const char *name, void *handle);
	void *getResource(int tag, const char *name);
	int setPurgeOnFree(void *handle, int purge);
	int freeResources(uint bytesWanted, uint *bytesFreed);
	int dump(const char *path, int tag);
	int log(const char *path);
	ResTag *getResTagStruct(int tag);
	ResNode *checkAllocatedRes(void *handle);
	void *handleFromSerialNumber(int serial);
	int serialFromHandle(void *handle);
	ResNode *resourceHandleFromSerialNumber(int serial);
	int loaded(int tag, const char *name);
	ResNode *isResourceLoaded(int tag, const char *name);
	ResNode *checkForNeedLoadRes(int tag, const char *name, int *result);
	ResNode *checkList1(int tag, const char *name);
	ResNode *locateResByName(int tag, const char *name, ResNode *start);
	ResNode *load(int tag, const char *name, void *handle);
	void unlinkNode(ResNode *node);
	int freeNode(ResNode *node);
	int loadFileData(ResHandlerMsg *msg);
	int getUserField(ResNode *node);
	int totalAllocated();
	void checkAnotherLists(ResNode *node);
	uint changeFileNameToUpper(const char *src, char *dst);
	int findExternalFile(const char *name, char *outPath, uint size);
	void setReference(ResRef *ref, void *handle);
	void removeReference(ResRef *ref);
	void allocatedIterFirst(ResNode **next, void **handle, int tag);
	void allocatedIterNext(ResNode **next, void **handle, int tag);
	void touch(void *handle);
	void resolveToSerialNumber(ResRef *ref);
	void freeAll();
	int countDependents(void *handle);
	int initLists(int tag, const char *name);
	void setMemoryLimitMB(int megabytes);
	int setLogFile(const char *path);

	int getMemoryLimit() const { return _memoryLimit; }

private:
	int openLabFile(const char *path);
	int fileOpenSearch(const char *dirPath);
	int lookupFileHandle(const char *path);

	SysStruct *_resHS;
	int _resSysInitialized;
	int _started;
	Common::Array<ResLabFile> _labFiles;
	int _nextLabPriority;
	int _nextDirectoryPriority;
	ResTag *_tagHead;
	ResNode *_nodeHead;
	ResNode *_nodeTail;
	ResHandler *_defaultHandler;
	Common::DumpFile *_logFile;
	int _memoryLimit;
	SputRender *_sputRender;
	bool _demo;
};

} // End of namespace Grim

#endif
