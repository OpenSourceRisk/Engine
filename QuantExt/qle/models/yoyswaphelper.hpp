/*
 Copyright (C) 2020 Quaternion Risk Management Ltd
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

/*! \file qle/models/yoyswaphelper.hpp
    \brief Year on year inflation swap calibration helper
    \ingroup models
*/

#ifndef quantext_yoy_swap_helper_hpp
#define quantext_yoy_swap_helper_hpp

#include <ql/instruments/yearonyearinflationswap.hpp>
#include <ql/models/calibrationhelper.hpp>

namespace QuantExt {

/*! Year on year (YoY) inflation swap calibration helper.
    \ingroup models
*/
class YoYSwapHelper : public QuantLib::CalibrationHelper, public QuantLib::Observer, public QuantLib::Observable {
public:
    //! Year on year helper constructor
    YoYSwapHelper(const QuantLib::Handle<QuantLib::Quote>& rate,
        QuantLib::Natural settlementDays,
        const QuantLib::Period& tenor,
        const QuantLib::ext::shared_ptr<QuantLib::YoYInflationIndex>& yoyIndex,
        const QuantLib::Handle<QuantLib::YieldTermStructure>& rateCurve,
        const QuantLib::Period& observationLag,
        const QuantLib::Calendar& yoyCalendar,
        QuantLib::BusinessDayConvention yoyConvention,
        const QuantLib::DayCounter& yoyDayCount,
        const QuantLib::Calendar& fixedCalendar,
        QuantLib::BusinessDayConvention fixedConvention,
        const QuantLib::DayCounter& fixedDayCount,
        const QuantLib::Calendar& paymentCalendar,
        QuantLib::BusinessDayConvention paymentConvention,
        const QuantLib::Period& fixedTenor = 1 * QuantLib::Years,
        const QuantLib::Period& yoyTenor = 1 * QuantLib::Years);

    //! \name CalibrationHelper interface
    //@{
    QuantLib::Real calibrationError() override;
    //@}

    //! \name Observer interface
    //@{
    void update() override;
    //@}

    //! \name YoYSwapHelper inspectors
    //@{
    QuantLib::ext::shared_ptr<QuantLib::YearOnYearInflationSwap> yoySwap() const;
    //@}

    //! Set the pricing engine to be used by the underlying YoY swap
    void setPricingEngine(const QuantLib::ext::shared_ptr<QuantLib::PricingEngine>& engine);

    //! Return the market fair year on year rate
    QuantLib::Real marketRate() const;

    //! Return the model implied fair year on year rate
    QuantLib::Real modelRate() const;

private:
    //! Create the underlying YoY swap
    void createSwap();

    //! The YoY market swap quote
    QuantLib::Handle<QuantLib::Quote> rate_;

    //! The underlying YoY swap
    QuantLib::ext::shared_ptr<QuantLib::YearOnYearInflationSwap> yoySwap_;

    //! The pricing engine used to value the YoY swap
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engine_;

    // Store variables needed to rebuild the YoY swap
    QuantLib::Date evaluationDate_;
    QuantLib::Natural settlementDays_;
    QuantLib::Period tenor_;
    QuantLib::ext::shared_ptr<QuantLib::YoYInflationIndex> yoyIndex_;
    QuantLib::Handle<QuantLib::YieldTermStructure> rateCurve_;
    QuantLib::Period observationLag_;
    QuantLib::Calendar yoyCalendar_;
    QuantLib::BusinessDayConvention yoyConvention_;
    QuantLib::DayCounter yoyDayCount_;
    QuantLib::Calendar fixedCalendar_;
    QuantLib::BusinessDayConvention fixedConvention_;
    QuantLib::DayCounter fixedDayCount_;
    QuantLib::Calendar paymentCalendar_;
    QuantLib::BusinessDayConvention paymentConvention_;
    QuantLib::Period fixedTenor_;
    QuantLib::Period yoyTenor_;
};

}

#endif
