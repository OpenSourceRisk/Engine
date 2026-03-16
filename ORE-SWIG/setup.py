# -*- coding: iso-8859-1 -*-
"""
 Copyright (C) 2018 Quaternion Risk Management Ltd
 All rights reserved.
"""

import os, sys, math, codecs, shutil, platform
if sys.version_info < (3,10):
    from distutils.cmd import Command
    from distutils.command.build_ext import build_ext
    from distutils.command.build import build
    from distutils.ccompiler import get_default_compiler
    from distutils.core import setup, Extension
    from distutils import sysconfig
else:
    from setuptools import Command
    from setuptools.command.build_ext import build_ext
    from setuptools.command.build import build
    from setuptools._distutils.ccompiler import get_default_compiler
    from setuptools import setup, Extension

class test(Command):
    # Original version of this class posted
    # by Berthold Hoellmann to distutils-sig@python.org
    description = "test the distribution prior to install"

    user_options = [
        ('test-dir=', None,
         "directory that contains the test definitions"),
        ]

    def initialize_options(self):
        self.build_base = 'build'
        self.test_dir = 'test'

    def finalize_options(self):
        build = self.get_finalized_command('build')
        self.build_purelib = build.build_purelib
        self.build_platlib = build.build_platlib

    def run(self):
        # Testing depends on the module having been built
        self.run_command('build')

        # extend sys.path
        old_path = sys.path[:]
        sys.path.insert(0, self.build_purelib)
        sys.path.insert(0, self.build_platlib)
        sys.path.insert(0, self.test_dir)

        # import and run test-suite
        module = __import__('OREAnalyticsTestSuite', globals(), locals(), [''])
        module.test()

        # restore sys.path
        sys.path = old_path[:]

class my_wrap(Command):
    description = "generate Python wrappers"
    user_options = []
    def initialize_options(self): pass
    def finalize_options(self): pass
    def run(self):
        print('Generating Python bindings for ORE...')
        swig_version = os.popen("swig -version").read().split()[2]
        major_swig_version = swig_version[0]
        if major_swig_version < '3':
           print('Warning: You have SWIG {} installed, but at least SWIG 3.0.1'
                 ' is recommended for MacOS/Linux and SWIG 4.3.0 for Windows. \nSome features may not work.'
                 .format(swig_version))
        ql_swig_dir = os.path.join("QuantLib-SWIG","SWIG")
        qle_swig_dir = os.path.join("QuantExt-SWIG","SWIG")
        oredata_swig_dir = os.path.join("OREData-SWIG","SWIG")
        orea_swig_dir = os.path.join("OREAnalytics-SWIG","SWIG")
        os.system('swig -python -c++ ' +
                  '-I%s ' % ql_swig_dir +
                  '-I%s ' % qle_swig_dir +
                  '-I%s ' % oredata_swig_dir +
                  '-I%s ' % orea_swig_dir +
                  '-o oreanalytics_wrap.cpp ' +
                  os.path.join("OREAnalytics-SWIG","SWIG","oreanalytics.i"))

class my_build(build):
    user_options = build.user_options + [
        ('static', None,
         "link against static CRT libraries on Windows")
    ]
    boolean_options = build.boolean_options + ['static']
    def initialize_options(self):
        build.initialize_options(self)
        self.static = None
    def finalize_options(self):
        build.finalize_options(self)

