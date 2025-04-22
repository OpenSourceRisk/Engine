#!/bin/sh
status=0
return_code=0
expected_dir="./ExpectedOutput"
output_file="output.txt"
> "$output_file"

#dos2unix $expected_dir/*.txt

for file in *.py; do
    if [ -f "$file" ]; then
        > "$output_file"
        if [[ "$file" == "compare_files.py" ||  "$file" == "compare_results.py" ]]; then
            continue
        fi
        echo RUN $file  | tee -a "$output_file"
        python3 "$file"  &>> "$output_file" || status=1
        return_code=$?
        if [ $return_code -gt $status ]; then
                status=$return_code
        fi

        if [[ "$file" == "log.py" ]]; then
            continue
        fi

        file_name="${file%.py}"
        python3 compare_results.py "txt" "$output_file" "$expected_dir/$file_name.txt"
        output_status=$?
        if [ $output_status -eq 0 ]; then
            echo "The output matches the expected output."
        else
            echo "The output differs from the expected output for $file_name."
            status=$output_status
        fi

        # check outputted reports for ore.py
        if [[ "$file" == "ore.py" ]]; then
            python3 compare_results.py "ore" "." "comparison_config.json"
            output_status=$?
            if [ $output_status -gt $status ]; then
                status=$compare_status
            fi
        fi

        # check outputted reports for portfolio.py
        if [[ "$file" == "portfolio.py" ]]; then
            python3 compare_results.py "portfolio" "." "comparison_config.json"
            output_status=$?
            if [ $output_status -gt $status ]; then
                status=$compare_status
            fi
        fi
    fi
done

# clean up
rm $output_file
exit $status
