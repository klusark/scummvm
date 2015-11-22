project(create_project)

set(SOURCE_FILES
	../codeblocks.cpp
	../codeblocks.h
	../cmake.cpp
	../cmake.h
	../create_project.cpp
	../create_project.h
	../msbuild.cpp
	../msbuild.h
	../msvc.cpp
	../msvc.h
	../visualstudio.cpp
	../visualstudio.h
	../xcode.cpp
	../xcode.h
	)

add_executable(create_project ${SOURCE_FILES})
