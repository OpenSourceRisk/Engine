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

/*! \file ored/configuration/fxvolcurveconfig.hpp
    \brief FX volatility curve configuration classes
    \ingroup configuration
*/

#pragma once

#include <ored/configuration/curveconfig.hpp>
#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/time/period.hpp>
#include <ql/types.hpp>

namespace ore {
namespace data {
using ore::data::XMLNode;
using QuantLib::Calendar;
using QuantLib::DayCounter;
using QuantLib::Period;
using std::string;
using std::vector;

//! FX volatility structure configuration
/*!
  \ingroup configuration
*/
class FXVolatilityCurveConfig : public CurveConfig {
public:
    //! supported volatility structure types
    /*! For ATM we will only load ATM quotes, for Smile we load ATM, RR, BF or Deltas
     *  SmileInterpolation - currently suports which of the 2 Vanna Volga approximations,
     *  as per  Castagna& Mercurio(2006), to use. The second approximation is more accurate
     *  but can ask for the square root of a negative number under unusual circumstances.
     */
    enum class Dimension { ATM, SmileVannaVolga, SmileDelta };
    enum class SmileInterpolation {
        VannaVolga1,
        VannaVolga2,
        Linear,
        Cubic
    }; // Vanna Volga first/second approximation respectively

    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    FXVolatilityCurveConfig() {}
    //! Detailed constructor
    FXVolatilityCurveConfig(const string& curveID, const string& curveDescription, const Dimension& dimension,
                            const vector<string>& expiries, const vector<string>& deltas = vector<string>(),
                            const string& fxSpotID = "", const string& fxForeignCurveID = "",
                            const string& fxDomesticCurveID = "",
                            const DayCounter& dayCounter = QuantLib::Actual365Fixed(),
                            const Calendar& calendar = QuantLib::TARGET(),
                            const SmileInterpolation& interp = SmileInterpolation::VannaVolga2,
                            const string& conventionsID = "", const QuantLib::Natural& smileDelta = 25);

    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Inspectors
    //@{
    const Dimension& dimension() const { return dimension_; }
    const vector<string>& expiries() const { return expiries_; }
    const vector<string>& deltas() const { return deltas_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    const Calendar& calendar() const { return calendar_; }
    // only required for Smile
    const string& fxSpotID() const { return fxSpotID_; }
    const string& fxForeignYieldCurveID() const { return fxForeignYieldCurveID_; }
    const string& fxDomesticYieldCurveID() const { return fxDomesticYieldCurveID_; }
    const SmileInterpolation& smileInterpolation() const { return smileInterpolation_; }
    const string& conventionsID() const { return conventionsID_; }
    const QuantLib::Natural& smileDelta() const { return smileDelta_; }
    const vector<string>& quotes() override;
    //@}

    //! \name Setters
    //@{
    Dimension& dimension() { return dimension_; }
    SmileInterpolation& smileInterpolation() { return smileInterpolation_; }
    vector<string>& deltas() { return deltas_; }
    DayCounter& dayCounter() { return dayCounter_; }
    Calendar& calendar() { return calendar_; }
    string& fxSpotID() { return fxSpotID_; }
    string& fxForeignYieldCurveID() { return fxForeignYieldCurveID_; }
    string& fxDomesticYieldCurveID() { return fxDomesticYieldCurveID_; }
    string conventionsID() { return conventionsID_; }
    QuantLib::Natural& smileDelta() { return smileDelta_; }
    const std::set<string>& requiredYieldCurveIDs() const { return requiredYieldCurveIDs_; };
    //@}
private:
    void populateRequiredYieldCurveIDs();

    Dimension dimension_;
    vector<string> expiries_;
    vector<string> deltas_;
    DayCounter dayCounter_;
    Calendar calendar_;
    string fxSpotID_;
    string fxForeignYieldCurveID_;
    string fxDomesticYieldCurveID_;
    string conventionsID_;
    QuantLib::Natural smileDelta_;
    std::set<string> requiredYieldCurveIDs_;
    SmileInterpolation smileInterpolation_;
};
} // namespace data
} // namespace ore
