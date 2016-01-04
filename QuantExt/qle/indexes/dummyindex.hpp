/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
  Copyright (C) 2010 - 2015 Quaternion Risk Management Ltd.
  All rights reserved.
*/

/* \file dummyindex.hpp
    \brief example index
*/

#ifndef dummyindex_hpp
#define dummyindex_hpp

#include <ql/indexes/iborindex.hpp>

using namespace QuantLib;

namespace QuantExt {

  // %Dummy index
    /* Dummy rate fixed by ...

      \warning This is just a dummy test index
    */
    class DummyIndex : public IborIndex {
    public:
        DummyIndex(const Handle<YieldTermStructure>& h =
                   Handle<YieldTermStructure>());
    };

}

#endif
