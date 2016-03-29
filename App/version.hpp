/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2011 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#pragma once

#include <iostream>

inline std::string oreVersion () {
    return "0.1";
}

inline void printVersion() {
    std::cout << "Open Risk Engine Version " << oreVersion() << std::endl;
    std::cout << "Build Date " << __DATE__ << " " << __TIME__ << std::endl;
    std::cout << "Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd. All Rights Reserved" << std::endl;
    std::cout << "www.quaternion.com" << std::endl;
}

// Checks for "-version" or "--version" on the command line paramters.
// if present, it prints the version and exits with return code 0.
// We do this here, so we can disply the version number even if ENV_FILE is not set
inline void checkVersion(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "-version" ||
            std::string(argv[i]) == "--version") {
            printVersion();
            exit(0);
        }
    }
}
