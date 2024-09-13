# This is an automatically generated configuration file.
# It is recommended not to modify this file or any other included in the .mk nd m4 directories.

ACLOCAL_AMFLAGS = -I m4
include .mk/my_conf.am

# Example
# =======
# **Binaries** must be placed at bin folder
# bin_PROGRAMS = bin/my_program
# bin_my_program_SOURCES = src/main.c

# **Libraries** must be placed at lib folder
# Additional configuration for library. Meaning of version info:
# The '-version-info' option is used to specify versioning information for shared libraries.
# - 'current' represents the current interface version.
# - 'revision' is incremented for minor changes or bug fixes.
# - 'age' specifies how many previous versions are compatible with the current one.
# This helps manage compatibility and versioning for shared library users.
# lib_LTLIBRARIES =  lib/libmylib.la
# lib_mylib_la_SOURCES = src/mylib.c include/mylib.h
# lib_mylib_la_LDFLAGS = -version-info 1:0:0

# **Tests** must be placed at tests folder
# check_PROGRAMS = tests/mylib_test
# tests_mylib_test_SOURCES = tests/mylib_test.cpp include/mylib.h
# tests_mylib_test_CXXFLAGS = $(GTEST_INCLUDE)
# tests_mylib_test_LDADD = lib/libmylib.la $(GTEST_LIBS)
# tests_mylib_test_LDFLAGS = -static -lpthread

# Add tests to compilation process (space separated list)
# TESTS = tests/mylib_test

lib_LTLIBRARIES = lib/libthreadpool.la lib/libsaurion.la

lib_libthreadpool_la_SOURCES = src/threadpool.cpp include/threadpool.hpp src/cthreadpool.cpp include/cthreadpool.hpp
lib_libthreadpool_la_LDFLAGS = -version-info 1:0:0

lib_libsaurion_la_SOURCES = src/linked_list.c include/linked_list.h src/low_saurion.c include/low_saurion.h src/saurion.cpp include/saurion.hpp include/config.h
lib_libsaurion_la_LDFLAGS = -version-info 1:0:0

check_PROGRAMS = tests/client tests/threadpool_test tests/saurion_test tests/linked_list_test

tests_client_SOURCES = tests/client.cpp

tests_threadpool_test_SOURCES = tests/threadpool_test.cpp include/threadpool.hpp
tests_threadpool_test_CXXFLAGS = $(GTEST_INCLUDE)
tests_threadpool_test_LDADD = lib/libthreadpool.la $(GTEST_LIBS)
tests_threadpool_test_LDFLAGS = -static -lpthread

tests_linked_list_test_SOURCES = tests/linked_list_test.cpp include/linked_list.h
tests_linked_list_test_CXXFLAGS = $(GTEST_INCLUDE)
tests_linked_list_test_LDADD = lib/libsaurion.la lib/libthreadpool.la $(GTEST_LIBS)
tests_linked_list_test_LDFLAGS = -static -lpthread

tests_saurion_test_SOURCES = tests/low_saurion_test.cpp include/low_saurion.h include/saurion.hpp include/low_saurion_secret.h
tests_saurion_test_CXXFLAGS = $(GTEST_INCLUDE)
tests_saurion_test_LDADD = lib/libsaurion.la lib/libthreadpool.la $(GTEST_LIBS)
tests_saurion_test_LDFLAGS = -static -lpthread -luring

TESTS = tests/threadpool_test tests/linked_list_test tests/saurion_test
