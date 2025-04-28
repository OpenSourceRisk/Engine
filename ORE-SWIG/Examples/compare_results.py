import sys
import json
import nbformat
import os
import ast
from compare_files import compare_files, read_comparison_config, is_float  # noqa

def read_notebook(notebook_path):
    with open(notebook_path, 'r', encoding='utf-8') as f:
        return nbformat.read(f, as_version=4)

def convert(str):
    idx1 = str.find("array(")
    idx2 = str.find("])")
    substr = str[idx1 + len("array(") + 1 : idx2]
    res = ''.join(substr.split())
    arr = ast.literal_eval(res)
    return arr

def compare_floats(arr1, arr2, tolerance=1e-4):
    for token1, token2 in zip(arr1, arr2):
        try:
            value1 = float(token1)
            value2 = float(token2)
            if abs(value1 - value2) > tolerance:
                return 1
        except ValueError:
            # Handle non-numeric tokens if needed
            if token1 != token2:
                return 1
    return 0


def extract_output_cells(notebook):
    ignore_phrases = ["Run time", "today's date", "[notice]", "Requirement"]
    output_data = {}
    for cell in notebook.cells:
        if cell.cell_type == 'code':
            outputs = cell.get('outputs', [])
            tmp = ""
            for output in outputs:
                if output.output_type == 'execute_result':
                    output_data[cell.execution_count] = output['data']
                elif output.output_type == 'stream':
                    if any(phrase in output['text'] for phrase in ignore_phrases):
                        continue
                    else:
                        tmp += ''.join(output['text'])
                        # depending on the way the module was imported, could lead to ORE.ORE.[class] vs ORE.[class] mismatch
                        if "type <class 'ORE.ORE." in tmp:
                            tmp = tmp.replace("type <class 'ORE.ORE.", "type <class 'ORE.")
                        output_data[cell.execution_count] = tmp
                # can't compare two images since the base64 representation changes every time it is rendered'
#                elif output.output_type == 'display_data': 
#                    output_data[cell.execution_count] = output['data']
    return output_data

def compare_notebooks(actual_notebook_path, expected_notebook_path):
    actual_notebook = read_notebook(actual_notebook_path)
    expected_notebook = read_notebook(expected_notebook_path)
    
    actual_output = extract_output_cells(actual_notebook)
    expected_output = extract_output_cells(expected_notebook)

    if actual_output != expected_output:
        status = 0
        for i in actual_output:
            if actual_output.get(i)!=expected_output.get(i):
                if 'text/plain' in actual_output.get(i):
                    actual_dict = dict(actual_output.get(i))
                    expected_dict = dict(expected_output.get(i))
                    actual_floats = []
                    expected_floats = []
                    if 'array' in actual_dict['text/plain']:
                        actual_floats = convert(actual_dict['text/plain'])
                        expected_floats = convert(expected_dict['text/plain'])
                    elif is_float(actual_dict['text/plain']):
                        actual_floats = [float(actual_dict['text/plain'])]
                        expected_floats = [float(expected_dict['text/plain'])]
                    else:
                        print("execution count: ", i)
                        print("actual output: ")
                        print(actual_output.get(i))
                        print("------------------------------------------------------")
                        print("expected output: ")
                        print(expected_output.get(i))
                        status += 1
                        continue
                    cell_status = compare_floats(actual_floats, expected_floats)
                    if cell_status == 1:
                        print("execution count: ", i, " [check tolerance]")
                        print(actual_output.get(i))
                        status += cell_status
                else:
                    status += 1
                    print("execution count: ", i)
                    print("actual output: ")
                    print(actual_output.get(i))
                    print("------------------------------------------------------")
                    print("expected output: ")
                    print(expected_output.get(i))
        if status != 0:
            print(f"Output mismatch in notebook: {actual_notebook_path}")
            return 1

    print(f"Notebook {actual_notebook_path} matches expected output.")
    return 0

def compare_all_files(example_name, path, expected_output_path, comp_config):
    """Method that loops over the analytics results files and compares them to expected output."""

    print(f"Start comparing output files for {example_name}")

    failed_files = []
    if not os.path.isfile(comp_config):
        raise ValueError('Expected path ' + comp_config + ' to exist')
    default_config = read_comparison_config(comp_config)
    for f in os.listdir(expected_output_path):
        if f.endswith('.ipynb') or f.endswith('.txt'):
            continue
        try:
            file_1 = os.path.join(expected_output_path, f)
            file_2 = os.path.join(path, 'Output', f)
            fail_msg = 'Error comparing {0} for Example {1}'.format(f, example_name)
            result = compare_files(file_1, file_2, example_name, default_config)
            if result == True:
                continue                
            else:
                print("Failed for: ", file_2)
                failed_files.append(file_2)
        except Exception as e:
            print('Exception: ', e)
            print('Failed for file: ', file_2)
            failed_files.append(file_2)
    if len(failed_files) < 1:  
        print("All output files match expected output.")
        return 0
    else:
        return 1

def compare_txt(file1, file2, tolerance=1e-4):
    ignore_phrases = ["run time", "<class 'ORE."]
    with open(file1, 'r') as f1, open(file2, 'r') as f2:
        lines1 = f1.readlines()
        lines2 = f2.readlines()

    for line1, line2 in zip(lines1, lines2):
        if any(phrase in line1 for phrase in ignore_phrases)  or any(phrase in line2 for phrase in ignore_phrases):
            continue 

        if line1.strip() != line2.strip():
            # If lines are not exactly the same, check for numerical tolerance
            tokens1 = line1.strip().split()
            tokens2 = line2.strip().split()
            return compare_floats(tokens1, tokens2, tolerance)

    return 0

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("incorrect usage")
        sys.exit(1)
    if sys.argv[1] == "notebooks":
        actual_notebook_path = sys.argv[2]
        expected_notebook_path = sys.argv[3]
    
        result = compare_notebooks(actual_notebook_path, expected_notebook_path)
        sys.exit(result)
    elif sys.argv[1] == "txt":
        result_file = sys.argv[2]
        expected_file = sys.argv[3]

        result = compare_txt(result_file, expected_file)
        sys.exit(result)

    else:
        example_name = sys.argv[1]
        path = sys.argv[2]
        comp_config = sys.argv[3]

        if example_name == "ore":
            expected_output_path = os.path.join(path, 'ExpectedOutput', 'ORE-Output')
        elif example_name == "portfolio":
            expected_output_path = os.path.join(path, 'ExpectedOutput', 'Portfolio')
        else:
            expected_output_path = os.path.join(path, 'ExpectedOutput')

        result = compare_all_files(example_name, path, expected_output_path, comp_config)
        sys.exit(result)

