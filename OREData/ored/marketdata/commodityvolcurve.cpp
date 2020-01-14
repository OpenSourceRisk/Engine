/*
Copyright (C) 2018 Quaternion Risk Management Ltd
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

#include <ored/marketdata/commodityvolcurve.hpp>

#include <algorithm>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/math/matrix.hpp>
#include <ql/termstructures/volatility/equityfx/blackconstantvol.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancecurve.hpp>
#include <ql/termstructures/volatility/equityfx/blackvariancesurface.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <qle/termstructures/blackvariancesurfacesparse.hpp>

#include <regex>

using namespace std;
using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace data {

CommodityVolCurve::CommodityVolCurve(const Date& asof, const CommodityVolatilityCurveSpec& spec, const Loader& loader,
                                     const CurveConfigurations& curveConfigs) {

    try {
        boost::shared_ptr<CommodityVolatilityCurveConfig> config =
            curveConfigs.commodityVolatilityCurveConfig(spec.curveConfigID());

        // Do different things depending on the type of volatility configured
        switch (config->type()) {
        case CommodityVolatilityCurveConfig::Type::Constant:
            buildConstantVolatility(asof, *config, loader);
            break;
        case CommodityVolatilityCurveConfig::Type::Curve:
            buildVolatilityCurve(asof, *config, loader);
            break;
        case CommodityVolatilityCurveConfig::Type::Surface:
            buildVolatilitySurface(asof, *config, loader);
            break;
        default:
            QL_FAIL("Commodity volatility type not recognised");
        }

        // Enable/disable extrapolation as per config
        volatility_->enableExtrapolation(config->extrapolate());

    } catch (std::exception& e) {
        QL_FAIL("Commodity volatility curve building failed : " << e.what());
    } catch (...) {
        QL_FAIL("Commodity volatility curve building failed: unknown error");
    }
}

void CommodityVolCurve::buildConstantVolatility(const Date& asof, CommodityVolatilityCurveConfig& config,
                                                const Loader& loader) {
    // Should have a single quote for constant volatility structure
    QL_REQUIRE(config.quotes().size() == 1, "Expected a single quote when the commodity volatility type is constant");
    string quoteId = config.quotes()[0];

    // Loop over all market datums and find the single quote
    // Return error if there are duplicates (this is why we do not use loader.get() method)
    Real quoteValue = Null<Real>();
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {
        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::COMMODITY_OPTION) {

            boost::shared_ptr<CommodityOptionQuote> q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md);

            if (q->name() == quoteId) {
                QL_REQUIRE(quoteValue == Null<Real>(), "Duplicate quote found for quote with id " << quoteId);
                quoteValue = q->quote()->value();
            }
        }
    }
    QL_REQUIRE(quoteValue != Null<Real>(), "Quote not found for id " << quoteId);

    // Build the constant volatility structure
    DayCounter dayCounter = parseDayCounter(config.dayCounter());
    Calendar calendar = parseCalendar(config.calendar());
    LOG("CommodityVolatilityCurve: Building BlackConstantVol term structure");
    volatility_ = boost::make_shared<BlackConstantVol>(asof, calendar, quoteValue, dayCounter);
}

void CommodityVolCurve::buildVolatilityCurve(const Date& asof, CommodityVolatilityCurveConfig& config,
                                             const Loader& loader) {

    // Loop over all market datums and find the quotes *in* the config
    // Return error if there are duplicate quotes (this is why we do not use loader.get() method)

    QL_REQUIRE(config.quotes().size() > 0, "No quotes specified in config " << config.curveID());

    bool foundRegex = false;
    regex reg1;

    // check for regex string in config
    for (Size i = 0; i < config.quotes().size(); i++) {
        foundRegex |= config.quotes()[i].find("*") != string::npos;
        if (foundRegex) {
            break;
        }
    }

    if (foundRegex) {
        QL_REQUIRE(config.quotes().size() == 1,
                   "Wild card specified in config " << config.curveID() << " but more quotes also specified.");

        // build regex string
        string regexstr = config.quotes()[0];
        boost::replace_all(regexstr, "*", ".*");
        reg1 = regex(regexstr);
    }

    map<Date, Real> curveData;
    Calendar calendar = parseCalendar(config.calendar());
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {
        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::COMMODITY_OPTION) {

            boost::shared_ptr<CommodityOptionQuote> q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md);

            bool addQuote = false;
            vector<string>::const_iterator it;  // if not wild-card -> find quote.
            if (foundRegex) {
                if (regex_match(q->name(), reg1)) {
                    // quote only valid if expiry date is > asof and the quote expiry is a date or period.
                    Date eDate;
                    if (auto expiryDate = boost::dynamic_pointer_cast<ExpiryDate>(q->expiry())) {
                        eDate = expiryDate->expiryDate();
                    } else if (auto expiryPeriod = boost::dynamic_pointer_cast<ExpiryPeriod>(q->expiry())) {
                        eDate = calendar.adjust(asof + expiryPeriod->expiryPeriod());
                    }
                    addQuote = eDate != Date() && eDate > asof;
                }
            } else {
                it = find(config.quotes().begin(), config.quotes().end(), q->name()); // find in config
                if (it != config.quotes().end()) {
                    addQuote = true;
                }
            }

            // add quote if relevant
            if (addQuote) {
                // Convert expiry to a date if necessary
                Date aDate;
                if (auto expiryDate = boost::dynamic_pointer_cast<ExpiryDate>(q->expiry())) {
                    aDate = expiryDate->expiryDate();
                } else if (auto expiryPeriod = boost::dynamic_pointer_cast<ExpiryPeriod>(q->expiry())) {
                    aDate = calendar.adjust(asof + expiryPeriod->expiryPeriod());
                }
                QL_REQUIRE(aDate > asof, "Commodity volatility quote '" << q->name() << "' has expiry in the past ("
                    << io::iso_date(aDate) << ")");

                // Add the quote to the curve data
                QL_REQUIRE(curveData.count(aDate) == 0, "Duplicate quote for the date " << io::iso_date(aDate) << 
                    " provided by commodity volatility config " << config.curveID());
                curveData[aDate] = q->quote()->value();
            }
        }
    }
    LOG("CommodityVolatilityCurve: read " << curveData.size() << " quotes.");

    if (foundRegex) {
        QL_REQUIRE(curveData.size() > 0, "No quotes found matching " << config.quotes()[0]);
    } else {
        QL_REQUIRE(curveData.size() == config.quotes().size(), "Found " << curveData.size() << " quotes, but "
                                                                        << config.quotes().size()
                                                                        << " quotes given in config.");
    }

    // Create the dates and volatility vector
    vector<Date> dates;
    vector<Volatility> volatilities;
    for (const auto& datum : curveData) {
        dates.push_back(datum.first);
        volatilities.push_back(datum.second);
    }

    // Build the volatility structure curve
    DayCounter dayCounter = parseDayCounter(config.dayCounter());
    LOG("CommodityVolatilityCurve: Building BlackVarianceCurve term structure");
    volatility_ = boost::make_shared<BlackVarianceCurve>(asof, dates, volatilities, dayCounter);
}

void CommodityVolCurve::buildVolatilitySurface(const Date& asof, CommodityVolatilityCurveConfig& config,
                                               const Loader& loader) {

    map<Date, vector<Real>> surfaceData; // implicit order is assumed here for known strikes and expiries from config.
    vector<Real> strikes;       // wild card case
    vector<Date> expiries;      // wild card case
    vector<Volatility> vols;    // wild card case

    // wild cards?
    vector<string>::iterator exprWcIt = find(config.expiries().begin(), config.expiries().end(), "*");
    vector<string>::iterator strkWcIt = find(config.strikes().begin(), config.strikes().end(), "*");
    bool expWc = exprWcIt != config.expiries().end();
    bool strkWc = strkWcIt != config.strikes().end();
    bool wildCard = expWc || strkWc;
    if (expWc) {
        QL_REQUIRE(config.expiries().size() == 1, "Wild card expiry specified but more expiries also specified.");
    }
    if (strkWc) {
        QL_REQUIRE(config.strikes().size() == 1, "Wild card strike specified but more strikes also specified.");
    }

    // If we do not have a strike wild card, we expect a list of absolute strike values
    vector<Real> configuredStrikes;
    if (!strkWc) {
        // Parse the list of absolute strikes
        configuredStrikes = parseVectorOfValues<Real>(config.strikes(), &parseReal);
        sort(configuredStrikes.begin(), configuredStrikes.end());
        QL_REQUIRE(adjacent_find(configuredStrikes.begin(), configuredStrikes.end())
            == configuredStrikes.end(), "The configured strikes contain duplicates");
    }

    // If we do not have an expiry wild card, the configured expiries must be either a date or a period
    set<Date> configuredExpiryDates;
    set<Period> configuredExpiryPeriods;
    if (!expWc) {
        // Parse the list of expiry strings.
        for (const string& strExpiry : config.expiries()) {
            Date date;
            Period period;
            bool isDate;
            parseDateOrPeriod(strExpiry, date, period, isDate);
            if (isDate) {
                configuredExpiryDates.insert(date);
            } else {
                configuredExpiryPeriods.insert(period);
            }
        }
    }

    // Loop over all market datums and find the quotes in the config
    // Return error if there are duplicate quotes (this is why we do not use loader.get() method)
    bool relevantQuote = false;
    bool strikeRelevant = strkWc;
    bool expiryRelevant = expWc;
    Size quotesAdded = 0;
    Calendar calendar = parseCalendar(config.calendar());
    DayCounter dayCounter = parseDayCounter(config.dayCounter());
    for (const boost::shared_ptr<MarketDatum>& md : loader.loadQuotes(asof)) {
        if (md->asofDate() == asof && md->instrumentType() == MarketDatum::InstrumentType::COMMODITY_OPTION) {
            boost::shared_ptr<CommodityOptionQuote> q = boost::dynamic_pointer_cast<CommodityOptionQuote>(md);
            relevantQuote = false;
            vector<string>::const_iterator it;

            // Only read absolute strike quotes for now.
            auto strike = boost::dynamic_pointer_cast<AbsoluteStrike>(q->strike());
            if (!strike)
                continue;

            // no wild cards
            if (!wildCard) {

                it = find(config.quotes().begin(), config.quotes().end(), q->name());
                relevantQuote = it != config.quotes().end();
                if (relevantQuote) {
                    // Convert expiry to date if necessary
                    Date aDate;
                    if (auto expiryDate = boost::dynamic_pointer_cast<ExpiryDate>(q->expiry())) {
                        aDate = expiryDate->expiryDate();
                        QL_REQUIRE(aDate > asof, "Commodity volatility quote '" << q->name() << 
                            "' has expiry in past (" << io::iso_date(aDate) << ")");
                    } else if (auto expiryPeriod = boost::dynamic_pointer_cast<ExpiryPeriod>(q->expiry())) {
                        aDate = calendar.adjust(asof + expiryPeriod->expiryPeriod());
                    }

                    // Position of quote in vector
                    auto strikeIt = find_if(configuredStrikes.begin(), configuredStrikes.end(), 
                        [&strike](Real s) { return close(s, strike->strike()); });
                    Size pos = distance(configuredStrikes.begin(), strikeIt);
                    QL_REQUIRE(pos < configuredStrikes.size(), "The quote '" << q->name() << 
                        "' is in the list of configured quotes but does not match any of the configured strikes");

                    // Add quote to surface
                    if (surfaceData.count(aDate) == 0)
                        surfaceData[aDate] = vector<Real>(configuredStrikes.size(), Null<Real>());
                    QL_REQUIRE(surfaceData[aDate][pos] == Null<Real>(), "Quote " << q->name() << 
                        " provides a duplicate quote for the date " << io::iso_date(aDate) << 
                        " and the strike " << configuredStrikes[pos]);
                    surfaceData[aDate][pos] = q->quote()->value();
                    quotesAdded++;
                }
            } else {

                // Don't bother if commodity name and currency do not match
                if (config.curveID() != q->commodityName() || config.currency() != q->quoteCurrency()) {
                    continue;
                }

                if (!expWc) {
                    // Is the quote's expiry in the list of configured expiries.
                    if (auto expiryDate = boost::dynamic_pointer_cast<ExpiryDate>(q->expiry())) {
                        expiryRelevant = configuredExpiryDates.find(expiryDate->expiryDate()) 
                            != configuredExpiryDates.end();
                    } else if (auto expiryPeriod = boost::dynamic_pointer_cast<ExpiryPeriod>(q->expiry())) {
                        expiryRelevant = configuredExpiryPeriods.find(expiryPeriod->expiryPeriod())
                            != configuredExpiryPeriods.end();
                    }
                } else {
                    expiryRelevant = true;
                }
                if (!strkWc) {
                    // expiry only wild card
                    auto strikeIt = find_if(configuredStrikes.begin(), configuredStrikes.end(),
                        [&strike](Real s) { return close(s, strike->strike()); });
                    strikeRelevant = strikeIt != configuredStrikes.end();
                } else {
                    strikeRelevant = true;
                }
                relevantQuote = strikeRelevant && expiryRelevant;

                if (relevantQuote) {
                    // expiry (may be tenor or date)
                    Date eDate;
                    if (auto expiryDate = boost::dynamic_pointer_cast<ExpiryDate>(q->expiry())) {
                        eDate = expiryDate->expiryDate();
                    } else if (auto expiryPeriod = boost::dynamic_pointer_cast<ExpiryPeriod>(q->expiry())) {
                        eDate = calendar.adjust(asof + expiryPeriod->expiryPeriod());
                    }

                    // strike + vol
                    strikes.push_back(strike->strike());
                    vols.push_back(q->quote()->value());
                    expiries.push_back(eDate);
                    quotesAdded++;
                }
            }
        }
    }
    LOG("CommodityVolatilityCurve: read " << quotesAdded << " quotes.");

    // check loaded quotes
    if (!wildCard) {
        QL_REQUIRE(config.quotes().size() == quotesAdded, "Found " << quotesAdded << " quotes, but "
                                                                   << config.quotes().size()
                                                                   << " quotes required by config.");
    } else {
        QL_REQUIRE(quotesAdded > 0, "No quotes loaded for" << config.curveID());
        QL_REQUIRE(strikes.size() == vols.size() && vols.size() == expiries.size(),
                   "Quotes loaded don't produce strike,vol,expiry vectors of equal length.");
    }

    // Build the volatility surface
    tuple<vector<Real>, vector<Date>> sAndE;
    Matrix volatilities;
    BlackVarianceSurface::Extrapolation lowerStrikeExtrap =
        config.lowerStrikeConstantExtrapolation()
            ? BlackVarianceSurface::Extrapolation::ConstantExtrapolation
            : BlackVarianceSurface::Extrapolation::InterpolatorDefaultExtrapolation;
    BlackVarianceSurface::Extrapolation upperStrikeExtrap =
        config.upperStrikeConstantExtrapolation()
            ? BlackVarianceSurface::Extrapolation::ConstantExtrapolation
            : BlackVarianceSurface::Extrapolation::InterpolatorDefaultExtrapolation;
    
    if (!wildCard) {
        // Vol matrix (strikes, dates), date & expiry vectors
        volatilities = Matrix(configuredStrikes.size(), surfaceData.size());
        get<1>(sAndE).reserve(surfaceData.size());
        get<0>(sAndE).reserve(configuredStrikes.size());
        Size counter = 0;
        for (const auto& datum : surfaceData) {
            get<1>(sAndE).push_back(datum.first);
            for (Size i = 0; i < configuredStrikes.size(); i++) {
                if (counter == 0)
                    get<0>(sAndE).push_back(configuredStrikes[i]);
                volatilities[i][counter] = datum.second[i];
            }
            counter++;
        }
        LOG("CommodityVolatilityCurve: Building BlackVarianceSurface term structure");
        volatility_ =
            boost::make_shared<BlackVarianceSurface>(asof, calendar, get<1>(sAndE), get<0>(sAndE), volatilities,
                                                     dayCounter, lowerStrikeExtrap, upperStrikeExtrap);
    } else {
        bool lowerConstStrikeExtrap = lowerStrikeExtrap == BlackVarianceSurface::Extrapolation::ConstantExtrapolation;
        bool upperConstStrikeExtrap = upperStrikeExtrap == BlackVarianceSurface::Extrapolation::ConstantExtrapolation;
      
        volatility_ =
            boost::make_shared<BlackVarianceSurfaceSparse>(asof, calendar, expiries, strikes, vols,
                                                     dayCounter, lowerConstStrikeExtrap, upperConstStrikeExtrap);
    }
}

}
}
