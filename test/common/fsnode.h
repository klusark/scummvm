#include <cxxtest/TestSuite.h>

#include "common/file.h"
#include "common/fs.h"
#include "common/path.h"
#include "common/ptr.h"
#include "common/stream.h"
#include "../system/null_osystem.h"

// FSNode::tryCreateReadStream() opens a file without asking the filesystem
// first whether it exists. It must give the same results as
// createReadStream() for missing files, directories and real files.
class FSNodeTestSuite : public CxxTest::TestSuite {
	static Common::String readAll(Common::SeekableReadStream *s) {
		Common::ScopedPtr<Common::SeekableReadStream> owned(s);
		if (!s)
			return "<none>";
		return s->readString();
	}

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

	void test_empty_node() {
		Common::FSNode node;
		TS_ASSERT_EQUALS(readAll(node.tryCreateReadStream()), "<none>");
		TS_ASSERT_EQUALS(readAll(node.createReadStream()), "<none>");
	}

#if NULL_OSYSTEM_IS_AVAILABLE
	void test_missing_file() {
		Common::FSNode node(Common::Path("test/fsnode-tmp/does-not-exist.bin"));
		TS_ASSERT(!node.exists());
		TS_ASSERT_EQUALS(readAll(node.tryCreateReadStream()), "<none>");
		TS_ASSERT_EQUALS(readAll(node.createReadStream()), "<none>");
	}

	void test_missing_child() {
		Common::FSNode dir(Common::Path("test"));
		TS_ASSERT(dir.isDirectory());
		Common::FSNode child = dir.getChild("fsnode-no-such-file");
		TS_ASSERT(!child.exists());
		TS_ASSERT_EQUALS(readAll(child.tryCreateReadStream()), "<none>");
		TS_ASSERT_EQUALS(readAll(child.createReadStream()), "<none>");
	}

	void test_directory() {
		Common::FSNode dir(Common::Path("test"));
		TS_ASSERT(dir.isDirectory());
		TS_ASSERT_EQUALS(readAll(dir.tryCreateReadStream()), "<none>");
		TS_ASSERT_EQUALS(readAll(dir.createReadStream()), "<none>");
	}

	void test_file() {
		Common::DumpFile out;
		TS_ASSERT(out.open(Common::Path("test/fsnode-tmp/hello.txt"), true));
		out.writeString("hello");
		out.close();

		Common::FSNode node(Common::Path("test/fsnode-tmp/hello.txt"));
		TS_ASSERT(node.exists());
		TS_ASSERT_EQUALS(readAll(node.tryCreateReadStream()), "hello");
		TS_ASSERT_EQUALS(readAll(node.createReadStream()), "hello");
	}

	// A file listed by getChildren() and opened through the listed node.
	void test_file_from_listing() {
		Common::DumpFile out;
		TS_ASSERT(out.open(Common::Path("test/fsnode-tmp/listed/a.txt"), true));
		out.writeString("listed");
		out.close();

		Common::FSNode dir(Common::Path("test/fsnode-tmp/listed"));
		Common::FSList files;
		TS_ASSERT(dir.getChildren(files, Common::FSNode::kListFilesOnly));
		TS_ASSERT_EQUALS(files.size(), 1u);
		if (!files.empty())
			TS_ASSERT_EQUALS(readAll(files.front().tryCreateReadStream()), "listed");
	}
#endif
};
