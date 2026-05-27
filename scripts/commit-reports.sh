#!/usr/bin/env bash
# Commit newly-created benchmark reports with an auto-generated description.
#
# Usage:
#   commit-reports.sh                   # batch commit all new reports
#   commit-reports.sh "note about run"  # batch commit, prepending a note
#   commit-reports.sh --individual      # one commit per report (each with its own summary)
#   commit-reports.sh --dry-run         # print what would be committed, change nothing
#
# Reports must be in timepitch-bench/reports/ relative to repo root. The script:
#   - finds untracked .json files matching the report naming convention
#   - reads metadata from each (host, library revs, params)
#   - composes a commit message; commits the .json + matching .txt together.

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || true)"
if [[ -z "${REPO_ROOT}" ]]; then
    echo "error: not in a git repo" >&2
    exit 1
fi

REPORTS_DIR="${REPO_ROOT}/timepitch-bench/reports"
if [[ ! -d "${REPORTS_DIR}" ]]; then
    echo "error: ${REPORTS_DIR} not found" >&2
    exit 1
fi

# --- parse args -------------------------------------------------------------
INDIVIDUAL=0
DRY_RUN=0
USER_NOTE=""
for arg in "$@"; do
    case "${arg}" in
        --individual) INDIVIDUAL=1 ;;
        --dry-run)    DRY_RUN=1 ;;
        --help|-h)
            sed -n '2,11p' "$0" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        --*)
            echo "unknown flag: ${arg}" >&2; exit 2 ;;
        *)
            if [[ -n "${USER_NOTE}" ]]; then
                echo "error: only one note string allowed" >&2; exit 2
            fi
            USER_NOTE="${arg}" ;;
    esac
done

# --- collect untracked reports ---------------------------------------------
cd "${REPO_ROOT}"
mapfile -t NEW_JSON < <(git ls-files --others --exclude-standard -- \
    "timepitch-bench/reports/*.json" 2>/dev/null || true)

if [[ ${#NEW_JSON[@]} -eq 0 ]]; then
    echo "no new report files in ${REPORTS_DIR}"
    exit 0
fi

# --- helper: extract a top-level JSON string field --------------------------
json_str() {
    local file="$1" key="$2"
    python3 -c "
import json, sys
with open(sys.argv[1]) as f:
    d = json.load(f)
def walk(d, path):
    cur = d
    for p in path.split('.'):
        if isinstance(cur, dict) and p in cur:
            cur = cur[p]
        else:
            return ''
    return str(cur)
print(walk(d, sys.argv[2]))" "${file}" "${key}"
}

# --- compose summary for one report file ------------------------------------
summarize() {
    local jf="$1"
    local signal=$(json_str "${jf}" params.signal)
    local tr=$(json_str "${jf}" params.time_ratio)
    local ps=$(json_str "${jf}" params.pitch_scale)
    local block=$(json_str "${jf}" params.block_size)
    local sr=$(json_str "${jf}" params.sample_rate)
    local sweep=$(json_str "${jf}" params.shepard_sweep_rate)
    local host=$(json_str "${jf}" host.hostname)
    local cpu=$(json_str "${jf}" host.cpu_model)
    local dsp=$(json_str "${jf}" build.timepitch_bench_git_rev)
    local dirty=$(json_str "${jf}" build.timepitch_bench_git_dirty)
    local ssm=$(json_str "${jf}" libraries.signalsmith.git_rev)
    local stt=$(json_str "${jf}" libraries.soundtouch.git_rev)
    local rbn=$(json_str "${jf}" libraries.rubberband.git_rev)

    echo "  signal=${signal} time=${tr} pitch=${ps} block=${block} sr=${sr}"
    if [[ "${signal}" == "shepard" ]]; then
        echo "  shepard_sweep_rate=${sweep}"
    fi
    echo "  host=${host} (${cpu})"
    echo "  timepitch-bench=${dsp}$([[ ${dirty} == True || ${dirty} == true ]] && echo ' (dirty)')"
    echo "  libs: signalsmith=${ssm} soundtouch=${stt} rubberband=${rbn}"
}

# --- compose commit message for a set of reports ----------------------------
compose_message() {
    local files=("$@")
    local n=${#files[@]}
    local first="${files[0]}"
    local signal=$(json_str "${first}" params.signal)
    local host=$(json_str "${first}" host.hostname)

    local subject
    if [[ ${n} -eq 1 ]]; then
        local tr=$(json_str "${first}" params.time_ratio)
        local ps=$(json_str "${first}" params.pitch_scale)
        subject="bench: ${signal} t=${tr} p=${ps} on ${host}"
    else
        local mixed=0
        for f in "${files[@]}"; do
            local s=$(json_str "${f}" params.signal)
            if [[ "${s}" != "${signal}" ]]; then mixed=1; break; fi
        done
        if [[ ${mixed} -eq 1 ]]; then
            subject="bench: ${n} runs on ${host}"
        else
            subject="bench: ${n} ${signal} runs on ${host}"
        fi
    fi
    echo "${subject}"
    echo
    if [[ -n "${USER_NOTE}" ]]; then
        echo "${USER_NOTE}"
        echo
    fi
    for f in "${files[@]}"; do
        echo "$(basename "${f}"):"
        summarize "${f}"
        echo
    done
}

# --- commit ----------------------------------------------------------------
commit_files() {
    local files=("$@")
    local txts=()
    for jf in "${files[@]}"; do
        local tf="${jf%.json}.txt"
        if [[ -f "${tf}" ]]; then
            txts+=("${tf}")
        fi
    done

    local msg
    msg="$(compose_message "${files[@]}")"

    if [[ ${DRY_RUN} -eq 1 ]]; then
        echo "--- would commit ---"
        printf '%s\n' "${files[@]}" "${txts[@]}"
        echo "--- with message ---"
        echo "${msg}"
        echo "--------------------"
        return
    fi

    git add -- "${files[@]}" "${txts[@]}"
    git commit -m "${msg}"
}

if [[ ${INDIVIDUAL} -eq 1 ]]; then
    for jf in "${NEW_JSON[@]}"; do
        commit_files "${jf}"
    done
else
    commit_files "${NEW_JSON[@]}"
fi
