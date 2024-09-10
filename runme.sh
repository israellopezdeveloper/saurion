#/bin/sh

set -eu

SCRIPT_PATH=$(realpath "$(dirname "$0")")

BLUE="\e[1;94m"
GREEN="\e[1;92m"
RESET="\e[0m"

STOPPABLE=false

banner() {
	local title="$1"
	echo ""
	local length=${#title}
	local decoration="==$(printf "%-${length}s" | tr ' ' '=')=="
	echo -e "${BLUE}${decoration}"
	echo "= ${title^^} ="
	echo -e "${decoration}${RESET}"
}

wait_key() {
	if STOPPABLE; then
		echo -e -n "${GREEN}Press any key to continue${RESET}"
		read -s -n 1 key
		echo ""
	fi
}

configure() {
	local params="$@"
	banner "Configuring ... [${params}]"

	rm -rf "${SCRIPT_PATH}/build"
	autoreconf --install

	mkdir "${SCRIPT_PATH}/build"
	cd $_
	${SCRIPT_PATH}/configure ${params} &&
		wait_key
}

make_exec() {
	local params="$@"
	banner "MAKE ${params}"

	make ${params} &&
		wait_key
}

clear

configure &&
	make_exec &&
	make_exec check &&
	make_exec maintainer-clean
