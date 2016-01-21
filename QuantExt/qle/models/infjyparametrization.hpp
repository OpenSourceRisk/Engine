/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file infjyparametrization.hpp
    \brief Inflation Jarrow Yildrim parametrization
*/

#ifndef quantext_inf_jy_parametrization_hpp
#define quantext_inf_jy_parametrization_hpp

#include <qle/models/irlgm1fparametrization.hpp>
#include <qle/models/fxbsparametrization.hpp>

namespace QuantExt {

class InfJYParametrization : public Parametrization {
  public:
    /*! the JY model uses a FX analogy which we exactly
        mirror here, i.e. the real rate model is an
        IrLgm1f model and the index is a FX spot rate
        in technical terms - the inflation index in
        the model actually represents the value of
        the index as of today minus availability lag
        which is 6w for EUHICPXT usually for example */
    InfJYParametrization(const boost::shared_ptr<IrLgm1fParametrization> real,
                         const boost::shared_ptr<FxBsParametrization> cpi);

    const boost::shared_ptr<IrLgm1fParametrization> real() const;
    const boost::shared_ptr<FxBsParametrization> cpi() const;

    void update() const;

  private:
    const boost::shared_ptr<IrLgm1fParametrization> real_;
    const boost::shared_ptr<FxBsParametrization> cpi_;
};

// inline

inline const boost::shared_ptr<IrLgm1fParametrization>
InfJYParametrization::real() const {
    return real_;
}

inline const boost::shared_ptr<FxBsParametrization>
InfJYParametrization::cpi() const {
    return cpi_;
}

inline void InfJYParametrization::update() const {
    real_->update();
    cpi_->update();
}

} // namespace QuantExt

#endif
