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

#include <ored/marketdata/inflationcapfloorpricesurface.hpp>

#include <ored/utilities/indexparser.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>

#include <qle/indexes/inflationindexwrapper.hpp>
#include <ql/math/interpolations/bilinearinterpolation.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/experimental/inflation/yoycapfloortermpricesurface.hpp>
#include <ql/experimental/inflation/interpolatedyoyoptionletstripper.hpp>

#include <algorithm>

using namespace QuantLib;
using namespace std;
using namespace ore::data;

namespace ore {
namespace data {

InflationCapFloorPriceSurface::InflationCapFloorPriceSurface(
    Date asof, InflationCapFloorPriceSurfaceSpec spec, const Loader& loader, const CurveConfigurations& curveConfigs,
    map<string, boost::shared_ptr<YieldCurve>>& yieldCurves,
    map<string, boost::shared_ptr<InflationCurve>>& inflationCurves) {

    try {

        const boost::shared_ptr<InflationCapFloorPriceSurfaceConfig>& config =
            curveConfigs.inflationCapFloorPriceSurfaceConfig(spec.curveConfigID());

        QL_REQUIRE((config->type() == InflationCapFloorPriceSurfaceConfig::Type::ZC) || 
                   (config->type() == InflationCapFloorPriceSurfaceConfig::Type::YY),
                   "Inflation cap floor price surfaces must be of type 'ZC' or 'YY'");

        Handle<YieldTermStructure> yts;
        auto it = yieldCurves.find(config->yieldTermStructure());
        if (it != yieldCurves.end()) {
            yts = it->second->handle();
        } else {
            QL_FAIL("The nominal term structure, " << config->yieldTermStructure()
                                                   << ", required in the building "
                                                      "of the curve, "
                                                   << spec.name() << ", was not found.");
        }

        const std::vector<Period>& terms = config->maturities();
        std::vector<Real> capStrikes = config->capStrikes();
        std::vector<Real> floorStrikes = config->floorStrikes();

        Matrix cPrice(capStrikes.size(), capStrikes.size() == 0 ? 0 : terms.size(), Null<Real>()),
            fPrice(floorStrikes.size(), floorStrikes.size() == 0 ? 0 : terms.size(), Null<Real>());
        
        // We loop over all market data, looking for quotes that match the configuration
        for (auto& md : loader.loadQuotes(asof)) {

            if (md->asofDate() == asof && (md->instrumentType() == MarketDatum::InstrumentType::ZC_INFLATIONCAPFLOOR ||
                                           md->instrumentType() == MarketDatum::InstrumentType::YY_INFLATIONCAPFLOOR)) {

                boost::shared_ptr<InflationCapFloorQuote> q;
                    
                if (config->type() == InflationCapFloorPriceSurfaceConfig::Type::ZC) {
                    q = boost::dynamic_pointer_cast<ZcInflationCapFloorQuote>(md);
                }
                else {
                    q = boost::dynamic_pointer_cast<YyInflationCapFloorQuote>(md);
                }

                if (q != NULL && q->index() == spec.index()) {
                    auto it1 = std::find(terms.begin(), terms.end(), q->term());
                    Real strike = parseReal(q->strike());
                    Size strikeIdx = Null<Size>();
                    if (q->isCap()) {
                        for (Size i = 0; i < capStrikes.size(); ++i) {
                            if (close_enough(capStrikes[i], strike))
                                strikeIdx = i;
                        }
                    }
                    else {
                        for (Size i = 0; i < floorStrikes.size(); ++i) {
                            if (close_enough(floorStrikes[i], strike))
                                strikeIdx = i;
                        }
                    }
                    if (it1 != terms.end() && strikeIdx != Null<Size>()) {
                        if (q->isCap()) {
                            cPrice[strikeIdx][it1 - terms.begin()] = q->quote()->value();
                        }
                        else {
                            fPrice[strikeIdx][it1 - terms.begin()] = q->quote()->value();
                        }
                    }
                }               
            }
        }

        for (Size j = 0; j < terms.size(); ++j) {
            for (Size i = 0; i < capStrikes.size(); ++i)
                QL_REQUIRE(cPrice[i][j] != Null<Real>(), "quote for cap floor price surface, type cap, strike "
                                                             << capStrikes[i] << ", term " << terms[j]
                                                             << ", not found.");
            for (Size i = 0; i < floorStrikes.size(); ++i)
                QL_REQUIRE(fPrice[i][j] != Null<Real>(), "quote for cap floor price surface, type floor, strike "
                                                             << floorStrikes[i] << ", term " << terms[j]
                                                             << ", not found.");
        }

        // The strike grids have some minimum requirements which we fulfill here at
        // least technically; note that the extrapolated prices will not be sensible,
        // instead only the given strikes for the given option type may be sensible
        // in the end

        static const Real largeStrike = 1.0;
        static const Real largeStrikeFactor = 0.99;

        Size addFloor = 0, addCap = 0;
        if (floorStrikes.size() == 0) {
            floorStrikes.push_back(-largeStrike);
            floorStrikes.push_back(-(largeStrike * largeStrikeFactor));
            addFloor = 2;
        }
        if (floorStrikes.size() == 1) {
            floorStrikes.insert(floorStrikes.begin(), -largeStrike);
            addFloor = 1;
        }
        if (capStrikes.size() == 0) {
            capStrikes.push_back(largeStrike * largeStrikeFactor);
            capStrikes.push_back(largeStrike);
            addCap = 2;
        }
        if (capStrikes.size() == 1) {
            floorStrikes.push_back(largeStrike);
            addCap = 1;
        }
        if (addFloor > 0) {
            Matrix tmp(fPrice.rows() + addFloor, terms.size(), 1E-10);
            for (Size i = addFloor; i < fPrice.rows() + addFloor; ++i) {
                for (Size j = 0; j < fPrice.columns(); ++j) {
                    tmp[i][j] = fPrice[i - addFloor][j];
                }
            }
            fPrice = tmp;
        }
        if (addCap > 0) {
            Matrix tmp(cPrice.rows() + addCap, terms.size(), 1E-10);
            for (Size i = 0; i < cPrice.rows(); ++i) {
                for (Size j = 0; j < cPrice.columns(); ++j) {
                    tmp[i][j] = cPrice[i][j];
                }
            }
            cPrice = tmp;
        }

        ostringstream capStrikesString, floorStrikesString;
        for (Size i = 0; i < floorStrikes.size(); ++i)
            floorStrikesString << floorStrikes[i] << ",";
        for (Size i = 0; i < capStrikes.size(); ++i)
            capStrikesString << capStrikes[i] << ",";
        DLOG("Building inflation cap floor price surface:");
        DLOG("Cap Strikes are: " << capStrikesString.str());
        DLOG("Floor Strikes are: " << floorStrikesString.str());
        DLOGGERSTREAM << "Cap Price Matrix:\n" << cPrice << "Floor Price Matrix:\n" << fPrice;


        if (config->type() == InflationCapFloorPriceSurfaceConfig::Type::ZC) {
            // ZC Curve

            boost::shared_ptr<ZeroInflationIndex> index;
            auto it2 = inflationCurves.find(config->indexCurve());
            if (it2 != inflationCurves.end()) {
                boost::shared_ptr<ZeroInflationTermStructure> ts =
                    boost::dynamic_pointer_cast<ZeroInflationTermStructure>(it2->second->inflationTermStructure());
                QL_REQUIRE(ts,
                    "inflation term structure " << config->indexCurve() << " was expected to be zero, but is not");
                index = parseZeroInflationIndex(config->index(), it2->second->interpolatedIndex(),
                    Handle<ZeroInflationTermStructure>(ts));
            }
            else {
                QL_FAIL("The zero inflation curve, " << config->indexCurve()
                    << ", required in building the inflation cap floor price surface "
                    << spec.name() << ", was not found");
            }
            // Build the term structure
            surface_ = boost::shared_ptr<InterpolatedCPICapFloorTermPriceSurface<QuantLib::Bilinear>>(
                new InterpolatedCPICapFloorTermPriceSurface<QuantLib::Bilinear>(
                    1.0, config->startRate(), config->observationLag(), config->calendar(), config->businessDayConvention(),
                    config->dayCounter(), Handle<ZeroInflationIndex>(index), yts, capStrikes, floorStrikes, terms, cPrice,
                    fPrice));

        }
        if (config->type() == InflationCapFloorPriceSurfaceConfig::Type::YY) {

            boost::shared_ptr<YoYInflationIndex> index;
            auto it2 = inflationCurves.find(config->indexCurve());
            if (it2 != inflationCurves.end()) {
                boost::shared_ptr<YoYInflationTermStructure> ts =
                    boost::dynamic_pointer_cast<YoYInflationTermStructure>(it2->second->inflationTermStructure());
                QL_REQUIRE(ts,
                    "inflation term structure " << config->indexCurve() << " was expected to be yoy, but is not");
                bool interp = it2->second->interpolatedIndex();
                index = boost::make_shared<QuantExt::YoYInflationIndexWrapper>(
                    parseZeroInflationIndex(config->index(), interp), interp, Handle<YoYInflationTermStructure>(ts));
            }
            else {
                QL_FAIL("The yoy inflation curve, " << config->indexCurve()
                    << ", required in building the inflation cap floor price surface "
                    << spec.name() << ", was not found");
            }
            // Build the term structure
            boost::shared_ptr<InterpolatedYoYCapFloorTermPriceSurface<QuantLib::Bilinear, QuantLib::Linear >> yoySurface =
                boost::make_shared<InterpolatedYoYCapFloorTermPriceSurface<QuantLib::Bilinear, QuantLib::Linear>>(
                    0, config->observationLag(), index, config->startRate(), yts, config->dayCounter(), config->calendar(), 
                    config->businessDayConvention(), capStrikes, floorStrikes, terms, cPrice, fPrice);
            surface_ = yoySurface;

            boost::shared_ptr<InterpolatedYoYOptionletStripper<QuantLib::Linear>> yoyStripper = 
                boost::make_shared<InterpolatedYoYOptionletStripper<QuantLib::Linear>>();
            boost::shared_ptr<QuantExt::YoYOptionletVolatilitySurface> ovs = boost::make_shared<QuantExt::YoYOptionletVolatilitySurface>(
                yoySurface->settlementDays(), yoySurface->calendar(), yoySurface->businessDayConvention(), yoySurface->dayCounter(), 
                yoySurface->observationLag(), yoySurface->frequency(), yoySurface->indexIsInterpolated());
            Handle<QuantLib::YoYOptionletVolatilitySurface> hovs(ovs);

            boost::shared_ptr<YoYInflationBachelierCapFloorEngine> cfEngine = 
                boost::make_shared<YoYInflationBachelierCapFloorEngine>(yoySurface->yoyIndex(), hovs);
            
            yoyStripper->initialize(yoySurface, cfEngine, 1);

        }


    } catch (std::exception& e) {
        QL_FAIL("inflation cap floor price surface building failed: " << e.what());
    } catch (...) {
        QL_FAIL("inflation cap floor price surface building failed: unknown error");
    }
}
} // namespace data
} // namespace ore
