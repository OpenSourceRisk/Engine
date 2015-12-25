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

/*! \file parametrization.hpp
    \brief base class for model parametrizations
*/

#ifndef quantext_model_parametrization_hpp
#define quantext_model_parametrization_hpp

#include <ql/qldefines.hpp>

namespace QuantExt {

class Parametrization {
  public:
    enum Model {
        IR_LGM1F,
        FXEQD_BS,
        INF_JY,
        INF_DK,
        CRD_LGM1F,
        CRD_BK,
        CRD_CIR,
        CRD_PK,
        COM_GB
    };
    /*! interface */
    virtual Model class() const = 0;
    virtual Currency currency() const = 0; 
    virtual Size size() const = 0;
    virtual std::vector<Size> sectionStarts() const = 0;
    virtual Array& times() const;
};

class IrLgm1fParametrization : Parametrization {
  public:
    /*! interface */
    virtual Real zeta(const Time t) const = 0;
    virtual Real H(const Time t) const = 0;
    /*! inspectors */
    virtual Real alpha(const Time t) const;
    virtual Real Hprime(const Time t) const;
    virtual Real Hprime2(const Time t) const;
    virtual Real hullWhiteSigma(Time t) const;
    virtual Real hullWhiteKappa(Time t) const;
    /*! Parametrization interface implementation */
    Size factors() const { return 1; }
};

class FxEquityBsParametrization : Parametrization {
  public:
    /*! interface */
    virtual Real variance(const Time t) const = 0;
    /*! inspectors */
    virtual Real sigma(const Time t) const;
    virtual Real stdDevitation(const Time t) const;
    /*! Parametrization interface implementation */
    Size factors() const { return 1; }
};

class InflationJYParametrization : Parametrization {
  public:
    InflationJYParametrization(
        const boost::shared_ptr<IrLgm1fParametrization> nominal,
        const boost::shared_ptr<IrLgm1fParametrization> real,
        const boost::shared_ptr<FxEquityBsParametrization> inflIndex);
    virtual Real rho_n_r(const Time t) const = 0;
    virtual Real rho_n_i(const Time t) const = 0;
    virtual Real rho_r_i(const Time t) const = 0;
    /*! Parametrization interface implementation */
    Size factors() const { return 2; }
};

/*! do we need this explicitly or is the JY parametrization enough ? */
// class InflationDKParametrization : Parametrization {
//   public:
//     /*! interface */
//     virtual Real zeta_i(const Time t) const = 0;
//     virtual Real H_i(const Time t) const = 0;
//     virtual Real zeta_n(const Time t) const = 0;
//     virtual Real H_n(const Time t) const = 0;
//     virtual Real rho_n_i(const Time t) const = 0;
//     /*! inspectors */
//     virtual Real alpha_i(const Time t) const;
//     virtual Real Hprime_i(const Time t) const;
//     virtual Real Hprime2_i(const Time t) const;
//     virtual Real alpha_n(const Time t) const;
//     virtual Real Hprime_n(const Time t) const;
//     virtual Real Hprime2_n(const Time t) const;
//     /*! Parametrization interface implementation */
//     Size factors() const { return 2; }
// };

class CreditLgm1fParametrization : Parametrization {
  public:
    /*! interface */
    virtual Real zeta(const Time t) const = 0;
    virtual Real H(const Time t) const = 0;
    /*! inspectors */
    virtual Real alpha(const Time t) const;
    virtual Real Hprime(const Time t) const;
    virtual Real Hprime2(const Time t) const;
    virtual Real hullWhiteSigma(Time t) const;
    virtual Real hullWhiteKappa(Time t) const;
    /*! Parametrization interface implementation */
    Size factors() const { return 1; }
};

class CreditCirParametrization : Parametrization {
  public:
    /*! interface */
    virtual Real a(const Time t) const = 0;
    virtual Real sigma(const Time t) const = 0;
    virtual Real theta(const Time t) const = 0;
    /*! Parametrization interface implementation */
    Size factors() const { return 1; }
};

class CreditBKParametrization : Parametrization {
  public:
    /*! interface */
    virtual Real a(const Time t) const = 0;
    virtual Real sigma(const Time t) const = 0;
    /*! Parametrization interface implementation */
    Size factors() const { return 1; }
};

/*! we leave this for later implementation */
class CreditPKParametrization : Parametrization {};

class ComGabillonParametrization : Parametrization {
    /*! interface */
    virtual Real g(const Time t) const = 0;
    virtual Real sigma_s(const Time t) const = 0;
    virtual Real sigma_l(const Time t) const = 0;
    virtual Real rho_s_l(const Time t) const = 0;
    virtual Real kappa(const Time t) const = 0;
    /*! Parametrization interface implementation */
    Size factors() const { return 2; }
};

} // namespace QuantExt

#endif
