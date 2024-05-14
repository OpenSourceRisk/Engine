"""Merge a specific comparison configuration, if given, and a default or fallback comparison configuration.

The logic is kept quite simple here. The comparison configuration is a set of rules for comparing various csv, json and
text files. The rules are keyed on the file name under the keys csv_settings and files. The file name can be an exact
file name or a regular expression.

If no specific configuration is given, the default configuration is returned.

If a specific configuration is given, it is used as the base configuration. Any file name under csv_settings and files
in the default configuration that is not in the specific configuration is added to the end of the merged configuration
so that the specific configuration takes precedence in any comparison. If there is a file name under csv_settings and
files in the default configuration that is also in the specific configuration, it is ignored and the merged
configuration will contain the specific configuration settings for that file name. An attempt is not made to merge the
settings when the file names are the same.

"""
import argparse
import json
import logging
import os
from collections import OrderedDict


def read_comparison_config(path):
    """Read in comparison configuration and check structure."""

    with open(path, 'r') as f:
        config = json.load(f, object_pairs_hook=OrderedDict)

    # if ('csv_settings' not in config and 'json_settings' not in config) or ('files' not in config['csv_settings'] and 'files' not in config['json_settings']):
    #     raise ValueError('Expected csv_settings or csv_settings...files keys in comparison configuration.')

    return config


def merge_configurations(default_config_path, specific_config_path=None, output_path=None):
    """Merge specific configuration, if given, with the default configuration."""

    # Read in the default comparison configuration
    default_config = read_comparison_config(default_config_path)

    # If a path to the specific configuration is not provided, just return the default configuration.
    if not specific_config_path:
        return default_config

    full_merged_config = {}
    for settings_type in ['csv_settings', 'json_settings']:
        default_files_config = default_config[settings_type]['files'] \
            if default_config and settings_type in default_config and 'files' in default_config[settings_type] \
            else {}
            
        # Read in the specific configuration as the starting merged config.
        merged_config = read_comparison_config(specific_config_path)
        merged_files_config = merged_config[settings_type]['files'] \
            if merged_config and settings_type in merged_config and 'files' in merged_config[settings_type] \
            else {}

        for key, value in default_files_config.items():
            if key not in merged_files_config:
                merged_files_config[key] = value

        full_merged_config[settings_type] = { "files": merged_files_config }

    # Write out merged config if path provided.
    if output_path:
        output_file = os.path.join(output_path, 'merged_comparison_config.json')
        with open(output_file, 'w') as f:
            json.dump(merged_config, f)

    return full_merged_config


if __name__ == "__main__":
    # Parse input parameters
    parser = argparse.ArgumentParser(description='Merge a specific comparison config and a default')
    parser.add_argument('--default_config', help='Path to default comparison configuration', required=True)
    parser.add_argument('--specific_config', help='Path to specific configuration to merge', required=False,
                        default=None)
    parser.add_argument('--output_path', help='Optional path to dump merged json comparison file.', required=False,
                        default=None)
    args = parser.parse_args()

    main_logger = logging.getLogger(__name__)
    main_logger.info('Start merging configurations.')
    main_result = merge_configurations(args.default_config, args.specific_config, args.output_path)
    main_logger.info('Finished merging configurations.')
