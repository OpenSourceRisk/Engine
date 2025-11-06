#!/bin/bash

status=0
notebook_dir="./Notebooks"
output_dir="./tmp"

mkdir -p "$output_dir"
export PYTHONUNBUFFERED=1

# 🧹 Clean up zombie Jupyter kernel processes
echo "🔍 Checking for zombie Jupyter kernel processes..."
for pid in $(ps aux | grep '[j]upyter' | awk '{print $2}'); do
    echo "⚠️ Killing zombie Jupyter process PID $pid"
    kill -9 "$pid"
done

pids=()

MAX_PARALLEL=4
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
        papermill "$notebook_basename" "$output_path"
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

echo "Highest exit code: $status"
#rm -d -rf ./tmp
exit $status

#expected_dir="./ExpectedResults"
#return_code=0
#output_status=0
#compare_status=0
#for dir in $(find $notebook_dir -type d); do
#    if [[ $(basename "$dir") == "Input" || $(basename "$dir") == "Output" ||  $(basename "$dir") == "Notebooks" || $(basename "$dir") == "ExpectedOutput" ]]; then
#        continue
#    fi

#    relative_dir=${dir#$notebook_dir/}

    # Loop over all ipynb files in the current subdirectory
#    for file in "$dir"/*.ipynb; do
        # Run nbconvert on the current ipynb file
#        if test -f "$file"; then
#            jupyter nbconvert --execute "$file" --to notebook   --output-dir="./tmp" --output=$(basename "$file")
#            return_code=$?
#            if [ $return_code -gt $status ]; then
#                    status=$return_code
#            fi

            #if [[ $(basename "$file") == "progress.ipynb" ]]; then
            #    continue
            #fi

            # Check the notebook's conversion and internal outputs
            #expected_file="$notebook_dir/$relative_dir/ExpectedOutput/$(basename "$file")"
            #actual_file="./tmp/$(basename "$file")"
            #python3 compare_results.py "notebooks" "$actual_file" "$expected_file"
            #compare_status=$?
            #if [ $compare_status -gt $status ]; then
            #    status=$compare_status
            #fi
            #if [[ $(basename "$file") == "hello.ipynb" ]]; then
            #    continue
            #fi

            # Compare the outputted reports of each notebook run
            #TODO: ignore example_7 outputs until fix
            #if [[ "$relative_dir" == "Example_7" ]]; then 
            #    continue 
            #fi
            #python3 compare_results.py "$relative_dir" "$notebook_dir/$relative_dir" "comparison_config.json"
            #output_status=$?
            #if [ $output_status -gt $status ]; then
            #    status=$compare_status
            #fi
#        fi
#    done
#done

# run .ipynb not in Notebooks directory
# cannot compare QuasiMonteCarloMethods notebooks - there is no 'expected output' due to the use of random generators
#jupyter nbconvert --execute "./Notebooks/QuasiMonteCarloMethods.ipynb" --to notebook   --output-dir="./tmp" --output="QuasiMonteCarloMethods"

# clean up