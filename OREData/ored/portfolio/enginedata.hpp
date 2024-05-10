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

/*! \file ored/portfolio/enginedata.hpp
    \brief A class to hold pricing engine parameters
    \ingroup tradedata
*/

#pragma once

#include <ored/utilities/parsers.hpp>
#include <ored/utilities/xmlutils.hpp>
#include <qle/termstructures/dynamicstype.hpp>

namespace ore {
namespace data {
using ore::data::XMLNode;
using ore::data::XMLSerializable;
using ore::data::XMLUtils;
using std::string;
using std::vector;

//! Pricing engine description
/*! \ingroup tradedata
 */
class EngineData : public XMLSerializable {
public:
    //! Default constructor
    EngineData() {}

    //! \name Inspectors
    //@{
    bool hasProduct(const string& productName);
    const string& model(const string& productName) const { return model_.at(productName); }
    const map<string, string>& modelParameters(const string& productName) const { return modelParams_.at(productName); }
    const string& engine(const string& productName) const { return engine_.at(productName); }
    const map<string, string>& engineParameters(const string& productName) const {
        return engineParams_.at(productName);
    }
    const std::map<std::string, std::string>& globalParameters() const { return globalParams_; }

    //! Return all products
    vector<string> products() const;
    //@}

    //! \name Setters
    //@{
    string& model(const string& productName) { return model_[productName]; }
    map<string, string>& modelParameters(const string& productName) { return modelParams_[productName]; }
    string& engine(const string& productName) { return engine_[productName]; }
    map<string, string>& engineParameters(const string& productName) { return engineParams_[productName]; }
    std::map<std::string, std::string>& globalParameters() { return globalParams_; }
    //@}

    //! Clear all data
    void clear();

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    map<string, string> model_;
    map<string, map<string, string>> modelParams_;
    map<string, string> engine_;
    map<string, map<string, string>> engineParams_;
    std::map<std::string, std::string> globalParams_;
};

bool operator==(const EngineData& lhs, const EngineData& rhs);
bool operator!=(const EngineData& lhs, const EngineData& rhs);

} // namespace data
} // namespace ore
