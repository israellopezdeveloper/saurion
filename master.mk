# master.mk

MAKEFILE_PATH := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
SUBPROJECTS := $(shell find . -type f -name 'Makefile' -exec dirname {} \;)
SUBPROJECTS := $(filter-out ., $(SUBPROJECTS))
COVERAGE_BRANCH_VALUES :=
COVERAGE_FUNCTIONS_VALUES :=
COVERAGE_LINES_VALUES :=
COVERAGE_REGION_VALUES :=

create_module:
	@echo "Ingrese el nombre del módulo:"
	@read module_name; \
	if [ -z "$$module_name" ]; then \
	  echo "Error: El nombre del módulo no puede estar vacío"; \
	  exit 1; \
	fi; \
	mkdir -p $(MAKEFILE_PATH)$$module_name/include && \
	touch $(MAKEFILE_PATH)$$module_name/include/$$module_name.hpp && \
	mkdir -p $(MAKEFILE_PATH)$$module_name/src && \
	touch $(MAKEFILE_PATH)$$module_name/src/$$module_name.cpp && \
	mkdir -p $(MAKEFILE_PATH)$$module_name/test && \
	touch $(MAKEFILE_PATH)$$module_name/test/$${module_name}_test.cpp && \
	echo -e "EXECUTABLE=$${module_name}_main\nLIB_NAME=$${module_name}\nTEST_EXECUTABLE=$${module_name}_test\nDEPENDENCIES=\nEXTERNAL_DEPENDENDIES=\ninclude ../module.mk" > $(MAKEFILE_PATH)$$module_name/Makefile && \
	echo -e "#include \"gtest/gtest.h\"\nclass $${module_name}Test : public ::testing::Test {\n protected:\n  void SetUp() override {\n  }\n  void TearDown() override {\n  }\n};\nTEST_F($${module_name}Test, Test) { EXPECT_TRUE(true); }" > $(MAKEFILE_PATH)$$module_name/test/$${module_name}_test.cpp && \
	cd "$$module_name" && doxygen -g && sed -i 's/PROJECT_NAME           = "My Project"/PROJECT_NAME           = "$$module_name"/' Doxyfile && \
	sed -i 's/OUTPUT_DIRECTORY       =/OUTPUT_DIRECTORY       = build\/docs/' Doxyfile && \
	sed -i 's/INPUT                  =/INPUT                  = .\/src .\/include/' Doxyfile && \
	sed -i 's/FILE_PATTERNS          =/FILE_PATTERNS          = *.c *.h *.cpp *.hpp/' Doxyfile && \
	sed -i 's/RECURSIVE              = NO/RECURSIVE              = YES/' Doxyfile && \
	sed -i 's/EXTRACT_ALL            = NO/EXTRACT_ALL            = YES/' Doxyfile

.PHONY: all clean valgrind run tests format libs image image-push coverage docs

define foreach_subdir
  @echo "Entering directory '$(1)'"
  @echo "$(MAKE) -C $(1) $(2)"
endef

all:
	@$(foreach ndir, $(SUBPROJECTS), $(MAKE) -C $(ndir) all;)

clean:
	@$(foreach ndir, $(SUBPROJECTS), $(MAKE) -C $(ndir) clean;)

valgrind:
	@$(foreach ndir, $(SUBPROJECTS), $(MAKE) -C $(ndir) valgrind;)

tests:
	@$(foreach ndir, $(SUBPROJECTS), $(MAKE) -C $(ndir) tests;)

run:
	@$(foreach ndir, $(SUBPROJECTS), $(MAKE) -C $(ndir) run;)

format:
	@$(foreach ndir, $(SUBPROJECTS), $(MAKE) -C $(ndir) format;)

libs:
	@$(foreach ndir, $(SUBPROJECTS), $(MAKE) -C $(ndir) libs;)

docs:
	@$(foreach ndir, $(SUBPROJECTS), $(MAKE) -C $(ndir) docs;)

