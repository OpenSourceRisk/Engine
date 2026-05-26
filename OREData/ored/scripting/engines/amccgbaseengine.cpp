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
#include <ored/utilities/log.hpp>

#include <qle/ad/backwardderivatives.hpp>
#include <qle/ad/computationgraph.hpp>
#include <qle/ad/forwardevaluation.hpp>
#include <qle/ad/ssaform.hpp>
#include <qle/cashflows/averageonindexedcoupon.hpp>
#include <qle/cashflows/cappedflooredaveragebmacoupon.hpp>
#include <qle/cashflows/fixedratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/floatingratefxlinkednotionalcoupon.hpp>
#include <qle/cashflows/fxlinkedcashflow.hpp>
#include <qle/cashflows/indexedcoupon.hpp>
#include <qle/cashflows/interpolatediborcoupon.hpp>
#include <qle/cashflows/overnightindexedcoupon.hpp>
#include <qle/cashflows/scaledcoupon.hpp>
#include <qle/cashflows/subperiodscoupon.hpp>
#include <qle/instruments/rebatedexercise.hpp>
#include <qle/math/computeenvironment.hpp>
#include <qle/math/randomvariable.hpp>
#include <qle/math/randomvariable_ops.hpp>
#include <qle/methods/multipathvariategenerator.hpp>

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

AmcCgBaseEngine::AmcCgBaseEngine(const QuantLib::ext::shared_ptr<ModelCG>& modelCg, const Model::Params& mcParams,
                                 const double indicatorSmoothingForValues,
                                 const double indicatorSmoothingForDerivatives,
                                 const double sqrtSmoothingForDerivatives, const bool useCachedSensis,
                                 const bool useExternalComputeFramework,
                                 const bool useDoublePrecisionForExternalCalculation,
                                 const bool generateAdditionalResults)
    : modelCg_(modelCg), amcEnabled_(false), mcParams_(mcParams),
      indicatorSmoothingForValues_(indicatorSmoothingForValues),
      indicatorSmoothingForDerivatives_(indicatorSmoothingForDerivatives),
      sqrtSmoothingForDerivatives_(sqrtSmoothingForDerivatives), useCachedSensis_(useCachedSensis),
      useExternalComputeFramework_(useExternalComputeFramework),
      useDoublePrecisionForExternalCalculation_(useDoublePrecisionForExternalCalculation),
      generateAdditionalResults_(generateAdditionalResults) {

    opNodeRequirements_ = getRandomVariableOpNodeRequirements();
    ops_ = getRandomVariableOps(modelCg_->size(), mcParams_.regressionOrder, mcParams_.polynomType,
                                indicatorSmoothingForValues_, mcParams_.regressionVarianceCutoff);
    grads_ = getRandomVariableGradients(modelCg_->size(), mcParams_.regressionOrder, mcParams_.polynomType,
                                        indicatorSmoothingForDerivatives_, sqrtSmoothingForDerivatives_,
                                        mcParams_.regressionVarianceCutoff);
}

AmcCgBaseEngine::AmcCgBaseEngine(const QuantLib::ext::shared_ptr<ModelCG>& modelCg,
                                 const std::vector<QuantLib::Date>& simulationDates,
                                 const bool reevaluateExerciseInStickyCloseOutDateRun)
    : modelCg_(modelCg), amcEnabled_(true), simulationDates_(simulationDates),
      reevaluateExerciseInStickyCloseOutDateRun_(reevaluateExerciseInStickyCloseOutDateRun) {}

std::set<std::string> AmcCgBaseEngine::getCashflowCurrencies(QuantLib::ext::shared_ptr<QuantLib::CashFlow> flow,
                                                             const std::string& payCcy) {
    std::set<std::string> currencies;

    currencies.insert(payCcy);

    if (auto fxl = QuantLib::ext::dynamic_pointer_cast<FXLinkedCashFlow>(flow)) {
        currencies.insert(fxl->fxIndex()->sourceCurrency().code());
        currencies.insert(fxl->fxIndex()->targetCurrency().code());
    }

    if (auto indexCpn = QuantLib::ext::dynamic_pointer_cast<IndexedCoupon>(flow)) {
        if (auto fxIndex = QuantLib::ext::dynamic_pointer_cast<FxIndex>(indexCpn->index())) {
            currencies.insert(fxIndex->sourceCurrency().code());
            currencies.insert(fxIndex->targetCurrency().code());
        }
    } else if (auto fxl = QuantLib::ext::dynamic_pointer_cast<FloatingRateFXLinkedNotionalCoupon>(flow)) {
        currencies.insert(fxl->fxIndex()->sourceCurrency().code());
        currencies.insert(fxl->fxIndex()->targetCurrency().code());
    }

    if (auto ibor = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(flow)) {
        currencies.insert(ibor->index()->currency().code());
        return currencies;
    }
    if (auto ibor = QuantLib::ext::dynamic_pointer_cast<InterpolatedIborCoupon>(flow)) {
        currencies.insert(ibor->index()->currency().code());
        return currencies;
    }
    if (auto cms = QuantLib::ext::dynamic_pointer_cast<CmsCoupon>(flow)) {
        currencies.insert(cms->index()->currency().code());
        return currencies;
    }
    if (auto on = QuantLib::ext::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(flow)) {
        currencies.insert(on->index()->currency().code());
        return currencies;
    }
    if (auto cfon = QuantLib::ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(flow)) {
        auto on = cfon->underlying();
        currencies.insert(on->index()->currency().code());
        return currencies;
    }
    if (auto av = QuantLib::ext::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(flow)) {
        currencies.insert(av->index()->currency().code());
        return currencies;
    }
    if (auto cfav = QuantLib::ext::dynamic_pointer_cast<CappedFlooredAverageONIndexedCoupon>(flow)) {
        auto av = cfav->underlying();
        currencies.insert(av->index()->currency().code());
        return currencies;
    }
    if (auto bma = QuantLib::ext::dynamic_pointer_cast<AverageBMACoupon>(flow)) {
        currencies.insert(bma->index()->currency().code());
        return currencies;
    }
    if (auto cfbma = QuantLib::ext::dynamic_pointer_cast<CappedFlooredAverageBMACoupon>(flow)) {
        auto bma = cfbma->underlying();
        currencies.insert(bma->index()->currency().code());
        return currencies;
    }
    if (auto sub = QuantLib::ext::dynamic_pointer_cast<SubPeriodsCoupon1>(flow)) {
        currencies.insert(sub->index()->currency().code());
        return currencies;
    }

    return currencies;
}

