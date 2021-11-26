/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>

using QuantLib::DefaultProbabilityTermStructure;
using QuantLib::Handle;
using QuantLib::YieldTermStructure;
using std::string;

namespace ore {
namespace data {

const string xccyCurveNamePrefix = "__XCCY__";

string xccyCurveName(const string& ccyCode) { return xccyCurveNamePrefix + "-" + ccyCode; }

Handle<YieldTermStructure> xccyYieldCurve(const boost::shared_ptr<Market>& market, const string& ccyCode,
                                          const string& configuration) {
    bool dummy;
    return xccyYieldCurve(market, ccyCode, dummy, configuration);
}

Handle<YieldTermStructure> xccyYieldCurve(const boost::shared_ptr<Market>& market, const string& ccyCode,
                                          bool& outXccyExists, const string& configuration) {

    Handle<YieldTermStructure> curve;
    string xccyCurve = xccyCurveName(ccyCode);
    outXccyExists = true;
    try {
        curve = market->yieldCurve(xccyCurve, configuration);
    } catch (const Error&) {
        DLOG("Could not link " << ccyCode << " termstructure to cross currency yield curve " << xccyCurve
                               << " so just using " << ccyCode << " discount curve.");
        curve = market->discountCurve(ccyCode, configuration);
        outXccyExists = false;
    }

    return curve;
}

std::string securitySpecificCreditCurveName(const std::string& securityId, const std::string& creditCurveId) {
    auto tmp = "__SECURITYCREDITCURVE__" + securityId + "__&__" + creditCurveId;
    return tmp;
}

std::string creditCurveNameFromSecuritySpecificCreditCurveName(const std::string& name) {
    if (name.substr(0, 23) == "__SECURITYCREDITCURVE__") {
        std::size_t pos = name.find("__&__", 23);
        if (pos != std::string::npos) {
            auto tmp = name.substr(pos + 5);
            return tmp;
        } else {
            return name;
        }
    }
    return name;
}

QuantLib::Handle<QuantLib::DefaultProbabilityTermStructure>
securitySpecificCreditCurve(const boost::shared_ptr<Market>& market, const std::string& securityId,
                            const std::string& creditCurveId, const std::string& configuration) {
    Handle<DefaultProbabilityTermStructure> curve;
    std::string name = securitySpecificCreditCurveName(securityId, creditCurveId);
    try {
        curve = market->defaultCurve(name, configuration);
    } catch (const std::exception& e) {
        DLOG("Could not link " << securityId << " to security specific credit curve " << name << " so just using "
                               << creditCurveId << " default curve.");
        curve = market->defaultCurve(creditCurveId, configuration);
    }
    return curve;
}

} // namespace data
} // namespace ore
