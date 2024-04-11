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

/*! \file engine/valuationcalculator.hpp
    \brief The cube valuation calculator interface
    \ingroup simulation
*/

#include <orea/engine/valuationcalculator.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/utilities/log.hpp>

namespace ore {
namespace analytics {

void NPVCalculator::init(const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const QuantLib::ext::shared_ptr<SimMarket>& simMarket) {
    DLOG("init NPVCalculator");
    tradeCcyIndex_.resize(portfolio->size());
    std::set<std::string> ccys;
    for (auto const& [tradeId, trade] : portfolio->trades())
        ccys.insert(trade->npvCurrency());
    
    size_t i = 0;
    for (auto const& [tradeId, trade] : portfolio->trades()) {
        tradeCcyIndex_[i] = std::distance(ccys.begin(), ccys.find(trade->npvCurrency()));
        i++;
    }
    
    ccyQuotes_.resize(ccys.size());
    for (Size i = 0; i < ccys.size(); ++i)
        ccyQuotes_[i] = (simMarket->fxRate(*std::next(ccys.begin(), i) + baseCcyCode_));
    fxRates_.resize(ccys.size());
}

void NPVCalculator::initScenario() {
    for (Size i = 0; i < ccyQuotes_.size(); ++i)
        fxRates_[i] = ccyQuotes_[i]->value();
}

void NPVCalculator::calculate(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                              const QuantLib::ext::shared_ptr<SimMarket>& simMarket, QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                              QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet, const Date& date, Size dateIndex,
                              Size sample, bool isCloseOut) {
    if (!isCloseOut)
        outputCube->set(npv(tradeIndex, trade, simMarket), tradeIndex, dateIndex, sample, index_);
}

void NPVCalculator::calculateT0(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                                const QuantLib::ext::shared_ptr<SimMarket>& simMarket, QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                                QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet) {
    outputCube->setT0(npv(tradeIndex, trade, simMarket), tradeIndex, index_);
}

Real NPVCalculator::npv(Size tradeIndex, const QuantLib::ext::shared_ptr<Trade>& trade,
                        const QuantLib::ext::shared_ptr<SimMarket>& simMarket) {
    Real npv = trade->instrument()->NPV();
    if (close_enough(npv, 0.0))
        return npv;
    Real fx = fxRates_[tradeCcyIndex_[tradeIndex]];
    Real numeraire = simMarket->numeraire();
    return npv * fx / numeraire;
}

void CashflowCalculator::init(const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                              const QuantLib::ext::shared_ptr<SimMarket>& simMarket) {
    DLOG("init CashflowCalculator");
    tradeAndLegCcyIndex_.clear();
    std::set<std::string> ccys;
    for (auto const& [tradeId,trade] : portfolio->trades()) {
        tradeAndLegCcyIndex_.push_back(std::vector<Size>(trade->legs().size()));
        for (auto const& l : trade->legCurrencies()) {
            ccys.insert(l);
        }
    }
    size_t i = 0;
    for (const auto& [tradeId, trade] : portfolio->trades()) {
        for (Size j = 0; j < trade->legs().size(); ++j) {
            tradeAndLegCcyIndex_[i][j] =
                std::distance(ccys.begin(), ccys.find(trade->legCurrencies()[j]));
        }
        i++;
    }
    ccyQuotes_.resize(ccys.size());
    for (Size i = 0; i < ccys.size(); ++i)
        ccyQuotes_[i] = (simMarket->fxRate(*std::next(ccys.begin(), i) + baseCcyCode_));
    fxRates_.resize(ccys.size());
}

void CashflowCalculator::initScenario() {
    for (Size i = 0; i < ccyQuotes_.size(); ++i)
        fxRates_[i] = ccyQuotes_[i]->value();
}

void CashflowCalculator::calculate(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                                   const QuantLib::ext::shared_ptr<SimMarket>& simMarket,
                                   QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                                   QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet, const Date& date, Size dateIndex,
                                   Size sample, bool isCloseOut) {
    if (isCloseOut)
        return;

    Real netPositiveFlow = 0;
    Real netNegativeFlow = 0;

    QL_REQUIRE(dateGrid_->valuationDates()[dateIndex] == date, "Date mixup, date is " << date << " but grid index is "
                                                                             << dateIndex << ", grid(dateIndex) is "
                                                                             << dateGrid_->dates()[dateIndex]);
    // Date startDate = dateIndex == 0 ? t0Date_ : dateGrid_->dates()[dateIndex - 1];
    Date startDate = date;
    Date endDate = date == dateGrid_->dates().back() ? date : dateGrid_->dates()[dateIndex + 1];

    bool isOption = trade->instrument()->isOption();
    bool isExercised = false;
    bool isPhysical = false;
    Real longShort = 1.0;
    if (isOption) {
        QuantLib::ext::shared_ptr<data::OptionWrapper> wrapper =
            QuantLib::ext::dynamic_pointer_cast<data::OptionWrapper>(trade->instrument());
        isExercised = wrapper->isExercised();
        longShort = wrapper->isLong() ? 1.0 : -1.0;
        isPhysical = wrapper->isPhysicalDelivery();
    }

    try {
        if (!isOption || (isExercised && isPhysical)) {
            for (Size i = 0; i < trade->legs().size(); i++) {
                const Leg& leg = trade->legs()[i];
                Real legFlow = 0;
                for (auto flow : leg) {
                    // Take flows in (t, t+1]
                    if (startDate < flow->date() && flow->date() <= endDate)
                        legFlow += flow->amount();
                }
                if (legFlow != 0) {
                    // Do FX conversion and add to netFlow
                    Real fx = fxRates_[tradeAndLegCcyIndex_[tradeIndex][i]];
                    Real direction = trade->legPayers()[i] ? -1.0 : 1.0;
                    legFlow *= direction * longShort * fx;
                    if (legFlow > 0)
                        netPositiveFlow += legFlow;
                    else
                        netNegativeFlow += legFlow;
                }
            }
        }
    } catch (std::exception& e) {
        ALOG("Failed to calculate cashflows for trade " << trade->id() << " : " << e.what());
        netPositiveFlow = 0;
        netNegativeFlow = 0;
    } catch (...) {
        ALOG("Failed to calculate cashflows for trade " << trade->id() << " : Unhandled Exception");
        netPositiveFlow = 0;
        netNegativeFlow = 0;
    }

    Real numeraire = simMarket->numeraire();

    outputCube->set(netPositiveFlow / numeraire, tradeIndex, dateIndex, sample, index_);
    outputCube->set(netNegativeFlow / numeraire, tradeIndex, dateIndex, sample, index_+1);
}

void NPVCalculatorFXT0::init(const QuantLib::ext::shared_ptr<Portfolio>& portfolio,
                             const QuantLib::ext::shared_ptr<SimMarket>& simMarket) {
    DLOG("init NPVCalculatorFXT0");
    tradeCcyIndex_.resize(portfolio->size());
    std::set<std::string> ccys;
    for (auto const& [tradeId, trade]: portfolio->trades())
        ccys.insert(trade->npvCurrency());
    size_t i = 0;
    for (auto const& [tradeId, trade] : portfolio->trades()){
        tradeCcyIndex_[i] = std::distance(ccys.begin(), ccys.find(trade->npvCurrency()));
        i++;
    }
    fxRates_.resize(ccys.size());
    for (Size i = 0; i < ccys.size(); ++i)
        fxRates_[i] = t0Market_->fxRate(*std::next(ccys.begin(), i) + baseCcyCode_)->value();
}

void NPVCalculatorFXT0::calculate(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                                  const QuantLib::ext::shared_ptr<SimMarket>& simMarket, QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                                  QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet, const Date& date, Size dateIndex,
                                  Size sample, bool isCloseOut) {
    if (!isCloseOut)
        outputCube->set(npv(tradeIndex, trade, simMarket), tradeIndex, dateIndex, sample, index_);
}

void NPVCalculatorFXT0::calculateT0(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                                    const QuantLib::ext::shared_ptr<SimMarket>& simMarket,
                                    QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                                    QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet) {
    outputCube->setT0(npv(tradeIndex, trade, simMarket), tradeIndex, index_);
}

Real NPVCalculatorFXT0::npv(Size tradeIndex, const QuantLib::ext::shared_ptr<Trade>& trade,
                            const QuantLib::ext::shared_ptr<SimMarket>& simMarket) {
    Real npv = trade->instrument()->NPV();
    if (close_enough(npv, 0.0))
        return npv;
    Real fx = fxRates_[tradeCcyIndex_[tradeIndex]];
    Real numeraire = simMarket->numeraire();
    return npv * fx / numeraire;
}

} // namespace analytics
} // namespace ore
