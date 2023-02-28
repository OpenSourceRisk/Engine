# Function for comparing two files.
import os
import argparse
import collections
import copy
import difflib
import json
import logging
import numpy as np
import pandas as pd
from datacompy.core import Compare
import re


def has_csv_config(config, filename_1, filename_2):
    # Check if config has a configuration for filename_1 or filename_2

    for filename in [filename_1, filename_2]:

        for key in config['csv_settings']['files']:
            if re.match(key, filename):
                return config['csv_settings']['files'][key]

    return None


def create_df(file, col_types=None):
    # Read csv or json file into a Dataframe.

    logger = logging.getLogger(__name__)

    logger.debug('Start creating DataFrame from file %s.', file)

    _, filename = os.path.split(file)
    _, file_extension = os.path.splitext(file)

    if file_extension == '.csv' or file_extension == '.txt':
        logger.debug('Creating DataFrame from csv file %s.', file)
        return pd.read_csv(file, dtype=col_types, quotechar='"', quoting=2)
    elif file_extension == '.json':
        if filename == 'simm.json':
            with open(file, 'r') as json_file:
                simm_json = json.load(json_file)
                if 'simmReport' in simm_json:

                    logger.debug('Creating DataFrame from simm json file %s.', file)
                    simm_df = pd.DataFrame(simm_json['simmReport'])

                    # In a lot of regression tests, the NettingSetId field was left blank which in the simm JSON
                    # response is an empty string. This empty string is then in the portfolio column in the DataFrame
                    # created from the simm JSON. When the simm csv file is read in to a DataFrame, the empty field
                    # under Portfolio is read in to a DataFrame as Nan. We replace the empty string here with Nan in
                    # portfolio column so that everything works downstream.
                    simm_df['portfolio'].replace('', np.nan, inplace=True)

                    return simm_df

                else:
                    logger.warning('Expected simm json file %s to contain the field simmReport.', file)
                    return None
        if filename == 'npv.json':
            with open(file, 'r') as json_file:
                npv_json = json.load(json_file)
                if 'npv' in npv_json:

                    logger.debug('Creating DataFrame from npv json file %s.', file)
                    npv_df = pd.DataFrame(npv_json['npv'])

                    # In a lot of regression tests, the NettingSetId field was left blank which in the simm JSON
                    # response is an empty string. This empty string is then in the portfolio column in the DataFrame
                    # created from the simm JSON. When the simm csv file is read in to a DataFrame, the empty field
                    # under Portfolio is read in to a DataFrame as Nan. We replace the empty string here with Nan in
                    # portfolio column so that everything works downstream.
                    npv_df['nettingSetId'].replace('', np.nan, inplace=True)

                    return npv_df

                else:
                    logger.warning('Expected npv json file %s to contain the field npv.', file)
                    return None
        if filename == 'flownpv.json':
            with open(file, 'r') as json_file:
                flownpv_json = json.load(json_file)
                if 'cashflowNpv' in flownpv_json:
                    logger.debug('Creating DataFrame from flownpv json file %s.', file)
                    flownpv_df = pd.DataFrame(flownpv_json['cashflowNpv'])
                    flownpv_df.rename(columns={"baseCurrency" : "BaseCurrency", "horizon" : "Horizon", "presentValue" : "PresentValue", "tradeId" : "TradeId"}, inplace=True)
                    flownpv_df.drop(columns='jobId', inplace=True)
                    return flownpv_df

                else:
                    logger.warning('Expected flownpv json file %s to contain the field cashflowNpv.', file)
                    return None
        if filename == 'saccr.json':
            with open(file, 'r') as json_file:
                saccr_json = json.load(json_file)
                if 'saccr' in saccr_json:
                    logger.debug('Creating DataFrame from saccr json file %s.', file)
                    saccr_df = pd.DataFrame(saccr_json['saccr'])

                    saccr_cols = {
                        'assetClass': 'AssetClass',
                        'hedgingSet': 'HedgingSet',
                        'nettingSetId': 'NettingSet',
                        'addOn': 'AddOn',
                        'cC': 'CC',
                        'eAD': 'EAD',
                        'multiplier': 'Multiplier',
                        'npv': 'NPV',
                        'pfe': 'PFE',
                        'rC': 'RC',
                        'rW': 'RW'
                    }

                    for col in ['addOn', 'cC', 'eAD', 'multiplier', 'npv', 'pfe', 'rC', 'assetClass', 'hedgingSet', 'nettingSetId']:
                        saccr_df[col].replace('', np.nan, inplace=True)

                    for col in ['rW']:
                        saccr_df[col].replace(0.0, np.nan, inplace=True)

                    saccr_df.rename(columns=saccr_cols, inplace=True)
                    return saccr_df
                else:
                    logger.warning('Expected saccr json file %s to contain the field saccr', file)
                    return None
        if filename == 'frtb.json':
            with open(file, 'r') as json_file:
                frtb_json = json.load(json_file)
                if 'frtb' in frtb_json:
                    logger.debug('Creating DataFrame from frtb json file %s.', file)
                    frtb_df = pd.DataFrame(frtb_json['frtb'])

                    # In a lot of regression tests, the NettingSetId field was left blank which in the simm JSON
                    # response is an empty string. This empty string is then in the portfolio column in the DataFrame
                    # created from the simm JSON. When the simm csv file is read in to a DataFrame, the empty field
                    # under Portfolio is read in to a DataFrame as Nan. We replace the empty string here with Nan in
                    # portfolio column so that everything works downstream.
                    # npv_df['nettingSetId'].replace('', np.nan, inplace=True)

                    frtb_cols = {
                        'bucket': 'Bucket',
                        'capitalRequirement': 'CapitalRequirement',
                        'correlationScenario': 'CorrelationScenario',
                        'frtbCharge': 'FrtbCharge',
                        'risk_class': 'RiskClass'
                    }
                    for col in frtb_cols.keys():
                        frtb_df[col].replace('', np.nan, inplace=True)
                    frtb_df.rename(columns=frtb_cols, inplace=True)
                    frtb_df.drop(columns='jobId', inplace=True)
                    return frtb_df
                else:
                    logger.warning('Expected frtb json file %s to contain the field frtb', file)
                    return None
        if filename == 'bacva.json':
            with open(file, 'r') as json_file:
                bacva_json = json.load(json_file)
                if 'bacva' in bacva_json:
                    logger.debug('Creating DataFrame from bacva json file %s.', file)
                    bacva_df = pd.DataFrame(bacva_json['bacva'])

                    bacva_cols = {
                        'analytic': 'Analytic',
                        'counterparty': 'Counterparty',
                        'nettingSetId': 'NettingSet',
                        'value': 'Value',
                    }

                    for col in ['analytic', 'counterparty', 'nettingSetId']:
                        bacva_df[col].replace('', np.nan, inplace=True)

                    for col in ['value']:
                        bacva_df[col].replace(0.0, np.nan, inplace=True)

                    bacva_df.rename(columns=bacva_cols, inplace=True)
                    return bacva_df
                else:
                    logger.warning('Expected bacva json file %s to contain the field bacva', file)
                    return None   
    else:
        logger.warning('File %s is neither a csv nor a json file so cannot create DataFrame.', file)
        return None

    logger.debug('Finished creating DataFrame from file %s.', file)


