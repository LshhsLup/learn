# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.22

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/lsh/server/learn

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/lsh/server/learn/build

# Include any dependencies generated for this target.
include CMakeFiles/lsh.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/lsh.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/lsh.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/lsh.dir/flags.make

CMakeFiles/lsh.dir/lsh/config.cpp.o: CMakeFiles/lsh.dir/flags.make
CMakeFiles/lsh.dir/lsh/config.cpp.o: ../lsh/config.cpp
CMakeFiles/lsh.dir/lsh/config.cpp.o: CMakeFiles/lsh.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/lsh/server/learn/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/lsh.dir/lsh/config.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/lsh.dir/lsh/config.cpp.o -MF CMakeFiles/lsh.dir/lsh/config.cpp.o.d -o CMakeFiles/lsh.dir/lsh/config.cpp.o -c /home/lsh/server/learn/lsh/config.cpp

CMakeFiles/lsh.dir/lsh/config.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/lsh.dir/lsh/config.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/lsh/server/learn/lsh/config.cpp > CMakeFiles/lsh.dir/lsh/config.cpp.i

CMakeFiles/lsh.dir/lsh/config.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/lsh.dir/lsh/config.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/lsh/server/learn/lsh/config.cpp -o CMakeFiles/lsh.dir/lsh/config.cpp.s

CMakeFiles/lsh.dir/lsh/fiber.cpp.o: CMakeFiles/lsh.dir/flags.make
CMakeFiles/lsh.dir/lsh/fiber.cpp.o: ../lsh/fiber.cpp
CMakeFiles/lsh.dir/lsh/fiber.cpp.o: CMakeFiles/lsh.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/lsh/server/learn/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/lsh.dir/lsh/fiber.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/lsh.dir/lsh/fiber.cpp.o -MF CMakeFiles/lsh.dir/lsh/fiber.cpp.o.d -o CMakeFiles/lsh.dir/lsh/fiber.cpp.o -c /home/lsh/server/learn/lsh/fiber.cpp

CMakeFiles/lsh.dir/lsh/fiber.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/lsh.dir/lsh/fiber.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/lsh/server/learn/lsh/fiber.cpp > CMakeFiles/lsh.dir/lsh/fiber.cpp.i

CMakeFiles/lsh.dir/lsh/fiber.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/lsh.dir/lsh/fiber.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/lsh/server/learn/lsh/fiber.cpp -o CMakeFiles/lsh.dir/lsh/fiber.cpp.s

CMakeFiles/lsh.dir/lsh/log.cpp.o: CMakeFiles/lsh.dir/flags.make
CMakeFiles/lsh.dir/lsh/log.cpp.o: ../lsh/log.cpp
CMakeFiles/lsh.dir/lsh/log.cpp.o: CMakeFiles/lsh.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/lsh/server/learn/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object CMakeFiles/lsh.dir/lsh/log.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/lsh.dir/lsh/log.cpp.o -MF CMakeFiles/lsh.dir/lsh/log.cpp.o.d -o CMakeFiles/lsh.dir/lsh/log.cpp.o -c /home/lsh/server/learn/lsh/log.cpp

CMakeFiles/lsh.dir/lsh/log.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/lsh.dir/lsh/log.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/lsh/server/learn/lsh/log.cpp > CMakeFiles/lsh.dir/lsh/log.cpp.i

CMakeFiles/lsh.dir/lsh/log.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/lsh.dir/lsh/log.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/lsh/server/learn/lsh/log.cpp -o CMakeFiles/lsh.dir/lsh/log.cpp.s

CMakeFiles/lsh.dir/lsh/thread.cpp.o: CMakeFiles/lsh.dir/flags.make
CMakeFiles/lsh.dir/lsh/thread.cpp.o: ../lsh/thread.cpp
CMakeFiles/lsh.dir/lsh/thread.cpp.o: CMakeFiles/lsh.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/lsh/server/learn/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Building CXX object CMakeFiles/lsh.dir/lsh/thread.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/lsh.dir/lsh/thread.cpp.o -MF CMakeFiles/lsh.dir/lsh/thread.cpp.o.d -o CMakeFiles/lsh.dir/lsh/thread.cpp.o -c /home/lsh/server/learn/lsh/thread.cpp

CMakeFiles/lsh.dir/lsh/thread.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/lsh.dir/lsh/thread.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/lsh/server/learn/lsh/thread.cpp > CMakeFiles/lsh.dir/lsh/thread.cpp.i

CMakeFiles/lsh.dir/lsh/thread.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/lsh.dir/lsh/thread.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/lsh/server/learn/lsh/thread.cpp -o CMakeFiles/lsh.dir/lsh/thread.cpp.s

CMakeFiles/lsh.dir/lsh/util.cpp.o: CMakeFiles/lsh.dir/flags.make
CMakeFiles/lsh.dir/lsh/util.cpp.o: ../lsh/util.cpp
CMakeFiles/lsh.dir/lsh/util.cpp.o: CMakeFiles/lsh.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/lsh/server/learn/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_5) "Building CXX object CMakeFiles/lsh.dir/lsh/util.cpp.o"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/lsh.dir/lsh/util.cpp.o -MF CMakeFiles/lsh.dir/lsh/util.cpp.o.d -o CMakeFiles/lsh.dir/lsh/util.cpp.o -c /home/lsh/server/learn/lsh/util.cpp

CMakeFiles/lsh.dir/lsh/util.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/lsh.dir/lsh/util.cpp.i"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/lsh/server/learn/lsh/util.cpp > CMakeFiles/lsh.dir/lsh/util.cpp.i

CMakeFiles/lsh.dir/lsh/util.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/lsh.dir/lsh/util.cpp.s"
	/usr/bin/c++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/lsh/server/learn/lsh/util.cpp -o CMakeFiles/lsh.dir/lsh/util.cpp.s

# Object files for target lsh
lsh_OBJECTS = \
"CMakeFiles/lsh.dir/lsh/config.cpp.o" \
"CMakeFiles/lsh.dir/lsh/fiber.cpp.o" \
"CMakeFiles/lsh.dir/lsh/log.cpp.o" \
"CMakeFiles/lsh.dir/lsh/thread.cpp.o" \
"CMakeFiles/lsh.dir/lsh/util.cpp.o"

# External object files for target lsh
lsh_EXTERNAL_OBJECTS =

../lib/liblsh.so: CMakeFiles/lsh.dir/lsh/config.cpp.o
../lib/liblsh.so: CMakeFiles/lsh.dir/lsh/fiber.cpp.o
../lib/liblsh.so: CMakeFiles/lsh.dir/lsh/log.cpp.o
../lib/liblsh.so: CMakeFiles/lsh.dir/lsh/thread.cpp.o
../lib/liblsh.so: CMakeFiles/lsh.dir/lsh/util.cpp.o
../lib/liblsh.so: CMakeFiles/lsh.dir/build.make
../lib/liblsh.so: CMakeFiles/lsh.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/lsh/server/learn/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_6) "Linking CXX shared library ../lib/liblsh.so"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/lsh.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/lsh.dir/build: ../lib/liblsh.so
.PHONY : CMakeFiles/lsh.dir/build

CMakeFiles/lsh.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/lsh.dir/cmake_clean.cmake
.PHONY : CMakeFiles/lsh.dir/clean

CMakeFiles/lsh.dir/depend:
	cd /home/lsh/server/learn/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/lsh/server/learn /home/lsh/server/learn /home/lsh/server/learn/build /home/lsh/server/learn/build /home/lsh/server/learn/build/CMakeFiles/lsh.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/lsh.dir/depend

