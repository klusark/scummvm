#include <cxxtest/TestSuite.h>

#include "common/archive.h"
#include "common/array.h"
#include "common/endian.h"
#include "common/file.h"
#include "common/fs.h"
#include "common/hashmap.h"
#include "common/macresman.h"
#include "common/memstream.h"
#include "common/path.h"
#include "common/ptr.h"
#include "../system/null_osystem.h"

// The tests below build tiny resource forks by hand and check every place
// MacResManager looks for one. The filesystem tests also check that a file
// which is not in the archive is never looked up relative to the current
// working directory: game detection probes thousands of missing names per
// directory, and each such probe used to cost a filesystem call.

namespace MacResManTest {

static const uint32 kTestType = MKTAG('T', 'E', 'S', 'T');
static const uint16 kTestId = 128;

typedef Common::Array<byte> Bytes;

static void putBE32(Bytes &b, uint32 v) {
	b.push_back((v >> 24) & 0xFF);
	b.push_back((v >> 16) & 0xFF);
	b.push_back((v >> 8) & 0xFF);
	b.push_back(v & 0xFF);
}

static void putBE16(Bytes &b, uint16 v) {
	b.push_back((v >> 8) & 0xFF);
	b.push_back(v & 0xFF);
}

// A raw resource fork with one resource of type kTestType, id kTestId.
static Bytes makeRawFork(const char *payload) {
	const uint32 payloadLen = strlen(payload);
	const uint32 dataLen = 4 + payloadLen;
	const uint32 mapLen = 28 + 10 + 12;

	Bytes b;
	putBE32(b, 16);            // data offset
	putBE32(b, 16 + dataLen);  // map offset
	putBE32(b, dataLen);
	putBE32(b, mapLen);

	putBE32(b, payloadLen);
	for (uint32 i = 0; i < payloadLen; i++)
		b.push_back(payload[i]);

	for (int i = 0; i < 22; i++) // header copy, next map handle, file ref
		b.push_back(0);
	putBE16(b, 0);             // attributes
	putBE16(b, 28);            // type list offset (points at the count)
	putBE16(b, mapLen);        // name list offset (empty)
	putBE16(b, 0);             // number of types - 1
	putBE32(b, kTestType);
	putBE16(b, 0);             // number of resources - 1
	putBE16(b, 10);            // reference list offset from the type list
	putBE16(b, kTestId);
	putBE16(b, 0xFFFF);        // no name
	putBE32(b, 0);             // attributes + data offset
	putBE32(b, 0);             // handle
	return b;
}

static Bytes makeAppleDouble(const char *payload) {
	Bytes fork = makeRawFork(payload);
	Bytes b;
	putBE32(b, 0x00051607);
	putBE32(b, 0x00020000);
	for (int i = 0; i < 16; i++)
		b.push_back(0);
	putBE16(b, 1);             // one entry
	putBE32(b, 2);             // resource fork
	putBE32(b, 26 + 12);
	putBE32(b, fork.size());
	b.push_back(fork);
	return b;
}

static Bytes makeMacBinary(const char *dataFork, const char *payload) {
	Bytes fork = makeRawFork(payload);
	Common::MemoryReadStream data((const byte *)dataFork, strlen(dataFork));
	Common::MemoryReadStream res(fork.data(), fork.size());
	Common::MemoryWriteStreamDynamic out(DisposeAfterUse::YES);
	TimeDate t = {};
	t.tm_year = 100;
	t.tm_mday = 1;
	Common::MacResManager::writeMacBinary(&out, &data, &res, "game", Common::MacFinderInfo(), &t, &t);
	return Bytes(out.getData(), out.size());
}

static Bytes makeBytes(const char *s) {
	return Bytes((const byte *)s, strlen(s));
}

// An in-memory archive. Its members are not FSNodes, like files in a
// zip or installer, so MacResManager must not fall back to the disk.
class MemArchive : public Common::Archive {
public:
	void add(const char *path, const Bytes &data) {
		_files[Common::Path(path)] = data;
	}

	bool hasFile(const Common::Path &path) const override {
		return _files.contains(path);
	}

	int listMembers(Common::ArchiveMemberList &list) const override {
		for (const auto &f : _files)
			list.push_back(Common::ArchiveMemberPtr(new Common::GenericArchiveMember(f._key, *this)));
		return _files.size();
	}

	const Common::ArchiveMemberPtr getMember(const Common::Path &path) const override {
		if (!hasFile(path))
			return Common::ArchiveMemberPtr();
		return Common::ArchiveMemberPtr(new Common::GenericArchiveMember(path, *this));
	}

