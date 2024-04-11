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

#pragma once

#include <orea/cube/npvcube.hpp>
#include <orea/simulation/simmarket.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/utilities/dategrid.hpp>

namespace ore {
namespace analytics {
using ore::data::Trade;
using QuantLib::Date;
using QuantLib::Real;
using QuantLib::Size;

//! ValuationCalculator interface
class ValuationCalculator {
public:
    virtual ~ValuationCalculator() {}

    virtual void calculate(
        //! The trade
        const QuantLib::ext::shared_ptr<Trade>& trade,
        //! Trade index for writing to the cube
        Size tradeIndex,
        //! The market
        const QuantLib::ext::shared_ptr<SimMarket>& simMarket,
        //! The cube for data on trade level
        QuantLib::ext::shared_ptr<NPVCube>& outputCube,
        //! The cube for data on netting set level
        QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet,
        //! The date
        const Date& date,
        //! Date index
        Size dateIndex,
        //! Sample
        Size sample,
        //! isCloseOut
        bool isCloseOut = false) = 0;

    virtual void calculateT0(
        //! The trade
        const QuantLib::ext::shared_ptr<Trade>& trade,
        //! Trade index for writing to the cube
        Size tradeIndex,
        //! The market
        const QuantLib::ext::shared_ptr<SimMarket>& simMarket,
        //! The cube
        QuantLib::ext::shared_ptr<NPVCube>& outputCube,
        //! The cube
        QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet) = 0;

    // called once before the valuation engine run
    virtual void init(const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const QuantLib::ext::shared_ptr<SimMarket>& simMarket) = 0;

    // called after each scenario update before the calculators are run
    virtual void initScenario() = 0;
};

//! NPVCalculator
/*! Calculate the NPV of the given trade, convert to base currency and divide by the numeraire
 *  If the NPV() call throws, we log an exception and write 0 to the cube
 *
 */
class NPVCalculator : public ValuationCalculator {
public:
    //! base ccy and index to write to
    NPVCalculator(const std::string& baseCcyCode, Size index = 0) : baseCcyCode_(baseCcyCode), index_(index) {}

    virtual void calculate(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                           const QuantLib::ext::shared_ptr<SimMarket>& simMarket, QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                           QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet, const Date& date, Size dateIndex,
                           Size sample, bool isCloseOut = false) override;

    virtual void calculateT0(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                             const QuantLib::ext::shared_ptr<SimMarket>& simMarket, QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                             QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet) override;

    virtual Real npv(Size tradeIndex, const QuantLib::ext::shared_ptr<Trade>& trade,
                     const QuantLib::ext::shared_ptr<SimMarket>& simMarket);

    void init(const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const QuantLib::ext::shared_ptr<SimMarket>& simMarket) override;
    void initScenario() override;

protected:
    std::string baseCcyCode_;
    Size index_;

    std::vector<Handle<Quote>> ccyQuotes_;
    std::vector<double> fxRates_;
    std::vector<Size> tradeCcyIndex_;
};

//! CashflowCalculator
/*! Calculates the cashflow, converted to base ccy, from t to t+1, this interval is defined by the provided dategrid
 *  The interval is (t, t+1], i.e. we exclude todays flows and include flows that fall exactly on t+1.
 *  For t0 we do nothing (and so the cube will have a 0 value)
 */
class CashflowCalculator : public ValuationCalculator {
public:
    //! Constructor takes the base ccy, date grid and index of cube to write to.
    CashflowCalculator(const std::string& baseCcyCode, const Date& t0Date, const QuantLib::ext::shared_ptr<DateGrid>& dateGrid,
                       Size index)
        : baseCcyCode_(baseCcyCode), t0Date_(t0Date), dateGrid_(dateGrid), index_(index) {}

    virtual void calculate(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                           const QuantLib::ext::shared_ptr<SimMarket>& simMarket, QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                           QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet, const Date& date, Size dateIndex,
                           Size sample, bool isCloseOut = false) override;

    virtual void calculateT0(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                             const QuantLib::ext::shared_ptr<SimMarket>& simMarket, QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                             QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet) override {}

    void init(const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const QuantLib::ext::shared_ptr<SimMarket>& simMarket) override;
    void initScenario() override;

private:
    std::string baseCcyCode_;
    Date t0Date_;
    QuantLib::ext::shared_ptr<DateGrid> dateGrid_;
    Size index_;

    std::vector<Handle<Quote>> ccyQuotes_;
    std::vector<double> fxRates_;
    std::vector<std::vector<Size>> tradeAndLegCcyIndex_;
};

//! NPVCalculatorFXT0
/*! Calculate the NPV of the given trade, convert to base currency USING T0 RATES and divide by the numeraire
 *  This can sometimes be useful for finite difference ("bump-revalue") sensitivities
 *    (for FX spot sensis, if we wish to bump the spot in the pricing model, but still convert to base using static FX)
 *  If the NPV() call throws, we log an exception and write 0 to the cube
 *
 */
class NPVCalculatorFXT0 : public ValuationCalculator {
public:
    //! base ccy and index to write to
    NPVCalculatorFXT0(const std::string& baseCcyCode, const QuantLib::ext::shared_ptr<Market>& t0Market, Size index = 0)
        : baseCcyCode_(baseCcyCode), t0Market_(t0Market), index_(index) {}

    virtual void calculate(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                           const QuantLib::ext::shared_ptr<SimMarket>& simMarket, QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                           QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet, const Date& date, Size dateIndex,
                           Size sample, bool isCloseOut = false) override;

    virtual void calculateT0(const QuantLib::ext::shared_ptr<Trade>& trade, Size tradeIndex,
                             const QuantLib::ext::shared_ptr<SimMarket>& simMarket, QuantLib::ext::shared_ptr<NPVCube>& outputCube,
                             QuantLib::ext::shared_ptr<NPVCube>& outputCubeNettingSet) override;

    Real npv(Size tradeIndex, const QuantLib::ext::shared_ptr<Trade>& trade, const QuantLib::ext::shared_ptr<SimMarket>& simMarket);

    void init(const QuantLib::ext::shared_ptr<Portfolio>& portfolio, const QuantLib::ext::shared_ptr<SimMarket>& simMarket) override;
    void initScenario() override {}

private:
    std::string baseCcyCode_;
    QuantLib::ext::shared_ptr<Market> t0Market_;
    Size index_;

    std::vector<double> fxRates_;
    std::vector<Size> tradeCcyIndex_;
};

} // namespace analytics
} // namespace ore
