/*
 Copyright (C) 2021 Quaternion Risk Management Ltd
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

#include <qle/models/lgmcalibrationinfo.hpp>

namespace QuantExt {

std::map<std::string, boost::any> getAdditionalResultsMap(const LgmCalibrationInfo& info) {
    std::map<std::string, boost::any> result;
    if (info.valid) {
        result["lgmCalibrationError"] = info.rmse;
        std::vector<Real> timeToExpiry, swapLength, strike, atmForward, annuity, vega, vols;
        std::vector<Real> modelTime, modelVol, marketVol, modelValue, marketValue, modelAlpha, modelKappa, modelHwSigma;
        std::vector<Real> volDiff, valueDiff;
        for (auto const& d : info.swaptionData) {
            timeToExpiry.push_back(d.timeToExpiry);
            swapLength.push_back(d.swapLength);
            strike.push_back(d.strike);
            atmForward.push_back(d.atmForward);
            annuity.push_back(d.annuity);
            vega.push_back(d.vega);
            vols.push_back(d.stdDev / std::sqrt(d.timeToExpiry));
        }
        for (auto const& d : info.lgmCalibrationData) {
            modelTime.push_back(d.modelTime);
            modelVol.push_back(d.modelVol);
            marketVol.push_back(d.marketVol);
            modelValue.push_back(d.modelValue);
            marketValue.push_back(d.marketValue);
            modelAlpha.push_back(d.modelAlpha);
            modelKappa.push_back(d.modelKappa);
            modelHwSigma.push_back(d.modelHwSigma);
            volDiff.push_back(d.modelVol - d.marketVol);
            valueDiff.push_back(d.modelValue - d.marketValue);
        }
        result["lgmCalibrationBasketExpiryTimes"] = timeToExpiry;
        result["lgmCalibrationBasketSwapLengths"] = swapLength;
        result["lgmCalibrationBasketStrikes"] = strike;
        result["lgmCalibrationBasketAtmForwards"] = atmForward;
        result["lgmCalibrationBasketAnnuities"] = annuity;
        result["lgmCalibrationBasketVegas"] = vega;
        result["lgmCalibrationBasketVols"] = vols;
        result["lgmCalibrationTimes"] = modelTime;
        result["lgmCalibrationModelVols"] = modelVol;
        result["lgmCalibrationMarketVols"] = marketVol;
        result["lgmCalibrationModelValues"] = modelValue;
        result["lgmCalibrationMarketValues"] = marketValue;
        result["lgmCalibrationModelAlphas"] = modelAlpha;
        result["lgmCalibrationModelKappas"] = modelKappa;
        result["lgmCalibrationModelHwSigmas"] = modelHwSigma;
        result["lgmCalibrationModelMarketVolDiffs"] = volDiff;
        result["lgmCalibrationModelMarketValueDiffs"] = valueDiff;
    }
    return result;
}

} // namespace QuantExt
