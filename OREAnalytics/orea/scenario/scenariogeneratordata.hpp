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

namespace ore {
namespace analytics {
using namespace QuantLib;
using namespace QuantExt;

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
    ScenarioGeneratorData()
        : grid_(QuantLib::ext::make_shared<DateGrid>()), sequenceType_(SobolBrownianBridge), seed_(0), samples_(0),
          ordering_(SobolBrownianGenerator::Steps), directionIntegers_(SobolRsg::JoeKuoD7), withCloseOutLag_(false),
          withMporStickyDate_(false) {}

    //! Constructor
    ScenarioGeneratorData(QuantLib::ext::shared_ptr<DateGrid> dateGrid, SequenceType sequenceType, long seed, Size samples,
                          SobolBrownianGenerator::Ordering ordering = SobolBrownianGenerator::Steps,
                          SobolRsg::DirectionIntegers directionIntegers = SobolRsg::JoeKuoD7,
                          bool withCloseOutLag = false, bool withMporStickyDate = false)
        : sequenceType_(sequenceType), seed_(seed), samples_(samples), ordering_(ordering),
          directionIntegers_(directionIntegers), withCloseOutLag_(false), withMporStickyDate_(false) {
        setGrid(dateGrid);
    }

    void clear();

    //! Load members from XML
    virtual void fromXML(XMLNode* node) override;

    //! Write members to XML
    virtual XMLNode* toXML(XMLDocument& doc) const override;

    //! \name Inspectors
    //@{
    QuantLib::ext::shared_ptr<DateGrid> getGrid() const { return grid_; }
    SequenceType sequenceType() const { return sequenceType_; }
    long seed() const { return seed_; }
    Size samples() const { return samples_; }
    SobolBrownianGenerator::Ordering ordering() const { return ordering_; }
    SobolRsg::DirectionIntegers directionIntegers() const { return directionIntegers_; }
    QuantLib::ext::shared_ptr<DateGrid> closeOutDateGrid() const { return closeOutDateGrid_; }
    bool withCloseOutLag() const { return withCloseOutLag_; }
    bool withMporStickyDate() const { return withMporStickyDate_; }
    Period closeOutLag() const { return closeOutLag_; }
    //@}

    //! \name Setters
    //@{
    void setGrid(QuantLib::ext::shared_ptr<DateGrid> grid);
    SequenceType& sequenceType() { return sequenceType_; }
    long& seed() { return seed_; }
    Size& samples() { return samples_; }
    SobolBrownianGenerator::Ordering& ordering() { return ordering_; }
    SobolRsg::DirectionIntegers& directionIntegers() { return directionIntegers_; }
    bool& withCloseOutLag() { return withCloseOutLag_; }
    bool& withMporStickyDate() { return withMporStickyDate_; }
    Period& closeOutLag() { return closeOutLag_; }
    //@}
private:
    QuantLib::ext::shared_ptr<DateGrid> grid_;
    SequenceType sequenceType_;
    long seed_;
    Size samples_;
    SobolBrownianGenerator::Ordering ordering_;
    SobolRsg::DirectionIntegers directionIntegers_;
    QuantLib::ext::shared_ptr<DateGrid> closeOutDateGrid_;
    bool withCloseOutLag_;
    bool withMporStickyDate_;
    Period closeOutLag_;
    MporCashFlowMode mporCashFlowMode_;
    string gridString_;
};

} // namespace analytics
} // namespace ore
