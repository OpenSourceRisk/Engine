# Default Comparison Configuration File

The file *comparison_config.json* holds the default configuration that is used for comparing expected and generated csv, json and text files during the regression and Example test runs.

Under the field `csv_settings` and `files`, there is a collection of objects. Each object has a key that is equal to either a file name or a regular expression that should ultimately match a file name. This configuration is read into an `OrderedDict` in Python and the files that are to be compared during a test run are compared against the keys to determine the comparison configuration that they should use. For this reason, keys with exact names should be placed before regular expression keys in the configuration so that they will be found first.

Each object under a given key, `file_name`, has the following format:
  ```js
  {
    "csv_settings":
    {
      "files":
      {
        "file_name":
        {
          "keys": [
            "a",
            "b"
          ],
          "use_cols": [
            "col1",
            "col2",
            "col3"
          ],
          "optional_cols": [
            "col4"
          ]
          "rename_cols":
          {
            "A": "a",
            "B": "b",
          },
          "col_types" : {
            "a": "str"
          },
          "drop_rows" : {
            "cols": [
              "x_1"
            ],
            "threshold": 0.0
          },
          "column_settings": [
            {
              "names": [
                "x_1",
                "x_2"
              ],
              "abs_tol": null,
              "rel_tol": 1e-12
            },
            {
              "names": [
                "col4"
              ],
              "abs_tol": null,
              "rel_tol": null
            }
          ]
        }
      }
    }
  }
  ```

- The `keys` specify which columns will be used as keys for the comparison. The comparison fails if all of these keys are not in both files to be compared.
- The `use_cols` specifies on which columns the actual comparisons are evaluated.
- The `optional_cols`, as with `use_cols` above, specifies columns on which comparisons are evaluated, but these columns are only included if they are present in both files. If they are not present in either file or if present in one file nad not the other, the corresponding comparison defined in `column_settings` below will not be evaluated for the missing column/s.
- The `rename_cols` object specifies columns that should be renamed before the comparison is performed. In the example above, `A` would be renamed to `a` etc.
- The `col_types` object allows you to explicitly specify the type of a given set of columns if necessary.
- The `drop_rows` object allows you to specify a threshold for the values in a given set of columns. If the absolute value for a given row, in one of the specified columns, is below the threshold, the row is dropped from the comparison.
- The `column_settings` object allows you to specify an absolute and/or a relative tolerance that should be used for a group of columns when comparing their values. There can be multiple groupings used in the `column_settings` array with different values of absolute and relative tolerance.

For new files that would require the same comparison config as another standard file, e.g "simm_additional.csv" from a SIMM Impact calc would be the same as a "simm.csv" report from a SIMM calc, they can copy that file's comparison config:

For the regression tests under the *RegressionTests* directory and the Example tests under *Examples* and *ExamplesPlus*, each test may have its own specific comparison configuration file following this format. If a test specific comparison configuration file is present, it is merged with this default comparison configuration file to give the final comparison configuration file used for the test. The merge function is in the file *merge_comparison_configs.py* and it uses the following logic:
- The test specific file is used as the starting point for the final merged configuration `OrderedDict`.
- Any file names in the default comparison configuration file that are not in the test specific comparison configuration file, are added *at the end* of the merged configuration `OrderedDict`. They will therefore only be used during comparison if there is not a match in the test specific file.
