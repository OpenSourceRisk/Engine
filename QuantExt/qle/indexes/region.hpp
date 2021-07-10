/*
 Copyright (C) 2018 Quaternion Risk Management Ltd
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

/*! \file qle/indexes/region.hpp
    \brief Region, i.e. geographical area, specification
*/

#ifndef quantext_region_hpp
#define quantext_region_hpp

#include <ql/indexes/region.hpp>

namespace QuantExt {

//! Sweden as geographical/economic region
class SwedenRegion : public QuantLib::Region {
public:
    SwedenRegion();
};

//! Denmark as geographical/economic region
class DenmarkRegion : public QuantLib::Region {
public:
    DenmarkRegion();
};

//! Canada as geographical/economic region
class CanadaRegion : public QuantLib::Region {
public:
    CanadaRegion();
};

//! Spain as geographical/economic region
class SpainRegion : public QuantLib::Region {
public:
    SpainRegion();
};

//! Germany as geographical/economic region
class GermanyRegion : public QuantLib::Region {
    public:
    GermanyRegion();
};

//! Belgium as geographical/economic region
class BelgiumRegion : public QuantLib::Region {
public:
    BelgiumRegion();
};

} // namespace QuantExt

#endif
