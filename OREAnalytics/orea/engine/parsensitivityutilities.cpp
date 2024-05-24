/*
 Copyright (C) 2024 AcadiaSoft Inc.
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

#include <orea/engine/parsensitivityinstrumentbuilder.hpp>
#include <orea/engine/parsensitivityutilities.hpp>
#include <ored/utilities/log.hpp>
#include <ql/instruments/creditdefaultswap.hpp>
#include <ql/instruments/forwardrateagreement.hpp>
#include <ql/instruments/makecapfloor.hpp>
#include <ql/instruments/makeois.hpp>
#include <ql/instruments/makevanillaswap.hpp>
#include <ql/instruments/yearonyearinflationswap.hpp>
#include <ql/instruments/zerocouponinflationswap.hpp>
#include <ql/math/solvers1d/newtonsafe.hpp>
#include <ql/pricingengine.hpp>
#include <ql/pricingengines/capfloor/bacheliercapfloorengine.hpp>
#include <ql/pricingengines/capfloor/blackcapfloorengine.hpp>
#include <ql/quotes/simplequote.hpp>
#include <ql/termstructures/volatility/inflation/yoyinflationoptionletvolatilitystructure.hpp>
#include <qle/instruments/brlcdiswap.hpp>
#include <qle/instruments/crossccybasismtmresetswap.hpp>
#include <qle/instruments/crossccybasisswap.hpp>
#include <qle/instruments/deposit.hpp>
#include <qle/instruments/fixedbmaswap.hpp>
#include <qle/instruments/fxforward.hpp>
#include <qle/instruments/makecds.hpp>
#include <qle/instruments/subperiodsswap.hpp>
#include <qle/instruments/tenorbasisswap.hpp>
#include <qle/pricingengines/inflationcapfloorengines.hpp>

using namespace QuantLib;
using namespace QuantExt;
using namespace std;

namespace ore {
namespace analytics {

namespace {

/* Helper class for implying the fair flat cap/floor volatility
   This class is copied from QuantLib's capfloor.cpp and generalised to cover both normal and lognormal volatilities */
class ImpliedCapFloorVolHelper {
public:
    ImpliedCapFloorVolHelper(
        const QuantLib::Instrument& cap,
        const std::function<QuantLib::ext::shared_ptr<QuantLib::PricingEngine>(const QuantLib::Handle<Quote>)>
            engineGenerator,
        const Real targetValue);
    Real operator()(Volatility x) const;
    Real derivative(Volatility x) const;

private:
    Real targetValue_;
    QuantLib::ext::shared_ptr<PricingEngine> engine_;
    QuantLib::ext::shared_ptr<QuantLib::SimpleQuote> vol_;
    const Instrument::results* results_;
};

ImpliedCapFloorVolHelper::ImpliedCapFloorVolHelper(
    const QuantLib::Instrument& cap,
    const std::function<QuantLib::ext::shared_ptr<PricingEngine>(const Handle<Quote>)> engineGenerator,
    const Real targetValue)
    : targetValue_(targetValue) {
    // set an implausible value, so that calculation is forced
    // at first ImpliedCapFloorVolHelper::operator()(Volatility x) call
    vol_ = QuantLib::ext::shared_ptr<SimpleQuote>(new SimpleQuote(-1));
    engine_ = engineGenerator(Handle<Quote>(vol_));
    cap.setupArguments(engine_->getArguments());
    results_ = dynamic_cast<const Instrument::results*>(engine_->getResults());
}

Real ImpliedCapFloorVolHelper::operator()(Volatility x) const {
    if (x != vol_->value()) {
        vol_->setValue(x);
        engine_->calculate();
    }
    return results_->value - targetValue_;
}

Real ImpliedCapFloorVolHelper::derivative(Volatility x) const {
    if (x != vol_->value()) {
        vol_->setValue(x);
        engine_->calculate();
    }
    std::map<std::string, boost::any>::const_iterator vega_ = results_->additionalResults.find("vega");
    QL_REQUIRE(vega_ != results_->additionalResults.end(), "vega not provided");
    return boost::any_cast<Real>(vega_->second);
}

