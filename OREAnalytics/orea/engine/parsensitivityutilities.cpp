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

#include <ql/instruments/bmaswap.hpp>
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
#include <ql/termstructures/volatility/optionlet/constantoptionletvol.hpp>
#include <qle/cashflows/blackovernightindexedcouponpricer.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>

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

class CapFloorImpliedVolCalculator {
public:
    virtual ~CapFloorImpliedVolCalculator() = default;

    virtual double impliedVol(double initialGuess, double accuracy, double minVol, double maxVol, size_t maxIter,
                              VolatilityType volType, double volDisplacement) const = 0;
    virtual size_t numberCoupons() const = 0;
    virtual double strike() const = 0;
    virtual CapFloor::Type type() const = 0;
    virtual Date maturityDate() const = 0;
    virtual double targetValue() const = 0;
};

Volatility runImplyCapFloorVolWithBoundExtension(const std::unique_ptr<CapFloorImpliedVolCalculator>& calc,
                                                 const double initialGuess, VolatilityType type, Real displacement) {
    string strikeStr = "?";
    try {

        Real accuracy = 1.0e-6;
        Natural maxEvaluations = 100;
        Volatility minVolLognormal = 1.0e-7;
        Volatility maxVolLognormal = 4.0;
        Volatility minVolNormal = 1.0e-7;
        Volatility maxVolNormal = 0.05;
        Volatility minVol = type == VolatilityType::Normal ? minVolNormal : minVolLognormal;
        Volatility maxVol = type == VolatilityType::Normal ? maxVolNormal : maxVolLognormal;

        // 1. Get strike for logging
        std::ostringstream oss;
        oss << (calc->type() == CapFloor::Type::Cap ? "Cap: " : "Floor: ") << calc->numberCoupons()
            << " strikes, starting with " << calc->strike() << ".";

        strikeStr = oss.str();
        // 2. Try to get implied Vol with defaults
        TLOG("Getting impliedVolatility for cap (" << calc->maturityDate() << " strike " << strikeStr << ")");
        try {
            double vol = calc->impliedVol(initialGuess, accuracy, minVol, maxVol, maxEvaluations, type, displacement);
            TLOG("Got vol " << vol << " on first attempt");
            return vol;
        } catch (std::exception& e) {
            ALOG("Exception getting implied Vol for Cap (" << calc->maturityDate() << " strike " << strikeStr << ") "
                                                           << e.what());
        }

        // 3. Try with bigger bounds
        try {
            double vol = calc->impliedVol(initialGuess, accuracy, minVol / 100., maxVol * 100., maxEvaluations, type,
                                         displacement);
            TLOG("Got vol " << vol << " on second attempt");
            return vol;
        } catch (std::exception& e) {
            ALOG("Exception getting implied Vol for Cap (" << calc->maturityDate() << " strike " << strikeStr << ") "
                                                           << e.what());
        }

    } catch (...) {
        // pass through to below
    }

    ALOG("Cap impliedVolatility() failed for Cap ("
         << calc->type() << ", maturity " << calc->maturityDate() << ", strike " << strikeStr << " for target "
         << calc->targetValue() << ". Returning Initial guess " << initialGuess << " and continuing");
    return initialGuess;
}

class IborCapFloorImpliedVolCalculator : public CapFloorImpliedVolCalculator {
public:
    IborCapFloorImpliedVolCalculator(const ext::shared_ptr<CapFloor> cap,
                                     const Handle<YieldTermStructure>& discountCurve)
        : cap_(cap), discountCurve_(discountCurve) {
        QL_REQUIRE(cap_ != nullptr, "instrument required");
        QL_REQUIRE((cap_->type() == CapFloor::Type::Cap && !cap_->capRates().empty()) ||
                       (cap_->type() == CapFloor::Type::Floor && !cap_->floorRates().empty()),
                   "Only cap or floor with at least one coupon supported");
        QL_REQUIRE(!cap_->isExpired(), "instrument expired");
        QL_REQUIRE(cap->pricingEngine() != nullptr, "pricing engine need to be set");
        targetValue_ = cap_->NPV();
    }
    ~IborCapFloorImpliedVolCalculator() {}
    size_t numberCoupons() const override {
        return cap_->type() == CapFloor::Cap ? cap_->capRates().size() : cap_->floorRates().size();
    }

