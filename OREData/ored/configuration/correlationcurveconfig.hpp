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

/*! \file ored/configuration/swaptionvolcurveconfig.hpp
    \brief Swaption volatility curve configuration classes
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
using QuantLib::Null;

//! Correlation curve configuration
/*!
  \ingroup configuration
*/
class CorrelationCurveConfig : public CurveConfig {
public:
    //! supported Correlation dimensions
    enum class Dimension { ATM , Constant};
    // supported volatility types
    enum class QuoteType { Rate, Price };

    //! \name Constructors/Destructors
    //@{
    //! Default constructor
    CorrelationCurveConfig() {}
    //! Detailed constructor
    CorrelationCurveConfig(const string& curveID, const string& curveDescription, const Dimension& dimension,
                                  const QuoteType& quoteType, const bool extrapolate,
                                  const vector<Period>& optionTenors,
                                  const DayCounter& dayCounter,
                                  const Calendar& calendar, const BusinessDayConvention& businessDayConvention,
                                  const string& index1, const string& index2, const string& currency, const string& swaptionVol = "",
                                  const string& discountCurve = "", const string& cmsModel = "", const string& cmsEngine = "",
                                  const Real cmsMeanReversion = Null<Real>(), const Real cmsLowerRateBoundLogNormal = Null<Real>(),
                                  const Real cmsUpperRateBoundLogNormal = Null<Real>(), const Real cmsLowerRateBoundNormal = Null<Real>(),
                                  const Real cmsUpperRateBoundNormal = Null<Real>(), const Real cmsPriceThreshold = Null<Real>(),
                                  const string& cmsBsStdDev = "", const string& cmsSpreadModel = "", const string& cmsSpreadEngine = "",
                                  const Real cmsSpreadIntegrationPoints = Null<Real>());
    //@}

    //! \name Serialisation
    //@{
    void fromXML(XMLNode* node) override;
    XMLNode* toXML(XMLDocument& doc) override;
    //@}

    //! \name Inspectors
    //@{
    const Dimension& dimension() const { return dimension_; }
    const QuoteType& quoteType() const { return quoteType_; }
    const bool& extrapolate() const { return extrapolate_; }
    const vector<Period>& optionTenors() const { return optionTenors_; }
    const DayCounter& dayCounter() const { return dayCounter_; }
    const Calendar& calendar() const { return calendar_; }
    const BusinessDayConvention& businessDayConvention() const { return businessDayConvention_; }
    const string& index1() const { return index1_; }
    const string& index2() const { return index2_; }
    const string& currency() const { return currency_; }
    const string& swaptionVolatility() const { return swaptionVol_; }
    const string& discountCurve() const { return discountCurve_; }
    const vector<string>& quotes() override;

    const string cmsModel() const { return cmsModel_; }
    const string cmsEngine() const { return cmsEngine_; }
    const Real cmsMeanReversion() const { return cmsMeanReversion_; }
    const Real cmsLowerRateBoundLogNormal() const { return cmsLowerRateBoundLogNormal_; }
    const Real cmsUpperRateBoundLogNormal() const { return cmsUpperRateBoundLogNormal_; }
    const Real cmsLowerRateBoundNormal() const { return cmsLowerRateBoundNormal_; }
    const Real cmsUpperRateBoundNormal() const { return cmsUpperRateBoundNormal_; }
    const Real cmsVegaRatio() const { return cmsVegaRatio_; }
    const Real cmsPriceThreshold() const { return cmsPriceThreshold_; }
    const string cmsBsStdDev() const { return cmsBsStdDev_; }

    const string cmsSpreadModel() const { return cmsSpreadModel_; }
    const string cmsSpreadEngine() const { return cmsSpreadEngine_; }
    const Real cmsSpreadIntegrationPoints() const { return cmsSpreadIntegrationPoints_; }
    //@}

    //! \name Setters
    //@{
    Dimension& dimension() { return dimension_; }
    QuoteType& quoteType() { return quoteType_; }
    bool& extrapolate() { return extrapolate_; }
    vector<Period>& optionTenors() { return optionTenors_; }
    DayCounter& dayCounter() { return dayCounter_; }
    Calendar& calendar() { return calendar_; }
    string& index1() { return index1_; }
    string& index2() { return index2_; }
    string& currency() { return currency_; }
    string& swaptionVolatility() { return swaptionVol_; }
    string& discountCurve() { return discountCurve_; }
    
    string& cmsModel() { return cmsModel_; }
    string& cmsEngine() { return cmsEngine_; }
    Real& cmsMeanReversion() { return cmsMeanReversion_; }
    Real& cmsLowerRateBoundLogNormal() { return cmsLowerRateBoundLogNormal_; }
    Real& cmsUpperRateBoundLogNormal() { return cmsUpperRateBoundLogNormal_; }
    Real& cmsLowerRateBoundNormal() { return cmsLowerRateBoundNormal_; }
    Real& cmsUpperRateBoundNormal() { return cmsUpperRateBoundNormal_; }
    Real& cmsVegaRatio() { return cmsVegaRatio_; }
    Real& cmsPriceThreshold() { return cmsPriceThreshold_; }
    string& cmsBsStdDev() { return cmsBsStdDev_; }

    string& cmsSpreadModel() { return cmsSpreadModel_; }
    string& cmsSpreadEngine() { return cmsSpreadEngine_; }
    Real& cmsSpreadIntegrationPoints() { return cmsSpreadIntegrationPoints_; }
    //@}

private:
    Dimension dimension_;
    QuoteType quoteType_;
    bool extrapolate_;
    vector<Period> optionTenors_;
    DayCounter dayCounter_;
    Calendar calendar_;
    BusinessDayConvention businessDayConvention_;
    string index1_, index2_;
    string currency_;
    string swaptionVol_;
    string discountCurve_;
    
    string cmsModel_;
    string cmsEngine_;
    Real cmsMeanReversion_;
    Real cmsLowerRateBoundLogNormal_;
    Real cmsUpperRateBoundLogNormal_;
    Real cmsLowerRateBoundNormal_;
    Real cmsUpperRateBoundNormal_;
    Real cmsVegaRatio_;
    Real cmsPriceThreshold_;
    string cmsBsStdDev_;

    string cmsSpreadModel_;
    string cmsSpreadEngine_;
    Real cmsSpreadIntegrationPoints_;
};

std::ostream& operator<<(std::ostream& out, CorrelationCurveConfig::QuoteType t);
} // namespace data
} // namespace ore
