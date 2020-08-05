/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
 All rights reserved.
*/

/*! \file engine/sensitivitycalculator.hpp
    \brief sensitivity calculator
    \ingroup engine
*/

#pragma once

#include <orea/cube/npvcube.hpp>
#include <orea/engine/valuationcalculator.hpp>
#include <ored/utilities/dategrid.hpp>
#include <orea/simulation/simmarket.hpp>
#include <ored/portfolio/trade.hpp>

using ore::data::Trade;
using QuantLib::Date;
using QuantLib::Size;
using QuantLib::Real;

namespace ore {
namespace analytics {

//! SensitivityCalculator
/*! Calculates the sensitivities. The values are stored starting at the given index.

    Swap: | FxSpot deltaFxSpot deltaDiscount deltaForward gamma |
          If we have n bucket times, this occupies 2 + 2n + 2n(2n+1)/2 entries. For the gamma the lower triangle of
          the matrix is stored only. deltaFxSpot is set to zero if the ccy of the swap is the base ccy.
          The FxSpot rate itself it stored so that it can be used to convert the sensitivities to the base ccy and to
          convert the sensitivities to FxSpot to sensitivities to log(FxSpot).

    CurrencySwap: | FxSpot_ccy1 FxSpot_ccy2 deltaFxSpot_ccy1 deltaFxSpot_ccy2
                    deltaDiscount_ccy1 deltaForward_ccy1 deltaDiscount_ccy2 deltaForward_ccy2
                    gamma_ccy1 gamma_ccy2 |
          ccy1 and ccy2 refer to the two currencies of the swap, in alphabetical order,
          swaps with more than two currencies are not supported.
          If we have n bucket times this occupies 4 + 4n + 4n(2n+1)/2 entries.
          deltaFxSpot_ccy1 and ccy2 are set to zero if the ccy of the swap is the base ccy.

    EuropeanSwaption: | FxSpot deltaFxSpot deltaDiscount deltaForward gamma vega theta |
                      If we have n delta-gamma bucket times, and u x v vega bucket times (opt x und), this occupies
                      2 + 2n + 2n(2n+1)/2 + uv + 1 entries. deltaFxSpot is set to zero if the ccy of the swaption is
                      the base ccy.

    FXForward: | FxSpot_ccy1 FxSpot_ccy2 deltaFxSpot_ccy1 deltaFxSpot_ccy2 (4)
                    deltaDiscount_ccy1 deltaDiscount_ccy2 (2n)
                    gamma_ccy1 gamma_ccy2 | (2n(n+1)/2)
          ccy1 and ccy2 refer to the two currencies of the trade, in alphabetical order,
          If we have n bucket times this occupies 4 + 2n + 2 n(n+1)/2 entries.
          deltaFxSpot_ccy1 resp. deltaFxSpot_ccy2 are set to zero if ccy1 resp. ccy2 is the base ccy.

    FXOption : | FxSpot deltaFxSpot gammaFxSpot vega deltaRate deltaDividend gamma gammaSpotRate gammaSpotDiv |
               If we have n delta-gamma bucket times and w vega bucket times, this occupies
               3 + w + n + n + 2n(2n+1)/2 + n + n entries.
               Warning: FX options must be FOR-DOM with DOM identical to the base ccy.
 */
class SensitivityCalculator : public ValuationCalculator {
public:
    //! Constructor takes the base ccy, date grid and index of cube to write to.
    SensitivityCalculator(const std::string& baseCcyCode, Size index, const bool compute2ndOrder)
        : baseCcyCode_(baseCcyCode), index_(index), compute2ndOrder_(compute2ndOrder) {
        // TODO, need to reorder the entries, so that the position of the first order sensitivities
        // does not depend on whether we compute second order sensitivities
        QL_REQUIRE(compute2ndOrder_,
                   "SensitivitiyCalculator only implemented to work with 1st and 2nd order sensitivity calculation");
    }

    virtual void calculate(const boost::shared_ptr<Trade>& trade, Size tradeIndex,
                           const boost::shared_ptr<SimMarket>& simMarket, boost::shared_ptr<NPVCube>& outputCube,
                           const Date& date, Size dateIndex, Size sample, bool isCloseOut = false);

    virtual void calculateT0(const boost::shared_ptr<Trade>& trade, Size tradeIndex,
                             const boost::shared_ptr<SimMarket>& simMarket, boost::shared_ptr<NPVCube>& outputCube);

private:
    QuantLib::Disposable<std::vector<Real>> sensis(const boost::shared_ptr<Trade>& trade,
                                                   const boost::shared_ptr<SimMarket>& simMarket);
    std::string baseCcyCode_;
    Size index_;
    const bool compute2ndOrder_;
};

} // analytics
} // openriskengine