def compare_files(file_1, file_2, name, config=None):
    logger = logging.getLogger(__name__)

    logger.info('%s: Start comparing file %s against %s', name, file_1, file_2)

    # Check that both file paths actually exist.
    for file in [file_1, file_2]:
        if not os.path.isfile(file):
            logger.warning('The file path %s does not exist.', file)
            return False

    # Attempt to get the csv comparison configuration to use for the given filenames.
    csv_comp_config = None
    if config is not None:
        _, filename_1 = os.path.split(file_1)
        _, filename_2 = os.path.split(file_2)
        csv_comp_config = has_csv_config(config, filename_1, filename_2)

    # If the file extension is csv or json, enforce the use of a comparison configuration.
    # If the file is a txt file, attempt to find a comparison config. If none, proceed with direct comparison.
    _, ext_1 = os.path.splitext(file_1)
    _, ext_2 = os.path.splitext(file_2)

    if csv_comp_config is None:
        if ext_1 == '.csv':
            raise ValueError('File, ' + file_1 + ', requires a comparison configuration but none given.')
        if ext_2 == '.csv':
            raise ValueError('File, ' + file_2 + ', requires a comparison configuration but none given.')

    if csv_comp_config is None:
        # If there was no configuration then fall back to a straight file comparison.
        result = compare_files_direct(name, file_1, file_2)
    else:
        # If there was a configuration, use it for the comparison.
        result = compare_files_df(name, file_1, file_2, csv_comp_config)

    logger.info('%s: Finished comparing file %s against %s: %s.', name, file_1, file_2, result)

    return result


