/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2015 Quaternion Risk Management Ltd.
  All rights reserved.
*/

#include <ql/time/calendars/target.hpp>
#include <ql/time/daycounters/actual360.hpp>
#include <ql/time/daycounters/actual365fixed.hpp>
#include <ql/currencies/europe.hpp>

#include <qle/indexes/dummyindex.hpp>

namespace QuantExt {

  DummyIndex::DummyIndex(const Handle<YieldTermStructure>& h)
    : IborIndex("DummyIndex", 1*Days,
                2, // settlement days
                EURCurrency(), 
                TARGET(),
                Following, 
                false,
                Actual360(), 
                h) {}

}
