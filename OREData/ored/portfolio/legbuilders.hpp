/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file ored/portfolio/legbuilders.hpp
    \brief Leg Builders
    \ingroup portfolio
*/

#pragma once

#include <ored/portfolio/enginefactory.hpp>
#include <ored/portfolio/fixingdates.hpp>

#include <qle/indexes/fxindex.hpp>

namespace ore {
namespace data {

class FixedLegBuilder : public LegBuilder {
public:
    FixedLegBuilder() : LegBuilder(LegType::Fixed) {}
    Leg buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(), const bool useXbsCurves = false,
                 const bool attachPricer = true,
                 std::set<std::tuple<std::set<std::string>, std::string, std::string>>* productModelEngines =
                     nullptr) const override;
};

class ZeroCouponFixedLegBuilder : public LegBuilder {
public:
    ZeroCouponFixedLegBuilder() : LegBuilder(LegType::ZeroCouponFixed) {}
    Leg buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(), const bool useXbsCurves = false,
                 const bool attachPricer = true,
                 std::set<std::tuple<std::set<std::string>, std::string, std::string>>* productModelEngines =
                     nullptr) const override;
};

class FloatingLegBuilder : public LegBuilder {
public:
    FloatingLegBuilder() : LegBuilder(LegType::Floating) {}
    Leg buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(), const bool useXbsCurves = false,
                 const bool attachPricer = true,
                 std::set<std::tuple<std::set<std::string>, std::string, std::string>>* productModelEngines =
                     nullptr) const override;
};

class CashflowLegBuilder : public LegBuilder {
public:
    CashflowLegBuilder() : LegBuilder(LegType::Cashflow) {}
    Leg buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(), const bool useXbsCurves = false,
                 const bool attachPricer = true,
                 std::set<std::tuple<std::set<std::string>, std::string, std::string>>* productModelEngines =
                     nullptr) const override;
};

class CPILegBuilder : public LegBuilder {
public:
    CPILegBuilder() : LegBuilder(LegType::CPI) {}
    Leg buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(), const bool useXbsCurves = false,
                 const bool attachPricer = true,
                 std::set<std::tuple<std::set<std::string>, std::string, std::string>>* productModelEngines =
                     nullptr) const override;
};

class YYLegBuilder : public LegBuilder {
public:
    YYLegBuilder() : LegBuilder(LegType::YY) {}
    Leg buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(), const bool useXbsCurves = false,
                 const bool attachPricer = true,
                 std::set<std::tuple<std::set<std::string>, std::string, std::string>>* productModelEngines =
                     nullptr) const override;
};

class CMSLegBuilder : public LegBuilder {
public:
    CMSLegBuilder() : LegBuilder(LegType::CMS) {}
    Leg buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(), const bool useXbsCurves = false,
                 const bool attachPricer = true,
                 std::set<std::tuple<std::set<std::string>, std::string, std::string>>* productModelEngines =
                     nullptr) const override;
};

class CMBLegBuilder : public LegBuilder {
public:
    CMBLegBuilder() : LegBuilder(LegType::CMB) {}
    Leg buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(), const bool useXbsCurves = false,
                 const bool attachPricer = true,
                 std::set<std::tuple<std::set<std::string>, std::string, std::string>>* productModelEngines =
                     nullptr) const override;
};

class DigitalCMSLegBuilder : public LegBuilder {
public:
    DigitalCMSLegBuilder() : LegBuilder(LegType::DigitalCMS) {}
    Leg buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(), const bool useXbsCurves = false,
                 const bool attachPricer = true,
                 std::set<std::tuple<std::set<std::string>, std::string, std::string>>* productModelEngines =
                     nullptr) const override;
};

class CMSSpreadLegBuilder : public LegBuilder {
public:
    CMSSpreadLegBuilder() : LegBuilder(LegType::CMSSpread) {}
    Leg buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(), const bool useXbsCurves = false,
                 const bool attachPricer = true,
                 std::set<std::tuple<std::set<std::string>, std::string, std::string>>* productModelEngines =
                     nullptr) const override;
};

class DigitalCMSSpreadLegBuilder : public LegBuilder {
public:
    DigitalCMSSpreadLegBuilder() : LegBuilder(LegType::DigitalCMSSpread) {}
    Leg buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(), const bool useXbsCurves = false,
                 const bool attachPricer = true,
                 std::set<std::tuple<std::set<std::string>, std::string, std::string>>* productModelEngines =
                     nullptr) const override;
};

class EquityLegBuilder : public LegBuilder {
public:
    EquityLegBuilder() : LegBuilder(LegType::Equity) {}
    Leg buildLeg(const LegData& data, const QuantLib::ext::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(), const bool useXbsCurves = false,
                 const bool attachPricer = true,
                 std::set<std::tuple<std::set<std::string>, std::string, std::string>>* productModelEngines =
                     nullptr) const override;
};

} // namespace data
} // namespace ore
