/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file ored/marketdata/basecorrelationcurve.hpp
    \brief Wrapper class for building base correlation structures
    \ingroup curves
*/

#pragma once

#include <ored/configuration/curveconfigurations.hpp>
#include <ored/marketdata/curvespec.hpp>
#include <ored/marketdata/loader.hpp>

#include <ql/experimental/credit/basecorrelationstructure.hpp>

namespace ore {
namespace data {
using ore::data::CurveConfigurations;
using QuantLib::Date;

typedef QuantLib::BaseCorrelationTermStructure<QuantLib::BilinearInterpolation> BilinearBaseCorrelationTermStructure;

//! Wrapper class for building Base Correlation structures
//! \ingroup curves
class BaseCorrelationCurve {
public:
    BaseCorrelationCurve() {}
    BaseCorrelationCurve(Date asof, BaseCorrelationCurveSpec spec, const Loader& loader,
                         const CurveConfigurations& curveConfigs);

    //! \name Inspectors
    //@{
    const BaseCorrelationCurveSpec& spec() const { return spec_; }
    //! Base Correlation term structure
    const boost::shared_ptr<BilinearBaseCorrelationTermStructure>& baseCorrelationTermStructure() const {
        return baseCorrelation_;
    }
    //@}
private:
    BaseCorrelationCurveSpec spec_;
    boost::shared_ptr<BilinearBaseCorrelationTermStructure> baseCorrelation_;
};
} // namespace data
} // namespace ore
