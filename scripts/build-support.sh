#!/usr/bin/env bash

ARCAL_SUSPENDED_CPPTOOLS_PIDS=()

arcal_suspend_cpptools() {
    ARCAL_SUSPENDED_CPPTOOLS_PIDS=()

    case "${ARCAL_SUSPEND_CPPTOOLS:-ON}" in
        1|ON|On|on|TRUE|True|true|YES|Yes|yes) ;;
        *) return 0 ;;
    esac

    if ! command -v pgrep >/dev/null 2>&1; then
        return 0
    fi

    mapfile -t ARCAL_SUSPENDED_CPPTOOLS_PIDS < <(
        pgrep -u "$(id -u)" -f '/cpptools(-srv)?( |$)' || true
    )

    if [[ ${#ARCAL_SUSPENDED_CPPTOOLS_PIDS[@]} -eq 0 ]]; then
        return 0
    fi

    echo "Suspending VS Code cpptools during build (${#ARCAL_SUSPENDED_CPPTOOLS_PIDS[@]} process(es)); set ARCAL_SUSPEND_CPPTOOLS=OFF to disable." >&2
    kill -STOP "${ARCAL_SUSPENDED_CPPTOOLS_PIDS[@]}" 2>/dev/null || true
}

arcal_resume_cpptools() {
    if [[ ${#ARCAL_SUSPENDED_CPPTOOLS_PIDS[@]} -eq 0 ]]; then
        return 0
    fi

    kill -CONT "${ARCAL_SUSPENDED_CPPTOOLS_PIDS[@]}" 2>/dev/null || true
    ARCAL_SUSPENDED_CPPTOOLS_PIDS=()
}

# Given a subset config JSON path, echo the CMake target base name.
# Mirrors string(MAKE_C_IDENTIFIER) used in arcal_add_subset().
arcal_target_name_for_config() {
    local config_path="$1"
    local suffix
    suffix=$(python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['cal_name_suffix'])" "$config_path")
    local name="arcal-${suffix}"
    echo "${name//[^a-zA-Z0-9_]/_}"
}
