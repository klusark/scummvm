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

#ifndef GRIM_SPUTRESOURCE_H
#define GRIM_SPUTRESOURCE_H

#include "common/endian.h"
#include "common/scummsys.h"
#include "common/str.h"

#include "engines/grim/renderdroid/rd.h"
#include "engines/grim/res.h"
#include "engines/grim/std/std_bitmap.h"

namespace Common {
class FSNode;
}

namespace Grim {

// The tag values are the reversed characters
enum SputResourceTags {
	kSputTagBknd = MKTAG('D', 'N', 'K', 'B'),
	kSputTagCmap = MKTAG('P', 'A', 'M', 'C'),
	kSputTagCost = MKTAG('T', 'S', 'O', 'C'),
	kSputTagFont = MKTAG('T', 'N', 'O', 'F'),
	kSputTagKeyf = MKTAG('F', 'Y', 'E', 'K'),
	kSputTagMat = MKTAG(' ', 'T', 'A', 'M'),
	kSputTagMem = MKTAG(' ', 'M', 'E', 'M'),
	kSputTagModl = MKTAG('L', 'D', 'O', 'M'),
	kSputTagSet = MKTAG(' ', 'T', 'E', 'S'),
	kSputTagSnd = MKTAG(' ', 'D', 'N', 'S'),
	kSputTagSync = MKTAG('S', 'Y', 'N', 'C'),
	kSputTagZbuf = MKTAG('F', 'U', 'B', 'Z')
};

struct ResMemBlock {
	byte *data;
	int size;
};

class BlastText;
class Cos;
class SputRender;
class SputRoom;
class Text;
class WuRegistry;

class SputResource : public ResHandler {
public:
	SputResource(Res *res, RenderDroid *rd, StdDisplay *stdDisplay, StdBitmap *stdBitmap);

	void setSputRender(SputRender *sputRender) { _sputRender = sputRender; }
	void setSputRoom(SputRoom *sputRoom) { _sputRoom = sputRoom; }
	void setCos(Cos *cos) { _cos = cos; }
	void setText(Text *text) { _text = text; }
	void setBlastText(BlastText *blastText) { _blastText = blastText; }
	void setWuRegistry(WuRegistry *wuRegistry) { _wuRegistry = wuRegistry; }

	int handler(ResHandlerMsg *msg);

	int stopPurge(int state);
	void *alloc(int resType, const char *name);
	int closeResources();
	int findResources();
	int addResourcePath(const char *path);
	int initialize(SysStruct *hs);
	int logToFile(const char *path);
	int terminate();
	int startup();
	int saveVerifyCallback(const char *fileName);
	int shutdown();
	int registerHandlers();
	int setMemoryUsage(int totalAllowedLimit, int purgeAmount);
	int loadResSync(ResHandlerMsg *msg);
	int loadResBknd(ResHandlerMsg *msg);
	void loadBknd();
	int loadResCost(ResHandlerMsg *msg);
	int loadResCmap(ResHandlerMsg *msg);
	int loadResFont(ResHandlerMsg *msg);
	int loadResKeyf(ResHandlerMsg *msg);
	int loadResMat(ResHandlerMsg *msg);
	int loadResModl(ResHandlerMsg *msg);
	int loadResSet(ResHandlerMsg *msg);
	int loadResSnd(ResHandlerMsg *msg);
	int loadResZbuf(ResHandlerMsg *msg);
	int loadResMem(ResHandlerMsg *msg);
	int cmap();
	RdMaterial *mat(char *path, int videoMemory, int videoMemoryOnly);
	int unlock(RdMaterial *material);
	int reportAllocations();

private:
	int addResourcePathInternal(const char *path);
	int addRemasteredResourcePath(const char *path);
	bool findResourceDirectory(const Common::FSNode &base, const Common::String &basePath, const char *relativePath, Common::String &fullPath);

	Res *_res;
	RenderDroid *_rd;
	StdDisplay *_stdDisplay;
	StdBitmap *_stdBitmap;
	SputRender *_sputRender;
	SputRoom *_sputRoom;
	Cos *_cos;
	Text *_text;
	BlastText *_blastText;
	WuRegistry *_wuRegistry;
	SysStruct *_hs;
	int _initialized;
	int _started;
	int _loaderLocked;
	int _totalAllowedLimit;
	int _purgeAmount;
	Common::String _extraDataPath;
	RdMaterialLoaderFunc _oldMatLoader;
	RdMaterialUnloaderFunc _oldMatUnloader;
};

} // End of namespace Grim

#endif
