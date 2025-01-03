#ifndef quantext_analytic_hw_swaption_engine_hpp
#define quantext_analytic_hw_swaption_engine_hpp

#include <ql/instruments/swaption.hpp>
#include <ql/cashflows/floatingratecoupon.hpp>
#include <ql/cashflows/fixedratecoupon.hpp>
#include <qle/models/crossassetmodel.hpp>


namespace QuantExt {

class AnalyticHwSwaptionEngine : public GenericEngine<Swaption::arguments, Swaption::results>,
                                public PiecewiseConstantHelper1{

public:
    AnalyticHwSwaptionEngine(const Array& t, const Swaption& swaption,
                            const QuantLib::ext::shared_ptr<HwModel>& model,
                             const Handle<YieldTermStructure>& discountCurve = Handle<YieldTermStructure>());       
    
    void calculate() const;

private:

    QuantLib::ext::shared_ptr<HwModel> model_;
    QuantLib::ext::shared_ptr<IrHwParametrization> p_;
    Handle<YieldTermStructure> c_;
    Swaption swaption;

    std::vector<QuantLib::ext::shared_ptr<FixedRateCoupon>> fixedLeg_;
    std::vector<QuantLib::ext::shared_ptr<FloatingRateCoupon>> floatingLeg_;

    Real s(const Time t, const Array& x) const;
    Real a(const Time t, const Array& x) const;
    Array q(const QuantLib::Time t, const QuantLib::Array& x) const;
    Real v() const;
};

}

#endif