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
using ore::data::ScriptLibraryData;
using ore::data::ScriptedTrade;
%}

%template(ScriptedTradeNewScheduleDataVector)
std::vector<ext::shared_ptr<ScriptedTradeScriptData::NewScheduleData>>;
%template(ScriptedTradeCalibrationDataVector)
std::vector<ext::shared_ptr<ScriptedTradeScriptData::CalibrationData>>;

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

%feature("flatnested") ScriptedTradeScriptData;
%rename(ScriptedTradeNewScheduleData) ScriptedTradeScriptData::NewScheduleData;
%rename(ScriptedTradeCalibrationData) ScriptedTradeScriptData::CalibrationData;
%shared_ptr(ScriptedTradeScriptData)
%shared_ptr(ScriptedTradeScriptData::NewScheduleData)
%shared_ptr(ScriptedTradeScriptData::CalibrationData)
class ScriptedTradeScriptData : public XMLSerializable {
public:
    class NewScheduleData : public XMLSerializable {
    public:
        NewScheduleData();
        NewScheduleData(const std::string& name, const std::string& operation,
                        const std::vector<std::string>& sourceSchedules);
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
        const std::string& name() const;
        const std::string& operation() const;
        const std::vector<std::string>& sourceSchedules() const;
    };

    class CalibrationData : public XMLSerializable {
    public:
        CalibrationData();
        CalibrationData(const std::string& index, const std::vector<std::string>& strikes);
        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;
        const std::string& index() const;
        const std::vector<std::string>& strikes() const;
    };

    ScriptedTradeScriptData();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
};

%shared_ptr(ScriptLibraryData)
class ScriptLibraryData : public XMLSerializable {
public:
    ScriptLibraryData();
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    bool has(const std::string& scriptName, const std::string& purpose,
             const bool fallBackOnEmptyPurpose = true) const;
};
%extend ScriptLibraryData {
    std::string productTag(const std::string& scriptName, const std::string& purpose,
                           const bool fallBackOnEmptyPurpose = true) const {
        return self->get(scriptName, purpose, fallBackOnEmptyPurpose).first;
    }

    ScriptedTradeScriptData scriptData(const std::string& scriptName, const std::string& purpose,
                                       const bool fallBackOnEmptyPurpose = true) const {
        return self->get(scriptName, purpose, fallBackOnEmptyPurpose).second;
    }
}

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
