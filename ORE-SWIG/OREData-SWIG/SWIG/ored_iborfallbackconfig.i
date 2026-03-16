/*
 Copyright (C) 2018, 2020 Quaternion Risk Management Ltd
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

#ifndef ored_iborfallbackconfig_i
#define ored_iborfallbackconfig_i

%include ored_xmlutils.i

%shared_ptr(IborFallbackConfig)
class IborFallbackConfig : public XMLSerializable {
public:
    struct FallbackData;

    IborFallbackConfig();
    IborFallbackConfig(const bool enableIborFallbacks, const bool useRfrCurveInTodaysMarket,
                       const bool useRfrCurveInSimulationMarket, const std::map<std::string, FallbackData>& fallbacks);

    bool enableIborFallbacks() const;
    bool useRfrCurveInTodaysMarket() const;
    bool useRfrCurveInSimulationMarket() const;

    void addIndexFallbackRule(const string& iborIndex, const FallbackData& fallbackData);

    bool isIndexReplaced(const string& iborIndex, const QuantLib::Date& asof = QuantLib::Date::maxDate()) const;
    const FallbackData& fallbackData(const string& iborIndex) const;

    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;

    void clear();
    static IborFallbackConfig defaultConfig();
    void updateSwitchDate(QuantLib::Date targetSwitchDate, const std::string& indexName = "");
    void logSwitchDates();

};

#endif