    double strike() const override {
        return cap_->type() == CapFloor::Cap ? cap_->capRates().front() : cap_->floorRates().front();
    }

    CapFloor::Type type() const override { return cap_->type(); }

    double targetValue() const override { return targetValue_; }

    Date maturityDate() const override { return cap_->maturityDate(); }

    double impliedVol(double initialGuess, double accuracy, double minVol, double maxVol, size_t maxIter,
                      VolatilityType volType, double volDisplacement) const override {
        auto engineGenerator = pricingEngineFactory(discountCurve_, volType, volDisplacement);
        ImpliedCapFloorVolHelper f(*cap_, engineGenerator, targetValue());
        NewtonSafe solver;
        solver.setMaxEvaluations(maxIter);
        return solver.solve(f, accuracy, initialGuess, minVol, maxVol);
    }

private:
    ext::shared_ptr<CapFloor> cap_;
    Handle<YieldTermStructure> discountCurve_;
    double targetValue_;

    std::function<QuantLib::ext::shared_ptr<PricingEngine>(const Handle<Quote>&)>
    pricingEngineFactory(const Handle<YieldTermStructure>& d, VolatilityType type, Real displacement) const {
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
};

class YoYCapFloorImpliedVolCalculator : public CapFloorImpliedVolCalculator {
public:
    YoYCapFloorImpliedVolCalculator(const ext::shared_ptr<YoYInflationCapFloor>& cap,
                                    const Handle<YieldTermStructure>& discountCurve,
                                    const Handle<YoYInflationIndex>& index)
        : cap_(cap), discountCurve_(discountCurve), index_(index) {
        QL_REQUIRE(cap_ != nullptr, "instrument required");
        QL_REQUIRE(cap_->type() == YoYInflationCapFloor::Type::Cap && !cap_->capRates().empty() ||
                       cap_->type() == YoYInflationCapFloor::Type::Floor && !cap_->floorRates().empty(),
                   "Only cap or floor with at least one coupon supported");
        QL_REQUIRE(!cap_->isExpired(), "instrument expired");
        QL_REQUIRE(cap_->pricingEngine() != nullptr, "pricing engine need to be set");
        targetValue_ = cap_->NPV();
    }
    ~YoYCapFloorImpliedVolCalculator() {}
    size_t numberCoupons() const override {
        return cap_->type() == YoYInflationCapFloor::Cap ? cap_->capRates().size() : cap_->floorRates().size();
    }

    double strike() const override {
        return cap_->type() == YoYInflationCapFloor::Cap ? cap_->capRates().front() : cap_->floorRates().front();
    }

    CapFloor::Type type() const override { return cap_->type() == YoYInflationCapFloor::Cap ?  CapFloor::Cap : CapFloor::Floor; }

    double targetValue() const override { return targetValue_; }

    Date maturityDate() const override { return cap_->maturityDate(); }

    double impliedVol(double initialGuess, double accuracy, double minVol, double maxVol, size_t maxIter,
                      VolatilityType volType, double volDisplacement) const override {
        auto engineGenerator = pricingEngineFactory(discountCurve_, volType, volDisplacement, index_);
        ImpliedCapFloorVolHelper f(*cap_, engineGenerator, targetValue_);
        NewtonSafe solver;
        solver.setMaxEvaluations(maxIter);
        return solver.solve(f, accuracy, initialGuess, minVol, maxVol);
    }

private:
    ext::shared_ptr<YoYInflationCapFloor> cap_;
    Handle<YieldTermStructure> discountCurve_;
    Handle<YoYInflationIndex> index_;
    double targetValue_; 
    std::function<QuantLib::ext::shared_ptr<PricingEngine>(const Handle<Quote>&)>
    pricingEngineFactory(const Handle<YieldTermStructure>& d, VolatilityType type, Real displacement,
                         const Handle<YoYInflationIndex>& index) const {
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
                    return QuantLib::ext::make_shared<QuantExt::YoYInflationUnitDisplacedBlackCapFloorEngine>(*index, c,
                                                                                                              d);
                };
        } else if (type == Normal)
            engineGenerator = [&d, &index](const Handle<Quote>& h) {
                auto c = Handle<QuantLib::YoYOptionletVolatilitySurface>(
                    QuantLib::ext::make_shared<QuantExt::ConstantYoYOptionletVolatility>(
                        h, 0, NullCalendar(), Unadjusted, Actual365Fixed(),
                        index->yoyInflationTermStructure()->observationLag(), index->frequency(),
                        index->interpolated()));
                return QuantLib::ext::make_shared<QuantExt::YoYInflationBachelierCapFloorEngine>(*index, c, d);
            };
        else
            QL_FAIL("volatility type " << type << " not implemented");
        return engineGenerator;
    }
};

