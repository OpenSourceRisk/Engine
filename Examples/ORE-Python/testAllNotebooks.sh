#!/bin/bash

status=0
notebook_dir="./Notebooks"
output_dir="./tmp"

mkdir -p "$output_dir"
export PYTHONUNBUFFERED=1

pids=()

MAX_PARALLEL=2
running=0

# Run notebooks in parallel using papermill
for notebook in $(find "$notebook_dir" -type f -name "*.ipynb" | grep -Ev '/(Input|Output|ExpectedOutput)/'); do
    echo "Running $notebook"

    notebook_dirname=$(dirname "$notebook")
    notebook_basename=$(basename "$notebook")
    parent_folder=$(basename "$(dirname "$notebook")")

    # Create a unique output directory for this notebook
    notebook_output_dir="$output_dir/$parent_folder"
    mkdir -p "$notebook_output_dir"

    output_path="$(realpath "$notebook_output_dir")/$notebook_basename"
    
    (
        cd "$notebook_dirname" || exit
        # --execution-timeout: kill cells that run longer than 10 minutes
        # --start-timeout: fail fast if kernel doesn't start within 60 seconds
        papermill --execution-timeout 600 --start-timeout 60 \
            "$notebook_basename" "$output_path"
        rc=$?
        # Kill any lingering Jupyter kernel processes spawned by this papermill run
        for child in $(jobs -p 2>/dev/null); do
            kill -9 "$child" 2>/dev/null
        done
        exit $rc
    ) &
    
    pids+=($!)
	((running+=1))

    if (( running >= MAX_PARALLEL )); then
        wait -n
        ((running-=1))
    fi

done


# Wait for all background jobs and collect exit codes
for pid in "${pids[@]}"; do
    wait "$pid"
    code=$?
    if (( code > status )); then
        status=$code
    fi
done

# Kill any remaining Jupyter kernel processes that may have leaked
# The ore_app image doesn't have ps/pkill, so scan /proc directly
if [ -d /proc ]; then
    for p in /proc/[0-9]*/cmdline; do
        kpid=$(echo "$p" | cut -d/ -f3)
        # Skip our own PID and PID 1
        [ "$kpid" = "$$" ] || [ "$kpid" = "1" ] && continue
        if tr '\0' ' ' < "$p" 2>/dev/null | grep -qE 'jupyter|ipykernel'; then
            echo "Killing orphaned kernel process $kpid"
            kill -9 "$kpid" 2>/dev/null || true
        fi
    done
fi

echo "Highest exit code: $status"
exit $status