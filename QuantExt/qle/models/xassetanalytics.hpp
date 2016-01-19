/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2016 Quaternion Risk Management Ltd.
*/

/*! \file xassetanalytics.hpp
    \brief analytics for the cross asset model
*/

#ifndef quantext_xasset_analytics_hpp
#define quantext_xasset_analytics_hpp

#include <ql/types.hpp>

using namespace QuantLib;

namespace QuantExt {

class XAssetModel;

namespace XAssetAnalytics {

/*! ir state expectation, part that is independent of current state */
Real ir_expectation_1(const XAssetModel *model, const Size i, const Time t0,
                      const Real dt);

/*! ir state expecation, part that is dependent on current state */
Real ir_expectation_2(const XAssetModel *model, const Size,
                      const Real zi_0);

/*! fx state expectation, part that is independent of current state */
Real fx_expectation_1(const XAssetModel *model, const Size i, const Time t0,
                      const Real dt);

/*! fx state expectation, part that is dependent on current state */
Real fx_expectation_2(const XAssetModel *model, const Size i, const Time t0,
                      const Real xi_0, const Real zi_0, const Real z0_0,
                      const Real dt);

/*! ir-ir covariance */
Real ir_ir_covariance(const XAssetModel *model, const Size i, const Size j,
                      const Time t0, Time dt);

/*! ir-fx covariance */
Real ir_fx_covariance(const XAssetModel *model, const Size i, const Size j,
                      const Time t0, Time dt);

/*! fx-fx covariance */
Real fx_fx_covariance(const XAssetModel *model, const Size i, const Size j,
                      const Time t0, Time dt);

/*! computation of integrals for analytic ir-fx moments,
    the integration bounds are given by a and b while the integrand is
    specified by indicators hi, hj, alphai, alphaj, sigmai, sigmaj
    which can be null (factor not present) or an integer specifying
    the ir or fx component; in the implementation we often use
    the alias na for null to improve readibility */
Real integral(const XAssetModel *model, const Size hi, const Size hj,
              const Size alphai, const Size alphaj, const Size sigmai,
              const Size sigmaj, const Real a, const Real b);

/*! generic integrand for analytic ir-fx moments */
Real integral_helper(const XAssetModel *model, const Size hi, const Size hj,
                     const Size alphai, const Size alphaj, const Size sigmai,
                     const Size sigmaj, const Real t);

} // namespace XAssetAnalytics

} // namesapce QuantExt

#endif
