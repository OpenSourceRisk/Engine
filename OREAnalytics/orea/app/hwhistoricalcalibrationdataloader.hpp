/*
 Copyright (C) 2023 Quaternion Risk Management Ltd
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

/*! \file orea/app/hwhistoricalcalibrationdataloader.hpp
    \brief Class for loading required data for Hull-White historical calibration
    \ingroup app
*/

#include <string>
#include <vector>
#include <map>
#include <ql/quantlib.hpp>

using namespace QuantLib;

#pragma once
namespace ore {
namespace analytics {

// Loader class (curve discount factors only)
class HwHistoricalCalibrationDataLoader {
public:
    HwHistoricalCalibrationDataLoader(const std::string& baseCurrency, const std::vector<std::string>& foreignCurrency,
                                      const std::vector<QuantLib::Period>& curveTenors,
                                      const QuantLib::Date& startDate = QuantLib::Date(),
                                      const QuantLib::Date& endDate = QuantLib::Date());


    void loadHistoricalCurveDataFromCsv(const std::string& fileName);
    void loadPCAFromCsv(const std::vector<std::string>& fileNames);
    void loadFixings(const std::string& fileName);
    void cleanData();

    // Getters
    const std::map<std::string, std::map<Date, std::vector<Real>>>& getIrCurves() const { return irCurves_; }
    const std::map<std::string, std::map<Date, Real>>& getFxSpots() const { return fxSpots_; }
    const std::map<std::string, Array>& getEigenValue() const { return eigenValue_; }
    const std::map<std::string, Matrix>& getEigenVector() const { return eigenVector_; }

    // Move
    std::map<std::string, std::map<Date, std::vector<Real>>> moveIrCurves() { return std::move(irCurves_); }
    std::map<std::string, std::map<Date, Real>> moveFxSpots() { return std::move(fxSpots_); }
    std::map<std::string, Array> moveEigenValue() { return std::move(eigenValue_); }
    std::map<std::string, Matrix> moveEigenVector() { return std::move(eigenVector_); }

private:
    void loadIr(const std::string& curveId, const Date& d, const std::vector<Real>& df);
    void loadFx(const std::string& curveId, const Date& d, const Real& fxSpot);
    void loadEigenValue(const std::string& ccy, const Array& eigenValue);
    void loadEigenVector(const std::string& ccy, const Matrix& eigenVector);

    std::string baseCurrency_;
    std::vector<std::string> foreignCurrency_;
    std::vector<QuantLib::Period> tenors_;
    Date startDate_, endDate_;
    std::map<std::string, std::map<Date, std::vector<Real>>> irCurves_;
    std::map<std::string, std::map<Date, Real>> fxSpots_;
    std::map<std::string, Size> principalComponent_;
    std::map<std::string, Array> eigenValue_;
    std::map<std::string, Matrix> eigenVector_;
};

} // namespace analytics
} // namespace ore