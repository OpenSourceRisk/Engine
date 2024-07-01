/*
 Copyright (C) 2019 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/scriptedtrade.hpp
    \brief scripted trade data model
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/schedule.hpp>
#include <ored/portfolio/trade.hpp>
#include <ored/portfolio/tradefactory.hpp>

namespace ore {
namespace data {

class ScriptedTradeEventData : public XMLSerializable {
public:
    enum class Type { Value, Array, Derived };
    // default ctor
    ScriptedTradeEventData() : type_(Type::Value) {}
    // single value cor
    ScriptedTradeEventData(const std::string& name, const std::string& date)
        : type_(Type::Value), name_(name), value_(date) {}
    // array ctor
    ScriptedTradeEventData(const std::string& name, const ScheduleData& schedule)
        : type_(Type::Array), name_(name), schedule_(schedule) {}
    // derived schedule ctor
    ScriptedTradeEventData(const std::string& name, const std::string& baseSchedule, const std::string& shift,
                           const std::string& calendar, const std::string& convention)
        : type_(Type::Derived), name_(name), baseSchedule_(baseSchedule), shift_(shift), calendar_(calendar),
          convention_(convention) {}

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    Type type() const { return type_; }
    const std::string& name() const { return name_; }
    const std::string& value() const { return value_; }
    const ScheduleData& schedule() const { return schedule_; }
    const std::string& baseSchedule() const { return baseSchedule_; }
    const std::string& shift() const { return shift_; }
    const std::string& calendar() const { return calendar_; }
    const std::string& convention() const { return convention_; }
    const bool hasData();

private:
    Type type_;
    std::string name_;
    std::string value_;        // single value
    ScheduleData schedule_;    // schedule data
    std::string baseSchedule_; // derived schedule
    std::string shift_;
    std::string calendar_;
    std::string convention_;
};

class ScriptedTradeValueTypeData : public XMLSerializable {
public:
    // node name ctor
    explicit ScriptedTradeValueTypeData(const std::string& nodeName) : nodeName_(nodeName), isArray_(false) {}
    // single value cor
    ScriptedTradeValueTypeData(const std::string& nodeName, const std::string& name, const std::string& value)
        : nodeName_(nodeName), isArray_(false), name_(name), value_(value) {}
    // array ctor
    ScriptedTradeValueTypeData(const std::string& nodeName, const std::string& name,
                               const std::vector<std::string>& values)
        : nodeName_(nodeName), isArray_(true), name_(name), values_(values) {}

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    bool isArray() const { return isArray_; }
    const std::string& name() const { return name_; }
    const std::string& value() const { return value_; }
    const std::vector<std::string>& values() const { return values_; }

private:
    std::string nodeName_;
    bool isArray_;
    std::string name_;
    std::string value_;
    std::vector<std::string> values_;
};

class ScriptedTradeScriptData : public XMLSerializable {
public:
    class NewScheduleData : public XMLSerializable {
    public:
        NewScheduleData() {}
        NewScheduleData(const std::string& name, const std::string& operation,
                        const std::vector<std::string>& sourceSchedules)
            : name_(name), operation_(operation), sourceSchedules_(sourceSchedules) {}
        virtual void fromXML(XMLNode* node) override;
        virtual XMLNode* toXML(ore::data::XMLDocument& doc) const override;
        const std::string& name() const { return name_; }
        const std::string& operation() const { return operation_; }
        const std::vector<std::string>& sourceSchedules() const { return sourceSchedules_; }

    private:
        std::string name_;
        std::string operation_;
        std::vector<std::string> sourceSchedules_;
    };

    class CalibrationData : public XMLSerializable {
    public:
        CalibrationData() {}
        CalibrationData(const std::string& index, const std::vector<std::string>& strikes)
            : index_(index), strikes_(strikes) {}
        virtual void fromXML(XMLNode* node) override;
        virtual XMLNode* toXML(ore::data::XMLDocument& doc) const override;
        const std::string& index() const { return index_; }
        const std::vector<string>& strikes() const { return strikes_; }

    private:
        std::string index_;
        std::vector<std::string> strikes_;
    };

    // default ctor
    ScriptedTradeScriptData() {}
    // ctor taking data
    ScriptedTradeScriptData(const std::string& code, const std::string& npv,
                            const std::vector<std::pair<std::string, std::string>>& results,
                            const std::vector<std::string>& schedulesEligibleForCoarsening,
                            const std::vector<NewScheduleData>& newSchedules = {},
                            const std::vector<CalibrationData>& calibrationSpec = {},
                            const std::vector<std::string>& stickyCloseOutStates = {},
                            const std::vector<std::string>& conditionalExpectationModelStates = {})
        : code_(code), npv_(npv), results_(results), schedulesEligibleForCoarsening_(schedulesEligibleForCoarsening),
          newSchedules_(newSchedules), calibrationSpec_(calibrationSpec), stickyCloseOutStates_(stickyCloseOutStates),
          conditionalExpectationModelStates_(conditionalExpectationModelStates) {
        formatCode();
    }

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    const std::string& code() const { return code_; }
    const std::string& npv() const { return npv_; }
    // a result is given by a name and a context variable, e.g. ("notionalCurrency", "PayCcy")
    const std::vector<std::pair<std::string, std::string>>& results() const { return results_; }
    const std::vector<std::string>& schedulesEligibleForCoarsening() const { return schedulesEligibleForCoarsening_; }
    const std::vector<NewScheduleData>& newSchedules() const { return newSchedules_; }
    const std::vector<CalibrationData>& calibrationSpec() const { return calibrationSpec_; }
    const std::vector<std::string>& stickyCloseOutStates() const { return stickyCloseOutStates_; }
    const std::vector<std::string>& conditionalExpectationModelStates() const {
        return conditionalExpectationModelStates_;
    }

private:
    void formatCode();
    std::string code_;
    std::string npv_;
    std::vector<std::pair<std::string, std::string>> results_;
    std::vector<std::string> schedulesEligibleForCoarsening_;
    std::vector<NewScheduleData> newSchedules_;
    std::vector<CalibrationData> calibrationSpec_;
    std::vector<std::string> stickyCloseOutStates_;
    std::vector<std::string> conditionalExpectationModelStates_;
};

class ScriptLibraryData : public XMLSerializable {
public:
    ScriptLibraryData() {}
    ScriptLibraryData(const ScriptLibraryData& d);
    ScriptLibraryData(ScriptLibraryData&& d);
    ScriptLibraryData& operator=(const ScriptLibraryData& d);
    ScriptLibraryData& operator=(ScriptLibraryData&& d);

    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    bool has(const std::string& scriptName, const std::string& purpose, const bool fallBackOnEmptyPurpose = true) const;

    // get (product tag, script data)
    std::pair<std::string, ScriptedTradeScriptData> get(const std::string& scriptName, const std::string& purpose,
                                                        const bool fallBackOnEmptyPurpose = true) const;

private:
    // scriptName => ( productTag, purpose => script )
    std::map<std::string, std::pair<std::string, std::map<std::string, ScriptedTradeScriptData>>> scripts_;
};

class ScriptedTrade : public Trade {
public:
    // ctor taking a trade type and an envelope
    ScriptedTrade(const std::string& tradeType = "ScriptedTrade", const Envelope& env = Envelope())
        : Trade(tradeType, env) {}
    // ctor taking data and an explicit script + product tag
    ScriptedTrade(const Envelope& env, const std::vector<ScriptedTradeEventData>& events,
                  const std::vector<ScriptedTradeValueTypeData>& numbers,
                  const std::vector<ScriptedTradeValueTypeData>& indices,
                  const std::vector<ScriptedTradeValueTypeData>& currencies,
                  const std::vector<ScriptedTradeValueTypeData>& daycounters,
                  const std::map<std::string, ScriptedTradeScriptData>& script, const std::string& productTag,
                  const std::string& tradeType = "ScriptedTrade")
        : Trade(tradeType, env), events_(events), numbers_(numbers), indices_(indices), currencies_(currencies),
          daycounters_(daycounters), script_(script), productTag_(productTag) {}
    // ctor taking data and a reference to a script in the script library
    ScriptedTrade(const Envelope& env, const std::vector<ScriptedTradeEventData>& events,
                  const std::vector<ScriptedTradeValueTypeData>& numbers,
                  const std::vector<ScriptedTradeValueTypeData>& indices,
                  const std::vector<ScriptedTradeValueTypeData>& currencies,
                  const std::vector<ScriptedTradeValueTypeData>& daycounters, const std::string& scriptName,
                  const std::string& tradeType = "ScriptedTrade")
        : Trade(tradeType, env), events_(events), numbers_(numbers), indices_(indices), currencies_(currencies),
          daycounters_(daycounters), scriptName_(scriptName) {}

    // clear data members specific to ScriptedTrade, e.g. called in fromXML()
    void clear();

    // Trade interface
    void build(const QuantLib::ext::shared_ptr<EngineFactory>&) override;
    QuantLib::Real notional() const override;
    std::string notionalCurrency() const override;
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(ore::data::XMLDocument& doc) const override;

    // build and incorporate provided premium data
    void build(const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory, const PremiumData& premiumData,
               const Real premiumMultiplier);

    // underlying asset names
    std::map<ore::data::AssetClass, std::set<std::string>>
    underlyingIndices(const QuantLib::ext::shared_ptr<ReferenceDataManager>& referenceDataManager = nullptr) const override;

    // Add ISDA taxonomy classification to additional data
    virtual void setIsdaTaxonomyFields();

    // Inspectors
    const std::vector<ScriptedTradeEventData>& events() const { return events_; }
    const std::vector<ScriptedTradeValueTypeData>& numbers() const { return numbers_; }
    const std::vector<ScriptedTradeValueTypeData>& indices() const { return indices_; }
    const std::vector<ScriptedTradeValueTypeData>& currencies() const { return currencies_; }
    const std::vector<ScriptedTradeValueTypeData>& daycounters() const { return daycounters_; }
    const std::map<std::string, ScriptedTradeScriptData>& script() const { return script_; }
    const std::string& productTag() const { return productTag_; }
    const std::string& scriptName() const { return scriptName_; }
    const std::string& simmProductClass() const { return simmProductClass_; }
    const std::string& scheduleProductClass() const { return scheduleProductClass_; }

    // get script for purpose, possibly fall back on script with empty purpose
    const ScriptedTradeScriptData& script(const std::string& purpose, const bool fallBackOnEmptyPurpose = true) const;

protected:
    // data
    std::vector<ScriptedTradeEventData> events_;
    std::vector<ScriptedTradeValueTypeData> numbers_;
    std::vector<ScriptedTradeValueTypeData> indices_;
    std::vector<ScriptedTradeValueTypeData> currencies_;
    std::vector<ScriptedTradeValueTypeData> daycounters_;
    // either we have a script + product tag ...
    std::map<std::string, ScriptedTradeScriptData> script_;
    std::string productTag_;
    // .. or a script name referencing a script in the library
    std::string scriptName_;

    // set in build()
    std::string simmProductClass_;
    std::string scheduleProductClass_;
};

class ScriptLibraryStorage : public QuantLib::Singleton<ScriptLibraryStorage, std::integral_constant<bool, true>> {
    ScriptLibraryData data_;
    mutable boost::shared_mutex mutex_;

public:
    const ScriptLibraryData& get() const;
    void set(const ScriptLibraryData& data);
    void set(ScriptLibraryData&& data);
    void clear();
};

} // namespace data
} // namespace ore
