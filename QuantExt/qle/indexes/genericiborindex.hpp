/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2014 - 2016 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/* \file genericiborindex.hpp
   \brief Generic Ibor Index
*/

#ifndef _quantext_generic_index_hpp
#define _quantext_generic_index_hpp

#include <ql/indexes/iborindex.hpp>

using namespace QuantLib;

namespace QuantExt {

    //! Generic Ibor Index
    /*! This Ibor Index allows you to wrap any arbitary currency in a generic index.
        
        We assume 2 settlement days, Target Calendar, ACT/360.

        The name is always CCY-GENERIC so there is no risk of collision with real ibor names
     */
    class GenericIborIndex : public IborIndex {
    public:
        GenericIborIndex(const Period& tenor, const Currency& ccy, const Handle<YieldTermStructure>& h =
                         Handle<YieldTermStructure>())
        : IborIndex(ccy.code() + "-GENERIC", tenor, 2, ccy, TARGET(),
                    Following, false, Actual360(), h) {}
    };

}

#endif
