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

/*! \file qle/instruments/equityautodeltahedgedoption.hpp
    \brief Equity Auto Delta Hedged Option instrument
    \ingroup instruments
*/

#ifndef quantext_equity_auto_delta_hedged_option_hpp
#define quantext_equity_auto_delta_hedged_option_hpp

#include <ql/currency.hpp>
#include <ql/instrument.hpp>
#include <ql/option.hpp>
#include <ql/time/date.hpp>

namespace QuantExt {
using namespace QuantLib;

//! Data for a single underlying option batch within the delta-hedged structure
struct UnderlyingOptionBatch {
    Option::Type type;
    Real strike;
    Real quantity;
    Real premium;
    Currency premiumCurrency;
    Date expiryDate;
};

//! Equity Auto Delta Hedged Option instrument
/*! An instrument consisting of N batches of European vanilla equity options
    with an embedded discrete delta-hedging strategy.

    The equity amount at expiry for each batch i is:
      EqAmount_i = [DeltaHedge_Final^i + Premium^i - Option_Final^i] x N^i

    \ingroup instruments
*/
class EquityAutoDeltaHedgedOption : public Instrument {
public:
    class arguments;
    class engine;
    //! \name Constructors
    //@{
    EquityAutoDeltaHedgedOption(const std::vector<UnderlyingOptionBatch>& batches,
                                Real hedgingVolatility,
                                Real driftRate,
                                const Date& observationStartDate,
                                const std::string& equityName,
                                const Currency& currency);

    //! \name Instrument interface
    //@{
    bool isExpired() const override;
    void setupArguments(PricingEngine::arguments*) const override;
    //@}

    //! \name Additional interface
    //@{
    const std::vector<UnderlyingOptionBatch>& batches() const { return batches_; }
    Real hedgingVolatility() const { return hedgingVol_; }
    Real driftRate() const { return driftRate_; }
    const Date& observationStartDate() const { return observationStartDate_; }
    const std::string& equityName() const { return equityName_; }
    const Currency& currency() const { return currency_; }
    //@}

private:
    // data members
    std::vector<UnderlyingOptionBatch> batches_;
    Real hedgingVol_;
    Real driftRate_;
    Date observationStartDate_;
    std::string equityName_;
    Currency currency_;
};

//! \ingroup instruments
class EquityAutoDeltaHedgedOption::arguments : public virtual PricingEngine::arguments {
public:
    std::vector<UnderlyingOptionBatch> batches;
    Real hedgingVolatility;
    Real driftRate;
    Date observationStartDate;
    std::string equityName;
    Currency currency;
    void validate() const override;
};

//! \ingroup instruments
class EquityAutoDeltaHedgedOption::engine
    : public GenericEngine<EquityAutoDeltaHedgedOption::arguments, Instrument::results> {};

} // namespace QuantExt

#endif
