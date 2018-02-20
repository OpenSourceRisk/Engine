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

/*! \file ored/configuration/securityconfig.hpp
    \brief security spread configuration classes
    \ingroup configuration
*/

#pragma once

#include <ored/configuration/curveconfig.hpp>
#include <ql/types.hpp>

using std::string;
using std::vector;
using ore::data::XMLNode;

namespace ore {
namespace data {

//! Security configuration
/*!
  \ingroup configuration
*/
class SecurityConfig : public CurveConfig {
public:
    //! \name Constructors/Destructors
    //@{
    //! Detailed constructor
    SecurityConfig(const string& curveID, const string& curveDescription, const string& spreadQuote,
                   const string& recoveryQuote)
        : CurveConfig(curveID, curveDescription) {
        quotes_ = {spreadQuote, recoveryQuote};
    };
    //! Default constructor
    SecurityConfig() {}
    //@}

    //! \name Inspectors
    //@{
    const string& spreadQuote() { return quotes_[0]; }
    const string& recoveryRatesQuote() { return quotes_[1]; }
    //@}

    void fromXML(XMLNode* node) override {
        XMLUtils::checkNode(node, "Security");

        curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
        curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
        quotes_.push_back(XMLUtils::getChildValue(node, "SpreadQuote", true));
        quotes_.push_back(XMLUtils::getChildValue(node, "RecoveryRateQuote", true));
    }

    XMLNode* toXML(XMLDocument& doc) override {
        XMLNode* node = doc.allocNode("Security");

        XMLUtils::addChild(doc, node, "CurveId", curveID_);
        XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
        XMLUtils::addChild(doc, node, "SpreadQuotes", quotes_[0]);
        XMLUtils::addChild(doc, node, "RecoveryRateQuotes", quotes_[1]);
        return node;
    }
};
} // namespace data
} // namespace ore
