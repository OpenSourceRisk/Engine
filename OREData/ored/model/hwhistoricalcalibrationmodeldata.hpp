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

/*! \file ored/model/crossassetmodeldata.hpp
    \brief Cross asset model data
    \ingroup models
*/

#pragma once

#include <map>
#include <ql/currency.hpp>
#include <ql/math/matrix.hpp>
#include <ql/time/date.hpp>
#include <ql/time/period.hpp>
#include <ql/types.hpp>
#include <string>
#include <vector>

#include <ored/utilities/parsers.hpp>
#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {

// Holds configuration, historical input maps and (after calibration) output results for HW historical calibration.
class HwHistoricalCalibrationModelData : public XMLSerializable {
public:
    HwHistoricalCalibrationModelData() = default;

    // ------------ Inputs / Configuration ------------
    QuantLib::Date asOf;
    std::vector<QuantLib::Period> curveTenors;
    //QuantLib::Currency baseCurrency;
    //std::vector<QuantLib::Currency> foreignCurrencies;
    QuantLib::Real lambda;
    bool useForwardRate;
    QuantLib::Real varianceRetained;
    bool pcaCalibration = false;
    bool meanReversionCalibration = false;

    QuantLib::Size basisFunctionNumber;
    QuantLib::Real kappaUpperBound;
    QuantLib::Size haltonMaxGuess;

    std::map<std::string, std::map<QuantLib::Date, std::vector<QuantLib::Real>>> irCurves;
    std::map<std::string, std::map<QuantLib::Date, QuantLib::Real>> fxSpots;
    std::map<std::string, QuantLib::Array> eigenValues;
    std::map<std::string, QuantLib::Matrix> eigenVectors;

    // ------------ Outputs (populated by builder/model) ------------
    std::map<std::string, Size> principalComponents;
    std::map<std::string, Matrix> kappa, v, irSigma;
    std::map<std::string, Array> irKappa;
    std::map<std::string, Real> fxSigma;
    std::map<std::pair<std::string, std::string>, Matrix> rho;

    // ------------ XMLSerializable ------------
    void fromXML(XMLNode* node) override {};
    XMLNode* toXML(XMLDocument& doc) const override;

    XMLNode* toXML2(XMLDocument& doc) const;

private:

};

} // namespace data
} // namespace ore
