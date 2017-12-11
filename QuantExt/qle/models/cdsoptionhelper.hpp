/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

/*! \file models/cdsoptionhelper.hpp
    \brief cds option calibration helper
    \ingroup models
*/

#ifndef quantext_cdsoptionhelper_hpp
#define quantext_cdsoptionhelper_hpp

#include <qle/instruments/cdsoption.hpp>
#include <qle/instruments/creditdefaultswap.hpp>

#include <ql/models/calibrationhelper.hpp>
#include <ql/quotes/simplequote.hpp>

using namespace QuantLib;

namespace QuantExt {
//! CDS option helper
/*!
 \ingroup models
 */
class CdsOptionHelper : public CalibrationHelper {
public:
    CdsOptionHelper(const Date& exerciseDate, const Handle<Quote>& volatility, const Protection::Side side,
                    const Schedule& schedule, const BusinessDayConvention paymentConvention,
                    const DayCounter& dayCounter, const Handle<DefaultProbabilityTermStructure>& probability,
                    const Real recoveryRate, const Handle<YieldTermStructure>& termStructure,
                    const Rate spread = Null<Rate>(), const Rate upfront = Null<Rate>(),
                    const bool settlesAccrual = true, const bool paysAtDefaultTime = true,
                    const Date protectionStart = Date(), const Date upfrontDate = Date(),
                    const boost::shared_ptr<Claim>& claim = boost::shared_ptr<Claim>(),
                    const CalibrationHelper::CalibrationErrorType errorType = CalibrationHelper::RelativePriceError);

    virtual void addTimesTo(std::list<Time>& times) const {}
    virtual Real modelValue() const;
    virtual Real blackPrice(Volatility volatility) const;

    boost::shared_ptr<CreditDefaultSwap> underlying() const { return cds_; }
    boost::shared_ptr<QuantExt::CdsOption> option() const { return option_; }

private:
    boost::shared_ptr<CreditDefaultSwap> cds_;
    boost::shared_ptr<QuantExt::CdsOption> option_;
    boost::shared_ptr<SimpleQuote> blackVol_;
    boost::shared_ptr<PricingEngine> blackEngine_;
};
} // namespace QuantExt

#endif
