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

/*! \file ored/configuration/onedimsolverconfig.hpp
    \brief Class for holding 1-D solver configuration
    \ingroup configuration
*/

#pragma once

#include <ored/utilities/xmlutils.hpp>
#include <qle/termstructures/eqcommoptionsurfacestripper.hpp>
#include <ql/utilities/null.hpp>

namespace ore {
namespace data {

/*! Serializable 1-D solver configuration
    \ingroup configuration
*/
class OneDimSolverConfig : public XMLSerializable {
public:
    //! Default constructor with everything \c QuantLib::Null
    OneDimSolverConfig();

    //! Constructor for max min based solver configuration
    OneDimSolverConfig(QuantLib::Size maxEvaluations,
        QuantLib::Real initialGuess,
        QuantLib::Real accuracy,
        const std::pair<QuantLib::Real, QuantLib::Real>& minMax,
        QuantLib::Real lowerBound = QuantLib::Null<QuantLib::Real>(),
        QuantLib::Real upperBound = QuantLib::Null<QuantLib::Real>());

    //! Constructor for step based solver configuration
    OneDimSolverConfig(QuantLib::Size maxEvaluations,
        QuantLib::Real initialGuess,
        QuantLib::Real accuracy,
        QuantLib::Real step,
        QuantLib::Real lowerBound = QuantLib::Null<QuantLib::Real>(),
        QuantLib::Real upperBound = QuantLib::Null<QuantLib::Real>());

    //! \name XMLSerializable interface
    //@{
    void fromXML(ore::data::XMLNode* node) override;
    ore::data::XMLNode* toXML(ore::data::XMLDocument& doc) const override;
    //@}

    //! \name Inspectors
    //@{
    QuantLib::Size maxEvaluations() const { return maxEvaluations_; }
    QuantLib::Real initialGuess() const { return initialGuess_; }
    QuantLib::Real accuracy() const { return accuracy_; }
    const std::pair<QuantLib::Real, QuantLib::Real>& minMax() const { return minMax_; }
    QuantLib::Real step() const { return step_; }
    QuantLib::Real lowerBound() const { return lowerBound_; }
    QuantLib::Real upperBound() const { return upperBound_; }
    //@}

    //! Return \c true if default constructed and not populated i.e. no useful configuration.
    bool empty() const { return empty_; }

    //! Conversion to QuantExt::Solver1DOptions
    operator QuantExt::Solver1DOptions() const;

private:
    QuantLib::Size maxEvaluations_;
    QuantLib::Real initialGuess_;
    QuantLib::Real accuracy_;
    std::pair<QuantLib::Real, QuantLib::Real> minMax_;
    QuantLib::Real step_;
    QuantLib::Real lowerBound_;
    QuantLib::Real upperBound_;

    // Set to false if members are initialised.
    bool empty_;

    //! Basic checks
    void check() const;
};

}
}
