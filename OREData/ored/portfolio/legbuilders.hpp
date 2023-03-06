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
    ORE_REGISTER_LEG_BUILDER(FixedLegBuilder)
public:
    FixedLegBuilder() : LegBuilder("Fixed") {}
    Leg buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(),
                 const bool useXbsCurves = false) const override;
};

class ZeroCouponFixedLegBuilder : public LegBuilder {
    ORE_REGISTER_LEG_BUILDER(ZeroCouponFixedLegBuilder)
public:
    ZeroCouponFixedLegBuilder() : LegBuilder("ZeroCouponFixed") {}
    Leg buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(),
                 const bool useXbsCurves = false) const override;
};

class FloatingLegBuilder : public LegBuilder {
    ORE_REGISTER_LEG_BUILDER(FloatingLegBuilder)
public:
    FloatingLegBuilder() : LegBuilder("Floating") {}
    Leg buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(),
                 const bool useXbsCurves = false) const override;
};

class CashflowLegBuilder : public LegBuilder {
    ORE_REGISTER_LEG_BUILDER(CashflowLegBuilder)
public:
    CashflowLegBuilder() : LegBuilder("Cashflow") {}
    Leg buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(),
                 const bool useXbsCurves = false) const override;
};

class CPILegBuilder : public LegBuilder {
    ORE_REGISTER_LEG_BUILDER(CPILegBuilder)
public:
    CPILegBuilder() : LegBuilder("CPI") {}
    Leg buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(),
                 const bool useXbsCurves = false) const override;
};

class YYLegBuilder : public LegBuilder {
    ORE_REGISTER_LEG_BUILDER(YYLegBuilder)
public:
    YYLegBuilder() : LegBuilder("YY") {}
    Leg buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(),
                 const bool useXbsCurves = false) const override;
};

class CMSLegBuilder : public LegBuilder {
    ORE_REGISTER_LEG_BUILDER(CMSLegBuilder)
public:
    CMSLegBuilder() : LegBuilder("CMS") {}
    Leg buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(),
                 const bool useXbsCurves = false) const override;
};

class CMBLegBuilder : public LegBuilder {
    ORE_REGISTER_LEG_BUILDER(CMBLegBuilder)
public:
    CMBLegBuilder() : LegBuilder("CMB") {}
    Leg buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(),
                 const bool useXbsCurves = false) const override;
};

class DigitalCMSLegBuilder : public LegBuilder {
    ORE_REGISTER_LEG_BUILDER(DigitalCMSLegBuilder)
public:
    DigitalCMSLegBuilder() : LegBuilder("DigitalCMS") {}
    Leg buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(),
                 const bool useXbsCurves = false) const override;
};

class CMSSpreadLegBuilder : public LegBuilder {
    ORE_REGISTER_LEG_BUILDER(CMSSpreadLegBuilder)
public:
    CMSSpreadLegBuilder() : LegBuilder("CMSSpread") {}
    Leg buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(),
                 const bool useXbsCurves = false) const override;
};

class DigitalCMSSpreadLegBuilder : public LegBuilder {
    ORE_REGISTER_LEG_BUILDER(DigitalCMSSpreadLegBuilder)
public:
    DigitalCMSSpreadLegBuilder() : LegBuilder("DigitalCMSSpread") {}
    Leg buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(),
                 const bool useXbsCurves = false) const override;
};

class EquityLegBuilder : public LegBuilder {
    ORE_REGISTER_LEG_BUILDER(EquityLegBuilder)
public:
    EquityLegBuilder() : LegBuilder("Equity") {}
    Leg buildLeg(const LegData& data, const boost::shared_ptr<EngineFactory>& engineFactory,
                 RequiredFixings& requiredFixings, const string& configuration,
                 const QuantLib::Date& openEndDateReplacement = Null<Date>(),
                 const bool useXbsCurves = false) const override;
};

} // namespace data
} // namespace ore
