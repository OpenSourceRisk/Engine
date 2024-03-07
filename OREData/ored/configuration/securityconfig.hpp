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

namespace ore {
namespace data {
using ore::data::XMLNode;
using std::string;
using std::vector;

//! Security configuration
/*!
  \ingroup configuration
*/
class SecurityConfig : public CurveConfig {
public:
    //! \name Constructors/Destructors
    //@{
    //! Detailed constructor
    SecurityConfig(const string& curveID, const string& curveDescription, const string& spreadQuote = "",
                   const string& recoveryQuote = "", const string& cprQuote = "", const string& priceQuote = "")
        : CurveConfig(curveID, curveDescription), spreadQuote_(spreadQuote), recoveryQuote_(recoveryQuote),
          cprQuote_(cprQuote), priceQuote_(priceQuote) {
        setQuotes();
    };
    //! Default constructor
    SecurityConfig() {}
    //@}

    //! \name Inspectors
    //@{
    const string& spreadQuote() { return spreadQuote_; }
    const string& recoveryRatesQuote() { return recoveryQuote_; }
    const string& cprQuote() { return cprQuote_; }
    const string& priceQuote() { return priceQuote_; }
    //@}

    void fromXML(XMLNode* node) override {
        XMLUtils::checkNode(node, "Security");

        curveID_ = XMLUtils::getChildValue(node, "CurveId", true);
        curveDescription_ = XMLUtils::getChildValue(node, "CurveDescription", true);
        spreadQuote_ = XMLUtils::getChildValue(node, "SpreadQuote", false);
        recoveryQuote_ = XMLUtils::getChildValue(node, "RecoveryRateQuote", false);
        cprQuote_ = XMLUtils::getChildValue(node, "CPRQuote", false);
        priceQuote_ = XMLUtils::getChildValue(node, "PriceQuote", false);
        setQuotes();
    }

    XMLNode* toXML(XMLDocument& doc) const override {
        XMLNode* node = doc.allocNode("Security");

        XMLUtils::addChild(doc, node, "CurveId", curveID_);
        XMLUtils::addChild(doc, node, "CurveDescription", curveDescription_);
        if (!spreadQuote_.empty())
            XMLUtils::addChild(doc, node, "SpreadQuote", spreadQuote_);
        if (!recoveryQuote_.empty())
            XMLUtils::addChild(doc, node, "RecoveryRateQuote", recoveryQuote_);
        if (!cprQuote_.empty())
            XMLUtils::addChild(doc, node, "CPRQuote", cprQuote_);
        if (!priceQuote_.empty())
            XMLUtils::addChild(doc, node, "PriceQuote", priceQuote_);
        return node;
    }

private:
    void setQuotes() {
        quotes_.clear();
        if (!spreadQuote_.empty())
            quotes_.push_back(spreadQuote_);
        if (!recoveryQuote_.empty())
            quotes_.push_back(recoveryQuote_);
        if (!cprQuote_.empty())
            quotes_.push_back(cprQuote_);
        if (!priceQuote_.empty())
            quotes_.push_back(priceQuote_);
    }
    string spreadQuote_, recoveryQuote_, cprQuote_, priceQuote_;
};
} // namespace data
} // namespace ore
