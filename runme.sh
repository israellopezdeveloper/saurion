#!/bin/bash

set -eu

SCRIPT_PATH=$(realpath "$(dirname "$0")")

BLUE="\e[1;94m"
GREEN="\e[1;92m"
RESET="\e[0m"

declare -A OPTION_FLAGS
declare -A MAKE_TARGETS

OPTION_FLAGS=(
	[docs]=false
	[loop]=false
	# Modos mutuamente excluyentes
	[leak]=false
	[coverage]=false
	[debug]=false
)

MAKE_TARGETS=(
	[docs]="doxygen-doc"
	[leak]="leak-check"
	[coverage]="coverage"
	[debug]="valgrind"
)

banner() {
	local title="$1"
	echo ""
	local length=${#title}
	local decoration="==$(printf "%-${length}s" | tr ' ' '=')=="
	echo -e "${BLUE}${decoration}"
	echo "= ${title} ="
	echo -e "${decoration}${RESET}"
}

wait_key() {
	if ${STOPPABLE:-false}; then
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
	cd "${SCRIPT_PATH}/build"
	${SCRIPT_PATH}/configure ${params} &&
		wait_key
}

make_exec() {
	local params="$@"
	banner "MAKE ${params}"

	make ${params} &&
		wait_key
}

run_make_target() {
	local target="$1"
	banner "Running ${target}"
	make "${target}" &&
		wait_key
}

show_help() {
	echo "Usage: $0 [options]"
	echo "Options:"
	echo "  -d, --docs       Enable document generation"
	echo "  -l, --leak       Enable memory leak check (mutually exclusive with --coverage and --debug)"
	echo "  -c, --coverage   Enable coverage tests (mutually exclusive with --leak and --debug)"
	echo "  -g, --debug      Enable debug mode and run valgrind (mutually exclusive with --leak and --coverage)"
	echo "  -i, --loop       Enable loop mode"
	echo "  -h, --help       Show this help message"
}

loop_execution() {
	make_exec compile_commands || true
	local executed=0
	while true; do
		if [ ${executed} -eq 1 ]; then
			echo -e "\n"
			echo -en "${GREEN}Execute again [y/n]${RESET}"
			read -s -n 1 REPLAY
			if [ "${REPLAY}" = "n" ]; then
				echo ""
				break
			fi
			echo -en "\ec\e[3J"
		fi
		executed=1
		make_exec || continue
		make_exec check || {
			find "${SCRIPT_PATH}/build/tests" -executable -name "*_test" -exec {} \; || true
		}

		for option in "${!MAKE_TARGETS[@]}"; do
			if ${OPTION_FLAGS[$option]}; then
				run_make_target "${MAKE_TARGETS[$option]}" || true
			fi
		done

	done
}

# Variable para rastrear la opción seleccionada
selected_mode=""

# Procesar las opciones de línea de comandos
while [ $# -gt 0 ]; do
	case "$1" in
	-d | --docs)
		OPTION_FLAGS[docs]=true
		shift
		;;
	-l | --leak)
		if [ -n "$selected_mode" ]; then
			echo "Error: Options --leak, --coverage, and --debug are mutually exclusive."
			exit 1
		fi
		OPTION_FLAGS[leak]=true
		selected_mode="leak"
		shift
		;;
	-c | --coverage)
		if [ -n "$selected_mode" ]; then
			echo "Error: Options --leak, --coverage, and --debug are mutually exclusive."
			exit 1
		fi
		OPTION_FLAGS[coverage]=true
		selected_mode="coverage"
		shift
		;;
	-g | --debug)
		if [ -n "$selected_mode" ]; then
			echo "Error: Options --leak, --coverage, and --debug are mutually exclusive."
			exit 1
		fi
		OPTION_FLAGS[debug]=true
		selected_mode="debug"
		shift
		;;
	-i | --loop)
		OPTION_FLAGS[loop]=true
		shift
		;;
	-h | --help)
		show_help
		exit 0
		;;
	*)
		echo "Invalid option: $1"
		show_help
		exit 1
		;;
	esac
done

clear

# Construcción de las opciones de configuración
CONFIGURE_OPTIONS=""

BUILD_MODES=()

if ${OPTION_FLAGS[leak]}; then
	BUILD_MODES+=("memleak")
fi

if ${OPTION_FLAGS[coverage]}; then
	BUILD_MODES+=("coverage")
fi

if ${OPTION_FLAGS[debug]}; then
	BUILD_MODES+=("debug")
fi

if [ ${#BUILD_MODES[@]} -gt 0 ]; then
	CONFIGURE_OPTIONS+=" --enable-build-mode=$(
		IFS=','
		echo "${BUILD_MODES[*]}"
	)"
fi

if ${OPTION_FLAGS[docs]}; then
	CONFIGURE_OPTIONS+=" --enable-doxygen-doc"
fi

# Ejecutar configuración y construcción
configure $CONFIGURE_OPTIONS

# Si el modo loop está habilitado, ejecutamos en bucle
if ${OPTION_FLAGS[loop]}; then
	loop_execution
else
	make_exec compile_commands
	make_exec
	make_exec check

	for option in "${!MAKE_TARGETS[@]}"; do
		if ${OPTION_FLAGS[$option]}; then
			run_make_target "${MAKE_TARGETS[$option]}" || true
		fi
	done

	make_exec maintainer-clean
fi
