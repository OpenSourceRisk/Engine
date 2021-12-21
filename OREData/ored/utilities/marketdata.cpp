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
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/configuration/conventions.hpp>

#include <boost/algorithm/string.hpp>

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
    auto tmp = "__SECCRCRV_" + securityId + "_&_" + creditCurveId + "_&_";
    return tmp;
}

std::string creditCurveNameFromSecuritySpecificCreditCurveName(const std::string& name) {
    if (boost::starts_with(name, "__SECCRCRV_")) {
        std::size_t pos = name.find("_&_", 11);
        if (pos != std::string::npos) {
            std::size_t pos2 = name.find("_&_", pos + 3);
            if (pos2 != std::string::npos) {
                auto tmp = name.substr(pos + 3, pos2 - (pos + 3));
                return tmp;
            }
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

std::string prettyPrintInternalCurveName(std::string name) {
    std::size_t pos = 0;
    bool found;
    do {
        found = false;
        std::size_t pos2 = name.find("__SECCRCRV_", pos);
        if (pos2 != std::string::npos) {
            std::size_t pos3 = name.find("_&_", pos2);
            if (pos3 != std::string::npos) {
                std::size_t pos4 = name.find("_&_", pos3 + 3);
                if (pos4 != std::string::npos) {
                    name.replace(pos2, pos4 + 3 - pos2,
                                 name.substr(pos2 + 11, pos3 - (pos2 + 11)) + "(" +
                                     name.substr(pos3 + 3, pos4 - (pos3 + 3)) + ")");
                    pos = pos + (pos4 - pos2 - 12);
                    found = true;
                }
            }
        }
    } while (found);
    return name;
}

boost::shared_ptr<QuantExt::FxIndex> buildFxIndex(const string& fxIndex, const string& domestic, 
    const string& foreign, const boost::shared_ptr<Market>& market, const string& configuration,
    bool useXbsCurves) {

    // 1. Parse the index we have with no term structures
    boost::shared_ptr<QuantExt::FxIndex> fxIndexBase = parseFxIndex(fxIndex);

    // get market data objects - we set up the index using source/target, fixing days
    // and calendar from legData_[i].fxIndex()
    string source = fxIndexBase->sourceCurrency().code();
    string target = fxIndexBase->targetCurrency().code();

    // get the base index from the market for the currency pair
    Handle<QuantExt::FxIndex> fxInd = market->fxIndex(source + target);

    // check if we need to invert the index
    bool invertFxIndex = false;
    if (!domestic.empty() && !foreign.empty()) {
        if (domestic == target && foreign == source) {
            invertFxIndex = false;
        } else if (domestic == source && foreign == target) {
            invertFxIndex = true;
        } else {
            QL_FAIL("Cannot combine FX Index " << fxIndex << " with reset ccy " << domestic
                                               << " and reset foreignCurrency " << foreign);
        }
    }

    Handle<YieldTermStructure> sorTS, tarTS;
    if (useXbsCurves) {
        sorTS = xccyYieldCurve(market, source, configuration);
        tarTS = xccyYieldCurve(market, target, configuration);
    }

    if (fxInd.empty()) {
        sorTS = sorTS.empty() ? market->discountCurve(source) : sorTS;
        tarTS = tarTS.empty() ? market->discountCurve(target) : tarTS;
        auto spot = market->fxSpot(source + target);

        Natural spotDays = 2;
        Calendar calendar = parseCalendar(source + "," + target);

        const boost::shared_ptr<Conventions>& conventions = InstrumentConventions::instance().conventions();
        auto fxCon =
            boost::dynamic_pointer_cast<FXConvention>(conventions->getFxConvention(source, target));
        if (fxCon) {
            spotDays = fxCon->spotDays();
            calendar = fxCon->advanceCalendar();
        }

        return boost::make_shared<QuantExt::FxIndex>(fxIndexBase->familyName(), spotDays,
            fxIndexBase->sourceCurrency(), fxIndexBase->targetCurrency(), calendar, spot, 
            sorTS, tarTS, invertFxIndex);
    }
    else
        return fxInd->clone(Handle<Quote>(), sorTS, tarTS, fxIndexBase->familyName(), invertFxIndex);
}

} // namespace data
} // namespace ore
