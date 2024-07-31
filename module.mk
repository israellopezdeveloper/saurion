# ==================================================================================
# Makefile Help
# ==================================================================================
# Author: Israel López
# Date: 2023-06-06
# Version: 1.0
# License: MIT
# ==================================================================================
# Dependencies:
# - pvs-studio
# - qutebrowser
# - valgrind
# - clang
# ==================================================================================
# Este Makefile está diseñado para compilar y enlazar un proyecto de C/C++ con soporte
# para múltiples librerías y un sistema de pruebas. A continuación, se detallan las 
# variables que deben ser definidas para el correcto funcionamiento del Makefile.
#
# Variables Necesarias:
# ----------------------------------------------------------------------------------
# EXECUTABLE:
#   - Descripción: El nombre del archivo ejecutable que se generará.
#   - Ejemplo: EXECUTABLE = my_program
#
# LIB_NAME:
#   - Descripción: El nombre de la librería que se va a crear.
#   - Ejemplo: LIB_NAME = my_library
#
# TEST_EXECUTABLE:
#   - Descripción: El nombre del archivo ejecutable que se generará para los tests.
#   - Ejemplo: TEST_EXECUTABLE = test_runner
#
# DEPENDENCIES:
#   - Descripción: Un array de rutas relativas a las dependencias dentro de este 
#                  mismo proyecto. Cada elemento debe tener el formato:
#                  `ruta>STATIC|SHARED`
#   - Ejemplo: DEPENDENCIES = ../lib1>STATIC ../lib2>SHARED
#
# EXTERNAL_DEPENDENCIES:
#   - Descripción: Un array de nombres de librerías externas que se utilizarán 
#                  en el proyecto.
#   - Ejemplo: EXTERNAL_DEPENDENCIES = pthread m
# ==================================================================================

CC=clang
CXX=clang++
AR=ar

filter ?= *

##################################
# PATHS
##################################
CURRENT_PATH=$(shell pwd)
COMMON_MK_PATH=$(dir $(realpath $(lastword $(MAKEFILE_LIST))))
CONFIG_DIR=$(COMMON_MK_PATH)config
CONFIG_FILE=$(CONFIG_DIR)/config.h
TEST_MALLOC_FILE=$(CONFIG_DIR)/test_malloc.h
INCLUDES_DIR=include
SOURCES_DIR=src
TESTS_DIR=test
BUILD_DIR=build
BIN_DIR=bin
LIB_DIR=lib
TEMP_STATIC=$(foreach dep,$(DEPENDENCIES),$(if $(findstring >STATIC,$(dep)),$(dep)))
TEMP_SHARED=$(foreach dep,$(DEPENDENCIES),$(if $(findstring >SHARED,$(dep)),$(dep)))
DEPENDENCIES_STATIC_LIBS=$(foreach dep,$(TEMP_STATIC),$(subst >STATIC,,$(dep)))
DEPENDENCIES_SHARED_LIBS=$(foreach dep,$(TEMP_SHARED),$(subst >SHARED,,$(dep)))
DEPENDENCIES_LIBS=$(foreach dir,$(DEPENDENCIES_STATIC_LIBS),$(dir)/lib/lib$(notdir $(dir)).a) $(foreach dir,$(DEPENDENCIES_SHARED_LIBS),$(dir)/lib/lib$(notdir $(dir)).so)
GTEST_DIR=$(COMMON_MK_PATH)gtest
GTEST_LIB_DIR=$(GTEST_DIR)/final/usr/local/lib
GTEST_INCLUDE_DIR=$(GTEST_DIR)/final/usr/local/include

##################################
# COMPILATION FLAGS
##################################
INCLUDES_FLAGS=-I$(INCLUDES_DIR) $(patsubst %, -I%/$(INCLUDES_DIR)/,$(DEPENDENCIES_SHARED_LIBS)) $(patsubst %, -I%/$(INCLUDES_DIR)/,$(DEPENDENCIES_STATIC_LIBS)) -I$(COMMON_MK_PATH)config
EXCLUDES=-Wno-macro-redefined \
				 -Wno-c23-extensions
