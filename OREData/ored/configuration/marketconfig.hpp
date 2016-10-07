/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 All rights reserved.

 This file is part of OpenRiskEngine, a free-software/open-source library
 for transparent pricing and risk analysis - http://openriskengine.org

 OpenRiskEngine is free software: you can redistribute it and/or modify it
 under the terms of the Modified BSD License.  You should have received a
 copy of the license along with this program; if not, please email
 <users@openriskengine.org>. The license is also available online at
 <http://openriskengine.org/license.shtml>.

 This program is distributed on the basis that it will form a useful
 contribution to risk analytics and model standardisation, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the license for more details.
*/

/*! \file ored/configuration/marketconfig.hpp
    \brief A class that loads market configuration from xml input.
    \ingroup configuration
*/

#pragma once

#include <vector>
#include <map>
#include <ored/utilities/xmlutils.hpp>

using std::string;
using std::map;

namespace openriskengine {
namespace data {

//! Market configuration, apply this across all market scenarios and all times
/*!
  The market configuration object determines the market composition in terms of
  - discount curve specifications by currency
  - swaption volatility structure specifications by currency
  - cap/floor volatility structure specifications by currency
  - FX spot specifications by currency pair
  - FX volatility structure specifications by currency pair
  - Ibor Index curve specifications by index name

  The 'curve specifications' are unique string representations of CurveSpec objects.

  \ingroup configuration
*/
class MarketConfiguration : public XMLSerializable {
public:
    //! \name Constructors
    //@{
    //! Default constructor
    MarketConfiguration() {}
    //@}

    //! \name Inspectors
    //@{
    string baseCurrency() { return baseCurrency_; }
    //@}

    //! \name Setters/Getters
    //@{
    //! Return discount curve specification (as string) by currency
    map<string, string>& discountCurveSpecs() { return discountCurveSpecs_; }
    //! Return yield curve specification (as string) by currency
    map<string, string>& yieldCurveSpecs() { return yieldCurveSpecs_; }
    //! Return swaption volatility structure specification (as string) by currency
    map<string, string>& swaptionVolSpecs() { return swaptionVolSpec_; }
    //! Return cap/floor volatility structure specification (as string) by currency
    map<string, string>& capVolSpecs() { return capVolSpec_; }
    //! Return FX spot specification (as string) by currency
    map<string, string>& fxSpecs() { return fxSpec_; }
    //! Return FX volatility structure specification (as string) by currency
    map<string, string>& fxVolSpecs() { return fxVolSpec_; }
    //! Return Ibor Index curve specification (as string) by currency
    map<string, string>& iborIndexSpecs() { return iborIndexSpecs_; }
    //@}

    //! \name Other
    //@{
    //! clear all market configutation data
    void clear();
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node);
    virtual XMLNode* toXML(XMLDocument& doc);
    //@}
private:
    string baseCurrency_; // = pricing or domestic currency
    map<string, string> discountCurveSpecs_;
    map<string, string> yieldCurveSpecs_;
    map<string, string> swaptionVolSpec_;
    map<string, string> capVolSpec_;
    map<string, string> fxSpec_;
    map<string, string> fxVolSpec_;
    map<string, string> iborIndexSpecs_;
};
}
}
