/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
 All rights reserved.
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

#include <ored/model/modelbuilder.hpp>
#include <ored/model/crcirdata.hpp>

namespace ore {
namespace data {
using namespace QuantLib;

//! Builder for a cir model component
class CrCirBuilder : public ModelBuilder {
public:
    CrCirBuilder(const boost::shared_ptr<ore::data::Market>& market, const boost::shared_ptr<CrCirData>& data,
                 const std::string& configuration = Market::defaultConfiguration);

    Real error() const;
    boost::shared_ptr<QuantExt::CrCirpp> model() const;
    boost::shared_ptr<QuantExt::CrCirppParametrization> parametrization() const;

    bool requiresRecalibration() const override { return false; }

private:
    void performCalculations() const override {}
    void buildBasket() const;

    boost::shared_ptr<ore::data::Market> market_;
    const std::string configuration_;
    boost::shared_ptr<CrCirData> data_;

    Handle<YieldTermStructure> rateCurve_;
    Handle<DefaultProbabilityTermStructure> creditCurve_;
    Handle<Quote> recoveryRate_;

    mutable Real error_;
    boost::shared_ptr<QuantExt::CrCirpp> model_;
    boost::shared_ptr<QuantExt::CrCirppParametrization> parametrization_;

    // TODO: Move CalibrationErrorType, optimizer and end criteria parameters to data
    boost::shared_ptr<OptimizationMethod> optimizationMethod_;
    EndCriteria endCriteria_;
    BlackCalibrationHelper::CalibrationErrorType calibrationErrorType_;
};

// implementation

inline Real CrCirBuilder::error() const {
    calculate();
    return error_;
}

inline boost::shared_ptr<QuantExt::CrCirpp> CrCirBuilder::model() const {
    calculate();
    return model_;
}

inline boost::shared_ptr<QuantExt::CrCirppParametrization> CrCirBuilder::parametrization() const {
    return parametrization_;
}

} // namespace data
} // namespace ore
