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

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <ored/marketdata/commoditycurve.hpp>
#include <ored/utilities/conventionsbasedfutureexpiry.hpp>
#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <qle/termstructures/averagefuturepricehelper.hpp>
#include <qle/termstructures/averageoffpeakpowerhelper.hpp>
#include <qle/termstructures/averagespotpricehelper.hpp>
#include <qle/termstructures/commodityaveragebasispricecurve.hpp>
#include <qle/termstructures/commoditybasispricecurve.hpp>
#include <qle/termstructures/crosscurrencypricetermstructure.hpp>
#include <qle/termstructures/futurepricehelper.hpp>
#include <qle/termstructures/iterativebootstrap.hpp>
#include <qle/termstructures/piecewisepricecurve.hpp>
#include <ql/time/calendars/weekendsonly.hpp>
#include <ql/time/date.hpp>
#include <regex>

using QuantExt::AverageFuturePriceHelper;
using QuantExt::AverageOffPeakPowerHelper;
using QuantExt::AverageSpotPriceHelper;
using QuantExt::FutureExpiryCalculator;
using QuantExt::FuturePriceHelper;
using QuantExt::CommodityAverageBasisPriceCurve;
using QuantExt::CommodityBasisPriceCurve;
using QuantExt::CommodityIndex;
using QuantExt::CommoditySpotIndex;
using QuantExt::CrossCurrencyPriceTermStructure;
using QuantExt::CubicFlat;
using QuantExt::InterpolatedPriceCurve;
using QuantExt::LinearFlat;
using QuantExt::LogLinearFlat;
using QuantExt::PriceTermStructure;
using QuantExt::PiecewisePriceCurve;
using QuantLib::BootstrapHelper;
using std::map;
using std::regex;
using std::string;

// Explicit template instantiation to avoid "error C2079: ... uses undefined class ..."
// Explained in the answer to the SO question here: 
// https://stackoverflow.com/a/57666066/1771882
// Needs to be in global namespace also i.e. not under ore::data
// https://stackoverflow.com/a/25594741/1771882
template class QuantExt::PiecewisePriceCurve<QuantLib::Linear, QuantExt::IterativeBootstrap>;
template class QuantExt::PiecewisePriceCurve<QuantLib::LogLinear, QuantExt::IterativeBootstrap>;
template class QuantExt::PiecewisePriceCurve<QuantLib::Cubic, QuantExt::IterativeBootstrap>;
template class QuantExt::PiecewisePriceCurve<QuantExt::LinearFlat, QuantExt::IterativeBootstrap>;
template class QuantExt::PiecewisePriceCurve<QuantExt::LogLinearFlat, QuantExt::IterativeBootstrap>;
template class QuantExt::PiecewisePriceCurve<QuantExt::CubicFlat, QuantExt::IterativeBootstrap>;
template class QuantExt::PiecewisePriceCurve<QuantLib::BackwardFlat, QuantExt::IterativeBootstrap>;

