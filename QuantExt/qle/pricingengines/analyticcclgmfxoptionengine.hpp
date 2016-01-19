/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Mangament Ltd.
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
  public:
    AnalyticCcLgmFxOptionEngine(const boost::shared_ptr<XAssetModel> &model,
                                const Size foreignCurrency);
    void calculate() const;

  private:
    const boost::shared_ptr<XAssetModel> model_;
    const Size foreignCurrency_;
};

} // namespace QuantExt

#endif
