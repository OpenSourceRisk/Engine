/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Mangament Ltd.
*/

/*! \file analyticcclgmfxoptionengine.hpp
    \brief analytic cc lgm fx option engine
*/

#ifndef quantext_cclgm_fxoptionengine_hpp
#define quantext_cclgm_fxoptionengine_hpp

#include <qle/models/crossassetmodel.hpp>
#include <ql/instruments/vanillaoption.hpp>

namespace QuantExt {

class AnalyticCcLgmFxOptionEngine : public VanillaOption::engine {
  public:
    AnalyticCcLgmFxOptionEngine(const boost::shared_ptr<CrossAssetModel> &model,
                                const Size foreignCurrency);
    void calculate() const;

    /*! if cache is enabled, the integrals independent of fx
      volatility are cached, which can speed up calibtration;
      remember to flush the cache when the ir parameters
      change, this can be done by another call to cache */
    void cache(bool enable = false);

    /*! the actual option price calculation, exposed to public,
      since it is useful to directly use the core computation
      sometimes */
    Real value(const Time t0, const Time t,
               const boost::shared_ptr<StrikedTypePayoff> payoff,
               const Real domesticDiscount, const Real fxForward) const;

  private:
    const boost::shared_ptr<CrossAssetModel> model_;
    const Size foreignCurrency_;
    bool cacheEnabled_;
    mutable bool cacheDirty_;
    mutable Real cachedIntegrals_;
};

// inline

inline void AnalyticCcLgmFxOptionEngine::cache(bool enable) {
    cacheEnabled_ = enable;
    cacheDirty_ = true;
}

} // namespace QuantExt

#endif
