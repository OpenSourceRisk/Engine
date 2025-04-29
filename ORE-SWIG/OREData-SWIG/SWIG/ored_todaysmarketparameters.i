/*
 Copyright (C) 2024 Quaternion Risk Management Ltd
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

#ifndef ored_todaysmarketparameters_i
#define ored_todaysmarketparameters_i
%include <std_pair.i>
%include <std_vector.i>
%include <std_string.i>
%include ored_xmlutils.i

%{
// put c++ declarations here
using ore::data::TodaysMarketParameters;
using ore::data::MarketConfiguration;
using ore::data::MarketObject;
using ore::data::XMLDocument;
using ore::data::XMLSerializable;
using ore::data::parseMarketObject;
%}

MarketObject parseMarketObject(const std::string& mo);

class MarketConfiguration {
public:
    MarketConfiguration(std::map<MarketObject, std::string> marketObjectIds = {});
    std::string operator()(const MarketObject o) const;
    void setId(const MarketObject o, const std::string& id);
    void add(const MarketConfiguration& o);
};


%template() std::pair<std::string, MarketConfiguration>;
%template(TodaysMarketConfigVector) std::vector<std::pair<std::string, MarketConfiguration>>;
%shared_ptr(TodaysMarketParameters)
class TodaysMarketParameters : public XMLSerializable {
  public:
    TodaysMarketParameters();

    const std::vector<std::pair<std::string, MarketConfiguration>>& configurations() const;
    bool hasConfiguration(const std::string& configuration) const;
    bool hasMarketObject(const MarketObject& o) const;

    const std::map<std::string, std::string>& mapping(const MarketObject o, const std::string& configuration) const;
    std::map<std::string, std::string>& mappingReference(const MarketObject o, const std::string& configuration);
    std::vector<std::string> curveSpecs(const std::string& configuration) const;

    std::string marketObjectId(const MarketObject o, const std::string& configuration) const;
    void clear();
    bool empty();
    void addConfiguration(const std::string& name, const MarketConfiguration& configuration);
    void addMarketObject(const MarketObject o, const std::string& id, const std::map<std::string, std::string>& assignments);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

#endif
