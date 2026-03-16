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

/*! \file qle/models/yoycapfloorhelper.hpp
    \brief Year on year inflation cap floor calibration helper
    \ingroup models
*/

#ifndef quantext_yoycapfloor_helper_hpp
#define quantext_yoycapfloor_helper_hpp

#include <ql/instruments/inflationcapfloor.hpp>
#include <ql/models/calibrationhelper.hpp>

namespace QuantExt {

/*! Year on year (YoY) inflation cap floor calibration helper.
    \ingroup models
*/
class YoYCapFloorHelper : public QuantLib::CalibrationHelper, public QuantLib::Observer, public QuantLib::Observable {
public:
    YoYCapFloorHelper(const QuantLib::Handle<QuantLib::Quote>& premium,
        QuantLib::YoYInflationCapFloor::Type type,
        QuantLib::Rate strike,
        QuantLib::Natural settlementDays,
        const QuantLib::Period& tenor,
        const QuantLib::ext::shared_ptr<QuantLib::YoYInflationIndex>& yoyIndex,
        const QuantLib::Period& observationLag,
        const QuantLib::Calendar& yoyCalendar,
        QuantLib::BusinessDayConvention yoyConvention,
        const QuantLib::DayCounter& yoyDayCount,
        const QuantLib::Calendar& paymentCalendar,
        QuantLib::BusinessDayConvention paymentConvention,
        const QuantLib::Period& yoyTenor = 1 * QuantLib::Years);

    //! \name CalibrationHelper interface
    //@{
    QuantLib::Real calibrationError() override;
    //@}

    //! \name Observer interface
    //@{
    void update() override;
    //@}

    //! \name YoYCapFloorHelper inspectors
    //@{
    QuantLib::ext::shared_ptr<QuantLib::YoYInflationCapFloor> yoyCapFloor() const;
    //@}

    //! Set the pricing engine to be used by the underlying YoY cap floor
    void setPricingEngine(const QuantLib::ext::shared_ptr<QuantLib::PricingEngine>& engine);

    //! Return the market premium value
    QuantLib::Real marketValue() const;

    //! Return the model value
    QuantLib::Real modelValue() const;

private:
    //! Create the underlying YoY cap floor
    void createCapFloor();

    //! The market price quote for the YoY cap floor
    QuantLib::Handle<QuantLib::Quote> premium_;

    //! The underlying YoY cap floor
    QuantLib::ext::shared_ptr<QuantLib::YoYInflationCapFloor> yoyCapFloor_;

    //! The pricing engine used to value the YoY swap
    QuantLib::ext::shared_ptr<QuantLib::PricingEngine> engine_;

    // Store variables needed to rebuild the YoY cap floor
    QuantLib::Date evaluationDate_;
    QuantLib::YoYInflationCapFloor::Type type_;
    QuantLib::Rate strike_;
    QuantLib::Natural settlementDays_;
    QuantLib::Period tenor_;
    QuantLib::ext::shared_ptr<QuantLib::YoYInflationIndex> yoyIndex_;
    QuantLib::Period observationLag_;
    QuantLib::Calendar yoyCalendar_;
    QuantLib::BusinessDayConvention yoyConvention_;
    QuantLib::DayCounter yoyDayCount_;
    QuantLib::Calendar paymentCalendar_;
    QuantLib::BusinessDayConvention paymentConvention_;
    QuantLib::Period yoyTenor_;
};

}

#endif
