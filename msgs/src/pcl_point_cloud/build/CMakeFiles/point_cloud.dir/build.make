# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 2.8

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list

# Suppress display of executed commands.
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
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The program to use to edit the cache.
CMAKE_EDIT_COMMAND = /usr/bin/ccmake

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/kevin/research/gazebo/msgs/src/pcl_point_cloud

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/build

# Include any dependencies generated for this target.
include CMakeFiles/point_cloud.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/point_cloud.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/point_cloud.dir/flags.make

point_cloud.pb.cc: ../point_cloud.proto
	$(CMAKE_COMMAND) -E cmake_progress_report /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/build/CMakeFiles $(CMAKE_PROGRESS_1)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold "Running C++ protocol buffer compiler on point_cloud.proto"
	/usr/bin/protoc --cpp_out /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/build -I /home/kevin/research/gazebo/msgs/src/pcl_point_cloud /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/point_cloud.proto

point_cloud.pb.h: point_cloud.pb.cc

point_type.pb.cc: ../point_type.proto
	$(CMAKE_COMMAND) -E cmake_progress_report /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/build/CMakeFiles $(CMAKE_PROGRESS_2)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --blue --bold "Running C++ protocol buffer compiler on point_type.proto"
	/usr/bin/protoc --cpp_out /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/build -I /home/kevin/research/gazebo/msgs/src/pcl_point_cloud /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/point_type.proto

point_type.pb.h: point_type.pb.cc

CMakeFiles/point_cloud.dir/point_cloud.pb.cc.o: CMakeFiles/point_cloud.dir/flags.make
CMakeFiles/point_cloud.dir/point_cloud.pb.cc.o: point_cloud.pb.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/build/CMakeFiles $(CMAKE_PROGRESS_3)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object CMakeFiles/point_cloud.dir/point_cloud.pb.cc.o"
	/usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/point_cloud.dir/point_cloud.pb.cc.o -c /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/build/point_cloud.pb.cc

CMakeFiles/point_cloud.dir/point_cloud.pb.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/point_cloud.dir/point_cloud.pb.cc.i"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/build/point_cloud.pb.cc > CMakeFiles/point_cloud.dir/point_cloud.pb.cc.i

CMakeFiles/point_cloud.dir/point_cloud.pb.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/point_cloud.dir/point_cloud.pb.cc.s"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/build/point_cloud.pb.cc -o CMakeFiles/point_cloud.dir/point_cloud.pb.cc.s

CMakeFiles/point_cloud.dir/point_cloud.pb.cc.o.requires:
.PHONY : CMakeFiles/point_cloud.dir/point_cloud.pb.cc.o.requires

CMakeFiles/point_cloud.dir/point_cloud.pb.cc.o.provides: CMakeFiles/point_cloud.dir/point_cloud.pb.cc.o.requires
	$(MAKE) -f CMakeFiles/point_cloud.dir/build.make CMakeFiles/point_cloud.dir/point_cloud.pb.cc.o.provides.build
.PHONY : CMakeFiles/point_cloud.dir/point_cloud.pb.cc.o.provides

CMakeFiles/point_cloud.dir/point_cloud.pb.cc.o.provides.build: CMakeFiles/point_cloud.dir/point_cloud.pb.cc.o

CMakeFiles/point_cloud.dir/point_type.pb.cc.o: CMakeFiles/point_cloud.dir/flags.make
CMakeFiles/point_cloud.dir/point_type.pb.cc.o: point_type.pb.cc
	$(CMAKE_COMMAND) -E cmake_progress_report /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/build/CMakeFiles $(CMAKE_PROGRESS_4)
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Building CXX object CMakeFiles/point_cloud.dir/point_type.pb.cc.o"
	/usr/bin/c++   $(CXX_DEFINES) $(CXX_FLAGS) -o CMakeFiles/point_cloud.dir/point_type.pb.cc.o -c /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/build/point_type.pb.cc

CMakeFiles/point_cloud.dir/point_type.pb.cc.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/point_cloud.dir/point_type.pb.cc.i"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -E /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/build/point_type.pb.cc > CMakeFiles/point_cloud.dir/point_type.pb.cc.i

CMakeFiles/point_cloud.dir/point_type.pb.cc.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/point_cloud.dir/point_type.pb.cc.s"
	/usr/bin/c++  $(CXX_DEFINES) $(CXX_FLAGS) -S /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/build/point_type.pb.cc -o CMakeFiles/point_cloud.dir/point_type.pb.cc.s

CMakeFiles/point_cloud.dir/point_type.pb.cc.o.requires:
.PHONY : CMakeFiles/point_cloud.dir/point_type.pb.cc.o.requires

CMakeFiles/point_cloud.dir/point_type.pb.cc.o.provides: CMakeFiles/point_cloud.dir/point_type.pb.cc.o.requires
	$(MAKE) -f CMakeFiles/point_cloud.dir/build.make CMakeFiles/point_cloud.dir/point_type.pb.cc.o.provides.build
.PHONY : CMakeFiles/point_cloud.dir/point_type.pb.cc.o.provides

CMakeFiles/point_cloud.dir/point_type.pb.cc.o.provides.build: CMakeFiles/point_cloud.dir/point_type.pb.cc.o

# Object files for target point_cloud
point_cloud_OBJECTS = \
"CMakeFiles/point_cloud.dir/point_cloud.pb.cc.o" \
"CMakeFiles/point_cloud.dir/point_type.pb.cc.o"

# External object files for target point_cloud
point_cloud_EXTERNAL_OBJECTS =

libpoint_cloud.so: CMakeFiles/point_cloud.dir/point_cloud.pb.cc.o
libpoint_cloud.so: CMakeFiles/point_cloud.dir/point_type.pb.cc.o
libpoint_cloud.so: CMakeFiles/point_cloud.dir/build.make
libpoint_cloud.so: libpoint_type.so
libpoint_cloud.so: /usr/lib/x86_64-linux-gnu/libprotobuf.so
libpoint_cloud.so: CMakeFiles/point_cloud.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --red --bold "Linking CXX shared library libpoint_cloud.so"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/point_cloud.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/point_cloud.dir/build: libpoint_cloud.so
.PHONY : CMakeFiles/point_cloud.dir/build

CMakeFiles/point_cloud.dir/requires: CMakeFiles/point_cloud.dir/point_cloud.pb.cc.o.requires
CMakeFiles/point_cloud.dir/requires: CMakeFiles/point_cloud.dir/point_type.pb.cc.o.requires
.PHONY : CMakeFiles/point_cloud.dir/requires

CMakeFiles/point_cloud.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/point_cloud.dir/cmake_clean.cmake
.PHONY : CMakeFiles/point_cloud.dir/clean

CMakeFiles/point_cloud.dir/depend: point_cloud.pb.cc
CMakeFiles/point_cloud.dir/depend: point_cloud.pb.h
CMakeFiles/point_cloud.dir/depend: point_type.pb.cc
CMakeFiles/point_cloud.dir/depend: point_type.pb.h
	cd /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/kevin/research/gazebo/msgs/src/pcl_point_cloud /home/kevin/research/gazebo/msgs/src/pcl_point_cloud /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/build /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/build /home/kevin/research/gazebo/msgs/src/pcl_point_cloud/build/CMakeFiles/point_cloud.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/point_cloud.dir/depend

