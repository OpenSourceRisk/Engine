"""
 Copyright (C) 2026 Quaternion Risk Management Ltd
 All rights reserved.
"""

from ORE import *
import unittest


class ConfigurationBindingsTest(unittest.TestCase):

    def test_bootstrap_config_roundtrip(self):
        cfg = BootstrapConfig(1.0e-10, 1.0e-10, False, 4, 2.5, 1.5, 8, False, 0.0)
        xml = cfg.toXMLString()
        cfg2 = BootstrapConfig()
        cfg2.fromXMLString(xml)
        self.assertTrue(len(cfg2.toXMLString()) > 0)

    def test_security_config_api_exposed(self):
        self.assertTrue(hasattr(SecurityConfig, "fromXML"))
        self.assertTrue(hasattr(SecurityConfig, "toXML"))


if __name__ == '__main__':
    print('testing ORE configuration bindings')
    unittest.main()
