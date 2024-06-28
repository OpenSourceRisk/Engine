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
#include <unordered_map>

namespace ore {
namespace data {
using ore::data::XMLNode;
using ore::data::XMLSerializable;
using ore::data::XMLUtils;
using std::pair;
using std::string;
using std::vector;

//! Market Configuration structure
/*!
  The Market Configuration bundles configurations for each of the market objects
  and assigns a configuration ID.

  Several Market Configurations can be specified and held in a market object in parallel.
  Applications then need to specify the desired market configuration ID when making calls
  to any of the term structures provided by the market object.

  \ingroup marketdata
 */

std::ostream& operator<<(std::ostream& out, const MarketObject& o);

std::set<MarketObject> getMarketObjectTypes();

class MarketConfiguration {
public:
    MarketConfiguration(map<MarketObject, string> marketObjectIds = {});
    string operator()(const MarketObject o) const;
    void setId(const MarketObject o, const string& id);
    void add(const MarketConfiguration& o);

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
    const vector<pair<string, MarketConfiguration>>& configurations() const;
    bool hasConfiguration(const string& configuration) const;
    bool hasMarketObject(const MarketObject& o) const;

    //! EUR => Yield/EUR/EUR6M, USD => Yield/USD/USD3M etc.
    const map<string, string>& mapping(const MarketObject o, const string& configuration) const;

    //! return a mapping reference for modification
    map<string, string>& mappingReference(const MarketObject o, const string& configuration);

    //! Build a vector of all the curve specs (may contain duplicates)
    vector<string> curveSpecs(const string& configuration) const;

    //! Intermediate id for a given market object and configuration, see the description of configurations_ below
    string marketObjectId(const MarketObject o, const string& configuration) const;
    //@}

    //! Clear the contents
    void clear();

    //! Check if any parameters
    bool empty();

    //! \name Setters
    //@{
    void addConfiguration(const string& name, const MarketConfiguration& configuration);
    void addMarketObject(const MarketObject o, const string& id, const map<string, string>& assignments);
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

private:
    /* Maps a configuration label to a MarketConfiguration instance. For the command line application, the configuration
       label is specified under the Markets node in ore.xml. An example is given by:

       default          => (DiscountCurve => xois_eur)
       collateral_inccy => (DiscountCurve => ois)
       collateral_eur   => (DiscountCurve => xois_eur)
       ...

       The RHS maps each market object to a configuration id, which is an intermediate identifier used to group
       assignments for each market object together, see the description of marketObjects_ below. Missing market objects
       are mapped to the market default configuration "default". The latter is defined as a constant in
       Market::defaultConfiguration.

       A configuration label "default" is always added, which maps all market objects to the market default
       configuration "default". The entries for the configuration label "default" can be overwritten though. */
    vector<pair<string, MarketConfiguration>> configurations_;

    /* For each market object type, maps the intermediate configuration id to a list of assignments, e.g.

       DiscountCurve => (xois_eur => (EUR => Yield/EUR/EUR1D,
                                      USD => Yield/USD/USD-IN_EUR)
                         ois      => (EUR => Yield/EUR/EUR1D,
                                      USD => Yield/USD/USD1D))
       IndexCurve    => ...

       For each pair (market object, intermediate configuration id) defined in the rhs of the configurations_ map, a
       mapping should be defined in marketObjects_). */
    map<MarketObject, map<string, map<string, string>>> marketObjects_;

    void curveSpecs(const map<string, map<string, string>>&, const string&, vector<string>&) const;
};

// inline

inline const vector<pair<string, MarketConfiguration>>& TodaysMarketParameters::configurations() const {
    return configurations_;
}

inline bool TodaysMarketParameters::hasConfiguration(const string& configuration) const {
    auto it = find_if(configurations_.begin(), configurations_.end(),
                      [&configuration](const pair<string, MarketConfiguration>& s) { return s.first == configuration; });
    return it != configurations_.end();
}

inline bool TodaysMarketParameters::hasMarketObject(const MarketObject& o) const {
    auto it = marketObjects_.find(o);
    return it != marketObjects_.end();
}

inline string TodaysMarketParameters::marketObjectId(const MarketObject o, const string& configuration) const {
    QL_REQUIRE(hasConfiguration(configuration), "configuration " << configuration << " not found");
    auto it = find_if(configurations_.begin(), configurations_.end(),
                      [&configuration](const pair<string, MarketConfiguration>& s) { return s.first == configuration; });
    return it->second(o);
}

} // namespace data
} // namespace ore