CXXFLAGS=-Wall -Wextra -pedantic -Wpedantic -Werror -pedantic-errors $(EXCLUDES) $(INCLUDES_FLAGS) -O3 -pthread -fPIC
TESTFLAGS=-I$(GTEST_INCLUDE_DIR) -L$(GTEST_LIB_DIR) -lgtest_main -lgtest
LDFLAGS=$(patsubst %, -L%/$(LIB_DIR)/,$(DEPENDENCIES_STATIC_LIBS)) $(patsubst %, -L%/$(LIB_DIR)/,$(DEPENDENCIES_SHARED_LIBS)) $(foreach dir,$(DEPENDENCIES_STATIC_LIBS),-l:lib$(notdir $(dir)).a) -lc $(foreach dir,$(DEPENDENCIES_SHARED_LIBS),-l:lib$(notdir $(dir)).so) $(foreach dir, $(EXTERNAL_DEPENDENCIES),-l$(notdir $(dir)))
COVERAGE_FLAGS=-fprofile-instr-generate -fcoverage-mapping -fprofile-arcs -ftest-coverage
STATIC_LIB_FLAGS=-r
SHARED_LIB_FLAGS=-shared -fPIC

##################################
# DIRECTORIES CREATION
##################################
$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(LIB_DIR):
	@mkdir -p $(LIB_DIR)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(GTEST_DIR):
	@git clone https://github.com/google/googletest.git $(GTEST_DIR)
	@cd $(GTEST_DIR) && \
		mkdir -p build && \
		mkdir -p final && \
		cd build && \
		cmake .. && make && \
		make install DESTDIR=../final && \
		cd .. && rm -rf build

$(CONFIG_DIR):
	mkdir -p $(CONFIG_DIR)

$(CONFIG_FILE): $(CONFIG_DIR)
	@if [ ! -f $(CONFIG_FILE) ]; then \
		echo "#ifndef CONFIG_H" > config/config.h; \
		echo "#define CONFIG_H" >> config/config.h; \
		echo "" >> config/config.h; \
		echo "// Definiciones de configuración aquí" >> config/config.h; \
		echo "" >> config/config.h; \
		echo "#endif // CONFIG_H" >> config/config.h; \
	fi

