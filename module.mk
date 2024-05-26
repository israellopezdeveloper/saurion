########################################################################################################
# Author: Israel López
# Date: 2023-06-06
# Version: 1.0
#
# Description: Module for C++ projects
# Creates a Makefile scaffold for C++ projects, which:
# - creates directories and compiles the project
# - generates a coverage report
# - runs tests
# - generates a compile_commands.json file
# - generates a PVS-Studio report
# - runs valgrind memory leak detection
#
# License: MIT
#
# Dependencies:
# - pvs-studio
# - qutebrowser
# - valgrind
# - clang
########################################################################################################

CXX=clang++
AR=ar

##################################
# PATHS
##################################
CURRENT_PATH=$(shell pwd)
COMMON_MK_PATH=$(dir $(realpath $(lastword $(MAKEFILE_LIST))))
INCLUDES_DIR=include
SOURCES_DIR=src
TESTS_DIR=test
BUILD_DIR=build
BIN_DIR=bin
LIB_DIR=lib
GTEST_DIR=$(COMMON_MK_PATH)gtest
GTEST_LIB_DIR=$(GTEST_DIR)/final/usr/local/lib
GTEST_INCLUDE_DIR=$(GTEST_DIR)/final/usr/local/include

##################################
# COMPILATION FLAGS
##################################
CXXFLAGS=-Wall -Wextra -pedantic -Wpedantic -Werror -pedantic-errors -I$(INCLUDES_DIR)/ -O3 -pthread
TESTFLAGS=-I$(GTEST_INCLUDE_DIR) -L$(GTEST_LIB_DIR) -lgtest_main -lgtest
COVERAGE_FLAGS=-fprofile-instr-generate -fcoverage-mapping
STATIC_LIB_FLAGS=-r
SHARED_LIB_FLAGS=-shared -fpic

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