namespace ore {
namespace data {

CommodityCurve::CommodityCurve()
    : commoditySpot_(Null<Real>()), onValue_(Null<Real>()), tnValue_(Null<Real>()), regexQuotes_(false) {}

CommodityCurve::CommodityCurve(const Date& asof, const CommodityCurveSpec& spec, const Loader& loader,
                               const CurveConfigurations& curveConfigs, const Conventions& conventions,
                               const FXTriangulation& fxSpots,
                               const map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
                               const map<string, boost::shared_ptr<CommodityCurve>>& commodityCurves)
    : spec_(spec), commoditySpot_(Null<Real>()), onValue_(Null<Real>()), tnValue_(Null<Real>()), regexQuotes_(false) {

    try {

        boost::shared_ptr<CommodityCurveConfig> config = curveConfigs.commodityCurveConfig(spec_.curveConfigID());

        dayCounter_ = config->dayCountId() == "" ? Actual365Fixed() : parseDayCounter(config->dayCountId());
        interpolationMethod_ = config->interpolationMethod() == "" ? "Linear" : config->interpolationMethod();

        if (config->type() == CommodityCurveConfig::Type::Direct) {

            // Populate the raw price curve data
            map<Date, Handle<Quote>> data;
            populateData(data, asof, config, loader, conventions);

            // Create the commodity price curve
            buildCurve(asof, data, config);

        } else if (config->type() == CommodityCurveConfig::Type::Basis) {

            // We have a commodity basis configuration

            // Look up the required base price curve in the commodityCurves map
            CommodityCurveSpec ccSpec(config->currency(), config->basePriceCurveId());
            DLOG("Looking for base price curve with id, " << config->basePriceCurveId() << ", and spec, " << ccSpec
                                                          << ".");
            auto itCc = commodityCurves.find(ccSpec.name());
            QL_REQUIRE(itCc != commodityCurves.end(), "Can't find price curve with id " << config->basePriceCurveId());
            auto pts = Handle<PriceTermStructure>(itCc->second->commodityPriceCurve());

            buildBasisPriceCurve(asof, *config, conventions, pts, loader);

        } else if (config->type() == CommodityCurveConfig::Type::Piecewise) {

            // We have a piecewise commodity configuration
            buildPiecewiseCurve(asof, *config, conventions, loader, commodityCurves);

        } else {

            // We have a cross currency type commodity curve configuration
            boost::shared_ptr<CommodityCurveConfig> baseConfig =
                curveConfigs.commodityCurveConfig(config->basePriceCurveId());

            buildCrossCurrencyPriceCurve(asof, config, baseConfig, fxSpots, yieldCurves, commodityCurves);
        }

        // Apply extrapolation from the curve configuration
        commodityPriceCurve_->enableExtrapolation(config->extrapolation());

        // Ask for price now so that errors are thrown during the build, not later.
        commodityPriceCurve_->price(asof + 1 * Days);

    } catch (std::exception& e) {
        QL_FAIL("commodity curve building failed: " << e.what());
    } catch (...) {
        QL_FAIL("commodity curve building failed: unknown error");
    }
}

void CommodityCurve::populateData(map<Date, Handle<Quote>>& data, const Date& asof,
                                  const boost::shared_ptr<CommodityCurveConfig>& config, const Loader& loader,
                                  const Conventions& conventions) {

    // Some default conventions for building the commodity curve
    Period spotTenor = 2 * Days;
    Real pointsFactor = 1.0;
    Calendar cal = parseCalendar(config->currency());
    bool spotRelative = true;
    BusinessDayConvention bdc = Following;
    bool outright = true;

    // Overwrite the default conventions if the commodity curve config provides explicit conventions
    if (!config->conventionsId().empty()) {
        QL_REQUIRE(conventions.has(config->conventionsId()),
                   "Commodity conventions " << config->conventionsId() << " requested by commodity config "
                                            << config->curveID() << " not found");
        auto convention =
            boost::dynamic_pointer_cast<CommodityForwardConvention>(conventions.get(config->conventionsId()));
        QL_REQUIRE(convention, "Convention " << config->conventionsId() << " not of expected type CommodityConvention");

        spotTenor = convention->spotDays() * Days;
        pointsFactor = convention->pointsFactor();
        if (convention->advanceCalendar() != NullCalendar())
            cal = convention->advanceCalendar();
        spotRelative = convention->spotRelative();
        bdc = convention->bdc();
        outright = convention->outright();
    }

    // Commodity spot quote if provided by the configuration
    Date spotDate = cal.advance(asof, spotTenor);
    if (!config->commoditySpotQuoteId().empty()) {
        auto spot = loader.get(config->commoditySpotQuoteId(), asof)->quote();
        commoditySpot_ = spot->value();
        data[spotDate] = spot;
    } else {
        QL_REQUIRE(outright, "If the commodity forward quotes are not outright,"
                                 << " a commodity spot quote needs to be configured");
    }

    // Add the forward quotes to the curve data
    for (auto& q : getQuotes(asof, config->curveID(), config->fwdQuotes(), loader)) {

        // We add ON and TN quotes after this loop if they are given and not outright quotes
        TLOG("Commodity Forward Price found for quote: " << q->name());
        Date expiry;
        Real value = q->quote()->value();
        if (!q->tenorBased()) {
            expiry = q->expiryDate();
            add(asof, expiry, value, data, outright, pointsFactor);
        } else {
            if (q->startTenor() == boost::none) {
                expiry = cal.advance(spotRelative ? spotDate : asof, q->tenor(), bdc);
                add(asof, expiry, value, data, outright, pointsFactor);
            } else {
                if (*q->startTenor() == 0 * Days && q->tenor() == 1 * Days) {
                    onValue_ = q->quote()->value();
                    if (outright)
                        add(asof, asof, value, data, outright);
                } else if (*q->startTenor() == 1 * Days && q->tenor() == 1 * Days) {
                    tnValue_ = q->quote()->value();
                    if (outright) {
                        expiry = cal.advance(asof, 1 * Days, bdc);
                        add(asof, expiry, value, data, outright);
                    }
                } else {
                    expiry = cal.advance(cal.advance(asof, *q->startTenor(), bdc), q->tenor(), bdc);
                    add(asof, expiry, value, data, outright, pointsFactor);
                }
            }
        }
    }

    // Deal with ON and TN if quotes are not outright quotes
    if (spotTenor == 2 * Days && tnValue_ != Null<Real>() && !outright) {
        add(asof, cal.advance(asof, 1 * Days, bdc), -tnValue_, data, outright, pointsFactor);
        if (onValue_ != Null<Real>()) {
            add(asof, asof, -onValue_ - tnValue_, data, outright, pointsFactor);
        }
    }

    // Some logging and checks
    LOG("Read " << data.size() << " quotes for commodity curve " << config->curveID());
    if (!regexQuotes_) {
        QL_REQUIRE(data.size() == config->quotes().size(), "Found " << data.size() << " quotes, but "
                                                                    << config->quotes().size()
                                                                    << " quotes given in config " << config->curveID());
    } else {
        QL_REQUIRE(data.size() > 0,
                   "Regular expression specified in commodity config " << config->curveID() << " but no quotes read");
    }
}

void CommodityCurve::add(const Date& asof, const Date& expiry, Real value, map<Date, Handle<Quote>>& data, bool outright,
                         Real pointsFactor) {

    if (expiry < asof)
        return;

    QL_REQUIRE(data.find(expiry) == data.end(), "Duplicate quote for expiry " << io::iso_date(expiry) << " found.");

    if (!outright) {
        QL_REQUIRE(commoditySpot_ != Null<Real>(), "Can't use forward points without a commodity spot value");
        value = commoditySpot_ + value / pointsFactor;
    }

    data[expiry] = Handle<Quote>(boost::make_shared<SimpleQuote>(value));
}

void CommodityCurve::buildCurve(const Date& asof, const map<Date, Handle<Quote>>& data,
                                const boost::shared_ptr<CommodityCurveConfig>& config) {

    vector<Date> curveDates;
    curveDates.reserve(data.size());
    vector<Handle<Quote>> curvePrices;
    curvePrices.reserve(data.size());
    for (auto const& datum : data) {
        curveDates.push_back(datum.first);
        curvePrices.push_back(datum.second);
    }

    // Build the curve using the data
    populateCurve<InterpolatedPriceCurve>(asof, curveDates, curvePrices, dayCounter_,
                                          parseCurrency(config->currency()));
}

void CommodityCurve::buildCrossCurrencyPriceCurve(
    const Date& asof, const boost::shared_ptr<CommodityCurveConfig>& config,
    const boost::shared_ptr<CommodityCurveConfig>& baseConfig, const FXTriangulation& fxSpots,
    const map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
    const map<string, boost::shared_ptr<CommodityCurve>>& commodityCurves) {

    // Look up the required base price curve in the commodityCurves map
    // We pass in the commodity curve ID only in the member basePriceCurveId of config e.g. PM:XAUUSD.
    // But, the map commodityCurves is keyed on the spec name e.g. Commodity/USD/PM:XAUUSD
    auto commIt = commodityCurves.find(CommodityCurveSpec(baseConfig->currency(), baseConfig->curveID()).name());
    QL_REQUIRE(commIt != commodityCurves.end(), "Could not find base commodity curve with id "
                                                    << baseConfig->curveID()
                                                    << " required in the building of commodity curve with id "
                                                    << config->curveID());

    // Look up the two yield curves in the yieldCurves map
    auto baseYtsIt = yieldCurves.find(YieldCurveSpec(baseConfig->currency(), config->baseYieldCurveId()).name());
    QL_REQUIRE(baseYtsIt != yieldCurves.end(),
               "Could not find base yield curve with id "
                   << config->baseYieldCurveId() << " and currency " << baseConfig->currency()
                   << " required in the building of commodity curve with id " << config->curveID());

    auto ytsIt = yieldCurves.find(YieldCurveSpec(config->currency(), config->yieldCurveId()).name());
    QL_REQUIRE(ytsIt != yieldCurves.end(), "Could not find yield curve with id "
                                               << config->yieldCurveId() << " and currency " << config->currency()
                                               << " required in the building of commodity curve with id "
                                               << config->curveID());

    // Get the FX spot rate, number of units of this currency per unit of base currency
    Handle<Quote> fxSpot = fxSpots.getQuote(baseConfig->currency() + config->currency());

    // Populate the commodityPriceCurve_ member
    commodityPriceCurve_ = boost::make_shared<CrossCurrencyPriceTermStructure>(
        asof, QuantLib::Handle<PriceTermStructure>(commIt->second->commodityPriceCurve()), fxSpot,
        baseYtsIt->second->handle(), ytsIt->second->handle(), parseCurrency(config->currency()));
}

void CommodityCurve::buildBasisPriceCurve(const Date& asof, const CommodityCurveConfig& config,
                                          const Conventions& conventions, const Handle<PriceTermStructure>& basePts,
                                          const Loader& loader) {

    LOG("CommodityCurve: start building commodity basis curve.");

    // We need to have commodity future conventions for both the base curve and the basis curve
    QL_REQUIRE(conventions.has(config.conventionsId()), "Commodity conventions " << config.conventionsId()
                                                                                 << " requested by commodity config "
                                                                                 << config.curveID() << " not found");
    auto basisConvention =
        boost::dynamic_pointer_cast<CommodityFutureConvention>(conventions.get(config.conventionsId()));
    QL_REQUIRE(basisConvention,
               "Convention " << config.conventionsId() << " not of expected type CommodityFutureConvention");
    auto basisFec = boost::make_shared<ConventionsBasedFutureExpiry>(*basisConvention);

    QL_REQUIRE(conventions.has(config.baseConventionsId()),
               "Commodity conventions " << config.baseConventionsId() << " requested by commodity config "
                                        << config.curveID() << " not found");
    auto baseConvention =
        boost::dynamic_pointer_cast<CommodityFutureConvention>(conventions.get(config.baseConventionsId()));
    QL_REQUIRE(baseConvention,
               "Convention " << config.baseConventionsId() << " not of expected type CommodityFutureConvention");
    auto baseFec = boost::make_shared<ConventionsBasedFutureExpiry>(*baseConvention);

    // Construct the commodity index.
    auto index = parseCommodityIndex(baseConvention->id(), conventions, false, basePts);

    // Sort the configured quotes on expiry dates
    // Ignore tenor based quotes i.e. we expect an explicit expiry date and log a warning if the expiry date does not
    // match our own calculated expiry date based on the basis conventions.
    map<Date, Handle<Quote>> basisData;
    for (auto& q : getQuotes(asof, config.curveID(), config.fwdQuotes(), loader)) {

        if (q->tenorBased()) {
            TLOG("Skipping tenor based quote, " << q->name() << ".");
            continue;
        }

        if (q->expiryDate() < asof) {
            TLOG("Skipping quote because its expiry date, " << io::iso_date(q->expiryDate())
                                                            << ", is before the market date " << io::iso_date(asof));
            continue;
        }

        QL_REQUIRE(basisData.find(q->expiryDate()) == basisData.end(), "Found duplicate quote, "
                                                                           << q->name() << ", for expiry date "
                                                                           << io::iso_date(q->expiryDate()) << ".");

        basisData[q->expiryDate()] = q->quote();
        TLOG("Using quote " << q->name() << " in commodity basis curve.");

        // We expect the expiry date in the quotes to match our calculated expiry date. The code will work if it does
        // not but we log a warning in this case.
        Date calcExpiry = basisFec->nextExpiry(true, q->expiryDate());
        if (calcExpiry != q->expiryDate()) {
            WLOG("Calculated expiry date, " << io::iso_date(calcExpiry) << ", does not equal quote's expiry date "
                                            << io::iso_date(q->expiryDate()) << ".");
        }
    }

    if (basisConvention->isAveraging()) {
        // We are building a curve that will be used to return an average price.
        if (!baseConvention->isAveraging() && config.averageBase()) {
            DLOG("Creating a CommodityAverageBasisPriceCurve.");
            populateCurve<CommodityAverageBasisPriceCurve>(asof, basisData, basisFec, index,
                basePts, baseFec, config.addBasis());
        } else {
            // Either 1) base convention is not averaging and config.averageBase() is false or 2) the base convention 
            // is averaging. Either way, we build a CommodityBasisPriceCurve.
            DLOG("Creating a CommodityBasisPriceCurve for an average price curve.");
            populateCurve<CommodityBasisPriceCurve>(asof, basisData, basisFec, index, basePts,
                baseFec, config.addBasis(), config.monthOffset());
        }
    } else {
        // We are building a curve that will be used to return a price on a single date.
        QL_REQUIRE(!baseConvention->isAveraging(), "A commodity basis curve with non-averaging" <<
            " basis and averaging base is not valid.");

        DLOG("Creating a CommodityBasisPriceCurve.");
        populateCurve<CommodityBasisPriceCurve>(asof, basisData, basisFec, index, basePts,
            baseFec, config.addBasis(), config.monthOffset());
    }

    LOG("CommodityCurve: finished building commodity basis curve.");
}

// Allow for more readable code in method below.
template<class I> using Crv = QuantExt::PiecewisePriceCurve<I, QuantExt::IterativeBootstrap>;
template<class C> using BS = QuantExt::IterativeBootstrap<C>;

void CommodityCurve::buildPiecewiseCurve(const Date& asof, const CommodityCurveConfig& config,
    const Conventions& conventions, const Loader& loader,
    const map<string, boost::shared_ptr<CommodityCurve>>& commodityCurves) {

    LOG("CommodityCurve: start building commodity piecewise curve.");

    // We store the instruments in a map. The key is the instrument's pillar date. The segments are ordered in 
    // priority so if we encounter the same pillar date later, we ignore it with a debug log.
    map<Date, boost::shared_ptr<Helper>> mpInstruments;
    const auto& priceSegments = config.priceSegments();
    QL_REQUIRE(!priceSegments.empty(), "CommodityCurve: need at least one price segment to build piecewise curve.");
    for (const auto& kv : priceSegments) {
        addInstruments(asof, loader, config.curveID(), config.currency(), kv.second,
            conventions, commodityCurves, mpInstruments);
    }

    // Populate the vector of helpers.
    vector<boost::shared_ptr<Helper>> instruments;
    instruments.reserve(mpInstruments.size());
    for (const auto& kv : mpInstruments) {
        instruments.push_back(kv.second);
    }

    // Use bootstrap configuration if provided.
    BootstrapConfig bc;
    if (config.bootstrapConfig()) {
        bc = *config.bootstrapConfig();
    }
    Real acc = bc.accuracy();
    Real globalAcc = bc.globalAccuracy();
    bool noThrow = bc.dontThrow();
    Size maxAttempts = bc.maxAttempts();
    Real maxF = bc.maxFactor();
    Real minF = bc.minFactor();
    Size noThrowSteps = bc.dontThrowSteps();

    // Create curve based on interpolation method provided.
    Currency ccy = parseCurrency(config.currency());
    if (interpolationMethod_ == "Linear") {
        BS<Crv<Linear>> bs(acc, globalAcc, noThrow, maxAttempts, maxF, minF, noThrowSteps);
        commodityPriceCurve_ = boost::make_shared<Crv<Linear>>(asof, instruments, dayCounter_, ccy, Linear(), bs);
    } else if (interpolationMethod_ == "LogLinear") {
        BS<Crv<QuantLib::LogLinear>> bs(acc, globalAcc, noThrow, maxAttempts, maxF, minF, noThrowSteps);
        commodityPriceCurve_ = boost::make_shared<Crv<QuantLib::LogLinear>>(asof, instruments,
            dayCounter_, ccy, QuantLib::LogLinear(), bs);
    } else if (interpolationMethod_ == "Cubic") {
        BS<Crv<QuantLib::Cubic>> bs(acc, globalAcc, noThrow, maxAttempts, maxF, minF, noThrowSteps);
        commodityPriceCurve_ = boost::make_shared<Crv<QuantLib::Cubic>>(asof, instruments,
            dayCounter_, ccy, QuantLib::Cubic(), bs);
    } else if (interpolationMethod_ == "LinearFlat") {
        BS<Crv<QuantExt::LinearFlat>> bs(acc, globalAcc, noThrow, maxAttempts, maxF, minF, noThrowSteps);
        commodityPriceCurve_ = boost::make_shared<Crv<QuantExt::LinearFlat>>(asof, instruments,
            dayCounter_, ccy, QuantExt::LinearFlat(), bs);
    } else if (interpolationMethod_ == "LogLinearFlat") {
        BS<Crv<QuantExt::LogLinearFlat>> bs(acc, globalAcc, noThrow, maxAttempts, maxF, minF, noThrowSteps);
        commodityPriceCurve_ = boost::make_shared<Crv<QuantExt::LogLinearFlat>>(asof, instruments,
            dayCounter_, ccy, QuantExt::LogLinearFlat(), bs);
    } else if (interpolationMethod_ == "CubicFlat") {
        BS<Crv<QuantExt::CubicFlat>> bs(acc, globalAcc, noThrow, maxAttempts, maxF, minF, noThrowSteps);
        commodityPriceCurve_ = boost::make_shared<Crv<QuantExt::CubicFlat>>(asof, instruments,
            dayCounter_, ccy, QuantExt::CubicFlat(), bs);
    } else if (interpolationMethod_ == "BackwardFlat") {
        BS<Crv<BackwardFlat>> bs(acc, globalAcc, noThrow, maxAttempts, maxF, minF, noThrowSteps);
        commodityPriceCurve_ = boost::make_shared<Crv<BackwardFlat>>(asof, instruments,
            dayCounter_, ccy, BackwardFlat(), bs);
    } else {
        QL_FAIL("The interpolation method, " << interpolationMethod_ << ", is not supported.");
    }

    LOG("CommodityCurve: finished building commodity piecewise curve.");
}

vector<boost::shared_ptr<CommodityForwardQuote>>
CommodityCurve::getQuotes(const Date& asof, const string& configId, const vector<string>& quotes,
    const Loader& loader) {

    LOG("CommodityCurve: start getting configured commodity quotes.");

    // Check if we are using a regular expression to select the quotes for the curve. If we are, the quotes should
    // contain exactly one element.
    regexQuotes_ = false;
    for (Size i = 0; i < quotes.size(); i++) {
        if ((regexQuotes_ = quotes[i].find("*") != string::npos)) {
            QL_REQUIRE(i == 0 && quotes.size() == 1,
                       "Wild card config, " << configId << ", should have exactly one quote.");
            DLOG("Regular expression, '" << quotes[0] << "', specified for forward quotes"
                                         << " for commodity curve " << configId);
            break;
        }
    }

    // If we find a regex in forward quotes, check there is only one and initialise the regex
    regex reg;
    if (regexQuotes_)
        reg = regex(boost::replace_all_copy(quotes[0], "*", ".*"));

    // Add the relevant forward quotes to the result vector
    vector<boost::shared_ptr<CommodityForwardQuote>> result;
    for (auto& md : loader.loadQuotes(asof)) {

        // Only looking for quotes on asof date, with quote type PRICE and instrument type commodity forward
        if (md->asofDate() == asof && md->quoteType() == MarketDatum::QuoteType::PRICE &&
            md->instrumentType() == MarketDatum::InstrumentType::COMMODITY_FWD) {

            boost::shared_ptr<CommodityForwardQuote> q = boost::dynamic_pointer_cast<CommodityForwardQuote>(md);

            // Check if the quote is requested by the config and if it isn't continue to the next quote
            if (!regexQuotes_) {
                vector<string>::const_iterator it =
                    find(quotes.begin(), quotes.end(), q->name());
                if (it == quotes.end())
                    continue;
            } else {
                if (!regex_match(q->name(), reg))
                    continue;
            }

            // If we make it here, the quote is relevant.
            result.push_back(q);
            TLOG("Added quote " << q->name() << ".");
        }
    }

    LOG("CommodityCurve: finished getting configured commodity quotes.");

    return result;
}

void CommodityCurve::addInstruments(const Date& asof, const Loader& loader, const string& configId,
    const string& currency, const PriceSegment& priceSegment, const Conventions& conventions,
    const map<string, boost::shared_ptr<CommodityCurve>>& commodityCurves,
    map<Date, boost::shared_ptr<Helper>>& instruments) {

    // Get the relevant quotes
    auto quotes = getQuotes(asof, configId, priceSegment.quotes(), loader);

    using PST = PriceSegment::Type;
    using AD = CommodityFutureConvention::AveragingData;
    PST type = priceSegment.type();

    // Pre-populate some variables if averaging segment.
    boost::shared_ptr<CommodityFutureConvention> convention;
    AD ad;
    boost::shared_ptr<CommodityIndex> index;
    boost::shared_ptr<FutureExpiryCalculator> uFec;
    if (type == PST::AveragingFuture || type == PST::AveragingSpot || type == PST::AveragingOffPeakPower) {

        // Get the associated averaging commodity future convention.
        convention = boost::dynamic_pointer_cast<CommodityFutureConvention>(
            conventions.get(priceSegment.conventionsId()));
        QL_REQUIRE(convention, "Convention " << priceSegment.conventionsId() <<
            " not of expected type CommodityFutureConvention.");

        ad = convention->averagingData();
        QL_REQUIRE(!ad.empty(), "CommodityCurve: convention " << convention->id() <<
            " should have non-empty averaging data for piecewise price curve construction.");

        // The commodity index for which we are building a price curve.
        index = parseCommodityIndex(ad.commodityName(), conventions, false);

        // If referencing a future, we need conventions for the underlying future that is being averaged.
        if (type == PST::AveragingFuture || type == PST::AveragingOffPeakPower) {

            auto uConvention = boost::dynamic_pointer_cast<CommodityFutureConvention>(
                conventions.get(ad.conventionsId()));
            QL_REQUIRE(uConvention, "Convention " << priceSegment.conventionsId() <<
                " not of expected type CommodityFutureConvention.");
            uFec = boost::make_shared<ConventionsBasedFutureExpiry>(*uConvention);

            if (ad.dailyExpiryOffset() != Null<Natural>() && ad.dailyExpiryOffset() > 0) {
                QL_REQUIRE(uConvention->contractFrequency() == Daily, "CommodityCurve: the averaging data has" <<
                    " a positive DailyExpiryOffset (" << ad.dailyExpiryOffset() << ") but the underlying future" <<
                    " contract frequency is not daily (" << uConvention->contractFrequency() << ").");
            }
        }

    }

    // Pre-populate some variables if the price segment is AveragingOffPeakPower.
    boost::shared_ptr<CommodityIndex> peakIndex;
    Natural peakHoursPerDay = 16;
    Calendar peakCalendar;
    if (type == PST::AveragingOffPeakPower) {

        // Look up the peak price curve in the commodityCurves map
        const string& ppId = priceSegment.peakPriceCurveId();
        QL_REQUIRE(!ppId.empty(), "CommodityCurve: AveragingOffPeakPower segment in " <<
            " curve configuration " << configId << " does not provide a peak price curve ID.");
        CommodityCurveSpec ccSpec(currency, ppId);
        DLOG("Looking for peak price curve with id, " << ppId << ", and spec, " << ccSpec << ".");
        auto itCc = commodityCurves.find(ccSpec.name());
        QL_REQUIRE(itCc != commodityCurves.end(), "Can't find peak price curve with id " << ppId);
        auto peakPts = Handle<PriceTermStructure>(itCc->second->commodityPriceCurve());

        // Create the daily peak price index linked to the peak price term structure.
        peakIndex = parseCommodityIndex(ppId, conventions, false, peakPts);

        // Calendar defining the peak business days.
        peakCalendar = parseCalendar(priceSegment.peakPriceCalendar());

        // Look up the conventions for the peak price commodity to determine peak hours per day.
        if (conventions.has(ppId)) {
            auto peakConvention = boost::dynamic_pointer_cast<CommodityFutureConvention>(conventions.get(ppId));
            if (peakConvention && peakConvention->hoursPerDay() != Null<Natural>()) {
                peakHoursPerDay = peakConvention->hoursPerDay();
            }
        }
    }

    // Add an instrument for each relevant quote.
    for (const auto& quote : quotes) {

        if (quote->tenorBased()) {
            TLOG("Skipping tenor based quote, " << quote->name() << ".");
            continue;
        }

        const Date& expiry = quote->expiryDate();
        if (expiry < asof) {
            TLOG("Skipping quote, " << quote->name() << ", because its expiry date, " << io::iso_date(expiry)
                << ", is before the market date " << io::iso_date(asof));
            continue;
        }

        switch (type) {
        case PST::Future:
            if (instruments.count(expiry) == 0) {
                instruments[expiry] = boost::make_shared<FuturePriceHelper>(quote->quote(), expiry);
            } else {
                TLOG("Skipping quote, " << quote->name() << ", because its expiry date, " <<
                    io::iso_date(expiry) << ", is already in the instrument set.");
            }
            break;

        // An averaging future referencing an underlying future or spot. Setup is similar.
        case PST::AveragingFuture:
        case PST::AveragingSpot:
        case PST::AveragingOffPeakPower: {

            // Determine the calculation period.
            using ADCP = AD::CalculationPeriod;
            Date start;
            Date end;
            if (ad.period() == ADCP::ExpiryToExpiry) {

                auto fec = boost::make_shared<ConventionsBasedFutureExpiry>(*convention);
                end = fec->nextExpiry(true, expiry);
                if (end != expiry) {
                    WLOG("Calculated expiry date, " << io::iso_date(end) << ", does not equal quote's expiry date "
                        << io::iso_date(expiry) << ". Proceed with quote's expiry.");
                }
                start = fec->priorExpiry(false, end) + 1;

            } else if (ad.period() == ADCP::PreviousMonth) {

                end = Date::endOfMonth(expiry - 1 * Months);
                start = Date(1, end.month(), end.year());
            }

            boost::shared_ptr<Helper> helper;
            if (type == PST::AveragingOffPeakPower) {
                TLOG("Building average off-peak power helper from quote, " << quote->name() << ".");
                helper = boost::make_shared<AverageOffPeakPowerHelper>(quote->quote(), index, start,
                    end, uFec, peakIndex, peakCalendar, peakHoursPerDay);
            } else {
                TLOG("Building average future price helper from quote, " << quote->name() << ".");
                helper = boost::make_shared<AverageFuturePriceHelper>(quote->quote(), index, start, end, uFec,
                    ad.pricingCalendar(), ad.deliveryRollDays(), ad.futureMonthOffset(), ad.useBusinessDays(),
                    ad.dailyExpiryOffset());
            }

            // Only add to instruments if an instrument with the same pillar date is not there already.
            Date pillar = helper->pillarDate();
            if (instruments.count(pillar) == 0) {
                instruments[pillar] = helper;
            } else {
                TLOG("Skipping quote, " << quote->name() << ", because an instrument with its pillar date, " <<
                    io::iso_date(pillar) << ", is already in the instrument set.");
            }
            break;
        }

        default:
            QL_FAIL("CommodityCurve: unrecognised price segment type.");
            break;
        }
    }
}

} // namespace data
} // namespace ore