std::function<QuantLib::ext::shared_ptr<PricingEngine>(const Handle<Quote>&)>
pricingEngineGeneratorFactory(const CapFloor& cap, const Handle<YieldTermStructure>& d, VolatilityType type,
                              Real displacement, const Handle<Index>& index) {
    std::function<QuantLib::ext::shared_ptr<PricingEngine>(const Handle<Quote>)> engineGenerator;
    if (type == ShiftedLognormal)
        engineGenerator = [&d, displacement](const Handle<Quote>& h) {
            return QuantLib::ext::make_shared<BlackCapFloorEngine>(d, h, Actual365Fixed(), displacement);
        };
    else if (type == Normal)
        engineGenerator = [&d](const Handle<Quote>& h) {
            return QuantLib::ext::make_shared<BachelierCapFloorEngine>(d, h, Actual365Fixed());
        };
    else
        QL_FAIL("volatility type " << type << " not implemented");
    return engineGenerator;
}

std::function<QuantLib::ext::shared_ptr<PricingEngine>(const Handle<Quote>&)>
pricingEngineGeneratorFactory(const YoYInflationCapFloor& cap, const Handle<YieldTermStructure>& d, VolatilityType type,
                              Real displacement, const Handle<YoYInflationIndex>& index) {
    std::function<QuantLib::ext::shared_ptr<PricingEngine>(const Handle<Quote>)> engineGenerator;
    if (type == ShiftedLognormal) {
        if (close_enough(displacement, 0.0))
            engineGenerator = [&d, &index](const Handle<Quote>& h) {
                // hardcode A365F as for ir caps, or should we use the dc from the original market vol ts ?
                // calendar, bdc not needed here, settlement days should be zero so that the
                // reference date is = evaluation date
                auto c = Handle<QuantLib::YoYOptionletVolatilitySurface>(
                    QuantLib::ext::make_shared<QuantExt::ConstantYoYOptionletVolatility>(
                        h, 0, NullCalendar(), Unadjusted, Actual365Fixed(),
                        index->yoyInflationTermStructure()->observationLag(), index->frequency(),
                        index->interpolated()));
                return QuantLib::ext::make_shared<QuantExt::YoYInflationBlackCapFloorEngine>(*index, c, d);
            };
        else
            engineGenerator = [&d, &index](const Handle<Quote>& h) {
                auto c = Handle<QuantLib::YoYOptionletVolatilitySurface>(
                    QuantLib::ext::make_shared<QuantExt::ConstantYoYOptionletVolatility>(
                        h, 0, NullCalendar(), Unadjusted, Actual365Fixed(),
                        index->yoyInflationTermStructure()->observationLag(), index->frequency(),
                        index->interpolated()));
                return QuantLib::ext::make_shared<QuantExt::YoYInflationUnitDisplacedBlackCapFloorEngine>(*index, c, d);
            };
    } else if (type == Normal)
        engineGenerator = [&d, &index](const Handle<Quote>& h) {
            auto c = Handle<QuantLib::YoYOptionletVolatilitySurface>(
                QuantLib::ext::make_shared<QuantExt::ConstantYoYOptionletVolatility>(
                    h, 0, NullCalendar(), Unadjusted, Actual365Fixed(),
                    index->yoyInflationTermStructure()->observationLag(), index->frequency(), index->interpolated()));
            return QuantLib::ext::make_shared<QuantExt::YoYInflationBachelierCapFloorEngine>(*index, c, d);
        };
    else
        QL_FAIL("volatility type " << type << " not implemented");
    return engineGenerator;
}

template <typename CapFloorType, typename IndexType>
Volatility impliedVolatilityImpl(const CapFloorType& cap, Real targetValue, const Handle<YieldTermStructure>& d,
                                 Volatility guess, VolatilityType type, Real displacement, Real accuracy,
                                 Natural maxEvaluations, Volatility minVolLognormal, Volatility maxVolLognormal,
                                 Volatility minVolNormal, Volatility maxVolNormal,
                                 const Handle<IndexType>& index = Handle<IndexType>()) {
    QL_REQUIRE(!cap.isExpired(), "instrument expired");
    auto engineGenerator = pricingEngineGeneratorFactory(cap, d, type, displacement, index);
    ImpliedCapFloorVolHelper f(cap, engineGenerator, targetValue);
    NewtonSafe solver;
    solver.setMaxEvaluations(maxEvaluations);
    Real minVol = type == Normal ? minVolNormal : minVolLognormal;
    Real maxVol = type == Normal ? maxVolNormal : maxVolLognormal;
    return solver.solve(f, accuracy, guess, minVol, maxVol);
}