class my_build_ext(build_ext):
    user_options = build_ext.user_options + [
        ('static', None,
         "link against static CRT libraries on Windows")
    ]
    boolean_options = build.boolean_options + ['static']
    def initialize_options(self):
        build_ext.initialize_options(self)
        self.static = None
    def get_var(self, v):
        if v in os.environ:
            return os.getenv(v)
        else:
            raise Exception("Environment variable {} not set".format(v))
    def validate_path(self, p):
        if os.path.exists(p):
            return p
        else:
            raise Exception("Invalid path: {}".format(p))

    def build_extension(self, ext):
        """Override to use pre-built object files when available.

        The precompile.sh script (run during CIBW_BEFORE_ALL) compiles
        oreanalytics_wrap.cpp for each Python version in parallel and stores
        the .o files under .prebuilt/<platform>-<pytag>/.  When this directory
        is found we copy the .o into the build temp directory so that
        setuptools only needs to perform the (fast) link step.
        """
        prebuilt_dir = os.environ.get('ORE_PREBUILT_DIR', '')
        if prebuilt_dir and os.path.isdir(prebuilt_dir):
            py_tag = f"cpython-{sys.version_info.major}{sys.version_info.minor}"
            machine = platform.machine()   # e.g. x86_64, aarch64
            tag = f"linux-{machine}-{py_tag}"
            prebuilt_obj = os.path.join(prebuilt_dir, tag, "oreanalytics_wrap.o")
            if os.path.isfile(prebuilt_obj):
                # Determine where setuptools expects the object file
                build_temp = self.build_temp  # e.g. build/temp.linux-x86_64-cpython-312
                os.makedirs(build_temp, exist_ok=True)
                dest = os.path.join(build_temp, "oreanalytics_wrap.o")
                print(f"Using pre-built object: {prebuilt_obj} -> {dest}")
                shutil.copy2(prebuilt_obj, dest)

                # Now run only the link step.
                # We need to gather the objects list and call link_shared_object
                # via the standard path but with sources already "compiled".
                # The simplest way is to replace the compiler's compile method
                # with a no-op that returns the existing objects, then call
                # the parent build_extension which will invoke compile (no-op)
                # and then link.
                original_compile = self.compiler.compile
                def _skip_compile(sources, output_dir=None, **kwargs):
                    # Return the list of object files that already exist
                    return [dest]
                self.compiler.compile = _skip_compile
                try:
                    build_ext.build_extension(self, ext)
                finally:
                    self.compiler.compile = original_compile
                return

        # No pre-built objects; fall through to normal compilation
        build_ext.build_extension(self, ext)

    def finalize_options(self):
        build_ext.finalize_options(self)
        self.set_undefined_options('build', ('static','static'))

        self.include_dirs = self.include_dirs or []
        self.library_dirs = self.library_dirs or []
        self.define = self.define or []
        self.libraries = self.libraries or []

        extra_compile_args = []
        extra_link_args = []

        compiler = self.compiler or get_default_compiler()

        if compiler == 'msvc':
            BOOST_DIR = self.get_var('BOOST')
            try:
                BOOST_LIB = self.get_var('BOOST_LIB64')
            except:
                BOOST_LIB = self.get_var('BOOST_LIB32')
            ORE_DIR = os.path.join('..')

            # ADD INCLUDE DIRECTORIES
            self.include_dirs.append(self.validate_path(BOOST_DIR))
            #self.include_dirs.append(self.validate_path(os.path.join(ORE_DIR, 'build', 'QuantLib')))
            self.include_dirs.append(self.validate_path(os.path.join(ORE_DIR, 'QuantLib')))
            self.include_dirs.append(self.validate_path(os.path.join(ORE_DIR, 'QuantExt')))
            self.include_dirs.append(self.validate_path(os.path.join(ORE_DIR, 'OREData')))
            self.include_dirs.append(self.validate_path(os.path.join(ORE_DIR, 'OREAnalytics')))

            # ADD LIBRARY DIRECTORIES

            target = "Release"

            if self.debug:
                target = "Debug"

            self.library_dirs.append(self.validate_path(BOOST_LIB))
            
            ORE_BUILD_DIR = os.path.join(ORE_DIR,"build")
            
            # for internal use
            if(os.path.exists(os.path.join("..","..","build"))):
                ORE_BUILD_DIR = os.path.join("..","..","build","ore")
                print("ORE BUILD DIR: ", ORE_BUILD_DIR)

            try:
                self.include_dirs.append(self.validate_path(os.path.join(ORE_BUILD_DIR, "QuantLib")))
                self.library_dirs.append(self.validate_path(os.path.join(ORE_BUILD_DIR, 'QuantLib', 'ql', target)))
                self.library_dirs.append(self.validate_path(os.path.join(ORE_BUILD_DIR, 'QuantExt', 'qle', target)))
                self.library_dirs.append(self.validate_path(os.path.join(ORE_BUILD_DIR, 'OREData', 'ored', target)))
                self.library_dirs.append(self.validate_path(os.path.join(ORE_BUILD_DIR, 'OREAnalytics', 'orea', target)))
            except:
                self.include_dirs.append(self.validate_path(os.path.join(ORE_BUILD_DIR, "QuantLib")))
                self.library_dirs.append(self.validate_path(os.path.join(ORE_BUILD_DIR, 'QuantLib', 'ql')))
                self.library_dirs.append(self.validate_path(os.path.join(ORE_BUILD_DIR, 'QuantExt', 'qle')))
                self.library_dirs.append(self.validate_path(os.path.join(ORE_BUILD_DIR, 'OREData', 'ored')))
                self.library_dirs.append(self.validate_path(os.path.join(ORE_BUILD_DIR, 'OREAnalytics', 'orea')))

            #if 'INCLUDE' in os.environ:
            #    dirs = [dir for dir in os.environ['INCLUDE'].split(';')]
            #    self.include_dirs += [ d for d in dirs if d.strip() ]
            #if 'LIB' in os.environ:
            #    dirs = [dir for dir in os.environ['LIB'].split(';')]
            #    self.library_dirs += [ d for d in dirs if d.strip() ]
            dbit = round(math.log(sys.maxsize, 2) + 1)
            if dbit == 64:
                machinetype = '/machine:x64'
            else:
                machinetype = '/machine:x86'
            self.define += [('__WIN32__', None), ('WIN32', None),
                            ('NDEBUG', None), ('_WINDOWS', None),
                            ('NOMINMAX', None)]
            # ORE and QuantLib specific flags
            self.define += [('QL_ENABLE_SESSIONS', None), ('QL_USE_STD_ANY', None), ('QL_FASTER_LAZY_OBJECTS', None), ('QL_USE_STD_OPTIONAL', None)]
            if 'ORE_USE_ZLIB' in os.environ:
                self.define += [('ORE_USE_ZLIB', None)]
            extra_compile_args = ['/GR', '/FD', '/Zm250', '/EHsc', '/bigobj', '/std:c++20', '/wd4996' ]
            extra_link_args = ['/subsystem:windows', machinetype]
            self.libraries = [ 'advapi32' ]

            if self.debug:
                if self.static or 'ORE_STATIC_RUNTIME' in os.environ:
                    extra_compile_args.append('/MTd')
                else:
                    extra_compile_args.append('/MDd')
            else:
                if self.static or 'ORE_STATIC_RUNTIME' in os.environ:
                    extra_compile_args.append('/MT')
                else:
                    extra_compile_args.append('/MD')

        elif compiler == 'unix':
            ql_compile_args = \
                os.popen('./oreanalytics-config --cflags').read()[:-1].split()
            ql_link_args = \
                os.popen('./oreanalytics-config --libs').read()[:-1].split()

            self.define += [ (arg[2:],None) for arg in ql_compile_args
                             if arg.startswith('-D') ]
            self.define += [("NDEBUG", None)]
            self.include_dirs += [ arg[2:] for arg in ql_compile_args
                                   if arg.startswith('-I') ]
            self.library_dirs += [ arg[2:] for arg in ql_link_args
                                   if arg.startswith('-L') ]
            self.libraries += [ arg[2:] for arg in ql_link_args
                                if arg.startswith('-l') ]

            extra_compile_args = [ arg for arg in ql_compile_args
                                   if not arg.startswith('-D')
                                   if not arg.startswith('-I') ] \
                                   + [ '-Wno-unused' ] + ['-std=c++20']
            if 'CXXFLAGS' in os.environ:
                extra_compile_args += os.environ['CXXFLAGS'].split()

            extra_link_args = [ arg for arg in ql_link_args
                                if not arg.startswith('-L')
                                if not arg.startswith('-l') ]
            if 'LDFLAGS' in os.environ:
                extra_link_args += os.environ['LDFLAGS'].split()

        else:
            pass

        for ext in self.extensions:
            ext.extra_compile_args = ext.extra_compile_args or []
            ext.extra_compile_args += extra_compile_args

            ext.extra_link_args = ext.extra_link_args or []
            ext.extra_link_args += extra_link_args

