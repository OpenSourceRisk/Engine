/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

/*! \file amccgswapengine.hpp
    \brief AMC CG swap engine
    \ingroup engines
*/

#include <ored/scripting/engines/amccgbaseengine.hpp>

#include <ored/utilities/indexnametranslator.hpp>

#include <qle/ad/computationgraph.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/cappedflooredaveragebmacoupon.hpp>
#include <qle/cashflows/fixedratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>

#include <ql/cashflows/averagebmacoupon.hpp>
#include <ql/cashflows/capflooredcoupon.hpp>
#include <ql/cashflows/cmscoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/iborcoupon.hpp>
#include <ql/cashflows/simplecashflow.hpp>
#include <ql/exercise.hpp>
#include <ql/experimental/coupons/strippedcapflooredcoupon.hpp>
#include <ql/indexes/swapindex.hpp>
#include <ql/rebatedexercise.hpp>

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

AmcCgBaseEngine::AmcCgBaseEngine(const QuantLib::ext::shared_ptr<ModelCG>& modelCg,
                                 const std::vector<QuantLib::Date>& simulationDates)
    : modelCg_(modelCg), simulationDates_(simulationDates) {}

Real AmcCgBaseEngine::time(const Date& d) const {
    return QuantLib::ActualActual(QuantLib::ActualActual::ISDA).yearFraction(modelCg_->referenceDate(), d);
}

