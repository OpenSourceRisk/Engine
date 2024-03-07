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

/*! \file ored/configuration/fxspotconfig.hpp
    \brief security spread configuration classes
    \ingroup configuration
*/

#pragma once

#include <ored/configuration/curveconfig.hpp>
#include <ored/utilities/parsers.hpp>
#include <ql/currency.hpp>
#include <ql/types.hpp>

namespace ore {
namespace data {
using ore::data::XMLNode;
using QuantLib::Currency;
using std::string;
using std::vector;

//! FXSpot configuration
/*!
  \ingroup configuration
*/
class FXSpotConfig : public CurveConfig {
public:
    //! \name Constructors/Destructors
    //@{
    //! Detailed constructor
    FXSpotConfig(const string& curveID, const string& curveDescription) : CurveConfig(curveID, curveDescription) {
        QL_REQUIRE(curveID.size() == 6, "FXSpot curveID must be of the form Ccy1Ccy2");
        Currency ccy1 = parseCurrency(curveID.substr(0, 3));
        Currency ccy2 = parseCurrency(curveID.substr(3, 3));
        quotes_.push_back("FX/RATE/" + ccy1.code() + "/" + ccy2.code());
    };
    //! Default constructor
    FXSpotConfig() {}
    //@}

    void fromXML(XMLNode* node) override {
        XMLUtils::checkNode(node, "FXSpot");
        curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
        QL_REQUIRE(curveID_.size() == 6, "FXSpot curveID must be of the form Ccy1Ccy2");
        Currency ccy1 = parseCurrency(curveID_.substr(0, 3));
        Currency ccy2 = parseCurrency(curveID_.substr(3, 3));
        quotes_.push_back("FX/RATE/" + ccy1.code() + "/" + ccy2.code());

        curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
    }

    XMLNode* toXML(XMLDocument& doc) const override {
        XMLNode* node = doc.allocNode("FXSpot");
        XMLUtils::addChild(doc, node, "CurveId", curveID_);
        XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
        return node;
    }
};
} // namespace data
} // namespace ore
