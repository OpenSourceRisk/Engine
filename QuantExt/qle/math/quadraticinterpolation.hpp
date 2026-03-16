/*
  Copyright (C) 2021 Skandinaviska Enskilda Banken AB (publ)
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

 /*! \file quadraticinterpolation.hpp
     \brief quadratic interpolation between discrete points
 */

 #ifndef quantext_quadratic_interpolation_hpp
 #define quantext_quadratic_interpolation_hpp

 #include <ql/math/interpolation.hpp>
 #include <ql/math/matrix.hpp>
 #include <vector>

 namespace QuantExt {
 using namespace QuantLib;

     namespace detail {
         template<class I1, class I2> class QuadraticInterpolationImpl;
     }

     //! %Quadratic interpolation between discrete points
     /*! \ingroup interpolations
         \warning See the Interpolation class for information about the
                  required lifetime of the underlying data.
     */
     class QuadraticInterpolation : public Interpolation {
       public:
         /*! \pre the \f$ x \f$ values must be sorted. */
         template <class I1, class I2>
         QuadraticInterpolation(const I1& xBegin, const I1& xEnd,
                                const I2& yBegin,
                                Real x_mul = 1, Real x_offset = 0,
                                Real y_mul = 1, Real y_offset = 0,
                                Size skip = 0) {
             impl_ = ext::shared_ptr<Interpolation::Impl>(new
                 detail::QuadraticInterpolationImpl<I1,I2>(
                     xBegin + skip, xEnd, yBegin + skip,
                     x_mul, x_offset, y_mul, y_offset));
             impl_->update();
         }
         template <class I1, class I2>
         std::vector<Real> lambdas() const {
             ext::shared_ptr<detail::QuadraticInterpolationImpl<I1,I2> > p =
                 ext::dynamic_pointer_cast<
                     detail::QuadraticInterpolationImpl<I1,I2> >(impl_);
             QL_REQUIRE(p, "unable to cast impl to "
                           "QuadraticInterpolationImpl<I1,I2>");
             return p->lambdas();
         }
     };

     //! %Quadratic-interpolation factory and traits
     /*! \ingroup interpolations */
     class Quadratic {
       public:
         Quadratic(Real x_mul = 1, Real x_offset = 0,
                   Real y_mul = 1, Real y_offset = 0,
                   Size skip = 0)
         : x_mul_(x_mul), x_offset_(x_offset),
           y_mul_(y_mul), y_offset_(y_offset),
           skip_(skip) {}
         template <class I1, class I2>
         Interpolation interpolate(const I1& xBegin, const I1& xEnd,
                                   const I2& yBegin) const {
             return QuadraticInterpolation(
                 xBegin, xEnd, yBegin,
                 x_mul_, x_offset_, y_mul_, y_offset_, skip_);
         }
         template <class I1, class I2>
         QuantLib::ext::shared_ptr<QuadraticInterpolation> interpolatePtr(
             const I1& xBegin, const I1& xEnd,
             const I2& yBegin) const {
             return QuantLib::ext::make_shared<QuadraticInterpolation>(
                 xBegin, xEnd, yBegin,
                 x_mul_, x_offset_, y_mul_, y_offset_, skip_);
         }
         static const bool global = false;
         static const Size requiredPoints = 1;
         Real x_mul_;
         Real x_offset_;
         Real y_mul_;
         Real y_offset_;
         Size skip_;
     };

     namespace detail {

         template <class I1, class I2>
         class QuadraticInterpolationImpl
             : public Interpolation::templateImpl<I1,I2> {
           public:
             QuadraticInterpolationImpl(const I1& xBegin, const I1& xEnd,
                                        const I2& yBegin,
                                        Real x_mul = 1,
                                        Real x_offset = 0,
                                        Real y_mul = 1,
                                        Real y_offset = 0)
             : Interpolation::templateImpl<I1,I2>(xBegin, xEnd, yBegin,
                                                  Quadratic::requiredPoints),
               n_(static_cast<int>(xEnd-xBegin)),
               x_mul_(x_mul), x_offset_(x_offset),
               y_mul_(y_mul), y_offset_(y_offset),
               x_(std::vector<Real>(n_)),
               y_(std::vector<Real>(n_+1)),
               lambdas_(std::vector<Real>(n_)) {}
             std::vector<Real> lambdas() const {
                 return lambdas_;
             }
             void update() override {

                 for(Size i=0; i < n_; ++i) {
                     x_[i] = this->xBegin_[i] * x_mul_ + x_offset_;
                     y_[i] = this->yBegin_[i] * y_mul_ + y_offset_;
                 }
                 y_[n_] = 0;

                 // Return if x <= 0 or y = 0
                 // Error will be thrown when calling value or
                 // derivative functions
                 for(Size i=0; i < n_; ++i) {
                     if(x_[i] <= 0 || QuantLib::close_enough(y_[i], 0.0)) {
                         p_ = Null<Real>();
                         return;
                     }
                 }

                 Matrix q(n_+1, n_+1, 0.0);
                 for(Size i=0; i<n_; ++i) {
                     q[i][0] = q[n_][i+1] = x_[i];

                     for(Size j=0; j<i; ++j) {
                         q[i][j+1] += std::pow(x_[i] - x_[j], 3) / 6.0;
                     }
                     Time t = -std::pow(x_[i], 3) / 6.0;
                     for(Size j=1; j<n_+1; ++j) {
                         q[i][j] += t;
                     }
                 }
                 Matrix q_inv = QuantLib::inverse(q);
                 Array l(y_.begin(), y_.end());

                 Array lambdaArray = q_inv * l;
                 lambdas_ = std::vector<Real>(lambdaArray.begin(),
                                              lambdaArray.end());

                 p_ = 0;
                 for(Size i=1; i < n_+1; ++i) {
                     p_ += lambdaArray[i];
                 }
             }
             Real value(Real x) const override {
                 QL_REQUIRE(p_ != Null<Real>(), "failed to calibrate lambda");
                 x = x * x_mul_ + x_offset_;
                 Real l = lambdas_[0] * x;
                 Real b = 0;
                 for (Size i=0; i<n_ && x_[i]<x; ++i) {
                     b += lambdas_[i+1] * std::pow(x - x_[i], 3);
                 }
                 l += (b - p_ * std::pow(x, 3)) / 6.0;
                 return (l - y_offset_) / y_mul_;
             }
             Real primitive(Real x) const override {
                 QL_FAIL("QuadraticInterpolation primitive is not implemented");
             }
             Real derivative(Real x) const override {
                 QL_REQUIRE(p_ != 0.0, "failed to calibrate lambda");
                 x = x * x_mul_ + x_offset_;
                 Real l = lambdas_[0];
                 Real b = 0;
                 for (Size i=0; i<n_ && x_[i]<x; ++i) {
                     b += lambdas_[i+1] * std::pow(x - x_[i], 2);
                 }
                 l += (b - p_ * std::pow(x, 2)) / 2.0;
                 return l / y_mul_;
             }
             Real secondDerivative(Real x) const override {
                 QL_REQUIRE(p_ != 0.0, "failed to calibrate lambda");
                 x = x * x_mul_ + x_offset_;
                 Real l = 0;
                 Real b = 0;
                 for (Size i=0; i<n_ && x_[i]<x; ++i) {
                     b += lambdas_[i+1] * (x - x_[i]);
                 }
                 l += (b - p_* x);
                 return l / y_mul_;;
             }
           private:
             const Size n_;
             Real p_;
             Real x_mul_;
             Real x_offset_;
             Real y_mul_;
             Real y_offset_;
             std::vector<Real> x_;
             std::vector<Real> y_;
             std::vector<Real> lambdas_;
         };

     }

 }

 #endif
