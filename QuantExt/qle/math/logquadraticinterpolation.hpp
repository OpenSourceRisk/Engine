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

 /*! \file logquadraticinterpolation.hpp
     \brief log-quadratic interpolation between discrete points
 */

 #ifndef quantext_log_quadratic_interpolation_hpp
 #define quantext_log_quadratic_interpolation_hpp

 #include <ql/math/interpolations/loginterpolation.hpp>
 #include <ql/utilities/dataformatters.hpp>
 #include <qle/math/quadraticinterpolation.hpp>

 namespace QuantExt {
 using namespace QuantLib;

     namespace detail {
         template<class I1, class I2, class I> class LogInterpolationImpl;
     }

     //! %log-quadratic interpolation between discrete points
     /*! \ingroup interpolations
         \warning See the Interpolation class for information about the
                  required lifetime of the underlying data.
     */
     class LogQuadraticInterpolation : public Interpolation {
       public:
         /*! \pre the \f$ x \f$ values must be sorted. */
         template <class I1, class I2>
         LogQuadraticInterpolation(const I1& xBegin, const I1& xEnd,
                                   const I2& yBegin,
                                   Real x_mul = 1, Real x_offset = 0,
                                   Real y_mul = 1, Real y_offset = 0,
                                   Size skip = 0) {
             impl_ = ext::shared_ptr<Interpolation::Impl>(new
                 QuantExt::detail::LogInterpolationImpl<I1, I2, Quadratic>(
                     xBegin, xEnd, yBegin,
                     Quadratic(x_mul, x_offset, y_mul, y_offset, skip)));
             impl_->update(); 
         }
         template <class I1, class I2>
         std::vector<Real> lambdas() const {
             typedef QuantExt::detail::LogInterpolationImpl<I1,I2,Quadratic>
                 Impl;
             ext::shared_ptr<Impl> p =
                 ext::dynamic_pointer_cast<Impl>(impl_);
             QL_REQUIRE(p, "unable to cast impl to "
                           "LogInterpolationImpl<I1,I2,Quadratic>");

             ext::shared_ptr<QuadraticInterpolation> p2 =
                 ext::dynamic_pointer_cast<QuadraticInterpolation>(
                     p->interpolation());
             QL_REQUIRE(p2, "unable to cast interpolation to "
                            "QuadraticInterpolation");

             return p2->lambdas<I1,I2>();
         }
     };

     //! log-quadratic interpolation factory and traits
     /*! \ingroup interpolations */
     class LogQuadratic {
       public:
         LogQuadratic(Real x_mul = 1, Real x_offset = 0,
                      Real y_mul = 1, Real y_offset = 0,
                      Size skip = 0)
         : x_mul_(x_mul), x_offset_(x_offset),
           y_mul_(y_mul), y_offset_(y_offset),
           skip_(skip) {}
         template <class I1, class I2>
         Interpolation interpolate(const I1& xBegin, const I1& xEnd,
                                   const I2& yBegin) const {
             return LogQuadraticInterpolation(
                 xBegin, xEnd, yBegin,
                 x_mul_, x_offset_, y_mul_, y_offset_, skip_);
         }
         static const bool global = false;
         static const Size requiredPoints = 2;
         Real x_mul_;
         Real x_offset_;
         Real y_mul_;
         Real y_offset_;
         Size skip_;
     };

     namespace detail {

         template <class I1, class I2, class Interpolator>
         class LogInterpolationImpl
             : public Interpolation::templateImpl<I1,I2> {
           public:
             LogInterpolationImpl(const I1& xBegin, const I1& xEnd,
                                  const I2& yBegin,
                                  const Interpolator& factory = Interpolator())
             : Interpolation::templateImpl<I1,I2>(xBegin, xEnd, yBegin,
                                                  Interpolator::requiredPoints),
               logY_(xEnd-xBegin) {
                 interpolation_ = factory.interpolatePtr(this->xBegin_,
                                                         this->xEnd_,
                                                         logY_.begin());
             }
             QuantLib::ext::shared_ptr<Interpolation> interpolation() const {
                 return interpolation_;
             }
             void update() override {
                 for (Size i=0; i<logY_.size(); ++i) {
                     QL_REQUIRE(this->yBegin_[i]>0.0,
                                "invalid value (" << this->yBegin_[i]
                                << ") at index " << i);
                     logY_[i] = std::log(this->yBegin_[i]);
                 }
                 interpolation_->update();
             }
             Real value(Real x) const override {
                 return std::exp((*interpolation_)(x, true));
             }
             Real primitive(Real) const override {
                 QL_FAIL("LogInterpolation primitive not implemented");
             }
             Real derivative(Real x) const override {
                 return value(x)*interpolation_->derivative(x, true);
             }
             Real secondDerivative(Real x) const override {
                 return value(x)*interpolation_->secondDerivative(x, true) +
                        derivative(x)*interpolation_->derivative(x, true);
             }
           private:
             std::vector<Real> logY_;
             QuantLib::ext::shared_ptr<Interpolation> interpolation_;
             Real multiplier_;
             Real offset_;
         };

     }

 }

 #endif