// wrapper function, does not throw
template <typename CapFloorType, typename IndexType>
Volatility impliedVolatilityWrapper(const CapFloorType& cap, Real targetValue, const Handle<YieldTermStructure>& d,
                                    Volatility guess, VolatilityType type, Real displacement,
                                    const Handle<IndexType>& index = Handle<IndexType>()) {
    string strikeStr = "?";
    try {

        Real accuracy = 1.0e-6;
        Natural maxEvaluations = 100;
        Volatility minVolLognormal = 1.0e-7;
        Volatility maxVolLognormal = 4.0;
        Volatility minVolNormal = 1.0e-7;
        Volatility maxVolNormal = 0.05;

        // 1. Get strike for logging
        std::ostringstream oss;
        if (!cap.capRates().empty()) {
            oss << "Cap: " << cap.capRates().size() << " strikes, starting with " << cap.capRates().front()
                << "."; // they are probably all the same here
        }
        if (!cap.floorRates().empty()) {
            oss << "Floor: " << cap.floorRates().size() << " strikes, starting with " << cap.floorRates().front()
                << "."; // they are probably all the same here
        }
        strikeStr = oss.str();

        // 2. Try to get implied Vol with defaults
        TLOG("Getting impliedVolatility for cap (" << cap.maturityDate() << " strike " << strikeStr << ")");
        try {
            double vol =
                impliedVolatilityImpl(cap, targetValue, d, guess, type, displacement, accuracy, maxEvaluations,
                                      minVolLognormal, maxVolLognormal, minVolNormal, maxVolNormal, index);
            TLOG("Got vol " << vol << " on first attempt");
            return vol;
        } catch (std::exception& e) {
            ALOG("Exception getting implied Vol for Cap (" << cap.maturityDate() << " strike " << strikeStr << ") "
                                                           << e.what());
        }

        // 3. Try with bigger bounds
        try {
            Volatility vol = impliedVolatilityImpl(cap, targetValue, d, guess, type, displacement, accuracy,
                                                   maxEvaluations, minVolLognormal / 100.0, maxVolLognormal * 100.0,
                                                   minVolNormal / 100.0, maxVolNormal * 100.0, index);
            TLOG("Got vol " << vol << " on second attempt");
            return vol;
        } catch (std::exception& e) {
            ALOG("Exception getting implied Vol for Cap (" << cap.maturityDate() << " strike " << strikeStr << ") "
                                                           << e.what());
        }

    } catch (...) {
        // pass through to below
    }

    ALOG("Cap impliedVolatility() failed for Cap (" << cap.type() << ", maturity " << cap.maturityDate() << ", strike "
                                                    << strikeStr << " for target " << targetValue
                                                    << ". Returning Initial guess " << guess << " and continuing");
    return guess;
}
} // namespace

Real impliedQuote(const QuantLib::ext::shared_ptr<Instrument>& i) {
    if (QuantLib::ext::dynamic_pointer_cast<VanillaSwap>(i))
        return QuantLib::ext::dynamic_pointer_cast<VanillaSwap>(i)->fairRate();
    if (QuantLib::ext::dynamic_pointer_cast<Deposit>(i))
        return QuantLib::ext::dynamic_pointer_cast<Deposit>(i)->fairRate();
    if (QuantLib::ext::dynamic_pointer_cast<QuantLib::ForwardRateAgreement>(i))
        return QuantLib::ext::dynamic_pointer_cast<QuantLib::ForwardRateAgreement>(i)->forwardRate();
    if (QuantLib::ext::dynamic_pointer_cast<OvernightIndexedSwap>(i))
        return QuantLib::ext::dynamic_pointer_cast<OvernightIndexedSwap>(i)->fairRate();
    if (QuantLib::ext::dynamic_pointer_cast<CrossCcyBasisMtMResetSwap>(i))
        return QuantLib::ext::dynamic_pointer_cast<CrossCcyBasisMtMResetSwap>(i)->fairSpread();
    if (QuantLib::ext::dynamic_pointer_cast<CrossCcyBasisSwap>(i))
        return QuantLib::ext::dynamic_pointer_cast<CrossCcyBasisSwap>(i)->fairPaySpread();
    if (QuantLib::ext::dynamic_pointer_cast<FxForward>(i))
        return QuantLib::ext::dynamic_pointer_cast<FxForward>(i)->fairForwardRate().rate();
    if (QuantLib::ext::dynamic_pointer_cast<QuantExt::CreditDefaultSwap>(i))
        return QuantLib::ext::dynamic_pointer_cast<QuantExt::CreditDefaultSwap>(i)->fairSpreadClean();
    if (QuantLib::ext::dynamic_pointer_cast<ZeroCouponInflationSwap>(i))
        return QuantLib::ext::dynamic_pointer_cast<ZeroCouponInflationSwap>(i)->fairRate();
    if (QuantLib::ext::dynamic_pointer_cast<YearOnYearInflationSwap>(i))
        return QuantLib::ext::dynamic_pointer_cast<YearOnYearInflationSwap>(i)->fairRate();
    if (QuantLib::ext::dynamic_pointer_cast<TenorBasisSwap>(i)) {
        if (QuantLib::ext::dynamic_pointer_cast<TenorBasisSwap>(i)->spreadOnRec())
            return QuantLib::ext::dynamic_pointer_cast<TenorBasisSwap>(i)->fairRecLegSpread();
        else
            return QuantLib::ext::dynamic_pointer_cast<TenorBasisSwap>(i)->fairPayLegSpread();
    }
    if (QuantLib::ext::dynamic_pointer_cast<FixedBMASwap>(i))
        return QuantLib::ext::dynamic_pointer_cast<FixedBMASwap>(i)->fairRate();
    if (QuantLib::ext::dynamic_pointer_cast<SubPeriodsSwap>(i))
        return QuantLib::ext::dynamic_pointer_cast<SubPeriodsSwap>(i)->fairRate();
    QL_FAIL("SensitivityAnalysis: impliedQuote: unknown instrument (is null = " << std::boolalpha << (i == nullptr)
                                                                                << ")");
}