	Common::SeekableReadStream *createReadStreamForMember(const Common::Path &path) const override {
		if (!hasFile(path))
			return nullptr;
		const Bytes &b = _files[path];
		byte *copy = (byte *)malloc(b.size() + 1);
		if (!b.empty())
			memcpy(copy, b.data(), b.size());
		return new Common::MemoryReadStream(copy, b.size(), DisposeAfterUse::YES);
	}

private:
	Common::HashMap<Common::Path, Bytes, Common::Path::IgnoreCase_Hash, Common::Path::IgnoreCase_EqualTo> _files;
};

// Maps names to real files, like the detector's FileMapArchive: members
// are FSNodes, so MacResManager may look for __MACOSX next to them.
class NodeArchive : public Common::Archive {
public:
	void add(const char *name, const Common::FSNode &node) {
		_files[Common::Path(name)] = node;
	}

	bool hasFile(const Common::Path &path) const override {
		return _files.contains(path);
	}

	int listMembers(Common::ArchiveMemberList &list) const override {
		for (const auto &f : _files)
			list.push_back(Common::ArchiveMemberPtr(new Common::FSNode(f._value)));
		return _files.size();
	}

	const Common::ArchiveMemberPtr getMember(const Common::Path &path) const override {
		if (!hasFile(path))
			return Common::ArchiveMemberPtr();
		return Common::ArchiveMemberPtr(new Common::FSNode(_files[path]));
	}

	Common::SeekableReadStream *createReadStreamForMember(const Common::Path &path) const override {
		if (!hasFile(path))
			return nullptr;
		return _files[path].createReadStream();
	}

private:
	Common::HashMap<Common::Path, Common::FSNode, Common::Path::IgnoreCase_Hash, Common::Path::IgnoreCase_EqualTo> _files;
};

static Common::String readTestResource(Common::MacResManager &res) {
	Common::ScopedPtr<Common::SeekableReadStream> s(res.getResource(kTestType, kTestId));
	if (!s)
		return "<none>";
	Common::String out;
	while (true) {
		byte c = s->readByte();
		if (s->eos())
			break;
		out += (char)c;
	}
	return out;
}

static Common::String readAll(Common::SeekableReadStream *s) {
	Common::ScopedPtr<Common::SeekableReadStream> owned(s);
	if (!s)
		return "<none>";
	Common::String out;
	while (true) {
		byte c = s->readByte();
		if (s->eos())
			break;
		out += (char)c;
	}
	return out;
}

static bool writeFile(const char *path, const Bytes &data) {
	Common::DumpFile f;
	if (!f.open(Common::Path(path), true))
		return false;
	f.write(data.data(), data.size());
	f.close();
	return true;
}

} // End of namespace MacResManTest

class MacResManagerTestSuite : public CxxTest::TestSuite {
public:
	void setUp() {
#if NULL_OSYSTEM_IS_AVAILABLE
		Common::install_null_g_system();
#endif
	}

	void tearDown() {
#if NULL_OSYSTEM_IS_AVAILABLE
		Common::uninstall_null_g_system();
#endif
	}

	void test_raw_fork_builder_is_valid() {
		using namespace MacResManTest;
		MemArchive a;
		a.add("game.rsrc", makeRawFork("raw"));
		Common::MacResManager res;
		TS_ASSERT(res.open(Common::Path("game"), a));
		TS_ASSERT(res.hasResFork());
		TS_ASSERT_EQUALS(readTestResource(res), "raw");
	}

	void test_missing_file_is_not_found() {
		using namespace MacResManTest;
		MemArchive a;
		a.add("other", makeBytes("x"));
		Common::MacResManager res;
		TS_ASSERT(!res.open(Common::Path("missing"), a));
		TS_ASSERT(!res.open(Common::Path("dir/missing"), a));
		TS_ASSERT(!res.hasResFork());
	}

	void test_missing_data_fork_is_not_found() {
		using namespace MacResManTest;
		MemArchive a;
		a.add("other", makeBytes("x"));
		TS_ASSERT_EQUALS(readAll(Common::MacResManager::openFileOrDataFork(Common::Path("missing"), a)), "<none>");
	}

	void test_appledouble_next_to_file() {
		using namespace MacResManTest;
		MemArchive a;
		a.add("game", makeBytes("data"));
		a.add("._game", makeAppleDouble("dot-underscore"));
		Common::MacResManager res;
		TS_ASSERT(res.open(Common::Path("game"), a));
		TS_ASSERT_EQUALS(readTestResource(res), "dot-underscore");
	}

	void test_appledouble_without_data_fork() {
		using namespace MacResManTest;
		MemArchive a;
		a.add("._game", makeAppleDouble("rsrc-only"));
		Common::MacResManager res;
		TS_ASSERT(res.open(Common::Path("game"), a));
		TS_ASSERT_EQUALS(readTestResource(res), "rsrc-only");
	}

	void test_appledouble_in_macosx_folder() {
		using namespace MacResManTest;
		MemArchive a;
		a.add("game", makeBytes("data"));
		a.add("__MACOSX/._game", makeAppleDouble("macosx"));
		Common::MacResManager res;
		TS_ASSERT(res.open(Common::Path("game"), a));
		TS_ASSERT_EQUALS(readTestResource(res), "macosx");
	}

