/*
  Copyright (C) 2021 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*!
  \file orea/app/calibrateparameter.hpp
  \calibrate parameter (sigma) for PFE model
 */

#pragma once

#include <ql/currency.hpp>
#include <ql/math/matrix.hpp>
#include <ql/time/date.hpp>

#include <map>
#include <vector>

using namespace QuantLib;

namespace QuantExt {

class HwHistoricalCalibrationModel {
public:
    HwHistoricalCalibrationModel(const Date& asOfDate, const std::vector<Period>& curveTenor, const Real& lambda,
                                 const bool& useForwardRate,
                                 const std::map<std::string, std::map<Date, std::vector<Real>>>& dataIR,
                                 const std::map<std::string, std::map<Date, Real>>& dataFX);

    HwHistoricalCalibrationModel(const Date& asOfDate, const std::vector<Period>& curveTenor,
                                 const bool& useForwardRate, const std::map<std::string, Array>& eigenValue,
                                 const std::map<std::string, Matrix>& eigenVector);

    void pcaCalibration(const Real& varianceRetained);
    void meanReversionCalibration(const Size& basisFunctionNumber, const Real& kappaUpperBound, const Size& maxGuess = 500);

    // Getters
    std::map<std::string, Size> principalComponent() const { return principalComponent_; };
    Size basisFunctionNumber() const { return basisFunctionNumber_; };
    std::map<std::string, Array> eigenValue() const { return eigenValue_; };
    std::map<std::string, Matrix> eigenVector() const { return eigenVector_; };
    Size tenorSize() const { return curveTenor_.size(); };
    std::map<std::string, Matrix> v() const { return v_; };
    std::map<std::string, Matrix> kappa() const { return kappa_; };
    std::map<std::string, Matrix> irSigma() const { return sigmaFormatted_; };
    std::map<std::string, Array> irKappa() const { return kappaFormatted_; };
    std::map<std::string, Real> fxSigma() const { return fxSigma_; };
    std::map<std::pair<std::string, std::string>, Matrix> rho() const { return correlationMatrix_; };

private:
    //const Currency baseCurrency_;
    //const std::vector<Currency> foreignCurrency_;
    const Date asOfDate_;
    const std::vector<Period> curveTenor_;
    const Real returnThreshold_ = 0.0050;
    const Real averageThreshold_ = 0.0001;
    const Real lambda_ = 1.0;
    const bool useForwardRate_;
    std::vector<Real> curveTenorReal_;
    Size basisFunctionNumber_ = 0;
    std::map<std::string, Size> principalComponent_;
    std::map<std::string, Real> fxVariance_, fxSigma_;
    std::map<std::string, Array> eigenValue_, fxLogReturn_, kappaFormatted_;
    std::map<std::string, Matrix> eigenVector_, v_, kappa_;
    std::map<std::string, std::map<Date, std::vector<Real>>> dataIR_;
    std::map<std::string, std::map<Date, Real>> dataFX_;
    std::map<std::string, Matrix> irAbsoluteReturn_, irCovariance_, irAbsoluteReturnAdjusted_, sigmaFormatted_;
    //std::map<std::string, bool> fxInverse_;
    std::map<std::pair<std::string, std::string>, Matrix> correlationMatrix_;
    //void initialize();
    void computeIrAbsoluteReturn();
    void computeFxLogReturn();
    void computeCorrelation();
    void pca(const Real& varianceRetained);
    Real correlation(Array arr1, Array arr2);
    void formatIrKappa();
    void formatIrSigma();
};

} // namespace quantext
