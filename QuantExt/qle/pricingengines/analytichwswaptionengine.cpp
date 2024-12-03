#include <qle/pricingengines/analytichwswaptionengine.hpp>
#include <boost/bind/bind.hpp>

namespace QuantExt {

AnalyticHwSwaptionEngine::AnalyticHwSwaptionEngine(const Array& t,
                                                    const Swaption& swaption,
                                                    const ext::shared_ptr<HwModel>&model,
                                                    const Handle<YieldTermStructure>& discountCurve)
    : GenericEngine<Swaption::arguments, Swaption::results>(), PiecewiseConstantHelper1(t), 
    model_(model), p_(model->parametrization()),
    c_(discountCurve.empty() ? p_->termStructure() : discountCurve),
    swaption(swaption){
        const Leg& origFixedLeg = swaption.underlying()->fixedLeg();
        for (const auto& cf : origFixedLeg) {
            ext::shared_ptr<FixedRateCoupon> fixedCoupon = ext::dynamic_pointer_cast<FixedRateCoupon>(cf);
            if (fixedCoupon) {
                fixedLeg_.push_back(fixedCoupon);
            }
        }
        const Leg& origFloatingLeg = swaption.underlying()->floatingLeg();
        for (const auto& cf : origFloatingLeg) {
            ext::shared_ptr<FloatingRateCoupon> coupon = ext::dynamic_pointer_cast<FloatingRateCoupon>(cf);
            if (coupon) {
                floatingLeg_.push_back(coupon); 
            }
        }
        registerWith(model);
        registerWith(c_);
}


Real AnalyticHwSwaptionEngine::s(const Time t, const Array& x) const {
    Real floatingLegNPV = 0.0;
    Real fixedLegNPV = 0.0;

    // Calculate NPV of floating leg
    for (auto const& coupon : floatingLeg_) {
        Time paymentTime = c_->timeFromReference(coupon->date());
        Real discountFactor = model_->discountBond(t, paymentTime, x, c_);
        floatingLegNPV += coupon->amount() * discountFactor;
    }
    // Calculate NPV of fixed leg
    for (auto const& coupon : fixedLeg_) {
        Time paymentTime = c_->timeFromReference(coupon->date());
        Real discountFactor = model_->discountBond(t, paymentTime, x, c_);
        fixedLegNPV += coupon->amount() * discountFactor;
        
    }
    return floatingLegNPV - fixedLegNPV;
}

Real AnalyticHwSwaptionEngine::a(const Time t, const Array& x) const{
    Real annuity = 0.0;
    for (auto const& coupon : fixedLeg_){
        Time paymentTime = c_->timeFromReference(coupon->date());
        Real discountFactor = model_->discountBond(t, paymentTime, x, c_);
        annuity += coupon->accrualPeriod() * discountFactor;
    }
    return annuity; 
}

Array AnalyticHwSwaptionEngine::q(const Time t, const Array& x) const {
    Size n = p_->n();
    Array q(n, 0.0);

    Date startDate = swaption.underlying()->fixedSchedule().startDate();
    Time T0 = c_->timeFromReference(startDate);
    Date endDate = swaption.underlying()->fixedSchedule().endDate();
    Time TN = c_->timeFromReference(endDate);

    Real P_T0 = model_->discountBond(t, T0, x, c_);
    Real P_TN = model_->discountBond(t, TN, x, c_);
    
    // calculate G_j(t, T0) and G_j(t, T_N)
    for (Size j = 0; j < n; ++j) {
        Real G_j_T0 = p_->g(t, T0)[j];
        Real G_j_TN = p_->g(t, TN)[j];
        q[j] = -(P_T0 * G_j_T0 - P_TN * G_j_TN) / a(t, x);
    }

    for (Size j = 0; j < n; ++j) {
        Real sum = 0.0;
        for (const auto& coupon : fixedLeg_) {
            Time T_i = c_->timeFromReference(coupon->date());
            Real P_Ti = model_->discountBond(t, T_i, x, c_);
            Real G_j_Ti = model_->parametrization()->g(t, T_i)[j];
            sum += coupon->accrualPeriod() * P_Ti * G_j_Ti;
        }
        q[j] -= (s(t, x) * sum) / a(t, x);
    }

    return q;
}


Real AnalyticHwSwaptionEngine::v() const{
    Date startDate = swaption.underlying()->fixedSchedule().startDate();
    Time T0 = c_->timeFromReference(startDate);
    
    auto integrand = [this](Real t) -> Real {
        Array x(p_->n(), 0.0); 
        QL_REQUIRE(p_->sigma_x(t).columns() == q(t,x).size(), "sigma_x and q arrays must be of the same size to perform element-wise multiplication");
        Array product = p_->sigma_x(t) * q(t,x);
        return std::pow(Norm2(product), 2);
    };

    for (Size i = 0; i < y_->size(); ++i) {
        y_->setParam(i, inverse(integrand(t_[i])));
    }
    PiecewiseConstantHelper1::update();
    Real integral = this->int_y_sqr(T0);

    return integral;
}

void AnalyticHwSwaptionEngine::calculate() const {
    QL_REQUIRE(arguments_.settlementType == Settlement::Physical, "cash-settled swaptions are not supported ...");
    
    Date reference = p_->termStructure()->referenceDate();
    Date expiry = arguments_.exercise->dates().back();

    if (expiry <= reference) {
        // swaption is expired, possibly generated swap is not
        // valued by this engine, so we set the npv to zero
        results_.value = 0.0;
        return;
    }
    //TODO: fix where should x come from?
    Array x(p_->n(), 0.0);

    NormalDistribution pdf;
    CumulativeNormalDistribution cdf;

    const auto& underlyingSwap = swaption.underlying();
    Real strikePrice = underlyingSwap->fixedRate();

    Real s0 = s(0, x);
    Real a0 = a(0, x);
    Real c = strikePrice; 
    Real d = (s0 - c) / sqrt(v());
    results_.value = a0 * ((s0 - c) * cdf(d) + sqrt(v()) * pdf(d));
}

}