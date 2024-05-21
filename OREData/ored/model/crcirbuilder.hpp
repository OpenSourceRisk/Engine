/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file ored/model/crcirbuilder.hpp
    \brief Build an cir model
    \ingroup models
*/

#pragma once

#include <map>
#include <ostream>
#include <vector>

#include <qle/models/crcirpp.hpp>
#include <qle/models/modelbuilder.hpp>

#include <ored/model/crcirdata.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

//! Builder for a cir model component
class CrCirBuilder : public QuantExt::ModelBuilder {
public:
    CrCirBuilder(const QuantLib::ext::shared_ptr<ore::data::Market>& market, const QuantLib::ext::shared_ptr<CrCirData>& data,
                 const std::string& configuration = Market::defaultConfiguration);

    Real error() const;
    QuantLib::ext::shared_ptr<QuantExt::CrCirpp> model() const;
    QuantLib::ext::shared_ptr<QuantExt::CrCirppParametrization> parametrization() const;

    bool requiresRecalibration() const override { return false; }

private:
    void performCalculations() const override {}
    void buildBasket() const;

    QuantLib::ext::shared_ptr<ore::data::Market> market_;
    const std::string configuration_;
    QuantLib::ext::shared_ptr<CrCirData> data_;

    Handle<YieldTermStructure> rateCurve_;
    Handle<DefaultProbabilityTermStructure> creditCurve_;
    Handle<Quote> recoveryRate_;

    mutable Real error_;
    QuantLib::ext::shared_ptr<QuantExt::CrCirpp> model_;
    QuantLib::ext::shared_ptr<QuantExt::CrCirppParametrization> parametrization_;

    // TODO: Move CalibrationErrorType, optimizer and end criteria parameters to data
    QuantLib::ext::shared_ptr<OptimizationMethod> optimizationMethod_;
    EndCriteria endCriteria_;
    BlackCalibrationHelper::CalibrationErrorType calibrationErrorType_;
};

// implementation

inline Real CrCirBuilder::error() const {
    calculate();
    return error_;
}

inline QuantLib::ext::shared_ptr<QuantExt::CrCirpp> CrCirBuilder::model() const {
    calculate();
    return model_;
}

inline QuantLib::ext::shared_ptr<QuantExt::CrCirppParametrization> CrCirBuilder::parametrization() const {
    return parametrization_;
}

} // namespace data
} // namespace ore