class OISCapFloorImpliedVolCalculator : public CapFloorImpliedVolCalculator {
public:
    OISCapFloorImpliedVolCalculator(const ext::shared_ptr<QuantLib::Swap>& cap) : cap_(cap) {
        QL_REQUIRE(cap_ != nullptr && cap_->legs().size() == 1 && !cap_->leg(0).empty(),
                   "A ois cap floor, should have one floating leg with at least one coupon");
        const auto firstCoupon =
            ext::dynamic_pointer_cast<QuantExt::CappedFlooredOvernightIndexedCoupon>(cap->leg(0).front());

        QL_REQUIRE(firstCoupon != nullptr,
                   "A ois cap floor should have at least one CappedFlooredOvernightIndexedCoupon");

        couponPricerBackup_ = firstCoupon->pricer();

        QL_REQUIRE(couponPricerBackup_ != nullptr, "Coupon pricer is missing");
        strike_ = firstCoupon->isCapped() ? firstCoupon->cap() : firstCoupon->floor();
        type_ = firstCoupon->isCapped() ? CapFloor::Type::Cap : CapFloor::Type::Floor;
    }
    ~OISCapFloorImpliedVolCalculator() {
        for (auto& cf : cap_->leg(0)) {
            auto cp = ext::dynamic_pointer_cast<FloatingRateCoupon>(cf);
            cp->setPricer(couponPricerBackup_);
        }
    }

    size_t numberCoupons() const override { return cap_->leg(0).size(); }

    double strike() const override { return strike_; }

    CapFloor::Type type() const override { return type_; }
    Date maturityDate() const override { return cap_->maturityDate(); }
    double targetValue() const override { return targetValue_; }
    double impliedVol(double initalGuess, double accuracy, double minVol, double maxVol, size_t maxIter,
                      VolatilityType volType, double volDisplacement) const override {
        auto implVolQuote = ext::make_shared<SimpleQuote>(initalGuess);
        Handle<OptionletVolatilityStructure> constOvts;
        if (volType == ShiftedLognormal) {
            constOvts = Handle<OptionletVolatilityStructure>(QuantLib::ext::make_shared<ConstantOptionletVolatility>(
                0, NullCalendar(), Unadjusted, Handle<Quote>(implVolQuote), Actual365Fixed(), ShiftedLognormal,
                volDisplacement));
        } else {
            minVol = 1.0e-9;
            maxVol = 0.05;
            constOvts = Handle<OptionletVolatilityStructure>(QuantLib::ext::make_shared<ConstantOptionletVolatility>(
                0, NullCalendar(), Unadjusted, Handle<Quote>(implVolQuote), Actual365Fixed(), Normal));
        }
        auto pricer = QuantLib::ext::make_shared<BlackOvernightIndexedCouponPricer>(constOvts, true);

        const auto& cap = cap_;
        Real price = cap->NPV();
        for (auto& cf : cap->leg(0)) {
            auto cp = ext::dynamic_pointer_cast<FloatingRateCoupon>(cf);
            cp->setPricer(pricer);
        }

        Brent solver;

        auto target = [&cap, &price, &implVolQuote](double x) -> double {
            implVolQuote->setValue(x);
            return (price - cap->NPV()) * 10000.;
        };

        return solver.solve(target, accuracy, initalGuess, minVol, maxVol);
    }

private:
    ext::shared_ptr<QuantLib::Swap> cap_;
    ext::shared_ptr<QuantLib::FloatingRateCouponPricer> couponPricerBackup_;
    CapFloor::Type type_;
    double strike_;
    double targetValue_;
};

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
    if (auto p = QuantLib::ext::dynamic_pointer_cast<BMASwap>(i)) {
        return p->fairIndexFraction();
    }
    if (QuantLib::ext::dynamic_pointer_cast<FixedBMASwap>(i))
        return QuantLib::ext::dynamic_pointer_cast<FixedBMASwap>(i)->fairRate();
    if (QuantLib::ext::dynamic_pointer_cast<SubPeriodsSwap>(i))
        return QuantLib::ext::dynamic_pointer_cast<SubPeriodsSwap>(i)->fairRate();
    QL_FAIL("ParSensitivitiyAnalysis: impliedQuote(): unknown instrument (is null = " << std::boolalpha
                                                                                      << (i == nullptr) << ")");
}

