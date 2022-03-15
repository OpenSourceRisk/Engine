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
#include <ored/utilities/to_string.hpp>
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

QuantLib::Handle<QuantExt::CreditCurve>
securitySpecificCreditCurve(const boost::shared_ptr<Market>& market, const std::string& securityId,
                            const std::string& creditCurveId, const std::string& configuration) {
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

boost::shared_ptr<QuantExt::FxIndex> buildFxIndex(const string& fxIndex, const string& domestic, 
    const string& foreign, const boost::shared_ptr<Market>& market, const string& configuration,
    bool useXbsCurves) {

    // get the base index from the market for the currency pair
    Handle<QuantExt::FxIndex> fxInd = market->fxIndex(fxIndex);

    // check if we need to invert the index
    string source = fxInd->sourceCurrency().code();
    string target = fxInd->targetCurrency().code();
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
    if (invertFxIndex || useXbsCurves)
        return fxInd->clone(Handle<Quote>(), sorTS, tarTS, fxInd->familyName(), invertFxIndex);
    else
        return fxInd.currentLink();
}


void getFxIndexConventions(const string& index, Natural& fixingDays, Calendar& fixingCalendar) {
    // can take an fx index or ccy pair e.g. EURUSD
    string ccy1, ccy2;
    if (isFxIndex(index)) {
        auto ind = parseFxIndex(index);
        ccy1 = ind->sourceCurrency().code();
        ccy2 = ind->targetCurrency().code();
    } else {
        QL_REQUIRE(index.size() == 6, "getFxIndexConventions: index must be an FXIndex of form FX-ECB-EUR-USD, " <<
            "or a currency pair e.g. EURUSD.");
        ccy1 = index.substr(0, 3);
        ccy2 = index.substr(3);
    }        
    
    try {
        const boost::shared_ptr<Conventions>& conventions = InstrumentConventions::instance().conventions();
        boost::shared_ptr<Convention> con;
        try {
            // first look for the index directly
            con = conventions->get(index);
        } catch (...) {
            // then by currency pair
            con = conventions->getFxConvention(ccy1, ccy2);
        }
        if (auto fxCon = boost::dynamic_pointer_cast<FXConvention>(con)) {
            fixingDays = fxCon->spotDays();
            fixingCalendar = fxCon->advanceCalendar();
        }
    } catch (...) {
        fixingDays = 2;
        fixingCalendar = parseCalendar(ccy1 + "," + ccy2);
    }
}

std::pair<std::string, std::string> splitCreditCurveId(const std::string& creditCurveId) {
    Size pos = creditCurveId.rfind("_");
    if (pos != std::string::npos) {
	Period term;
	string termString = creditCurveId.substr(pos + 1, creditCurveId.length());
	if (tryParse<Period>(termString, term, parsePeriod)) {
	    std::cout << "split " << creditCurveId << " into " << creditCurveId.substr(0, pos) << ", " << termString << std::endl;
	    return make_pair(creditCurveId.substr(0, pos), termString);
	}
    }
    std::cout << "did not split " << creditCurveId << std::endl;
    return make_pair(creditCurveId, "");
}

/*! Get default curve for index cds from market:
    - if creditCurveId ends on _5Y (or any other term), use that to get the curve from the market
    - otherwise use creditCurveId + _5Y where the term "5Y" is derived from startDate and endDate;
      if a curve with this name is not available or if no reasonable term can be derived,
      fall back on the label creditCurveId without term suffix
*/
QuantLib::Handle<QuantExt::CreditCurve> indexCdsDefaultCurve(const boost::shared_ptr<Market>& market,
							     const std::string& creditCurveId,
							     const std::string& config, const Date& startDate,
							     const Date& endDate) {
    std::cout << "getting index cds curve for " << creditCurveId;

    auto p = splitCreditCurveId(creditCurveId);
    if(!p.second.empty()) {
	std::cout << " => " << creditCurveId << std::endl;
	return market->defaultCurve(creditCurveId, config);
    }

    // imply the term from startDate / endDate

    static const std::vector<Period> eligibleTerms = {5 * Years, 7 * Years, 10 * Years, 3 * Years, 1 * Years,
						      2 * Years, 4 * Years, 6 * Years,  8 * Years, 9 * Years};
    static const int gracePeriod = 15;

    for(auto const& p: eligibleTerms) {
	if(std::abs(cdsMaturity(startDate, p, DateGeneration::CDS2015) - endDate) < gracePeriod) {
	    std::string id = creditCurveId + "_" + ore::data::to_string(p);
	    std::cout << " => " << id << std::endl;
	    try {
		return market->defaultCurve(id, config);
	    } catch(...) {
		std::cout << "not found! fall back on " << creditCurveId << std::endl;
		return market->defaultCurve(creditCurveId, config);
	    }
	}
    }

    std::cout << "no eligible term found, fall back on " << creditCurveId << std::endl;
    return market->defaultCurve(creditCurveId, config);
}

} // namespace data
} // namespace ore
