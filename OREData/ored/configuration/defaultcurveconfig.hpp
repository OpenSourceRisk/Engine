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

/*! \file ored/configuration/defaultcurveconfig.hpp
    \brief Default curve configuration classes
    \ingroup configuration
*/

#pragma once

#include <boost/optional.hpp>
#include <ored/configuration/bootstrapconfig.hpp>
#include <ored/configuration/curveconfig.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/date.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/period.hpp>
#include <ql/types.hpp>

namespace ore {
namespace data {
using ore::data::XMLNode;
using QuantLib::BusinessDayConvention;
using QuantLib::Calendar;
using QuantLib::DayCounter;
using QuantLib::Period;
using std::string;
using std::vector;

//! Default curve configuration
/*!
  \ingroup configuration
*/
class DefaultCurveConfig : public CurveConfig {
public:
    class Config : public XMLSerializable {
    public:
        //! Supported default curve types
        enum class Type { SpreadCDS, HazardRate, Benchmark, Price, MultiSection, TransitionMatrix, Null };
        Config(const Type& type, const string& discountCurveID, const string& recoveryRateQuote,
               const DayCounter& dayCounter, const string& conventionID,
               const std::vector<std::pair<std::string, bool>>& cdsQuotes = {}, bool extrapolation = true,
               const string& benchmarkCurveID = "", const string& sourceCurveID = "",
               const std::vector<string>& pillars = std::vector<string>(), const Calendar& calendar = Calendar(),
               const Size spotLag = 0, const QuantLib::Date& startDate = QuantLib::Date(),
               const BootstrapConfig& bootstrapConfig = BootstrapConfig(),
               QuantLib::Real runningSpread = QuantLib::Null<Real>(),
               const QuantLib::Period& indexTerm = 0 * QuantLib::Days,
               const boost::optional<bool>& implyDefaultFromMarket = boost::none, const bool allowNegativeRates = false,
               const int priority = 0);
        Config()
            : extrapolation_(true), spotLag_(0), runningSpread_(QuantLib::Null<Real>()), indexTerm_(0 * QuantLib::Days),
              allowNegativeRates_(false) {}

        void fromXML(XMLNode* node) override;
        XMLNode* toXML(XMLDocument& doc) const override;

        //! \name Inspectors
        //@{
        const int priority() const { return priority_; }
        const Type& type() const { return type_; }
        const string& discountCurveID() const { return discountCurveID_; }
        const string& benchmarkCurveID() const { return benchmarkCurveID_; }
        const string& sourceCurveID() const { return sourceCurveID_; }
        const string& recoveryRateQuote() const { return recoveryRateQuote_; }
        const string& conventionID() const { return conventionID_; }
        const DayCounter& dayCounter() const { return dayCounter_; }
        const std::vector<string>& pillars() const { return pillars_; }
        const Calendar& calendar() const { return calendar_; }
        const Size& spotLag() const { return spotLag_; }
        bool extrapolation() const { return extrapolation_; }
        const std::vector<std::pair<std::string, bool>>& cdsQuotes() const { return cdsQuotes_; }
        const QuantLib::Date& startDate() const { return startDate_; }
        const BootstrapConfig& bootstrapConfig() const { return bootstrapConfig_; }
        const Real runningSpread() const { return runningSpread_; }
        const QuantLib::Period& indexTerm() const { return indexTerm_; }
        const boost::optional<bool>& implyDefaultFromMarket() const { return implyDefaultFromMarket_; }
        const vector<string>& multiSectionSourceCurveIds() const { return multiSectionSourceCurveIds_; }
        const vector<string>& multiSectionSwitchDates() const { return multiSectionSwitchDates_; }
        const bool allowNegativeRates() const { return allowNegativeRates_; }
        const string& initialState() const { return initialState_; }
        const vector<string>& states() const { return states_; }
       //@}

        //! \name Setters
        //@{
        int& priority() { return priority_; }
        Type& type() { return type_; }
        string& discountCurveID() { return discountCurveID_; }
        string& benchmarkCurveID() { return benchmarkCurveID_; }
        string& sourceCurveID() { return sourceCurveID_; }
        string& recoveryRateQuote() { return recoveryRateQuote_; }
        string& conventionID() { return conventionID_; }
        DayCounter& dayCounter() { return dayCounter_; }
        std::vector<string> pillars() { return pillars_; }
        Calendar calendar() { return calendar_; }
        Size spotLag() { return spotLag_; }
        bool& extrapolation() { return extrapolation_; }
        QuantLib::Date& startDate() { return startDate_; }
        void setBootstrapConfig(const BootstrapConfig& bootstrapConfig) { bootstrapConfig_ = bootstrapConfig; }
        Real& runningSpread() { return runningSpread_; }
        QuantLib::Period& indexTerm() { return indexTerm_; }
        boost::optional<bool>& implyDefaultFromMarket() { return implyDefaultFromMarket_; }
        bool& allowNegativeRates() { return allowNegativeRates_; }
        //@}

    private:
        //! Quote and optional flag pair
        std::vector<std::pair<std::string, bool>> cdsQuotes_;
        Type type_;
        string discountCurveID_;
        string recoveryRateQuote_;
        DayCounter dayCounter_;
        string conventionID_;
        bool extrapolation_;
        string benchmarkCurveID_;
        string sourceCurveID_;
        vector<string> pillars_;
        Calendar calendar_;
        Size spotLag_;
        QuantLib::Date startDate_;
        BootstrapConfig bootstrapConfig_;
        Real runningSpread_;
        QuantLib::Period indexTerm_;
        vector<string> multiSectionSourceCurveIds_;
        vector<string> multiSectionSwitchDates_;
        string initialState_;
        vector<string> states_;

        /*! Indicates if the reference entity's default status should be implied from the market data. If \c true, this
            behaviour is active and if \c false it is not. If not explicitly set, it is assumed to be \c false.

            When a default credit event has been determined for an entity, certain market data providers continue to
            supply a recovery rate from the credit event determination date up to the credit event auction settlement
            date. In this period, no CDS spreads or upfront prices are provided.

            When this flag is \c true, we assume an entity is in default if we find a recovery rate in the market but no
            CDS spreads or upfront prices. In this case, we build a survival probability curve with a value of 0.0
            tomorrow. This will give some approximation to the correct price for CDS and index CDS in these cases.

            When this flag is \c false, we make no such assumption and the default curve building fails.
        */
        boost::optional<bool> implyDefaultFromMarket_;

        /*! If this flag is set to true, the checks for negative hazard rates are disabled when building a curve of type
            - HazardRate
            - Benchmark
        */
        bool allowNegativeRates_;

        int priority_ = 0;
    };

    //! the curve builder will try to build the configs by ascending key in the map, first success wins
    DefaultCurveConfig(const string& curveId, const string& curveDescription, const string& currency,
                       const std::map<int, Config>& configs);
    //! single config ctor
    DefaultCurveConfig(const string& curveID, const string& curveDescription, const string& currency,
                       const Config& config)
        : DefaultCurveConfig(curveID, curveDescription, currency, {{0, config}}) {}
    //! default ctor
    DefaultCurveConfig() {}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
    //@}

    const string& currency() const { return currency_; }
    const std::map<int, Config>& configs() const { return configs_; }

private:
    void populateQuotes();
    void populateRequiredCurveIds();
    void populateRequiredCurveIds(const std::string& discountCurveID, const std::string& benchmarkCurveID,
                                  const std::string& sourceCurveID,
                                  const std::vector<std::string>& multiSectionSourceCurveIds);
    std::string currency_;
    std::map<int, Config> configs_;
};

} // namespace data
} // namespace ore
