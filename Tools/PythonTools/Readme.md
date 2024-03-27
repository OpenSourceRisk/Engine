# Default Comparison Configuration File

The file *comparison_config.json* holds the default configuration that is used for comparing expected and generated csv, json and text files during the regression and Example test runs.

Under the field `csv_settings`, `json_settings` and `files`, there is a collection of objects. Each object has a key that is equal to either a file name or a regular expression that should ultimately match a file name. This configuration is read into an `OrderedDict` in Python and the files that are to be compared during a test run are compared against the keys to determine the comparison configuration that they should use. For this reason, keys with exact names should be placed before regular expression keys in the configuration so that they will be found first.

Each object under a given key, `file_name`, has the following format (note that the formats of `csv_settings` and `json_settings` are different):
  ```js
  {
    "csv_settings": {
      "files": {
        "file_name": {
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
          "require_equal_optional_cols": false,
          "rename_cols": {
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
    },
    "json_settings": {
      "files": {
        "file_name": {
          "ignore_keys": [
            "key1",
            "key2",
            "key3/subkey1"
          ],
          "keys": {
            "key1/": [
              "subkey1",
              "subkey2"
            ],
            "": [
              "subkey1",
              "subkey2"
            ]
          },
          "settings": [
            {
              "names": [
                "key1/subkey1",
                "key2/subkey1/subkey2"
              ],
              "abs_tol": 0.01,
              "rel_tol": 0.001
            }
          ]
        },
        "all_string_file_name": {}
      }
    }
  }
  ```

For `csv_settings`:
  - The `keys` specify which columns will be used as keys for the comparison. The comparison fails if all of these keys are not in both files to be compared.
  - The `use_cols` specifies on which columns the actual comparisons are evaluated.
  - The `optional_cols`, as with `use_cols` above, specifies columns on which comparisons are evaluated, but these columns are only included if they are present in both files. If they are not present in either file or if present in one file nad not the other, the corresponding comparison defined in `column_settings` below will not be evaluated for the missing column/s.
  - The 'require_equal_optional_cols' specifies whether the test is failed if the optional columns are not all present in both files or not all absent in both files. It defaults to true if not given. In this case it is not allowed that an optional column is present in one file but not the other.
  - The `rename_cols` object specifies columns that should be renamed before the comparison is performed. In the example above, `A` would be renamed to `a` etc.
  - The `col_types` object allows you to explicitly specify the type of a given set of columns if necessary.
  - The `drop_rows` object allows you to specify a threshold for the values in a given set of columns. If the absolute value for a given row, in one of the specified columns, is below the threshold, the row is dropped from the comparison.
  - The `column_settings` object allows you to specify an absolute and/or a relative tolerance that should be used for a group of columns when comparing their values. There can be multiple groupings used in the `column_settings` array with different values of absolute and relative tolerance.

  For new files that would require the same comparison config as another standard file, e.g "simm_additional.csv" from a SIMM Impact calc would be the same as a "simm.csv" report from a SIMM calc, they can copy that file's comparison config:

  For the regression tests under the *RegressionTests* directory and the Example tests under *Examples* and *ExamplesPlus*, each test may have its own specific comparison configuration file following this format. If a test specific comparison configuration file is present, it is merged with this default comparison configuration file to give the final comparison configuration file used for the test. The merge function is in the file *merge_comparison_configs.py* and it uses the following logic:
  - The test specific file is used as the starting point for the final merged configuration `OrderedDict`.
  - Any file names in the default comparison configuration file that are not in the test specific comparison configuration file, are added *at the end* of the merged configuration `OrderedDict`. They will therefore only be used during comparison if there is not a match in the test specific file.

For `json_settings`:
  - Any key value (i.e. in `ignore_keys`, `settings.names`, etc.) must include the parent, if any. Using the sample comparison_config.json template above, we would ignore "key1" and "key2" at the top level in a JSON file comparison, and "subkey1" only if it appears inside of "key3".
  - The `ignore_keys` is an array of strings, each string being a key in the JSON file. If the key is found in one or both of the files, any diffs will be ignored for this key and its children (i.e. if the value is itself a nested object).
  - The `keys` is a dictionary whose keys consist of paths, and the values **must consist of** arrays. This will allow for a fallback comparison using `datacompy.core.Compare()` (which is already used for CSV comparisons) and will allow a JSON diff to still pass if the only reason for the diff is that the results are not given in the same order. A blank key ("") means that we expect the whole JSON file consists of an array of unnested JSON objects (and is usually a CSV report that was converted directly to a JSON report). Hence, it would never make sense to have a blank ("") key along with other non-blank keys for the same file comparison config. **NOTE:** A non-blank key must end in a "/".
  - The `settings` object works the same as the `column_settings` file in `csv_settings`, except that the keys must include the parent in the JSON `settings`.
  - **NOTE:** In order for a JSON check to be applied, a comp config must be provided for the filename, even if the config is empty (see e.g. `all_string_filename` in the template above). Otherwise a direct file comparison will be done.
  - **NOTE:** String diffs are automatically processed (i.e. unless they are in `ignore_keys`, then a string diff will be a failing diff) (see e.g. `all_string_filename` in the template above). Only numerical differences need to be handled in `settings`.

