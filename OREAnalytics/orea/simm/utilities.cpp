/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
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

#include <orea/simm/utilities.hpp>
#include <orea/simm/simmconfigurationisdav1_0.hpp>
#include <orea/simm/simmconfigurationisdav1_3.hpp>
#include <orea/simm/simmconfigurationisdav1_3_38.hpp>
#include <orea/simm/simmconfigurationisdav2_0.hpp>
#include <orea/simm/simmconfigurationisdav2_1.hpp>
#include <orea/simm/simmconfigurationisdav2_2.hpp>
#include <orea/simm/simmconfigurationisdav2_3.hpp>
#include <orea/simm/simmconfigurationisdav2_3_8.hpp>
#include <orea/simm/simmconfigurationisdav2_5.hpp>
#include <orea/simm/simmconfigurationisdav2_5a.hpp>
#include <orea/simm/simmconfigurationisdav2_6.hpp>
#include <orea/simm/simmconfigurationcalibration.hpp>

#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <ql/math/matrixutilities/symmetricschurdecomposition.hpp>
#include <ql/utilities/null.hpp>

#include <boost/make_shared.hpp>
#include <boost/regex.hpp>

#include <fstream>

using std::map;
using std::set;
using std::string;
using namespace ore::data;

namespace ore {
namespace analytics {

using QuantLib::Null;
using QuantLib::Size;

std::vector<std::string> loadFactorList(const std::string& inputFileName, const char delim) {
    LOG("Load factor list from file " << inputFileName);
    std::ifstream file;
    file.open(inputFileName);
    QL_REQUIRE(file.is_open(), "error opening file " << inputFileName);
    std::vector<string> result;
    try {
        while (!file.eof()) {
            std::string line;
            getline(file, line, delim);
            if (line.size() == 0)
                continue;
            result.push_back(line);
        }
    } catch (std::exception& e) {
        file.close();
        QL_FAIL("error during reading file: " << e.what());
    }

    LOG("Loaded factor list of size " << result.size());
    return result;
} // loadFactorList

std::vector<std::vector<double>> loadScenarios(const std::string& inputFileName, const char delim) {
    LOG("Load scenarios from file " << inputFileName);
    std::ifstream file;
    file.open(inputFileName);
    QL_REQUIRE(file.is_open(), "error opening file " << inputFileName);
    std::vector<std::vector<double>> result;
    try {
        while (!file.eof()) {
            Size currentScenario = Null<Size>(); //, currentFactor;// = Null<Size>();
            std::string line;
            getline(file, line, delim);
            if (line.size() == 0)
                continue;
            std::vector<string> tokens;
            boost::trim(line);
            boost::split(tokens, line, boost::is_any_of(",;\t "), boost::token_compress_off);
            QL_REQUIRE(tokens.size() == 3, "loadScenarios, expected 3 tokens in line: " << line);
            Size tmpSc = parseInteger(tokens[0]);
            // Size tmpFc = parseInteger(tokens[1]);
            if (currentScenario != tmpSc) {
                result.push_back(std::vector<double>());
                currentScenario = tmpSc;
            }
            // currentFactor = tmpFc;
            double move = parseReal(tokens[2]);
            result.back().push_back(move);
        }
    } catch (std::exception& e) {
        file.close();
        QL_FAIL("error during reading file: " << e.what());
    }

    LOG("Loaded " << result.size() << " scenarios, first entry contains " << result.front().size() << " factors");
    return result;
} // loadScenarios

Matrix loadCovarianceMatrix(const std::string& inputFileName, const char delim) {
    LOG("Load covariance matrix from file " << inputFileName);
    std::ifstream file;
    file.open(inputFileName);
    QL_REQUIRE(file.is_open(), "error opening file " << inputFileName);
    std::vector<std::pair<Size, Size>> pos;
    std::vector<double> val;
    Size maxI = 0, maxJ = 0;
    try {
        while (!file.eof()) {
            std::string line;
            getline(file, line, delim);
            if (line.size() == 0)
                continue;
            std::vector<string> tokens;
            boost::trim(line);
            boost::split(tokens, line, boost::is_any_of(",;\t "), boost::token_compress_off);
            QL_REQUIRE(tokens.size() == 3, "loadCovarianceMatrix, expected 3 tokens in line: " << line);
            Size i = parseInteger(tokens[0]);
            Size j = parseInteger(tokens[1]);
            double tmp = parseReal(tokens[2]);
            pos.push_back(std::make_pair(i, j));
            val.push_back(tmp);
            maxI = std::max(maxI, i);
            maxJ = std::max(maxJ, j);
        }
    } catch (std::exception& e) {
        file.close();
        QL_FAIL("error during reading file: " << e.what());
    }

    LOG("Loaded " << val.size() << " data points, dimension of matrix is " << maxI + 1 << "x" << maxJ + 1);
    QL_REQUIRE(maxI == maxJ, "Expected quadratic matrix");

    Matrix result(maxI + 1, maxI + 1);
    for (Size i = 0; i < pos.size(); ++i) {
        result[pos[i].first][pos[i].second] = val[i];
        result[pos[i].second][pos[i].first] = val[i];
    }

    // log the eigenvalues
    QuantLib::SymmetricSchurDecomposition ssd(result);
    for (Size i = 0; i < ssd.eigenvalues().size(); ++i) {
        LOG("Eigenvalue " << i << " =  " << ssd.eigenvalues()[i]);
    }
    return result;
} // load CovarianceMatrix

SimmVersion parseSimmVersion(const string& version) {
    static map<string, SimmVersion> versionMap = {{"1.0", SimmVersion::V1_0},
                                                  {"1.1", SimmVersion::V1_1},
                                                  {"1.2", SimmVersion::V1_2},
                                                  {"1.3", SimmVersion::V1_3},
                                                  {"1.3.38", SimmVersion::V1_3_38},
                                                  {"2.0", SimmVersion::V2_0},
                                                  {"2.1", SimmVersion::V2_1},
                                                  {"2.2", SimmVersion::V2_2},
                                                  {"2.3", SimmVersion::V2_3},
                                                  {"2.3.8", SimmVersion::V2_3_8},
                                                  {"2.5", SimmVersion::V2_5},
                                                  {"2.5A", SimmVersion::V2_5A},
                                                  {"2.5.6", SimmVersion::V2_6},
						                          // alias
                                                  {"2.4", SimmVersion::V2_3_8},
                                                  {"2.6", SimmVersion::V2_6},
                                                  // old names for backwards compatibility
                                                  {"ISDA_V315", SimmVersion::V1_0},
                                                  {"ISDA_V329", SimmVersion::V1_3},
                                                  {"ISDA_V338", SimmVersion::V1_3_38},
                                                  {"ISDA_V344", SimmVersion::V2_0}};

    QL_REQUIRE(versionMap.count(version) > 0,
               "Could not parse SIMM version string " << version << " to a valid version");

    return versionMap.at(version);
}

QuantLib::ext::shared_ptr<SimmConfiguration> buildSimmConfiguration(const string& simmVersion,
                                                            const QuantLib::ext::shared_ptr<SimmBucketMapper>& simmBucketMapper,
                                                            const QuantLib::ext::shared_ptr<SimmCalibrationData>& simmCalibrationData,
                                                            const Size& mporDays) {

    // Check first if the SIMM calibration has the requested simmVersion
    if (simmCalibrationData) {
        const auto& simmCalibration = simmCalibrationData->getBySimmVersion(simmVersion);
        if (simmCalibration) {
            auto simmConfiguration = QuantLib::ext::make_shared<SimmConfigurationCalibration>(simmBucketMapper, simmCalibration, mporDays);
            return simmConfiguration;
        }
    }

    auto version = parseSimmVersion(simmVersion);

    switch (version) {
    case SimmVersion::V1_0:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V1_0>(simmBucketMapper);
        break;
    case SimmVersion::V1_3:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V1_3>(simmBucketMapper);
        break;
    case SimmVersion::V1_3_38:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V1_3_38>(simmBucketMapper);
        break;
    case SimmVersion::V2_0:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_0>(simmBucketMapper);
        break;
    case SimmVersion::V2_1:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_1>(simmBucketMapper);
        break;
    case SimmVersion::V2_2:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_2>(simmBucketMapper, mporDays);
        break;
    case SimmVersion::V2_3:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_3>(simmBucketMapper, mporDays);
        break;
    case SimmVersion::V2_3_8:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_3_8>(simmBucketMapper, mporDays);
        break;
    case SimmVersion::V2_5:
	    return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_5>(simmBucketMapper, mporDays);
        break;
    case SimmVersion::V2_5A:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_5A>(simmBucketMapper, mporDays);
        break;
    case SimmVersion::V2_6:
        return QuantLib::ext::make_shared<SimmConfiguration_ISDA_V2_6>(simmBucketMapper, mporDays);
        break;
    default:
        break;
    }

    QL_FAIL("SIMM configuration for version '" << simmVersion << "' cannot be built");
}

std::string escapeCommaSeparatedList(const std::string& str, const char& csvQuoteChar) {
    std::string result = str;
    if (result.find(',') != string::npos && csvQuoteChar == '\0') {
        result = '\"' + result + '\"';
    }
    return result;
}

} // namespace analytics
} // namespace ore
