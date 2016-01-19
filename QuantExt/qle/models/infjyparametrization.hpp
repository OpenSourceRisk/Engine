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

namespace QuantExt {

class InflationJYParametrization : public Parametrization {
  public:
    InflationJYParametrization(
        const boost::shared_ptr<IrLgm1fParametrization> real,
        const boost::shared_ptr<FxBsParametrization> inflIndex);
};

} // namespace QuantExt

#endif
