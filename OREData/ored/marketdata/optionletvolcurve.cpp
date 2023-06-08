/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

#include <ored/marketdata/optionletvolcurve.hpp>
#include <ored/marketdata/marketdatumparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/to_string.hpp>

#include <qle/math/flatextrapolation.hpp>
#include <qle/models/carrmadanarbitragecheck.hpp>
#include <qle/termstructures/capfloortermvolsurface.hpp>
#include <qle/termstructures/capfloortermvolsurfacesparse.hpp>
#include <qle/termstructures/datedstrippedoptionlet.hpp>
#include <qle/termstructures/datedstrippedoptionletadapter.hpp>
#include <qle/termstructures/optionletstripper1.hpp>
#include <qle/termstructures/optionletstripper2.hpp>
#include <qle/termstructures/optionletstripperwithatm.hpp>
#include <qle/termstructures/piecewiseatmoptionletcurve.hpp>
#include <qle/termstructures/piecewiseoptionletstripper.hpp>
#include <qle/termstructures/strippedoptionletadapter.hpp>
#include <qle/interpolators/optioninterpolator2d.hpp>
#include <qle/instruments/makeoiscapfloor.hpp>

#include <ql/math/comparison.hpp>
#include <ql/math/matrix.hpp>
#include <ql/termstructures/volatility/optionlet/strippedoptionletadapter.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

typedef ore::data::OptionletVolatilityCurveConfig::VolatilityType OfgVolType;
typedef ore::data::OptionletVolatilityCurveConfig::Type OfgType;
typedef QuantExt::CapFloorTermVolSurfaceExact::InterpolationMethod CftvsInterp;

