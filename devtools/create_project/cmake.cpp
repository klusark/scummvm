/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "cmake.h"

#include <fstream>

namespace CreateProjectTool {

CMakeProvider::CMakeProvider(StringList &global_warnings, std::map<std::string, StringList> &project_warnings, const int version)
	: ProjectProvider(global_warnings, project_warnings, version) {
}

void CMakeProvider::createWorkspace(const BuildSetup &setup) {
	std::string filename = setup.outputDir + "/CMakeLists.txt";
	std::ofstream workspace(filename.c_str());
	if (!workspace)
		error("Could not open \"" + filename + "\" for writing");

	workspace << "cmake_minimum_required(VERSION 3.2)\n"
			"project(" << setup.projectDescription << ")\n\n"
			"Include(FindSDL)\n"
			"Include(FindFreetype)\n"
			"Include(FindZLIB)\n"
			"Find_Package(SDL REQUIRED)\n"
			"Find_Package(Freetype REQUIRED)\n"
			"Find_Package(ZLIB REQUIRED)\n"
			"include_directories(${" << setup.projectDescription << "_SOURCE_DIR} ${" << setup.projectDescription << "_SOURCE_DIR}/engines\n"
			"$ENV{"<<LIBS_DEFINE<<"}/include\n"
			"${SDL_INCLUDE_DIR}\n"
			"${FREETYPE_INCLUDE_DIRS}\n"
			"${ZLIB_INCLUDE_DIRS}\n"
			")\n\n";

	writeWarnings(workspace);
	writeDefines(setup.defines, workspace);
}

// HACK We need to pre-process library names
//      since the MSVC and mingw precompiled
//      libraries have different names :(
static std::string processLibraryName(std::string name) {
	// Remove "_static" in lib name
	size_t pos = name.find("_static");
	if (pos != std::string::npos)
		return name.replace(pos, 7, "");

	// Remove "-static" in lib name
	pos = name.find("-static");
	if (pos != std::string::npos)
		return name.replace(pos, 7, "");

	// Replace "zlib" by "libz"
	if (name == "zlib")
		return "libz";

	return name;
}

void CMakeProvider::createProjectFile(const std::string &name, const std::string &, const BuildSetup &setup, const std::string &moduleDir,
                                           const StringList &includeList, const StringList &excludeList) {

	const std::string projectFile = setup.outputDir + "/CMakeLists.txt";
	std::ofstream project(projectFile.c_str(), std::ofstream::out | std::ofstream::app);
	if (!project)
		error("Could not open \"" + projectFile + "\" for writing");

	if (name == setup.projectName) {
		std::string libraries;

		project << "add_executable(" << name << "\n";

	} else {
		project << "add_library(" << name << "\n";
	}

	std::string modulePath;
	if (!moduleDir.compare(0, setup.srcDir.size(), setup.srcDir)) {
		modulePath = moduleDir.substr(setup.srcDir.size());
		if (!modulePath.empty() && modulePath.at(0) == '/')
			modulePath.erase(0, 1);
	}

	if (modulePath.size())
		addFilesToProject(moduleDir, project, includeList, excludeList, setup.filePrefix + '/' + modulePath);
	else
		addFilesToProject(moduleDir, project, includeList, excludeList, setup.filePrefix);


	project << ")\n";

	if (name == setup.projectName) {
		std::string libraries;

		project << "target_link_libraries(" << name << "\n";
		project << "\t${SDL_LIBRARY}\n";
		project << "\t${FREETYPE_LIBRARIES}\n";
		project << "\t${ZLIB_LIBRARIES}\n";
		project << "\tGL\n";
		//for (StringList::const_iterator i = setup.libraries.begin(); i != setup.libraries.end(); ++i) {
		//	project << "\t" << *i << "\n";
		//}
		for (UUIDMap::const_iterator i = _uuidMap.begin(); i != _uuidMap.end(); ++i) {
			if (i->first == setup.projectName)
				continue;

			project << "\t" << i->first << "\n";
		}
		project << ")\n";
	}

}

void CMakeProvider::writeWarnings(std::ofstream &output) const {
	output << "add_definitions(\n";
	// Global warnings
	for (StringList::const_iterator i = _globalWarnings.begin(); i != _globalWarnings.end(); ++i) {
		output << "\t" << *i << "\n";
	}
	output << ")\n";
	// Check for project-specific warnings:
	/*std::map<std::string, StringList>::iterator warningsIterator = _projectWarnings.find(name);
	if (warningsIterator != _projectWarnings.end())
		for (StringList::const_iterator i = warningsIterator->second.begin(); i != warningsIterator->second.end(); ++i)
			output << "\t\t\t\t\t<Add option=\"" << *i << "\" />\n";*/

}

void CMakeProvider::writeDefines(const StringList &defines, std::ofstream &output) const {
	output << "add_definitions(\n";
	for (StringList::const_iterator i = defines.begin(); i != defines.end(); ++i) {
		output << "\t-D" << *i << "\n";
	}
	output << ")\n";
}

void CMakeProvider::writeFileListToProject(const FileNode &dir, std::ofstream &projectFile, const int indentation,
                                                const StringList &duplicate, const std::string &objPrefix, const std::string &filePrefix) {

	for (FileNode::NodeList::const_iterator i = dir.children.begin(); i != dir.children.end(); ++i) {
		const FileNode *node = *i;

		if (!node->children.empty()) {
			writeFileListToProject(*node, projectFile, indentation + 1, duplicate, objPrefix + node->name + '_', filePrefix + node->name + '/');
		} else {
			std::string name, ext;
			splitFilename(node->name, name, ext);

			if (ext == "rc") {
				/*projectFile << "\t\t<Unit filename=\"" << convertPathToWin(filePrefix + node->name) << "\">\n"
				               "\t\t\t<Option compilerVar=\"WINDRES\" />\n"
				               "\t\t</Unit>\n";*/
			} else if (ext == "asm") {
				/*projectFile << "\t\t<Unit filename=\"" << convertPathToWin(filePrefix + node->name) << "\">\n"
				               "\t\t\t<Option compiler=\"gcc\" use=\"1\" buildCommand=\"$(" << LIBS_DEFINE << ")bin/nasm.exe -f win32 -g $file -o $object\" />"
				               "\t\t</Unit>\n";*/
			} else {
				projectFile << "\t" << filePrefix + node->name << "\n";
			}
		}
	}
}

void CMakeProvider::writeReferences(const BuildSetup &setup, std::ofstream &output) {
	output << "\t\t<Project filename=\"" << setup.projectName << ".cbp\" active=\"1\">\n";

	for (UUIDMap::const_iterator i = _uuidMap.begin(); i != _uuidMap.end(); ++i) {
		if (i->first == " << PROJECT_NAME << ")
			continue;

		output << "\t\t\t<Depends filename=\"" << i->first << ".cbp\" />\n";
	}

	output << "\t\t</Project>\n";
}

const char *CMakeProvider::getProjectExtension() {
	return ".txt";
}


} // End of CreateProjectTool namespace
