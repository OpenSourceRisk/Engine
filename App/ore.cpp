/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of ORE, a free-software/open-source library
 for transparent pricing and risk analysis - http://opensourcerisk.org

 ORE is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program.
 The license is also available online at <http://opensourcerisk.org>

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

#ifdef BOOST_MSVC
// disable warning C4503: '__LINE__Var': decorated name length exceeded, name was truncated
// This pragma statement needs to be at the top of the file - lower and it will not work:
// http://stackoverflow.com/questions/9673504/is-it-possible-to-disable-compiler-warning-c4503
// http://boost.2283326.n4.nabble.com/General-Warnings-and-pragmas-in-MSVC-td2587449.html
#pragma warning(disable : 4503)
#endif


#include <orea/app/oreapp.hpp>

#include <orea/app/initbuilders.hpp>

#include <qle/version.hpp>
#include <qle/gitversion.hpp>

#include <boost/program_options.hpp>

#include <iostream>

namespace po = boost::program_options;

#if !defined(BOOST_ALL_NO_LIB) && defined(BOOST_MSVC)
#include <orea/auto_link.hpp>
#include <ored/auto_link.hpp>
#include <ql/auto_link.hpp>
#include <qle/auto_link.hpp>
// Find the name of the correct boost library with which to link.
#define BOOST_LIB_NAME boost_serialization
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_date_time
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_system
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_program_options
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_timer
#include <boost/config/auto_link.hpp>
#define BOOST_LIB_NAME boost_chrono
#include <boost/config/auto_link.hpp>
#endif

using namespace std;
using namespace ore::data;
using namespace ore::analytics;

int main(int argc, char** argv) {

    if (argc == 2 && (string(argv[1]) == "-v" || string(argv[1]) == "--version")) {
        cout << "ORE version " << OPEN_SOURCE_RISK_VERSION << endl;
        exit(0);
    }

    if (argc == 2 && (string(argv[1]) == "-h" || string(argv[1]) == "--hash")) {
        #ifdef GIT_HASH
        cout << "Git hash " << GIT_HASH << endl;
        #endif
        exit(0);
    }

    // Parse command line arguments. The following form is supported:
    //   ore --input path/to/ore.xml [--output OutputDir]
    // In addition, any other --<name> <value> option is applied as an override
    // to the "setup" group of the parameters, e.g. --logMask 255, --marketDataFile market.txt.
    string inputFile;
    string outputPath;

    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "Produce help message")
        ("input,i", po::value<string>(&inputFile), "Input file (path to ore.xml)")
        ("output,o", po::value<string>(&outputPath), "Output directory, overrides setup/outputPath");

    po::variables_map vm;
    std::vector<std::string> unrecognized;
    try {
        po::parsed_options parsed = po::command_line_parser(argc, argv)
                                        .options(desc)
                                        .allow_unregistered()
                                        .run();
        po::store(parsed, vm);
        po::notify(vm);
        unrecognized = po::collect_unrecognized(parsed.options, po::include_positional);
    } catch (const std::exception& e) {
        cout << "error parsing command line: " << e.what() << endl << endl << desc << endl;
        return -1;
    }

    if (vm.count("help")) {
        cout << "usage: ORE path/to/ore.xml" << endl
             << "       ORE --input path/to/ore.xml [--output OutputDir] [--<setupParam> <value> ...]" << endl
             << endl
             << desc << endl;
        return 0;
    }

    std::map<std::string, std::string> setupOverrides;
    for (std::size_t i = 0; i < unrecognized.size(); ++i) {
        const std::string& token = unrecognized[i];
        if (token.size() > 2 && token.substr(0, 2) == "--") {
            std::string key = token.substr(2);
            std::string value;
            // Support both "--name value" and "--name=value".
            auto eq = key.find('=');
            if (eq != std::string::npos) {
                value = key.substr(eq + 1);
                key = key.substr(0, eq);
            } else if (i + 1 < unrecognized.size() && unrecognized[i + 1].substr(0, 2) != "--") {
                value = unrecognized[++i];
            }
            if (key.empty()) {
                cout << "error parsing command line: empty option name" << endl << endl << desc << endl;
                return -1;
            }
            setupOverrides[key] = value;
        } else if (inputFile.empty()) {
            // A single bare token is treated as the input file (backward compatible positional form).
            inputFile = token;
        } else {
            cout << "error parsing command line: unexpected argument '" << token << "'" << endl << endl
                 << desc << endl;
            return -1;
        }
    }

    if (inputFile.empty()) {
        cout << endl
             << "usage: ORE path/to/ore.xml" << endl
             << "       ORE --input path/to/ore.xml [--output OutputDir] [--<setupParam> <value> ...]" << endl
             << endl
             << desc << endl;
        return -1;
    }

    ore::analytics::initBuilders();

    try {
        auto params = QuantLib::ext::make_shared<Parameters>();
        params->fromFile(inputFile);
        if (!outputPath.empty())
            params->set("setup", "outputPath", outputPath);
        for (const auto& kv : setupOverrides) {
            cout << "overriding setup/" << kv.first << " = " << kv.second << endl;
            params->set("setup", kv.first, kv.second);
        }
        OREApp ore(params, true);
        ore.run();
        return 0;
    } catch (const exception& e) {
        cout << endl << "an error occurred: " << e.what() << endl;
        return -1;
    }
}
