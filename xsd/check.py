import logging
import os
import argparse
from lxml import etree
from lxml.etree import XMLSchemaParseError
from lxml.etree import XMLSyntaxError
import sys
from typing import Iterable, List, Optional

# Global result flag: True means no schema errors encountered; set to False on any schema error
ERROR: bool = True


def remove_temp_files(dir_name:str) -> None:
    """
    This function removes temporary xsd files created during the validation process.
    If no errors occurred (ERROR is True), also remove check.log.
    """

    # Remove check.log only if there were no schema errors
    if ERROR:
        check_log_path = os.path.join(dir_name, 'check.log')
        if os.path.exists(check_log_path):
            os.remove(check_log_path)

def iter_xml_files(root_dir: str):
    for dirpath, _, filenames in os.walk(root_dir):
        for fname in filenames:
            if fname.lower().endswith(".xml"):
                yield os.path.join(dirpath, fname)


def xml_validator(xml_path: str, schema: etree.XMLSchema) -> None:
    """
    Validate a single XML file against a precompiled XML schema.

    :param xml_path: Filesystem path to the XML file.
    :type xml_path: str
    :param schema: Precompiled lxml XMLSchema object.
    :type schema: etree.XMLSchema
    """
    global ERROR

    if "invalid_calendaradjustments" in str(xml_path):
        logging.info(f"Skipping validation for {xml_path} (name contains 'invalid_calendaradjustments')")
        return

    try:
        # Parse the XML (no schema-bound parser creation per file)
        with open(xml_path, 'rb') as xml_file:
            doc = etree.parse(xml_file)
        # Assert validity against the compiled schema
        schema.assertValid(doc)
    except XMLSyntaxError as e:
        ERROR = False
        logging.error(f"XMLSyntaxError in {xml_path}: {str(e)}")
    except etree.DocumentInvalid as e:
        ERROR = False
        logging.error(f"Schema validation failed for {xml_path}: {str(e)}")
    except Exception as e:
        ERROR = False
        logging.error(f"Unexpected error validating {xml_path}: {e}")


def _compile_schema_once(dir_name: str) -> Optional[etree.XMLSchema]:
    """Preprocess XSDs, write temp files under script directory, and compile schema once."""
    global ERROR
    xsd_path = os.path.join(dir_name, 'input.xsd')
    # Compile the schema once
    try:
        if not os.path.isfile(xsd_path):
            ERROR = False
            logging.error(f"Schema file not found at: {xsd_path}")
            return None
        # Parse the XSD file from disk; do not treat the path as XML content
        schema_doc = etree.parse(xsd_path)
        schema = etree.XMLSchema(schema_doc)
        return schema
    except XMLSchemaParseError as e:
        ERROR = False
        logging.error(f"XMLSchemaParseError while compiling schema from '{xsd_path}': {e}")
        return None
    except XMLSyntaxError as e:
        ERROR = False
        logging.error(f"XMLSyntaxError while parsing schema file '{xsd_path}': {e}")
        return None
    except Exception as e:
        ERROR = False
        logging.error(f"Unexpected error compiling schema from '{xsd_path}': {e}")
        return None


def _collect_xmls(paths: Iterable[str]) -> List[str]:
    files: List[str] = []
    for p in paths:
        if os.path.isdir(p):
            files.extend(iter_xml_files(p))
    return files


def _setup_logging(log_level: str, dir_name: str) -> None:
    """Write detailed logs to check.log; only ERRORs to the console."""
    file_level = getattr(logging, log_level.upper(), logging.DEBUG)

    logger = logging.getLogger()
    # Allow file handler to capture everything up to DEBUG
    logger.setLevel(logging.DEBUG)

    # Remove any existing handlers to avoid duplicate logs
    for h in list(logger.handlers):
        logger.removeHandler(h)

    formatter = logging.Formatter('%(asctime)s %(levelname)-8s %(message)s')

    # Console handler (ERROR only)
    ch = logging.StreamHandler(stream=sys.stderr)
    ch.setLevel(logging.ERROR)
    ch.setFormatter(formatter)
    logger.addHandler(ch)

    # File handler writing logs to check.log at configured level
    check_log_path = os.path.join(dir_name, 'check.log')
    fh = logging.FileHandler(check_log_path)
    fh.setLevel(file_level)
    fh.setFormatter(formatter)
    logger.addHandler(fh)


def main(xml_path:str, jobs:int=1) -> None:
    """
    Main function to initiate XML schema validation for configured files.

    It constructs the necessary file paths, iterates over XML files in designated directories,
    and validates each file using the ``xml_validator`` function.

    :returns: None
    :rtype: None
    """
    logging.info("Start Schema Check")
    dir_name = os.path.abspath(os.path.dirname(__file__))

    dict_paths = {
        "Examples": os.path.join(dir_name, '..', 'Examples'),
        "RegressionTest": os.path.join(dir_name, '..', 'RegressionTests'),
        "OREData": os.path.join(dir_name, '..', 'OREData', 'test')
    }

    # Preprocess and compile schema once
    schema = _compile_schema_once(dir_name)
    if schema is None:
        logging.error("Schema compilation failed; aborting.")
        remove_temp_files(dir_name)
        sys.exit(1)

    # If a specific XML file is provided, validate it directly
    if xml_path and os.path.isfile(xml_path):
        logging.info(f"Validate {xml_path}")
        xml_validator(xml_path, schema)
        remove_temp_files(dir_name)
        sys.exit(0)

    # Collect all XML files to validate
    paths_to_scan = list(dict_paths.values())
    xml_files = _collect_xmls(paths_to_scan)
    logging.info(f"Found {len(xml_files)} XML files to validate")

    # Optional parallel validation using threads (safe for IO-bound parsing)
    # Using threads avoids schema pickling problems; lxml releases GIL on I/O and parsing.
    if jobs and jobs > 1:
        try:
            from concurrent.futures import ThreadPoolExecutor, as_completed
            with ThreadPoolExecutor(max_workers=jobs) as executor:
                futures = {executor.submit(xml_validator, xf, schema): xf for xf in xml_files}
                for future in as_completed(futures):
                    # Drain exceptions to ensure they get logged
                    try:
                        future.result()
                    except Exception as e:
                        # Any exception surfaced here means at least one validation failed
                        global ERROR
                        ERROR = False
                        logging.error(f"Validation task failed: {e}")
        except Exception as e:
            logging.warning(f"Parallel validation setup failed ({e}); falling back to sequential.")
            for xf in xml_files:
                xml_validator(xf, schema)
    else:
        for xf in xml_files:
            xml_validator(xf, schema)

    # Remove the temporary xsd files and conditionally remove check.log
    remove_temp_files(dir_name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Check if xml file is valid')
    # Default file logging level to DEBUG for comprehensive trace in check.log
    parser.add_argument('--log_level', type=str, default="INFO")
    parser.add_argument('--xml_path', type=str, default="", help="Direct XML path")
    parser.add_argument('--jobs', type=int, default=2, help="Number of parallel validation workers (threads)")
    args = parser.parse_args()

    # Prepare logging (console ERROR-only, file full details) before running main
    script_dir = os.path.abspath(os.path.dirname(__file__))
    _setup_logging(args.log_level, script_dir)

    main(xml_path=args.xml_path, jobs=args.jobs)

    if not ERROR:
        sys.exit(-1)
