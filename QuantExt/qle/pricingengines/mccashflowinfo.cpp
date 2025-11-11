/*
 Copyright (C) 2025 Quaternion Risk Management Ltd
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

#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/cmscoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>
#include <ql/indexes/swapindex.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/cappedflooredaveragebmacoupon.hpp>
#include <qle/cashflows/equitycashflow.hpp>
#include <qle/cashflows/equitycoupon.hpp>
#include <qle/cashflows/fixedratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/iborfracoupon.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/cashflows/interpolatediborcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>
#include <qle/indexes/equityindex.hpp>
#include <qle/indexes/fxindex.hpp>
#include <qle/pricingengines/mccashflowinfo.hpp>

namespace QuantExt {

Real time(const Handle<CrossAssetModel>& model, const Date& d) {
    return model->irlgm1f(0)->termStructure()->timeFromReference(d);
}

CashflowInfo::CashflowInfo(QuantLib::ext::shared_ptr<CashFlow> flow, const Currency& payCcy, const bool payer,
                 const Size legNo, const Size cfNo, const Handle<CrossAssetModel>& model,
                 const std::vector<LgmVectorised>& lgmVectorised, const bool exerciseIntoIncludeSameDayFlows,
                 const double tinyTime, const Size cfOnCpnMaxSimTimes, const Period cfOnCpnAddSimTimesCutoff) {

    auto today = model->irlgm1f(0)->termStructure()->referenceDate();
    this->legNo = legNo;
    this->cfNo = cfNo;
    this->payTime = time(model, flow->date());
    this->payCcyIndex = model->ccyIndex(payCcy);
    this->payer = payer;

    auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(flow);
    if (cpn && cpn->accrualStartDate() < flow->date()) {
        this->exIntoCriterionTime = time(model, cpn->accrualStartDate()) + tinyTime;
    } else {
        this->exIntoCriterionTime = this->payTime + (exerciseIntoIncludeSameDayFlows ? tinyTime : 0.0);
    }

    // Handle SimpleCashflow
    if (QuantLib::ext::dynamic_pointer_cast<SimpleCashFlow>(flow) != nullptr) {
        this->amountCalculator = [flow](const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            return RandomVariable(n, flow->amount());
        };
        return;
    }

    // handle fx linked fixed cashflow
    if (auto fxl = QuantLib::ext::dynamic_pointer_cast<FXLinkedCashFlow>(flow)) {
        Date fxLinkedFixingDate = fxl->fxFixingDate();
        Size fxLinkedSourceCcyIdx = model->ccyIndex(fxl->fxIndex()->sourceCurrency());
        Size fxLinkedTargetCcyIdx = model->ccyIndex(fxl->fxIndex()->targetCurrency());
        if (fxLinkedFixingDate > today) {
            Real fxSimTime = time(model, fxLinkedFixingDate);
            this->simulationTimes.push_back(fxSimTime);
            this->modelIndices.push_back({});
            if (fxLinkedSourceCcyIdx > 0) {
                this->modelIndices.front().push_back(
                    model->pIdx(CrossAssetModel::AssetType::FX, fxLinkedSourceCcyIdx - 1));
            }
            if (fxLinkedTargetCcyIdx > 0) {
                this->modelIndices.front().push_back(
                    model->pIdx(CrossAssetModel::AssetType::FX, fxLinkedTargetCcyIdx - 1));
            }
        }
        this->amountCalculator = [today, fxLinkedSourceCcyIdx, fxLinkedTargetCcyIdx, fxLinkedFixingDate,
                                  fxl](const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            if (fxLinkedFixingDate <= today)
                return RandomVariable(n, fxl->amount());
            RandomVariable fxSource(n, 1.0), fxTarget(n, 1.0);
            Size fxIdx = 0;
            if (fxLinkedSourceCcyIdx > 0)
                fxSource = exp(*states.at(0).at(fxIdx++));
            if (fxLinkedTargetCcyIdx > 0)
                fxTarget = exp(*states.at(0).at(fxIdx));
            return RandomVariable(n, fxl->foreignAmount()) * fxSource / fxTarget;
        };
        return;
    }

    // handle some wrapped coupon types: extract the wrapper this-> and continue with underlying flow
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
                fxLinkedSourceCcyIdx = model->ccyIndex(fxIndex->sourceCurrency());
                fxLinkedTargetCcyIdx = model->ccyIndex(fxIndex->targetCurrency());
                if (fixingDate <= today) {
                    fxLinkedFixedFxRate = fxIndex->fixing(fixingDate);
                } else {
                    fxLinkedSimTime = time(model, fixingDate);
                    if (fxLinkedSourceCcyIdx > 0) {
                        fxLinkedModelIndices.push_back(
                            model->pIdx(CrossAssetModel::AssetType::FX, fxLinkedSourceCcyIdx - 1));
                    }
                    if (fxLinkedTargetCcyIdx > 0) {
                        fxLinkedModelIndices.push_back(
                            model->pIdx(CrossAssetModel::AssetType::FX, fxLinkedTargetCcyIdx - 1));
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
                eqLinkedIdx = model->eqIndex(eqIndex->name());
                eqLinkedQuantity = indexCpn->quantity();
                if (fixingDate <= today) {
                    eqLinkedFixedPrice = eqIndex->fixing(fixingDate);
                } else {
                    eqLinkedSimTime = time(model, fixingDate);
                    eqLinkedModelIndices.push_back(model->pIdx(CrossAssetModel::AssetType::EQ, eqLinkedIdx));
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
            fxLinkedSourceCcyIdx = model->ccyIndex(fxl->fxIndex()->sourceCurrency());
            fxLinkedTargetCcyIdx = model->ccyIndex(fxl->fxIndex()->targetCurrency());
            if (fixingDate <= today) {
                fxLinkedFixedFxRate = fxl->fxIndex()->fixing(fixingDate);
            } else {
                fxLinkedSimTime = time(model, fixingDate);
                if (fxLinkedSourceCcyIdx > 0) {
                    fxLinkedModelIndices.push_back(
                        model->pIdx(CrossAssetModel::AssetType::FX, fxLinkedSourceCcyIdx - 1));
                }
                if (fxLinkedTargetCcyIdx > 0) {
                    fxLinkedModelIndices.push_back(
                        model->pIdx(CrossAssetModel::AssetType::FX, fxLinkedTargetCcyIdx - 1));
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
            this->simulationTimes.push_back(fxLinkedSimTime);
            this->modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(eqLinkedSimTime);
            this->modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        this->amountCalculator = [flow, isFxLinked, isFxIndexed, fxLinkedFixedFxRate, fxLinkedSourceCcyIdx,
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
        return;
    }

    if (auto ibor = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(flow)) {
        Real fixedRate = ibor->fixingDate() <= today ? ibor->iborIndex()->fixing(ibor->fixingDate()) : Null<Real>();
        Size indexCcyIdx = model->ccyIndex(ibor->index()->currency());
        Real simTime = time(model, ibor->fixingDate());
        if (ibor->fixingDate() > today) {
            this->simulationTimes.push_back(simTime);
            this->modelIndices.push_back({model->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});
        }

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(fxLinkedSimTime);
            this->modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(eqLinkedSimTime);
            this->modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        this->amountCalculator = [&lgmVectorised, indexCcyIdx, ibor, simTime, fixedRate, isFxLinked, fxLinkedForeignNominal,
                                  fxLinkedSourceCcyIdx, fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isCapFloored,
                                  isNakedOption, effFloor, effCap, isFxIndexed, isEqIndexed, eqLinkedFixedPrice,
                                  eqLinkedQuantity, statesFxIdx, statesEqIdx](
                                     const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            RandomVariable fixing = fixedRate != Null<Real>()
                                        ? RandomVariable(n, fixedRate)
                                        : lgmVectorised[indexCcyIdx].fixing(ibor->index(), ibor->fixingDate(), simTime,
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

        return;
    }

    if (auto ibor = QuantLib::ext::dynamic_pointer_cast<InterpolatedIborCoupon>(flow)) {
        Real fixedRate = ibor->fixingDate() <= today ? ibor->iborIndex()->fixing(ibor->fixingDate()) : Null<Real>();
        Size indexCcyIdx = model->ccyIndex(ibor->index()->currency());
        Real simTime = time(model, ibor->fixingDate());
        if (ibor->fixingDate() > today) {
            this->simulationTimes.push_back(simTime);
            this->modelIndices.push_back({model->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});
        }

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(fxLinkedSimTime);
            this->modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(eqLinkedSimTime);
            this->modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        this->amountCalculator = [&lgmVectorised, indexCcyIdx, ibor, simTime, fixedRate, isFxLinked, fxLinkedForeignNominal,
                                  fxLinkedSourceCcyIdx, fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isCapFloored,
                                  isNakedOption, effFloor, effCap, isFxIndexed, isEqIndexed, eqLinkedFixedPrice,
                                  eqLinkedQuantity, statesFxIdx, statesEqIdx](
                                     const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            RandomVariable fixing;
            if (fixedRate != Null<Real>()) {
                fixing = RandomVariable(n, fixedRate);
            } else {
                auto interpolatedIndex =
                    QuantLib::ext::dynamic_pointer_cast<InterpolatedIborIndex>(ibor->interpolatedIborIndex());
                RandomVariable shortW = RandomVariable(n, interpolatedIndex->shortWeight(ibor->fixingDate()));
                RandomVariable longW = RandomVariable(n, interpolatedIndex->longWeight(ibor->fixingDate()));
                RandomVariable shortFixing = lgmVectorised[indexCcyIdx].fixing(
                    interpolatedIndex->shortIndex(), ibor->fixingDate(), simTime, *states.at(0).at(0));
                RandomVariable longFixing = lgmVectorised[indexCcyIdx].fixing(
                    interpolatedIndex->longIndex(), ibor->fixingDate(), simTime, *states.at(0).at(0));
                fixing = shortW * shortFixing + longW * longFixing;
            }
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

        return;
    }

    if (auto cms = QuantLib::ext::dynamic_pointer_cast<CmsCoupon>(flow)) {
        Real fixedRate = cms->fixingDate() <= today ? (cms->rate() - cms->spread()) / cms->gearing() : Null<Real>();
        Size indexCcyIdx = model->ccyIndex(cms->index()->currency());
        Real simTime = time(model, cms->fixingDate());
        if (cms->fixingDate() > today) {
            this->simulationTimes.push_back(simTime);
            this->modelIndices.push_back({model->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});
        }

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(fxLinkedSimTime);
            this->modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(eqLinkedSimTime);
            this->modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        this->amountCalculator = [&lgmVectorised, indexCcyIdx, cms, simTime, fixedRate, isFxLinked, fxLinkedForeignNominal,
                                  fxLinkedSourceCcyIdx, fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isCapFloored,
                                  isNakedOption, effFloor, effCap, isFxIndexed, isEqIndexed, eqLinkedFixedPrice,
                                  eqLinkedQuantity, statesFxIdx, statesEqIdx](
                                     const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            RandomVariable fixing =
                fixedRate != Null<Real>()
                    ? RandomVariable(n, fixedRate)
                    : lgmVectorised[indexCcyIdx].fixing(cms->index(), cms->fixingDate(), simTime, *states.at(0).at(0));
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

        return;
    }

    if (auto on = QuantLib::ext::dynamic_pointer_cast<OvernightIndexedCoupon>(flow)) {
        std::vector<Real> simTime;
        std::vector<Size> simIdx;
        Size indexCcyIdx = model->ccyIndex(on->index()->currency());

        std::vector<Size> relevantIdx;
        Time cutOffTime = time(model, today + cfOnCpnAddSimTimesCutoff);
        for (Size i = 0; i < on->fixingDates().size(); ++i) {
            auto t = time(model, on->fixingDates()[i]);
            if (t < 0.0 && i == 0 && cfOnCpnMaxSimTimes == 1) {
                relevantIdx.push_back(0);
                break;
            }
            if (t >= 0.0) {
                if (relevantIdx.size() == 0 || t <= cutOffTime) {
                    relevantIdx.push_back(i);
                }
            }
        }
        if (relevantIdx.empty()) {
            relevantIdx.push_back(0);
        }
        Size maxSimTimes = cfOnCpnMaxSimTimes == 0 ? relevantIdx.size() : cfOnCpnMaxSimTimes;
        Real step = std::max(relevantIdx.size() / static_cast<Real>(maxSimTimes), 1.0);
        for (Size i = 0; i < maxSimTimes; ++i) {
            Size idx = static_cast<Size>(i * step);
            if (idx >= relevantIdx.size()) {
                break;
            }
            Time t = std::max(time(model, on->fixingDates()[relevantIdx[idx]]), 0.0);
            simTime.push_back(t);
            simIdx.push_back(relevantIdx[idx]);
            this->simulationTimes.push_back(t);
            this->modelIndices.push_back({model->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});
        }

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(fxLinkedSimTime);
            this->modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(eqLinkedSimTime);
            this->modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        this->amountCalculator = [&lgmVectorised, indexCcyIdx, on, simTime, isFxLinked, fxLinkedForeignNominal,
                                  fxLinkedSourceCcyIdx, fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isFxIndexed,
                                  isEqIndexed, eqLinkedFixedPrice, eqLinkedQuantity, statesFxIdx, statesEqIdx,
                                  simIdx](const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            auto statesFcn = [&states = as_const(states)](Size i) -> const RandomVariable* {
                return states.at(i).at(0);
            };
            RandomVariable effectiveRate = lgmVectorised[indexCcyIdx].compoundedOnRate(
                on->overnightIndex(), on->fixingDates(), on->valueDates(), on->dt(), on->rateCutoff(),
                on->includeSpread(), on->spread(), on->gearing(), on->lookback(), Null<Real>(), Null<Real>(), false,
                false, simTime, simIdx, statesFcn);
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

        return;
    }

    if (auto cfon = QuantLib::ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(flow)) {
        std::vector<Real> simTime;
        std::vector<Size> simIdx;
        Size indexCcyIdx = model->ccyIndex(cfon->underlying()->index()->currency());

        std::vector<Size> relevantIdx;
        Time cutOffTime = time(model, today + cfOnCpnAddSimTimesCutoff);
        for (Size i = 0; i < cfon->underlying()->fixingDates().size(); ++i) {
            auto t = time(model, cfon->underlying()->fixingDates()[i]);
            if (t < 0.0 && i == 0 && cfOnCpnMaxSimTimes == 1) {
                relevantIdx.push_back(0);
                break;
            }
            if (t >= 0.0) {
                if (relevantIdx.size() == 0 || t <= cutOffTime) {
                    relevantIdx.push_back(i);
                }
            }
        }
        if (relevantIdx.empty()) {
            relevantIdx.push_back(0);
        }
        Size maxSimTimes = cfOnCpnMaxSimTimes == 0 ? relevantIdx.size() : cfOnCpnMaxSimTimes;
        Real step = std::max(relevantIdx.size() / static_cast<Real>(maxSimTimes), 1.0);
        for (Size i = 0; i < maxSimTimes; ++i) {
            Size idx = static_cast<Size>(i * step);
            if (idx >= relevantIdx.size()) {
                break;
            }
            Time t = std::max(time(model, cfon->underlying()->fixingDates()[relevantIdx[idx]]), 0.0);
            simTime.push_back(t);
            simIdx.push_back(relevantIdx[idx]);
            this->simulationTimes.push_back(t);
            this->modelIndices.push_back({model->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});
        }

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(fxLinkedSimTime);
            this->modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(eqLinkedSimTime);
            this->modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        this->amountCalculator = [&lgmVectorised, indexCcyIdx, cfon, simTime, isFxLinked, fxLinkedForeignNominal,
                                  fxLinkedSourceCcyIdx, fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isFxIndexed,
                                  isEqIndexed, eqLinkedFixedPrice, eqLinkedQuantity, statesFxIdx, statesEqIdx,
                                  simIdx](const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            auto statesFcn = [&states = as_const(states)](Size i) -> const RandomVariable* {
                return states.at(i).at(0);
            };
            RandomVariable effectiveRate = lgmVectorised[indexCcyIdx].compoundedOnRate(
                cfon->underlying()->overnightIndex(), cfon->underlying()->fixingDates(),
                cfon->underlying()->valueDates(), cfon->underlying()->dt(), cfon->underlying()->rateCutoff(),
                cfon->underlying()->includeSpread(), cfon->underlying()->spread(), cfon->underlying()->gearing(),
                cfon->underlying()->lookback(), cfon->cap(), cfon->floor(), cfon->localCapFloor(), cfon->nakedOption(),
                simTime, simIdx, statesFcn);
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

        return;
    }

    if (auto av = QuantLib::ext::dynamic_pointer_cast<AverageONIndexedCoupon>(flow)) {
        std::vector<Real> simTime;
        std::vector<Size> simIdx;
        Size indexCcyIdx = model->ccyIndex(av->index()->currency());

        std::vector<Size> relevantIdx;
        Time cutOffTime = time(model, today + cfOnCpnAddSimTimesCutoff);
        for (Size i = 0; i < av->fixingDates().size(); ++i) {
            auto t = time(model, av->fixingDates()[i]);
            if (t < 0.0 && i == 0 && cfOnCpnMaxSimTimes == 1) {
                relevantIdx.push_back(0);
                break;
            }
            if (t >= 0.0) {
                if (relevantIdx.size() == 0 || t <= cutOffTime) {
                    relevantIdx.push_back(i);
                }
            }
        }
        if (relevantIdx.empty()) {
            relevantIdx.push_back(0);
        }
        Size maxSimTimes = cfOnCpnMaxSimTimes == 0 ? relevantIdx.size() : cfOnCpnMaxSimTimes;
        Real step = std::max(relevantIdx.size() / static_cast<Real>(maxSimTimes), 1.0);
        for (Size i = 0; i < maxSimTimes; ++i) {
            Size idx = static_cast<Size>(i * step);
            if (idx >= relevantIdx.size()) {
                break;
            }
            Time t = std::max(time(model, av->fixingDates()[relevantIdx[idx]]), 0.0);
            simTime.push_back(t);
            simIdx.push_back(relevantIdx[idx]);
            this->simulationTimes.push_back(t);
            this->modelIndices.push_back({model->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});
        }

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(fxLinkedSimTime);
            this->modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(eqLinkedSimTime);
            this->modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        this->amountCalculator = [&lgmVectorised, indexCcyIdx, av, simTime, isFxLinked, fxLinkedForeignNominal,
                                  fxLinkedSourceCcyIdx, fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isFxIndexed,
                                  isEqIndexed, eqLinkedFixedPrice, eqLinkedQuantity, statesFxIdx, statesEqIdx,
                                  simIdx](const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            auto statesFcn = [&states = as_const(states)](Size i) -> const RandomVariable* {
                return states.at(i).at(0);
            };
            RandomVariable effectiveRate = lgmVectorised[indexCcyIdx].averagedOnRate(
                av->overnightIndex(), av->fixingDates(), av->valueDates(), av->dt(), av->rateCutoff(), false,
                av->spread(), av->gearing(), av->lookback(), Null<Real>(), Null<Real>(), false, false, simTime, simIdx,
                statesFcn);
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

        return;
    }

    if (auto cfav = QuantLib::ext::dynamic_pointer_cast<CappedFlooredAverageONIndexedCoupon>(flow)) {
        std::vector<Real> simTime;
        std::vector<Size> simIdx;
        Size indexCcyIdx = model->ccyIndex(cfav->underlying()->index()->currency());

        std::vector<Size> relevantIdx;
        Time cutOffTime = time(model, today + cfOnCpnAddSimTimesCutoff);
        for (Size i = 0; i < cfav->underlying()->fixingDates().size(); ++i) {
            auto t = time(model, cfav->underlying()->fixingDates()[i]);
            if (t < 0.0 && i == 0 && cfOnCpnMaxSimTimes == 1) {
                relevantIdx.push_back(0);
                break;
            }
            if (t >= 0.0) {
                if (relevantIdx.size() == 0 || t <= cutOffTime) {
                    relevantIdx.push_back(i);
                }
            }
        }
        if (relevantIdx.empty()) {
            relevantIdx.push_back(0);
        }
        Size maxSimTimes = cfOnCpnMaxSimTimes == 0 ? relevantIdx.size() : cfOnCpnMaxSimTimes;
        Real step = std::max(relevantIdx.size() / static_cast<Real>(maxSimTimes), 1.0);
        for (Size i = 0; i < maxSimTimes; ++i) {
            Size idx = static_cast<Size>(i * step);
            if (idx >= relevantIdx.size()) {
                break;
            }
            Time t = std::max(time(model, cfav->underlying()->fixingDates()[relevantIdx[idx]]), 0.0);
            simTime.push_back(t);
            simIdx.push_back(relevantIdx[idx]);
            this->simulationTimes.push_back(t);
            this->modelIndices.push_back({model->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});
        }

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(fxLinkedSimTime);
            this->modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(eqLinkedSimTime);
            this->modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        this->amountCalculator = [&lgmVectorised, indexCcyIdx, cfav, simTime, isFxLinked, fxLinkedForeignNominal,
                                  fxLinkedSourceCcyIdx, fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isFxIndexed,
                                  isEqIndexed, eqLinkedFixedPrice, eqLinkedQuantity, statesFxIdx, statesEqIdx,
                                  simIdx](const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            auto statesFcn = [&states = as_const(states)](Size i) -> const RandomVariable* {
                return states.at(i).at(0);
            };
            RandomVariable effectiveRate = lgmVectorised[indexCcyIdx].averagedOnRate(
                cfav->underlying()->overnightIndex(), cfav->underlying()->fixingDates(),
                cfav->underlying()->valueDates(), cfav->underlying()->dt(), cfav->underlying()->rateCutoff(),
                cfav->includeSpread(), cfav->underlying()->spread(), cfav->underlying()->gearing(),
                cfav->underlying()->lookback(), cfav->cap(), cfav->floor(), cfav->localCapFloor(), cfav->nakedOption(),
                simTime, simIdx, statesFcn);
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

        return;
    }

    if (auto bma = QuantLib::ext::dynamic_pointer_cast<AverageBMACoupon>(flow)) {
        Real simTime = std::max(0.0, time(model, bma->fixingDates().front()));
        Size indexCcyIdx = model->ccyIndex(bma->index()->currency());
        this->simulationTimes.push_back(simTime);
        this->modelIndices.push_back({model->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(fxLinkedSimTime);
            this->modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(eqLinkedSimTime);
            this->modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        this->amountCalculator = [&lgmVectorised, indexCcyIdx, bma, simTime, isFxLinked, fxLinkedForeignNominal,
                                  fxLinkedSourceCcyIdx, fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isFxIndexed,
                                  isEqIndexed, eqLinkedFixedPrice, eqLinkedQuantity, statesFxIdx, statesEqIdx](
                                     const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            RandomVariable effectiveRate = lgmVectorised[indexCcyIdx].averagedBmaRate(
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

        return;
    }

    if (auto cfbma = QuantLib::ext::dynamic_pointer_cast<CappedFlooredAverageBMACoupon>(flow)) {
        Real simTime = std::max(0.0, time(model, cfbma->underlying()->fixingDates().front()));
        Size indexCcyIdx = model->ccyIndex(cfbma->underlying()->index()->currency());
        this->simulationTimes.push_back(simTime);
        this->modelIndices.push_back({model->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(fxLinkedSimTime);
            this->modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(eqLinkedSimTime);
            this->modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        this->amountCalculator =
            [&lgmVectorised, indexCcyIdx, cfbma, simTime, isFxLinked, fxLinkedForeignNominal, fxLinkedSourceCcyIdx,
             fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isFxIndexed, isEqIndexed, eqLinkedFixedPrice, eqLinkedQuantity,
             statesFxIdx, statesEqIdx](const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
                RandomVariable effectiveRate = lgmVectorised[indexCcyIdx].averagedBmaRate(
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

        return;
    }

    if (auto sub = QuantLib::ext::dynamic_pointer_cast<SubPeriodsCoupon1>(flow)) {
        Real simTime = std::max(0.0, time(model, sub->fixingDates().front()));
        Size indexCcyIdx = model->ccyIndex(sub->index()->currency());
        this->simulationTimes.push_back(simTime);
        this->modelIndices.push_back({model->pIdx(CrossAssetModel::AssetType::IR, indexCcyIdx)});

        Size simTimeCounter = 1;
        Size statesFxIdx = Null<Size>();
        if (fxLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(fxLinkedSimTime);
            this->modelIndices.push_back(fxLinkedModelIndices);
            statesFxIdx = simTimeCounter++;
        }
        Size statesEqIdx = Null<Size>();
        if (eqLinkedSimTime != Null<Real>()) {
            this->simulationTimes.push_back(eqLinkedSimTime);
            this->modelIndices.push_back(eqLinkedModelIndices);
            statesEqIdx = simTimeCounter++;
        }

        this->amountCalculator = [&lgmVectorised, indexCcyIdx, sub, simTime, isFxLinked, fxLinkedForeignNominal,
                                  fxLinkedSourceCcyIdx, fxLinkedTargetCcyIdx, fxLinkedFixedFxRate, isFxIndexed,
                                  isEqIndexed, eqLinkedFixedPrice, eqLinkedQuantity, statesFxIdx, statesEqIdx](
                                     const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            RandomVariable fixing = lgmVectorised[indexCcyIdx].subPeriodsRate(
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
            return RandomVariable(n, (isFxLinked ? fxLinkedForeignNominal : sub->nominal()) * sub->accrualPeriod()) *
                   fixing * fxFixing * eqFixing;
        };

        return;
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

        Size eqCcyIndex = model->ccyIndex(eq->equityCurve()->currency());

        Size simTimeCounter = 0;
        Size irStartFixingIdx = Null<Size>();
        if (eq->fixingStartDate() != Date() && eq->fixingStartDate() > today) {
            this->simulationTimes.push_back(time(model, eq->fixingStartDate()));
            this->modelIndices.push_back({model->pIdx(CrossAssetModel::AssetType::IR, eqCcyIndex)});
            irStartFixingIdx = simTimeCounter++;
        }
        Size eqStartFixingIdx = Null<Size>();
        if (eq->fixingStartDate() != Date() && eq->fixingStartDate() > today &&
            eq->inputInitialPrice() == Null<Real>()) {
            this->simulationTimes.push_back(time(model, eq->fixingStartDate()));
            this->modelIndices.push_back(
                {model->pIdx(CrossAssetModel::AssetType::EQ, model->eqIndex(eq->equityCurve()->name()))});
            eqStartFixingIdx = simTimeCounter++;
        }
        Size eqEndFixingIdx = Null<Size>();
        if (eq->fixingEndDate() != Date() && eq->fixingEndDate() > today) {
            this->simulationTimes.push_back(time(model, eq->fixingEndDate()));
            this->modelIndices.push_back(
                {model->pIdx(CrossAssetModel::AssetType::EQ, model->eqIndex(eq->equityCurve()->name()))});
            eqEndFixingIdx = simTimeCounter++;
        }

        Size fxStartFixingIdx = Null<Size>();
        Size fxEndFixingIdx = Null<Size>();
        Size fxSourceCcyIdx = Null<Size>();
        Size fxTargetCcyIdx = Null<Size>();
        if (eq->fxIndex()) {
            fxSourceCcyIdx = model->ccyIndex(eq->fxIndex()->sourceCurrency());
            fxTargetCcyIdx = model->ccyIndex(eq->fxIndex()->targetCurrency());
            std::vector<Size> fxModelIndices;
            if (fxSourceCcyIdx > 0) {
                fxModelIndices.push_back(model->pIdx(CrossAssetModel::AssetType::FX, fxLinkedSourceCcyIdx - 1));
            }
            if (fxTargetCcyIdx > 0) {
                fxModelIndices.push_back(model->pIdx(CrossAssetModel::AssetType::FX, fxLinkedTargetCcyIdx - 1));
            }
            if (!eq->initialPriceIsInTargetCcy() && eq->fixingStartDate() > today) {
                this->simulationTimes.push_back(time(model, eq->fixingStartDate()));
                this->modelIndices.push_back(fxModelIndices);
                fxStartFixingIdx = simTimeCounter++;
            }
            if (eq->fixingEndDate() > today) {
                this->simulationTimes.push_back(time(model, eq->fixingEndDate()));
                this->modelIndices.push_back(fxModelIndices);
                fxEndFixingIdx = simTimeCounter++;
            }
        }

        this->amountCalculator = [&model, today, &lgmVectorised, eq, irStartFixingIdx, eqStartFixingIdx, eqEndFixingIdx, fxStartFixingIdx,
                                  fxEndFixingIdx, fxSourceCcyIdx, fxTargetCcyIdx, eqCcyIndex](
                                     const Size n, const std::vector<std::vector<const RandomVariable*>>& states) {
            RandomVariable initialPrice;
            if (eq->inputInitialPrice() != Null<Real>() || eq->fixingStartDate() <= today) {
                initialPrice = RandomVariable(n, eq->initialPrice());
            } else {
                initialPrice = exp(*states.at(eqStartFixingIdx).at(0));
            }

            RandomVariable endFixing;
            if (eq->fixingEndDate() <= today) {
                endFixing = RandomVariable(n, eq->equityCurve()->fixing(eq->fixingEndDate(), false, false));
            } else {
                endFixing = exp(*states.at(eqEndFixingIdx).at(0));
            }

            RandomVariable startFxFixing(n, 1.0);
            RandomVariable endFxFixing(n, 1.0);
            if (eq->fxIndex()) {
                if (!eq->initialPriceIsInTargetCcy()) {
                    if (eq->fixingStartDate() <= today) {
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
                if (eq->fixingEndDate() <= today) {
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

            if (eq->fixingEndDate() > today) {
                RandomVariable dividendBasePrice;
                RandomVariable irState;
                if (eq->fixingStartDate() == Date() || eq->fixingStartDate() <= today) {
                    dividendBasePrice = RandomVariable(n, eq->equityCurve()->equitySpot()->value());
                    irState = RandomVariable(n, 0.0);
                } else {
                    dividendBasePrice = exp(*states.at(eqStartFixingIdx).at(0));
                    irState = *states.at(irStartFixingIdx).at(0);
                }
                Real fixingStartTime =
                    std::max(0.0, eq->fixingStartDate() == Date() ? 0.0 : time(model, eq->fixingStartDate()));
                Real fixingEndTime = time(model, eq->fixingEndDate());
                // a) non-simulated dividend yield curve
                RandomVariable divComp(n, eq->equityCurve()->equityDividendCurve()->discount(fixingEndTime) /
                                              eq->equityCurve()->equityDividendCurve()->discount(fixingStartTime));
                // b) simulated dividend yield curve
                // RandomVariable divComp = lgmVectorised[eqCcyIndex].discountBond(fixingStartTime, fixingEndTime,
                // irState,
                //                                                       eq->equityCurve()->equityDividendCurve());
                // forecast curve is always simulated
                RandomVariable forecastComp = lgmVectorised[eqCcyIndex].discountBond(
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
        return;
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
        if (eq->fixingDate() > today) {
            this->simulationTimes.push_back(time(model, eq->fixingDate()));
            this->modelIndices.push_back(
                {model->pIdx(CrossAssetModel::AssetType::EQ, model->eqIndex(eq->equityCurve()->name()))});
        }
        this->amountCalculator = [today, eq](const Size n,
                                            const std::vector<std::vector<const RandomVariable*>>& states) {
            return eq->fixingDate() <= today ? RandomVariable(n, eq->amount())
                                              : RandomVariable(n, eq->quantity()) * exp(*states.at(0).at(0));
        };
    }

    QL_FAIL("McMultiLegBaseEngine::createCashflowInfo(): unhandled coupon leg " << legNo << " cashflow " << cfNo);
}

} // namespace QuantExt