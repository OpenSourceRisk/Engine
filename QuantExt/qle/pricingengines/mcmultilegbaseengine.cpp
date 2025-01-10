/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/cappedflooredaveragebmacoupon.hpp>
#include <qle/cashflows/equitycashflow.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/fixedratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/iborfracoupon.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/instruments/rebatedexercise.hpp>
#include <qle/math/randomvariablelsmbasissystem.hpp>
#include <qle/pricingengines/mcmultilegbaseengine.hpp>
#include <qle/processes/irlgm1fstateprocess.hpp>

#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/cmscoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/math/interpolations/linearinterpolation.hpp>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/array.hpp>

namespace QuantExt {

McMultiLegBaseEngine::McMultiLegBaseEngine(
    const Handle<CrossAssetModel>& model, const SequenceType calibrationPathGenerator,
    const SequenceType pricingPathGenerator, const Size calibrationSamples, const Size pricingSamples,
    const Size calibrationSeed, const Size pricingSeed, const Size polynomOrder,
    const LsmBasisSystem::PolynomialType polynomType, const SobolBrownianGenerator::Ordering ordering,
    SobolRsg::DirectionIntegers directionIntegers, const std::vector<Handle<YieldTermStructure>>& discountCurves,
    const std::vector<Date>& simulationDates, const std::vector<Date>& stickyCloseOutDates,
    const std::vector<Size>& externalModelIndices, const bool minimalObsDate, const RegressorModel regressorModel,
    const Real regressionVarianceCutoff, const bool recalibrateOnStickyCloseOutDates,
    const bool reevaluateExerciseInStickyRun)
    : model_(model), calibrationPathGenerator_(calibrationPathGenerator), pricingPathGenerator_(pricingPathGenerator),
      calibrationSamples_(calibrationSamples), pricingSamples_(pricingSamples), calibrationSeed_(calibrationSeed),
      pricingSeed_(pricingSeed), polynomOrder_(polynomOrder), polynomType_(polynomType), ordering_(ordering),
      directionIntegers_(directionIntegers), discountCurves_(discountCurves), simulationDates_(simulationDates),
      stickyCloseOutDates_(stickyCloseOutDates), externalModelIndices_(externalModelIndices),
      minimalObsDate_(minimalObsDate), regressorModel_(regressorModel),
      regressionVarianceCutoff_(regressionVarianceCutoff),
      recalibrateOnStickyCloseOutDates_(recalibrateOnStickyCloseOutDates),
      reevaluateExerciseInStickyRun_(reevaluateExerciseInStickyRun) {

    if (discountCurves_.empty())
        discountCurves_.resize(model_->components(CrossAssetModel::AssetType::IR));
    else {
        QL_REQUIRE(discountCurves_.size() == model_->components(CrossAssetModel::AssetType::IR),
                   "McMultiLegBaseEngine: " << discountCurves_.size() << " discount curves given, but model has "
                                            << model_->components(CrossAssetModel::AssetType::IR) << " IR components.");
    }
}

Real McMultiLegBaseEngine::time(const Date& d) const {
    return model_->irlgm1f(0)->termStructure()->timeFromReference(d);
}

McMultiLegBaseEngine::CashflowInfo McMultiLegBaseEngine::createCashflowInfo(QuantLib::ext::shared_ptr<CashFlow> flow,
                                                                            const Currency& payCcy, bool payer,
                                                                            Size legNo, Size cfNo) const {
    CashflowInfo info;

    // set some common info: pay time, pay ccy index in the model, payer, exercise into decision time

    info.legNo = legNo;
    info.cfNo = cfNo;
    info.payTime = time(flow->date());
    info.payCcyIndex = model_->ccyIndex(payCcy);
    info.payer = payer;

    auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(flow);
    if (cpn && cpn->accrualStartDate() < flow->date()) {
        info.exIntoCriterionTime = time(cpn->accrualStartDate()) + tinyTime;
    } else {
        info.exIntoCriterionTime = info.payTime + (exerciseIntoIncludeSameDayFlows_ ? tinyTime : 0.0);
    }

    // Handle SimpleCashflow
    if (QuantLib::ext::dynamic_pointer_cast<SimpleCashFlow>(flow) != nullptr) {
        info.amountCalculator = [flow](const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            return RandomVariable(n, flow->amount());
        };
        return info;
    }

    // handle fx linked fixed cashflow
    if (auto fxl = QuantLib::ext::dynamic_pointer_cast<FXLinkedCashFlow>(flow)) {
        Date fxLinkedFixingDate = fxl->fxFixingDate();
        Size fxLinkedSourceCcyIdx = model_->ccyIndex(fxl->fxIndex()->sourceCurrency());
        Size fxLinkedTargetCcyIdx = model_->ccyIndex(fxl->fxIndex()->targetCurrency());
        if (fxLinkedFixingDate > today_) {
            Real fxSimTime = time(fxLinkedFixingDate);
            info.simulationTimes.push_back(fxSimTime);
            info.modelIndices.push_back({});
            if (fxLinkedSourceCcyIdx > 0) {
                info.modelIndices.front().push_back(
                    model_->pIdx(CrossAssetModel::AssetType::FX, fxLinkedSourceCcyIdx - 1));
            }
            if (fxLinkedTargetCcyIdx > 0) {
                info.modelIndices.front().push_back(
                    model_->pIdx(CrossAssetModel::AssetType::FX, fxLinkedTargetCcyIdx - 1));
            }
        }
        info.amountCalculator = [this, fxLinkedSourceCcyIdx, fxLinkedTargetCcyIdx, fxLinkedFixingDate,
                                 fxl](const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            if (fxLinkedFixingDate <= today_)
                return RandomVariable(n, fxl->amount());
            RandomVariable fxSource(n, 1.0), fxTarget(n, 1.0);
            Size fxIdx = 0;
            if (fxLinkedSourceCcyIdx > 0)
                fxSource = exp(*states.at(0).at(fxIdx++));
            if (fxLinkedTargetCcyIdx > 0)
                fxTarget = exp(*states.at(0).at(fxIdx));
            return RandomVariable(n, fxl->foreignAmount()) * fxSource / fxTarget;
        };

        return info;
    }

    // handle some wrapped coupon types: extract the wrapper info and continue with underlying flow
    // we can have multiple nested wrappers, e.g. fx and eq for eq swap funding legs

    bool isFxLinked = false;
    bool isFxIndexed = false;
    bool isEqIndexed = false;
    Size fxLinkedSourceCcyIdx = Null<Size>();
    Size fxLinkedTargetCcyIdx = Null<Size>();
    Real fxLinkedFixedFxRate = Null<Real>();
    Real fxLinkedSimTime = Null<Real>(); // if fx fixing date > today
    Real fxLinkedForeignNominal = Null<Real>();
    Size eqLinkedIdx = Null<Size>();
    Real eqLinkedFixedPrice = Null<Real>();
    Real eqLinkedSimTime = Null<Real>(); // if eq fixing date > today
    Real eqLinkedQuantity = Null<Real>();
    std::vector<Size> fxLinkedModelIndices;
    std::vector<Size> eqLinkedModelIndices;

    bool foundWrapper;
    do {
        foundWrapper = false;
        if (auto indexCpn = QuantLib::ext::dynamic_pointer_cast<IndexedCoupon>(flow)) {
            if (auto fxIndex = QuantLib::ext::dynamic_pointer_cast<FxIndex>(indexCpn->index())) {
                QL_REQUIRE(!isFxIndexed,
                           "McMultiLegBaseEngine::createCashflowInfo(): multiple fx indexings found for coupon at leg "
                               << legNo << " cashflow " << cfNo << ". Only one fx indexing is allowed.");
                isFxIndexed = true;
                auto fixingDate = indexCpn->fixingDate();
                fxLinkedSourceCcyIdx = model_->ccyIndex(fxIndex->sourceCurrency());
                fxLinkedTargetCcyIdx = model_->ccyIndex(fxIndex->targetCurrency());
                if (fixingDate <= today_) {
                    fxLinkedFixedFxRate = fxIndex->fixing(fixingDate);
                } else {
                    fxLinkedSimTime = time(fixingDate);
                    if (fxLinkedSourceCcyIdx > 0) {
                        fxLinkedModelIndices.push_back(
                            model_->pIdx(CrossAssetModel::AssetType::FX, fxLinkedSourceCcyIdx - 1));
                    }
                    if (fxLinkedTargetCcyIdx > 0) {
                        fxLinkedModelIndices.push_back(
                            model_->pIdx(CrossAssetModel::AssetType::FX, fxLinkedTargetCcyIdx - 1));
                    }
                }
                flow = indexCpn->underlying();
                foundWrapper = true;
            } else if (auto eqIndex = QuantLib::ext::dynamic_pointer_cast<EquityIndex2>(indexCpn->index())) {
                QL_REQUIRE(!isEqIndexed,
                           "McMultiLegBaseEngine::createCashflowInfo(): multiple eq indexings found for coupon at leg "
                               << legNo << " cashflow " << cfNo << ". Only one eq indexing is allowed.");
                isEqIndexed = true;
                auto fixingDate = indexCpn->fixingDate();
                eqLinkedIdx = model_->eqIndex(eqIndex->name());
                eqLinkedQuantity = indexCpn->quantity();
                if (fixingDate <= today_) {
                    eqLinkedFixedPrice = eqIndex->fixing(fixingDate);
                } else {
                    eqLinkedSimTime = time(fixingDate);
                    eqLinkedModelIndices.push_back(model_->pIdx(CrossAssetModel::AssetType::EQ, eqLinkedIdx));
                }
                flow = indexCpn->underlying();
                foundWrapper = true;
            } else {
                QL_FAIL("McMultiLegBaseEngine::createCashflowInfo(): unhandled indexing for coupon at leg "
                        << legNo << " cashflow " << cfNo << ": supported indexings are fx, eq");
            }
        } else if (auto fxl = QuantLib::ext::dynamic_pointer_cast<FloatingRateFXLinkedNotionalCoupon>(flow)) {
            isFxLinked = true;
            auto fixingDate = fxl->fxFixingDate();
            fxLinkedSourceCcyIdx = model_->ccyIndex(fxl->fxIndex()->sourceCurrency());
            fxLinkedTargetCcyIdx = model_->ccyIndex(fxl->fxIndex()->targetCurrency());
            if (fixingDate <= today_) {
                fxLinkedFixedFxRate = fxl->fxIndex()->fixing(fixingDate);
            } else {
                fxLinkedSimTime = time(fixingDate);
                if (fxLinkedSourceCcyIdx > 0) {
                    fxLinkedModelIndices.push_back(
                        model_->pIdx(CrossAssetModel::AssetType::FX, fxLinkedSourceCcyIdx - 1));
                }
                if (fxLinkedTargetCcyIdx > 0) {
                    fxLinkedModelIndices.push_back(
                        model_->pIdx(CrossAssetModel::AssetType::FX, fxLinkedTargetCcyIdx - 1));
                }
            }
            flow = fxl->underlying();
            fxLinkedForeignNominal = fxl->foreignAmount();
            foundWrapper = true;
        }
    } while (foundWrapper);

    // handle cap / floored coupons

    bool isCapFloored = false;
    bool isNakedOption = false;
    Real effCap = Null<Real>(), effFloor = Null<Real>();
    if (auto stripped = QuantLib::ext::dynamic_pointer_cast<StrippedCappedFlooredCoupon>(flow)) {
        isNakedOption = true;
        flow = stripped->underlying(); // this is a CappedFlooredCoupon, handled below
    }

    if (auto cf = QuantLib::ext::dynamic_pointer_cast<CappedFlooredCoupon>(flow)) {
        isCapFloored = true;
        effCap = cf->effectiveCap();
        effFloor = cf->effectiveFloor();
        flow = cf->underlying();
    }

    // handle the coupon types

    if (QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(flow) != nullptr) {

        Size simTimeCounter = 0;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(fxLinkedSimTime);
            info.modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(eqLinkedSimTime);
            info.modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        info.amountCalculator = [flow, isFxLinked, isFxIndexed, fxLinkedFixedFxRate, fxLinkedSourceCcyIdx,
                                 fxLinkedTargetCcyIdx, isEqIndexed, eqLinkedFixedPrice, eqLinkedQuantity, statesFxIdx,
                                 statesEqIdx](const Size n,
                                              const std::vector<std::vector<const RandomVariable*>>& states) {
            RandomVariable fxFixing(n, 1.0);
            if (isFxLinked || isFxIndexed) {
                if (fxLinkedFixedFxRate != Null<Real>()) {
                    fxFixing = RandomVariable(n, fxLinkedFixedFxRate);
                } else {
                    RandomVariable fxSource(n, 1.0), fxTarget(n, 1.0);
                    Size fxIdx = 0;
                    if (fxLinkedSourceCcyIdx > 0)
                        fxSource = exp(*states.at(statesFxIdx).at(fxIdx++));
                    if (fxLinkedTargetCcyIdx > 0)
                        fxTarget = exp(*states.at(statesFxIdx).at(fxIdx));
                    fxFixing = fxSource / fxTarget;
                }
            }
            RandomVariable eqFixing(n, 1.0);
            if (isEqIndexed) {
                if (eqLinkedFixedPrice != Null<Real>()) {
                    eqFixing = RandomVariable(n, eqLinkedFixedPrice);
                } else {
                    eqFixing = exp(*states.at(statesEqIdx).at(0));
                }
                eqFixing *= RandomVariable(n, eqLinkedQuantity);
            }
            return eqFixing * fxFixing * RandomVariable(n, flow->amount());
        };
        return info;
    }

    if (auto ibor = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(flow)) {
        Real fixedRate =
            ibor->fixingDate() <= today_ ? (ibor->rate() - ibor->spread()) / ibor->gearing() : Null<Real>();
        Size indexCcyIdx = model_->ccyIndex(ibor->index()->currency());
        Real simTime = time(ibor->fixingDate());
        if (ibor->fixingDate() > today_) {
            info.simulationTimes.push_back(simTime);
            info.modelIndices.push_back({model_->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});
        }

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(fxLinkedSimTime);
            info.modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(eqLinkedSimTime);
            info.modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        info.amountCalculator = [this, indexCcyIdx, ibor, simTime, fixedRate, isFxLinked, fxLinkedForeignNominal,
                                 fxLinkedSourceCcyIdx, fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isCapFloored,
                                 isNakedOption, effFloor, effCap, isFxIndexed, isEqIndexed, eqLinkedFixedPrice,
                                 eqLinkedQuantity, statesFxIdx, statesEqIdx](
                                    const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            RandomVariable fixing = fixedRate != Null<Real>()
                                        ? RandomVariable(n, fixedRate)
                                        : lgmVectorised_[indexCcyIdx].fixing(ibor->index(), ibor->fixingDate(), simTime,
                                                                             *states.at(0).at(0));
            RandomVariable fxFixing(n, 1.0);
            if (isFxLinked || isFxIndexed) {
                if (fxLinkedFixedFxRate != Null<Real>()) {
                    fxFixing = RandomVariable(n, fxLinkedFixedFxRate);
                } else {
                    RandomVariable fxSource(n, 1.0), fxTarget(n, 1.0);
                    Size fxIdx = 0;
                    if (fxLinkedSourceCcyIdx > 0)
                        fxSource = exp(*states.at(statesFxIdx).at(fxIdx++));
                    if (fxLinkedTargetCcyIdx > 0)
                        fxTarget = exp(*states.at(statesFxIdx).at(fxIdx));
                    fxFixing = fxSource / fxTarget;
                }
            }
            RandomVariable eqFixing(n, 1.0);
            if (isEqIndexed) {
                if (eqLinkedFixedPrice != Null<Real>()) {
                    eqFixing = RandomVariable(n, eqLinkedFixedPrice);
                } else {
                    eqFixing = exp(*states.at(statesEqIdx).at(0));
                }
                eqFixing *= RandomVariable(n, eqLinkedQuantity);
            }

            RandomVariable effectiveRate;
            if (isCapFloored) {
                RandomVariable swapletRate(n, 0.0);
                RandomVariable floorletRate(n, 0.0);
                RandomVariable capletRate(n, 0.0);
                if (!isNakedOption)
                    swapletRate = RandomVariable(n, ibor->gearing()) * fixing + RandomVariable(n, ibor->spread());
                if (effFloor != Null<Real>())
                    floorletRate = RandomVariable(n, ibor->gearing()) *
                                   max(RandomVariable(n, effFloor) - fixing, RandomVariable(n, 0.0));
                if (effCap != Null<Real>())
                    capletRate = RandomVariable(n, ibor->gearing()) *
                                 max(fixing - RandomVariable(n, effCap), RandomVariable(n, 0.0)) *
                                 RandomVariable(n, isNakedOption && effFloor == Null<Real>() ? -1.0 : 1.0);
                effectiveRate = swapletRate + floorletRate - capletRate;
            } else {
                effectiveRate = RandomVariable(n, ibor->gearing()) * fixing + RandomVariable(n, ibor->spread());
            }
            return RandomVariable(n, (isFxLinked ? fxLinkedForeignNominal : ibor->nominal()) * ibor->accrualPeriod()) *
                   effectiveRate * fxFixing * eqFixing;
        };

        return info;
    }

    if (auto cms = QuantLib::ext::dynamic_pointer_cast<CmsCoupon>(flow)) {
        Real fixedRate = cms->fixingDate() <= today_ ? (cms->rate() - cms->spread()) / cms->gearing() : Null<Real>();
        Size indexCcyIdx = model_->ccyIndex(cms->index()->currency());
        Real simTime = time(cms->fixingDate());
        if (cms->fixingDate() > today_) {
            info.simulationTimes.push_back(simTime);
            info.modelIndices.push_back({model_->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});
        }

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(fxLinkedSimTime);
            info.modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(eqLinkedSimTime);
            info.modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        info.amountCalculator = [this, indexCcyIdx, cms, simTime, fixedRate, isFxLinked, fxLinkedForeignNominal,
                                 fxLinkedSourceCcyIdx, fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isCapFloored,
                                 isNakedOption, effFloor, effCap, isFxIndexed, isEqIndexed, eqLinkedFixedPrice,
                                 eqLinkedQuantity, statesFxIdx, statesEqIdx](
                                    const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            RandomVariable fixing =
                fixedRate != Null<Real>()
                    ? RandomVariable(n, fixedRate)
                    : lgmVectorised_[indexCcyIdx].fixing(cms->index(), cms->fixingDate(), simTime, *states.at(0).at(0));
            RandomVariable fxFixing(n, 1.0);
            if (isFxLinked || isFxIndexed) {
                if (fxLinkedFixedFxRate != Null<Real>()) {
                    fxFixing = RandomVariable(n, fxLinkedFixedFxRate);
                } else {
                    RandomVariable fxSource(n, 1.0), fxTarget(n, 1.0);
                    Size fxIdx = 0;
                    if (fxLinkedSourceCcyIdx > 0)
                        fxSource = exp(*states.at(statesFxIdx).at(fxIdx++));
                    if (fxLinkedTargetCcyIdx > 0)
                        fxTarget = exp(*states.at(statesFxIdx).at(fxIdx));
                    fxFixing = fxSource / fxTarget;
                }
            }
            RandomVariable eqFixing(n, 1.0);
            if (isEqIndexed) {
                if (eqLinkedFixedPrice != Null<Real>()) {
                    eqFixing = RandomVariable(n, eqLinkedFixedPrice);
                } else {
                    eqFixing = exp(*states.at(statesEqIdx).at(0));
                }
                eqFixing *= RandomVariable(n, eqLinkedQuantity);
            }

            RandomVariable effectiveRate;
            if (isCapFloored) {
                RandomVariable swapletRate(n, 0.0);
                RandomVariable floorletRate(n, 0.0);
                RandomVariable capletRate(n, 0.0);
                if (!isNakedOption)
                    swapletRate = RandomVariable(n, cms->gearing()) * fixing + RandomVariable(n, cms->spread());
                if (effFloor != Null<Real>())
                    floorletRate = RandomVariable(n, cms->gearing()) *
                                   max(RandomVariable(n, effFloor) - fixing, RandomVariable(n, 0.0));
                if (effCap != Null<Real>())
                    capletRate = RandomVariable(n, cms->gearing()) *
                                 max(fixing - RandomVariable(n, effCap), RandomVariable(n, 0.0)) *
                                 RandomVariable(n, isNakedOption && effFloor == Null<Real>() ? -1.0 : 1.0);
                effectiveRate = swapletRate + floorletRate - capletRate;
            } else {
                effectiveRate = RandomVariable(n, cms->gearing()) * fixing + RandomVariable(n, cms->spread());
            }

            return RandomVariable(n, (isFxLinked ? fxLinkedForeignNominal : cms->nominal()) * cms->accrualPeriod()) *
                   effectiveRate * fxFixing * eqFixing;
        };

        return info;
    }

    if (auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndexedCoupon>(flow)) {
        Real simTime = std::max(0.0, time(on->valueDates().front()));
        Size indexCcyIdx = model_->ccyIndex(on->index()->currency());
        info.simulationTimes.push_back(simTime);
        info.modelIndices.push_back({model_->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(fxLinkedSimTime);
            info.modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(eqLinkedSimTime);
            info.modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        info.amountCalculator =
            [this, indexCcyIdx, on, simTime, isFxLinked, fxLinkedForeignNominal, fxLinkedSourceCcyIdx,
             fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isFxIndexed, isEqIndexed, eqLinkedFixedPrice, eqLinkedQuantity,
             statesFxIdx, statesEqIdx](const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
                RandomVariable effectiveRate = lgmVectorised_[indexCcyIdx].compoundedOnRate(
                    on->overnightIndex(), on->fixingDates(), on->valueDates(), on->dt(), on->rateCutoff(),
                    on->includeSpread(), on->spread(), on->gearing(), on->lookback(), Null<Real>(), Null<Real>(), false,
                    false, simTime, *states.at(0).at(0));
                RandomVariable fxFixing(n, 1.0);
                if (isFxLinked || isFxIndexed) {
                    if (fxLinkedFixedFxRate != Null<Real>()) {
                        fxFixing = RandomVariable(n, fxLinkedFixedFxRate);
                    } else {
                        RandomVariable fxSource(n, 1.0), fxTarget(n, 1.0);
                        Size fxIdx = 0;
                        if (fxLinkedSourceCcyIdx > 0)
                            fxSource = exp(*states.at(statesFxIdx).at(fxIdx++));
                        if (fxLinkedTargetCcyIdx > 0)
                            fxTarget = exp(*states.at(statesFxIdx).at(fxIdx));
                        fxFixing = fxSource / fxTarget;
                    }
                }
                RandomVariable eqFixing(n, 1.0);
                if (isEqIndexed) {
                    if (eqLinkedFixedPrice != Null<Real>()) {
                        eqFixing = RandomVariable(n, eqLinkedFixedPrice);
                    } else {
                        eqFixing = exp(*states.at(statesEqIdx).at(0));
                    }
                    eqFixing *= RandomVariable(n, eqLinkedQuantity);
                }
                return RandomVariable(n, (isFxLinked ? fxLinkedForeignNominal : on->nominal()) * on->accrualPeriod()) *
                       effectiveRate * fxFixing * eqFixing;
            };

        return info;
    }

    if (auto cfon = QuantLib::ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(flow)) {
        Real simTime = std::max(0.0, time(cfon->underlying()->valueDates().front()));
        Size indexCcyIdx = model_->ccyIndex(cfon->underlying()->index()->currency());
        info.simulationTimes.push_back(simTime);
        info.modelIndices.push_back({model_->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(fxLinkedSimTime);
            info.modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(eqLinkedSimTime);
            info.modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        info.amountCalculator = [this, indexCcyIdx, cfon, simTime, isFxLinked, fxLinkedForeignNominal,
                                 fxLinkedSourceCcyIdx, fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isFxIndexed,
                                 isEqIndexed, eqLinkedFixedPrice, eqLinkedQuantity, statesFxIdx, statesEqIdx](
                                    const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            RandomVariable effectiveRate = lgmVectorised_[indexCcyIdx].compoundedOnRate(
                cfon->underlying()->overnightIndex(), cfon->underlying()->fixingDates(),
                cfon->underlying()->valueDates(), cfon->underlying()->dt(), cfon->underlying()->rateCutoff(),
                cfon->underlying()->includeSpread(), cfon->underlying()->spread(), cfon->underlying()->gearing(),
                cfon->underlying()->lookback(), cfon->cap(), cfon->floor(), cfon->localCapFloor(), cfon->nakedOption(),
                simTime, *states.at(0).at(0));
            RandomVariable fxFixing(n, 1.0);
            if (isFxLinked || isFxIndexed) {
                if (fxLinkedFixedFxRate != Null<Real>()) {
                    fxFixing = RandomVariable(n, fxLinkedFixedFxRate);
                } else {
                    RandomVariable fxSource(n, 1.0), fxTarget(n, 1.0);
                    Size fxIdx = 0;
                    if (fxLinkedSourceCcyIdx > 0)
                        fxSource = exp(*states.at(statesFxIdx).at(fxIdx++));
                    if (fxLinkedTargetCcyIdx > 0)
                        fxTarget = exp(*states.at(statesFxIdx).at(fxIdx));
                    fxFixing = fxSource / fxTarget;
                }
            }
            RandomVariable eqFixing(n, 1.0);
            if (isEqIndexed) {
                if (eqLinkedFixedPrice != Null<Real>()) {
                    eqFixing = RandomVariable(n, eqLinkedFixedPrice);
                } else {
                    eqFixing = exp(*states.at(statesEqIdx).at(0));
                }
                eqFixing *= RandomVariable(n, eqLinkedQuantity);
            }
            return RandomVariable(n, (isFxLinked ? fxLinkedForeignNominal : cfon->nominal()) * cfon->accrualPeriod()) *
                   effectiveRate * fxFixing * eqFixing;
        };

        return info;
    }

    if (auto av = QuantLib::ext::dynamic_pointer_cast<AverageONIndexedCoupon>(flow)) {
        Real simTime = std::max(0.0, time(av->valueDates().front()));
        Size indexCcyIdx = model_->ccyIndex(av->index()->currency());
        info.simulationTimes.push_back(simTime);
        info.modelIndices.push_back({model_->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(fxLinkedSimTime);
            info.modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(eqLinkedSimTime);
            info.modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        info.amountCalculator =
            [this, indexCcyIdx, av, simTime, isFxLinked, fxLinkedForeignNominal, fxLinkedSourceCcyIdx,
             fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isFxIndexed, isEqIndexed, eqLinkedFixedPrice, eqLinkedQuantity,
             statesFxIdx, statesEqIdx](const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
                RandomVariable effectiveRate = lgmVectorised_[indexCcyIdx].averagedOnRate(
                    av->overnightIndex(), av->fixingDates(), av->valueDates(), av->dt(), av->rateCutoff(), false,
                    av->spread(), av->gearing(), av->lookback(), Null<Real>(), Null<Real>(), false, false, simTime,
                    *states.at(0).at(0));
                RandomVariable fxFixing(n, 1.0);
                if (isFxLinked || isFxIndexed) {
                    if (fxLinkedFixedFxRate != Null<Real>()) {
                        fxFixing = RandomVariable(n, fxLinkedFixedFxRate);
                    } else {
                        RandomVariable fxSource(n, 1.0), fxTarget(n, 1.0);
                        Size fxIdx = 0;
                        if (fxLinkedSourceCcyIdx > 0)
                            fxSource = exp(*states.at(statesFxIdx).at(fxIdx++));
                        if (fxLinkedTargetCcyIdx > 0)
                            fxTarget = exp(*states.at(statesFxIdx).at(fxIdx));
                        fxFixing = fxSource / fxTarget;
                    }
                }
                RandomVariable eqFixing(n, 1.0);
                if (isEqIndexed) {
                    if (eqLinkedFixedPrice != Null<Real>()) {
                        eqFixing = RandomVariable(n, eqLinkedFixedPrice);
                    } else {
                        eqFixing = exp(*states.at(statesEqIdx).at(0));
                    }
                    eqFixing *= RandomVariable(n, eqLinkedQuantity);
                }
                return RandomVariable(n, (isFxLinked ? fxLinkedForeignNominal : av->nominal()) * av->accrualPeriod()) *
                       effectiveRate * fxFixing * eqFixing;
            };

        return info;
    }

    if (auto cfav = QuantLib::ext::dynamic_pointer_cast<CappedFlooredAverageONIndexedCoupon>(flow)) {
        Real simTime = std::max(0.0, time(cfav->underlying()->valueDates().front()));
        Size indexCcyIdx = model_->ccyIndex(cfav->underlying()->index()->currency());
        info.simulationTimes.push_back(simTime);
        info.modelIndices.push_back({model_->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(fxLinkedSimTime);
            info.modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(eqLinkedSimTime);
            info.modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        info.amountCalculator = [this, indexCcyIdx, cfav, simTime, isFxLinked, fxLinkedForeignNominal,
                                 fxLinkedSourceCcyIdx, fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isFxIndexed,
                                 isEqIndexed, eqLinkedFixedPrice, eqLinkedQuantity, statesFxIdx, statesEqIdx](
                                    const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            RandomVariable effectiveRate = lgmVectorised_[indexCcyIdx].averagedOnRate(
                cfav->underlying()->overnightIndex(), cfav->underlying()->fixingDates(),
                cfav->underlying()->valueDates(), cfav->underlying()->dt(), cfav->underlying()->rateCutoff(),
                cfav->includeSpread(), cfav->underlying()->spread(), cfav->underlying()->gearing(),
                cfav->underlying()->lookback(), cfav->cap(), cfav->floor(), cfav->localCapFloor(), cfav->nakedOption(),
                simTime, *states.at(0).at(0));
            RandomVariable fxFixing(n, 1.0);
            if (isFxLinked || isFxIndexed) {
                if (fxLinkedFixedFxRate != Null<Real>()) {
                    fxFixing = RandomVariable(n, fxLinkedFixedFxRate);
                } else {
                    RandomVariable fxSource(n, 1.0), fxTarget(n, 1.0);
                    Size fxIdx = 0;
                    if (fxLinkedSourceCcyIdx > 0)
                        fxSource = exp(*states.at(statesFxIdx).at(fxIdx++));
                    if (fxLinkedTargetCcyIdx > 0)
                        fxTarget = exp(*states.at(statesFxIdx).at(fxIdx));
                    fxFixing = fxSource / fxTarget;
                }
            }
            RandomVariable eqFixing(n, 1.0);
            if (isEqIndexed) {
                if (eqLinkedFixedPrice != Null<Real>()) {
                    eqFixing = RandomVariable(n, eqLinkedFixedPrice);
                } else {
                    eqFixing = exp(*states.at(statesEqIdx).at(0));
                }
                eqFixing *= RandomVariable(n, eqLinkedQuantity);
            }
            return RandomVariable(n, (isFxLinked ? fxLinkedForeignNominal : cfav->nominal()) * cfav->accrualPeriod()) *
                   effectiveRate * fxFixing * eqFixing;
        };

        return info;
    }

    if (auto bma = QuantLib::ext::dynamic_pointer_cast<AverageBMACoupon>(flow)) {
        Real simTime = std::max(0.0, time(bma->fixingDates().front()));
        Size indexCcyIdx = model_->ccyIndex(bma->index()->currency());
        info.simulationTimes.push_back(simTime);
        info.modelIndices.push_back({model_->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(fxLinkedSimTime);
            info.modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(eqLinkedSimTime);
            info.modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        info.amountCalculator = [this, indexCcyIdx, bma, simTime, isFxLinked, fxLinkedForeignNominal,
                                 fxLinkedSourceCcyIdx, fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isFxIndexed,
                                 isEqIndexed, eqLinkedFixedPrice, eqLinkedQuantity, statesFxIdx, statesEqIdx](
                                    const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            RandomVariable effectiveRate = lgmVectorised_[indexCcyIdx].averagedBmaRate(
                QuantLib::ext::dynamic_pointer_cast<BMAIndex>(bma->index()), bma->fixingDates(),
                bma->accrualStartDate(), bma->accrualEndDate(), false, bma->spread(), bma->gearing(), Null<Real>(),
                Null<Real>(), false, simTime, *states.at(0).at(0));
            RandomVariable fxFixing(n, 1.0);
            if (isFxLinked || isFxIndexed) {
                if (fxLinkedFixedFxRate != Null<Real>()) {
                    fxFixing = RandomVariable(n, fxLinkedFixedFxRate);
                } else {
                    RandomVariable fxSource(n, 1.0), fxTarget(n, 1.0);
                    Size fxIdx = 0;
                    if (fxLinkedSourceCcyIdx > 0)
                        fxSource = exp(*states.at(statesFxIdx).at(fxIdx++));
                    if (fxLinkedTargetCcyIdx > 0)
                        fxTarget = exp(*states.at(statesFxIdx).at(fxIdx));
                    fxFixing = fxSource / fxTarget;
                }
            }
            RandomVariable eqFixing(n, 1.0);
            if (isEqIndexed) {
                if (eqLinkedFixedPrice != Null<Real>()) {
                    eqFixing = RandomVariable(n, eqLinkedFixedPrice);
                } else {
                    eqFixing = exp(*states.at(statesEqIdx).at(0));
                }
                eqFixing *= RandomVariable(n, eqLinkedQuantity);
            }
            return RandomVariable(n, (isFxLinked ? fxLinkedForeignNominal : bma->nominal()) * bma->accrualPeriod()) *
                   effectiveRate * fxFixing * eqFixing;
        };

        return info;
    }

    if (auto cfbma = QuantLib::ext::dynamic_pointer_cast<CappedFlooredAverageBMACoupon>(flow)) {
        Real simTime = std::max(0.0, time(cfbma->underlying()->fixingDates().front()));
        Size indexCcyIdx = model_->ccyIndex(cfbma->underlying()->index()->currency());
        info.simulationTimes.push_back(simTime);
        info.modelIndices.push_back({model_->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(fxLinkedSimTime);
            info.modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(eqLinkedSimTime);
            info.modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        info.amountCalculator =
            [this, indexCcyIdx, cfbma, simTime, isFxLinked, fxLinkedForeignNominal, fxLinkedSourceCcyIdx,
             fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isFxIndexed, isEqIndexed, eqLinkedFixedPrice, eqLinkedQuantity,
             statesFxIdx, statesEqIdx](const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
                RandomVariable effectiveRate = lgmVectorised_[indexCcyIdx].averagedBmaRate(
                    QuantLib::ext::dynamic_pointer_cast<BMAIndex>(cfbma->underlying()->index()),
                    cfbma->underlying()->fixingDates(), cfbma->underlying()->accrualStartDate(),
                    cfbma->underlying()->accrualEndDate(), cfbma->includeSpread(), cfbma->underlying()->spread(),
                    cfbma->underlying()->gearing(), cfbma->cap(), cfbma->floor(), cfbma->nakedOption(), simTime,
                    *states.at(0).at(0));
                RandomVariable fxFixing(n, 1.0);
                if (isFxLinked || isFxIndexed) {
                    if (fxLinkedFixedFxRate != Null<Real>()) {
                        fxFixing = RandomVariable(n, fxLinkedFixedFxRate);
                    } else {
                        RandomVariable fxSource(n, 1.0), fxTarget(n, 1.0);
                        Size fxIdx = 0;
                        if (fxLinkedSourceCcyIdx > 0)
                            fxSource = exp(*states.at(statesFxIdx).at(fxIdx++));
                        if (fxLinkedTargetCcyIdx > 0)
                            fxTarget = exp(*states.at(statesFxIdx).at(fxIdx));
                        fxFixing = fxSource / fxTarget;
                    }
                }
                RandomVariable eqFixing(n, 1.0);
                if (isEqIndexed) {
                    if (eqLinkedFixedPrice != Null<Real>()) {
                        eqFixing = RandomVariable(n, eqLinkedFixedPrice);
                    } else {
                        eqFixing = exp(*states.at(statesEqIdx).at(0));
                    }
                    eqFixing *= RandomVariable(n, eqLinkedQuantity);
                }
                return RandomVariable(n, (isFxLinked ? fxLinkedForeignNominal : cfbma->underlying()->nominal()) *
                                             cfbma->underlying()->accrualPeriod()) *
                       effectiveRate * fxFixing * eqFixing;
            };

        return info;
    }

    if (auto sub = QuantLib::ext::dynamic_pointer_cast<SubPeriodsCoupon1>(flow)) {
        Real simTime = std::max(0.0, time(sub->fixingDates().front()));
        Size indexCcyIdx = model_->ccyIndex(sub->index()->currency());
        info.simulationTimes.push_back(simTime);
        info.modelIndices.push_back({model_->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(fxLinkedSimTime);
            info.modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            info.simulationTimes.push_back(eqLinkedSimTime);
            info.modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        info.amountCalculator = [this, indexCcyIdx, sub, simTime, isFxLinked, fxLinkedForeignNominal,
                                 fxLinkedSourceCcyIdx, fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isFxIndexed,
                                 isEqIndexed, eqLinkedFixedPrice, eqLinkedQuantity, statesFxIdx, statesEqIdx](
                                    const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            RandomVariable fixing = lgmVectorised_[indexCcyIdx].subPeriodsRate(
                sub->index(), sub->fixingDates(), simTime, *states.at(0).at(0), sub->accrualFractions(), sub->type(),
                sub->includeSpread(), sub->spread(), sub->gearing(), sub->accrualPeriod());
            RandomVariable fxFixing(n, 1.0);
            if (isFxLinked || isFxIndexed) {
                if (fxLinkedFixedFxRate != Null<Real>()) {
                    fxFixing = RandomVariable(n, fxLinkedFixedFxRate);
                } else {
                    RandomVariable fxSource(n, 1.0), fxTarget(n, 1.0);
                    Size fxIdx = 0;
                    if (fxLinkedSourceCcyIdx > 0)
                        fxSource = exp(*states.at(statesFxIdx).at(fxIdx++));
                    if (fxLinkedTargetCcyIdx > 0)
                        fxTarget = exp(*states.at(statesFxIdx).at(fxIdx));
                    fxFixing = fxSource / fxTarget;
                }
            }
            RandomVariable eqFixing(n, 1.0);
            if (isEqIndexed) {
                if (eqLinkedFixedPrice != Null<Real>()) {
                    eqFixing = RandomVariable(n, eqLinkedFixedPrice);
                } else {
                    eqFixing = exp(*states.at(statesEqIdx).at(0));
                }
                eqFixing *= RandomVariable(n, eqLinkedQuantity);
            }
            RandomVariable effectiveRate =
                RandomVariable(n, sub->gearing()) * fixing + RandomVariable(n, sub->spread());
            return RandomVariable(n, (isFxLinked ? fxLinkedForeignNominal : sub->nominal()) * sub->accrualPeriod()) *
                   effectiveRate * fxFixing * eqFixing;
        };

        return info;
    }

    if (auto eq = QuantLib::ext::dynamic_pointer_cast<EquityCoupon>(flow)) {

        QL_REQUIRE(!isFxLinked, "McMultiLegBaseEngine::createCashflowInfo(): equity coupon at leg "
                                    << legNo << " cashflow "
                                    << " is fx linked, this is not allowed");
        QL_REQUIRE(!isFxIndexed, "McMultiLegBaseEngine::createCashflowInfo(): equity coupon at leg "
                                     << legNo << " cashflow "
                                     << " is fx indexed, this is not allowed");
        QL_REQUIRE(!isEqIndexed, "McMultiLegBaseEngine::createCashflowInfo(): equity coupon at leg "
                                     << legNo << " cashflow "
                                     << " is eq indexed, this is not allowed");

        Size eqCcyIndex = model_->ccyIndex(eq->equityCurve()->currency());

        Size simTimeCounter = 0;
        Size irStartFixingIdx = Null<Size>();
        if (eq->fixingStartDate() != Date() && eq->fixingStartDate() > today_) {
            info.simulationTimes.push_back(time(eq->fixingStartDate()));
            info.modelIndices.push_back({model_->pIdx(CrossAssetModel::AssetType::IR, eqCcyIndex)});
            irStartFixingIdx = simTimeCounter++;
        }
        Size eqStartFixingIdx = Null<Size>();
        if (eq->fixingStartDate() != Date() && eq->fixingStartDate() > today_ &&
            eq->inputInitialPrice() == Null<Real>()) {
            info.simulationTimes.push_back(time(eq->fixingStartDate()));
            info.modelIndices.push_back(
                {model_->pIdx(CrossAssetModel::AssetType::EQ, model_->eqIndex(eq->equityCurve()->name()))});
            eqStartFixingIdx = simTimeCounter++;
        }
        Size eqEndFixingIdx = Null<Size>();
        if (eq->fixingEndDate() != Date() && eq->fixingEndDate() > today_) {
            info.simulationTimes.push_back(time(eq->fixingEndDate()));
            info.modelIndices.push_back(
                {model_->pIdx(CrossAssetModel::AssetType::EQ, model_->eqIndex(eq->equityCurve()->name()))});
            eqEndFixingIdx = simTimeCounter++;
        }

        Size fxStartFixingIdx = Null<Size>();
        Size fxEndFixingIdx = Null<Size>();
        Size fxSourceCcyIdx = Null<Size>();
        Size fxTargetCcyIdx = Null<Size>();
        if (eq->fxIndex()) {
            fxSourceCcyIdx = model_->ccyIndex(eq->fxIndex()->sourceCurrency());
            fxTargetCcyIdx = model_->ccyIndex(eq->fxIndex()->targetCurrency());
            std::vector<Size> fxModelIndices;
            if (fxSourceCcyIdx > 0) {
                fxModelIndices.push_back(model_->pIdx(CrossAssetModel::AssetType::FX, fxLinkedSourceCcyIdx - 1));
            }
            if (fxTargetCcyIdx > 0) {
                fxModelIndices.push_back(model_->pIdx(CrossAssetModel::AssetType::FX, fxLinkedTargetCcyIdx - 1));
            }
            if (!eq->initialPriceIsInTargetCcy() && eq->fixingStartDate() > today_) {
                info.simulationTimes.push_back(time(eq->fixingStartDate()));
                info.modelIndices.push_back(fxModelIndices);
                fxStartFixingIdx = simTimeCounter++;
            }
            if (eq->fixingEndDate() > today_) {
                info.simulationTimes.push_back(time(eq->fixingEndDate()));
                info.modelIndices.push_back(fxModelIndices);
                fxEndFixingIdx = simTimeCounter++;
            }
        }

        info.amountCalculator = [this, eq, irStartFixingIdx, eqStartFixingIdx, eqEndFixingIdx, fxStartFixingIdx,
                                 fxEndFixingIdx, fxSourceCcyIdx, fxTargetCcyIdx, eqCcyIndex](
                                    const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            RandomVariable initialPrice;
            if (eq->inputInitialPrice() != Null<Real>() || eq->fixingStartDate() <= today_) {
                initialPrice = RandomVariable(n, eq->initialPrice());
            } else {
                initialPrice = exp(*states.at(eqStartFixingIdx).at(0));
            }

            RandomVariable endFixing;
            if (eq->fixingEndDate() <= today_) {
                endFixing = RandomVariable(n, eq->equityCurve()->fixing(eq->fixingEndDate(), false, false));
            } else {
                endFixing = exp(*states.at(eqEndFixingIdx).at(0));
            }

            RandomVariable startFxFixing(n, 1.0);
            RandomVariable endFxFixing(n, 1.0);
            if (eq->fxIndex()) {
                if (!eq->initialPriceIsInTargetCcy()) {
                    if (eq->fixingStartDate() <= today_) {
                        startFxFixing = RandomVariable(n, eq->fxIndex()->fixing(eq->fixingStartDate()));
                    } else {
                        RandomVariable fxSource(n, 1.0), fxTarget(n, 1.0);
                        Size fxIdx = 0;
                        if (fxSourceCcyIdx > 0)
                            fxSource = exp(*states.at(fxStartFixingIdx).at(fxIdx++));
                        if (fxTargetCcyIdx > 0)
                            fxTarget = exp(*states.at(fxStartFixingIdx).at(fxIdx));
                        startFxFixing = fxSource / fxTarget;
                    }
                }
                if (eq->fixingEndDate() <= today_) {
                    endFxFixing = RandomVariable(n, eq->fxIndex()->fixing(eq->fixingEndDate()));
                } else {
                    RandomVariable fxSource(n, 1.0), fxTarget(n, 1.0);
                    Size fxIdx = 0;
                    if (fxSourceCcyIdx > 0)
                        fxSource = exp(*states.at(fxEndFixingIdx).at(fxIdx++));
                    if (fxTargetCcyIdx > 0)
                        fxTarget = exp(*states.at(fxEndFixingIdx).at(fxIdx));
                    endFxFixing = fxSource / fxTarget;
                }
            }

            /* Dividends: We support

               a) non-simulated dividend yields
               b) simulated diviend yields

               However, since b) is not yet supported in MC (cross asset scenario generator), we disable this part
               of the code for now. The eq forecast curve is always simulated. */

            // pick up historical dividends in the return period for both a) and b)

            RandomVariable dividends(
                n, eq->equityCurve()->dividendsBetweenDates(eq->fixingStartDate(), eq->fixingEndDate()));

            /* We approximate the calculation by compounding the equity price with the deterministic zero bond
               as seen from the fixing start date. The more precise calculation would require the stochastic bank
               account at fixing start and end date, which is not available through the standard lgm interface. */

            if (eq->fixingEndDate() > today_) {
                RandomVariable dividendBasePrice;
                RandomVariable irState;
                if (eq->fixingStartDate() == Date() || eq->fixingStartDate() <= today_) {
                    dividendBasePrice = RandomVariable(n, eq->equityCurve()->equitySpot()->value());
                    irState = RandomVariable(n, 0.0);
                } else {
                    dividendBasePrice = exp(*states.at(eqStartFixingIdx).at(0));
                    irState = *states.at(irStartFixingIdx).at(0);
                }
                Real fixingStartTime =
                    std::max(0.0, eq->fixingStartDate() == Date() ? 0.0 : time(eq->fixingStartDate()));
                Real fixingEndTime = time(eq->fixingEndDate());
                // a) non-simulated dividend yield curve
                RandomVariable divComp(n, eq->equityCurve()->equityDividendCurve()->discount(fixingEndTime) /
                                              eq->equityCurve()->equityDividendCurve()->discount(fixingStartTime));
                // b) simulated dividend yield curve
                // RandomVariable divComp = lgmVectorised_[eqCcyIndex].discountBond(fixingStartTime, fixingEndTime,
                // irState,
                //                                                       eq->equityCurve()->equityDividendCurve());
                // forecast curve is always simulated
                RandomVariable forecastComp = lgmVectorised_[eqCcyIndex].discountBond(
                    fixingStartTime, fixingEndTime, irState, eq->equityCurve()->equityForecastCurve());
                dividends += dividendBasePrice * (RandomVariable(n, 1.0) - divComp) / forecastComp;
            }

            RandomVariable swapletRate;
            if (eq->returnType() == EquityReturnType::Dividend) {
                swapletRate = dividends;
            } else if (eq->inputInitialPrice() == 0.0) {
                swapletRate = (endFixing + dividends * RandomVariable(n, eq->dividendFactor())) * endFxFixing;
            } else if (eq->returnType() == EquityReturnType::Absolute) {
                swapletRate = (endFixing + dividends * RandomVariable(n, eq->dividendFactor())) * endFxFixing -
                              initialPrice * startFxFixing;
            } else {
                swapletRate = ((endFixing + dividends * RandomVariable(n, eq->dividendFactor())) * endFxFixing -
                               initialPrice * startFxFixing) /
                              (initialPrice * startFxFixing);
            }

            RandomVariable nominal;
            if (eq->returnType() == EquityReturnType::Dividend) {
                nominal = RandomVariable(n, eq->quantity());
            } else if (eq->notionalReset()) {
                nominal = startFxFixing * RandomVariable(n, eq->quantity()) * initialPrice;
            } else {
                nominal = RandomVariable(n, eq->inputNominal());
            }
            return swapletRate * nominal;
        };
        return info;
    } // end of equity coupon handling

    if (auto eq = QuantLib::ext::dynamic_pointer_cast<EquityCashFlow>(flow)) {
        QL_REQUIRE(!isFxLinked, "McMultiLegBaseEngine::createCashflowInfo(): equity cashflow at leg "
                                    << legNo << " cashflow "
                                    << " is fx linked, this is not allowed");
        QL_REQUIRE(!isFxIndexed, "McMultiLegBaseEngine::createCashflowInfo(): equity cashflow at leg "
                                     << legNo << " cashflow "
                                     << " is fx indexed, this is not allowed");
        QL_REQUIRE(!isEqIndexed, "McMultiLegBaseEngine::createCashflowInfo(): equity cashflow at leg "
                                     << legNo << " cashflow "
                                     << " is eq indexed, this is not allowed");
        if (eq->fixingDate() > today_) {
            info.simulationTimes.push_back(time(eq->fixingDate()));
            info.modelIndices.push_back(
                {model_->pIdx(CrossAssetModel::AssetType::EQ, model_->eqIndex(eq->equityCurve()->name()))});
        }
        info.amountCalculator = [this, eq](const Size n,
                                           const std::vector<std::vector<const RandomVariable*>>& states) {
            return eq->fixingDate() <= today_ ? RandomVariable(n, eq->amount())
                                              : RandomVariable(n, eq->quantity()) * exp(*states.at(0).at(0));
        };
        return info;
    }

    QL_FAIL("McMultiLegBaseEngine::createCashflowInfo(): unhandled coupon leg " << legNo << " cashflow " << cfNo);
} // createCashflowInfo()

Size McMultiLegBaseEngine::timeIndex(const Time t, const std::set<Real>& times) const {
    auto it = times.find(t);
    QL_REQUIRE(it != times.end(), "McMultiLegBaseEngine::cashflowPathValue(): time ("
                                      << t
                                      << ") not found in simulation times. This is an internal error. Contact dev.");
    return std::distance(times.begin(), it);
}

RandomVariable McMultiLegBaseEngine::cashflowPathValue(const CashflowInfo& cf,
                                                       const std::vector<std::vector<RandomVariable>>& pathValues,
                                                       const std::set<Real>& simulationTimes) const {

    Size n = pathValues[0][0].size();
    auto simTimesPayIdx = timeIndex(cf.payTime, simulationTimes);

    std::vector<RandomVariable> initialValues(model_->stateProcess()->initialValues().size());
    for (Size i = 0; i < model_->stateProcess()->initialValues().size(); ++i)
        initialValues[i] = RandomVariable(n, model_->stateProcess()->initialValues()[i]);

    std::vector<std::vector<const RandomVariable*>> states(cf.simulationTimes.size());
    for (Size i = 0; i < cf.simulationTimes.size(); ++i) {
        std::vector<const RandomVariable*> tmp(cf.modelIndices[i].size());
        if (cf.simulationTimes[i] == 0.0) {
            for (Size j = 0; j < cf.modelIndices[i].size(); ++j) {
                tmp[j] = &initialValues[cf.modelIndices[i][j]];
            }
        } else {
            auto simTimesIdx = timeIndex(cf.simulationTimes[i], simulationTimes);
            for (Size j = 0; j < cf.modelIndices[i].size(); ++j) {
                tmp[j] = &pathValues[simTimesIdx][cf.modelIndices[i][j]];
            }
        }
        states[i] = tmp;
    }

    auto amount = cf.amountCalculator(n, states) /
                  lgmVectorised_[0].numeraire(
                      cf.payTime, pathValues[simTimesPayIdx][model_->pIdx(CrossAssetModel::AssetType::IR, 0)],
                      discountCurves_[0]);

    if (cf.payCcyIndex > 0) {
        amount *= exp(pathValues[simTimesPayIdx][model_->pIdx(CrossAssetModel::AssetType::FX, cf.payCcyIndex - 1)]);
    }

    return amount * RandomVariable(n, cf.payer ? -1.0 : 1.0);
}

void McMultiLegBaseEngine::calculateModels(
    const std::set<Real>& simulationTimes, const std::set<Real>& exerciseXvaTimes, const std::set<Real>& exerciseTimes,
    const std::set<Real>& xvaTimes, const std::vector<CashflowInfo>& cashflowInfo,
    const std::vector<std::vector<RandomVariable>>& pathValues,
    const std::vector<std::vector<const RandomVariable*>>& pathValuesRef,
    std::vector<RegressionModel>& regModelUndDirty, std::vector<RegressionModel>& regModelUndExInto,
    std::vector<RegressionModel>& regModelContinuationValue, std::vector<RegressionModel>& regModelOption,
    RandomVariable& pathValueUndDirty, RandomVariable& pathValueUndExInto, RandomVariable& pathValueOption) const {

    // for each xva and exercise time collect the relevant cashflow amounts and train a model on them

    enum class CfStatus { open, cached, done };
    std::vector<CfStatus> cfStatus(cashflowInfo.size(), CfStatus::open);

    std::vector<RandomVariable> amountCache(cashflowInfo.size());

    Size counter = exerciseXvaTimes.size() - 1;
    auto previousExerciseTime = exerciseTimes.rbegin();

    for (auto t = exerciseXvaTimes.rbegin(); t != exerciseXvaTimes.rend(); ++t) {

        bool isExerciseTime = exerciseTimes.find(*t) != exerciseTimes.end();
        bool isXvaTime = xvaTimes.find(*t) != xvaTimes.end();

        for (Size i = 0; i < cashflowInfo.size(); ++i) {

            if (cfStatus[i] == CfStatus::done)
                continue;

            /* We assume here that for each time t below the following condition holds: If a cashflow belongs to the
              "exercise into" part of the underlying, it also belongs to the underlying itself on each time t.

              Apart from that we allow for the possibility that a cashflow belongs to the underlying npv without
              belonging to the exercise into underlying at a time t. Such a cashflow would be marked as "cached" at time
              t and transferred to the exercise-into value at the appropriate time t' < t.
            */

            bool isPartOfExercise =
                cashflowInfo[i].payTime >
                    *t - (includeTodaysCashflows_ || exerciseIntoIncludeSameDayFlows_ ? tinyTime : 0.0) &&
                (previousExerciseTime == exerciseTimes.rend() ||
                 cashflowInfo[i].exIntoCriterionTime > *previousExerciseTime);

            bool isPartOfUnderlying = cashflowInfo[i].payTime > *t - (includeTodaysCashflows_ ? tinyTime : 0.0);

            if (cfStatus[i] == CfStatus::open) {
                if (isPartOfExercise) {
                    auto tmp = cashflowPathValue(cashflowInfo[i], pathValues, simulationTimes);
                    pathValueUndDirty += tmp;
                    pathValueUndExInto += tmp;
                    cfStatus[i] = CfStatus::done;
                } else if (isPartOfUnderlying) {
                    auto tmp = cashflowPathValue(cashflowInfo[i], pathValues, simulationTimes);
                    pathValueUndDirty += tmp;
                    amountCache[i] = tmp;
                    cfStatus[i] = CfStatus::cached;
                }
            } else if (cfStatus[i] == CfStatus::cached) {
                if (isPartOfExercise) {
                    pathValueUndExInto += amountCache[i];
                    cfStatus[i] = CfStatus::done;
                    amountCache[i].clear();
                }
            }
        }

        if (exercise_ != nullptr) {
            regModelUndExInto[counter] = RegressionModel(
                *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, **model_,
                regressorModel_, regressionVarianceCutoff_);
            regModelUndExInto[counter].train(polynomOrder_, polynomType_, pathValueUndExInto, pathValuesRef,
                                             simulationTimes);
        }

        if (isExerciseTime) {

            // calculate rebate (exercise fees) if existent

            RandomVariable pvRebate(calibrationSamples_, 0.0);
            if (auto rebatedExercise = QuantLib::ext::dynamic_pointer_cast<QuantExt::RebatedExercise>(exercise_)) {
                Size exerciseTimes_idx = std::distance(exerciseTimes.begin(), exerciseTimes.find(*t));
                if (rebatedExercise->rebate(exerciseTimes_idx) != 0.0) {
                    Size simulationTimes_idx = std::distance(simulationTimes.begin(), simulationTimes.find(*t));
                    RandomVariable rdb = lgmVectorised_[0].reducedDiscountBond(
                        *t, time(rebatedExercise->rebatePaymentDate(exerciseTimes_idx)),
                        pathValues[simulationTimes_idx][0], discountCurves_[0]);
                    pvRebate = rdb * RandomVariable(calibrationSamples_, rebatedExercise->rebate(exerciseTimes_idx));
                }
            }

            auto exerciseValue = regModelUndExInto[counter].apply(model_->stateProcess()->initialValues(),
                                                                  pathValuesRef, simulationTimes) +
                                 pvRebate;
            regModelContinuationValue[counter] = RegressionModel(
                *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, **model_,
                regressorModel_, regressionVarianceCutoff_);
            regModelContinuationValue[counter].train(polynomOrder_, polynomType_, pathValueOption, pathValuesRef,
                                                     simulationTimes,
                                                     exerciseValue > RandomVariable(calibrationSamples_, 0.0));
            auto continuationValue = regModelContinuationValue[counter].apply(model_->stateProcess()->initialValues(),
                                                                              pathValuesRef, simulationTimes);
            pathValueOption = conditionalResult(exerciseValue > continuationValue &&
                                                    exerciseValue > RandomVariable(calibrationSamples_, 0.0),
                                                pathValueUndExInto + pvRebate, pathValueOption);
        }

        if (isXvaTime) {
            regModelUndDirty[counter] = RegressionModel(
                *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] != CfStatus::open; }, **model_,
                regressorModel_, regressionVarianceCutoff_);
            regModelUndDirty[counter].train(polynomOrder_, polynomType_, pathValueUndDirty, pathValuesRef,
                                            simulationTimes);
        }

        if (exercise_ != nullptr) {
            regModelOption[counter] = RegressionModel(
                *t, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, **model_,
                regressorModel_, regressionVarianceCutoff_);
            regModelOption[counter].train(polynomOrder_, polynomType_, pathValueOption, pathValuesRef, simulationTimes);
        }

        if (isExerciseTime && previousExerciseTime != exerciseTimes.rend())
            std::advance(previousExerciseTime, 1);

        --counter;
    }

    // add the remaining live cashflows to get the underlying value

    for (Size i = 0; i < cashflowInfo.size(); ++i) {
        if (cfStatus[i] == CfStatus::open)
            pathValueUndDirty += cashflowPathValue(cashflowInfo[i], pathValues, simulationTimes);
    }
}

void McMultiLegBaseEngine::generatePathValues(const std::vector<Real>& simulationTimes,
                                              std::vector<std::vector<RandomVariable>>& pathValues) const {

    if (simulationTimes.empty())
        return;

    std::set<Real> times(simulationTimes.begin(), simulationTimes.end());

    TimeGrid timeGrid(times.begin(), times.end());

    QuantLib::ext::shared_ptr<StochasticProcess> process = model_->stateProcess();
    if (model_->dimension() == 1) {
        // use lgm process if possible for better performance
        auto tmp = QuantLib::ext::make_shared<IrLgm1fStateProcess>(model_->irlgm1f(0));
        tmp->resetCache(timeGrid.size() - 1);
        process = tmp;
    } else if (auto tmp = QuantLib::ext::dynamic_pointer_cast<CrossAssetStateProcess>(process)) {
        // enable cache
        tmp->resetCache(timeGrid.size() - 1);
    }

    auto pathGenerator = makeMultiPathGenerator(calibrationPathGenerator_, process, timeGrid, calibrationSeed_,
                                                ordering_, directionIntegers_);

    // generated paths always contain t = 0 but simulationTimes might or might not contain t = 0
    Size offset = QuantLib::close_enough(simulationTimes.front(), 0.0) ? 0 : 1;

    for (Size i = 0; i < calibrationSamples_; ++i) {
        const MultiPath& path = pathGenerator->next().value;
        for (Size j = 0; j < simulationTimes.size(); ++j) {
            for (Size k = 0; k < model_->stateProcess()->size(); ++k) {
                pathValues[j][k].data()[i] = path[k][j + offset];
            }
        }
    }
}

void McMultiLegBaseEngine::calculate() const {

    includeReferenceDateEvents_ = Settings::instance().includeReferenceDateEvents();
    includeTodaysCashflows_ = Settings::instance().includeTodaysCashFlows()
                                  ? *Settings::instance().includeTodaysCashFlows()
                                  : includeReferenceDateEvents_;

    McEngineStats::instance().other_timer.resume();

    // check data set by derived engines

    QL_REQUIRE(currency_.size() == leg_.size(), "McMultiLegBaseEngine: number of legs ("
                                                    << leg_.size() << ") does not match currencies ("
                                                    << currency_.size() << ")");
    QL_REQUIRE(payer_.size() == leg_.size(), "McMultiLegBaseEngine: number of legs ("
                                                 << leg_.size() << ") does not match payer flag (" << payer_.size()
                                                 << ")");
    QL_REQUIRE(exercise_ == nullptr || optionSettlement_ != Settlement::Cash ||
                   cashSettlementDates_.size() == exercise_->dates().size(),
               "McMultiLegBaseEngine: cash settled exercise is given but cash settlement dates size ("
                   << cashSettlementDates_.size() << ") does not match exercise dates size ("
                   << exercise_->dates().size()
                   << ". Check derived engine and make sure the settlement date is set for cash settled options.");

    // set today's date

    today_ = model_->irlgm1f(0)->termStructure()->referenceDate();

    // set up lgm vectorized instances for each currency

    if (lgmVectorised_.empty()) {
        for (Size i = 0; i < model_->components(CrossAssetModel::AssetType::IR); ++i) {
            lgmVectorised_.push_back(LgmVectorised(model_->irlgm1f(i)));
        }
    }

    // populate the info to generate the (alive) cashflow amounts

    std::vector<CashflowInfo> cashflowInfo;

    Size legNo = 0;
    for (auto const& leg : leg_) {
        Currency currency = currency_[legNo];
        bool payer = payer_[legNo];
        Size cashflowNo = 0;
        for (auto const& cashflow : leg) {
            // we can skip cashflows that are paid
            if (cashflow->date() < today_ || (!includeTodaysCashflows_ && cashflow->date() == today_))
                continue;
            // for an alive cashflow, populate the data
            cashflowInfo.push_back(createCashflowInfo(cashflow, currency, payer, legNo, cashflowNo));
            // increment counter
            ++cashflowNo;
        }
        ++legNo;
    }

    /* build exercise times, cash settlement times */

    std::set<Real> exerciseTimes;
    std::vector<Real> cashSettlementTimes;

    if (exercise_ != nullptr) {

        QL_REQUIRE(exercise_->type() != Exercise::American,
                   "McMultiLegBaseEngine::calculate(): exercise style American is not supported yet.");

        Size counter = 0;
        for (auto const& d : exercise_->dates()) {
            if (d < today_ || (!includeReferenceDateEvents_ && d == today_))
                continue;
            exerciseTimes.insert(time(d));
            if (optionSettlement_ == Settlement::Type::Cash)
                cashSettlementTimes.push_back(time(cashSettlementDates_[counter++]));
        }
    }

    /* build cashflow generation times */

    std::set<Real> cashflowGenTimes;

    for (auto const& info : cashflowInfo) {
        cashflowGenTimes.insert(info.simulationTimes.begin(), info.simulationTimes.end());
        cashflowGenTimes.insert(info.payTime);
    }

    /* build xva times, truncate at max time seen so far, but ensure at least two xva times */

    Real maxTime = 0.0;
    if (auto m = std::max_element(exerciseTimes.begin(), exerciseTimes.end()); m != exerciseTimes.end())
        maxTime = std::max(maxTime, *m);
    if (auto m = std::max_element(cashSettlementTimes.begin(), cashSettlementTimes.end());
        m != cashSettlementTimes.end())
        maxTime = std::max(maxTime, *m);
    if (auto m = std::max_element(cashflowGenTimes.begin(), cashflowGenTimes.end()); m != cashflowGenTimes.end())
        maxTime = std::max(maxTime, *m);

    std::set<Real> xvaTimes;

    for (auto const& d : simulationDates_) {
        if (auto t = time(d); t < maxTime + tinyTime) {
            xvaTimes.insert(time(d));
        }
    }

    /* build combined time sets */

    std::set<Real> exerciseXvaTimes; // = exercise + xva times
    std::set<Real> simulationTimes;  // = cashflowGen + exercise + xva times

    exerciseXvaTimes.insert(exerciseTimes.begin(), exerciseTimes.end());
    exerciseXvaTimes.insert(xvaTimes.begin(), xvaTimes.end());

    simulationTimes.insert(cashflowGenTimes.begin(), cashflowGenTimes.end());
    simulationTimes.insert(exerciseTimes.begin(), exerciseTimes.end());
    simulationTimes.insert(xvaTimes.begin(), xvaTimes.end());

    McEngineStats::instance().other_timer.stop();

    // build simulation times corresponding to close-out grid for sticky runs (if required)

    std::vector<Real> simulationTimesWithCloseOutLag;
    if (recalibrateOnStickyCloseOutDates_ && !stickyCloseOutDates_.empty()) {
        std::vector<Real> xvaTimesWithCloseOutLag(1, 0.0);
        for (auto const& d : stickyCloseOutDates_) {
            xvaTimesWithCloseOutLag.push_back(time(d));
        }
        std::vector<Real> xvaTimesVec(1, 0.0);
        xvaTimesVec.insert(xvaTimesVec.end(), xvaTimes.begin(), xvaTimes.end());
        Interpolation l = Linear().interpolate(xvaTimesVec.begin(), xvaTimesVec.end(), xvaTimesWithCloseOutLag.begin());
        l.enableExtrapolation();
        std::transform(simulationTimes.begin(), simulationTimes.end(),
                       std::back_inserter(simulationTimesWithCloseOutLag), [&l](const Real t) { return l(t); });
    }

    // simulate the paths for the calibration

    McEngineStats::instance().path_timer.resume();

    QL_REQUIRE(!simulationTimes.empty(),
               "McMultiLegBaseEngine::calculate(): no simulation times, this is not expected.");

    std::vector<std::vector<RandomVariable>> pathValues(
        simulationTimes.size(),
        std::vector<RandomVariable>(model_->stateProcess()->size(), RandomVariable(calibrationSamples_)));
    std::vector<std::vector<const RandomVariable*>> pathValuesRef(
        simulationTimes.size(), std::vector<const RandomVariable*>(model_->stateProcess()->size()));

    for (Size i = 0; i < pathValues.size(); ++i) {
        for (Size j = 0; j < pathValues[i].size(); ++j) {
            pathValues[i][j].expand();
            pathValuesRef[i][j] = &pathValues[i][j];
        }
    }

    std::vector<std::vector<RandomVariable>> closeOutPathValues(
        simulationTimesWithCloseOutLag.size(),
        std::vector<RandomVariable>(model_->stateProcess()->size(), RandomVariable(calibrationSamples_)));
    std::vector<std::vector<const RandomVariable*>> closeOutPathValuesRef(
        simulationTimesWithCloseOutLag.size(), std::vector<const RandomVariable*>(model_->stateProcess()->size()));

    for (Size i = 0; i < closeOutPathValues.size(); ++i) {
        for (Size j = 0; j < closeOutPathValues[i].size(); ++j) {
            closeOutPathValues[i][j].expand();
            closeOutPathValuesRef[i][j] = &closeOutPathValues[i][j];
        }
    }

    generatePathValues(std::vector<Real>(simulationTimes.begin(), simulationTimes.end()), pathValues);
    generatePathValues(simulationTimesWithCloseOutLag, closeOutPathValues);

    McEngineStats::instance().path_timer.stop();

    McEngineStats::instance().calc_timer.resume();

    // setup the models

    std::vector<RegressionModel> regModelUndDirty(exerciseXvaTimes.size());          // available on xva times
    std::vector<RegressionModel> regModelUndExInto(exerciseXvaTimes.size());         // available on xva and ex times
    std::vector<RegressionModel> regModelContinuationValue(exerciseXvaTimes.size()); // available on ex times
    std::vector<RegressionModel> regModelOption(exerciseXvaTimes.size());            // available on xva and ex times
    RandomVariable pathValueUndDirty(calibrationSamples_);
    RandomVariable pathValueUndExInto(calibrationSamples_);
    RandomVariable pathValueOption(calibrationSamples_);

    calculateModels(simulationTimes, exerciseXvaTimes, exerciseTimes, xvaTimes, cashflowInfo, pathValues, pathValuesRef,
                    regModelUndDirty, regModelUndExInto, regModelContinuationValue, regModelOption, pathValueUndDirty,
                    pathValueUndExInto, pathValueOption);

    // setup the models on close-out grid if required or else copy them from valuation

    std::vector<RegressionModel> regModelUndDirtyCloseOut(regModelUndDirty);
    std::vector<RegressionModel> regModelUndExIntoCloseOut(regModelUndExInto);
    std::vector<RegressionModel> regModelContinuationValueCloseOut(regModelContinuationValue);
    std::vector<RegressionModel> regModelOptionCloseOut(regModelOption);

    if (!simulationTimesWithCloseOutLag.empty()) {
        RandomVariable pathValueUndDirtyCloseOut(calibrationSamples_);
        RandomVariable pathValueUndExIntoCloseOut(calibrationSamples_);
        RandomVariable pathValueOptionCloseOut(calibrationSamples_);
        // everything stays the same, we just use the lagged path values
        calculateModels(simulationTimes, exerciseXvaTimes, exerciseTimes, xvaTimes, cashflowInfo, closeOutPathValues,
                        closeOutPathValuesRef, regModelUndDirtyCloseOut, regModelUndExIntoCloseOut,
                        regModelContinuationValueCloseOut, regModelOptionCloseOut, pathValueUndDirtyCloseOut,
                        pathValueUndExIntoCloseOut, pathValueOptionCloseOut);
    }

    // set the result value (= underlying value if no exercise is given, otherwise option value)

    resultUnderlyingNpv_ = expectation(pathValueUndDirty).at(0) * model_->numeraire(0, 0.0, 0.0, discountCurves_[0]);
    resultValue_ = exercise_ == nullptr
                       ? resultUnderlyingNpv_
                       : expectation(pathValueOption).at(0) * model_->numeraire(0, 0.0, 0.0, discountCurves_[0]);

    McEngineStats::instance().calc_timer.stop();

    // construct the amc calculator

    amcCalculator_ = QuantLib::ext::make_shared<MultiLegBaseAmcCalculator>(
        externalModelIndices_, optionSettlement_, cashSettlementTimes, exerciseXvaTimes, exerciseTimes, xvaTimes,
        std::array<std::vector<McMultiLegBaseEngine::RegressionModel>, 2>{regModelUndDirty, regModelUndDirtyCloseOut},
        std::array<std::vector<McMultiLegBaseEngine::RegressionModel>, 2>{regModelUndExInto, regModelUndExIntoCloseOut},
        std::array<std::vector<McMultiLegBaseEngine::RegressionModel>, 2>{regModelContinuationValue,
                                                                          regModelContinuationValueCloseOut},
        std::array<std::vector<McMultiLegBaseEngine::RegressionModel>, 2>{regModelOption, regModelOptionCloseOut},
        resultValue_, model_->stateProcess()->initialValues(), model_->irlgm1f(0)->currency(),
        reevaluateExerciseInStickyRun_, includeTodaysCashflows_, includeReferenceDateEvents_);
}

QuantLib::ext::shared_ptr<AmcCalculator> McMultiLegBaseEngine::amcCalculator() const { return amcCalculator_; }

McMultiLegBaseEngine::MultiLegBaseAmcCalculator::MultiLegBaseAmcCalculator(
    const std::vector<Size>& externalModelIndices, const Settlement::Type settlement,
    const std::vector<Time>& cashSettlementTimes, const std::set<Real>& exerciseXvaTimes,
    const std::set<Real>& exerciseTimes, const std::set<Real>& xvaTimes,
    const std::array<std::vector<McMultiLegBaseEngine::RegressionModel>, 2>& regModelUndDirty,
    const std::array<std::vector<McMultiLegBaseEngine::RegressionModel>, 2>& regModelUndExInto,
    const std::array<std::vector<McMultiLegBaseEngine::RegressionModel>, 2>& regModelContinuationValue,
    const std::array<std::vector<McMultiLegBaseEngine::RegressionModel>, 2>& regModelOption, const Real resultValue,
    const Array& initialState, const Currency& baseCurrency, const bool reevaluateExerciseInStickyRun,
    const bool includeTodaysCashflows, const bool includeReferenceDateEvents)
    : externalModelIndices_(externalModelIndices), settlement_(settlement), cashSettlementTimes_(cashSettlementTimes),
      exerciseXvaTimes_(exerciseXvaTimes), exerciseTimes_(exerciseTimes), xvaTimes_(xvaTimes),
      regModelUndDirty_(regModelUndDirty), regModelUndExInto_(regModelUndExInto),
      regModelContinuationValue_(regModelContinuationValue), regModelOption_(regModelOption), resultValue_(resultValue),
      initialState_(initialState), baseCurrency_(baseCurrency),
      reevaluateExerciseInStickyRun_(reevaluateExerciseInStickyRun), includeTodaysCashflows_(includeTodaysCashflows),
      includeReferenceDateEvents_(includeReferenceDateEvents) {

    QL_REQUIRE(settlement_ != Settlement::Type::Cash || cashSettlementTimes.size() == exerciseTimes.size(),
               "MultiLegBaseAmcCalculator: settlement type is cash, but cash settlement times ("
                   << cashSettlementTimes.size() << ") does not match exercise times (" << exerciseTimes.size() << ")");
}

std::vector<QuantExt::RandomVariable> McMultiLegBaseEngine::MultiLegBaseAmcCalculator::simulatePath(
    const std::vector<QuantLib::Real>& pathTimes, const std::vector<std::vector<QuantExt::RandomVariable>>& paths,
    const std::vector<size_t>& relevantPathIndex, const std::vector<size_t>& relevantTimeIndex) {

    // check input path consistency

    QL_REQUIRE(!paths.empty(), "MultiLegBaseAmcCalculator::simulatePath(): no future path times, this is not allowed.");
    QL_REQUIRE(pathTimes.size() == paths.size(),
               "MultiLegBaseAmcCalculator::simulatePath(): inconsistent pathTimes size ("
                   << pathTimes.size() << ") and paths size (" << paths.size() << ") - internal error.");
    QL_REQUIRE(relevantPathIndex.size() >= xvaTimes_.size(),
               "MultiLegBaseAmcCalculator::simulatePath() relevant path indexes ("
                   << relevantPathIndex.size() << ") >= xvaTimes (" << xvaTimes_.size()
                   << ") required - internal error.");

    bool stickyCloseOutRun = false;
    std::size_t regModelIndex = 0;

    for (size_t i = 0; i < relevantPathIndex.size(); ++i) {
        if (relevantPathIndex[i] != relevantTimeIndex[i]) {
            stickyCloseOutRun = true;
            regModelIndex = 1;
            break;
        }
    }

    /* put together the relevant simulation times on the input paths and check for consistency with xva times,
       also put together the effective paths by filtering on relevant simulation times and model indices */
    std::vector<std::vector<const RandomVariable*>> effPaths(
        xvaTimes_.size(), std::vector<const RandomVariable*>(externalModelIndices_.size()));

    Size timeIndex = 0;
    for (Size i = 0; i < xvaTimes_.size(); ++i) {
        size_t pathIdx = relevantPathIndex[i];
        for (Size j = 0; j < externalModelIndices_.size(); ++j) {
            effPaths[timeIndex][j] = &paths[pathIdx][externalModelIndices_[j]];
        }
        ++timeIndex;
    }

    // init result vector

    Size samples = paths.front().front().size();
    std::vector<RandomVariable> result(xvaTimes_.size() + 1, RandomVariable(paths.front().front().size(), 0.0));

    // simulate the path: result at first time index is simply the reference date npv

    result[0] = RandomVariable(samples, resultValue_);

    // if we don't have an exercise, we return the dirty npv of the underlying at all times

    if (exerciseTimes_.empty()) {
        Size counter = 0;
        for (auto t : xvaTimes_) {
            Size ind = std::distance(exerciseXvaTimes_.begin(), exerciseXvaTimes_.find(t));
            QL_REQUIRE(ind < exerciseXvaTimes_.size(),
                       "MultiLegBaseAmcCalculator::simulatePath(): internal error, xva time "
                           << t << " not found in exerciseXvaTimes vector.");
            result[++counter] = regModelUndDirty_[regModelIndex][ind].apply(initialState_, effPaths, xvaTimes_);
        }
        result.resize(relevantPathIndex.size() + 1, RandomVariable(samples, 0.0));
        return result;
    }

    /* if we have an exercise we need to determine the exercise indicators except for a sticky run
       where we reuse the last saved indicators */

    if (!stickyCloseOutRun || reevaluateExerciseInStickyRun_) {

        exercised_ = std::vector<Filter>(exerciseTimes_.size() + 1, Filter(samples, false));
        Size counter = 0;

        Filter wasExercised(samples, false);

        for (auto t : exerciseTimes_) {

            // find the time in the exerciseXvaTimes vector
            Size ind = std::distance(exerciseXvaTimes_.begin(), exerciseXvaTimes_.find(t));
            QL_REQUIRE(ind != exerciseXvaTimes_.size(),
                       "MultiLegBaseAmcCalculator::simulatePath(): internal error, exercise time "
                           << t << " not found in exerciseXvaTimes vector.");

            // make the exercise decision

            RandomVariable exerciseValue =
                regModelUndExInto_[regModelIndex][ind].apply(initialState_, effPaths, xvaTimes_);
            RandomVariable continuationValue =
                regModelContinuationValue_[regModelIndex][ind].apply(initialState_, effPaths, xvaTimes_);

            exercised_[counter + 1] =
                !wasExercised && exerciseValue > continuationValue && exerciseValue > RandomVariable(samples, 0.0);
            wasExercised = wasExercised || exercised_[counter + 1];

            ++counter;
        }
    }

    // now we can populate the result using the exercise indicators

    Size counter = 0;
    Size xvaCounter = 0;
    Size exerciseCounter = 0;

    Filter wasExercised(samples, false);
    std::map<Real, RandomVariable> cashSettlements;

    for (auto t : exerciseXvaTimes_) {

        if (auto it = exerciseTimes_.find(t); it != exerciseTimes_.end()) {

            // update was exercised based on exercise at the exercise time

            ++exerciseCounter;
            wasExercised = wasExercised || exercised_[exerciseCounter];

            // if cash settled, determine the amount on exercise and until when it is to be included in exposure

            if (settlement_ == Settlement::Type::Cash) {
                RandomVariable cashPayment =
                    regModelUndExInto_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_);
                cashPayment = applyFilter(cashPayment, exercised_[exerciseCounter]);
                cashSettlements[cashSettlementTimes_[exerciseCounter - 1]] = cashPayment;
            }
        }

        if (xvaTimes_.find(t) != xvaTimes_.end()) {

            // there is no continuation value on the last exercise date

            RandomVariable futureOptionValue =
                exerciseCounter == exerciseTimes_.size()
                    ? RandomVariable(samples, 0.0)
                    : regModelOption_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_);

            /* Physical Settlement:

               Exercise value is "undExInto" if we are in the period between the date on which the exercise happend
               and the next exercise date after that, otherwise it is the full dirty npv. This assumes that two
               exercise dates d1, d2 are not so close together that a coupon

               - pays after d1, d2
               - but does not belong to the exercise-into underlying for both d1 and d2

               This assumption seems reasonable, since we would never exercise on d1 but wait until d2 since the
               underlying which we exercise into is the same in both cases.
               We don't introduce a hard check for this, but we rather assume that the exercise dates are set up
               appropriately adjusted to the coupon periods. The worst that can happen is that the exercised value
               uses the full dirty npv at a too early time.

               Cash Settlement:

               We use the cashSettlements map constructed on each exercise date.

            */

            RandomVariable exercisedValue(samples, 0.0);

            if (settlement_ == Settlement::Type::Physical) {
                exercisedValue = conditionalResult(
                    exercised_[exerciseCounter],
                    regModelUndExInto_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_),
                    regModelUndDirty_[regModelIndex][counter].apply(initialState_, effPaths, xvaTimes_));
            } else {
                exercisedValue.setAll(0.0);
                for (auto it = cashSettlements.begin(); it != cashSettlements.end();) {
                    if (t < it->first + (includeTodaysCashflows_ ? tinyTime : -tinyTime)) {
                        exercisedValue += it->second;
                        ++it;
                    } else {
                        it = cashSettlements.erase(it);
                    }
                }
            }

            result[xvaCounter + 1] =
                max(RandomVariable(samples, 0.0), conditionalResult(wasExercised, exercisedValue, futureOptionValue));

            ++xvaCounter;
        }

        ++counter;
    }

    result.resize(relevantPathIndex.size() + 1, RandomVariable(samples, 0.0));
    return result;
}

template <class Archive> void McMultiLegBaseEngine::MultiLegBaseAmcCalculator::serialize(Archive& ar, const unsigned int version) {
    ar.template register_type<McMultiLegBaseEngine::MultiLegBaseAmcCalculator>();
    ar& boost::serialization::base_object<AmcCalculator>(*this);

    ar& externalModelIndices_;
    ar& settlement_;
    ar& cashSettlementTimes_;
    ar& exerciseXvaTimes_;
    ar& exerciseTimes_;
    ar& xvaTimes_;

    ar& regModelUndDirty_;
    ar& regModelUndExInto_;
    ar& regModelContinuationValue_;
    ar& regModelOption_;
    ar& resultValue_;
    ar& initialState_;
    ar& baseCurrency_;
    ar& reevaluateExerciseInStickyRun_;
    ar& includeTodaysCashflows_;
    ar& includeReferenceDateEvents_;
}

McMultiLegBaseEngine::RegressionModel::RegressionModel(const Real observationTime,
                                                       const std::vector<CashflowInfo>& cashflowInfo,
                                                       const std::function<bool(std::size_t)>& cashflowRelevant,
                                                       const CrossAssetModel& model,
                                                       const McMultiLegBaseEngine::RegressorModel regressorModel,
                                                       const Real regressionVarianceCutoff)
    : observationTime_(observationTime), regressionVarianceCutoff_(regressionVarianceCutoff) {

    // we always include the full model state as of the observation time

    for (Size m = 0; m < model.dimension(); ++m) {
        regressorTimesModelIndices_.insert(std::make_pair(observationTime, m));
    }

    // for LaggedFX we add past fx states

    if (regressorModel == McMultiLegBaseEngine::RegressorModel::LaggedFX) {

        std::set<Size> modelFxIndices;
        for (Size i = 1; i < model.components(CrossAssetModel::AssetType::IR); ++i) {
            for (Size j = 0; j < model.stateVariables(CrossAssetModel::AssetType::FX, i - 1); ++j)
                modelFxIndices.insert(model.pIdx(CrossAssetModel::AssetType::FX, i - 1, j));
        }

        for (Size i = 0; i < cashflowInfo.size(); ++i) {
            if (!cashflowRelevant(i))
                continue;
            // add the cashflow simulation indices
            for (Size j = 0; j < cashflowInfo[i].simulationTimes.size(); ++j) {
                Real t = std::min(observationTime_, cashflowInfo[i].simulationTimes[j]);
                // the simulation time might be zero, but then we want to skip the factors
                if (QuantLib::close_enough(t, 0.0))
                    continue;
                for (auto& m : cashflowInfo[i].modelIndices[j]) {
                    // add FX factors
                    if (modelFxIndices.find(m) != modelFxIndices.end())
                        regressorTimesModelIndices_.insert(std::make_pair(t, m));
                }
            }
        }
    }
}

void McMultiLegBaseEngine::RegressionModel::train(const Size polynomOrder,
                                                  const LsmBasisSystem::PolynomialType polynomType,
                                                  const RandomVariable& regressand,
                                                  const std::vector<std::vector<const RandomVariable*>>& paths,
                                                  const std::set<Real>& pathTimes, const Filter& filter) {

    // check if the model is in the correct state

    QL_REQUIRE(!isTrained_, "McMultiLegBaseEngine::RegressionModel::train(): internal error: model is already trained, "
                            "train() should not be called twice on the same model instance.");

    /* build the regressor - if the regressand is identically zero we leave it empty which optimizes
    out unnecessary calculations below */

    std::vector<const RandomVariable*> regressor;
    if (!regressand.deterministic() || !QuantLib::close_enough(regressand.at(0), 0.0)) {
        for (auto const& [t, modelIdx] : regressorTimesModelIndices_) {
            auto pt = pathTimes.find(t);
            QL_REQUIRE(pt != pathTimes.end(),
                       "McMultiLegBaseEngine::RegressionModel::train(): internal error: did not find regressor time "
                           << t << " in pathTimes.");
            regressor.push_back(paths[std::distance(pathTimes.begin(), pt)][modelIdx]);
        }
    }

    // factor reduction to reduce dimensionalitty and handle collinearity

    std::vector<RandomVariable> transformedRegressor;
    if (regressionVarianceCutoff_ != Null<Real>()) {
        coordinateTransform_ = pcaCoordinateTransform(regressor, regressionVarianceCutoff_);
        transformedRegressor = applyCoordinateTransform(regressor, coordinateTransform_);
        regressor = vec2vecptr(transformedRegressor);
    }

    // compute regression coefficients

    if (!regressor.empty()) {

        // get the basis functions
        basisDim_ = regressor.size();
        basisOrder_ = polynomOrder;
        basisType_ = polynomType;
        basisSystemSizeBound_ = Null<Size>();

        basisFns_ = multiPathBasisSystem(regressor.size(), polynomOrder, polynomType, {}, Null<Size>());

        // compute the regression coefficients

        regressionCoeffs_ =
            regressionCoefficients(regressand, regressor, basisFns_, filter, RandomVariableRegressionMethod::QR);

    } else {

        /* an empty regressor is possible if there are no relevant cashflows, but then the regressand
           has to be zero too */

        QL_REQUIRE(close_enough_all(regressand, RandomVariable(regressand.size(), 0.0)),
                   "McMultiLegBaseEngine::RegressionModel::train(): internal error: regressand is not identically "
                   "zero, but no regressor was built.");
    }

    // update state of model

    isTrained_ = true;
}

RandomVariable
McMultiLegBaseEngine::RegressionModel::apply(const Array& initialState,
                                             const std::vector<std::vector<const RandomVariable*>>& paths,
                                             const std::set<Real>& pathTimes) const {

    // check if model is trained

    QL_REQUIRE(isTrained_, "McMultiLegBaseEngine::RegressionMdeol::apply(): internal error: model is not trained.");

    // determine sample size

    QL_REQUIRE(!paths.empty() && !paths.front().empty(),
               "McMultiLegBaseEngine::RegressionMdeol::apply(): paths are empty or have empty first component");
    Size samples = paths.front().front()->size();

    // if we do not have regression coefficients, the regressand was zero

    if (regressionCoeffs_.empty())
        return RandomVariable(samples, 0.0);

    // build initial state pointer

    std::vector<RandomVariable> initialStateValues(initialState.size());
    std::vector<const RandomVariable*> initialStatePointer(initialState.size());
    for (Size j = 0; j < initialState.size(); ++j) {
        initialStateValues[j] = RandomVariable(samples, initialState[j]);
        initialStatePointer[j] = &initialStateValues[j];
    }

    // build the regressor

    std::vector<const RandomVariable*> regressor(regressorTimesModelIndices_.size());
    std::vector<RandomVariable> tmp(regressorTimesModelIndices_.size());

    Size i = 0;
    for (auto const& [t, modelIdx] : regressorTimesModelIndices_) {
        auto pt = pathTimes.find(t);
        if (pt != pathTimes.end()) {

            // the time is a path time, no need to interpolate the path

            regressor[i] = paths[std::distance(pathTimes.begin(), pt)][modelIdx];

        } else {

            // the time is not a path time, we need to interpolate:
            // find the sim times and model states before and after the exercise time

            auto t2 = std::lower_bound(pathTimes.begin(), pathTimes.end(), t);

            // t is after last path time => flat extrapolation

            if (t2 == pathTimes.end()) {
                regressor[i] = paths[pathTimes.size() - 1][modelIdx];
                ++i;
                continue;
            }

            // t is before last path time

            Real time2 = *t2;
            const RandomVariable* s2 = paths[std::distance(pathTimes.begin(), t2)][modelIdx];

            Real time1;
            const RandomVariable* s1;
            if (t2 == pathTimes.begin()) {
                time1 = 0.0;
                s1 = initialStatePointer[modelIdx];
            } else {
                time1 = *std::next(t2, -1);
                s1 = paths[std::distance(pathTimes.begin(), std::next(t2, -1))][modelIdx];
            }

            // compute the interpolated state

            RandomVariable alpha1(samples, (time2 - t) / (time2 - time1));
            RandomVariable alpha2(samples, (t - time1) / (time2 - time1));
            tmp[i] = alpha1 * *s1 + alpha2 * *s2;
            regressor[i] = &tmp[i];
        }
        ++i;
    }

    // transform regressor if necessary

    std::vector<RandomVariable> transformedRegressor;
    if (!coordinateTransform_.empty()) {
        transformedRegressor = applyCoordinateTransform(regressor, coordinateTransform_);
        regressor = vec2vecptr(transformedRegressor);
    }

    // compute result and return it

    return conditionalExpectation(regressor, basisFns_, regressionCoeffs_);
}

template <class Archive> void McMultiLegBaseEngine::RegressionModel::serialize(Archive& ar, const unsigned int version) {
    ar& observationTime_;
    ar& regressionVarianceCutoff_;
    ar& isTrained_;
    ar& regressorTimesModelIndices_;
    ar& coordinateTransform_;
    ar& regressionCoeffs_;

    // serialise the function by serialising the paramters needed
    ar& basisDim_;
    ar& basisOrder_;
    ar& basisType_;
    ar& basisSystemSizeBound_;

    // if deserialising, recreate the basisFns_ by passing the individual parameters to the function
    if (Archive::is_loading::value) {
        if (basisDim_ > 0)
            basisFns_ = multiPathBasisSystem(basisDim_, basisOrder_, basisType_, {}, basisSystemSizeBound_);
    }
}

template void QuantExt::McMultiLegBaseEngine::MultiLegBaseAmcCalculator::serialize(boost::archive::binary_iarchive& ar,
                                                                         const unsigned int version);
template void QuantExt::McMultiLegBaseEngine::MultiLegBaseAmcCalculator::serialize(boost::archive::binary_oarchive& ar,
                                                                         const unsigned int version);
template void QuantExt::McMultiLegBaseEngine::RegressionModel::serialize(boost::archive::binary_iarchive& ar,
                                                                         const unsigned int version);
template void QuantExt::McMultiLegBaseEngine::RegressionModel::serialize(boost::archive::binary_oarchive& ar,
                                                                         const unsigned int version);

} // namespace QuantExt

BOOST_CLASS_EXPORT_IMPLEMENT(QuantExt::McMultiLegBaseEngine::MultiLegBaseAmcCalculator);
BOOST_CLASS_EXPORT_IMPLEMENT(QuantExt::McMultiLegBaseEngine::RegressionModel);
