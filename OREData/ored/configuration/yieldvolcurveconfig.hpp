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

/*! \file ored/configuration/yieldvolcurveconfig.hpp
\brief yield volatility curve configuration classes
\ingroup configuration
*/

#pragma once

#include <ored/configuration/curveconfig.hpp>
#include <ql/time/calendar.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/period.hpp>
#include <ql/types.hpp>


namespace ore {
    namespace data {
        using std::string;
        using std::vector;
        using ore::data::XMLNode;
        using QuantLib::Period;
        using QuantLib::DayCounter;
        using QuantLib::Calendar;
        using QuantLib::BusinessDayConvention;
        using QuantLib::Spread;

        //! Yield volatility curve configuration
        /*!
        \ingroup configuration
        */
        class YieldVolatilityCurveConfig : public CurveConfig {
        public:
            //! supported volatility dimensions
            enum class Dimension { ATM, Smile };
            // supported volatility types
            enum class VolatilityType { Lognormal, Normal, ShiftedLognormal };

            //! \name Constructors/Destructors
            //@{
            //! Default constructor
            YieldVolatilityCurveConfig() {}
            //! Detailed constructor
            YieldVolatilityCurveConfig(const string& curveID, const string& curveDescription, const Dimension& dimension,
                const VolatilityType& volatilityType, const bool extrapolate,
                const bool flatExtrapolation, const vector<Period>& optionTenors,
                const vector<Period>& bondTenors, const DayCounter& dayCounter,
                const Calendar& calendar, const BusinessDayConvention& businessDayConvention,
                // Only required for smile
                const vector<Period>& smileOptionTenors = vector<Period>(),
                const vector<Period>& smileBondTenors = vector<Period>(),
                const vector<Spread>& smileSpreads = vector<Spread>());
            //@}

            //! \name Serialisation
            //@{
            void fromXML(XMLNode* node) override;
            XMLNode* toXML(XMLDocument& doc) override;
            //@}

            //! \name Inspectors
            //@{
            const Dimension& dimension() const { return dimension_; }
            const VolatilityType& volatilityType() const { return volatilityType_; }
            const bool& extrapolate() const { return extrapolate_; }
            const bool& flatExtrapolation() const { return flatExtrapolation_; }
            const vector<Period>& optionTenors() const { return optionTenors_; }
            const vector<Period>& bondTenors() const { return bondTenors_; }
            const DayCounter& dayCounter() const { return dayCounter_; }
            const Calendar& calendar() const { return calendar_; }
            const BusinessDayConvention& businessDayConvention() const { return businessDayConvention_; }
            const vector<Period>& smileOptionTenors() const { return smileOptionTenors_; }
            const vector<Period>& smileBondTenors() const { return smileBondTenors_; }
            const vector<Spread>& smileSpreads() const { return smileSpreads_; }
            const vector<string>& quotes() override;
            //@}

            //! \name Setters
            //@{
            Dimension& dimension() { return dimension_; }
            VolatilityType& volatilityType() { return volatilityType_; }
            bool& flatExtrapolation() { return flatExtrapolation_; }
            vector<Period>& optionTenors() { return optionTenors_; }
            vector<Period>& bondTenors() { return bondTenors_; }
            DayCounter& dayCounter() { return dayCounter_; }
            Calendar& calendar() { return calendar_; }
            vector<Period>& smileOptionTenors() { return smileOptionTenors_; }
            vector<Period>& smileBondTenors() { return smileBondTenors_; }
            vector<Spread>& smileSpreads() { return smileSpreads_; }
            //@}

        private:
            Dimension dimension_;
            VolatilityType volatilityType_;
            bool extrapolate_, flatExtrapolation_;
            vector<Period> optionTenors_, bondTenors_;
            DayCounter dayCounter_;
            Calendar calendar_;
            BusinessDayConvention businessDayConvention_;
            vector<Period> smileOptionTenors_;
            vector<Period> smileBondTenors_;
            vector<Spread> smileSpreads_;
        };

        std::ostream& operator<<(std::ostream& out, YieldVolatilityCurveConfig::VolatilityType t);
    } // namespace data
} // namespace ore
