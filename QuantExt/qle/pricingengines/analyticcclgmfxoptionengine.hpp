/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Mangament Ltd.

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

/*! \file analyticcclgmfxoptionengine.hpp
    \brief analytic cc lgm fx option engine
*/

#ifndef quantext_cclgm_fxoptionengine_hpp
#define quantext_cclgm_fxoptionengine_hpp

#include <qle/models/xassetmodel.hpp>
#include <ql/instruments/vanillaoption.hpp>

namespace QuantExt {

class AnalyticCcLgmFxOptionEngine : public VanillaOption::engine {
    AnalyticCcLgmFxOptionEngine(const boost::shared_ptr<XAssetModel> &model,
                                const Size foreignCurrency);
    void calculate() const;

  private:
    const boost::shared_ptr<XAssetModel> model_;
    const Size foreignCurrency_;
};

} // namespace QuantExt

#endif