	void test_appledouble_in_macosx_folder_for_subdir_file() {
		using namespace MacResManTest;
		MemArchive a;
		a.add("disk/game", makeBytes("data"));
		a.add("__MACOSX/disk/._game", makeAppleDouble("macosx-root"));
		Common::MacResManager res;
		TS_ASSERT(res.open(Common::Path("disk/game"), a));
		TS_ASSERT_EQUALS(readTestResource(res), "macosx-root");

		MemArchive b;
		b.add("disk/game", makeBytes("data"));
		b.add("disk/__MACOSX/._game", makeAppleDouble("macosx-inner"));
		Common::MacResManager res2;
		TS_ASSERT(res2.open(Common::Path("disk/game"), b));
		TS_ASSERT_EQUALS(readTestResource(res2), "macosx-inner");
	}

	void test_appledouble_in_rsrc_file() {
		using namespace MacResManTest;
		MemArchive a;
		a.add("game.rsrc", makeAppleDouble("appledouble-rsrc"));
		Common::MacResManager res;
		TS_ASSERT(res.open(Common::Path("game"), a));
		TS_ASSERT_EQUALS(readTestResource(res), "appledouble-rsrc");
	}

	void test_macbinary_with_and_without_bin_suffix() {
		using namespace MacResManTest;
		MemArchive a;
		a.add("game.bin", makeMacBinary("datafork", "macbinary"));
		Common::MacResManager res;
		TS_ASSERT(res.open(Common::Path("game"), a));
		TS_ASSERT_EQUALS(readTestResource(res), "macbinary");
		TS_ASSERT_EQUALS(readAll(Common::MacResManager::openFileOrDataFork(Common::Path("game"), a)), "datafork");

		MemArchive b;
		b.add("game", makeMacBinary("datafork2", "macbinary2"));
		Common::MacResManager res2;
		TS_ASSERT(res2.open(Common::Path("game"), b));
		TS_ASSERT_EQUALS(readTestResource(res2), "macbinary2");
		TS_ASSERT_EQUALS(readAll(Common::MacResManager::openFileOrDataFork(Common::Path("game"), b)), "datafork2");
	}

	void test_plain_file_without_fork_still_opens() {
		using namespace MacResManTest;
		MemArchive a;
		a.add("game", makeBytes("plain"));
		Common::MacResManager res;
		TS_ASSERT(res.open(Common::Path("game"), a));
		TS_ASSERT(!res.hasResFork());
		TS_ASSERT_EQUALS(readAll(Common::MacResManager::openFileOrDataFork(Common::Path("game"), a)), "plain");
	}

	void test_data_fork_with_data_suffix() {
		using namespace MacResManTest;
		MemArchive a;
		a.add("game.data", makeBytes("suffix"));
		TS_ASSERT_EQUALS(readAll(Common::MacResManager::openFileOrDataFork(Common::Path("game"), a)), "suffix");
	}

	// A file that is not in the archive must not be found through a
	// __MACOSX folder relative to the current working directory.
	void test_missing_file_ignores_working_directory() {
#if NULL_OSYSTEM_IS_AVAILABLE
		using namespace MacResManTest;
		TS_ASSERT(writeFile("test/macresman-tmp/cwd/__MACOSX/._ghost", makeAppleDouble("cwd")));
		TS_ASSERT(writeFile("test/macresman-tmp/cwd/__MACOSX/._ghost2", makeAppleDouble("cwd")));
		TS_ASSERT(Common::FSNode(Common::Path("test/macresman-tmp/cwd/__MACOSX/._ghost")).exists());

		MemArchive empty;
		Common::MacResManager res;
		TS_ASSERT(!res.open(Common::Path("test/macresman-tmp/cwd/ghost"), empty));
		TS_ASSERT(!res.hasResFork());

		// Same through the detector-style archive holding unrelated files.
		NodeArchive nodes;
		nodes.add("unrelated", Common::FSNode(Common::Path("test/macresman-tmp/cwd/__MACOSX/._ghost")));
		Common::MacResManager res2;
		TS_ASSERT(!res2.open(Common::Path("test/macresman-tmp/cwd/ghost2"), nodes));
		TS_ASSERT(!res2.open(Common::Path("ghost2"), nodes));
#endif
	}

	// A real file on disk still gets the resource fork from a __MACOSX
	// folder next to it, even when the archive does not list that folder.
	void test_real_file_finds_macosx_sidecar_on_disk() {
#if NULL_OSYSTEM_IS_AVAILABLE
		using namespace MacResManTest;
		TS_ASSERT(writeFile("test/macresman-tmp/real/data", makeBytes("data")));
		TS_ASSERT(writeFile("test/macresman-tmp/real/__MACOSX/._data", makeAppleDouble("sidecar")));

		NodeArchive nodes;
		nodes.add("data", Common::FSNode(Common::Path("test/macresman-tmp/real/data")));
		Common::MacResManager res;
		TS_ASSERT(res.open(Common::Path("data"), nodes));
		TS_ASSERT(res.hasResFork());
		TS_ASSERT_EQUALS(readTestResource(res), "sidecar");
#endif
	}
};
