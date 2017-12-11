/*
 Copyright (C) 2017 Quaternion Risk Management Ltd
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

#include <qle/models/dkimpliedyoyinflationtermstructure.hpp>

namespace QuantExt {

DkImpliedYoYInflationTermStructure::DkImpliedYoYInflationTermStructure(const boost::shared_ptr<CrossAssetModel>& model,
                                                                       Size index)
    : YoYInflationTermStructure(
          model->infdk(index)->termStructure()->dayCounter(), model->infdk(index)->termStructure()->baseRate(),
          model->infdk(index)->termStructure()->observationLag(), model->infdk(index)->termStructure()->frequency(),
          model->infdk(index)->termStructure()->indexIsInterpolated(),
          model->infdk(index)->termStructure()->nominalTermStructure()),
      model_(model), index_(index), referenceDate_(model_->infdk(index)->termStructure()->referenceDate()),
      state_z_(0.0), state_y_(0.0) {
    registerWith(model_);
    update();
}

} // namespace QuantExt