#if os.name == 'posix':
#    # changes the compiler from gcc to g++
#    save_init_posix = sysconfig._init_posix
#    def my_init_posix():
#        save_init_posix()
#        g = sysconfig._config_vars
#        if 'CXX' in os.environ:
#            g['CC'] = os.environ['CXX']
#        else:
#            g['CC'] = 'g++'
#        if sys.platform.startswith("darwin"):
#            g['LDSHARED'] = g['CC'] + \
#                            ' -bundle -flat_namespace -undefined suppress'
#        else:
#            g['LDSHARED'] = g['CC'] + ' -shared'
#    sysconfig._init_posix = my_init_posix

datafiles  = []

classifiers = [
    'Development Status :: 5 - Production/Stable',
    'Environment :: Console',
    'Intended Audience :: Developers',
    'Intended Audience :: End Users/Desktop',
    'License :: OSI Approved :: BSD License',
    'Natural Language :: English',
    'Operating System :: Microsoft :: Windows',
    'Operating System :: POSIX',
    'Operating System :: Unix',
    'Operating System :: MacOS',
    'Programming Language :: C++',
    'Programming Language :: Python',
    'Topic :: Scientific/Engineering',
]

setup(name             = "open-source-risk-engine",
      version          = "1.8.15.0",
      description      = "Python bindings for the OREAnalytics library",
      long_description = """
OREAnalytics (http://opensourcerisk.org/) is a C++ library for financial quantitative
analysts and developers, aimed at providing a comprehensive software
framework for quantitative finance.
      """,
      author           = "Quaternion Risk Management",
      author_email     = "info@quaternion.com",
      url              = "http://opensourcerisk.org/",
      license          = codecs.open('LICENSE.txt','r+',
                                     encoding='utf8').read(),
      classifiers      = classifiers,
      py_modules       = ['__init__','ORE'],
      ext_modules      = [Extension("_ORE",
                                    ["oreanalytics_wrap.cpp"])
                         ],
      data_files       = datafiles,
      cmdclass         = {'test': test,
                          'wrap': my_wrap,
                          'build': my_build,
                          'build_ext': my_build_ext
                          }
      )