##################################
# COMPILATION FILE ARRAYS
##################################
INCLUDES=$(wildcard $(INCLUDES_DIR)/*.hpp)
SOURCES=$(wildcard $(SOURCES_DIR)/*.cpp)
OBJECTS=$(patsubst $(SOURCES_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))
OBJECTS_COVERAGE=$(patsubst $(SOURCES_DIR)/%.cpp,$(BUILD_DIR)/%_coverage.o,$(SOURCES))
TESTS_SOURCES=$(wildcard $(TESTS_DIR)/*.cpp)
TESTS_OBJECTS=$(patsubst $(TESTS_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(TESTS_SOURCES))
TESTS_OBJECTS_COVERAGE=$(patsubst $(TESTS_DIR)/%.cpp,$(BUILD_DIR)/%_coverage.o,$(TESTS_SOURCES))
IOBJECTS=$(patsubst $(SOURCES_DIR)/%.cpp,$(BUILD_DIR)/%.o.PVS-Studio.i,$(SOURCES))
POBJECTS=$(patsubst $(SOURCES_DIR)/%.cpp,$(BUILD_DIR)/%.o.PVS-Studio.log,$(SOURCES))

##################################
# LOG CONFIGURATIONS
##################################
PVS_CFG=$(COMMON_MK_PATH)/PVS-Studio.cfg
LOG_FORMAT=tasklist # csv, errorfile, fullhtml, html, tasklist, xml
LOG_FORMAT2=html
PVS_LOG=$(COMMON_MK_PATH)/project.tasks
PVS_LICENSE=$(HOME)/.config/PVS-Studio/PVS-Studio.lic
PVS_HTML=$(COMMON_MK_PATH)/report.html
PVS_PREPARE=$(COMMON_MK_PATH)/pvs-studio-prepare
PVS_STUDIO=pvs-studio

##################################
# TOOLS
##################################
CHATGPTSEND=folder2chatgpt
CHATGPTLOAD=chatgpt2folder
VALGRIND=valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --error-exitcode=1 -s
LLVM_PROF=llvm-profdata merge -sparse default.profraw -o $(BUILD_DIR)/default.profdata
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
.PHONY: all clean prepare chatgpt_send chatgpt_load valgrind show run test compile_commands

all: $(EXECUTABLE) $(TEST_EXECUTABLE) libs compile_commands $(TEST_EXECUTABLE)_coverage

run_all: test coverage

prepare: $(SOURCES)
	@$(PVS_PREPARE) -c 1 . > /dev/null 2>&1

clean:
	@rm -rf $(BUILD_DIR) $(BIN_DIR) $(LIB_DIR) compile_commands.json

#################
# NORMAL
#################
$(EXECUTABLE): $(BIN_DIR)/$(EXECUTABLE)

$(BIN_DIR)/$(EXECUTABLE): $(OBJECTS) $(BIN_DIR)
	@$(CXX) $(CXXFLAGS) $(OBJECTS) main.cpp -o $@
	@plog-converter -a 'GA:1,2' -t $(LOG_FORMAT) $(POBJECTS) -o $(PVS_LOG) > /dev/null 2>&1
	@plog-converter -a 'GA:1,2' -t $(LOG_FORMAT2) $(POBJECTS) -o $(PVS_HTML) > /dev/null 2>&1

$(OBJECTS): $(BUILD_DIR)/%.o : $(SOURCES_DIR)/%.cpp $(BUILD_DIR) prepare
	@$(CXX) $(CXXFLAGS) -c $< -o $@
	@$(CXX) $(CXXFLAGS) $< -E -o $@.PVS-Studio.i > /dev/null 2>&1
	@$(PVS_STUDIO) --lic-file=$(PVS_LICENSE) --cfg $(PVS_CFG) --source-file $< --i-file $@.PVS-Studio.i --output-file $@.PVS-Studio.log > /dev/null 2>&1

run: $(EXECUTABLE)
	@echo ""
	@echo "Running..."
	@echo "=========="
	@./$(EXECUTABLE)

#################
# TEST
#################
$(TEST_EXECUTABLE): $(BIN_DIR)/$(TEST_EXECUTABLE)

$(BIN_DIR)/$(TEST_EXECUTABLE): $(TESTS_OBJECTS) $(OBJECTS) $(BIN_DIR)
	@$(CXX) $(CXXFLAGS) $(TESTFLAGS) $(TESTS_OBJECTS) $(OBJECTS) -o $@

$(TESTS_OBJECTS): $(BUILD_DIR)/%.o : $(TESTS_DIR)/%.cpp $(BUILD_DIR) prepare
	@$(CXX) $(CXXFLAGS) -I$(GTEST_INCLUDE_DIR) -c $< -o $@

test: $(TEST_EXECUTABLE)
	@echo ""
	@echo "Running tests..."
	@echo "================"
	@./$(BIN_DIR)/$(TEST_EXECUTABLE)

#################
# LIBS
#################
libs: $(LIB_DIR)/lib$(LIB_NAME).a $(LIB_DIR)/lib$(LIB_NAME).so

$(LIB_DIR)/lib$(LIB_NAME).a: $(OBJECTS) $(LIB_DIR)
$(LIB_DIR)/lib$(LIB_NAME).so: $(OBJECTS) $(LIB_DIR)
	@$(AR) $(STATIC_LIB_FLAGS) $(LIB_DIR)/lib$(LIB_NAME).a $(OBJECTS) > /dev/null 2>&1
	@$(CXX) $(CXXFLAGS) $(SHARED_LIB_FLAGS) $(SOURCES) -o $(LIB_DIR)/lib$(LIB_NAME).so

#################
# COVERAGE
#################
$(EXECUTABLE)_coverage: $(BIN_DIR)/$(EXECUTABLE)_coverage

$(BIN_DIR)/$(EXECUTABLE)_coverage: $(OBJECTS_COVERAGE) $(BIN_DIR)
	@$(CXX) $(CXXFLAGS) $(COVERAGE_FLAGS) --coverage $(OBJECTS_COVERAGE) main.cpp -o $@

$(OBJECTS_COVERAGE): $(BUILD_DIR)/%_coverage.o : $(SOURCES_DIR)/%.cpp $(BUILD_DIR) prepare
	@$(CXX) $(CXXFLAGS) $(COVERAGE_FLAGS) -c $< -o $@

$(TEST_EXECUTABLE)_coverage: $(BIN_DIR)/$(TEST_EXECUTABLE)_coverage
	@./$(BIN_DIR)/$(TEST_EXECUTABLE)_coverage > /dev/null 2>&1
	@$(LLVM_PROF) > /dev/null 2>&1
	@$(LLVM_HTML_REPORT)
	@$(LLVM_JSON_REPORT) && \
		jq '.data[0].totals' $(BUILD_DIR)/cov_report.json > $(BUILD_DIR)/coverage_report.json
	@rm -rf $(BUILD_DIR)/cov_report.json $(BUILD_DIR)/default.profdata default.profraw

$(BIN_DIR)/$(TEST_EXECUTABLE)_coverage: $(TESTS_OBJECTS_COVERAGE) $(OBJECTS_COVERAGE) $(BIN_DIR)
	@$(CXX) $(CXXFLAGS) $(TESTFLAGS) $(COVERAGE_FLAGS) $(TESTS_OBJECTS_COVERAGE) $(OBJECTS_COVERAGE) -o $@

$(TESTS_OBJECTS_COVERAGE): $(BUILD_DIR)/%_coverage.o : $(TESTS_DIR)/%.cpp $(BUILD_DIR) prepare
	@$(CXX) $(CXXFLAGS) -I$(GTEST_INCLUDE_DIR) $(COVERAGE_FLAGS) -c $< -o $@

coverage: $(TEST_EXECUTABLE)_coverage
	@echo ""
	@echo "Running coverage..."
	@echo "==================="
	@cat $(BUILD_DIR)/coverage_report.json | jq
	@qutebrowser $(BUILD_DIR)/cov_report/index.html > /dev/null 2>&1

#################
# TOOLS
#################
show: $(EXECUTABLE)
	@qutebrowser $(PVS_HTML)

chatgpt_send:
	@echo ""
	@echo "Enter a message to send to ChatGPT:"
	@echo "==================================="
	@$(CHATGPTSEND) | xclip

chatgpt_load:
	@echo ""
	@echo "Loading code from ChatGPT..."
	@echo "============================"
	@$(CHATGPTLOAD)

valgrind: CXXFLAGS+=-O0 -g
valgrind: $(BIN_DIR)/$(TEST_EXECUTABLE)
	@echo ""
	@echo "Running with valgrind..."
	@echo "========================"
	@$(VALGRIND) --log-file=$(BUILD_DIR)/valgrind_output.txt ./$(BIN_DIR)/$(TEST_EXECUTABLE)

compile_commands:
	@compiledb -n make all
