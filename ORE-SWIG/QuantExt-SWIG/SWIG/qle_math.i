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

#endif
