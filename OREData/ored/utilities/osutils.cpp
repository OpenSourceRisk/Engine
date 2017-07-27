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

#include <ored/utilities/osutils.hpp>
#include <ored/version.hpp>

#include <iomanip>
#include <fstream>
#include <sstream>

#include <ql/version.hpp>
#include <boost/version.hpp>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <psapi.h>
#include <intrin.h>
#elif __APPLE__
#include <unistd.h>
#include <sys/sysctl.h>
#else
#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h> // getrusage()
#include <sys/utsname.h>  // uname()
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/predicate.hpp>
#endif

using namespace std;

namespace {
string memoryString(unsigned long long m) {
    ostringstream oss;
    oss << fixed << setprecision(2);
    if (m < 1024)
        oss << m << "B";
    else if (m < 1024 * 1024)
        oss << m / (double)1024 << "kB";
    else if (m < 1024 * 1024 * 1024)
        oss << m / (double)(1024 * 1024) << "MB";
    else
        oss << m / (double)(1024 * 1024 * 1024) << "GB";
    return oss.str();
}
}

namespace ore {
namespace data {
namespace os {

string getSystemDetails() {
    ostringstream oss;
    oss << "System Details:" << endl;
    oss << "  OS                     : " << os::getOsName() << endl;
    oss << "  OS Version             : " << os::getOsVersion() << endl;
    oss << "  CPU                    : " << os::getCpuName() << endl;
    oss << "  Cores                  : " << os::getNumberCores() << endl;
    oss << "  Memory (Available)     : " << os::getMemoryRAM() << endl;
    oss << "  Memory (Process)       : " << os::getMemoryUsage() << endl;
    oss << "  Hostname               : " << os::getHostname() << endl;
    oss << "  Username               : " << os::getUsername() << endl;
    oss << "  ORE Version : " << OPEN_SOURCE_RISK_VERSION << endl;
    oss << "  QuantLib Version       : " << QL_VERSION << endl;
    oss << "  Boost Version          : " << BOOST_LIB_VERSION << endl;
    return oss.str();
}

string getMemoryUsage() { return memoryString(getMemoryUsageBytes()); }

// -------------------------------------
// ---- Windows stuff
// -------------------------------------
#if defined(_WIN32) || defined(_WIN64)

string getOsName() {
#ifdef _WIN64
    return "Windows 64-bit";
#else
    return "Windows 32-bit";
#endif
}

// Ignore "warning C4996: 'GetVersionExA': was declared deprecated"
#pragma warning(push)
#pragma warning(disable : 4996)
string getOsVersion() {
    OSVERSIONINFO osvi;

    ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    GetVersionEx(&osvi);

    ostringstream oss;
    oss << osvi.dwMajorVersion << "." << osvi.dwMinorVersion << "." << osvi.dwBuildNumber << " " << osvi.szCSDVersion;
    return oss.str();

// Restore "warning C4996: 'GetVersionExA': was declared deprecated" to previous setting
#pragma warning(pop)
}

string getCpuName() {
    int CPUInfo[4] = {-1};
    char CPUBrandString[0x40];
    __cpuid(CPUInfo, 0x80000000);
    int nExIds = CPUInfo[0];

    memset(CPUBrandString, 0, sizeof(CPUBrandString));

    // Get the information associated with each extended ID.
    for (int i = 0x80000002; i <= nExIds; ++i) {
        __cpuid(CPUInfo, i);
        // Interpret CPU brand string.
        if (i == 0x80000002)
            memcpy(CPUBrandString, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000003)
            memcpy(CPUBrandString + 16, CPUInfo, sizeof(CPUInfo));
        else if (i == 0x80000004)
            memcpy(CPUBrandString + 32, CPUInfo, sizeof(CPUInfo));
    }
    return CPUBrandString;
}

unsigned int getNumberCores() {
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    return sysinfo.dwNumberOfProcessors;
}

string getMemoryRAM() {
    unsigned long long mem = 0;
    if (!GetPhysicallyInstalledSystemMemory(&mem))
        return "?";
    return memoryString(mem * 1024);
}

unsigned long long getMemoryUsageBytes() {
    PROCESS_MEMORY_COUNTERS info;
    if (!GetProcessMemoryInfo(GetCurrentProcess(), &info, sizeof(info)))
        return 0;
    return info.PeakWorkingSetSize;
}

string getUsername() {
    char acUserName[256 + 1];
    DWORD nUserName = sizeof(acUserName);
    if (!GetUserName(acUserName, &nUserName))
        return "?";
    acUserName[nUserName] = '\0';
    return acUserName;
}

string getHostname() {
    char buf[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD buflen = sizeof(buf);
    if (!GetComputerName(buf, &buflen))
        return "?";
    buf[buflen] = '\0';
    return buf;
}

// -------------------------------------
// ---- Generic *nix Stuff
// -------------------------------------
#else

unsigned long long getMemoryUsageBytes() {
    rusage ru;
    getrusage(RUSAGE_SELF, &ru);
    return ru.ru_maxrss;
}

string getUsername() {
    char* login = getlogin();
#ifdef __linux__
    if (!login)
        login = cuserid(NULL);
#endif
    return login;
}

string getHostname() {
    char buf[100];
    size_t buflen = 100;
    gethostname(buf, buflen);
    buf[buflen - 1] = '\0';
    return buf;
}

// -------------------------------------
// ---- Mac OS Stuff
// -------------------------------------
#ifdef __APPLE__

string getOsName() { return "Mac OSX"; }

string getOsVersion() {
    char buf[100];
    size_t buflen = 100;
    sysctlbyname("kern.osversion", &buf, &buflen, NULL, 0);
    return string(buf);
}

string getCpuName() {
    char buf[100];
    size_t buflen = 100;
    sysctlbyname("machdep.cpu.brand_string", &buf, &buflen, NULL, 0);
    return string(buf);
}

unsigned int getNumberCores() {
    // hw.physicalcpu_max
    int64_t ncpus;
    size_t len = sizeof(ncpus);
    sysctlbyname("hw.physicalcpu_max", &ncpus, &len, NULL, 0);
    return ncpus;
}

string getMemoryRAM() {
    int64_t mem;
    size_t len = sizeof(mem);
    sysctlbyname("hw.memsize", &mem, &len, NULL, 0);
    return memoryString(mem);
}

// -------------------------------------
// ---- Linux Stuff
// -------------------------------------

#else

string getOsName() {
#if defined(__unix) || defined(__unix__)
    return "Unix";
#elif __linux__
    return "Linux";
#elif __FreeBSD__
    return "FreeBSD";
#else
    return "Other";
#endif
}

string getOsVersion() {
    struct utsname unameData;
    if (uname(&unameData))
        return "?"; // error
    else
        return unameData.release;
}

static string parseProcFile(const char* filename, const string& nodename) {
    ifstream ifile(filename);
    if (ifile.is_open()) {
        string line;
        while (!ifile.eof()) {
            getline(ifile, line);
            if (boost::starts_with(line, nodename)) {
                string result = line.substr(nodename.size() + 1);
                boost::trim(result);
                if (result.size() > 0 && result.at(0) == ':') {
                    // remove leading :
                    result = result.substr(1);
                    boost::trim(result);
                }
                ifile.close();
                return result;
            }
        }
        ifile.close();
    }
    return "";
}

string getCpuName() { return parseProcFile("/proc/cpuinfo", "model name"); }

unsigned int getNumberCores() {
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    if (n < 0) // error
        return 0;
    else
        return n;
}

string getMemoryRAM() { return parseProcFile("/proc/meminfo", "MemTotal"); }

#endif

#endif

} // end namespace os
} // end namespace data
} // end namespace ore
