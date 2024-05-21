/*
 Copyright (C) 2016 Quaternion Risk Management Ltd
 Copyright (C) 2024 Skandinaviska Enskilda Banken AB (publ)
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
#include <ored/configuration/reportconfig.hpp>

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
     *  SmileInterpolation - currently supports which of the 2 Vanna Volga approximations,
     *  as per  Castagna& Mercurio(2006), to use. The second approximation is more accurate
     *  but can ask for the square root of a negative number under unusual circumstances.
     */
    enum class Dimension { ATM, SmileVannaVolga, SmileDelta, SmileBFRR, SmileAbsolute, ATMTriangulated };
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
                            const string& conventionsID = "", const std::vector<Size>& smileDelta = {25},
                            const string& smileExtrapolation = "Flat");

    FXVolatilityCurveConfig(const string& curveID, const string& curveDescription, const Dimension& dimension,
                            const string& baseVolatility1, const string& baseVolatility2,
                            const string& fxIndexTag = "GENERIC");

    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) const override;
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
    const std::string& smileExtrapolation() const { return smileExtrapolation_; }
    const string& conventionsID() const { return conventionsID_; }
    const std::vector<Size>& smileDelta() const { return smileDelta_; }
    const vector<string>& quotes() override;
    const string& baseVolatility1() const { return baseVolatility1_; }
    const string& baseVolatility2() const { return baseVolatility2_; }
    const string& fxIndexTag() const { return fxIndexTag_; }
    const ReportConfig& reportConfig() const { return reportConfig_; }
    //@}

    //! \name Setters
    //@{
    Dimension& dimension() { return dimension_; }
    SmileInterpolation& smileInterpolation() { return smileInterpolation_; }
    string& smileExtrapolation() { return smileExtrapolation_; }
    vector<string>& deltas() { return deltas_; }
    DayCounter& dayCounter() { return dayCounter_; }
    Calendar& calendar() { return calendar_; }
    string& fxSpotID() { return fxSpotID_; }
    string& fxForeignYieldCurveID() { return fxForeignYieldCurveID_; }
    string& fxDomesticYieldCurveID() { return fxDomesticYieldCurveID_; }
    string conventionsID() { return conventionsID_; }
    std::vector<Size>& smileDelta() { return smileDelta_; }
    const std::set<string>& requiredYieldCurveIDs() const { return requiredYieldCurveIDs_; };
    string& baseVolatility1() { return baseVolatility1_; }
    string& baseVolatility2() { return baseVolatility2_; }
    string& fxIndexTag() { return fxIndexTag_; }
    //@}

private:
    void populateRequiredCurveIds();

    Dimension dimension_;
    vector<string> expiries_;
    vector<string> deltas_;
    DayCounter dayCounter_;
    Calendar calendar_;
    string fxSpotID_;
    string fxForeignYieldCurveID_;
    string fxDomesticYieldCurveID_;
    string conventionsID_;
    std::vector<Size> smileDelta_;
    std::set<string> requiredYieldCurveIDs_;
    SmileInterpolation smileInterpolation_;
    string smileExtrapolation_;
    string baseVolatility1_;
    string baseVolatility2_;
    string fxIndexTag_;
    ReportConfig reportConfig_;
};
} // namespace data
} // namespace ore
