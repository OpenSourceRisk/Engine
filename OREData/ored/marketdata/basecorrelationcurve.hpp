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

#include <qle/termstructures/credit/basecorrelationstructure.hpp>

namespace ore {
namespace data {

class ReferenceDataManager;

//! Wrapper class for building Base Correlation structures
//! \ingroup curves
class BaseCorrelationCurve {
public:
    BaseCorrelationCurve() {}
    BaseCorrelationCurve(QuantLib::Date asof, BaseCorrelationCurveSpec spec, const Loader& loader,
                         const CurveConfigurations& curveConfigs,
        const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceData = nullptr);

    //! \name Inspectors
    //@{
    const BaseCorrelationCurveSpec& spec() const { return spec_; }
    //! Base Correlation term structure
    const QuantLib::ext::shared_ptr<QuantExt::BaseCorrelationTermStructure>& baseCorrelationTermStructure() const {
        return baseCorrelation_;
    }
    //@}
private:
    BaseCorrelationCurveSpec spec_;
    QuantLib::ext::shared_ptr<QuantExt::BaseCorrelationTermStructure> baseCorrelation_;
    QuantLib::ext::shared_ptr<ReferenceDataManager> referenceData_;

    /*! Use the reference data to adjust the detachment points, \p detachPoints, for existing losses if requested.
    */
    std::vector<QuantLib::Real> adjustForLosses(const std::vector<QuantLib::Real>& detachPoints) const;
};
} // namespace data
} // namespace ore