def compare_files_df(name, file_1, file_2, config):
    # Compare files using dataframes and a configuration.

    logger = logging.getLogger(__name__)

    logger.debug('%s: Start comparing file %s against %s using configuration.', name, file_1, file_2)

    # We can force the type of specific columns here.
    col_types = None
    if 'col_types' in config:
        col_types = config['col_types']

    # Read the files in to dataframes
    df_1 = create_df(file_1, col_types)
    df_2 = create_df(file_2, col_types)

    # Check that a DataFrame could be created for each.
    if df_1 is None:
        logger.warning('A DataFrame could not be created from the file %s.', file_1)
        return False

    if df_2 is None:
        logger.warning('A DataFrame could not be created from the file %s.', file_2)
        return False

    # In most cases, the header column in our csv files starts with a #. Remove it here if necessary.
    for df in [df_1, df_2]:
        first_col_name = df.columns[0]
        if first_col_name.startswith('#'):
            df.rename(columns={first_col_name: first_col_name[1:]}, inplace=True)

    # If we are asked to rename columns, try to do it here.
    if 'rename_cols' in config:
        logger.debug('Applying column renaming, %s, to both DataFrames', str(config['rename_cols']))
        for idx, df in enumerate([df_1, df_2]):
            df.rename(columns=config['rename_cols'], inplace=True)

    # Possibly drop some rows where specified column values are not above the threshold.
    if 'drop_rows' in config:

        criteria_cols = config['drop_rows']['cols']
        threshold = config['drop_rows']['threshold']

        for idx, df in enumerate([df_1, df_2]):
            if not all([elem in df.columns for elem in criteria_cols]):
                logger.warning('The columns, %s, in Dataframe %d do not contain all the drop_rows columns, %s.',
                               str(list(df.columns.values)), idx + 1, str(criteria_cols))
                return False

        df_1 = df_1[pd.DataFrame(abs(df_1[criteria_cols]) > threshold).all(axis=1)]
        df_2 = df_2[pd.DataFrame(abs(df_2[criteria_cols]) > threshold).all(axis=1)]

    # We must know the key(s) on which the comparison is to be performed.
    if 'keys' not in config:
        logger.warning('The comparison configuration must contain a keys field.')
        return False

    # Get the keys and do some checks.
    keys = config['keys']

    # Check keys are not empty
    if not keys or any([elem == '' for elem in keys]):
        logger.warning('The list of keys, %s, must be non-empty and each key must be a non-empty string.', str(keys))
        return False

    # Check that keys contain no duplicates
    dup_keys = [elem for elem, count in collections.Counter(keys).items() if count > 1]
    if dup_keys:
        logger.warning('The keys, %s, contain duplicates, %s.', str(keys), str(dup_keys))
        return False

    # Check that all keys are in each DataFrame
    for idx, df in enumerate([df_1, df_2]):
        if not all([elem in df.columns for elem in keys]):
            logger.warning('The columns, %s, in Dataframe %d do not contain all the keys, %s.',
                           str(list(df.columns.values)), idx + 1, str(keys))
            return False

    # We check for columns that would be used as keys but are not always necessary, 
    # e.g. netting set details, collect_regulations, post_regualtions
    if 'optional_keys' in config:
        optional_keys = config['optional_keys']

        # Check that optional keys are non-empty strings
        if any([elem == '' for elem in optional_keys]):
            logger.warning('The list of optional keys, %s, must be non-empty and each key must be a non-empty string.', str(keys))
            return False

        # Check that optional keys contain no duplicates (within itself or with keys)
        combined_keys = keys + optional_keys
        dup_keys = [elem for elem, count in collections.Counter(combined_keys).items() if count > 1]
        if dup_keys:
            logger.warning('The keys, %s, contain duplicates, %s.', str(combined_keys), str(dup_keys))
            return False

        # For each optional key, check whether it is found in at each DataFrame. If so, add it to the keys.
        for okey in optional_keys:
            if okey in df_1.columns and okey in df_2.columns:
                keys.append(okey)

    # If we are told to use only certain columns, drop the others in each DataFrame. We first check that both
    # DataFrames have all of the explicitly listed columns to use.
    if 'use_cols' in config:

        use_cols = copy.deepcopy(config['use_cols'])
        logger.debug('We will only use the columns, %s, in the comparison.', str(use_cols))
        use_cols += keys

        # Check that all of the requested columns are in the DataFrames.
        for idx, df in enumerate([df_1, df_2]):
            if not all([elem in df.columns for elem in use_cols]):
                logger.warning('The columns, %s, in Dataframe %d do not contain all the named columns, %s.',
                               str(list(df.columns.values)), idx + 1, str(use_cols))
                return False

        if 'optional_cols' in config:
            optional_cols = copy.deepcopy(config['optional_cols'])
            logger.debug('Optional columns found: %s', str (optional_cols))
            for col in optional_cols:
                if col in df_1.columns and col in df_2.columns :
                    logger.debug('Adding optional column %s to list of columns for comparison', col)
                    if col in use_cols:
                        logger.warning('Cannot add optional column %s as it is already in the list of columns for comparison.', col)
                    else:
                        use_cols.append(col)

        # Use only the requested columns.
        df_1 = df_1[use_cols]
        df_2 = df_2[use_cols]

    # Flag that will store the ultimate result i.e. True if the files are considered a match and False otherwise.
    is_match = True

    # Certain groups of columns may need special tolerances for their comparison. Deal with them first.
    cols_compared = set()
    if 'column_settings' in config:

        for col_group_config in config['column_settings']:

            names = col_group_config['names'].copy()

            if not names:
                logger.debug('No column names provided. Use joint columns from both files except keys')
                names = [s for s in list(set(list(df_1.columns) + list(df_2.columns))) if s not in keys]

            logger.info('Performing comparison of files for column names: %s.', str(names))

            # Check that keys contain no duplicates
            dup_names = [elem for elem, count in collections.Counter(names).items() if count > 1]
            if dup_names:
                logger.warning('The names, %s, contain duplicates, %s.', str(names), str(dup_names))
                logger.info('Skipping comparison for this group of names and marking files as different.')
                is_match = False
                continue

            # Check that none of the keys appear in names.
            if any([elem in keys for elem in names]):
                logger.warning('The names, %s, contain some of the keys, %s.', str(names), str(keys))
                logger.info('Skipping comparison for this group of names and marking files as different.')
                is_match = False
                continue

            # Check that all of the names are in each DataFrame, with the exception of optional columns
            optional_cols = config['optional_cols'] if 'optional_cols' in config else []
            required_names = [name for name in names if name not in optional_cols]

            all_names = True
            for idx, df in enumerate([df_1, df_2]):
                if not all([elem in df.columns for elem in required_names]):
                    logger.warning('The column names, %s, in Dataframe %d do not contain all the names, %s.',
                                   str(list(df.columns.values)), idx + 1, str(names))
                    all_names = False
                    break

            if not all_names:
                logger.info('Skipping comparison for this group of names and marking files as different.')
                is_match = False
                continue

            # We will compare this subset of columns using the provided tolerances.
            col_names = keys + required_names

            optional_names = [name for name in names if name in optional_cols]
            for optional_name in optional_names:
                if optional_name in df_1.columns and optional_name in df_2.columns:
                    col_names.append(optional_name)

            # If no (required) use_cols were provided in "names" and all the names were optional_cols
            # that were not present in both dataframes, then we continue to the next column_settings,
            # since this means that there are no columns that we can compare on.
            if len(keys) == len(col_names):
                continue

            # Add to the columns that we have already compared.
            cols_compared.update([name for name in col_names if name not in keys])

            abs_tol = 0.0
            if 'abs_tol' in col_group_config and col_group_config['abs_tol'] is not None:
                abs_tol = col_group_config['abs_tol']

            rel_tol = 0.0
            if 'rel_tol' in col_group_config and col_group_config['rel_tol'] is not None:
                rel_tol = col_group_config['rel_tol']

            sub_df_1 = df_1[col_names].copy(deep=True)
            sub_df_2 = df_2[col_names].copy(deep=True)

            comp = Compare(sub_df_1, sub_df_2, join_columns=keys, abs_tol=abs_tol, rel_tol=rel_tol,
                           df1_name='expected', df2_name='calculated')

            if comp.matches():
                logger.debug('The columns, %s, in the files match.', str(names))
            else:
                logger.warning('The columns, %s, in the files do not match.', str(names))
                is_match = False
                logger.warning(comp.report())

    # Get the remaining columns that have not been compared.
    rem_cols_1 = [col for col in df_1.columns if col not in cols_compared]
    rem_cols_2 = [col for col in df_2.columns if col not in cols_compared]
    sub_df_1 = df_1[rem_cols_1].copy(deep=True)
    sub_df_2 = df_2[rem_cols_2].copy(deep=True)
    logger.debug('The remaining columns in the first file are: %s.', str(rem_cols_1))
    logger.debug('The remaining columns in the second file are: %s.', str(rem_cols_2))

    comp = Compare(sub_df_1, sub_df_2, join_columns=keys, df1_name='expected', df2_name='calculated')

    if comp.all_columns_match() and comp.matches():
        logger.debug('The remaining columns in the files match.', )
    else:
        logger.warning('The remaining columns in the files do not match:')
        is_match = False
        logger.warning(comp.report())

    logger.debug('%s: Finished comparing file %s against %s using configuration: %s.', name, file_1, file_2, is_match)

    return is_match


