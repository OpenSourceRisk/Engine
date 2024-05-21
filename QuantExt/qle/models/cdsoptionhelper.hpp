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
#include <ql/instruments/creditdefaultswap.hpp>

#include <ql/models/calibrationhelper.hpp>
#include <ql/quotes/simplequote.hpp>

namespace QuantExt {
using namespace QuantLib;
//! CDS option helper
/*!
 \ingroup models
 */
class CdsOptionHelper : public BlackCalibrationHelper {
public:
    CdsOptionHelper(
        const Date& exerciseDate, const Handle<Quote>& volatility, const Protection::Side side,
        const Schedule& schedule, const BusinessDayConvention paymentConvention, const DayCounter& dayCounter,
        const Handle<DefaultProbabilityTermStructure>& probability, const Real recoveryRate,
        const Handle<YieldTermStructure>& termStructure, const Rate spread = Null<Rate>(),
        const Rate upfront = Null<Rate>(), const bool settlesAccrual = true,
        const CreditDefaultSwap::ProtectionPaymentTime proteectionPaymentTime =
            CreditDefaultSwap::ProtectionPaymentTime::atDefault,
        const Date protectionStart = Date(), const Date upfrontDate = Date(),
        const QuantLib::ext::shared_ptr<Claim>& claim = QuantLib::ext::shared_ptr<Claim>(),
        const BlackCalibrationHelper::CalibrationErrorType errorType = BlackCalibrationHelper::RelativePriceError);

    virtual void addTimesTo(std::list<Time>& times) const override {}
    virtual Real modelValue() const override;
    virtual Real blackPrice(Volatility volatility) const override;

    QuantLib::ext::shared_ptr<CreditDefaultSwap> underlying() const { return cds_; }
    QuantLib::ext::shared_ptr<QuantExt::CdsOption> option() const { return option_; }

private:
    Handle<YieldTermStructure> termStructure_;
    QuantLib::ext::shared_ptr<CreditDefaultSwap> cds_;
    QuantLib::ext::shared_ptr<QuantExt::CdsOption> option_;
    QuantLib::ext::shared_ptr<SimpleQuote> blackVol_;
    QuantLib::ext::shared_ptr<PricingEngine> blackEngine_;
};
} // namespace QuantExt

#endif
