#!/bin/sh
status=0
return_code=0
output_status=0
compare_status=0

expected_dir="./ExpectedResults"
notebook_dir="./Notebooks"

mkdir -p "tmp"

for dir in $(find $notebook_dir -type d); do

    if [[ $(basename "$dir") == "Input" || $(basename "$dir") == "Output" ||  $(basename "$dir") == "Notebooks" || $(basename "$dir") == "ExpectedOutput" ]]; then
        continue
    fi

    relative_dir=${dir#$notebook_dir/}

    # Loop over all ipynb files in the current subdirectory
    for file in "$dir"/*.ipynb; do
        # Run nbconvert on the current ipynb file
        if test -f "$file"; then
            jupyter nbconvert --execute "$file" --to notebook   --output-dir="./tmp" --output=$(basename "$file")
            return_code=$?
            if [ $return_code -gt $status ]; then
                    status=$return_code
            fi

            if [[ $(basename "$file") == "progress.ipynb" ]]; then
                continue
            fi

            # Check the notebook's conversion and internal outputs
            expected_file="$notebook_dir/$relative_dir/ExpectedOutput/$(basename "$file")"
            actual_file="./tmp/$(basename "$file")"
            python3 compare_results.py "notebooks" "$actual_file" "$expected_file"
            compare_status=$?
            if [ $compare_status -gt $status ]; then
                status=$compare_status
            fi
            if [[ $(basename "$file") == "hello.ipynb" ]]; then
                continue
            fi

            # Compare the outputted reports of each notebook run
            #TODO: ignore example_7 outputs until fix
            if [[ "$relative_dir" == "Example_7" ]]; then 
                continue 
            fi
            python3 compare_results.py "$relative_dir" "$notebook_dir/$relative_dir" "comparison_config.json"
            output_status=$?
            if [ $output_status -gt $status ]; then
                status=$compare_status
            fi
        fi
    done
done

# run .ipynb not in Notebooks directory
# cannot compare QuasiMonteCarloMethods notebooks - there is no 'expected output' due to the use of random generators
jupyter nbconvert --execute "./QuasiMonteCarloMethods.ipynb" --to notebook   --output-dir="./tmp" --output="QuasiMonteCarloMethods"

# clean up
rm -d -rf ./tmp
exit $status