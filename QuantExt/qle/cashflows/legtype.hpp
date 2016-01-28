/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file qle/instruments/legtype.hpp
    \brief Leg Type
*/

#ifndef quantext_legtype_hpp
#define quantext_legtype_hpp

namespace QuantExt {

    //! Currency Leg
    /*! QuantLib Leg with additional information: currency, payer, leg type
      \warning leg BPS and NPV are in leg currency rather than base currency
     */
    enum LegType { 
        Nominal = 0,
        Fixed = 1, 
        Ibor = 2,
        OIS = 3,
        CMS = 4,
        Inflation = 5,
        CappedFlooredIbor = 6, 
        CappedFlooredCMS = 7,
        CappedFlooredInflation = 8, 
        DigitalIbor = 9, 
        DigitalCMS = 10,
        FxOptionLeg = 11,
        SwaptionLeg = 12,
        RangeAccrual = 13,
        Structured = 14  // anything but Nominal, Fixed or Ibor
    }; 

    //TODO: LegType to string
    /*! \relates LegType */
    //std::ostream& operator<<(std::ostream&, LegType);
}

#endif
