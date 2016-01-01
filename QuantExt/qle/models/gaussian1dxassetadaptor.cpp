/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2015 Quaternion Risk Management

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

#include <qle/models/gaussian1dxassetadaptor.hpp>
#include <qle/processes/irlgm1fstateprocess.hpp>

#include <boost/make_shared.hpp>

namespace QuantExt {

Gaussian1dXAssetAdaptor::Gaussian1dXAssetAdaptor(
    Size ccy, const boost::shared_ptr<const XAssetModel> &model)
    : Gaussian1dModel(model->irlgm1f(ccy)->termStructure()), ccy_(ccy),
      x_(model) {
    stateProcess_ =
        boost::make_shared<IrLgm1fStateProcess>(model->irlgm1f(ccy));
}

} // namespace QuantExt
