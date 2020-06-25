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

#include <qle/indexes/region.hpp>

namespace QuantExt {

SwedenRegion::SwedenRegion() {
    static boost::shared_ptr<Data> SEdata(new Data("Sweden", "SE"));
    data_ = SEdata;
}

DenmarkRegion::DenmarkRegion() {
    static boost::shared_ptr<Data> DKdata(new Data("Denmark", "DK"));
    data_ = DKdata;
}

CanadaRegion::CanadaRegion() {
    static boost::shared_ptr<Data> CAdata = boost::make_shared<Data>("Canada", "CA");
    data_ = CAdata;
}

} // namespace QuantExt
