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
#include <ql/math/matrix.hpp>
#include <ql/time/date.hpp>
#include <ql/time/period.hpp>
#include <ql/types.hpp>
#include <string>
#include <vector>

#include <ored/utilities/xmlutils.hpp>

namespace ore {
namespace data {

using namespace QuantLib;

class HwHistoricalCalibrationModelData : public XMLSerializable {
public:
    HwHistoricalCalibrationModelData() = default;

    void fromXML(XMLNode* node) override {};
    XMLNode* toXML(XMLDocument& doc) const override;
    XMLNode* toXML2(XMLDocument& doc) const;

    // Setters
    void setAsOf(const Date& d) { asOf_ = d; }
    void setBaseCurrency(const std::string& ccy) { baseCurrency_ = ccy; }
    void setForeignCurrencies(const std::vector<std::string>& ccy) { foreignCurrencies_ = ccy; }
    void setCurveTenors(const std::vector<Period>& v) { curveTenors_ = v; }
    void setLambda(Real l) { lambda_ = l; }
    void setUseForwardRate(bool b) { useForwardRate_ = b; }
    void setVarianceRetained(Real v) { varianceRetained_ = v; }
    void setPcaCalibration(bool b) { pcaCalibration_ = b; }
    void setMeanReversionCalibration(bool b) { meanReversionCalibration_ = b; }
    void setMeanReversionParams(Size basisFunctionNumber, Real kappaUpperBound, Size haltonMaxGuess) {
        basisFunctionNumber_ = basisFunctionNumber;
        kappaUpperBound_ = kappaUpperBound;
        haltonMaxGuess_ = haltonMaxGuess;
    }
    void setIrCurves(std::map<std::string, std::map<Date, std::vector<Real>>>&& v) { irCurves_ = std::move(v); }
    void setFxSpots(std::map<std::string, std::map<Date, Real>>&& v) { fxSpots_ = std::move(v); }

    void setPcaFromInput(std::map<std::string, Size>&& principalComponents, std::map<std::string, Array>&& eigenValues, std::map<std::string, Matrix>&& eigenVectors) {
        principalComponents_ = std::move(principalComponents);
        eigenValues_ = std::move(eigenValues);
        eigenVectors_ = std::move(eigenVectors);
    }

    void setPcaResults(std::map<std::string, Array>&& eigenValues, std::map<std::string, Matrix>&& eigenVectors,
                       std::map<std::string, Size>&& principalComponents, std::map<std::string, Real>&& fxSigma,
                       std::map<std::pair<std::string, std::string>, Matrix>&& rho) {
        eigenValues_ = std::move(eigenValues);
        eigenVectors_ = std::move(eigenVectors);
        principalComponents_ = std::move(principalComponents);
        fxSigma_ = std::move(fxSigma);
        rho_ = std::move(rho);
    }

    void setMeanReversionResults(std::map<std::string, Matrix> kappa, std::map<std::string, Matrix> v,
                                 std::map<std::string, Matrix> irSigma, std::map<std::string, Array> irKappa) {
        kappa_ = std::move(kappa);
        v_ = std::move(v);
        irSigma_ = std::move(irSigma);
        irKappa_ = std::move(irKappa);
    }

    // Getters
    const Date& asOf() const { return asOf_; }
    const std::vector<Period>& curveTenors() const { return curveTenors_; }
    Real lambda() const { return lambda_; }
    bool useForwardRate() const { return useForwardRate_; }
    Real varianceRetained() const { return varianceRetained_; }
    bool pcaCalibration() const { return pcaCalibration_; }
    bool meanReversionCalibration() const { return meanReversionCalibration_; }
    Size basisFunctionNumber() const { return basisFunctionNumber_; }
    Real kappaUpperBound() const { return kappaUpperBound_; }
    Size haltonMaxGuess() const { return haltonMaxGuess_; }
    const std::map<std::string, std::map<QuantLib::Date, std::vector<QuantLib::Real>>>& irCurves() const {
        return irCurves_;
    }
    const std::map<std::string, std::map<QuantLib::Date, QuantLib::Real>>& fxSpots() const { return fxSpots_; }
    const std::map<std::string, QuantLib::Array>& eigenValues() const { return eigenValues_; }
    const std::map<std::string, QuantLib::Matrix>& eigenVectors() const { return eigenVectors_; }
    const std::map<std::string, Size>& principalComponents() const { return principalComponents_; }
    const std::map<std::string, Matrix>& kappa() const { return kappa_; }
    const std::map<std::string, Matrix>& v() const { return v_; }
    const std::map<std::string, Matrix>& irSigma() const { return irSigma_; }
    const std::map<std::string, Array>& irKappa() const { return irKappa_; }
    const std::map<std::string, Real>& fxSigma() const { return fxSigma_; }
    const std::map<std::pair<std::string, std::string>, Matrix>& rho() const { return rho_; }

private:
    QuantLib::Date asOf_;
    std::vector<QuantLib::Period> curveTenors_;
    std::string baseCurrency_;
    std::vector<std::string> foreignCurrencies_;
    QuantLib::Real lambda_;
    bool useForwardRate_;
    QuantLib::Real varianceRetained_;
    bool pcaCalibration_ = false;
    bool meanReversionCalibration_ = false;

    QuantLib::Size basisFunctionNumber_;
    QuantLib::Real kappaUpperBound_;
    QuantLib::Size haltonMaxGuess_;

    std::map<std::string, std::map<QuantLib::Date, std::vector<QuantLib::Real>>> irCurves_;
    std::map<std::string, std::map<QuantLib::Date, QuantLib::Real>> fxSpots_;
    std::map<std::string, QuantLib::Array> eigenValues_;
    std::map<std::string, QuantLib::Matrix> eigenVectors_;

    std::map<std::string, Size> principalComponents_;
    std::map<std::string, Matrix> kappa_, v_, irSigma_;
    std::map<std::string, Array> irKappa_;
    std::map<std::string, Real> fxSigma_;
    std::map<std::pair<std::string, std::string>, Matrix> rho_;
};

} // namespace data
} // namespace ore
