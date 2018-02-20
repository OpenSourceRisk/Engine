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

/*! \file ored/marketdata/todaysmarketparameters.hpp
    \brief A class to hold todays market configuration(s)
    \ingroup marketdata
*/

#pragma once

#include <ored/marketdata/market.hpp>
#include <ored/utilities/log.hpp>
#include <ored/utilities/parsers.hpp>
#include <ored/utilities/xmlutils.hpp>

using std::vector;
using std::string;
using std::pair;
using ore::data::XMLSerializable;
using ore::data::XMLDocument;
using ore::data::XMLNode;
using ore::data::XMLUtils;

namespace ore {
namespace data {

//! Market Configuration structure
/*!
  The Market Configuration bundles configurations for each of the market objects
  and assigns a configuration ID.

  Several Market Configurations can be specified and held in a market object in parallel.
  Applications then need to specify the desired market configuration ID when making calls
  to any of the term structures provided by the market object.

  \ingroup marketdata
 */

//! elements must be numbered 0...n, so we can iterate over them
enum class MarketObject {
    DiscountCurve = 0,
    YieldCurve = 1,
    IndexCurve = 2,
    SwapIndexCurve = 3,
    FXSpot = 4,
    FXVol = 5,
    SwaptionVol = 6,
    DefaultCurve = 7,
    CDSVol = 8,
    BaseCorrelation = 9,
    CapFloorVol = 10,
    ZeroInflationCurve = 11,
    YoYInflationCurve = 12,
    InflationCapFloorPriceSurface = 13,
    EquityCurve = 14,
    EquityVol = 15,
    Security = 16,
    CommodityCurve = 17
};

std::ostream& operator<<(std::ostream& out, const MarketObject& o);

class MarketConfiguration {
public:
    MarketConfiguration();
    string operator()(const MarketObject o) const {
        QL_REQUIRE(marketObjectIds_.find(o) != marketObjectIds_.end(),
                   "MarketConfiguration: did not find MarketObject " << o << " (this is unexpected)");
        return marketObjectIds_.at(o);
    }
    void setId(const MarketObject o, const string& id) {
        if (id != "")
            marketObjectIds_[o] = id;
    }

private:
    map<MarketObject, string> marketObjectIds_;
};

//! Today's Market Parameters
/*!
  This class is a container of instructions (all text) for how to build
  a market object.

  An instance of this object is needed in order to call a TodaysMarket
  constructor.

  \ingroup curves
 */
class TodaysMarketParameters : public XMLSerializable {
public:
    //! Default constructor
    TodaysMarketParameters() {}

    //! \name Inspectors
    //@{
    const map<string, MarketConfiguration>& configurations() const;
    bool hasConfiguration(const string& configuration) const;
    bool hasMarketObject(const MarketObject& o) const;

    //! EUR => Yield/EUR/EUR6M, USD => Yield/USD/USD3M etc.
    const map<string, string>& mapping(const MarketObject o, const string& configuration) const;

    //! Build a vector of all the curve specs (may contain duplicates)
    vector<string> curveSpecs(const string& configuration) const;

    //! Individual term structure ids for a given configuration
    string marketObjectId(const MarketObject o, const string& configuration) const;
    //@}

    //! \name Setters
    //@{
    void addConfiguration(const string& name, const MarketConfiguration& configuration);
    void addMarketObject(const MarketObject o, const string& id, const map<string, string>& assignments);
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node);
    XMLNode* toXML(XMLDocument& doc);
    //@}

private:
    // maps configuration name to id list
    map<string, MarketConfiguration> configurations_;
    // maps id to map (key,value) for each market object type
    map<MarketObject, map<string, map<string, string>>> marketObjects_;

    void curveSpecs(const map<string, map<string, string>>&, const string&, vector<string>&) const;
};

// inline

inline const map<string, MarketConfiguration>& TodaysMarketParameters::configurations() const {
    return configurations_;
}

inline bool TodaysMarketParameters::hasConfiguration(const string& configuration) const {
    auto it = configurations_.find(configuration);
    return it != configurations_.end();
}

inline bool TodaysMarketParameters::hasMarketObject(const MarketObject& o) const {
    auto it = marketObjects_.find(o);
    return it != marketObjects_.end();
}

inline string TodaysMarketParameters::marketObjectId(const MarketObject o, const string& configuration) const {
    QL_REQUIRE(hasConfiguration(configuration), "configuration " << configuration << " not found");
    return configurations_.at(configuration)(o);
}

inline const map<string, string>& TodaysMarketParameters::mapping(const MarketObject o,
                                                                  const string& configuration) const {
    QL_REQUIRE(hasConfiguration(configuration), "configuration " << configuration << " not found");
    auto it = marketObjects_.find(o);
    if (it != marketObjects_.end()) {
        auto it2 = it->second.find(marketObjectId(o, configuration));
        if (it2 != it->second.end()) {
            return it2->second;
        }
    }
    QL_FAIL("market object of type " << o << " with id " << marketObjectId(o, configuration)
                                     << " specified in configuration " << configuration << " not found");
}

inline void TodaysMarketParameters::addConfiguration(const string& id, const MarketConfiguration& configuration) {
    configurations_[id] = configuration;
}

inline void TodaysMarketParameters::addMarketObject(const MarketObject o, const string& id,
                                                    const map<string, string>& assignments) {
    marketObjects_[o][id] = assignments;
    for (auto s : assignments)
        DLOG("TodaysMarketParameters, add market objects of type " << o << ": " << id << " " << s.first << " "
                                                                   << s.second);
}

} // namespace data
} // namespace ore
