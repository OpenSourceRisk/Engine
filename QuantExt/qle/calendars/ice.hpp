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

/*! \file ice.hpp
    \brief Intercontinental Exchange calendars
*/

#ifndef quantext_ice_calendar_hpp
#define quantext_ice_calendar_hpp

#include <ql/time/calendar.hpp>

namespace QuantExt {

/*! Various ICE calendars outlined on the website at:
    https://www.theice.com/holiday-hours
*/
class ICE : public QuantLib::Calendar {

private:
    class FuturesUSImpl : public QuantLib::Calendar::WesternImpl {
    public:
        std::string name() const override { return "ICE Futures U.S."; }
        bool isBusinessDay(const QuantLib::Date& d) const override;
    };

    class FuturesUSImpl_1 : public FuturesUSImpl {
    public:
        std::string name() const override { return "ICE Futures U.S. 1"; }
        bool isBusinessDay(const QuantLib::Date& d) const override;
    };

    class FuturesUSImpl_2 : public FuturesUSImpl {
    public:
        std::string name() const override { return "ICE Futures U.S. 2"; }
        bool isBusinessDay(const QuantLib::Date& d) const override;
    };

    class FuturesEUImpl : public QuantLib::Calendar::WesternImpl {
    public:
        std::string name() const override { return "ICE Futures Europe"; }
        bool isBusinessDay(const QuantLib::Date& d) const override;
    };

    class FuturesEUImpl_1 : public FuturesEUImpl {
    public:
        std::string name() const override { return "ICE Futures Europe 1"; }
        bool isBusinessDay(const QuantLib::Date& d) const override;
    };

    class EndexEnergyImpl : public QuantLib::Calendar::WesternImpl {
    public:
        std::string name() const override { return "ICE Endex Energy"; }
        bool isBusinessDay(const QuantLib::Date& d) const override;
    };

    class EndexEquitiesImpl : public QuantLib::Calendar::WesternImpl {
    public:
        std::string name() const override { return "ICE Endex Equities"; }
        bool isBusinessDay(const QuantLib::Date& d) const override;
    };

    class SwapTradeUSImpl : public QuantLib::Calendar::WesternImpl {
    public:
        std::string name() const override { return "ICE Swap Trade U.S."; }
        bool isBusinessDay(const QuantLib::Date& d) const override;
    };

    class SwapTradeUKImpl : public QuantLib::Calendar::WesternImpl {
    public:
        std::string name() const override { return "ICE Swap Trade U.K."; }
        bool isBusinessDay(const QuantLib::Date& d) const override;
    };

    class FuturesSingaporeImpl : public QuantLib::Calendar::WesternImpl {
    public:
        std::string name() const override { return "ICE Futures Singapore"; }
        bool isBusinessDay(const QuantLib::Date& d) const override;
    };

public:
    enum Market {
        FuturesUS,       /*!< ICE Futures U.S. Currency, Stock and Credit Index,
                              Metal, Nat Gas, Power, Oil and Environmental */
        FuturesUS_1,     //!< ICE Futures U.S. Sugar, Cocoa, Coffee, Cotton and FCOJ
        FuturesUS_2,     //!< ICE Futures U.S. Canola
        FuturesEU,       //!< ICE Futures Europe
        FuturesEU_1,     //!< ICE Futures Europe for contracts where 26 Dec is a holiday
        EndexEnergy,     //!< ICE Endex European power and natural gas products
        EndexEquities,   //!< ICE Endex European equities
        SwapTradeUS,     //!< ICE Swap Trade U.S.
        SwapTradeUK,     //!< ICE Swap Trade U.K.
        FuturesSingapore //!< ICE futures Singapore
    };

    ICE(Market market);
};

} // namespace QuantExt

#endif