namespace ore {
namespace data {

OptionletVolCurve::OptionletVolCurve(
    const Date& asof, const OptionletVolatilityCurveSpec& spec, const Loader& loader,
    const CurveConfigurations& curveConfigs, boost::shared_ptr<IborIndex> iborIndex,
    Handle<YieldTermStructure> discountCurve,
    const std::map<std::string, std::pair<boost::shared_ptr<ore::data::OptionletVolCurve>,
                                          std::pair<std::string, QuantLib::Period>>>& requiredOptionletVolCurves,
    const bool buildCalibrationInfo)
    : spec_(spec) {

    try {
        // The configuration
        const boost::shared_ptr<OptionletVolatilityCurveConfig>& config =
            curveConfigs.optionletVolCurveConfig(spec_.curveConfigID());

        QL_REQUIRE(config->volatilityType() != OfgVolType::ShiftedLognormal,
                   "ShiftedLognormal vol type is not supported.");

        // Only support surface optionlet vol curve configuration
        QL_REQUIRE(config->type() == OfgType::Surface, "For optionlet vol curve config, ATM quotes are not supported.");
        optSurface(asof, *config, loader, iborIndex, discountCurve);

        // Turn on or off extrapolation
        capletVol_->enableExtrapolation(config->extrapolate());

    } catch (exception& e) {
        QL_FAIL("optionlet vol curve building failed :" << e.what());
    } catch (...) {
        QL_FAIL("optionlet vol curve building failed: unknown error");
    }

    // force bootstrap so that errors are thrown during the build, not later
    capletVol_->volatility(QL_EPSILON, capletVol_->minStrike());
}

void OptionletVolCurve::optSurface(const Date& asof, OptionletVolatilityCurveConfig& config, const Loader& loader,
                                  boost::shared_ptr<IborIndex> index, Handle<YieldTermStructure> discountCurve) {

    // Load optionlet vol surface
    // Map to store the quote values that we load with key (period, strike) where strike
    // needs a custom comparator to avoid == with double
    auto comp = [](const pair<Period, Rate>& a, const pair<Period, Rate>& b) {
        return (a.first < b.first) || (!(b.first < a.first) && (a.second < b.second && !close(a.second, b.second)));
    };
    map<pair<Period, Rate>, Real, decltype(comp)> volQuotes(comp);

    bool optionalQuotes = config.optionalQuotes();
    QL_REQUIRE(optionalQuotes == true, "Optional quotes for optionlet volatilities are not supported.");
    Size quoteCounter = 0;
    bool quoteRelevant = false;
    bool tenorRelevant = false;
    bool strikeRelevant = false;

    vector<Real> quoteStrikes;
    vector<Real> quoteData;
    vector<Period> quoteTenors;
    Period tenor = parsePeriod(config.indexTenor()); // 1D, 1M, 3M, 6M, 12M
    string currency = config.currency();
    vector<Period> configTenors = parseVectorOfValues<Period>(config.tenors(), &parsePeriod);

    std::ostringstream ss;
    ss << MarketDatum::InstrumentType::OPTIONLET << "/" << config.quoteType() << "/" << currency << "/";
    if (config.quoteIncludesIndexName())
        ss << config.index() << "/";
    ss << "*";
    Wildcard w(ss.str());
    for (const auto& md : loader.get(w, asof)) {
        QL_REQUIRE(md->asofDate() == asof, "MarketDatum asofDate '" << md->asofDate() << "' <> asof '" << asof << "'");
        boost::shared_ptr<OptionletQuote> ofq = boost::dynamic_pointer_cast<OptionletQuote>(md);
        QL_REQUIRE(ofq, "Internal error: could not downcast MarketDatum '" << md->name() << "' to OptionletQuote");
        QL_REQUIRE(ofq->ccy() == currency,
                   "OptionletQuote ccy '" << ofq->ccy() << "' <> config ccy '" << currency << "'");
        if (ofq->underlying() == tenor && !ofq->atm()) {
            auto j = std::find(configTenors.begin(), configTenors.end(), ofq->term());
            tenorRelevant = j != configTenors.end();

            Real strike = ofq->strike();
            auto i = std::find_if(config.strikes().begin(), config.strikes().end(),
                                  [&strike](const std::string& x) { return close_enough(parseReal(x), strike); });
            strikeRelevant = i != config.strikes().end();

            quoteRelevant = strikeRelevant && tenorRelevant;

            if (quoteRelevant) {
                // if we have optional quotes we just make vectors of all quotes and let the sparse surface
                // handle them
                quoteCounter++;
                if (optionalQuotes) {
                    quoteStrikes.push_back(ofq->strike());
                    quoteTenors.push_back(ofq->term());
                    quoteData.push_back(ofq->quote()->value());
                }
                auto key = make_pair(ofq->term(), ofq->strike());
                auto r = volQuotes.insert(make_pair(key, ofq->quote()->value()));
                if (config.quoteIncludesIndexName())
                    QL_REQUIRE(r.second, "Duplicate optionlet vol quote in config "
                                             << config.curveID() << ", with underlying tenor " << tenor << " ,currency "
                                             << currency << " and index " << config.index() << ", for tenor "
                                             << key.first << " and strike " << key.second);
                else
                    QL_REQUIRE(r.second, "Duplicate optionlet vol quote in config "
                                             << config.curveID() << ", with underlying tenor " << tenor
                                             << " and currency " << currency << ", for tenor " << key.first
                                             << " and strike " << key.second);
            }
        }
    }

    Size totalQuotes = config.tenors().size() * config.strikes().size();
    if (quoteCounter < totalQuotes) {
        WLOG("Found only " << quoteCounter << " out of " << totalQuotes << " quotes for Optionlet vol surface "
                           << config.curveID());
    }

    vector<Period> tenors = parseVectorOfValues<Period>(config.tenors(), &parsePeriod);
    vector<Rate> strikes = parseVectorOfValues<Real>(config.strikes(), &parseReal);
    vector<Date> dates;
    for (Size i = 0; i < tenors.size(); i++) {
        dates.push_back(asof + tenors[i]);
    }
    Matrix vols(tenors.size(), strikes.size());
    vector<vector<Handle<Quote>>> vols_vec;
    vector<Handle<Quote>> vols_tenor;
    for (Size i = 0; i < tenors.size(); i++) {
        for (Size j = 0; j < strikes.size(); j++) {
            auto key = make_pair(tenors[i], strikes[j]);
            auto it = volQuotes.find(key);
            if (!optionalQuotes) {
                QL_REQUIRE(it != volQuotes.end(), "Quote with tenor " << key.first << " and strike " << key.second
                                                                      << " not loaded for optionlet vol config "
                                                                      << config.curveID());
                // Organise the values in to a square matrix
                vols[i][j] = it->second;
                vols_tenor.push_back(Handle<Quote>(boost::make_shared<SimpleQuote>(it->second)));
            } else {
                if (it == volQuotes.end()) {
                    DLOG("Could not find quote with tenor " << key.first << " and strike " << key.second
                                                            << " for optionlet vol config " << config.curveID());
                }
            }
        }
        vols_vec.push_back(vols_tenor);
        vols_tenor.clear();
    }
    std::cout << "vols_vec.size() = " <<  vols_vec.size() << std::endl;
    std::cout << "dates.size() = " << dates.size() << std::endl;

    DLOG("Found " << quoteCounter << " quotes for optionlet vol surface " << config.curveID());
    boost::shared_ptr<StrippedOptionlet> optionletSurface;
    if (optionalQuotes) {
        QL_REQUIRE(quoteCounter > 0, "No Quotes provided for Optionlet vol surface " << config.curveID());
        /*
        if (config.interpolationMethod() == CapFloorTermVolSurfaceExact::Bilinear) {
            return boost::make_shared<QuantExt::CapFloorTermVolSurfaceSparse<Linear, Linear>>(
                config.settleDays(), config.calendar(), config.businessDayConvention(), config.dayCounter(), qtTenors,
                qtStrikes, qtData, true, true);
        } else if (config.interpolationMethod() == CapFloorTermVolSurfaceExact::BicubicSpline) {
            return boost::make_shared<QuantExt::CapFloorTermVolSurfaceSparse<Cubic, Cubic>>(
                config.settleDays(), config.calendar(), config.businessDayConvention(), config.dayCounter(), qtTenors,
                qtStrikes, qtData, true, true);
        } else {
            QL_FAIL("Invalid Interpolation method for capfloor surface "
                    << config.curveID() << ", must be either " << CapFloorTermVolSurfaceExact::Bilinear << " or "
                    << CapFloorTermVolSurfaceExact::BicubicSpline << ".");
        }
        */
    } else {
        // Return for the cap floor term volatility surface
        optionletSurface = boost::make_shared<StrippedOptionlet>(
            config.settleDays(), config.calendar(), config.businessDayConvention(), index, dates, strikes,
            vols_vec, config.dayCounter(), QuantLib::VolatilityType::Normal);
    }

    // This is not pretty but can't think of a better way (with template functions and or classes)
    if (config.timeInterpolation() == "Linear") {
        if (config.strikeInterpolation() == "Linear") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Linear, Linear>>(
                asof, optionletSurface);
        } else if (config.strikeInterpolation() == "LinearFlat") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Linear, LinearFlat>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "Cubic") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Linear, Cubic>>(
                asof, optionletSurface);
        } else if (config.strikeInterpolation() == "CubicFlat") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Linear, CubicFlat>>(
                asof, optionletSurface);
        } else {
            QL_FAIL("Optionlet vol config " << config.curveID() << " has unexpected strike interpolation "
                                        << config.strikeInterpolation());
        }
    } else if (config.timeInterpolation() == "LinearFlat") {
        if (config.strikeInterpolation() == "Linear") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, Linear>>(
                asof, optionletSurface);
        } else if (config.strikeInterpolation() == "LinearFlat") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, LinearFlat>>(
                asof, optionletSurface);
        } else if (config.strikeInterpolation() == "Cubic") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, Cubic>>(
                asof, optionletSurface);
        } else if (config.strikeInterpolation() == "CubicFlat") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<LinearFlat, CubicFlat>>(
                asof, optionletSurface);
        } else {
            QL_FAIL("Optionlet vol config " << config.curveID() << " has unexpected strike interpolation "
                                        << config.strikeInterpolation());
        }
    } else if (config.timeInterpolation() == "BackwardFlat") {
        if (config.strikeInterpolation() == "Linear") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, Linear>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "LinearFlat") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, LinearFlat>>(
                asof, optionletSurface);
        } else if (config.strikeInterpolation() == "Cubic") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, Cubic>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "CubicFlat") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<BackwardFlat, CubicFlat>>(asof, optionletSurface);
        } else {
            QL_FAIL("Optionlet vol config " << config.curveID() << " has unexpected strike interpolation "
                                        << config.strikeInterpolation());
        }
    } else if (config.timeInterpolation() == "Cubic") {
        if (config.strikeInterpolation() == "Linear") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, Linear>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "LinearFlat") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, LinearFlat>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "Cubic") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, Cubic>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "CubicFlat") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<Cubic, CubicFlat>>(asof, optionletSurface);
        } else {
            QL_FAIL("Optionlet vol config " << config.curveID() << " has unexpected strike interpolation "
                                        << config.strikeInterpolation());
        }
    } else if (config.timeInterpolation() == "CubicFlat") {
        if (config.strikeInterpolation() == "Linear") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, Linear>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "LinearFlat") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, LinearFlat>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "Cubic") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, Cubic>>(asof, optionletSurface);
        } else if (config.strikeInterpolation() == "CubicFlat") {
            capletVol_ = boost::make_shared<QuantExt::StrippedOptionletAdapter<CubicFlat, CubicFlat>>(asof, optionletSurface);
        } else {
            QL_FAIL("Optionlet vol config " << config.curveID() << " has unexpected strike interpolation "
                                        << config.strikeInterpolation());
        }
    } else {
        QL_FAIL("Optionlet vol config " << config.curveID() << " has unexpected time interpolation "
                                    << config.timeInterpolation());
    }
}

} // namespace data
} // namespace ore