def compare_files_direct(name, file_1, file_2):
    # Check that the contents of the two files are identical.

    logger = logging.getLogger(__name__)

    logger.debug('%s: Comparing file %s directly against %s', name, file_1, file_2)

    with open(file_1, 'r') as f1, open(file_2, 'r') as f2:
        s1 = f1.readlines()
        s2 = f2.readlines()
        s1.sort() 
        s2.sort()
        diff = difflib.unified_diff(s1, s2, fromfile=file_1, tofile=file_2)
        match = True
        for line in diff:
            match = False
            logger.warning(line.rstrip('\n'))

    return match


if __name__ == "__main__":
    # Parse input parameters
    parser = argparse.ArgumentParser(description='Compare two input files')
    parser.add_argument('--file_1', help='First file in comparison', required=True)
    parser.add_argument('--file_2', help='Second file in comparison', required=True)
    parser.add_argument('--config', help='Path to comparison configuration file', default='comparison_config.json')
    args = parser.parse_args()

    # Read in the comparison configuration
    with open(args.config, 'r') as f:
        comparison_config = json.load(f)

    main_logger = logging.getLogger(__name__)
    main_logger.info('Start comparison of files.')
    main_result = compare_files(args.file_1, args.file_2, "test", comparison_config)
    main_logger.info('Finished comparison of files: %s.', main_result)
