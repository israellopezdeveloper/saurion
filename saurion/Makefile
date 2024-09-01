EXECUTABLE=saurion_main
LIB_NAME=saurion
TEST_EXECUTABLE=saurion_test
DEPENDENCIES=../threadpool>STATIC
EXTERNAL_DEPENDENCIES=uring

generate_pkg_scripts: scripts/client scripts/server
	@rm scripts/client scripts/server > /dev/null 2>&1 || true
	@pkg -t node18-linux-x64 scripts/client.sh --output scripts/client > /dev/null 2>&1
	@pkg -t node18-linux-x64 scripts/server.sh --output scripts/server > /dev/null 2>&1

PRE_TEST_TASKS += generate_pkg_scripts

include ../.scaffold/module.mk
