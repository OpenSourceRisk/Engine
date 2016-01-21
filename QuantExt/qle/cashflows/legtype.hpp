/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/*! \file qle/cashflows/legtype.hpp
    \brief Leg type enumeration
*/

#ifndef quantext_legtype_hpp_
#define quantext_legtype_hpp_

namespace QuantExt {

    //! QuantLib Leg Type
    /*! QuantLib Leg Type
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

    /*! \relates LegType */
    // TODO:
    //std::ostream& operator<<(std::ostream&, LegType);
}

#endif
