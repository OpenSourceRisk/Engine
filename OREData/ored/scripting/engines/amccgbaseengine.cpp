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

AmcCgBaseEngine::AmcCgBaseEngine(const QuantLib::ext::shared_ptr<ModelCG>& modelCg,
                                 const std::vector<QuantLib::Date>& simulationDates)
    : modelCg_(modelCg), simulationDates_(simulationDates) {

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

AmcCgBaseEngine::CashflowInfo AmcCgBaseEngine::createCashflowInfo(QuantLib::ext::shared_ptr<CashFlow> flow,
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
        info.flowNode = modelCg_->pay(cg_const(g, flow->amount()), flow->date(), flow->date(), payCcy);
        return info;
    }

    // handle the coupon types

    if (QuantLib::ext::dynamic_pointer_cast<FixedRateCoupon>(flow) != nullptr) {
        info.flowNode = modelCg_->pay(cg_const(g, flow->amount()), flow->date(), flow->date(), payCcy);
        return info;
    }

    if (auto ibor = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(flow)) {
        std::string indexName = IndexNameTranslator::instance().oreName(ibor->index()->name());
        Date obsDate = ibor->fixingDate();
        info.flowNode = modelCg_->pay(
            cg_mult(g, cg_const(g, ibor->nominal() * ibor->accrualPeriod()),
                    cg_add(g,
                           cg_mult(g, cg_const(g, ibor->gearing()), modelCg_->eval(indexName, obsDate, Null<Date>())),
                           cg_const(g, ibor->spread()))),
            obsDate, flow->date(), payCcy);
        return info;
    }

    QL_FAIL("McMultiLegBaseEngine::createCashflowInfo(): unhandled coupon leg " << legNo << " cashflow " << cfNo);
}

void AmcCgBaseEngine::buildComputationGraph() const {

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
                (!includeSettlementDateFlows_ && cashflow->date() == modelCg_->referenceDate()))
                continue;
            // for an alive cashflow, populate the data
            cashflowInfo.push_back(createCashflowInfo(cashflow, currency_[legNo], payer_[legNo], legNo, cashflowNo));
            // increment counter
            ++cashflowNo;
        }
        ++legNo;
    }

    // create the amc npv nodes

    QuantExt::ComputationGraph& g = *modelCg_->computationGraph();

    enum class CfStatus { open, cached, done };
    std::vector<CfStatus> cfStatus(cashflowInfo.size(), CfStatus::open);

    std::size_t pathValueUndDirty = cg_const(g, 0.0);

    for (int i = simulationDates_.size() - 1; i >= 0; --i) {

        Real t = time(simulationDates_[i]);

        for (Size i = 0; i < cashflowInfo.size(); ++i) {

            /* we assume here that exIntoCriterionTime > t implies payTime > t, this must be ensured by the
               createCashflowInfo method */

            if (cfStatus[i] == CfStatus::open) {
                if (cashflowInfo[i].exIntoCriterionTime > t) {
                    pathValueUndDirty = cg_add(g, pathValueUndDirty, cashflowInfo[i].flowNode);
                    cfStatus[i] = CfStatus::done;
                } else if (cashflowInfo[i].payTime > t - (includeSettlementDateFlows_ ? tinyTime : 0.0)) {
                    pathValueUndDirty = cg_add(g, pathValueUndDirty, cashflowInfo[i].flowNode);
                    cfStatus[i] = CfStatus::cached;
                }
            } else if (cfStatus[i] == CfStatus::cached) {
                if (cashflowInfo[i].exIntoCriterionTime > t) {
                    cfStatus[i] = CfStatus::done;
                }
            }
        }

        g.setVariable("_AMC_NPV_" + std::to_string(i), pathValueUndDirty);
    }

    // add the remaining live cashflows to get the underlying value

    for (Size i = 0; i < cashflowInfo.size(); ++i) {
        if (cfStatus[i] == CfStatus::open)
            pathValueUndDirty = cg_add(g, pathValueUndDirty, cashflowInfo[i].flowNode);
    }

    g.setVariable(npvName() + "_0", pathValueUndDirty);
}

void AmcCgBaseEngine::calculate() const {}

} // namespace data
} // namespace ore
