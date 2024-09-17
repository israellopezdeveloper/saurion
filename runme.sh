#!/bin/sh

set -eu

SCRIPT_PATH=$(realpath "$(dirname "$0")")

BLUE="\e[1;94m"
GREEN="\e[1;92m"
RESET="\e[0m"

STOPPABLE=false
DOCS_ENABLED=false
MEMLEAK_ENABLED=false
COVERAGE_ENABLED=false
LOOP_ENABLED=false # Nueva bandera para activar el modo loop

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
	if ${STOPPABLE}; then
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

generate_documentation() {
	banner "Generating Documentation"
	make doxygen-doc &&
		wait_key
}

run_leak_check() {
	banner "Running Memory Leak Analysis"
	make leak-check &&
		wait_key
}

run_coverage_tests() {
	banner "Running Coverage Tests"
	make coverage &&
		wait_key
}

show_help() {
	echo "Usage: $0 [options]"
	echo "Options:"
	echo "  -d, --docs       Enable document generation"
	echo "  -l, --leak       Enable memory leak check"
	echo "  -c, --coverage   Enable coverage tests"
	echo "  -i, --loop       Enable loop mode"
	echo "  -h, --help       Show this help message"
}

loop_execution() {
	make_exec compile_commands || continue
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
			continue
		}

		if $MEMLEAK_ENABLED; then
			run_leak_check || continue
		fi

		if $COVERAGE_ENABLED; then
			run_coverage_tests || continue
		fi

		if $DOCS_ENABLED; then
			generate_documentation || continue
		fi

	done
}

# Procesar las opciones de línea de comandos
while [ $# -gt 0 ]; do
	case "$1" in
	-d | --docs)
		DOCS_ENABLED=true
		shift
		;;
	-l | --leak)
		MEMLEAK_ENABLED=true
		shift
		;;
	-c | --coverage)
		COVERAGE_ENABLED=true
		shift
		;;
	-i | --loop)
		LOOP_ENABLED=true
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

if $DOCS_ENABLED; then
	CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --enable-doxygen-doc"
fi

if $MEMLEAK_ENABLED; then
	CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --enable-build-mode=memleak"
fi

if $COVERAGE_ENABLED; then
	CONFIGURE_OPTIONS="$CONFIGURE_OPTIONS --enable-build-mode=coverage"
fi

# Ejecutar configuración y construcción
configure $CONFIGURE_OPTIONS

# Si el modo loop está habilitado, ejecutamos en bucle
if $LOOP_ENABLED; then
	loop_execution
else
	make_exec compile_commands
	make_exec
	make_exec check

	# Ejecutar generación de documentación si está habilitado
	if $DOCS_ENABLED; then
		generate_documentation
	fi

	# Ejecutar análisis de memory leaks si está habilitado
	if $MEMLEAK_ENABLED; then
		run_leak_check
	fi

	# Ejecutar coverage tests si está habilitado
	if $COVERAGE_ENABLED; then
		run_coverage_tests
	fi

	make_exec maintainer-clean
fi
