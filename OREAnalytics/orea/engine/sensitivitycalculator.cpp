/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

#include <orea/engine/sensitivitycalculator.hpp>
#include <ored/portfolio/fxoption.hpp>
#include <ored/portfolio/optionwrapper.hpp>
#include <ored/utilities/log.hpp>

//#include <qle/pricingengines/discountingcurrencyswapenginedeltagamma.hpp>
#include <qle/instruments/currencyswap.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
using namespace data;
namespace analytics {

// result types, taken from qlep/pricingengines/discountingcurrencyswapenginedeltagamma.hpp
struct CurrencyComparator {
    bool operator()(const Currency& c1, const Currency& c2) const { return c1.code() < c2.code(); }
};
typedef std::map<Currency, Matrix, CurrencyComparator> result_type_matrix;
typedef std::map<Currency, std::vector<Real>, CurrencyComparator> result_type_vector;
typedef std::map<Currency, Real, CurrencyComparator> result_type_scalar;

void SensitivityCalculator::calculate(const boost::shared_ptr<Trade>& trade, Size tradeIndex,
                                      const boost::shared_ptr<SimMarket>& simMarket,
                                      boost::shared_ptr<NPVCube>& outputCube, const Date& date, Size dateIndex,
                                      Size sample, bool isCloseOut) {
    std::vector<Real> result = sensis(trade, simMarket);
    for (Size i = 0; i < result.size(); ++i)
        outputCube->set(result[i], tradeIndex, dateIndex, sample, index_ + i);
}

void SensitivityCalculator::calculateT0(const boost::shared_ptr<Trade>& trade, Size tradeIndex,
                                        const boost::shared_ptr<SimMarket>& simMarket,
                                        boost::shared_ptr<NPVCube>& outputCube) {
    std::vector<Real> result = sensis(trade, simMarket);
    for (Size i = 0; i < result.size(); ++i)
        outputCube->setT0(result[i], tradeIndex, index_ + i);
}

Disposable<std::vector<Real>> SensitivityCalculator::sensis(const boost::shared_ptr<Trade>& trade,
                                                            const boost::shared_ptr<SimMarket>& simMarket) {
    // TODO this should obviously be refactored by having pluggable
    // classes that handle the marshalling from additional results
    // to the cube order
    // TODO this relies on the existence of certain additional results,
    // i.e. effectively on specific engines (or an implicit interface
    // we define by requiring them), this should be improved
    std::vector<Real> result(0);
    try {
        if (trade->tradeType() == "Swap") {
            if (trade->instrument()->qlInstrument()->isExpired())
                return result;
            std::vector<string> currencies = trade->legCurrencies();
            bool isXCCY = false;
            for (Size i = 1; i < currencies.size(); ++i)
                isXCCY = isXCCY || (currencies[i] != currencies[0]);
            if (!isXCCY) {
                // Single Currency Swap
                Real fx = simMarket->fxSpot(currencies[0] + baseCcyCode_)->value();
                result.push_back(fx);
                result.push_back(currencies[0] != baseCcyCode_
                                     ? trade->instrument()->multiplier() * trade->instrument()->qlInstrument()->NPV()
                                     : 0.0);
                std::vector<Real> deltaDiscount =
                    trade->instrument()->qlInstrument()->result<std::vector<Real>>("deltaDiscount");
                std::vector<Real> deltaForward =
                    trade->instrument()->qlInstrument()->result<std::vector<Real>>("deltaForward");
                Size n = deltaDiscount.size();
                for (Size i = 0; i < n; ++i)
                    result.push_back(trade->instrument()->multiplier() * deltaDiscount[i]);
                for (Size i = 0; i < n; ++i)
                    result.push_back(trade->instrument()->multiplier() * deltaForward[i]);
                if (compute2ndOrder_) {
                    Matrix gamma = trade->instrument()->qlInstrument()->result<Matrix>("gamma");
                    Size n = gamma.rows();
                    for (Size i = 0; i < n; ++i) {
                        for (Size j = 0; j <= i; ++j) {
                            result.push_back(trade->instrument()->multiplier() * gamma[i][j]);
                        }
                    }
                }
            } else {
                // Cross Currency Swap
                std::vector<string> distinctCurrs(currencies);
                std::sort(distinctCurrs.begin(), distinctCurrs.end());
                auto end = std::unique(distinctCurrs.begin(), distinctCurrs.end());
                distinctCurrs.resize(end - distinctCurrs.begin());
                QL_REQUIRE(distinctCurrs.size() == 2,
                           "SensitivityCalculator can only handle 2 currencies for Cross Currency Swaps, has "
                               << distinctCurrs.size());
                Currency ccy1 = parseCurrency(distinctCurrs[0]);
                Currency ccy2 = parseCurrency(distinctCurrs[1]);
                QL_REQUIRE(ccy1 != ccy2,
                           "SensitivitiyCalculator: Found Cross Currency Swap with only one currency (" << ccy1 << ")");
                Real fx1 = simMarket->fxSpot(distinctCurrs[0] + baseCcyCode_)->value();
                Real fx2 = simMarket->fxSpot(distinctCurrs[1] + baseCcyCode_)->value();
                result.push_back(fx1);
                result.push_back(fx2);
                boost::shared_ptr<CurrencySwap> instr =
                    boost::dynamic_pointer_cast<CurrencySwap>(trade->instrument()->qlInstrument());
                QL_REQUIRE(instr, "SensitivityCalculator: Cross Currency Swap: Expected QL instrument CurrencySwap");
                std::map<string, Real> legNpvs;
                for (Size l = 0; l < currencies.size(); ++l) {
                    legNpvs[currencies[l]] += instr->inCcyLegNPV(l);
                }
                result.push_back(distinctCurrs[0] != baseCcyCode_
                                     ? trade->instrument()->multiplier() * legNpvs[distinctCurrs[0]]
                                     : 0.0);
                result.push_back(distinctCurrs[1] != baseCcyCode_
                                     ? trade->instrument()->multiplier() * legNpvs[distinctCurrs[1]]
                                     : 0.0);
                std::vector<Real> deltaDiscount1 =
                    trade->instrument()->qlInstrument()->result<result_type_vector>("deltaDiscount")[ccy1];
                std::vector<Real> deltaForward1 = instr->result<result_type_vector>("deltaForward")[ccy1];
                std::vector<Real> deltaDiscount2 = instr->result<result_type_vector>("deltaDiscount")[ccy2];
                std::vector<Real> deltaForward2 = instr->result<result_type_vector>("deltaForward")[ccy2];
                Size n = deltaDiscount1.size();
                for (Size i = 0; i < n; ++i)
                    result.push_back(trade->instrument()->multiplier() * deltaDiscount1[i]);
                for (Size i = 0; i < n; ++i)
                    result.push_back(trade->instrument()->multiplier() * deltaForward1[i]);
                for (Size i = 0; i < n; ++i)
                    result.push_back(trade->instrument()->multiplier() * deltaDiscount2[i]);
                for (Size i = 0; i < n; ++i)
                    result.push_back(trade->instrument()->multiplier() * deltaForward2[i]);
                if (compute2ndOrder_) {
                    Matrix gamma1 = instr->result<result_type_matrix>("gamma")[ccy1];
                    Matrix gamma2 = instr->result<result_type_matrix>("gamma")[ccy2];
                    Size n = gamma1.rows();
                    for (Size i = 0; i < n; ++i) {
                        for (Size j = 0; j <= i; ++j) {
                            result.push_back(trade->instrument()->multiplier() * gamma2[i][j]);
                        }
                    }
                    for (Size i = 0; i < n; ++i) {
                        for (Size j = 0; j <= i; ++j) {
                            result.push_back(trade->instrument()->multiplier() * gamma2[i][j]);
                        }
                    }
                }
            } // XCCY Swap
        } else if (trade->tradeType() == "Swaption") {
            // European Swaption (we don't check this, it will throw anyway below)
            std::vector<string> currencies = trade->legCurrencies();
            Real fx = simMarket->fxSpot(currencies[0] + baseCcyCode_)->value();
            boost::shared_ptr<Instrument> qlInstr;
            Real multiplier = 1.0;
            bool hasVega = true;
            // option handling
            boost::shared_ptr<OptionWrapper> wrapper = boost::dynamic_pointer_cast<OptionWrapper>(trade->instrument());
            if (wrapper) { // option wrapper
                if (wrapper->isExercised()) {
                    qlInstr = wrapper->activeUnderlyingInstrument();
                    multiplier = wrapper->underlyingMultiplier() * (wrapper->isLong() ? 1.0 : -1.0);
                    hasVega = false;
                } else {
                    qlInstr = wrapper->qlInstrument();
                    multiplier = wrapper->multiplier() * (wrapper->isLong() ? 1.0 : -1.0);
                }
            } else { // not an option wrapper
                qlInstr = trade->instrument()->qlInstrument();
                multiplier = trade->instrument()->multiplier();
                hasVega = true;
            }
            // handle expired instruments
            if (qlInstr->isExpired())
                return result;
            // push results
            result.push_back(fx);
            result.push_back(currencies[0] != baseCcyCode_ ? multiplier * qlInstr->NPV() : 0.0);
            std::vector<Real> deltaDiscount = qlInstr->result<std::vector<Real>>("deltaDiscount");
            std::vector<Real> deltaForward = qlInstr->result<std::vector<Real>>("deltaForward");
            Size n = deltaDiscount.size();
            for (Size i = 0; i < n; ++i)
                result.push_back(multiplier * deltaDiscount[i]);
            for (Size i = 0; i < n; ++i)
                result.push_back(multiplier * deltaForward[i]);
            if (compute2ndOrder_) {
                Matrix gamma = qlInstr->result<Matrix>("gamma");
                Size n = gamma.rows();
                for (Size i = 0; i < n; ++i) {
                    for (Size j = 0; j <= i; ++j) {
                        result.push_back(multiplier * gamma[i][j]);
                    }
                }
            }
            if (hasVega) {
                Matrix vega = qlInstr->result<Matrix>("vega");
                Size u = vega.rows();
                Size v = vega.columns();
                for (Size i = 0; i < u; ++i) {
                    for (Size j = 0; j < v; ++j) {
                        result.push_back(multiplier * vega[i][j] * fx);
                    }
                }
                // if it has a vega, it has a theta, too
                Real theta = qlInstr->result<Real>("theta");
                result.push_back(multiplier * theta * fx);
            }
        } else if (trade->tradeType() == "FxOption") {
            // FxOption
            boost::shared_ptr<FxOption> fxOpt = boost::dynamic_pointer_cast<FxOption>(trade);
            QL_REQUIRE(fxOpt, "SensitivityCalculator: Expected FxOption trade class, could not cast");
            std::string forCcy = fxOpt->boughtCurrency();
            std::string domCcy = fxOpt->soldCurrency();
            QL_REQUIRE(domCcy == baseCcyCode_,
                       "SensitivitiyCalculator: Can only write sensitivities for for-base trades, base is "
                           << baseCcyCode_ << ", dom is " << domCcy);
            Real fx = simMarket->fxSpot(forCcy + domCcy)->value();
            boost::shared_ptr<Instrument> qlInstr = fxOpt->instrument()->qlInstrument();
            if (qlInstr->isExpired())
                return result;
            Real multiplier = fxOpt->instrument()->multiplier();
            result.push_back(fx);
            result.push_back(multiplier * qlInstr->result<Real>("deltaSpot"));
            if (compute2ndOrder_)
                result.push_back(multiplier * qlInstr->result<Real>("gammaSpot"));
            std::vector<Real> vega = qlInstr->result<std::vector<Real>>("vega");
            std::vector<Real> deltaRate = qlInstr->result<std::vector<Real>>("deltaRate");
            std::vector<Real> deltaDiv = qlInstr->result<std::vector<Real>>("deltaDividend");
            Size w = vega.size();
            Size n = deltaRate.size();
            for (Size i = 0; i < w; ++i)
                result.push_back(multiplier * vega[i]);
            for (Size i = 0; i < n; ++i)
                result.push_back(multiplier * deltaRate[i]);
            for (Size i = 0; i < n; ++i)
                result.push_back(multiplier * deltaDiv[i]);
            if (compute2ndOrder_) {
                Matrix gamma = qlInstr->result<Matrix>("gamma");
                std::vector<Real> spotRate = qlInstr->result<std::vector<Real>>("gammaSpotRate");
                std::vector<Real> spotDiv = qlInstr->result<std::vector<Real>>("gammaSpotDiv");
                for (Size i = 0; i < 2 * n; ++i) {
                    for (Size j = 0; j < i; ++j) {
                        result.push_back(multiplier * gamma[i][j]);
                    }
                }
                for (Size i = 0; i < n; ++i)
                    result.push_back(multiplier * spotRate[i]);
                for (Size i = 0; i < n; ++i)
                    result.push_back(multiplier * spotDiv[i]);
            }
        } else {
            QL_FAIL("trade does not provide sensitivities");
        }
    } catch (std::exception& e) {
        ALOG("Failed to get sensitivities for trade " << trade->id() << " : " << e.what());
    } catch (...) {
        ALOG("Failed to get sensitivities for trade " << trade->id() << " : Unhandled Exception");
    }
    return result;
}

} // namespace analytics
} // namespace ore