AmcCgBaseEngine::CashflowInfo
AmcCgBaseEngine::createCashflowInfo(QuantLib::ext::shared_ptr<QuantLib::CashFlow> flow, const std::string& payCcy,
                                    const bool payer, const Size legNo, const Size cfNo,
                                    const std::map<std::set<std::string>, std::string>& baseCurrencySuggestions) const {

    CashflowInfo info;
    QuantExt::ComputationGraph& g = *modelCg_->computationGraph();

    // set some common info

    info.legNo = legNo;
    info.cfNo = cfNo;
    info.payDate = flow->date();
    info.currencies.insert(payCcy);
    info.payer = payer;

    // set base currency for cashflow

    auto cfCurrencies = getCashflowCurrencies(flow, payCcy);

    if (auto f = baseCurrencySuggestions.find(cfCurrencies); f != baseCurrencySuggestions.end()) {

        // first priority is the external suggestions

        info.baseCurrency = f->second;

    } else {

        /* second priorty is to search for a common currency in cf currencies and available model base currencies,
           and if there is no such currency, use the model base currency as third and last priority */

        std::vector<std::string> commonCurrencies;
        std::set_intersection(modelCg_->availableBaseCurrencies().begin(), modelCg_->availableBaseCurrencies().end(),
                              cfCurrencies.begin(), cfCurrencies.end(), std::back_inserter(commonCurrencies));
        if (!commonCurrencies.empty())
            info.baseCurrency = commonCurrencies.front();
        else
            info.baseCurrency = modelCg_->baseCurrency();

    }

    // set exercise criteria

    auto cpn = QuantLib::ext::dynamic_pointer_cast<Coupon>(flow);
    if (cpn && cpn->accrualStartDate() < flow->date()) {
        info.exIntoCriterionDate = cpn->accrualStartDate() + 1;
    } else {
        info.exIntoCriterionDate = info.payDate + (exerciseIntoIncludeSameDayFlows_ ? 1 : 0);
    }

    // Handle scaling

    Real multiplier = payer ? -1.0 : 1.0;
    if (auto scf = QuantLib::ext::dynamic_pointer_cast<ScaledCashFlow>(flow)) {
        multiplier *= scf->multiplier();
        flow = scf->underlyingCashFlow();
    } else if (auto scp = QuantLib::ext::dynamic_pointer_cast<ScaledCoupon>(flow)) {
        multiplier *= scp->multiplier();
        flow = scp->underlyingCoupon();
    }

    // handle SimpleCashflow

    if (QuantLib::ext::dynamic_pointer_cast<SimpleCashFlow>(flow) != nullptr) {
        info.flowNode = modelCg_->pay(cg_const(g, multiplier * flow->amount()), flow->date(), flow->date(), payCcy,
                                      info.baseCurrency);
        return info;
    }

    // handle fx linked fixed cashflow

    if (auto fxl = QuantLib::ext::dynamic_pointer_cast<FXLinkedCashFlow>(flow)) {
        Date fxLinkedFixingDate = fxl->fxFixingDate();
        std::string fxIndex = IndexNameTranslator::instance().oreName(fxl->fxIndex()->name());
        info.flowNode = modelCg_->pay(
            cg_mult(g, cg_const(g, fxl->foreignAmount()), modelCg_->eval(fxIndex, fxLinkedFixingDate, Null<Date>())),
            flow->date(), flow->date(), payCcy, info.baseCurrency);
        info.currencies.insert(fxl->fxIndex()->sourceCurrency().code());
        info.currencies.insert(fxl->fxIndex()->targetCurrency().code());
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
            info.currencies.insert(fxIndex->sourceCurrency().code());
            info.currencies.insert(fxIndex->targetCurrency().code());
        }
    } else if (auto fxl = QuantLib::ext::dynamic_pointer_cast<FloatingRateFXLinkedNotionalCoupon>(flow)) {
        isFxLinked = true;
        fxLinkedFixingDate = fxl->fxFixingDate();
        fxLinkedIndex = IndexNameTranslator::instance().oreName(fxl->fxIndex()->name());
        flow = fxl->underlying();
        fxLinkedForeignNominal = fxl->foreignAmount();
        info.currencies.insert(fxl->fxIndex()->sourceCurrency().code());
        info.currencies.insert(fxl->fxIndex()->targetCurrency().code());
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
        info.flowNode = modelCg_->pay(cg_const(g, multiplier * flow->amount()), flow->date(), flow->date(), payCcy,
                                      info.baseCurrency);
        return info;
    }

    if (auto ibor = QuantLib::ext::dynamic_pointer_cast<IborCoupon>(flow)) {

        info.currencies.insert(ibor->index()->currency().code());

        std::string indexName = IndexNameTranslator::instance().oreName(ibor->index()->name());
        std::size_t fixing = modelCg_->eval(indexName, ibor->fixingDate(), Null<Date>());

        std::size_t effectiveRate;
        if (isCapFloored) {
            std::size_t swapletRate = cg_const(g, 0.0);
            std::size_t floorletRate = cg_const(g, 0.0);
            std::size_t capletRate = cg_const(g, 0.0);
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
            effectiveRate = cg_subtract(g, cg_add(g, swapletRate, floorletRate), capletRate);
        } else {
            effectiveRate = cg_add(g, cg_mult(g, cg_const(g, ibor->gearing()), fixing), cg_const(g, ibor->spread()));
        }

        info.flowNode =
            modelCg_->pay(cg_mult(g,
                                  cg_const(g, multiplier * (isFxLinked ? fxLinkedForeignNominal : ibor->nominal()) *
                                                  ibor->accrualPeriod()),
                                  effectiveRate),
                          flow->date(), flow->date(), payCcy, info.baseCurrency);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    if (auto ibor = QuantLib::ext::dynamic_pointer_cast<InterpolatedIborCoupon>(flow)) {

        info.currencies.insert(ibor->index()->currency().code());

        std::string indexNameShort =
            IndexNameTranslator::instance().oreName(ibor->interpolatedIborIndex()->shortIndex()->name());
        std::string indexNameLong =
            IndexNameTranslator::instance().oreName(ibor->interpolatedIborIndex()->longIndex()->name());
        std::size_t fixingShort = modelCg_->eval(indexNameShort, ibor->fixingDate(), Null<Date>());
        std::size_t fixingLong = modelCg_->eval(indexNameLong, ibor->fixingDate(), Null<Date>());
        std::size_t shortWeight = cg_const(g, ibor->interpolatedIborIndex()->shortWeight(ibor->fixingDate()));
        std::size_t longWeight = cg_const(g, ibor->interpolatedIborIndex()->longWeight(ibor->fixingDate()));
        std::size_t fixing = cg_add(g, cg_mult(g, shortWeight, fixingShort), cg_mult(g, longWeight, fixingLong));

        std::size_t effectiveRate;
        if (isCapFloored) {
            std::size_t swapletRate = cg_const(g, 0.0);
            std::size_t floorletRate = cg_const(g, 0.0);
            std::size_t capletRate = cg_const(g, 0.0);
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
            effectiveRate = cg_subtract(g, cg_add(g, swapletRate, floorletRate), capletRate);
        } else {
            effectiveRate = cg_add(g, cg_mult(g, cg_const(g, ibor->gearing()), fixing), cg_const(g, ibor->spread()));
        }

        info.flowNode =
            modelCg_->pay(cg_mult(g,
                                  cg_const(g, multiplier * (isFxLinked ? fxLinkedForeignNominal : ibor->nominal()) *
                                                  ibor->accrualPeriod()),
                                  effectiveRate),
                          flow->date(), flow->date(), payCcy, info.baseCurrency);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    if (auto cms = QuantLib::ext::dynamic_pointer_cast<CmsCoupon>(flow)) {

        info.currencies.insert(cms->index()->currency().code());

        std::string indexName = IndexNameTranslator::instance().oreName(cms->index()->name());
        std::size_t fixing = modelCg_->eval(indexName, cms->fixingDate(), Null<Date>());

        std::size_t effectiveRate;
        if (isCapFloored) {
            std::size_t swapletRate = cg_const(g, 0.0);
            std::size_t floorletRate = cg_const(g, 0.0);
            std::size_t capletRate = cg_const(g, 0.0);
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
            effectiveRate = cg_subtract(g, cg_add(g, swapletRate, floorletRate), capletRate);
        } else {
            effectiveRate = cg_add(g, cg_mult(g, cg_const(g, cms->gearing()), fixing), cg_const(g, cms->spread()));
        }

        info.flowNode =
            modelCg_->pay(cg_mult(g,
                                  cg_const(g, multiplier * (isFxLinked ? fxLinkedForeignNominal : cms->nominal()) *
                                                  cms->accrualPeriod()),
                                  effectiveRate),
                          flow->date(), flow->date(), payCcy, info.baseCurrency);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    if (auto on = QuantLib::ext::dynamic_pointer_cast<QuantExt::OvernightIndexedCoupon>(flow)) {

        info.currencies.insert(on->index()->currency().code());

        std::string indexName = IndexNameTranslator::instance().oreName(on->index()->name());

        QL_REQUIRE(on->lookback().units() == QuantLib::Days,
                   "AmcCgBaseEngine::createCashflowInfo(): on coupon has lookback with units != Days ("
                       << on->lookback() << "), this is not allowed.");
        std::size_t fixing = modelCg_->fwdCompAvg(false, indexName, on->valueDates().front(), on->valueDates().front(),
                                                  on->valueDates().back(), on->spread(), on->gearing(),
                                                  on->lookback().length(), on->rateCutoff(), on->fixingDays(),
                                                  on->includeSpread(), Null<Real>(), Null<Real>(), false, false);
        info.flowNode =
            modelCg_->pay(cg_mult(g,
                                  cg_const(g, multiplier * (isFxLinked ? fxLinkedForeignNominal : on->nominal()) *
                                                  on->accrualPeriod()),
                                  fixing),
                          flow->date(), flow->date(), payCcy, info.baseCurrency);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    if (auto cfon = QuantLib::ext::dynamic_pointer_cast<CappedFlooredOvernightIndexedCoupon>(flow)) {

        auto on = cfon->underlying();

        info.currencies.insert(on->index()->currency().code());

        std::string indexName = IndexNameTranslator::instance().oreName(on->index()->name());

        QL_REQUIRE(on->lookback().units() == QuantLib::Days,
                   "AmcCgBaseEngine::createCashflowInfo(): cfon coupon has lookback with units != Days ("
                       << on->lookback() << "), this is not allowed.");
        std::size_t fixing = modelCg_->fwdCompAvg(
            false, indexName, on->valueDates().front(), on->valueDates().front(), on->valueDates().back(), on->spread(),
            on->gearing(), on->lookback().length(), on->rateCutoff(), on->fixingDays(), on->includeSpread(),
            cfon->cap(), cfon->floor(), cfon->nakedOption(), cfon->localCapFloor());
        info.flowNode =
            modelCg_->pay(cg_mult(g,
                                  cg_const(g, multiplier * (isFxLinked ? fxLinkedForeignNominal : on->nominal()) *
                                                  on->accrualPeriod()),
                                  fixing),
                          flow->date(), flow->date(), payCcy, info.baseCurrency);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    if (auto av = QuantLib::ext::dynamic_pointer_cast<QuantExt::AverageONIndexedCoupon>(flow)) {

        info.currencies.insert(av->index()->currency().code());

        std::string indexName = IndexNameTranslator::instance().oreName(av->index()->name());

        QL_REQUIRE(av->lookback().units() == QuantLib::Days,
                   "AmcCgBaseEngine::createCashflowInfo(): av coupon has lookback with units != Days ("
                       << av->lookback() << "), this is not allowed.");
        std::size_t fixing =
            modelCg_->fwdCompAvg(true, indexName, av->valueDates().front(), av->valueDates().front(),
                                 av->valueDates().back(), av->spread(), av->gearing(), av->lookback().length(),
                                 av->rateCutoff(), av->fixingDays(), false, Null<Real>(), Null<Real>(), false, false);
        info.flowNode =
            modelCg_->pay(cg_mult(g,
                                  cg_const(g, multiplier * (isFxLinked ? fxLinkedForeignNominal : av->nominal()) *
                                                  av->accrualPeriod()),
                                  fixing),
                          flow->date(), flow->date(), payCcy, info.baseCurrency);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    if (auto cfav = QuantLib::ext::dynamic_pointer_cast<CappedFlooredAverageONIndexedCoupon>(flow)) {

        auto av = cfav->underlying();

        info.currencies.insert(av->index()->currency().code());

        std::string indexName = IndexNameTranslator::instance().oreName(av->index()->name());

        QL_REQUIRE(av->lookback().units() == QuantLib::Days,
                   "AmcCgBaseEngine::createCashflowInfo(): cfon coupon has lookback with units != Days ("
                       << av->lookback() << "), this is not allowed.");
        std::size_t fixing = modelCg_->fwdCompAvg(
            false, indexName, av->valueDates().front(), av->valueDates().front(), av->valueDates().back(), av->spread(),
            av->gearing(), av->lookback().length(), av->rateCutoff(), av->fixingDays(), cfav->includeSpread(),
            cfav->cap(), cfav->floor(), cfav->nakedOption(), cfav->localCapFloor());
        info.flowNode =
            modelCg_->pay(cg_mult(g,
                                  cg_const(g, multiplier * (isFxLinked ? fxLinkedForeignNominal : av->nominal()) *
                                                  av->accrualPeriod()),
                                  fixing),
                          flow->date(), flow->date(), payCcy, info.baseCurrency);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    if (auto bma = QuantLib::ext::dynamic_pointer_cast<AverageBMACoupon>(flow)) {

        info.currencies.insert(bma->index()->currency().code());

        std::string indexName = IndexNameTranslator::instance().oreName(bma->index()->name());

        std::size_t fixing = modelCg_->eval(indexName, bma->fixingDates().front(), Null<Date>());
        std::size_t effectiveRate =
            cg_add(g, cg_mult(g, cg_const(g, bma->gearing()), fixing), cg_const(g, bma->spread()));
        info.flowNode =
            modelCg_->pay(cg_mult(g,
                                  cg_const(g, multiplier * (isFxLinked ? fxLinkedForeignNominal : bma->nominal()) *
                                                  bma->accrualPeriod()),
                                  effectiveRate),
                          flow->date(), flow->date(), payCcy, info.baseCurrency);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    if (auto cfbma = QuantLib::ext::dynamic_pointer_cast<CappedFlooredAverageBMACoupon>(flow)) {

        auto bma = cfbma->underlying();

        info.currencies.insert(bma->index()->currency().code());

        std::string indexName = IndexNameTranslator::instance().oreName(bma->index()->name());
        std::size_t fixing = modelCg_->eval(indexName, bma->fixingDates().front(), Null<Date>());

        effFloor = cfbma->effectiveFloor();
        effCap = cfbma->effectiveFloor();
        isNakedOption = cfbma->nakedOption();

        // approximation, TODO, add averagedBmaRate to modelCg_ as in mccashflowinfo.cpp

        std::size_t effectiveRate;
        if (effFloor != Null<Real>() || effCap != Null<Real>()) {
            std::size_t swapletRate = cg_const(g, 0.0);
            std::size_t floorletRate = cg_const(g, 0.0);
            std::size_t capletRate = cg_const(g, 0.0);
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
            effectiveRate = cg_subtract(g, cg_add(g, swapletRate, floorletRate), capletRate);
        } else {
            effectiveRate = cg_add(g, cg_mult(g, cg_const(g, bma->gearing()), fixing), cg_const(g, bma->spread()));
        }

        info.flowNode =
            modelCg_->pay(cg_mult(g,
                                  cg_const(g, multiplier * (isFxLinked ? fxLinkedForeignNominal : bma->nominal()) *
                                                  bma->accrualPeriod()),
                                  effectiveRate),
                          flow->date(), flow->date(), payCcy, info.baseCurrency);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    if (auto sub = QuantLib::ext::dynamic_pointer_cast<SubPeriodsCoupon1>(flow)) {

        info.currencies.insert(sub->index()->currency().code());

        std::string indexName = IndexNameTranslator::instance().oreName(sub->index()->name());

        std::size_t fixing = modelCg_->eval(indexName, sub->fixingDates().front(), Null<Date>());
        std::size_t effectiveRate =
            cg_add(g, cg_mult(g, cg_const(g, sub->gearing()), fixing), cg_const(g, sub->spread()));
        info.flowNode =
            modelCg_->pay(cg_mult(g,
                                  cg_const(g, multiplier * (isFxLinked ? fxLinkedForeignNominal : sub->nominal()) *
                                                  sub->accrualPeriod()),
                                  effectiveRate),
                          flow->date(), flow->date(), payCcy, info.baseCurrency);
        if (isFxLinked || isFxIndexed) {
            info.flowNode = cg_mult(g, info.flowNode, fxLinkedNode);
        }
        return info;
    }

    QL_FAIL("McMultiLegBaseEngine::createCashflowInfo(): unhandled coupon leg " << legNo << " cashflow " << cfNo);
}

bool AmcCgBaseEngine::isComplexTrade() const {
    if (exercise_ != nullptr) {
        for (auto const& d : exercise_->dates()) {
            if (d < modelCg_->referenceDate() || (!includeReferenceDateEvents_ && d == modelCg_->referenceDate()))
                continue;
            return true;
        }
    }
    return false;
}

std::set<std::set<std::string>> AmcCgBaseEngine::relevantCurrencySets() const {
    std::set<std::set<std::string>> ccySets;
    Size legNo = 0;
    for (auto const& leg : leg_) {
        for (auto const& cashflow : leg) {
            // we can skip cashflows that are paid
            if (cashflow->date() < modelCg_->referenceDate() ||
                (!includeTodaysCashflows_ && cashflow->date() == modelCg_->referenceDate()))
                continue;
            ccySets.insert(getCashflowCurrencies(cashflow, currency_[legNo]));
        }
        ++legNo;
    }
    return ccySets;
}

void AmcCgBaseEngine::buildComputationGraph(
    const bool stickyCloseOutDateRun, std::vector<TradeExposure>* tradeExposure,
    TradeExposureMetaInfo* tradeExposureMetaInfo,
    const std::map<std::set<std::string>, std::string>& baseCurrencySuggestions) const {

    QL_REQUIRE((tradeExposure == nullptr) == (tradeExposureMetaInfo == nullptr),
               "AmcCgBaseEngine::buildComputationGraph(): tradeExposure and tradeExposureMetaInfo must both be null or "
               "both != null");

    if (!amcEnabled_ && cgVersion_ == modelCg_->cgVersion())
        return;

    cgVersion_ = modelCg_->cgVersion();

    QuantExt::ComputationGraph& g = *modelCg_->computationGraph();

    includeReferenceDateEvents_ = Settings::instance().includeReferenceDateEvents();
    includeTodaysCashflows_ = Settings::instance().includeTodaysCashFlows()
                                  ? *Settings::instance().includeTodaysCashFlows()
                                  : includeReferenceDateEvents_;

    relevantCurrencySets_.clear();
    relevantCurrencies_.clear();
    currencySetBaseCurrency_.clear();

    // check data set by derived engines

    QL_REQUIRE(currency_.size() == leg_.size(), "McMultiLegBaseEngine: number of legs ("
                                                    << leg_.size() << ") does not match currencies ("
                                                    << currency_.size() << ")");
    QL_REQUIRE(payer_.size() == leg_.size(), "McMultiLegBaseEngine: number of legs ("
                                                 << leg_.size() << ") does not match payer flag (" << payer_.size()
                                                 << ")");

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
            cashflowInfo.push_back(createCashflowInfo(cashflow, currency_[legNo], payer_[legNo], legNo, cashflowNo,
                                                      baseCurrencySuggestions));
            // increment counter
            ++cashflowNo;
        }
        ++legNo;
    }

    // set relevant currencies and update cf currencies accordingly

    std::string complexBaseCurrency;

    if (exerciseDates.empty()) {

        // if we have no optionality, we add the base ccy to the cf currencies, and group by the resulting currency sets 

        for (auto const& cf : cashflowInfo) {
            cf.currencies.insert(cf.baseCurrency);
            relevantCurrencySets_.insert(cf.currencies);
            if(auto f = currencySetBaseCurrency_.find(cf.currencies); f != currencySetBaseCurrency_.end()) {
                QL_REQUIRE(f->second == cf.baseCurrency,
                           "AmcCgBaseEngine: internal error when mapping currency sets to base ccys, the same set ["
                               << boost::join(cf.currencies, ",") << "] is mapped to non-unique base currency ("
                               << f->second << "," << cf.baseCurrency << ")");
            } else {
                currencySetBaseCurrency_.insert({cf.currencies, cf.baseCurrency});
            }
        }

    } else {

        // if we have optionality, we only use one base currency and one currency set for all cashflows

        std::set<std::string> allCcys;
        for (auto const& cf : cashflowInfo) {
            allCcys.insert(cf.currencies.begin(), cf.currencies.end());
            allCcys.insert(cf.baseCurrency);
        }

        complexBaseCurrency = cashflowInfo.front().baseCurrency;
        if (any_of(cashflowInfo.begin(), cashfloInfo.end(),
                   [&complexBaseCurrency](const auto& cf) { return cf.baseCurrency != complexBaseCurrency; })) {
            std::vector<std::string> commonCurrencies;
            std::set_intersection(modelCg_->availableBaseCurrencies().begin(),
                                  modelCg_->availableBaseCurrencies().end(), allCcys.begin(), allCcys.end(),
                                  std::back_inserter(commonCurrencies));
            if (!commonCurrencies.empty())
                complexBaseCurrency = commonCurrencies.front();
            else
                complexBaseCurrency = modelCg_->baseCurrency();
            for (auto& cf : cashflowInfo)
                cf.baseCurrency = complexBaseCurrency;
        }

        for (auto& cf : cashflowInfo) {
            cf.currencies = allCcys;
        }

        relevantCurrencySets_.insert(allCcys);

        currencySetBaseCurrency_[allCcys] = complexBaseCurrency_;
    }


    // build the set of simulation dates and union of simulation and exercise dates

    std::set<Date> simDates(simulationDates_.begin(), simulationDates_.end());
    std::set<Date> simExDates;
    std::set_union(simDates.begin(), simDates.end(), exerciseDates.begin(), exerciseDates.end(),
                   std::inserter(simExDates, simExDates.end()));

    // create the path values

    std::vector<std::size_t> pathValueUndDirtyRunning(relevantCurrencySets_.size(), cg_const(g, 0.0)); // per ccy set
    std::size_t pathValueUndExIntoRunning = cg_const(g, 0.0);

    std::vector<std::vector<std::size_t>> pathValueUndDirty(
        simExDates.size(),
        std::vector<std::size_t>(relevantCurrencySets_.size(), cg_const(g, 0.0))); // per sim date, ccy set
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
        std::vector<std::vector<std::size_t>> pathValueUndDirtyContribution; // per ccy set a vector of nodes
        for (Size ccySet = 0; ccySet < relevantCurrencySets_.size(); ++ccySet) {
            pathValueUndDirtyContribution.push_back(std::vector<std::size_t>(1, pathValueUndDirtyRunning[ccySet]));
        }
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
            Size ccySet = std::distance(
                relevantCurrencySets_.begin(),
                std::find(relevantCurrencySets_.begin(), relevantCurrencySets_.end(), cashflowInfo[i].currencies));

            if (cfStatus[i] == CfStatus::open) {
                if (isPartOfExercise) {
                    pathValueUndDirtyContribution[ccySet].push_back(cashflowInfo[i].flowNode);
                    pathValueUndExIntoContribution.push_back(cashflowInfo[i].flowNode);
                    cfStatus[i] = CfStatus::done;
                } else if (isPartOfUnderlying) {
                    pathValueUndDirtyContribution[ccySet].push_back(cashflowInfo[i].flowNode);
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

        for (Size ccySet = 0; ccySet < relevantCurrencySets_.size(); ++ccySet)
            pathValueUndDirtyRunning[ccySet] = cg_add(g, pathValueUndDirtyContribution[ccySet]);
        pathValueUndExIntoRunning = cg_add(g, pathValueUndExIntoContribution);

        if (isExerciseDate) {

            // calculate rebate (exercise fees) if existent

            if (rebatedExercise) {
                Size exerciseTimes_idx = std::distance(exerciseDates.begin(), exerciseDates.find(*d));
                for (Size k = 0; k < rebatedExercise->rebateCurrencies().size(); ++k) {
                    if (rebatedExercise->rebate(exerciseTimes_idx, k) != 0.0) {
                        // if no rebate currency is given, we assume that it is paid in the first leg's currency!
                        pathValueRebate[counter] =
                            cg_add(g, pathValueRebate[counter],
                                   modelCg_->pay(cg_const(g, rebatedExercise->rebate(exerciseTimes_idx, k)), *d,
                                                 rebatedExercise->rebatePaymentDate(exerciseTimes_idx),
                                                 rebatedExercise->rebateCurrency(k).empty()
                                                     ? currency_.front()
                                                     : rebatedExercise->rebateCurrency(k).code()),
                                   complexBaseCurrency);
                    }
                }
            }

            if (stickyCloseOutDateRun && !reevaluateExerciseInStickyCloseOutDateRun_) {

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

    // add the remaining live cashflows to get the underlying value (no grouping by ccy set necessary here)

    std::vector<std::size_t> pathValueUndDirtyContribution(1, cg_add(g, pathValueUndDirtyRunning));
    for (Size i = 0; i < cashflowInfo.size(); ++i) {
        if (cfStatus[i] == CfStatus::open) {
            pathValueUndDirtyContribution.push_back(cashflowInfo[i].flowNode);
        }
    }

    // set the npv at t0

    npv_ = exercise_ == nullptr ? cg_add(g, pathValueUndDirtyContribution) : pathValueOption[0];

    // generate the exposure at simulation dates

    if (tradeExposure == nullptr)
        return;

    tradeExposureMetaInfo->hasVega = exercise_ != nullptr;

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

    tradeExposure->clear();
    tradeExposure->resize(1, SimpleTradeExposure());

    std::get<SimpleTradeExposure>((*tradeExposure)[0]).groups.push_back({});
    std::get<SimpleTradeExposure>((*tradeExposure)[0]).groups.back().pathValue = npv_;

    if (exerciseDates.empty()) {

        tradeExposure->resize(simDates.size() + 1, SimpleTradeExposure());

        // if we don't have an exercise, we return the dirty npv of the underlying at all times

        for (Size counter = 0; counter < simDates.size(); ++counter) {

            for (Size ccySet = 0; ccySet < relevantCurrencySets_.size(); ++ccySet) {
                std::get<SimpleTradeExposure>((*tradeExposure)[counter + 1]).groups.push_back({});
                std::get<SimpleTradeExposure>((*tradeExposure)[counter + 1]).groups.back().pathValue =
                    pathValueUndDirty[counter][ccySet];
                auto const& currencySet = *std::next(relevantCurrencySets_.begin(), ccySet);
                std::string baseCcurency = relevantCurrencyBaseCurrency_.at(currencySet);
                std::get<SimpleTradeExposure>((*tradeExposure)[counter + 1]).groups.back().regressors =
                    modelCg_->npvRegressors(*std::next(simDates.begin(), counter), currencySet, baseCurrency);
                std::get<SimpleTradeExposure>((*tradeExposure)[counter + 1]).groups.back().baseCurrency = baseCurrency;
                std::cout
                    << "simDate #" << counter << ": "
                    << std::get<SimpleTradeExposure>((*tradeExposure)[counter + 1]).groups.back().regressors.size()
                    << " regressors, baseCurrency = "
                    << std::get<SimpleTradeExposure>((*tradeExposure)[counter + 1]).groups.back().baseCurrency
                    << std::endl;
            }
        }

    } else {

        tradeExposure->resize(simDates.size() + 1, ComplexTradeExposure());

        // iterate through simulation + exercise dates in forward direction

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
                    exercisedValue = cg_add(g, cg_mult(g, isExercisedNow, pathValueUndExInto[counter]),
                                            cg_mult(g, cg_subtract(g, cg_const(g, 1.0), isExercisedNow),
                                                    cg_add(g, pathValueUndDirty[counter])));
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

                // set results with decomposition

                // create dummy nodes to facilitate a clean recombination run starting at the components

                std::size_t comp1 = g.insert({exercisedValue}, RandomVariableOpCode::None);
                std::size_t comp2 = g.insert({futureOptionValue}, RandomVariableOpCode::None);

                std::get<ComplexTradeExposure>((*tradeExposure)[simCounter + 1]).componentPathValues = {comp1, comp2};

                std::size_t exercisedValueCond = createRegressionModel(
                    comp1, d, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; },
                    cg_const(g, 1.0));
                std::size_t futureOptionValueCond = createRegressionModel(
                    comp2, d, cashflowInfo, [&cfStatus](std::size_t i) { return cfStatus[i] == CfStatus::done; },
                    cg_const(g, 1.0));

                // we can not take max(0, futureOptionValueCond) here, because the part between startNodeRecombine
                // to targetConditionalExpectationDerivatives is applied to derivatives, which we do not want to
                // floor at zero
                std::get<ComplexTradeExposure>((*tradeExposure)[simCounter + 1])
                    .targetConditionalExpectationDerivative =
                    cg_add(g, cg_mult(g, wasExercised, exercisedValueCond),
                           cg_mult(g, cg_subtract(g, cg_const(g, 1.0), wasExercised), futureOptionValueCond));

                std::get<ComplexTradeExposure>((*tradeExposure)[simCounter + 1])
                    .targetConditionalExpDerivativeNpvNodes = {exercisedValueCond, futureOptionValueCond};

                // here we can take max(0, futureOptionValueCond)
                std::get<ComplexTradeExposure>((*tradeExposure)[simCounter + 1]).targetConditionalExpectation =
                    cg_add(g, cg_mult(g, wasExercised, exercisedValueCond),
                           cg_mult(g, cg_subtract(g, cg_const(g, 1.0), wasExercised),
                                   cg_max(g, cg_const(g, 0.0), futureOptionValueCond)));

                // increase counters and continue

                ++simCounter;
            }

            ++counter;
        }
    }
}

std::size_t AmcCgBaseEngine::createRegressionModel(const std::size_t amount, const Date& d,
                                                   const std::vector<CashflowInfo>& cashflowInfo,
                                                   const std::function<bool(std::size_t)>& cashflowRelevant,
                                                   const std::size_t filter) const {
    // TODO use relevant cashflow info to refine regressor if regressor model == LaggedFX
    auto regressors = modelCg_->npvRegressors(d, relevantCurrencies_);
    return modelCg_->npv(amount, d, filter, std::nullopt, {}, regressors);
}

void AmcCgBaseEngine::calculate() const {

    if (amcEnabled_)
        return;

    QL_REQUIRE(!useExternalComputeFramework_,
               "AmcCgBaseEngine::calculate(): external compute framework not yet supported.");
    // QL_REQUIRE(!useExternalComputeFramework_ || !useCachedSensis_,
    //            "ScriptedInstrumentPricingEngineCG: when using external compute framework, usage of cached sensis is "
    //            "not supported yet");

    buildComputationGraph(false);

    if (!haveBaseValues_ || !useCachedSensis_) {

        // calculate NPV and Sensis ("base scenario"), store base npv + sensis + base model params

        auto g = modelCg_->computationGraph();

        // populate values

        std::vector<RandomVariable> values(g->size(), RandomVariable(modelCg_->size()));

        // set constants

        for (auto const& c : g->constants()) {
            values[c.second] = RandomVariable(modelCg_->size(), c.first);
        }

        // set model parameters

        baseModelParams_.clear();
        for (auto const& p : modelCg_->modelParameters()) {
            double v = p.eval();
            TLOG("setting model parameter " << p << "  at node " << p.node() << " to value " << std::setprecision(16)
                                            << v);
            baseModelParams_.push_back(std::make_pair(p.node(), v));
            values[p.node()] = RandomVariable(modelCg_->size(), v);
        }
        DLOG("set " << baseModelParams_.size() << " model parameters");

        // set random variates

        auto const& rv = modelCg_->randomVariates();
        if (!rv.empty()) {
            auto gen =
                makeMultiPathVariateGenerator(mcParams_.sequenceType, rv.size(), rv.front().size(), mcParams_.seed,
                                              mcParams_.sobolOrdering, mcParams_.sobolDirectionIntegers);
            for (Size path = 0; path < modelCg_->size(); ++path) {
                auto p = gen->next();
                for (Size j = 0; j < rv.front().size(); ++j) {
                    for (Size k = 0; k < rv.size(); ++k) {
                        values[rv[k][j]].set(path, p.value[j][k]);
                    }
                }
            }
        }
        DLOG("generated random variates for dim = " << rv.size() << ", steps = " << rv.front().size());

        // set flags for nodes we want to keep (model params, npv and additional results)

        std::vector<bool> keepNodes(g->size(), false);

        keepNodes[npv_] = true;

        for (auto const& [n, _] : baseModelParams_)
            keepNodes[n] = true;

        // run the forward evaluation

        forwardEvaluation(*g, values, ops_, RandomVariable::deleter, useCachedSensis_, opNodeRequirements_, keepNodes);
        DLOG("ran forward evaluation");

        TLOGGERSTREAM(ssaForm(*g, getRandomVariableOpLabels(), values));

        // extract npv result and set it

        npvValue_ = modelCg_->extractT0Result(values[npv_]);
        DLOG("got NPV = " << npvValue_ << " " << modelCg_->baseCcy());

        if (useCachedSensis_) {

            baseNpv_ = npvValue_;

            // extract sensis and store them

            std::vector<RandomVariable> derivatives(g->size(), RandomVariable(modelCg_->size(), 0.0));
            derivatives[npv_] = RandomVariable(modelCg_->size(), 1.0);
            backwardDerivatives(*g, values, derivatives, grads_, RandomVariable::deleter, keepNodes, ops_,
                                opNodeRequirements_, keepNodes, RandomVariableOpCode::ConditionalExpectation,
                                ops_[RandomVariableOpCode::ConditionalExpectation]);

            sensis_.resize(baseModelParams_.size());
            for (Size i = 0; i < baseModelParams_.size(); ++i) {
                sensis_[i] = modelCg_->extractT0Result(derivatives[baseModelParams_[i].first]);
                TLOG("sensi at node " << baseModelParams_[i].first << ": " << sensis_[i]);
            }
            DLOG("got backward sensitivities");

            // set flag indicating that we can use cached sensis in subsequent calculations

            haveBaseValues_ = true;
        }

    } else {

        // useCachedSensis => calculate npv from stored base npv, sensis, model params

        std::vector<std::pair<std::size_t, double>> modelParams;
        for (auto const& p : modelCg_->modelParameters()) {
            modelParams.push_back(std::make_pair(p.node(), p.eval()));
        }

        double npv = baseNpv_;
        DLOG("computing npv using baseNpv " << baseNpv_ << " and sensis.");

        for (Size i = 0; i < baseModelParams_.size(); ++i) {
            QL_REQUIRE(modelParams[i].first == baseModelParams_[i].first,
                       "internal error: modelParams[" << i << "] node " << modelParams[i].first
                                                      << ") does not match baseModelParams node "
                                                      << baseModelParams_[i].first);
            Real tmp = sensis_[i] * (modelParams[i].second - baseModelParams_[i].second);
            npv += tmp;
        }

        npvValue_ = npv;
    }
}

} // namespace data
} // namespace ore
