import logging
import os
import shutil
import sys
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
    
    def runexample(self, name):
        self.logger.info('{}: run {}'.format(self._testMethodName, name))
        completed = False
        current_dir = os.path.abspath('../')
        input_dir = os.path.join(current_dir, name, 'Input')
        if not os.path.isdir(input_dir):
            return
        default_orexml_path = os.path.join(input_dir, 'ore.xml')
        headers = {'Accept': 'application/json', 'Content-Type': 'application/json'}
        if os.path.isfile(default_orexml_path):
            partial_encode = urllib.parse.quote(default_orexml_path, safe='')
            full_encode = urllib.parse.quote(partial_encode, safe='')
            api_uri = '/'.join(["http://127.0.0.1:5001/api/analytics/file" , full_encode])
            host_results_path = os.path.join(script_dir, 'Output', name)
            if not os.path.exists(host_results_path):
                os.makedirs(host_results_path)
            retries = 2
            i = 0
            while i < retries:
                try:
                    response = requests.get(api_uri, headers=headers, )
                    logging.info('Request returned with status code %d', response.status_code)
                    response.raise_for_status()

                    completed = True
                    break
                except requests.exceptions.Timeout:
                    self.logger.warning('API request ' + str(i) + ' timed out, max retries ' + str(retries))
                    i = i + 1
                    time.sleep(20)
                except Exception as e:
                    self.logger.warning('API request ' + str(i) + ' failed: ' + str(e) + ', max retries ' + str(retries))
                    i = i + 1
                    time.sleep(20)
            if not completed:
                raise AssertionError('Test Failed')

        else:
            return
            #raise ValueError('Expected path ' + default_orexml_path + ' to exist.') 

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