define get_coverage_values
	$(foreach ndir, $(SUBPROJECTS), $(eval COVERAGE_BRANCH_VALUES += $(shell jq '.branches.percent' "$(ndir)/build/coverage_report.json")))
	$(foreach ndir, $(SUBPROJECTS), $(eval COVERAGE_FUNCTIONS_VALUES += $(shell jq '.functions.percent' "$(ndir)/build/coverage_report.json")))
	$(foreach ndir, $(SUBPROJECTS), $(eval COVERAGE_LINES_VALUES += $(shell jq '.lines.percent' "$(ndir)/build/coverage_report.json")))
	$(foreach ndir, $(SUBPROJECTS), $(eval COVERAGE_REGION_VALUES += $(shell jq '.regions.percent' "$(ndir)/build/coverage_report.json")))
endef

coverage:
	@$(foreach ndir, $(SUBPROJECTS), $(MAKE) -C $(ndir) coverage;)

check_coverage: coverage
	@$(call get_coverage_values) \
	coverage_sum=0; \
	count=0; \
	for value in $(COVERAGE_BRANCH_VALUES); do \
		coverage_sum=$$(echo $$coverage_sum + $$value | bc); \
		count=$$((count + 1)); \
	done; \
	if [ $$count -eq 0 ]; then \
		echo "No coverage values found."; \
		exit 1; \
	fi; \
	average=$$(echo "scale=2; $$coverage_sum / $$count" | bc); \
	minimum=80.00; \
	if [ $$(echo "$$average < $$minimum" | bc -l) -eq 1 ]; then \
		echo "Branch coverage is below 80%!"; \
		exit 1; \
	else \
		echo "Branch coverage   => $$average / $$minimum"; \
	fi; \
	coverage_sum=0; \
	count=0; \
	for value in $(COVERAGE_FUNCTIONS_VALUES); do \
		coverage_sum=$$(echo $$coverage_sum + $$value | bc); \
		count=$$((count + 1)); \
	done; \
	if [ $$count -eq 0 ]; then \
		echo "No coverage values found."; \
		exit 1; \
	fi; \
	average=$$(echo "scale=2; $$coverage_sum / $$count" | bc); \
	minimum=80.00; \
	if [ $$(echo "$$average < $$minimum" | bc -l) -eq 1 ]; then \
		echo "Function coverage is below 80%!"; \
		exit 1; \
	else \
		echo "Function coverage => $$average / $$minimum"; \
	fi; \
	coverage_sum=0; \
	count=0; \
	for value in $(COVERAGE_LINES_VALUES); do \
		coverage_sum=$$(echo $$coverage_sum + $$value | bc); \
		count=$$((count + 1)); \
	done; \
	if [ $$count -eq 0 ]; then \
		echo "No coverage values found."; \
		exit 1; \
	fi; \
	average=$$(echo "scale=2; $$coverage_sum / $$count" | bc); \
	minimum=80.00; \
	if [ $$(echo "$$average < $$minimum" | bc -l) -eq 1 ]; then \
		echo "Line coverage is below 80%!"; \
		exit 1; \
	else \
		echo "Line coverage     => $$average / $$minimum"; \
	fi; \
	coverage_sum=0; \
	count=0; \
	for value in $(COVERAGE_REGION_VALUES); do \
		coverage_sum=$$(echo $$coverage_sum + $$value | bc); \
		count=$$((count + 1)); \
	done; \
	if [ $$count -eq 0 ]; then \
		echo "No coverage values found."; \
		exit 1; \
	fi; \
	average=$$(echo "scale=2; $$coverage_sum / $$count" | bc); \
	minimum=80.00; \
	if [ $$(echo "$$average < $$minimum" | bc -l) -eq 1 ]; then \
		echo "Region coverage is below 80%!"; \
		exit 1; \
	else \
		echo "Region coverage   => $$average / $$minimum"; \
	fi;

gtest:
	@git clone https://github.com/google/googletest.git gtest
	@cd gtest && \
		mkdir -p build && \
		mkdir -p final && \
		cd build && \
		cmake .. && make && \
		make install DESTDIR=../final && \
		cd .. && rm -rf build

