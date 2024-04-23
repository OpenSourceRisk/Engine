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

#include <ored/configuration/conventions.hpp>
#include <ored/utilities/currencyparser.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/marketdata.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <boost/algorithm/string.hpp>

using QuantLib::DefaultProbabilityTermStructure;
using QuantLib::Handle;
using QuantLib::YieldTermStructure;
using std::string;

namespace ore {
namespace data {

const string xccyCurveNamePrefix = "__XCCY__";

string xccyCurveName(const string& ccyCode) { return xccyCurveNamePrefix + "-" + ccyCode; }

Handle<YieldTermStructure> xccyYieldCurve(const QuantLib::ext::shared_ptr<Market>& market, const string& ccyCode,
                                          const string& configuration) {
    bool dummy;
    return xccyYieldCurve(market, ccyCode, dummy, configuration);
}

Handle<YieldTermStructure> xccyYieldCurve(const QuantLib::ext::shared_ptr<Market>& market, const string& ccyCode,
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

Handle<YieldTermStructure> indexOrYieldCurve(const QuantLib::ext::shared_ptr<Market>& market, const std::string& name,
                                             const std::string& configuration) {
    try {
        return market->iborIndex(name, configuration)->forwardingTermStructure();
    } catch (...) {
    }
    try {
        return market->yieldCurve(name, configuration);
    } catch (...) {
    }
    QL_FAIL("Could not find index or yield curve with name '" << name << "' under configuration '" << configuration
                                                              << "' or default configuration.");
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

QuantLib::Handle<QuantExt::CreditCurve> securitySpecificCreditCurve(const QuantLib::ext::shared_ptr<Market>& market,
                                                                    const std::string& securityId,
                                                                    const std::string& creditCurveId,
                                                                    const std::string& configuration) {
    Handle<QuantExt::CreditCurve> curve;
    std::string name = securitySpecificCreditCurveName(securityId, creditCurveId);
    try {
        curve = market->defaultCurve(name, configuration);
    } catch (const std::exception&) {
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

QuantLib::ext::shared_ptr<QuantExt::FxIndex> buildFxIndex(const string& fxIndex, const string& domestic, const string& foreign,
                                                  const QuantLib::ext::shared_ptr<Market>& market, const string& configuration,
                                                  bool useXbsCurves) {

    auto fxInd = parseFxIndex(fxIndex);

    string source = fxInd->sourceCurrency().code();
    string target = fxInd->targetCurrency().code();
    string family = fxInd->familyName();

    fxInd = *market->fxIndex("FX-" + family + "-" + foreign + "-" + domestic);

    QL_REQUIRE((domestic == target && foreign == source) || (domestic == source && foreign == target),
               "buildFxIndex(): index '" << fxIndex << "' does not match given currencies " << domestic << ", "
                                         << foreign);

    if (!useXbsCurves)
        return fxInd;

    return fxInd->clone(Handle<Quote>(), xccyYieldCurve(market, foreign, configuration),
                        xccyYieldCurve(market, domestic, configuration));
}

std::tuple<Natural, Calendar, BusinessDayConvention> getFxIndexConventions(const string& index) {
    // can take an fx index or ccy pair e.g. EURUSD
    string ccy1, ccy2;
    string fixingSource;
    if (isFxIndex(index)) {
        auto ind = parseFxIndex(index);
        ccy1 = ind->sourceCurrency().code();
        ccy2 = ind->targetCurrency().code();
        fixingSource = ind->familyName();
    } else {
        QL_REQUIRE(index.size() == 6, "getFxIndexConventions: index must be an FXIndex of form FX-ECB-EUR-USD, "
                                          << "or a currency pair e.g. EURUSD, got '" + index + "'");
        ccy1 = index.substr(0, 3);
        ccy2 = index.substr(3);
        fixingSource = "GENERIC";
    }

    if (ccy1 == ccy2)
        return std::make_tuple(0, NullCalendar(), Unadjusted);

    const QuantLib::ext::shared_ptr<Conventions>& conventions = InstrumentConventions::instance().conventions();
    QuantLib::ext::shared_ptr<Convention> con;
    // first look for the index and inverse index directly
    try {
        con = conventions->get("FX-" + fixingSource + "-" + ccy1 + "-" + ccy2);
    } catch (...) {
    }
    if (con == nullptr) {
        try {
            con = conventions->get("FX-" + fixingSource + "-" + ccy2 + "-" + ccy1);
        } catch (...) {
        }
    }
    // then by currency pair and inverse currency pair (getFxConvention() handles both)
    if (con == nullptr) {
        try {
            con = conventions->getFxConvention(ccy1, ccy2);
        } catch (...) {
        }
    }
    if (auto fxCon = QuantLib::ext::dynamic_pointer_cast<FXConvention>(con)) {
        TLOG("getFxIndexConvention(" << index << "): " << fxCon->spotDays() << " / " << fxCon->advanceCalendar().name()
                                     << " from convention.");
        return std::make_tuple(fxCon->spotDays(), fxCon->advanceCalendar(), fxCon->convention());
    } else if (auto comCon = QuantLib::ext::dynamic_pointer_cast<CommodityForwardConvention>(con); comCon !=nullptr
               && (isPseudoCurrency(ccy1) || isPseudoCurrency(ccy2))) {
        TLOG("getFxIndexConvention(" << index << "): " << fxCon->spotDays() << " / " << fxCon->advanceCalendar().name()
                                     << " from convention.");
        return std::make_tuple(0, comCon->advanceCalendar(), comCon->bdc());
    }

    // default calendar for pseudo currencies is USD
    if (isPseudoCurrency(ccy1))
        ccy1 = "USD";
    if (isPseudoCurrency(ccy2))
        ccy2 = "USD";

    try {
	Calendar cal = parseCalendar(ccy1 + "," + ccy2);
        TLOG("getFxIndexConvention(" << index << "): 2 (default) / " << cal.name()
                                     << " (from ccys), no convention found.");
        return std::make_tuple(2, cal, Following);
    } catch (const std::exception& e) {
        ALOG("could not get fx index convention for '" << index << "': " << e.what() << ", continue with 'USD'");
    }
    TLOG("getFxIndexConvention(" << index
                                 << "): 2 (default) / USD (default), no convention found, could not parse calendar '"
                                 << (ccy1 + "," + ccy2) << "'");
    return std::make_tuple(2, parseCalendar("USD"), Following);
}

std::pair<std::string, QuantLib::Period> splitCurveIdWithTenor(const std::string& creditCurveId) {
    Size pos = creditCurveId.rfind("_");
    if (pos != std::string::npos) {
        Period term;
        string termString = creditCurveId.substr(pos + 1, creditCurveId.length());
        if (tryParse<Period>(termString, term, parsePeriod)) {
            return make_pair(creditCurveId.substr(0, pos), term);
        }
    }
    return make_pair(creditCurveId, 0 * Days);
}

QuantLib::Handle<QuantExt::CreditCurve> indexCdsDefaultCurve(const QuantLib::ext::shared_ptr<Market>& market,
                                                             const std::string& creditCurveId,
                                                             const std::string& config) {
    try {
        return market->defaultCurve(creditCurveId, config);
    } catch (...) {
        DLOG("indexCdsDefaultCurve: could not get '" << creditCurveId << "', fall back on curve id without tenor.");
    }

    auto p = splitCurveIdWithTenor(creditCurveId);
    return market->defaultCurve(p.first, config);
}

} // namespace data
} // namespace ore