Volatility impliedVolatility(const QuantLib::CapFloor& cap, Real targetValue, const Handle<YieldTermStructure>& d,
                             Volatility guess, VolatilityType type, Real displacement) {
    return impliedVolatilityWrapper(cap, targetValue, d, guess, type, displacement, Handle<Index>());
}

Volatility impliedVolatility(const QuantLib::YoYInflationCapFloor& cap, Real targetValue,
                             const Handle<YieldTermStructure>& d, Volatility guess, VolatilityType type,
                             Real displacement, const Handle<YoYInflationIndex>& index) {
    return impliedVolatilityWrapper(cap, targetValue, d, guess, type, displacement, index);
}

bool riskFactorKeysAreSimilar(const ore::analytics::RiskFactorKey& x, const ore::analytics::RiskFactorKey& y) {
    return x.keytype == y.keytype && x.name == y.name;
}

double impliedVolatility(const RiskFactorKey& key, const ParSensitivityInstrumentBuilder::Instruments& instruments) {
    if (key.keytype == RiskFactorKey::KeyType::OptionletVolatility) {
        QL_REQUIRE(instruments.parCaps_.count(key) == 1, "Can not convert capFloor par shifts to zero Vols");
        QL_REQUIRE(instruments.parCapsYts_.count(key) > 0,
                   "getTodaysAndTargetQuotes: no cap yts found for key " << key);
        QL_REQUIRE(instruments.parCapsVts_.count(key) > 0,
                   "getTodaysAndTargetQuotes: no cap vts found for key " << key);
        const auto cap = instruments.parCaps_.at(key);
        Real price = cap->NPV();
        Volatility parVol = impliedVolatility(*cap, price, instruments.parCapsYts_.at(key), 0.01,
                                              instruments.parCapsVts_.at(key)->volatilityType(),
                                              instruments.parCapsVts_.at(key)->displacement());
        return parVol;
    } else if (key.keytype == RiskFactorKey::KeyType::YoYInflationCapFloorVolatility) {
        QL_REQUIRE(instruments.parYoYCaps_.count(key) == 1, "Can not convert capFloor par shifts to zero Vols");
        QL_REQUIRE(instruments.parYoYCapsYts_.count(key) > 0,
                   "getTodaysAndTargetQuotes: no cap yts found for key " << key);
        QL_REQUIRE(instruments.parYoYCapsVts_.count(key) > 0,
                   "getTodaysAndTargetQuotes: no cap vts found for key " << key);
        QL_REQUIRE(instruments.parYoYCapsIndex_.count(key) > 0,
                   "getTodaysAndTargetQuotes: no cap index found for key " << key);
        const auto& cap = instruments.parYoYCaps_.at(key);
        Real price = cap->NPV();
        Volatility parVol = impliedVolatility(
            *cap, price, instruments.parYoYCapsYts_.at(key), 0.01, instruments.parYoYCapsVts_.at(key)->volatilityType(),
            instruments.parYoYCapsVts_.at(key)->displacement(), instruments.parYoYCapsIndex_.at(key));
        return parVol;
    } else {
        QL_FAIL("impliedCapVolatility: Unsupported risk factor key "
                << key.keytype << ". Support OptionletVolatility and YoYInflationCapFloorVolatility");
    }
}

} // namespace analytics
} // namespace ore