AmcCgBaseEngine::CashflowInfo AmcCgBaseEngine::createCashflowInfo(QuantLib::ext::shared_ptr<QuantLib::CashFlow> flow,
                                                                  const std::string& payCcy, bool payer, Size legNo,
                                                                  Size cfNo) const {

    CashflowInfo info;
    QuantExt::ComputationGraph& g = *modelCg_->computationGraph();

    // set some common info: pay time, pay ccy, payer, exercise into decision time

    info.legNo = legNo;
    info.cfNo = cfNo;
    info.payDate = flow->date();
    info.payCcy = payCcy;
    info.payer = payer;

    Real payMult = payer ? -1.0 : 1.0;

    auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(flow);
    if (cpn && cpn->accrualStartDate() < flow->date()) {
        info.exIntoCriterionDate = cpn->accrualStartDate() + 1;
    } else {
        info.exIntoCriterionDate = info.payDate + (exerciseIntoIncludeSameDayFlows_ ? 1 : 0);
    }

    // handle SimpleCashflow
    if (QuantLib::ext::dynamic_pointer_cast<SimpleCashFlow>(flow) != nullptr) {
        info.flowNode = modelCg_->pay(cg_const(g, payMult * flow->amount()), flow->date(), flow->date(), payCcy);
        return info;
    }

    // handle fx linked fixed cashflow
    if (auto fxl = QuantLib::ext::dynamic_pointer_cast<FXLinkedCashFlow>(flow)) {
        Date fxLinkedFixingDate = fxl->fxFixingDate();
        std::string fxIndex = IndexNameTranslator::instance().oreName(fxl->fxIndex()->name());
        info.flowNode = modelCg_->pay(
            cg_mult(g, cg_const(g, fxl->foreignAmount()), modelCg_->eval(fxIndex, fxLinkedFixingDate, Null<Date>())),
            flow->date(), flow->date(), payCcy);
        info.addCcys.insert(fxl->fxIndex()->sourceCurrency().code());
        info.addCcys.insert(fxl->fxIndex()->targetCurrency().code());
        return info;
    }

    // handle some wrapped coupon types: extract the wrapper info and continue with underlying flow
    bool isFxLinked = false;
    bool isFxIndexed = false;
    std::string fxLinkedIndex;
    Date fxLinkedFixingDate;
    Real fxLinkedForeignNominal = Null<Real>();

    // A Coupon could be wrapped in a FxLinkedCoupon or IndexedCoupon but not both at the same time
    if (auto indexCpn = QuantLib::ext::dynamic_pointer_cast<IndexedCoupon>(flow)) {
        if (auto fxIndex = QuantLib::ext::dynamic_pointer_cast<FxIndex>(indexCpn->index())) {
            isFxIndexed = true;
            fxLinkedFixingDate = indexCpn->fixingDate();
            fxLinkedIndex = IndexNameTranslator::instance().oreName(fxIndex->name());
            flow = indexCpn->underlying();
            info.addCcys.insert(fxIndex->sourceCurrency().code());
            info.addCcys.insert(fxIndex->targetCurrency().code());
        }
    } else if (auto fxl = QuantLib::ext::dynamic_pointer_cast<FloatingRateFXLinkedNotionalCoupon>(flow)) {
        isFxLinked = true;
        fxLinkedFixingDate = fxl->fxFixingDate();
        fxLinkedIndex = IndexNameTranslator::instance().oreName(fxl->fxIndex()->name());
        flow = fxl->underlying();
        fxLinkedForeignNominal = fxl->foreignAmount();
        info.addCcys.insert(fxl->fxIndex()->sourceCurrency().code());
        info.addCcys.insert(fxl->fxIndex()->targetCurrency().code());
    }

    std::size_t fxLinkedNode = ComputationGraph::nan;
    if (isFxLinked || isFxIndexed) {
        fxLinkedNode = modelCg_->eval(fxLinkedIndex, fxLinkedFixingDate, Null<Date>());
    }

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
        info.flowNode = modelCg_->pay(cg_const(g, payMult * flow->amount()), flow->date(), flow->date(), payCcy);
        return info;
    }

    if (auto ibor = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(flow)) {
        std::string indexName = IndexNameTranslator::instance().oreName(ibor->index()->name());
        std::size_t fixing = modelCg_->eval(indexName, ibor->fixingDate(), Null<Date>());

        info.addCcys.insert(ibor->index()->currency().code());

        std::size_t effectiveRate;
        if (isCapFloored) {
            std::size_t swapletRate = ComputationGraph::nan;
            std::size_t floorletRate = ComputationGraph::nan;
            std::size_t capletRate = ComputationGraph::nan;
            if (!isNakedOption)
                swapletRate = cg_add(g, cg_mult(g, cg_const(g, ibor->gearing()), fixing), cg_const(g, ibor->spread()));
            if (effFloor != Null<Real>())
                floorletRate = cg_mult(g, cg_const(g, ibor->gearing()),
                                       cg_max(g, cg_subtract(g, cg_const(g, effFloor), fixing), cg_const(g, 0.0)));
            if (effCap != Null<Real>())
                capletRate = cg_mult(g, cg_const(g, ibor->gearing()),
                                     cg_max(g, cg_subtract(g, fixing, cg_const(g, effCap)), cg_const(g, 0.0)));
            if (isNakedOption && effFloor == Null<Real>()) {
                capletRate = cg_mult(g, capletRate, cg_const(g, -1.0));
            }
            effectiveRate = swapletRate;
            if (floorletRate != ComputationGraph::nan)
                effectiveRate = cg_add(g, effectiveRate, floorletRate);
            if (capletRate != ComputationGraph::nan) {
                if (isNakedOption && effFloor == Null<Real>()) {
                    effectiveRate = cg_subtract(g, effectiveRate, capletRate);
                } else {
                    effectiveRate = cg_add(g, effectiveRate, capletRate);
                }
            }
        } else {
            effectiveRate = cg_add(g, cg_mult(g, cg_const(g, ibor->gearing()), fixing), cg_const(g, ibor->spread()));
        }

        info.flowNode =
            modelCg_->pay(cg_mult(g,
                                  cg_const(g, payMult * (isFxLinked ? fxLinkedForeignNominal : ibor->nominal()) *
                                                  ibor->accrualPeriod()),
                                  effectiveRate),
                          flow->date(), flow->date(), payCcy);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    if (auto cms = QuantLib::ext::dynamic_pointer_cast<CmsCoupon>(flow)) {
        std::string indexName = IndexNameTranslator::instance().oreName(cms->index()->name());
        std::size_t fixing = modelCg_->eval(indexName, cms->fixingDate(), Null<Date>());

        info.addCcys.insert(cms->index()->currency().code());

        std::size_t effectiveRate;
        if (isCapFloored) {
            std::size_t swapletRate = ComputationGraph::nan;
            std::size_t floorletRate = ComputationGraph::nan;
            std::size_t capletRate = ComputationGraph::nan;
            if (!isNakedOption)
                swapletRate = cg_add(g, cg_mult(g, cg_const(g, cms->gearing()), fixing), cg_const(g, cms->spread()));
            if (effFloor != Null<Real>())
                floorletRate = cg_mult(g, cg_const(g, cms->gearing()),
                                       cg_max(g, cg_subtract(g, cg_const(g, effFloor), fixing), cg_const(g, 0.0)));
            if (effCap != Null<Real>())
                capletRate = cg_mult(g, cg_const(g, cms->gearing()),
                                     cg_max(g, cg_subtract(g, fixing, cg_const(g, effCap)), cg_const(g, 0.0)));
            if (isNakedOption && effFloor == Null<Real>()) {
                capletRate = cg_mult(g, capletRate, cg_const(g, -1.0));
            }
            effectiveRate = swapletRate;
            if (floorletRate != ComputationGraph::nan)
                effectiveRate = cg_add(g, effectiveRate, floorletRate);
            if (capletRate != ComputationGraph::nan) {
                if (isNakedOption && effFloor == Null<Real>()) {
                    effectiveRate = cg_subtract(g, effectiveRate, capletRate);
                } else {
                    effectiveRate = cg_add(g, effectiveRate, capletRate);
                }
            }
        } else {
            effectiveRate = cg_add(g, cg_mult(g, cg_const(g, cms->gearing()), fixing), cg_const(g, cms->spread()));
        }

        info.flowNode = modelCg_->pay(
            cg_mult(
                g, cg_const(g, payMult * (isFxLinked ? fxLinkedForeignNominal : cms->nominal()) * cms->accrualPeriod()),
                effectiveRate),
            flow->date(), flow->date(), payCcy);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    if (auto on = QuantLib::ext::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(flow)) {
        std::string indexName = IndexNameTranslator::instance().oreName(on->index()->name());

        info.addCcys.insert(on->index()->currency().code());

        QL_REQUIRE(on->lookback().units() == QuantLib::Days,
                   "AmcCgBaseEngine::createCashflowInfo(): on coupon has lookback with units != Days ("
                       << on->lookback() << "), this is not allowed.");
        std::size_t fixing = modelCg_->fwdCompAvg(false, indexName, on->valueDates().front(), on->valueDates().front(),
                                                  on->valueDates().back(), on->spread(), on->gearing(),
                                                  on->lookback().length(), on->rateCutoff(), on->fixingDays(),
                                                  on->includeSpread(), Null<Real>(), Null<Real>(), false, false);
        info.flowNode = modelCg_->pay(
            cg_mult(g,
                    cg_const(g, payMult * (isFxLinked ? fxLinkedForeignNominal : on->nominal()) * on->accrualPeriod()),
                    fixing),
            flow->date(), flow->date(), payCcy);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    if (auto cfon = QuantLib::ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(flow)) {
        auto on = cfon->underlying();
        std::string indexName = IndexNameTranslator::instance().oreName(on->index()->name());

        info.addCcys.insert(on->index()->currency().code());

        QL_REQUIRE(on->lookback().units() == QuantLib::Days,
                   "AmcCgBaseEngine::createCashflowInfo(): cfon coupon has lookback with units != Days ("
                       << on->lookback() << "), this is not allowed.");
        std::size_t fixing = modelCg_->fwdCompAvg(
            false, indexName, on->valueDates().front(), on->valueDates().front(), on->valueDates().back(), on->spread(),
            on->gearing(), on->lookback().length(), on->rateCutoff(), on->fixingDays(), on->includeSpread(),
            cfon->cap(), cfon->floor(), cfon->nakedOption(), cfon->localCapFloor());
        info.flowNode = modelCg_->pay(
            cg_mult(g,
                    cg_const(g, payMult * (isFxLinked ? fxLinkedForeignNominal : on->nominal()) * on->accrualPeriod()),
                    fixing),
            flow->date(), flow->date(), payCcy);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    if (auto av = QuantLib::ext::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(flow)) {
        std::string indexName = IndexNameTranslator::instance().oreName(av->index()->name());

        info.addCcys.insert(av->index()->currency().code());

        QL_REQUIRE(av->lookback().units() == QuantLib::Days,
                   "AmcCgBaseEngine::createCashflowInfo(): av coupon has lookback with units != Days ("
                       << av->lookback() << "), this is not allowed.");
        std::size_t fixing =
            modelCg_->fwdCompAvg(true, indexName, av->valueDates().front(), av->valueDates().front(),
                                 av->valueDates().back(), av->spread(), av->gearing(), av->lookback().length(),
                                 av->rateCutoff(), av->fixingDays(), false, Null<Real>(), Null<Real>(), false, false);
        info.flowNode = modelCg_->pay(
            cg_mult(g,
                    cg_const(g, payMult * (isFxLinked ? fxLinkedForeignNominal : av->nominal()) * av->accrualPeriod()),
                    fixing),
            flow->date(), flow->date(), payCcy);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    if (auto cfav = QuantLib::ext::dynamic_pointer_cast<CappedFlooredAverageONIndexedCoupon>(flow)) {
        auto av = cfav->underlying();
        std::string indexName = IndexNameTranslator::instance().oreName(av->index()->name());

        info.addCcys.insert(av->index()->currency().code());

        QL_REQUIRE(av->lookback().units() == QuantLib::Days,
                   "AmcCgBaseEngine::createCashflowInfo(): cfon coupon has lookback with units != Days ("
                       << av->lookback() << "), this is not allowed.");
        std::size_t fixing = modelCg_->fwdCompAvg(
            false, indexName, av->valueDates().front(), av->valueDates().front(), av->valueDates().back(), av->spread(),
            av->gearing(), av->lookback().length(), av->rateCutoff(), av->fixingDays(), cfav->includeSpread(),
            cfav->cap(), cfav->floor(), cfav->nakedOption(), cfav->localCapFloor());
        info.flowNode = modelCg_->pay(
            cg_mult(g,
                    cg_const(g, payMult * (isFxLinked ? fxLinkedForeignNominal : av->nominal()) * av->accrualPeriod()),
                    fixing),
            flow->date(), flow->date(), payCcy);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    if (auto bma = QuantLib::ext::dynamic_pointer_cast<AverageBMACoupon>(flow)) {
        std::string indexName = IndexNameTranslator::instance().oreName(bma->index()->name());

        info.addCcys.insert(bma->index()->currency().code());

        std::size_t fixing = modelCg_->eval(indexName, bma->fixingDates().front(), Null<Date>());
        std::size_t effectiveRate =
            cg_add(g, cg_mult(g, cg_const(g, bma->gearing()), fixing), cg_const(g, bma->spread()));
        info.flowNode = modelCg_->pay(
            cg_mult(
                g, cg_const(g, payMult * (isFxLinked ? fxLinkedForeignNominal : bma->nominal()) * bma->accrualPeriod()),
                effectiveRate),
            flow->date(), flow->date(), payCcy);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    if (auto cfbma = QuantLib::ext::dynamic_pointer_cast<CappedFlooredAverageBMACoupon>(flow)) {
        auto bma = cfbma->underlying();
        std::string indexName = IndexNameTranslator::instance().oreName(bma->index()->name());
        std::size_t fixing = modelCg_->eval(indexName, bma->fixingDates().front(), Null<Date>());

        info.addCcys.insert(bma->index()->currency().code());

        effFloor = cfbma->effectiveFloor();
        effCap = cfbma->effectiveFloor();
        isNakedOption = cfbma->nakedOption();

        std::size_t effectiveRate;
        std::size_t swapletRate = ComputationGraph::nan;
        std::size_t floorletRate = ComputationGraph::nan;
        std::size_t capletRate = ComputationGraph::nan;
        if (!isNakedOption)
            swapletRate = cg_add(g, cg_mult(g, cg_const(g, bma->gearing()), fixing), cg_const(g, bma->spread()));
        if (effFloor != Null<Real>())
            floorletRate = cg_mult(g, cg_const(g, bma->gearing()),
                                   cg_max(g, cg_subtract(g, cg_const(g, effFloor), fixing), cg_const(g, 0.0)));
        if (effCap != Null<Real>())
            capletRate = cg_mult(g, cg_const(g, bma->gearing()),
                                 cg_max(g, cg_subtract(g, fixing, cg_const(g, effCap)), cg_const(g, 0.0)));
        if (isNakedOption && effFloor == Null<Real>()) {
            capletRate = cg_mult(g, capletRate, cg_const(g, -1.0));
        }
        effectiveRate = swapletRate;
        if (floorletRate != ComputationGraph::nan)
            effectiveRate = cg_add(g, effectiveRate, floorletRate);
        if (capletRate != ComputationGraph::nan) {
            if (isNakedOption && effFloor == Null<Real>()) {
                effectiveRate = cg_subtract(g, effectiveRate, capletRate);
            } else {
                effectiveRate = cg_add(g, effectiveRate, capletRate);
            }
        }

        info.flowNode = modelCg_->pay(
            cg_mult(
                g, cg_const(g, payMult * (isFxLinked ? fxLinkedForeignNominal : bma->nominal()) * bma->accrualPeriod()),
                effectiveRate),
            flow->date(), flow->date(), payCcy);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    if (auto sub = QuantLib::ext::dynamic_pointer_cast<SubPeriodsCoupon1>(flow)) {
        std::string indexName = IndexNameTranslator::instance().oreName(sub->index()->name());

        info.addCcys.insert(sub->index()->currency().code());

        std::size_t fixing = modelCg_->eval(indexName, sub->fixingDates().front(), Null<Date>());
        std::size_t effectiveRate =
            cg_add(g, cg_mult(g, cg_const(g, sub->gearing()), fixing), cg_const(g, sub->spread()));
        info.flowNode = modelCg_->pay(
            cg_mult(
                g, cg_const(g, payMult * (isFxLinked ? fxLinkedForeignNominal : sub->nominal()) * sub->accrualPeriod()),
                effectiveRate),
            flow->date(), flow->date(), payCcy);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    QL_FAIL("McMultiLegBaseEngine::createCashflowInfo(): unhandled coupon leg " << legNo << " cashflow " << cfNo);
}

void AmcCgBaseEngine::buildComputationGraph(const bool stickyCloseOutDateRun,
                                            const bool reevaluateExerciseInStickyCloseOutDateRun,
                                            std::vector<TradeExposure>* tradeExposure,
                                            TradeExposureMetaInfo* tradeExposureMetaInfo) const {

    QL_REQUIRE(tradeExposure, "AmcCgBaseEngine::buildComputationGraph(): tradeExposure is null, this is unexpected");
    QL_REQUIRE(tradeExposureMetaInfo,
               "AmcCgBaseEngine::buildComputationGraph(): tradeExposureMetaInfo is null, this is unexpected");

    // build graph

    QuantExt::ComputationGraph& g = *modelCg_->computationGraph();

    includeReferenceDateEvents_ = Settings::instance().includeReferenceDateEvents();
    includeTodaysCashflows_ = Settings::instance().includeTodaysCashFlows()
                                  ? *Settings::instance().includeTodaysCashFlows()
                                  : includeReferenceDateEvents_;

    relevantCurrencies_.clear();

    // check data set by derived engines

    QL_REQUIRE(currency_.size() == leg_.size(), "McMultiLegBaseEngine: number of legs ("
                                                    << leg_.size() << ") does not match currencies ("
                                                    << currency_.size() << ")");
    QL_REQUIRE(payer_.size() == leg_.size(), "McMultiLegBaseEngine: number of legs ("
                                                 << leg_.size() << ") does not match payer flag (" << payer_.size()
                                                 << ")");

    // populate the info to generate the (alive) cashflow amounts

    std::vector<CashflowInfo> cashflowInfo;

    Size legNo = 0;
    for (auto const& leg : leg_) {
        Size cashflowNo = 0;
        for (auto const& cashflow : leg) {
            // we can skip cashflows that are paid
            if (cashflow->date() < modelCg_->referenceDate() ||
                (!includeTodaysCashflows_ && cashflow->date() == modelCg_->referenceDate()))
                continue;
            // for an alive cashflow, populate the data
            cashflowInfo.push_back(createCashflowInfo(cashflow, currency_[legNo], payer_[legNo], legNo, cashflowNo));
            // increment counter
            ++cashflowNo;
        }
        ++legNo;
    }

    // populate relevant currency set

    for (auto const& c : cashflowInfo) {
        relevantCurrencies_.insert(c.payCcy);
        relevantCurrencies_.insert(c.addCcys.begin(), c.addCcys.end());
    }

    // populate trade exposure meta info

    tradeExposureMetaInfo->hasVega = exercise_ != nullptr;
    tradeExposureMetaInfo->relevantCurrencies = relevantCurrencies_;

    for (auto const& ccy : relevantCurrencies_) {
        tradeExposureMetaInfo->relevantModelParameters.insert(
            ModelCG::ModelParameter(ModelCG::ModelParameter::Type::dsc, ccy));
        if (ccy != modelCg_->baseCcy()) {
            tradeExposureMetaInfo->relevantModelParameters.insert(
                ModelCG::ModelParameter(ModelCG::ModelParameter::Type::logFxSpot, ccy));
        }
        if (tradeExposureMetaInfo->hasVega) {
            tradeExposureMetaInfo->relevantModelParameters.insert(
                ModelCG::ModelParameter(ModelCG::ModelParameter::Type::lgm_zeta, ccy));
            if (ccy != modelCg_->baseCcy()) {
                tradeExposureMetaInfo->relevantModelParameters.insert(
                    ModelCG::ModelParameter(ModelCG::ModelParameter::Type::fxbs_sigma, ccy));
            }
        }
    }

    /* build set of relevant exercise dates and corresponding cash settlement times (if applicable) */

    std::set<Date> exerciseDates;
    std::vector<Date> cashSettlementDates;

    if (exercise_ != nullptr) {

        QL_REQUIRE(exercise_->type() != Exercise::American,
                   "McMultiLegBaseEngine::calculate(): exercise style American is not supported yet.");

        Size counter = 0;
        for (auto const& d : exercise_->dates()) {
            if (d < modelCg_->referenceDate() || (!includeReferenceDateEvents_ && d == modelCg_->referenceDate()))
                continue;
            exerciseDates.insert(d);
            if (optionSettlement_ == Settlement::Type::Cash)
                cashSettlementDates.push_back(cashSettlementDates_[counter++]);
        }
    }

    // build the set of simulation dates and union of simulation and exercise dates

    std::set<Date> simDates(simulationDates_.begin(), simulationDates_.end());
    std::set<Date> simExDates;
    std::set_union(simDates.begin(), simDates.end(), exerciseDates.begin(), exerciseDates.end(),
                   std::inserter(simExDates, simExDates.end()));

    tradeExposure->clear();
    tradeExposure->resize(simDates.size() + 1);

    // create the path values

    std::size_t pathValueUndDirtyRunning = cg_const(g, 0.0);
    std::size_t pathValueUndExIntoRunning = cg_const(g, 0.0);

    std::vector<std::size_t> pathValueUndDirty(simExDates.size(), cg_const(g, 0.0));
    std::vector<std::size_t> pathValueUndExInto(simExDates.size(), cg_const(g, 0.0));
    std::vector<std::size_t> pathValueOption(simExDates.size() + 1, cg_const(g, 0.0)); // +1 for convenience
    std::vector<std::size_t> pathValueRebate(simExDates.size() + 1, cg_const(g, 0.0)); // +1 for convenience
    std::vector<std::size_t> exerciseIndicator(exerciseDates.size());

    cachedExerciseIndicators_.resize(exerciseIndicator.size(), ComputationGraph::nan);

    enum class CfStatus { open, cached, done };
    std::vector<CfStatus> cfStatus(cashflowInfo.size(), CfStatus::open);

    std::vector<std::size_t> amountCache(cashflowInfo.size(), ComputationGraph::nan);
    Size counter = simExDates.size() - 1;
    Size exerciseCounter = exerciseDates.size() - 1;
    auto previousExerciseDate = exerciseDates.rbegin();

    auto rebatedExercise = QuantLib::ext::dynamic_pointer_cast<QuantExt::RebatedExercise>(exercise_);

    for (auto d = simExDates.rbegin(); d != simExDates.rend(); ++d) {

        bool isExerciseDate = exerciseDates.find(*d) != exerciseDates.end();

        // collect the contributions so that we can generate a single add node in the graph
        std::vector<std::size_t> pathValueUndDirtyContribution(1, pathValueUndDirtyRunning);
        std::vector<std::size_t> pathValueUndExIntoContribution(1, pathValueUndExIntoRunning);

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
                cashflowInfo[i].payDate > *d - (includeTodaysCashflows_ || exerciseIntoIncludeSameDayFlows_ ? 1 : 0) &&
                (previousExerciseDate == exerciseDates.rend() ||
                 cashflowInfo[i].exIntoCriterionDate > *previousExerciseDate);

            bool isPartOfUnderlying = cashflowInfo[i].payDate > *d - (includeTodaysCashflows_ ? 1 : 0);

            if (cfStatus[i] == CfStatus::open) {
                if (isPartOfExercise) {
                    pathValueUndDirtyContribution.push_back(cashflowInfo[i].flowNode);
                    pathValueUndExIntoContribution.push_back(cashflowInfo[i].flowNode);
                    cfStatus[i] = CfStatus::done;
                } else if (isPartOfUnderlying) {
                    pathValueUndDirtyContribution.push_back(cashflowInfo[i].flowNode);
                    amountCache[i] = cashflowInfo[i].flowNode;
                    cfStatus[i] = CfStatus::cached;
                }
            } else if (cfStatus[i] == CfStatus::cached) {
                if (isPartOfExercise) {
                    pathValueUndExIntoContribution.push_back(amountCache[i]);
                    cfStatus[i] = CfStatus::done;
                    amountCache[i] = ComputationGraph::nan;
                }
            }
        }

        pathValueUndDirtyRunning = cg_add(g, pathValueUndDirtyContribution);
        pathValueUndExIntoRunning = cg_add(g, pathValueUndExIntoContribution);

        if (isExerciseDate) {

            // calculate rebate (exercise fees) if existent

            if (rebatedExercise) {
                Size exerciseTimes_idx = std::distance(exerciseDates.begin(), exerciseDates.find(*d));
                if (rebatedExercise->rebate(exerciseTimes_idx) != 0.0) {
                    // note: the silent assumption is that the rebate is paid in the first leg's currency!
                    pathValueRebate[counter] =
                        modelCg_->pay(cg_const(g, rebatedExercise->rebate(exerciseTimes_idx)), *d,
                                      rebatedExercise->rebatePaymentDate(exerciseTimes_idx), currency_.front());
                }
            }

            if (stickyCloseOutDateRun && !reevaluateExerciseInStickyCloseOutDateRun) {

                // reuse exercise indicator from previous run on valuation dates

                exerciseIndicator[exerciseCounter] = cachedExerciseIndicators_[exerciseCounter];

            } else {

                // determine exercise decision

                // calculate exercise and continuation value and derive exercise decision

                auto reg = createRegressionModel(
                    pathValueUndExIntoRunning, *d, cashflowInfo,
                    [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, cg_const(g, 1.0));

                auto exerciseValue = cg_add(g, reg, pathValueRebate[counter]);
                std::size_t filter = cg_indicatorGt(g, exerciseValue, cg_const(g, 0.0));
                auto continuationValue = createRegressionModel(
                    pathValueOption[counter + 1], *d, cashflowInfo,
                    [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, filter);

                exerciseIndicator[exerciseCounter] = cg_mult(g, cg_indicatorGt(g, exerciseValue, continuationValue),
                                                             cg_indicatorGt(g, exerciseValue, cg_const(g, 0.0)));

                cachedExerciseIndicators_[exerciseCounter] = exerciseIndicator[exerciseCounter];
            }

            pathValueOption[counter] =
                cg_add(g,
                       cg_mult(g, exerciseIndicator[exerciseCounter],
                               cg_add(g, pathValueUndExIntoRunning, pathValueRebate[counter])),
                       cg_mult(g, cg_subtract(g, cg_const(g, 1.0), exerciseIndicator[exerciseCounter]),
                               pathValueOption[counter + 1]));

            if (previousExerciseDate != exerciseDates.rend())
                std::advance(previousExerciseDate, 1);

            --exerciseCounter;

        } else {

            // populate pathValueOption and pathValueRebate on non-exercise dates

            pathValueOption[counter] = pathValueOption[counter + 1];
            pathValueRebate[counter] = pathValueRebate[counter + 1];
        }

        pathValueUndDirty[counter] = pathValueUndDirtyRunning;
        pathValueUndExInto[counter] = pathValueUndExIntoRunning;

        --counter;
    }

    // add the remaining live cashflows to get the underlying value

    std::vector<std::size_t> pathValueUndDirtyContribution(1, pathValueUndDirtyRunning);
    for (Size i = 0; i < cashflowInfo.size(); ++i) {
        if (cfStatus[i] == CfStatus::open)
            pathValueUndDirtyContribution.push_back(cashflowInfo[i].flowNode);
    }

    pathValueUndDirtyRunning = cg_add(g, pathValueUndDirtyContribution);

    // set the npv at t0

    (*tradeExposure)[0].componentPathValues = {exercise_ == nullptr ? pathValueUndDirtyRunning : pathValueOption[0]};

    // generate the exposure at simulation dates

    if (exerciseDates.empty()) {

        // if we don't have an exercise, we return the dirty npv of the underlying at all times

        for (Size counter = 0; counter < simDates.size(); ++counter) {
            (*tradeExposure)[counter + 1].componentPathValues = {pathValueUndDirty[counter]};
            (*tradeExposure)[counter + 1].regressors =
                modelCg_->npvRegressors(*std::next(simDates.begin(), counter), relevantCurrencies_);
        }

    } else {

        // iterative through simulation + exercise dates in forward direction

        Size counter = 0;
        Size simCounter = 0;
        Size exerciseCounter = 0;

        std::size_t isExercisedNow = cg_const(g, 0.0);
        std::size_t wasExercised = cg_const(g, 0.0);
        std::map<Date, std::size_t> cashSettlements;

        for (auto const& d : simExDates) {

            bool isExerciseDate = exerciseDates.find(d) != exerciseDates.end();
            bool isSimDate = simDates.find(d) != simDates.end();

            if (isExerciseDate) {

                ++exerciseCounter; // early increment here to be able to set futureOptionValue below correctly!

                // update was exercised based on exercise at the exercise time

                isExercisedNow =
                    cg_mult(g, cg_subtract(g, cg_const(g, 1.0), wasExercised), exerciseIndicator[exerciseCounter - 1]);
                wasExercised =
                    cg_min(g, cg_add(g, wasExercised, exerciseIndicator[exerciseCounter - 1]), cg_const(g, 1.0));

                // if cash settled, determine the amount on exercise and until when it is to be included in exposure

                if (optionSettlement_ == Settlement::Type::Cash) {
                    // 1) use conditional expectation as of exercise date
                    // auto reg = createRegressionModel(
                    //     pathValueUndExInto[counter], d, cashflowInfo,
                    //     [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, cg_const(g, 1.0));
                    // cashSettlements[cashSettlementDates_[exerciseCounter - 1]] = cg_mult(g, reg, isExercisedNow);
                    // 2) use path values
                    cashSettlements[cashSettlementDates_[exerciseCounter - 1]] =
                        cg_mult(g, pathValueUndExInto[counter], isExercisedNow);
                }
            }

            if (isSimDate) {

                // there is no continuation value on the last exercise date

                std::size_t futureOptionValue =
                    exerciseCounter == exerciseDates.size() ? cg_const(g, 0.0) : pathValueOption[counter];

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

                std::size_t exercisedValue = cg_const(g, 0.0);

                if (optionSettlement_ == Settlement::Type::Physical) {
                    exercisedValue = cg_add(
                        g, cg_mult(g, isExercisedNow, pathValueUndExInto[counter]),
                        cg_mult(g, cg_subtract(g, cg_const(g, 1.0), isExercisedNow), pathValueUndDirty[counter]));
                } else {
                    for (auto it = cashSettlements.begin(); it != cashSettlements.end();) {
                        if (d < it->first + (includeTodaysCashflows_ ? 1 : 0)) {
                            exercisedValue = cg_add(g, exercisedValue, it->second);
                            ++it;
                        } else {
                            it = cashSettlements.erase(it);
                        }
                    }
                }

                // update for rebate

                if (rebatedExercise)
                    exercisedValue = cg_add(g, exercisedValue, cg_mult(g, isExercisedNow, pathValueUndExInto[counter]));

                if (exerciseDates.size() == 1) {

                    // for european exercise we can rely on standard regression outside the engine

                    std::size_t r = cg_add(g, cg_mult(g, wasExercised, exercisedValue),
                                           cg_mult(g, cg_subtract(g, cg_const(g, 1.0), wasExercised), futureOptionValue));

                    (*tradeExposure)[simCounter + 1].componentPathValues = {r};
                    (*tradeExposure)[simCounter + 1].regressors = modelCg_->npvRegressors(d, relevantCurrencies_);

                } else {

                    // for more than one exercise date, we need a decomposition

                    (*tradeExposure)[simCounter + 1].componentPathValues = {exercisedValue, futureOptionValue};

                    std::size_t exercisedValueCond = createRegressionModel(
                        exercisedValue, d, cashflowInfo,
                        [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, cg_const(g, 1.0));
                    std::size_t futureOptionValueCond = createRegressionModel(
                        futureOptionValue, d, cashflowInfo,
                        [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; }, cg_const(g, 1.0),
                        &(*tradeExposure)[simCounter + 1]);

                    std::size_t r = cg_add(g, cg_mult(g, wasExercised, exercisedValueCond),
                                           cg_mult(g, cg_subtract(g, cg_const(g, 1.0), wasExercised),
                                                   cg_max(g, cg_const(g, 0.0), futureOptionValueCond)));

                    (*tradeExposure)[simCounter + 1].targetConditionalExpectation = r;
                    (*tradeExposure)[simCounter + 1].startNodeRecombine = exercisedValueCond;

                    (*tradeExposure)[simCounter + 1].additionalRequiredNodes.insert(wasExercised);
                    (*tradeExposure)[simCounter + 1].additionalRequiredNodes.insert(cg_const(g, 1.0));
                    (*tradeExposure)[simCounter + 1].additionalRequiredNodes.insert(cg_const(g, 0.0));

                }

                ++simCounter;
            }

            ++counter;
        }
    }
}

std::size_t AmcCgBaseEngine::createRegressionModel(const std::size_t amount, const Date& d,
                                                   const std::vector<CashflowInfo>& cashflowInfo,
                                                   const std::function<bool(std::size_t)>& cashflowRelevant,
                                                   const std::size_t filter, TradeExposure* tradeExposure) const {
    // TODO use relevant cashflow info to refine regressor if regressor model == LaggedFX
    auto regressors = modelCg_->npvRegressors(d, relevantCurrencies_);
    if (tradeExposure)
        tradeExposure->regressors = regressors;
    return modelCg_->npv(amount, d, filter, std::nullopt, {}, regressors);
}

void AmcCgBaseEngine::calculate() const {}

} // namespace data
} // namespace ore
