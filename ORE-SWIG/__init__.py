# -*- coding: iso-8859-1 -*-
"""
 Copyright (C) 2018 Quaternion Risk Management Ltd
 All rights reserved.
"""

import sys
if sys.version_info.major >= 3:
    from .ORE import *
    from .ORE import _ORE
else:
    from ORE import *
    from ORE import _ORE
del sys

__author__ = 'Quaternion Risk Management'
__email__ = 'ino@quaternion.com'

if hasattr(_ORE,'__version__'):
    __version__ = _ORE.__version__
elif hasattr(_ORE.cvar,'__version__'):
    __version__ = _ORE.cvar.__version__
else:
    print('Could not find __version__ attribute')

if hasattr(_ORE,'__hexversion__'):
    __hexversion__ = _ORE.__hexversion__
elif hasattr(_ORE.cvar,'__hexversion__'):
    __hexversion__ = _ORE.cvar.__hexversion__
else:
    print('Could not find __hexversion__ attribute')

__license__ = """
COPYRIGHT AND PERMISSION NOTICE

Copyright (c) 2018 Quaternion Risk Management Ltd
All rights reserved.
"""
