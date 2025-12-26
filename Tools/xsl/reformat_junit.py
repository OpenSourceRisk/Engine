import xml.etree.ElementTree as ET
import os
from typing import Dict, List
import argparse


class TestCaseResult:
    def __init__(self, name, classname, time_sec, status,
                 message = "", file = "", line = ""):
        self.name = name
        self.classname = classname
        self.time_sec = time_sec
        self.status = status  
        self.message = message
        self.file = file
        self.line = line

def extract_suites(XMLFile):
    tree = ET.parse(XMLFile)
    root = tree.getroot()
    final_suites = {}

    # need a recursive function of sorts to be able to detect all the nested testsuites
    def get_suite(suite, suite_list, top_name):
        for child in suite:
            if child.tag == 'TestSuite':
                child_name = child.attrib.get('name', '')
                subsuite = top_name if top_name else child_name
                get_suite(child, suite_list + [child_name], subsuite)
            elif child.tag == 'TestCase':
                case_name = child.attrib.get('name', '')
                case_file = child.attrib.get('file', '')
                case_line = child.attrib.get('line', '')

                time_elem = 0
                sub_element = None
                for sub in child:
                    if sub.tag == 'TestingTime':
                        if type(sub) == ET.Element and sub.text != "":
                            time_elem = int(sub.text.strip())
                        else:
                            time_elem = 0
                    elif sub.tag == 'Exception' or sub.tag == 'Error':
                        sub_element = sub

                time_sec = time_elem / 1_000_000.0 
                classname = '.'.join(suite_list) if suite_list else top_name or 'root'

                status = 'passed'
                message = ''
                if sub_element is not None:
                    status = 'error'
                    msg_parts = []
                    msg_text = sub_element.text.strip()
                    if msg_text:
                        msg_parts.append(msg_text)
                    for sub in sub_element:
                        if sub.tag == 'LastCheckpoint':
                            lc_file = sub.attrib.get('file', '')
                            lc_line = sub.attrib.get('line', '')
                            lc_text = sub.text.strip() if sub.text != None else ""
                            detail = f"LastCheckpoint {lc_file}:{lc_line} {lc_text}".strip()
                            msg_parts.append(detail)
                    message = '\n'.join([p for p in msg_parts if p])

                result = TestCaseResult(case_name, classname, time_sec, status,
                                        message, case_file, case_line)
                top_key = top_name or (suite.attrib.get('name', '') or 'root')
                final_suites.setdefault(top_key, []).append(result)

    for testsuites in root.findall('TestSuite'):
        get_suite(testsuites, [testsuites.attrib.get('name', '')], testsuites.attrib.get('name', ''))

    return final_suites

def writeJUnit(path_out, suites):
    testsuites_el = ET.Element('testsuites')
    total_tests = 0
    total_failures = 0
    total_errors = 0
    total_time = 0.0

    for top_name, cases in suites.items():
        tests = len(cases)
        failures = sum(1 for c in cases if c.status == 'failure')
        errors = sum(1 for c in cases if c.status == 'error')
        time_sum = sum(c.time_sec for c in cases)
        total_tests += tests
        total_failures += failures
        total_failures += errors
        total_errors += errors
        total_time += time_sum

        ts_el = ET.SubElement(testsuites_el, 'testsuite', {
            'name': top_name,
            'tests': str(tests),
            'failures': str(failures),
            'errors': str(errors),
            'time': f"{time_sum:.6f}",
        })
        for c in cases:
            tc_attrs = {
                'name': c.name,
                'classname': c.classname,
                'time': f"{c.time_sec:.6f}",
            }
            if c.file:
                tc_attrs['file'] = c.file
            if c.line:
                tc_attrs['line'] = str(c.line)
            tc_el = ET.SubElement(ts_el, 'testcase', tc_attrs)
            if c.status in ('failure', 'error'):
                err_tag = 'error' if c.status == 'error' else 'failure'
                err_el = ET.SubElement(tc_el, err_tag, {
                    'type': 'Error',
                    'message': c.message[:8000] if c.message else ''
                })
                if c.message:
                    err_el.text = c.message

    testsuites_el.attrib.update({
        'tests': str(total_tests),
        'failures': str(total_failures),
        'errors': str(total_errors),
        'time': f"{total_time:.6f}",
    })

    tree_out = ET.ElementTree(testsuites_el)
    ET.indent(tree_out, space="  ", level=0)
    tree_out.write(path_out, encoding='utf-8', xml_declaration=True)

def parseXML(input_file):
    suites = extract_suites(input_file)
    base = os.path.splitext(os.path.basename(input_file))[0]
    parts = base.split("_")
    if len(parts) == 3:
        file_name = parts[1] + "_" + parts[2]
    else:
        file_name = parts[1]
    out_path = f"{file_name}_junit.xml"
    writeJUnit(out_path, suites)
    print(f" reformatted {input_file} to {out_path}")

if __name__ == "__main__":
    argparser = argparse.ArgumentParser(description='Convert to Junit')
    argparser.add_argument('--file', help='The file to convert')
    main_args = argparser.parse_args()
    input_file = main_args.file
    
    if os.path.exists(input_file):
        parseXML(input_file)
