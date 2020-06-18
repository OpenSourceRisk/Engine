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

/*! \file ored/portfolio/optiondata.hpp
    \brief trade option data model and serialization
    \ingroup tradedata
*/

#pragma once

#include <ored/portfolio/optionexercisedata.hpp>
#include <ored/portfolio/optionpaymentdata.hpp>
#include <ored/portfolio/schedule.hpp>

namespace ore {
namespace data {

//! Serializable obejct holding option data
/*!
  \ingroup tradedata
*/
class OptionData : public XMLSerializable {
public:
    //! Default constructor
    OptionData() : payoffAtExpiry_(true), premium_(0.0), automaticExercise_(false) {}
    //! Constructor
    OptionData(string longShort, string callPut, string style, bool payoffAtExpiry, vector<string> exerciseDates,
               string settlement = "Cash", string settlementMethod = "", double premium = 0, string premiumCcy = "",
               string premiumPayDate = "", vector<double> exerciseFees = vector<Real>(),
               vector<double> exercisePrices = vector<Real>(), string noticePeriod = "", string noticeCalendar = "",
               string noticeConvention = "", const vector<string>& exerciseFeeDates = vector<string>(),
               const vector<string>& exerciseFeeTypes = vector<string>(), string exerciseFeeSettlementPeriod = "",
               string exerciseFeeSettlementCalendar = "", string exerciseFeeSettlementConvention = "",
               string payoffType = "", const boost::optional<bool>& automaticExercise = boost::none,
               const boost::optional<OptionExerciseData>& exerciseData = boost::none,
               const boost::optional<OptionPaymentData>& paymentData = boost::none)
        : longShort_(longShort), callPut_(callPut), payoffType_(payoffType), style_(style),
          payoffAtExpiry_(payoffAtExpiry), exerciseDates_(exerciseDates), noticePeriod_(noticePeriod),
          noticeCalendar_(noticeCalendar), noticeConvention_(noticeConvention), settlement_(settlement),
          settlementMethod_(settlementMethod), premium_(premium), premiumCcy_(premiumCcy),
          premiumPayDate_(premiumPayDate), exerciseFees_(exerciseFees), exerciseFeeDates_(exerciseFeeDates),
          exerciseFeeTypes_(exerciseFeeTypes), exerciseFeeSettlementPeriod_(exerciseFeeSettlementPeriod),
          exerciseFeeSettlementCalendar_(exerciseFeeSettlementCalendar),
          exerciseFeeSettlementConvention_(exerciseFeeSettlementConvention), exercisePrices_(exercisePrices),
          automaticExercise_(automaticExercise), exerciseData_(exerciseData), paymentData_(paymentData) {}

    //! \name Inspectors
    //@{
    const string& longShort() const { return longShort_; }
    const string& callPut() const { return callPut_; }
    const string& payoffType() const { return payoffType_; }
    const string& style() const { return style_; }
    const bool& payoffAtExpiry() const { return payoffAtExpiry_; }
    const vector<string>& exerciseDates() const { return exerciseDates_; }
    const string& noticePeriod() const { return noticePeriod_; }
    const string& noticeCalendar() const { return noticeCalendar_; }
    const string& noticeConvention() const { return noticeConvention_; }
    const string& settlement() const { return settlement_; }
    const string& settlementMethod() const { return settlementMethod_; }
    double premium() const { return premium_; }
    const string& premiumCcy() const { return premiumCcy_; }
    const string& premiumPayDate() const { return premiumPayDate_; }
    const vector<double>& exerciseFees() const { return exerciseFees_; }
    const vector<string>& exerciseFeeDates() const { return exerciseFeeDates_; }
    const vector<string>& exerciseFeeTypes() const { return exerciseFeeTypes_; }
    const string& exerciseFeeSettlementPeriod() const { return exerciseFeeSettlementPeriod_; }
    const string& exerciseFeeSettlementCalendar() const { return exerciseFeeSettlementCalendar_; }
    const string& exerciseFeeSettlementConvention() const { return exerciseFeeSettlementConvention_; }
    const vector<double>& exercisePrices() const { return exercisePrices_; }
    bool automaticExercise() const {
        // Assumed false if not provided.
        return automaticExercise_ ? *automaticExercise_ : false;
    }
    const boost::optional<OptionExerciseData>& exerciseData() const { return exerciseData_; }
    const boost::optional<OptionPaymentData>& paymentData() const { return paymentData_; }
    //@}

    //! \name Serialisation
    //@{
    virtual void fromXML(XMLNode* node) override;
    virtual XMLNode* toXML(XMLDocument& doc) override;
    //@}

private:
    string longShort_;    // long or short
    string callPut_;      // call or put
    string payoffType_;   // Accumulator, Decumulator, TargetExact, TargetFull, TargetTruncated, ...
    string style_;        // European, Bermudan, American
    bool payoffAtExpiry_; // Y or N
    vector<string> exerciseDates_;
    string noticePeriod_;
    string noticeCalendar_;
    string noticeConvention_;
    string settlement_;       // Cash or Physical, default Cash.
    string settlementMethod_; // QuantLib::Settlement::Method, default empty
    double premium_;
    string premiumCcy_;
    string premiumPayDate_;
    vector<double> exerciseFees_;
    vector<string> exerciseFeeDates_;
    vector<string> exerciseFeeTypes_;
    string exerciseFeeSettlementPeriod_;
    string exerciseFeeSettlementCalendar_;
    string exerciseFeeSettlementConvention_;
    vector<double> exercisePrices_;
    boost::optional<bool> automaticExercise_;
    boost::optional<OptionExerciseData> exerciseData_;
    boost::optional<OptionPaymentData> paymentData_;
};
} // namespace data
} // namespace ore
