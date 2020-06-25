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

/*! \file ored/model/modelbuilder.hpp
    \brief Model builder base class
    \ingroup models
*/

#pragma once

#include <ql/patterns/lazyobject.hpp>

namespace ore {
namespace data {

class ModelBuilder : public QuantLib::LazyObject {
public:
    //! recalibrate model, if necessary
    void recalibrate() const { calculate(); }

    //! force recalibration of model
    virtual void forceRecalculate() { recalculate(); }

    //! if false is returned, the model does not require a recalibration
    virtual bool requiresRecalibration() const = 0;
};

} // namespace data
} // namespace ore
