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

/*! \file orea/engine/simmsensitivitystoragemanager.hpp
    \brief class helping to manage the storage of sensitivties for SIMM in a cube
    \ingroup engine
*/

#pragma once

#include <orea/engine/sensitivitystoragemanager.hpp>

namespace ore {
namespace analytics {

using QuantLib::Weeks;
using QuantLib::Months;
using QuantLib::Years;
  
class SimmSensitivityStorageManager : public SensitivityStorageManager {
public:
    /* IR/FX Delta and Vega only */
    explicit SimmSensitivityStorageManager(const std::vector<std::string>& currencies,
					   const QuantLib::Size firstCubeIndexToUse);

    QuantLib::Size getRequiredSize() const override;

    void addSensitivities(QuantLib::ext::shared_ptr<ore::analytics::NPVCube> cube,
                          const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
                          const QuantLib::ext::shared_ptr<ore::data::Market>& market,
                          const QuantLib::Size dateIndex = QuantLib::Null<QuantLib::Size>(),
                          const QuantLib::Size sampleIndex = QuantLib::Null<QuantLib::Size>()) const override;

    /*! Return delta and vega as an Array. The coordinates of the entries are

         ccy_1        irDelta_1
                      ...
                      irDelta_nCurveTenors

         ccy_2        irDelta_1
                      ...
                      irDelta_nCurveTenors

         ...

         ccy_nCcys    irDelta_1
                      ...
                      irDelta_nCurveTenors

         log(fx)-Delta_1
         ...
         log(fx)-Delta_(nCcys-1)

         theta

         which means the number of components is nCurveTenors * nCurrencies + (nCurrencies - 1).
         All entries are in base ccy (= first ccy in currencies), the fx deltas against base ccy.
    */

    boost::any getSensitivities(const QuantLib::ext::shared_ptr<ore::analytics::NPVCube>& cube, const std::string& nettingSetId,
                                const QuantLib::Size dateIndex = QuantLib::Null<QuantLib::Size>(),
                                const QuantLib::Size sampleIndex = QuantLib::Null<QuantLib::Size>()) const override;


    const std::vector<QuantLib::Period>& irDeltaTerms() { return irDeltaTerms_; }
    const std::vector<QuantLib::Period>& irVegaTerms() { return irVegaTerms_; }
    const std::vector<QuantLib::Period>& irVegaUnderlyingTerms() { return irVegaUnderlyingTerms_; }
    const std::vector<QuantLib::Period>& fxVegaTerms() { return fxVegaTerms_; }
  
private:
    QuantLib::Size getCcyIndex(const std::string& ccy) const;

    void processSwapSwaption(QuantLib::Array& delta, std::vector<QuantLib::Matrix>& vega, QuantLib::Real& theta,
                             const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
                             const QuantLib::ext::shared_ptr<ore::data::Market>& market) const;

    void processFxOption(QuantLib::Array& delta, std::vector<QuantLib::Array>& vega, QuantLib::Real& theta,
                         const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
                         const QuantLib::ext::shared_ptr<ore::data::Market>& market) const;

    void processFxForward(QuantLib::Array& delta, QuantLib::Real& theta,
                          const QuantLib::ext::shared_ptr<ore::data::Trade>& trade,
                          const QuantLib::ext::shared_ptr<ore::data::Market>& market) const;

    // map single swaption vega to the vega matrix
    std::map<std::pair<QuantLib::Size, QuantLib::Size>, QuantLib::Real>
    swaptionVega(QuantLib::Real vega, const QuantLib::Date& expiry, const QuantLib::Date& maturity, const QuantLib::Date& referenceDate, const QuantLib::DayCounter& dc) const;

    // map single fx vega to the vega grid
    std::map<QuantLib::Size, QuantLib::Real>
    fxVega(QuantLib::Real vega, const QuantLib::Date& expiry, const QuantLib::Date& referenceDate, const QuantLib::DayCounter& dc) const;

    std::vector<std::string> currencies_;
    QuantLib::Size nCurveTenors_;
    QuantLib::Size nSwaptionExpiries_;
    QuantLib::Size nSwaptionTerms_;
    QuantLib::Size nFxExpiries_;
    QuantLib::Size firstCubeIndexToUse_;
    std::vector<Real> swaptionExpiryTimes_;
    std::vector<Real> swaptionTermTimes_;
    std::vector<Real> fxExpiryTimes_;
    QuantLib::Size nc_, nco_;
    QuantLib::Size nx_, nxo_;

    QuantLib::Size N_;

    // define SIMM buckets

    std::vector<QuantLib::Period> irDeltaTerms_{2 * Weeks, 1 * Months, 3 * Months, 6 * Months, 1 * Years,  2 * Years,
                                                3 * Years, 5 * Years,  10 * Years, 15 * Years, 20 * Years, 30 * Years};

    std::vector<QuantLib::Period> irVegaTerms_{2 * Weeks, 1 * Months, 3 * Months, 6 * Months, 1 * Years,  2 * Years,
                                               3 * Years, 5 * Years,  10 * Years, 15 * Years, 20 * Years, 30 * Years};

    std::vector<QuantLib::Period> irVegaUnderlyingTerms_{1 * Years, 2 * Years, 3 * Years, 5 * Years, 10 * Years};

    std::vector<QuantLib::Period> fxVegaTerms_{2 * Weeks, 1 * Months, 3 * Months, 6 * Months, 1 * Years,  2 * Years,
                                               3 * Years, 5 * Years,  10 * Years, 15 * Years, 20 * Years, 30 * Years};

};

} // namespace analytics
} // namespace ore
