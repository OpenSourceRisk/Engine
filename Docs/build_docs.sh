#!/bin/bash

set -e

# Main tex files that we wish to build
tex_files=(
  "ore/Docs/AMC/amc.tex"
  "ore/Docs/DIM/dynamicsimm.tex"
  "ore/Docs/CodingStandards/ore_coding_standards.tex"
  "ore/Docs/ComputeEnvironment/computeenvironment.tex"
  "ore/Docs/CreditModel/creditmodel.tex"
  "ore/Docs/Models/OpenXVA_Models.tex"
  "ore/Docs/ScriptedTrade/scriptedtrade.tex"
  "ore/Docs/ScriptedTrade/scriptedtrades_analysis.tex"
  "ore/Docs/FormulaBasedCoupon/formulabasedcoupon.tex"
  "ore/Docs/UserGuide/userguide.tex"
  "ore/Docs/UserGuide/products.tex"
  "ore/Docs/UserGuide/methods.tex"
  "ore/Docs/Design/ore_design.tex"
  "ore/Docs/BondPricingConfig/bondpricingconfig.tex"
  "ore/Docs/HullWhiteModel/hullwhitemodel.tex"
  "ore/Docs/SabrModel/sabrmodel.tex"
  "ore/Docs/Inflation/inflation_simulation.tex"
  "ore/Docs/SviModel/svimodel.tex"
)

# Base directory of the repository. This script lives in ore/Docs, which is two
# levels below the repository root, so go up twice to find it.
base_dir="$(cd "$(dirname "$0")"; pwd)"/../..
export base_dir

# Directory for per-document build logs. Keeping each latexmk run's output in its
# own file stops the parallel runs from interleaving, so we can report exactly
# which document failed (and why) in the summary printed at the end.
log_dir="${base_dir}/pdf_build_logs"
rm -rf "${log_dir}"
mkdir -p "${log_dir}"

# Map a tex path to a flat, safe log-file name,
# e.g. ore/Docs/AMC/amc.tex -> ore_Docs_AMC_amc_tex
log_name() { printf '%s' "$1" | tr '/.' '__'; }

task() {
  local tex_file="$1"
  local build_log="${log_dir}/$(log_name "${tex_file}").out"

  # Capture all output of this document's build into its own log file so the
  # parallel runs do not interleave on the console.
  local rc=0
  local start_s=${SECONDS}
  {
    echo "Processing file ${tex_file}"
    echo "PDF file is ${tex_file/.tex/.pdf}"
    # Override LATEXMK_CMD to change how latexmk is launched (see latexdockercmd.sh).
    ${LATEXMK_CMD:-latexmk} -cd -f -interaction=batchmode -gg -e '$pdflatex=q/pdflatex %O -shell-escape %S/' -outdir=./ -pdf "${tex_file}"
  } > "${build_log}" 2>&1 || rc=$?
  local elapsed=$(( SECONDS - start_s ))

  # Record the build duration so the summary can flag slow documents.
  printf '%5ds  %s\n' "${elapsed}" "${tex_file}" > "${log_dir}/$(log_name "${tex_file}").time"

  if [ "${rc}" -eq 0 ]; then
    echo "PASS: ${tex_file} (${elapsed}s)"
    # Clean out non-essential files.
    # ${LATEXMK_CMD:-latexmk} -cd -interaction=batchmode -c -outdir=./ -pdf "${tex_file}" >> "${build_log}" 2>&1 || true
  else
    # Drop a marker the parent process scans after all jobs finish.
    printf '%s\n' "${tex_file}" > "${log_dir}/$(log_name "${tex_file}").failed"
    echo "FAIL: ${tex_file} (latexmk exit ${rc}, ${elapsed}s) - see summary below"
  fi
  return "${rc}"
}

# Print a consolidated summary at the end of the log so failures - and which file
# caused them - are easy to find without scrolling through interleaved output.
print_summary() {
  shopt -s nullglob
  local markers=( "${log_dir}"/*.failed )

  echo ""
  echo "================== PDF BUILD SUMMARY =================="
  echo "Documents built : ${#tex_files[@]}"
  echo "Documents failed: ${#markers[@]}"

  # Build durations, slowest first, so the job's bottleneck is obvious.
  local times=( "${log_dir}"/*.time )
  if [ "${#times[@]}" -gt 0 ]; then
    echo "------------------------------------------------------"
    echo "BUILD TIMES (slowest first):"
    cat "${times[@]}" | sort -rn
  fi

  if [ "${#markers[@]}" -eq 0 ]; then
    echo "All documents built successfully."
    echo "======================================================"
    return 0
  fi

  echo "------------------------------------------------------"
  echo "FAILED DOCUMENTS:"
  local m tex_file
  for m in "${markers[@]}"; do
    echo "  - $(cat "${m}")"
  done

  # For each failure, surface the meaningful LaTeX errors plus the tail of the
  # captured latexmk output.
  for m in "${markers[@]}"; do
    tex_file=$(cat "${m}")
    local build_log="${log_dir}/$(log_name "${tex_file}").out"
    local tex_log="${tex_file%.tex}.log"
    echo ""
    echo "====================================================="
    echo ">>> ERRORS for ${tex_file}"
    echo "-----------------------------------------------------"
    if [ -f "${tex_log}" ]; then
      grep -n -E '^!|^l\.[0-9]+|LaTeX Error|Package .* Error|Emergency stop|Runaway argument|Undefined control sequence|undefined on input line|failed to resolve' "${tex_log}" \
        || echo "(no explicit error lines in ${tex_log}; see output tail below)"
    else
      echo "(no LaTeX .log found at ${tex_log})"
    fi
    echo "----- latexmk output tail (${build_log}) -----"
    tail -n 30 "${build_log}" 2>/dev/null || echo "(no captured output)"
  done
  echo "====================================================="
  return 1
}

# Build every document in parallel, each writing to its own log file.
pids=()
for tex_file in "${tex_files[@]}"; do
  task "${tex_file}" &
  pids+=("$!")
done

for pid in "${pids[@]}"; do
  wait "${pid}" || true
done

summary_rc=0
print_summary || summary_rc=$?
exit "${summary_rc}"
