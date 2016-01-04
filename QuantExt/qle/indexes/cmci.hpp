/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2015 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*
 Copyright (C) 2012 Roland Lichters

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file cmci.hpp
    \brief constant maturity commodity index
*/

#ifndef quantext_cmci_hpp
#define quantext_cmci_hpp

#include <ql/index.hpp>
#include <ql/time/calendar.hpp>
#include <ql/currency.hpp>
#include <ql/time/daycounter.hpp>
#include <ql/time/period.hpp>
#include <ql/time/calendars/target.hpp>
#include <qle/indexes/commodityfuturespriceindex.hpp>

namespace QuantLib {

    //! constant maturity commodity index
    
    class CMCI : public CommodityFuturesPriceIndex {
      public:
        CMCI(Period expiry) 
        : CommodityFuturesPriceIndex("CMCI", expiry, TARGET()) {}
    };

    class CMCI3M : public CMCI {
      public:
        CMCI3M() : CMCI(3*Months) {}
    };

    class CMCI6M : public CMCI {
      public:
        CMCI6M() : CMCI(6*Months) {}
    };

    class CMCI1Y : public CMCI {
      public:
        CMCI1Y() : CMCI(1*Years) {}
    };

    class CMCI2Y : public CMCI {
      public:
        CMCI2Y() : CMCI(2*Years) {}
    };

    class CMCI3Y : public CMCI {
      public:
        CMCI3Y() : CMCI(3*Years) {}
    };
    

}

#endif
