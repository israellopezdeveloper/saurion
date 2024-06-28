# master.mk

MAKEFILE_PATH := $(dir $(realpath $(firstword $(MAKEFILE_LIST))))
SUBPROJECTS := $(shell find . -type f -name 'Makefile' -exec dirname {} \;)
SUBPROJECTS := $(filter-out ., $(SUBPROJECTS))

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
	echo -e "#include \"gtest/gtest.h\"\nclass $${module_name}Test : public ::testing::Test {\n protected:\n  void SetUp() override {\n  }\n  void TearDown() override {\n  }\n};\nTEST_F($${module_name}Test, Test) { EXPECT_TRUE(true); }" > $(MAKEFILE_PATH)$$module_name/test/$${module_name}_test.cpp

.PHONY: all clean valgrind run tests format libs image image-push

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

image:
	@docker build $(patsubst %, -t % ,$(DOCKERTAGS)) .

image-push:
	$(foreach tag, $(DOCKERTAGS), docker push $(tag); )

gtest:
	@git clone https://github.com/google/googletest.git gtest
	@cd gtest && \
		mkdir -p build && \
		mkdir -p final && \
		cd build && \
		cmake .. && make && \
		make install DESTDIR=../final && \
		cd .. && rm -rf build

