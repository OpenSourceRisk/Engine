/*
 Copyright (C) 2026 AcadiaSoft, Inc.
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

#ifndef qle_math_i
#define qle_math_i

%include interpolation.i

// Forward declarations
%{
#include <qle/math/constantinterpolation.hpp>
#include <qle/math/flatextrapolation.hpp>
#include <qle/math/flatextrapolation2d.hpp>
#include <qle/math/quadraticinterpolation.hpp>
#include <qle/math/logquadraticinterpolation.hpp>
#include <sstream>

struct SafeInterpolationHelper : public QuantLib::Interpolation {
    static QuantLib::ext::shared_ptr<Impl> get_impl(const QuantLib::Interpolation& interp) {
        return ((const SafeInterpolationHelper&)(interp)).impl_;
    }
};

class SafeInterpolationWrapper : public QuantLib::Interpolation {
    std::vector<QuantLib::Real> x_, y_;
public:
    template <typename F>
    SafeInterpolationWrapper(const std::vector<QuantLib::Real>& x,
                             const std::vector<QuantLib::Real>& y,
                             const F& builder)
    : x_(x), y_(y) {
        QuantLib::Interpolation inner = builder(x_, y_);
        this->impl_ = SafeInterpolationHelper::get_impl(inner);
    }
};

struct SafeInterpolation2DHelper : public QuantLib::Interpolation2D {
    static QuantLib::ext::shared_ptr<Impl> get_impl(const QuantLib::Interpolation2D& interp) {
        return ((const SafeInterpolation2DHelper&)(interp)).impl_;
    }
};

class SafeInterpolation2DWrapper : public QuantLib::Interpolation2D {
    std::vector<QuantLib::Real> x_, y_;
    QuantLib::Matrix z_;
public:
    template <typename F>
    SafeInterpolation2DWrapper(const std::vector<QuantLib::Real>& x,
                               const std::vector<QuantLib::Real>& y,
                               const QuantLib::Matrix& z,
                               const F& builder)
    : x_(x), y_(y), z_(z) {
        QuantLib::Interpolation2D inner = builder(x_, y_, z_);
        this->impl_ = SafeInterpolation2DHelper::get_impl(inner);
    }
};

struct SafeConstantInterpolationData {
    QuantLib::Real y_;
    SafeConstantInterpolationData(QuantLib::Real y) : y_(y) {}
};

class SafeConstantInterpolation : private SafeConstantInterpolationData, public QuantExt::ConstantInterpolation {
public:
    SafeConstantInterpolation(QuantLib::Real y)
    : SafeConstantInterpolationData(y),
      QuantExt::ConstantInterpolation(y_) {}
};

struct SafeQuadraticInterpolationData {
    std::vector<QuantLib::Real> x_, y_;
    SafeQuadraticInterpolationData(const std::vector<QuantLib::Real>& x, const std::vector<QuantLib::Real>& y)
    : x_(x), y_(y) {}
};

class SafeQuadraticInterpolation : private SafeQuadraticInterpolationData, public QuantExt::QuadraticInterpolation {
public:
    SafeQuadraticInterpolation(const std::vector<QuantLib::Real>& x,
                               const std::vector<QuantLib::Real>& y,
                               QuantLib::Real x_mul = 1, QuantLib::Real x_offset = 0,
                               QuantLib::Real y_mul = 1, QuantLib::Real y_offset = 0,
                               QuantLib::Size skip = 0)
    : SafeQuadraticInterpolationData(x, y),
      QuantExt::QuadraticInterpolation(x_.begin(), x_.end(), y_.begin(), x_mul, x_offset, y_mul, y_offset, skip) {}
};

struct SafeLogQuadraticInterpolationData {
    std::vector<QuantLib::Real> x_, y_;
    SafeLogQuadraticInterpolationData(const std::vector<QuantLib::Real>& x, const std::vector<QuantLib::Real>& y)
    : x_(x), y_(y) {}
};

class SafeLogQuadraticInterpolation : private SafeLogQuadraticInterpolationData, public QuantExt::LogQuadraticInterpolation {
public:
    SafeLogQuadraticInterpolation(const std::vector<QuantLib::Real>& x,
                                 const std::vector<QuantLib::Real>& y,
                                 QuantLib::Real x_mul = 1, QuantLib::Real x_offset = 0,
                                 QuantLib::Real y_mul = 1, QuantLib::Real y_offset = 0,
                                 QuantLib::Size skip = 0)
    : SafeLogQuadraticInterpolationData(x, y),
      QuantExt::LogQuadraticInterpolation(x_.begin(), x_.end(), y_.begin(), x_mul, x_offset, y_mul, y_offset, skip) {}
};

#include <qle/math/randomvariable.hpp>
#include <qle/math/randomvariablelsmbasissystem.hpp>
#include <ql/methods/montecarlo/lsmbasissystem.hpp>
%}

// ===== Base Interpolation Classes =====

%shared_ptr(QuantLib::Interpolation)
namespace QuantLib {
    class Interpolation {
      public:
        %rename(__call__) operator();
        Real operator()(Real x, bool allowExtrapolation = false) const;
        Real primitive(Real x, bool allowExtrapolation = false) const;
        Real derivative(Real x, bool allowExtrapolation = false) const;
        Real secondDerivative(Real x, bool allowExtrapolation = false) const;
        Real xMin() const;
        Real xMax() const;
        bool isInRange(Real x) const;
        void update();
        void enableExtrapolation(bool b = true);
        void disableExtrapolation(bool b = true);
        bool allowsExtrapolation() const;
    };
}

%shared_ptr(QuantLib::Interpolation2D)
namespace QuantLib {
    class Interpolation2D {
      public:
        %rename(__call__) operator();
        Real operator()(Real x, Real y, bool allowExtrapolation = false) const;
        Real xMin() const;
        Real xMax() const;
        Real yMin() const;
        Real yMax() const;
        bool isInRange(Real x, Real y) const;
        void update();
        void enableExtrapolation(bool b = true);
        void disableExtrapolation(bool b = true);
        bool allowsExtrapolation() const;
    };
}

// ===== Concrete Interpolation Classes =====

// ConstantInterpolation: returns constant value for all inputs
%shared_ptr(QuantExt::ConstantInterpolation)
%shared_ptr(SafeConstantInterpolation)
%rename(ConstantInterpolation) SafeConstantInterpolation;
class SafeConstantInterpolation : public QuantLib::Interpolation {
public:
    %rename(__call__) operator();
    SafeConstantInterpolation(QuantLib::Real y);
    QuantLib::Real operator()(QuantLib::Real x, bool allowExtrapolation = false) const;
};

// FlatExtrapolation: wraps an interpolation with flat extrapolation
%shared_ptr(QuantExt::FlatExtrapolation)
class QuantExt::FlatExtrapolation : public QuantLib::Interpolation {
public:
    %rename(__call__) operator();
    FlatExtrapolation(const ext::shared_ptr<QuantLib::Interpolation>& i);
    QuantLib::Real operator()(QuantLib::Real x, bool allowExtrapolation = false) const;
};

// QuadraticInterpolation: wraps quadratic interpolation safely
%shared_ptr(QuantExt::QuadraticInterpolation)
%shared_ptr(SafeQuadraticInterpolation)
%rename(QuadraticInterpolation) SafeQuadraticInterpolation;
class SafeQuadraticInterpolation : public QuantLib::Interpolation {
public:
    %rename(__call__) operator();
    SafeQuadraticInterpolation(const std::vector<QuantLib::Real>& x,
                               const std::vector<QuantLib::Real>& y,
                               QuantLib::Real x_mul = 1, QuantLib::Real x_offset = 0,
                               QuantLib::Real y_mul = 1, QuantLib::Real y_offset = 0,
                               QuantLib::Size skip = 0);
    QuantLib::Real operator()(QuantLib::Real x, bool allowExtrapolation = false) const;
};

// LogQuadraticInterpolation: wraps log-quadratic interpolation safely
%shared_ptr(QuantExt::LogQuadraticInterpolation)
%shared_ptr(SafeLogQuadraticInterpolation)
%rename(LogQuadraticInterpolation) SafeLogQuadraticInterpolation;
class SafeLogQuadraticInterpolation : public QuantLib::Interpolation {
public:
    %rename(__call__) operator();
    SafeLogQuadraticInterpolation(const std::vector<QuantLib::Real>& x,
                                 const std::vector<QuantLib::Real>& y,
                                 QuantLib::Real x_mul = 1, QuantLib::Real x_offset = 0,
                                 QuantLib::Real y_mul = 1, QuantLib::Real y_offset = 0,
                                 QuantLib::Size skip = 0);
    QuantLib::Real operator()(QuantLib::Real x, bool allowExtrapolation = false) const;
};

// ===== Interpolation Factories (struct-like) =====

// LinearFlat factory: creates linear interpolation with flat extrapolation
class QuantExt::LinearFlat {
public:
    LinearFlat();
    static const QuantLib::Size requiredPoints = 2;
};

%extend QuantExt::LinearFlat {
    ext::shared_ptr<QuantLib::Interpolation> interpolate_from_vectors(const std::vector<QuantLib::Real>& x,
                                                                      const std::vector<QuantLib::Real>& y) {
        return QuantLib::ext::make_shared<SafeInterpolationWrapper>(x, y, [self](const std::vector<QuantLib::Real>& x_vec, const std::vector<QuantLib::Real>& y_vec) {
            return self->interpolate(x_vec.begin(), x_vec.end(), y_vec.begin());
        });
    }
}

// LogLinearFlat factory: creates log-linear interpolation with flat extrapolation
class QuantExt::LogLinearFlat {
public:
    LogLinearFlat();
    static const QuantLib::Size requiredPoints = 2;
};

%extend QuantExt::LogLinearFlat {
    ext::shared_ptr<QuantLib::Interpolation> interpolate_from_vectors(const std::vector<QuantLib::Real>& x,
                                                                      const std::vector<QuantLib::Real>& y) {
        return QuantLib::ext::make_shared<SafeInterpolationWrapper>(x, y, [self](const std::vector<QuantLib::Real>& x_vec, const std::vector<QuantLib::Real>& y_vec) {
            return self->interpolate(x_vec.begin(), x_vec.end(), y_vec.begin());
        });
    }
}

// HermiteFlat factory: creates Hermite (Parabolic) interpolation with flat extrapolation
class QuantExt::HermiteFlat {
public:
    HermiteFlat();
    static const QuantLib::Size requiredPoints = 2;
};

%extend QuantExt::HermiteFlat {
    ext::shared_ptr<QuantLib::Interpolation> interpolate_from_vectors(const std::vector<QuantLib::Real>& x,
                                                                      const std::vector<QuantLib::Real>& y) {
        return QuantLib::ext::make_shared<SafeInterpolationWrapper>(x, y, [self](const std::vector<QuantLib::Real>& x_vec, const std::vector<QuantLib::Real>& y_vec) {
            return self->interpolate(x_vec.begin(), x_vec.end(), y_vec.begin());
        });
    }
}

// CubicFlat factory: creates cubic interpolation with flat extrapolation
class QuantExt::CubicFlat {
public:
    CubicFlat(QuantLib::CubicInterpolation::DerivativeApprox da = QuantLib::CubicInterpolation::Kruger,
              bool monotonic = false,
              QuantLib::CubicInterpolation::BoundaryCondition leftCondition = QuantLib::CubicInterpolation::SecondDerivative,
              QuantLib::Real leftConditionValue = 0.0,
              QuantLib::CubicInterpolation::BoundaryCondition rightCondition = QuantLib::CubicInterpolation::SecondDerivative,
              QuantLib::Real rightConditionValue = 0.0);
    static const QuantLib::Size requiredPoints = 2;
};

%extend QuantExt::CubicFlat {
    ext::shared_ptr<QuantLib::Interpolation> interpolate_from_vectors(const std::vector<QuantLib::Real>& x,
                                                                      const std::vector<QuantLib::Real>& y) {
        return QuantLib::ext::make_shared<SafeInterpolationWrapper>(x, y, [self](const std::vector<QuantLib::Real>& x_vec, const std::vector<QuantLib::Real>& y_vec) {
            return self->interpolate(x_vec.begin(), x_vec.end(), y_vec.begin());
        });
    }
}

// Constant factory: creates constant interpolations
class QuantExt::Constant {
public:
    Constant();
    static const QuantLib::Size requiredPoints = 1;
};

%extend QuantExt::Constant {
    ext::shared_ptr<QuantLib::Interpolation> interpolate(QuantLib::Real y) {
        return QuantLib::ext::make_shared<SafeConstantInterpolation>(y);
    }
}

// Quadratic factory: creates quadratic interpolations (template-based)
class QuantExt::Quadratic {
public:
    Quadratic(QuantLib::Real x_mul = 1, QuantLib::Real x_offset = 0,
              QuantLib::Real y_mul = 1, QuantLib::Real y_offset = 0,
              QuantLib::Size skip = 0);
    static const QuantLib::Size requiredPoints = 1;
};

%extend QuantExt::Quadratic {
    ext::shared_ptr<QuantLib::Interpolation> interpolate(const std::vector<QuantLib::Real>& x,
                                                          const std::vector<QuantLib::Real>& y) {
        return QuantLib::ext::make_shared<SafeInterpolationWrapper>(x, y, [self](const std::vector<QuantLib::Real>& x_vec, const std::vector<QuantLib::Real>& y_vec) {
            return self->interpolate(x_vec.begin(), x_vec.end(), y_vec.begin());
        });
    }
}

// LogQuadratic factory: creates log-quadratic interpolations (template-based)
class QuantExt::LogQuadratic {
public:
    LogQuadratic(QuantLib::Real x_mul = 1, QuantLib::Real x_offset = 0,
                 QuantLib::Real y_mul = 1, QuantLib::Real y_offset = 0,
                 QuantLib::Size skip = 0);
    static const QuantLib::Size requiredPoints = 2;
};

%extend QuantExt::LogQuadratic {
    ext::shared_ptr<QuantLib::Interpolation> interpolate(const std::vector<QuantLib::Real>& x,
                                                          const std::vector<QuantLib::Real>& y) {
        return QuantLib::ext::make_shared<SafeInterpolationWrapper>(x, y, [self](const std::vector<QuantLib::Real>& x_vec, const std::vector<QuantLib::Real>& y_vec) {
            return self->interpolate(x_vec.begin(), x_vec.end(), y_vec.begin());
        });
    }
}

// ===== 2D Interpolation Factories =====

// BilinearFlat: bilinear interpolation with flat extrapolation in 2D
class QuantExt::BilinearFlat {
public:
    BilinearFlat();
};

%extend QuantExt::BilinearFlat {
    ext::shared_ptr<QuantLib::Interpolation2D> interpolate_from_matrices(const std::vector<QuantLib::Real>& x,
                                                                          const std::vector<QuantLib::Real>& y,
                                                                          const QuantLib::Matrix& z) {
        return QuantLib::ext::make_shared<SafeInterpolation2DWrapper>(x, y, z, [self](const std::vector<QuantLib::Real>& x_vec, const std::vector<QuantLib::Real>& y_vec, const QuantLib::Matrix& z_mat) {
            return self->interpolate(x_vec.begin(), x_vec.end(), y_vec.begin(), y_vec.end(), z_mat);
        });
    }
}

// BicubicFlat: bicubic interpolation with flat extrapolation in 2D
class QuantExt::BicubicFlat {
public:
    BicubicFlat();
};

%extend QuantExt::BicubicFlat {
    ext::shared_ptr<QuantLib::Interpolation2D> interpolate_from_matrices(const std::vector<QuantLib::Real>& x,
                                                                          const std::vector<QuantLib::Real>& y,
                                                                          const QuantLib::Matrix& z) {
        return QuantLib::ext::make_shared<SafeInterpolation2DWrapper>(x, y, z, [self](const std::vector<QuantLib::Real>& x_vec, const std::vector<QuantLib::Real>& y_vec, const QuantLib::Matrix& z_mat) {
            return self->interpolate(x_vec.begin(), x_vec.end(), y_vec.begin(), y_vec.end(), z_mat);
        });
    }
}

// ===== RandomVariable and Filter Suite =====

%template(RandomVariableVector) std::vector<QuantExt::RandomVariable>;

namespace QuantExt {

    struct Filter {
        Filter();
        Filter(const Filter& r);
        explicit Filter(const Size n, const bool value = false);
        void clear();
        void set(const Size i, const bool v);
        void setAll(const bool v);
        void resetSize(const Size n);
        bool deterministic() const;
        void updateDeterministic();
        bool initialised() const;
        Size size() const;
        bool at(const Size i) const;
        void expand();
    };

    struct RandomVariable {
        RandomVariable();
        RandomVariable(const RandomVariable& r);
        explicit RandomVariable(const Size n, const Real value = 0.0, const Real time = Null<Real>());
        explicit RandomVariable(const Filter& f, const Real valueTrue = 1.0, const Real valueFalse = 0.0,
                                const Real time = Null<Real>());
        explicit RandomVariable(const std::vector<double>& data, const Real time = Null<Real>());
        explicit RandomVariable(const QuantLib::Array& data, const Real time = Null<Real>());

        void clear();
        void set(const Size i, const Real v);
        void setTime(const Real time);
        void setAll(const Real v);
        void resetSize(const Size n);

        bool deterministic() const;
        void updateDeterministic();
        bool initialised() const;
        bool isfinite() const;
        Size size() const;
        Real at(const Size i) const;
        Real time() const;
        void expand();
    };

    enum class RandomVariableRegressionMethod { QR, SVD };

    class RandomVariableLsmBasisSystem {
    private:
        RandomVariableLsmBasisSystem();
    public:
        static Real size(Size dim, Size order);
    };

    class RandomVariableStats {
    private:
        RandomVariableStats();
    public:
        static RandomVariableStats& instance();
        void reset();
        bool enabled;
        std::size_t data_ops;
        std::size_t calc_ops;
    };

    // Free functions under namespace QuantExt
    RandomVariable max(RandomVariable, const RandomVariable&);
    RandomVariable max(RandomVariable, const Real);
    RandomVariable max(const Real, RandomVariable);
    RandomVariable min(RandomVariable, const RandomVariable&);
    RandomVariable min(RandomVariable, const Real);
    RandomVariable min(Real, RandomVariable);
    RandomVariable pow(RandomVariable, const RandomVariable&);
    RandomVariable pow(RandomVariable, const Real);
    RandomVariable round(RandomVariable, const RandomVariable&);
    RandomVariable round(RandomVariable, const Real);
    RandomVariable abs(RandomVariable);
    RandomVariable exp(RandomVariable);
    RandomVariable frac(RandomVariable);
    RandomVariable log(RandomVariable);
    RandomVariable sqrt(RandomVariable);
    RandomVariable sin(RandomVariable);
    RandomVariable cos(RandomVariable);
    RandomVariable normalCdf(RandomVariable);
    RandomVariable normalPdf(RandomVariable);
    RandomVariable indicatorEq(RandomVariable, const RandomVariable&, const Real trueVal = 1.0, const Real falseVal = 0.0);
    RandomVariable indicatorGt(RandomVariable, const RandomVariable&, const Real trueVal = 1.0, const Real falseVal = 0.0,
                               const Real eps = 0.0);
    RandomVariable indicatorGeq(RandomVariable, const RandomVariable&, const Real trueVal = 1.0, const Real falseVal = 0.0,
                                const Real eps = 0.0);

    RandomVariable conditionalResult(const Filter&, RandomVariable, const RandomVariable&);

    void checkTimeConsistency(const RandomVariable& x, const RandomVariable& y);

    RandomVariable applyFilter(RandomVariable, const Filter&);
    RandomVariable applyInverseFilter(RandomVariable, const Filter&);

    RandomVariable expectation(const RandomVariable& r);
    RandomVariable variance(const RandomVariable& r);
    RandomVariable covariance(const RandomVariable& r, const RandomVariable& s);

    RandomVariable black(const RandomVariable& omega, const RandomVariable& t, const RandomVariable& strike,
                         const RandomVariable& forward, const RandomVariable& impliedVol);

    RandomVariable indicatorDerivative(const RandomVariable& x, const double eps);

    bool isDeterministicAndZero(const RandomVariable& x);
}

%extend QuantExt::Filter {
    bool __getitem__(Size i) const {
        return self->at(i);
    }
    void __setitem__(Size i, bool v) {
        self->set(i, v);
    }
    Filter __and__(const Filter& other) {
        return (*self) && other;
    }
    Filter __or__(const Filter& other) {
        return (*self) || other;
    }
    Filter __invert__() {
        return !(*self);
    }
    bool __eq__(const Filter& other) {
        return (*self) == other;
    }
    bool __ne__(const Filter& other) {
        return !((*self) == other);
    }
    std::string __str__() {
        std::ostringstream oss;
        oss << "Filter(size=" << self->size() << ", deterministic=" << (self->deterministic() ? "True" : "False") << ")";
        return oss.str();
    }
}

%extend QuantExt::RandomVariable {
    Real __getitem__(Size i) const {
        return self->at(i);
    }
    void __setitem__(Size i, Real v) {
        self->set(i, v);
    }
    RandomVariable __add__(const RandomVariable& other) {
        return (*self) + other;
    }
    RandomVariable __add__(Real other) {
        return (*self) + other;
    }
    RandomVariable __radd__(Real other) {
        return other + (*self);
    }
    RandomVariable __sub__(const RandomVariable& other) {
        return (*self) - other;
    }
    RandomVariable __sub__(Real other) {
        return (*self) - other;
    }
    RandomVariable __rsub__(Real other) {
        return other - (*self);
    }
    RandomVariable __mul__(const RandomVariable& other) {
        return (*self) * other;
    }
    RandomVariable __mul__(Real other) {
        return (*self) * other;
    }
    RandomVariable __rmul__(Real other) {
        return other * (*self);
    }
    RandomVariable __truediv__(const RandomVariable& other) {
        return (*self) / other;
    }
    RandomVariable __truediv__(Real other) {
        return (*self) / other;
    }
    RandomVariable __rtruediv__(Real other) {
        return other / (*self);
    }
    RandomVariable __neg__() {
        return -(*self);
    }
    RandomVariable __abs__() {
        return abs(*self);
    }
    
    RandomVariable& __iadd__(const RandomVariable& other) {
        *self += other;
        return *self;
    }
    RandomVariable& __iadd__(Real other) {
        *self += other;
        return *self;
    }
    RandomVariable& __isub__(const RandomVariable& other) {
        *self -= other;
        return *self;
    }
    RandomVariable& __isub__(Real other) {
        *self -= other;
        return *self;
    }
    RandomVariable& __imul__(const RandomVariable& other) {
        *self *= other;
        return *self;
    }
    RandomVariable& __imul__(Real other) {
        *self *= other;
        return *self;
    }
    RandomVariable& __itruediv__(const RandomVariable& other) {
        *self /= other;
        return *self;
    }
    RandomVariable& __itruediv__(Real other) {
        *self /= other;
        return *self;
    }

    Filter __lt__(const RandomVariable& other) {
        return (*self) < other;
    }
    Filter __le__(const RandomVariable& other) {
        return (*self) <= other;
    }
    Filter __gt__(const RandomVariable& other) {
        return (*self) > other;
    }
    Filter __ge__(const RandomVariable& other) {
        return (*self) >= other;
    }
    bool __eq__(const RandomVariable& other) {
        return (*self) == other;
    }
    bool __ne__(const RandomVariable& other) {
        return !((*self) == other);
    }

    std::vector<double> to_vector() const {
        return (std::vector<double>)(*self);
    }
    QuantLib::Array to_array() const {
        return (QuantLib::Array)(*self);
    }

    std::string __str__() {
        std::ostringstream oss;
        oss << "RandomVariable(size=" << self->size() << ", time=" << self->time() << ", deterministic=" << (self->deterministic() ? "True" : "False") << ")";
        return oss.str();
    }
}

%extend QuantExt::RandomVariableLsmBasisSystem {
    static std::vector<QuantExt::RandomVariable> evaluatePathBasis(QuantLib::Size order, QuantLib::LsmBasisSystem::PolynomialType type, const QuantExt::RandomVariable& rv) {
        auto basis = QuantExt::RandomVariableLsmBasisSystem::pathBasisSystem(order, type);
        std::vector<QuantExt::RandomVariable> res;
        res.reserve(basis.size());
        for (const auto& f : basis) {
            res.push_back(f(rv));
        }
        return res;
    }

    static std::vector<QuantExt::RandomVariable> evaluateMultiPathBasis(QuantLib::Size dim, QuantLib::Size order, QuantLib::LsmBasisSystem::PolynomialType type, const std::vector<QuantExt::RandomVariable>& rvs) {
        auto basis = QuantExt::RandomVariableLsmBasisSystem::multiPathBasisSystem(dim, order, type);
        std::vector<const QuantExt::RandomVariable*> ptrs = QuantExt::vec2vecptr(rvs);
        std::vector<QuantExt::RandomVariable> res;
        res.reserve(basis.size());
        for (const auto& f : basis) {
            res.push_back(f(ptrs));
        }
        return res;
    }
}

%inline %{
namespace QuantExt {
    QuantLib::Matrix pcaCoordinateTransform(const std::vector<QuantExt::RandomVariable>& regressor, const QuantLib::Real varianceCutoff = 1E-5) {
        return QuantExt::pcaCoordinateTransform(QuantExt::vec2vecptr(regressor), varianceCutoff);
    }
    
    std::vector<QuantExt::RandomVariable> applyCoordinateTransform(const std::vector<QuantExt::RandomVariable>& regressor, const QuantLib::Matrix& transform) {
        return QuantExt::applyCoordinateTransform(QuantExt::vec2vecptr(regressor), transform);
    }

    QuantLib::Array regressionCoefficients(
        const QuantExt::RandomVariable& r,
        const std::vector<QuantExt::RandomVariable>& basisValues,
        const QuantExt::Filter& filter = QuantExt::Filter(),
        const QuantExt::RandomVariableRegressionMethod method = QuantExt::RandomVariableRegressionMethod::QR) {
        
        std::vector<std::function<QuantExt::RandomVariable(const std::vector<const QuantExt::RandomVariable*>&)>> basisFn;
        for (Size j = 0; j < basisValues.size(); ++j) {
            QuantExt::RandomVariable val = basisValues[j];
            basisFn.push_back([val](const std::vector<const QuantExt::RandomVariable*>&) {
                return val;
            });
        }
        
        std::vector<const QuantExt::RandomVariable*> emptyRegressor;
        return QuantExt::regressionCoefficients(r, emptyRegressor, basisFn, filter, method);
    }

    QuantExt::RandomVariable conditionalExpectation(const std::vector<QuantExt::RandomVariable>& regressor,
                                                     const std::vector<QuantExt::RandomVariable>& basisValues,
                                                     const QuantLib::Array& coefficients) {
        QL_REQUIRE(basisValues.size() == coefficients.size(), "basisValues and coefficients size mismatch");
        QuantExt::RandomVariable res(regressor.empty() ? 0 : regressor[0].size(), 0.0);
        for (Size i = 0; i < basisValues.size(); ++i) {
            res += coefficients[i] * basisValues[i];
        }
        return res;
    }
}
%}

#endif
