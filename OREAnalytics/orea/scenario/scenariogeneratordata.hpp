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

/*! \file scenario/scenariogeneratordata.hpp
    \brief Scenario generator configuration
    \ingroup scenario
*/

#pragma once

#include <vector>

#include <ql/types.hpp>

#include <qle/models/crossassetmodel.hpp>

#include <orea/scenario/crossassetmodelscenariogenerator.hpp>
#include <orea/scenario/scenariosimmarketparameters.hpp>
#include <ored/utilities/xmlutils.hpp>

using namespace QuantLib;
using namespace QuantExt;

namespace ore {
namespace analytics {

//! Scenario Generator description
/*! ScenarioGeneratorData wraps the specification of how to build a scenario generator
  from a given cross asset model and covers the choice and configuration of
  - state process
  - simulation date grid
  - multipath generator
  - scenario factory
  - fixing method

  \ingroup scenario
 */
class ScenarioGeneratorData : public XMLSerializable {
public:
    //! Supported sequence types
    enum class SequenceType { MersenneTwister, MersenneTwisterAntithetic, Sobol, SobolBrownianBridge };

    ScenarioGeneratorData() {}

    //! Constructor
    ScenarioGeneratorData(CrossAssetStateProcess::discretization discretization,
                          boost::shared_ptr<ore::analytics::DateGrid> dateGrid, SequenceType sequenceType, long seed,
                          Size samples)
        : discretization_(discretization), grid_(dateGrid), sequenceType_(sequenceType), seed_(seed),
          samples_(samples) {}

    void clear();

    //! Load members from XML
    virtual void fromXML(XMLNode* node);

    //! Write members to XML
    virtual XMLNode* toXML(XMLDocument& doc);

    //! \name Inspectors
    //@{
    CrossAssetStateProcess::discretization discretization() const { return discretization_; }
    boost::shared_ptr<ore::analytics::DateGrid> grid() const { return grid_; }
    SequenceType sequenceType() const { return sequenceType_; }
    long seed() const { return seed_; }
    Size samples() const { return samples_; }
    //@}

    //! \name Setters
    //@{
    CrossAssetStateProcess::discretization& discretization() { return discretization_; }
    boost::shared_ptr<ore::analytics::DateGrid>& grid() { return grid_; }
    SequenceType& sequenceType() { return sequenceType_; }
    long& seed() { return seed_; }
    Size& samples() { return samples_; }
    //@}
private:
    CrossAssetStateProcess::discretization discretization_;
    boost::shared_ptr<ore::analytics::DateGrid> grid_;
    SequenceType sequenceType_;
    long seed_;
    Size samples_;
};

//! Enum parsers used in ScenarioGeneratorBuilder's fromXML
ScenarioGeneratorData::SequenceType parseSequenceType(const string& s);
CrossAssetStateProcess::discretization parseDiscretization(const string& s);

//! Enum to string used in ScenarioGeneratorData's toXML
std::ostream& operator<<(std::ostream& out, const ScenarioGeneratorData::SequenceType& type);
std::ostream& operator<<(std::ostream& out, const CrossAssetStateProcess::discretization& type);
} // namespace analytics
} // namespace ore