##################################
# COMPILATION FILE ARRAYS
##################################
INCLUDES=$(wildcard $(INCLUDES_DIR)/*.hpp) $(wildcard $(INCLUDES_DIR)/*.h)
CPP_SOURCES=$(wildcard $(SOURCES_DIR)/*.cpp)
C_SOURCES=$(wildcard $(SOURCES_DIR)/*.c)
C_OBJECTS=$(patsubst $(SOURCES_DIR)/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES))
CPP_OBJECTS=$(patsubst $(SOURCES_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(CPP_SOURCES))
CPP_OBJECTS_COVERAGE=$(patsubst $(SOURCES_DIR)/%.cpp,$(BUILD_DIR)/%_coverage.o,$(CPP_SOURCES))
C_OBJECTS_COVERAGE=$(patsubst $(SOURCES_DIR)/%.c,$(BUILD_DIR)/%_coverage.o,$(C_SOURCES))
TESTS_SOURCES=$(wildcard $(TESTS_DIR)/*.cpp)
TESTS_OBJECTS=$(patsubst $(TESTS_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(TESTS_SOURCES))
TESTS_OBJECTS_COVERAGE=$(patsubst $(TESTS_DIR)/%.cpp,$(BUILD_DIR)/%_coverage.o,$(TESTS_SOURCES))

##################################
# TOOLS
##################################
CHATGPTSEND=folder2chatgpt
CHATGPTLOAD=chatgpt2folder
VALGRIND=valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 -s
LLVM_PROF=llvm-profdata merge -sparse $(BUILD_DIR)/default.profraw -o $(BUILD_DIR)/default.profdata
LLVM_HTML_REPORT=llvm-cov show $(BIN_DIR)/$(TEST_EXECUTABLE)_coverage \
						-instr-profile=$(BUILD_DIR)/default.profdata \
						-ignore-filename-regex='.*gtest.*' \
						-ignore-filename-regex='.*test.*' \
						-format=html \
						-o $(BUILD_DIR)/cov_report
LLVM_JSON_REPORT=llvm-cov export $(BIN_DIR)/$(TEST_EXECUTABLE)_coverage \
						-instr-profile=$(BUILD_DIR)/default.profdata \
						-ignore-filename-regex='.*gtest.*' \
						-ignore-filename-regex='.*test.*' \
						-format=text > $(BUILD_DIR)/cov_report.json

##################################
# RULES
##################################
.PHONY: all clean chatgpt_send chatgpt_load valgrind show run test format list help docs

help:
	@echo -e "make <options>\n Options:"
	@echo -e "bin:->:Creates bin folder\n \
    build:->:Creates build folder\n \
    lib:->:Creates lib folder\n \
    $(EXECUTABLE):->:Creates main executable\n \
    $(TEST_EXECUTABLE):->:Creates test executable\n \
    libs:->:Create shared and static libraries\n \
    clean:->:Clean all produced files\n \
    compile_commands:->:Generates file for tree-sitter parser\n \
    run:->:runs $(EXECUTABLE) file\n \
    test:->:runs tests in $(TEST_EXECUTABLE)\n \
    coverage:->:runs the covarage over the tests\n \
    format:->:applies format to the sources\n \
    help:->:prints this" | sed 's&^[ \t]*&    &' | column -s: -t
	@echo -e "Objects: specify one or more objects:"
	@echo "    $(CPP_OBJECTS)"

all: $(EXECUTABLE) $(TEST_EXECUTABLE) libs $(TEST_EXECUTABLE)_coverage compile_commands docs

clean:
	@rm -rf $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR) compile_commands.json default.profraw docs

list:
	@LC_ALL=C $(MAKE) -pRrq -f $(firstword $(MAKEFILE_LIST)) : 2>/dev/null | awk -v RS= -F: '/(^|\n)# Files(\n|$$)/,/(^|\n)# Finished Make data base/ {if ($$1 !~ "^[#.]") {print $$1}}' | sort | grep -E -v -e '^[^[:alnum:]]' -e '^$@$$'

$(DEPENDENCIES_LIBS) : % :
	@echo "Compiling $@"
	@cd $(shell echo $@ | sed 's/\/lib\/.*$$//') && make libs > /dev/null 2>&1

#################
# NORMAL
#################
main.cpp:
	@echo -e "Ly8gVGhpcyBpcyBhIHBlcnNvbmFsIGFjYWRlbWljIHByb2plY3QuIERlYXIgUFZTLVN0dWRpbywg\ncGxlYXNlIGNoZWNrIGl0LgovLyBQVlMtU3R1ZGlvIFN0YXRpYyBDb2RlIEFuYWx5emVyIGZvciBD\nLCBDKyssIEMjLCBhbmQgSmF2YTogaHR0cHM6Ly9wdnMtc3R1ZGlvLmNvbQojaW5jbHVkZSA8Y3N0\nZGlvPgoKaW50IG1haW4oKSB7CiAgcHJpbnRmKCJIZWxsbywgV29ybGQhXG4iKTsKICByZXR1cm4g\nMDsKfQo=" | base64 -d > $@

$(EXECUTABLE): $(BIN_DIR)/$(EXECUTABLE)

$(BIN_DIR)/$(EXECUTABLE): $(CPP_OBJECTS) $(C_OBJECTS) $(BIN_DIR) main.cpp $(DEPENDENCIES_LIBS)
	@$(CXX) $(CXXFLAGS) $(CPP_OBJECTS) $(C_OBJECTS) main.cpp $(LDFLAGS) -o $@

$(CPP_OBJECTS): $(BUILD_DIR)/%.o : $(SOURCES_DIR)/%.cpp $(BUILD_DIR)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(C_OBJECTS): $(BUILD_DIR)/%.o : $(SOURCES_DIR)/%.c $(BUILD_DIR)
	@$(CC) $(CXXFLAGS) -c $< -o $@

run: $(EXECUTABLE)
	@echo ""
	@echo "Running..."
	@echo "=========="
	@./$(BIN_DIR)/$(EXECUTABLE)

#################
# TEST
#################
$(TEST_EXECUTABLE): $(BIN_DIR)/$(TEST_EXECUTABLE)

$(BIN_DIR)/$(TEST_EXECUTABLE): $(TEST_MALLOC_FILE) $(GTEST_DIR) $(TESTS_OBJECTS) $(CPP_OBJECTS) $(C_OBJECTS) $(BIN_DIR) $(DEPENDENCIES_LIBS)
	@$(CXX) $(CXXFLAGS) $(TESTFLAGS) $(TESTS_OBJECTS) $(CPP_OBJECTS) $(C_OBJECTS) $(LDFLAGS) -o $@; \
		sed -i '/#include "test_malloc.h"/d' $(CONFIG_FILE) && rm $(TEST_MALLOC_FILE) > /dev/null 2>&1 || true

$(TESTS_OBJECTS): $(BUILD_DIR)/%.o : $(TESTS_DIR)/%.cpp $(BUILD_DIR)
	@$(CXX) $(CXXFLAGS) -I$(GTEST_INCLUDE_DIR) -c $< -o $@

tests: $(TEST_EXECUTABLE)
	@echo ""
	@echo "Running tests..."
	@echo "================"
	@./$(BIN_DIR)/$(TEST_EXECUTABLE) --gtest_filter=$(filter)

$(TEST_MALLOC_FILE):
	@printf " \n\
	#ifndef TEST_MALLOC_H \n\
	#define TEST_MALLOC_H \n\
	\n\
	#include <stddef.h> \n\
	#include <stdlib.h> \n\
	#include <stdatomic.h> \n\
	\n\
	// Definición de las variables globales \n\
	static atomic_int test_mode; \n\
	static atomic_int num_mallocs; \n\
	static atomic_int target_mallocs; \n\
	int i = 0;
	atomic_store(&test_mode, i); \n\
	atomic_store(&num_mallocs, i); \n\
	atomic_store(&target_mallocs, i); \n\
	\n\
	void *fake_malloc(unsigned long size) { \n\
		void *ptr = NULL; \n\
		printf(\"fake_malloc atomic_load(&test_mode)=%%d, atomic_load(&num_mallocs)=%%d, atomic_load(&target_mallocs)=%%d\", atomic_load(&test_mode), atomic_load(&num_mallocs), atomic_load(&target_mallocs)); \n\
		puts(\"\");\n\
		if (atomic_load(&test_mode) == 0) { \n\
			ptr = malloc(size); \n\
		} else if (atomic_load(&test_mode) == 1 && atomic_load(&num_mallocs) == atomic_load(&target_mallocs)) { \n\
		  puts(\"test mode 1\"); \n\
			ptr = NULL; \n\
		} else if (atomic_load(&test_mode) == 2 && atomic_load(&num_mallocs) < atomic_load(&target_mallocs)) { \n\
		  puts(\"test mode 2\"); \n\
			ptr = NULL; \n\
		} else if (atomic_load(&test_mode) == 3 && atomic_load(&num_mallocs) > atomic_load(&target_mallocs)) { \n\
		  puts(\"test mode 3\"); \n\
			ptr = NULL; \n\
		} else { \n\
			puts(\"test mode else\"); \n\
			ptr = malloc(size); \n\
		} \n\
		if (ptr != NULL || atomic_load(&test_mode) != 0) { \n\
			atomic_load(&num_mallocs)++; \n\
		} \n\
		return ptr; \n\
	} \n\
	\n\
	// Macro para manejar las diferentes condiciones de test_mode \n\
	#define malloc(size) fake_malloc(size) \n\
	\n\
	#endif  // !TEST_MALLOC_H \n" > $(TEST_MALLOC_FILE)
	@if ! grep -q '#include "test_malloc.h"' $(CONFIG_FILE); then \
		sed -i '/#define CONFIG_H/a #include "test_malloc.h"' $(CONFIG_FILE); \
	fi

#################
# LIBS
#################
libs: $(LIB_DIR)/lib$(LIB_NAME).a $(LIB_DIR)/lib$(LIB_NAME).so

$(LIB_DIR)/lib$(LIB_NAME).a: $(CPP_OBJECTS) $(C_OBJECTS) $(LIB_DIR)
$(LIB_DIR)/lib$(LIB_NAME).so: $(CPP_OBJECTS) $(C_OBJECTS) $(LIB_DIR)
	@$(AR) $(STATIC_LIB_FLAGS) $(LIB_DIR)/lib$(LIB_NAME).a $(CPP_OBJECTS) $(C_OBJECTS) > /dev/null 2>&1
	@$(CXX) $(CXXFLAGS) $(SHARED_LIB_FLAGS) $(CPP_SOURCES) $(C_OBJECTS) -o $(LIB_DIR)/lib$(LIB_NAME).so

#################
# COVERAGE
#################
$(EXECUTABLE)_coverage: $(BIN_DIR)/$(EXECUTABLE)_coverage

$(BIN_DIR)/$(EXECUTABLE)_coverage: $(CPP_OBJECTS_COVERAGE) $(C_OBJECTS_COVERAGE) $(BIN_DIR) main.cpp $(DEPENDENCIES_LIBS)
	@$(CXX) $(CXXFLAGS) $(COVERAGE_FLAGS) --coverage $(CPP_OBJECTS_COVERAGE) $(C_OBJECTS_COVERAGE) main.cpp $(LDFLAGS) -o $@

$(CPP_OBJECTS_COVERAGE): $(BUILD_DIR)/%_coverage.o : $(SOURCES_DIR)/%.cpp $(BUILD_DIR)
	@$(CXX) $(CXXFLAGS) $(COVERAGE_FLAGS) -c $< -o $@

$(C_OBJECTS_COVERAGE): $(BUILD_DIR)/%_coverage.o : $(SOURCES_DIR)/%.c $(BUILD_DIR)
	@$(CC) $(CXXFLAGS) $(COVERAGE_FLAGS) -c $< -o $@

$(TEST_EXECUTABLE)_coverage: $(BIN_DIR)/$(TEST_EXECUTABLE)_coverage
	@./$(BIN_DIR)/$(TEST_EXECUTABLE)_coverage
	@mv default.profraw $(BUILD_DIR)
	@$(LLVM_PROF) > /dev/null 2>&1
	@$(LLVM_HTML_REPORT)
	@$(LLVM_JSON_REPORT) && \
  	jq '.data[0].totals' $(BUILD_DIR)/cov_report.json > $(BUILD_DIR)/coverage_report.json
	@rm -rf $(BUILD_DIR)/cov_report.json $(BUILD_DIR)/default.profdata default.profraw

$(BIN_DIR)/$(TEST_EXECUTABLE)_coverage: $(TEST_MALLOC_FILE) $(TESTS_OBJECTS_COVERAGE) $(CPP_OBJECTS_COVERAGE) $(C_OBJECTS_COVERAGE) $(BIN_DIR) $(DEPENDENCIES_LIBS)
	@$(CXX) $(CXXFLAGS) $(TESTFLAGS) $(COVERAGE_FLAGS) $(TESTS_OBJECTS_COVERAGE) $(CPP_OBJECTS_COVERAGE) $(C_OBJECTS_COVERAGE) $(LDFLAGS) -o $@; \
		sed -i '/#include "test_malloc.h"/d' $(CONFIG_FILE) && rm $(TEST_MALLOC_FILE) > /dev/null 2>&1 || true

$(TESTS_OBJECTS_COVERAGE): $(BUILD_DIR)/%_coverage.o : $(TESTS_DIR)/%.cpp $(BUILD_DIR)
	@$(CXX) $(CXXFLAGS) -I$(GTEST_INCLUDE_DIR) $(COVERAGE_FLAGS) -c $< -o $@

coverage: $(TEST_EXECUTABLE)_coverage
	@echo ""
	@echo "Running coverage..."
	@echo "==================="
	@cat $(BUILD_DIR)/coverage_report.json | jq
	@qutebrowser $(BUILD_DIR)/cov_report/index.html > /dev/null 2>&1 &

#################
# TOOLS
#################
format: main.cpp
	@clang-format -i $(CPP_SOURCES) $(INCLUDES) $(TESTS_SOURCES)

valgrind: CXXFLAGS+=-O0 -g
valgrind: $(BIN_DIR)/$(TEST_EXECUTABLE)
	@echo ""
	@echo "Running with valgrind..."
	@echo "========================"
	@$(VALGRIND) --log-file=$(BUILD_DIR)/valgrind_output.txt ./$(BIN_DIR)/$(TEST_EXECUTABLE) --gtest_filter=$(filter)

compile_commands:
	@compiledb -n make all

docs: $(BUILD_DIR)
	@doxygen Doxyfile > /dev/null 2>&1
	@cd build/docs/latex && make > /dev/null 2>&1
