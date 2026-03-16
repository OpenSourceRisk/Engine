import logging
import os
import sys
import shutil
import argparse
import collections
collections.Callable = collections.abc.Callable
import nose
import unittest
import urllib
import json
import time
import re
import requests
from requests.compat import urljoin
from requests.exceptions import HTTPError
from pathlib import Path
import xml.etree.ElementTree as ET
script_dir = Path(__file__).parents[0]
sys.path.append(os.path.join(script_dir, '../../'))
from Tools.PythonTools.setup_logging import setup_logging  # noqa

# Unit test class
class TestExamples(unittest.TestCase):
    def setUp(self):
        self.logger = logging.getLogger(__name__)

    def configureUri(self, input_dir, path):
        ignored = ["scenariodump.csv", "cube.csv.gz", "scenariodata.csv.gz"]
        if str(path) in ignored:
            return path
        if re.match(r'^\.{0,2}/.*', str(path)) or '.' in str(path):
            head_tail = os.path.split(path)
            if len(head_tail[0]) == 0:
                absolute_path = os.path.join(input_dir, head_tail[1])
            else:
                absolute_path = os.path.abspath(os.path.join(input_dir, path))
            partial_encode = urllib.parse.quote(absolute_path, safe='')
            full_encode = urllib.parse.quote(partial_encode, safe='')
            return '/'.join(["http://127.0.0.1:5000/file" , full_encode])
        else:
            return path
        
    def checkParam(self, value):
        fileToUri = {"marketDataFile": "marketData",
        "fixingDataFile": "fixingData",
        "conventionsFile" : "conventionsUri",
        "curveConfigFile": "curveConfigUri",
        "marketConfigFile": "marketConfigUri",
        "pricingEnginesFile": "pricingEnginesUri",
        "portfolioFile": "portfolioUri",
        "simulationConfigFile":"simulationConfigUri",
        "csaFile":"csaUri",
        "iborFallbackConfigFile":"iborFallbackConfigUri",
        "referenceDataFile":"referenceDataUri",
        "sensitivityConfigFile":"sensitivityConfigUri",
        "stressConfigFile":"stressConfigUri"}

        if value in fileToUri:
            return fileToUri[value]
        else:
            return value


    def prepare_payload(self, input_dir, output_dir, tests_base_name):
        logger = logging.getLogger(__name__)

        logger.info('Start preparing pricer body.')

        body = {
            "setup": {},
            "markets": {},
            "analytics": {}
        }

        ignore = ["outputFileName", "rawCubeOutputFile", "netCubeOutputFile", "quantile", "marginalAllocationLimit", "logFile", "outputSensitivityThreshold", "outputThreshold", "outputFile", "sensitivityOutputFile", "scenarioOutputFile"]

        orexml_path = os.path.join(input_dir, 'ore.xml')
        if os.path.isfile(orexml_path):
            tree = ET.parse(orexml_path)
            root = tree.getroot()
            setupNode = root.find('Setup')
            for child in setupNode.findall('Parameter'):
                param = self.checkParam(child.get('name'))
                body["setup"][param] = child.text if str(param) in ignore else self.configureUri(input_dir, child.text)
            if root.find('Markets') is not None:
                marketNode = root.find('Markets')
                for child in marketNode.findall('Parameter'):
                    param = self.checkParam(child.get('name'))
                    body["markets"][param] = child.text if str(param) in ignore else self.configureUri(input_dir, child.text)
            if root.find('Analytics') is not None:
                analyticsNode = root.find('Analytics')
                for analytic in analyticsNode.findall('Analytic'):
                    analyticType = analytic.get('type')
                    body["analytics"][analyticType] = {}
                    for child in analytic.findall('Parameter'):
                        param = self.checkParam(child.get('name'))
                        body["analytics"][analyticType][param] = child.text if str(param) in ignore else self.configureUri(input_dir, child.text)
            
            partial_encode = urllib.parse.quote( os.path.abspath(output_dir), safe='')
            full_encode = urllib.parse.quote(partial_encode, safe='')
            report_dir = '/'.join(["http://127.0.0.1:5000/report" , full_encode])

            body["setup"]["resultsUri"] = report_dir
            response_file_name = 'payload.json'
            with open(os.path.join(output_dir, response_file_name), 'w') as json_file:
                json.dump(body, json_file)
            
            return body
                    
    def runexample(self, name):
        self.logger.info('{}: run {}'.format(self._testMethodName, name))
        completed = False
        current_dir = os.path.abspath('../')
        input_dir = os.path.join(current_dir, name, 'Input')
        if not os.path.isdir(input_dir):
            return
        if not os.path.isfile(os.path.join(input_dir, 'ore.xml')):
            return
        headers = {'Accept': 'application/json', 'Content-Type': 'application/json'}
        api_uri = "http://127.0.0.1:5001/api/analytics"
        host_results_path = os.path.join(current_dir, name, 'JsonOutput')
        if os.path.isdir(host_results_path):
            shutil.rmtree(host_results_path)
            self.logger.info('Removing output directory before test: %s', host_results_path)
        if not os.path.exists(host_results_path):
            os.makedirs(host_results_path)

        body = self.prepare_payload(input_dir, host_results_path, name)
        retries = 2
        i = 0
        while i < retries:
            try:
                response = requests.post(api_uri, data=json.dumps(body), headers=headers)
                logging.info('Request returned with status code %d', response.status_code)
                response.raise_for_status()
                if response.status_code == 200:
                    completed = True
                    break

            except requests.exceptions.Timeout:
                self.logger.warning('Test: ' + str(name) + ' API request ' + str(i) + ' timed out, max retries ' + str(retries))
                i = i + 1
                time.sleep(20)
            except Exception as e:
                self.logger.warning('Test: ' + str(name) + ' API request ' + str(i) + ' failed: ' + str(e) + ', max retries ' + str(retries))
                i = i + 1
                time.sleep(20)
        if not completed:
           raise AssertionError('Test Failed')


def get_list_of_examples():
    examples_dir = os.path.normpath(Path(__file__).resolve().parents[1])
    ignored_dirs = ["__pycache__", "Input", "ORE-Python", "Academy", "CreditRisk"]
    return sorted([e for e in os.listdir(examples_dir)
                   if os.path.isdir(os.path.join(examples_dir, e)) and e not in ignored_dirs])

def add_utest(name):
    def do_run_test(self):
        self.runexample(name)

    return do_run_test

def create_all_utests():
    for name in get_list_of_examples():
        testable_name = 'test_{0}'.format(name)
        testable = add_utest(name)
        testable.__name__ = testable_name
        class_name = 'Test_{0}'.format(name)
        globals()[class_name] = type(class_name, (TestExamples,), {testable_name: testable})


setup_logging()

create_all_utests()

if __name__ == '__main__':
    nose.runmodule(name='__main__')
