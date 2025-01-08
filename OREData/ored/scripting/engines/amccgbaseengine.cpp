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

namespace ore {
namespace data {

using namespace QuantLib;
using namespace QuantExt;

AmcCgBaseEngine::AmcCgBaseEngine(const QuantLib::ext::shared_ptr<ModelCG>& modelCg,
                                 const std::vector<QuantLib::Date>& simulationDates,
                                 const std::vector<QuantLib::Date>& stickyCloseOutDates,
                                 const bool recalibrateOnStickyCloseOutDates, const bool reevaluateExerciseInStickyRun)
    : modelCg_(modelCg), simulationDates_(simulationDates), stickyCloseOutDates_(stickyCloseOutDates),
      recalibrateOnStickyCloseOutDates_(recalibrateOnStickyCloseOutDates),
      reevaluateExerciseInStickyRun_(reevaluateExerciseInStickyRun) {

    // determine name of npv node, we can use anything that is not yet taken

    npvName_ = "__AMCCG_NPV_";
    std::size_t counter = 0;
    while (modelCg_->computationGraph()->variables().find(npvName_ + std::to_string(counter)) !=
           modelCg_->computationGraph()->variables().end())
        ++counter;
    npvName_ += std::to_string(counter);
}

std::string AmcCgBaseEngine::npvName() const { return npvName_; }

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
    info.payTime = time(flow->date());
    info.payCcy = payCcy;
    info.payer = payer;

    Real payMult = payer ? -1.0 : 1.0;

    if (auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(flow)) {
        QL_REQUIRE(cpn->accrualStartDate() < flow->date(),
                   "AmcCgBaseEngine::createCashflowInfo(): coupon leg "
                       << legNo << " cashflow " << cfNo << " has accrual start date (" << cpn->accrualStartDate()
                       << ") >= pay date (" << flow->date()
                       << "), which breaks an assumption in the engine. This situation is unexpected.");
        info.exIntoCriterionTime = time(cpn->accrualStartDate()) + tinyTime;
    } else {
        info.exIntoCriterionTime = info.payTime;
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
            modelCg_->fwdCompAvg(false, indexName, av->valueDates().front(), av->valueDates().front(),
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

void AmcCgBaseEngine::buildComputationGraph() const {

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

    // create the amc npv nodes

    QuantExt::ComputationGraph& g = *modelCg_->computationGraph();

    enum class CfStatus { open, cached, done };
    std::vector<CfStatus> cfStatus(cashflowInfo.size(), CfStatus::open);

    std::size_t pathValueUndDirty = cg_const(g, 0.0);

    for (int i = simulationDates_.size() - 1; i >= 0; --i) {

        Real t = time(simulationDates_[i]);

        std::vector<std::size_t> pathValueUndDirtyContribution(1, pathValueUndDirty);

        for (Size j = 0; j < cashflowInfo.size(); ++j) {

            /* we assume here that exIntoCriterionTime > t implies payTime > t, this must be ensured by the
               createCashflowInfo method */

            if (cfStatus[j] == CfStatus::open) {
                if (cashflowInfo[j].exIntoCriterionTime > t) {
                    pathValueUndDirtyContribution.push_back(cashflowInfo[j].flowNode);
                    cfStatus[j] = CfStatus::done;
                } else if (cashflowInfo[j].payTime > t - (includeTodaysCashflows_ ? tinyTime : 0.0)) {
                    pathValueUndDirtyContribution.push_back(cashflowInfo[j].flowNode);
                    cfStatus[j] = CfStatus::cached;
                }
            } else if (cfStatus[j] == CfStatus::cached) {
                if (cashflowInfo[j].exIntoCriterionTime > t) {
                    cfStatus[j] = CfStatus::done;
                }
            }
        }

        pathValueUndDirty = cg_add(g, pathValueUndDirtyContribution);

        g.setVariable("_AMC_NPV_" + std::to_string(i), pathValueUndDirty);
    }

    // add the remaining live cashflows to get the underlying value

    std::vector<std::size_t> pathValueUndDirtyContribution(1, pathValueUndDirty);
    for (Size i = 0; i < cashflowInfo.size(); ++i) {
        if (cfStatus[i] == CfStatus::open)
            pathValueUndDirtyContribution.push_back(cashflowInfo[i].flowNode);
    }

    pathValueUndDirty = cg_add(g, pathValueUndDirtyContribution);

    g.setVariable(npvName() + "_0", pathValueUndDirty);
}

void AmcCgBaseEngine::calculate() const {}

std::set<std::string> AmcCgBaseEngine::relevantCurrencies() const { return relevantCurrencies_; }

} // namespace data
} // namespace ore
