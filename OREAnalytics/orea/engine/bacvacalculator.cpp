/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

#include <orea/app/reportwriter.hpp>
#include <ored/report/inmemoryreport.hpp>
#include <ored/utilities/parsers.hpp>
#include <orea/engine/bacvacalculator.hpp>

using namespace ore::data;

namespace ore {
namespace analytics {

BaCvaCalculator::BaCvaCalculator(const QuantLib::ext::shared_ptr<SaccrCalculator>& saccrCalculator,
                                 const QuantLib::ext::shared_ptr<SaccrTradeData>& saccrTradeData,
                                 const std::string& calculationCcy, QuantLib::Real rho, QuantLib::Real alpha)
    : saccrCalculator_(saccrCalculator), saccrTradeData_(saccrTradeData), rho_(rho), alpha_(alpha) {

    calculationCcy_ = calculationCcy;

    calculate();
}

void BaCvaCalculator::calculate() {

    timer_.start("calculate()");

    // calculate the effective maturity of the portfolio
    calculateEffectiveMaturity();

    auto allCounterpartyNettingSets = saccrTradeData_->portfolio()->counterpartyNettingSets();
    // loop each counterparty
    for (auto it = allCounterpartyNettingSets.begin(); it != allCounterpartyNettingSets.end(); it++) {

        Real sCva = 0.0;

        // look up risk weight
        QL_REQUIRE(saccrTradeData_->counterpartyManager()->has(it->first),
                   "counterparty ID " << it->first << " missing in counterparty manager for BA-CVA calculation");
        QuantLib::ext::shared_ptr<CounterpartyInformation> cp = saccrTradeData_->counterpartyManager()->get(it->first);

        // skip clearing counterparties
        if (cp->isClearingCP())
            continue;

        counterpartyNettingSets_.insert(*it);

        Real riskWeight = cp->baCvaRiskWeight();
        QL_REQUIRE(riskWeight != QuantLib::Null<Real>(), "missing risk weight for BA-CVA calculation");
        riskWeights_[it->first] = riskWeight;

        // loop over each netting set for the counterparty
        for (auto n : it->second) {
            // EAD is the SA-CCR number - assume non IMM bank
            Real ead = EAD(n);

            // Get the effective maturity for this netting set
            Real effMaturity = effectiveMaturity(n);

            // Calculte discount factor - assume non IMM bank
            Real discountFactor = (1 - std::exp(-0.05 * effMaturity)) / (0.05 * effMaturity);
            discountFactor_[n] = discountFactor;

            // add to sCVA number;
            sCva += ead * effMaturity * discountFactor;
        }
        sCva = sCva * riskWeight / alpha_;
        counterpartySCVA_[it->first] = sCva;
    }

    Real sCvaSum = 0.0;
    //Real sCvaSumSquare = 0.0;
    for (auto cp : counterpartySCVA_) {
        sCvaSum += cp.second;
        //sCvaSumSquare += cp.second * cp.second;
    }

    // full formula
    // cvaResult_ = discount_ * std::sqrt(rho_ * rho_ * sCvaSum * sCvaSum + (1 - rho_ * rho_) * sCvaSumSquare);

    // reduced forumula for gfma
    cvaResult_ = discount_ * rho_ * sCvaSum;

    timer_.stop("calculate()");
}

Real BaCvaCalculator::effectiveMaturity(string nettingSet) {
    if (effectiveMaturityMap_.size() == 0) {
        DLOG("No effective maturities calculated, recalculating");
        calculateEffectiveMaturity();
    }

    QL_REQUIRE(effectiveMaturityMap_.find(nettingSet) != effectiveMaturityMap_.end(),
               "Cannot find effective maturity for netting set " << nettingSet);
    return effectiveMaturityMap_[nettingSet];
}

Real BaCvaCalculator::discountFactor(string nettingSet) {
    if (discountFactor_.size() == 0) {
        DLOG("No discount factor calculated, recalculating");
        calculate();
    }

    QL_REQUIRE(discountFactor_.find(nettingSet) != discountFactor_.end(),
               "Cannot find discount factor for netting set " << nettingSet);
    return discountFactor_[nettingSet];
}

Real BaCvaCalculator::counterpartySCVA(string counterparty) {
    if (counterpartySCVA_.size() == 0) {
        DLOG("No counterparty sCVA calculated, recalculating");
        calculate();
    }

    QL_REQUIRE(counterpartySCVA_.find(counterparty) != counterpartySCVA_.end(),
               "Cannot find sCVA for counterparty " << counterparty);
    return counterpartySCVA_[counterparty];
}

Real BaCvaCalculator::riskWeight(string counterparty) {
    if (riskWeights_.size() == 0) {
        DLOG("No counterparty risk weight calculated, recalculating");
        calculate();
    }

    QL_REQUIRE(riskWeights_.find(counterparty) != riskWeights_.end(),
               "Cannot find risk weight for counterparty " << counterparty);
    return riskWeights_[counterparty];
}

void BaCvaCalculator::calculateEffectiveMaturity() {
    LOG("Calculating Effective Maturity for BA_CVA");

    Date today = Settings::instance().evaluationDate();
    DayCounter dayCounter = ActualActual(ActualActual::ISDA); // use generic daycounter of ActualActual

    map<string, Real> matNumerator, matDenominator;
    QuantLib::ext::shared_ptr<Portfolio> portfolio = saccrTradeData_->portfolio();

    // get the netting set
    map<string, string> nettingSetMap = portfolio->nettingSetMap();

    // loop over all trades and accumulate the effective maturity numerator and denominator
    // TODO: this is valid for FxForwards, XCCY swaps, and FX Options and should be reviewed if additional
    // trade types included in the BA-CVA calculation
    vector<string> supportedTradeTypes({"FxForward", "FxOption", "Swap"});
    for (auto& [tid,t] : portfolio->trades()) {
        string tt = t->tradeType();
        // for trades with cashflows, we use add up the cashflows, weighted by cf paydate
        bool noCashflows = true;
        if (tt == "Swap") {
            try {
                TLOG("Trade " << tid << ": adding positive cashflows to effective maturity.");
                const vector<Leg>& legs = t->legs();
                if (legs.size() > 0) {
                    const Real multiplier = t->instrument()->multiplier();
                    for (size_t i = 0; i < legs.size(); i++) {
                        const QuantLib::Leg& leg = legs[i];
                        bool payer = t->legPayers()[i];

                        string ccy = t->legCurrencies()[i];
                        for (size_t j = 0; j < leg.size(); j++) {
                            QuantLib::ext::shared_ptr<QuantLib::CashFlow> ptrFlow = leg[j];
                            Date payDate = ptrFlow->date();
                            // only take future cashflows
                            if (!ptrFlow->hasOccurred(today)) {
                                Real amount = ptrFlow->amount();
                                // update amount if there is a multiplier
                                amount = amount * (amount == Null<Real>() ? 1.0 : multiplier);

                                // look up the netting set
                                QL_REQUIRE(nettingSetMap.find(tid) != nettingSetMap.end(),
                                           "Failed to find netting set for trade " << tid);
                                string nettingSet = nettingSetMap[tid];

                                // calculate time to cashflow
                                Real time = dayCounter.yearFraction(today, payDate);

                                // look up FX rate in market it different from calculation ccy
                                Real fxRate = 1.0;
                                if (calculationCcy_ != ccy) {
                                    fxRate = saccrTradeData_->market()->fxRate(ccy + calculationCcy_)->value();
                                }

                                // Check amount
                                QL_REQUIRE(amount != Null<Real>(),
                                           "Cashflow amount is null : trade " << tid << ", leg " << i);

                                // add amount the numerator and denominator if cashflow is received
                                Real recAmount = payer ? -amount : amount;
                                if (recAmount > 0) {
                                    // only set to false if we are recording the cashflows
                                    noCashflows = false;
                                    auto itr = matNumerator.find(nettingSet);
                                    if (itr == matNumerator.end()) {
                                        matNumerator[nettingSet] = fxRate * recAmount * time;
                                    } else {
                                        matNumerator[nettingSet] += fxRate * recAmount * time;
                                    }

                                    auto itd = matDenominator.find(nettingSet);
                                    if (itd == matDenominator.end()) {
                                        matDenominator[nettingSet] = fxRate * recAmount;
                                    } else {
                                        matDenominator[nettingSet] += fxRate * recAmount;
                                    }
                                }
                            }
                        }
                    }
                } else {
                    DLOG("No cashflows from trade " << tid);
                    noCashflows = true;
                }
            } catch (std::exception& e) {
                ALOG("Exception calculating effective maturity for trade " << tid << " : " << e.what());
            } catch (...) {
                ALOG("Exception calculating effective maturity for trade " << tid << " : Unkown Exception");
            }
        }
        if (tt == "FxOption" || tt == "FxForward" || noCashflows) {
            // for trades without cashflows, we use notional * time to maturity weighting the effective maturity
            DLOG(
                "Trade " << tid
                         << " does not contain cashflows, using Notional and expiry in effective maturity calculation");
            try {
                Real notional = t->notional();
                string currency = t->notionalCurrency();
                Date maturity = t->maturity();

                // look up the netting set
                QL_REQUIRE(nettingSetMap.find(tid) != nettingSetMap.end(),
                           "Failed to find netting set for trade " << tid);
                string nettingSet = nettingSetMap[tid];

                Real time = dayCounter.yearFraction(today, maturity);

                Real fxRate = 1.0;
                if (calculationCcy_ != currency) {
                    fxRate = saccrTradeData_->market()->fxRate(currency + calculationCcy_)->value();
                }

                auto itr = matNumerator.find(nettingSet);
                if (itr == matNumerator.end()) {
                    matNumerator[nettingSet] = fxRate * std::abs(notional) * time;
                } else {
                    matNumerator[nettingSet] += fxRate * std::abs(notional) * time;
                }

                auto itd = matDenominator.find(nettingSet);
                if (itd == matDenominator.end()) {
                    matDenominator[nettingSet] = fxRate * std::abs(notional);
                } else {
                    matDenominator[nettingSet] += fxRate * std::abs(notional);
                }
            } catch (std::exception& e) {
                ALOG("Exception calculating effective maturity for trade " << tid << " : " << e.what());
            } catch (...) {
                ALOG("Exception calculating effective maturity for trade " << tid << " : Unkown Exception");
            }
        }
        if (std::find(supportedTradeTypes.begin(), supportedTradeTypes.end(), tt) == supportedTradeTypes.end()) {
            ALOG("Trade type " << tt << " for trade " << tid << " not yet supported for BA-CVA.")
        }
    }

    // loop over all netting sets and calculate the effectice maturity
    for (auto n : matNumerator) {
        QL_REQUIRE(matDenominator.find(n.first) != matDenominator.end(),
                   "Could not find denominator for nettingSet " << n.first);
        if (std::abs(n.second) > 0.0 && std::abs(matDenominator[n.first]) > 0.0) {
            effectiveMaturityMap_[n.first] = n.second / matDenominator[n.first];
        } else {
            effectiveMaturityMap_[n.first] = 0.0;
        }
    }

    LOG("Effective Maturity calculation complete.");
}

} // namespace analytics
} // namespace ore
