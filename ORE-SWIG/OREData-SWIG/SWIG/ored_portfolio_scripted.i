/*
 Copyright (C) 2026 Quaternion Risk Management Ltd
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

#ifndef ored_portfolio_scripted_i
#define ored_portfolio_scripted_i

%{
using ore::data::ScriptedTradeEventData;
using ore::data::ScriptedTradeValueTypeData;
using ore::data::ScriptedTradeScriptData;
using ore::data::ScriptedTrade;
%}

%shared_ptr(ScriptedTradeEventData)
class ScriptedTradeEventData : public XMLSerializable {
public:
    ScriptedTradeEventData();
    ScriptedTradeEventData(const std::string& name, const std::string& date);
    ScriptedTradeEventData(const std::string& name, const ScheduleData& schedule);
    ScriptedTradeEventData(const std::string& name, const std::string& baseSchedule, const std::string& shift,
                           const std::string& calendar, const std::string& convention);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ScriptedTradeValueTypeData)
class ScriptedTradeValueTypeData : public XMLSerializable {
public:
    explicit ScriptedTradeValueTypeData(const std::string& nodeName);
    ScriptedTradeValueTypeData(const std::string& nodeName, const std::string& name, const std::string& value);
    ScriptedTradeValueTypeData(const std::string& nodeName, const std::string& name,
                               const std::vector<std::string>& values);
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ScriptedTradeScriptData)
class ScriptedTradeScriptData : public XMLSerializable {
public:
    ScriptedTradeScriptData();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ScriptedTrade)
class ScriptedTrade : public Trade {
public:
    ScriptedTrade(const std::string& tradeType = "ScriptedTrade", const Envelope& env = Envelope());
    void build(const ext::shared_ptr<EngineFactory>&) override;
    QuantLib::Real notional() const override;
    std::string notionalCurrency() const override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

#endif