bool riskFactorKeysAreSimilar(const ore::analytics::RiskFactorKey& x, const ore::analytics::RiskFactorKey& y) {
    return x.keytype == y.keytype && x.name == y.name;
}

double impliedVolatility(const RiskFactorKey& key, const ParSensitivityInstrumentBuilder::Instruments& instruments) {
    if (key.keytype == RiskFactorKey::KeyType::OptionletVolatility && instruments.parCaps_.count(key) == 1) {
        auto ytsIt = instruments.parCapsYts_.find(key);
        auto vtsIt = instruments.parCapsVts_.find(key);
        auto capIt = instruments.parCaps_.find(key);
        QL_REQUIRE(ytsIt != instruments.parCapsYts_.end(),
                   "getTodaysAndTargetQuotes: no cap yts found for key " << key);
        QL_REQUIRE(vtsIt != instruments.parCapsVts_.end(),
                   "getTodaysAndTargetQuotes: no cap vts found for key " << key);
        QL_REQUIRE(capIt != instruments.parCaps_.end(), "getTodaysAndTargetQuotes: no cap found for key " << key);
        std::unique_ptr<CapFloorImpliedVolCalculator> calc =
            std::make_unique<IborCapFloorImpliedVolCalculator>(capIt->second, ytsIt->second);
        auto volType = vtsIt->second->volatilityType();
        auto displacement = vtsIt->second->displacement();
        return runImplyCapFloorVolWithBoundExtension(calc, 0.01, volType, displacement);
    } else if (key.keytype == RiskFactorKey::KeyType::OptionletVolatility && instruments.oisParCaps_.count(key) == 1) {
        auto vtsIt = instruments.parCapsVts_.find(key);
        auto capIt = instruments.oisParCaps_.find(key);
        QL_REQUIRE(vtsIt != instruments.parCapsVts_.end(),
                   "getTodaysAndTargetQuotes: no cap vts found for key " << key);
        QL_REQUIRE(capIt != instruments.oisParCaps_.end(), "getTodaysAndTargetQuotes: no cap found for key " << key);
        std::unique_ptr<CapFloorImpliedVolCalculator> calc =
            std::make_unique<OISCapFloorImpliedVolCalculator>(capIt->second);
        auto volType = vtsIt->second->volatilityType();
        auto displacement = vtsIt->second->displacement();
        return runImplyCapFloorVolWithBoundExtension(calc, 0.01, volType, displacement);
    } else if (key.keytype == RiskFactorKey::KeyType::YoYInflationCapFloorVolatility) {
        auto ytsIt = instruments.parYoYCapsYts_.find(key);
        auto vtsIt = instruments.parYoYCapsVts_.find(key);
        auto capIt = instruments.parYoYCaps_.find(key);
        auto indexIt = instruments.parYoYCapsIndex_.find(key);

        QL_REQUIRE(ytsIt != instruments.parYoYCapsYts_.end(),
                   "getTodaysAndTargetQuotes: no cap yts found for key " << key);
        QL_REQUIRE(vtsIt != instruments.parYoYCapsVts_.end(),
                   "getTodaysAndTargetQuotes: no cap vts found for key " << key);
        QL_REQUIRE(capIt != instruments.parYoYCaps_.end(), "getTodaysAndTargetQuotes: no cap found for key " << key);
        QL_REQUIRE(indexIt != instruments.parYoYCapsIndex_.end(),
                   "getTodaysAndTargetQuotes: no yoy index found for key " << key);
        std::unique_ptr<CapFloorImpliedVolCalculator> calc =
            std::make_unique<YoYCapFloorImpliedVolCalculator>(capIt->second, ytsIt->second, indexIt->second);
        auto volType = vtsIt->second->volatilityType();
        auto displacement = vtsIt->second->displacement();
        return runImplyCapFloorVolWithBoundExtension(calc, 0.01, volType, displacement);
    } else {
        QL_FAIL("impliedCapVolatility: Unsupported risk factor key "
                << key.keytype << ". Support OptionletVolatility and YoYInflationCapFloorVolatility");
    }
}

} // namespace analytics
} // namespace ore
