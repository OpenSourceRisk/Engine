#!/bin/sh
status=0
return_code=0
tools_dir="../../../Tools/PythonTools"
expected_dir="./ExpectedOutput"
output_file="output.txt"
> "$output_file"

#dos2unix $expected_dir/*.txt

for file in *.py; do
    if [ -f "$file" ]; then
        > "$output_file"
        echo RUN $file  | tee -a "$output_file"
        python3 "$file"  &>> "$output_file" || status=1
        return_code=$?
        if [ $return_code -gt $status ]; then
                status+=$return_code
        fi

        if [[ "$file" == "log.py" ]]; then
            continue
        fi

        file_name="${file%.py}"
        python3 "$tools_dir/compare_results.py" "txt" "$output_file" "$expected_dir/$file_name.txt"
        output_status=$?
        if [ $output_status -eq 0 ]; then
            echo "The output matches the expected output."
        else
            echo "The output differs from the expected output for $file_name."
            status+=$output_status
        fi

    fi
done

python3 log.py || status=1

# clean up
rm $output_file
exit $status
