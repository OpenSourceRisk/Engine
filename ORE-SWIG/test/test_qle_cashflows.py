"""
 Copyright (C) 2026 AcadiaSoft, Inc.
 All rights reserved.
"""

import ORE as ore


def test_bma_cashflow_wrappers_construct():
    """Construct the BMA coupon and Black pricer wrappers."""
    payment_date = ore.Date(15, ore.January, 2027)
    start_date = ore.Date(15, ore.January, 2026)
    end_date = ore.Date(15, ore.January, 2027)
    bma_index = ore.BMAIndex()
    underlying = ore.AverageBMACoupon(
        payment_date, 1_000_000.0, start_date, end_date, bma_index)
    coupon = ore.CappedFlooredAverageBMACoupon(
        underlying, 0.05, 0.0)
    pricer = ore.BlackAverageBMACouponPricer(
        ore.OptionletVolatilityStructureHandle())

    assert coupon.underlying() is not None
    assert coupon.isCapped()
    assert coupon.isFloored()
    assert pricer is not None


def test_cmb_and_zero_fixed_coupon_wrappers_construct():
    """Construct and inspect CMB and zero-fixed coupon wrappers."""
    payment_date = ore.Date(15, ore.January, 2027)
    start_date = ore.Date(15, ore.January, 2026)
    end_date = ore.Date(15, ore.January, 2027)
    cmb_index = ore.ConstantMaturityBondIndex(
        "Test CMB", ore.Period(10, ore.Years))
    cmb_coupon = ore.CmbCoupon(
        payment_date, 1_000_000.0, start_date, end_date, 2, cmb_index)
    zero_coupon = ore.ZeroFixedCoupon(
        payment_date, 1_000_000.0, 0.02, ore.Actual365Fixed(),
        [start_date, end_date], ore.Compounded, True)

    assert cmb_coupon.bondIndex() is not None
    assert zero_coupon.nominal() == 1_000_000.0
    assert zero_coupon.compounding() == ore.Compounded


def test_cashflow_utility_and_wrapper_bindings() -> None:
    """Exercise QuantExt cashflow utilities and wrapper types."""
    start_date = ore.Date(15, ore.January, 2026)
    end_date = ore.Date(15, ore.January, 2027)
    cashflow = ore.SimpleCashFlow(100.0, end_date)
    leg = [cashflow]

    scaled_cashflow = ore.ScaledCashFlow(2.0, cashflow)
    typed_cashflow = ore.TypedCashFlow(
        100.0, end_date, ore.TypedCashFlow.Type_Interest
    )

    assert scaled_cashflow.amount() == 200.0
    assert typed_cashflow.type() == ore.TypedCashFlow.Type_Interest
    assert ore.QLECashFlows.sumCashflows(
        leg, start_date, end_date
    ) == 100.0
    assert hasattr(ore, "QLESetCouponPricer")
    assert hasattr(ore, "QLESetCouponPricers")


def test_fx_linked_cashflow_wrappers() -> None:
    """Construct and inspect the FX-linked cashflow wrapper family."""
    evaluation_date = ore.Date(15, ore.January, 2026)
    fixing_date = ore.Date(14, ore.January, 2026)
    payment_date = ore.Date(15, ore.January, 2027)
    ore.Settings.instance().evaluationDate = evaluation_date

    fx_quote = ore.SimpleQuote(1.2)
    fx_index = ore.FxIndex(
        "FX::USDEUR",
        0,
        ore.USDCurrency(),
        ore.EURCurrency(),
        ore.TARGET(),
        ore.RelinkableQuoteHandle(fx_quote),
    )
    fx_index.addFixing(fixing_date, 1.2)

    underlying = ore.FixedRateCoupon(
        payment_date,
        1_000.0,
        0.02,
        ore.Actual365Fixed(),
        evaluation_date,
        payment_date,
    )
    fixed_coupon = ore.FixedRateFXLinkedNotionalCoupon(
        fixing_date, 1_000.0, fx_index, underlying
    )
    assert fixed_coupon.nominal() == 1_200.0
    assert fixed_coupon.rate() == 0.02
    assert abs(fixed_coupon.amount() - 24.0) < 1e-12
    assert fixed_coupon.clone(fx_index) is not None

    average = ore.AverageFXLinkedCashFlow(
        payment_date, [fixing_date], 1_000.0, fx_index
    )
    assert average.amount() == 1_200.0
    assert average.fxRate() == 1.2
    assert average.fixings()[fixing_date] == 1.2

    typed = ore.FXLinkedTypedCashFlow(
        payment_date,
        fixing_date,
        1_000.0,
        fx_index,
        ore.TypedCashFlow.Type_Fee,
    )
    assert typed.amount() == 1_200.0
    assert typed.type() == ore.TypedCashFlow.Type_Fee


def test_brl_cdi_coupon_pricer_attach_and_amount() -> None:
    """Attach BRLCdiCouponPricer and evaluate a future BRL CDI coupon."""
    evaluation_date = ore.Date(15, ore.January, 2026)
    ore.Settings.instance().evaluationDate = evaluation_date
    curve = ore.FlatForward(evaluation_date, 0.10, ore.Actual365Fixed())
    index = ore.BRLCdi(ore.YieldTermStructureHandle(curve))
    coupon = ore.QLEOvernightIndexedCoupon(
        ore.Date(30, ore.January, 2026),
        1_000.0,
        ore.Date(20, ore.January, 2026),
        ore.Date(30, ore.January, 2026),
        index,
        1.0,
        0.0,
        ore.Date(),
        ore.Date(),
        ore.Business252(),
        False,
        False,
        ore.Period(0, ore.Days),
        0,
        ore.nullInt(),
        ore.Date(),
        ore.Date(),
        True,
    )
    pricer = ore.BRLCdiCouponPricer()
    coupon.setPricer(pricer)
    assert coupon.amount() > 0.0


def test_range_accrual_pricer_by_call_spread_attach_and_amount() -> None:
    """Attach RangeAccrualPricerByCallSpread and evaluate a range coupon."""
    evaluation_date = ore.Date(15, ore.January, 2026)
    ore.Settings.instance().evaluationDate = evaluation_date
    curve = ore.FlatForward(evaluation_date, 0.03, ore.Actual365Fixed())
    index = ore.Euribor6M(ore.YieldTermStructureHandle(curve))
    calendar = ore.TARGET()
    start = ore.Date(20, ore.January, 2026)
    end = ore.Date(20, ore.April, 2026)
    observations = ore.Schedule(
        start,
        end,
        ore.Period(1, ore.Months),
        calendar,
        ore.ModifiedFollowing,
        ore.ModifiedFollowing,
        ore.DateGeneration.Forward,
        False,
    )
    coupon = ore.RangeAccrualFloatersCoupon(
        ore.Date(22, ore.April, 2026),
        1_000.0,
        index,
        start,
        end,
        2,
        ore.Actual365Fixed(),
        1.0,
        0.0,
        ore.Date(),
        ore.Date(),
        observations,
        0.0,
        0.10,
    )
    vol = ore.ConstantOptionletVolatility(
        evaluation_date,
        calendar,
        ore.Following,
        0.20,
        ore.Actual365Fixed(),
    )
    pricer = ore.RangeAccrualPricerByCallSpread(
        ore.OptionletVolatilityStructureHandle(vol)
    )
    coupon.setPricer(pricer)
    assert coupon.amount() > 0.0


def test_range_accrual_pricer_by_call_spread_default_eps() -> None:
    """Construct RangeAccrualPricerByCallSpread using default eps (1e-4)."""
    pricer = ore.RangeAccrualPricerByCallSpread(
        ore.OptionletVolatilityStructureHandle()
    )
    assert pricer is not None


def test_nonstandard_yoy_pricer_wrappers_construct() -> None:
    """Construct all concrete nonstandard YoY inflation pricer wrappers."""
    nominal_curve = ore.FlatForward(
        ore.Date(15, ore.January, 2026), 0.03, ore.Actual365Fixed()
    )
    nominal_handle = ore.YieldTermStructureHandle(nominal_curve)

    pricers = [
        ore.NonStandardBlackYoYInflationCouponPricer(nominal_handle),
        ore.NonStandardUnitDisplacedBlackYoYInflationCouponPricer(
            nominal_handle
        ),
        ore.NonStandardBachelierYoYInflationCouponPricer(nominal_handle),
    ]

    assert all(pricer.nominalTermStructure() is not None for pricer in pricers)


def test_standard_yoy_coupon_and_leg_wrappers_construct() -> None:
    """Construct the QuantExt standard YoY coupon and leg wrappers."""
    start = ore.Date(15, ore.January, 2026)
    end = ore.Date(15, ore.January, 2027)
    schedule = ore.Schedule(
        start,
        end,
        ore.Period(3, ore.Months),
        ore.TARGET(),
        ore.ModifiedFollowing,
        ore.ModifiedFollowing,
        ore.DateGeneration.Forward,
        False,
    )
    index = ore.YoYInflationIndex(
        "TEST-YY",
        ore.CustomRegion("Test", "ZZ"),
        False,
        ore.Monthly,
        ore.Period(2, ore.Months),
        ore.USDCurrency(),
    )
    coupon = ore.QLEYoYInflationCoupon(
        end,
        1_000.0,
        start,
        end,
        2,
        index,
        ore.Period(3, ore.Months),
        ore.CPI.AsIndex,
        ore.Actual365Fixed(),
    )
    leg = ore.QLEYoYInflationLeg(
        schedule=schedule,
        calendar=ore.TARGET(),
        index=index,
        observationLag=ore.Period(3, ore.Months),
        interpolation=ore.CPI.AsIndex,
        notionals=[1_000.0],
        paymentDayCounter=ore.Actual365Fixed(),
    )

    assert coupon is not None
    assert len(leg) == 4


def test_floating_annuity_wrappers_construct() -> None:
    """Construct floating annuity coupon and nominal wrappers."""
    evaluation_date = ore.Date(15, ore.January, 2026)
    ore.Settings.instance().evaluationDate = evaluation_date
    curve = ore.FlatForward(evaluation_date, 0.03, ore.Actual365Fixed())
    index = ore.Euribor3M(ore.YieldTermStructureHandle(curve))
    start = ore.Date(15, ore.January, 2026)
    end = ore.Date(15, ore.April, 2026)
    previous = ore.FixedRateCoupon(
        end,
        1_000.0,
        0.02,
        ore.Actual365Fixed(),
        start,
        end,
    )
    coupon = ore.FloatingAnnuityCoupon(
        1_000.0,
        False,
        previous,
        ore.Date(15, ore.July, 2026),
        end,
        ore.Date(15, ore.July, 2026),
        2,
        index,
    )
    nominal = ore.FloatingAnnuityNominal(coupon)

    assert coupon.previousNominal() == 1_000.0
    assert nominal.date() == end
    assert len(ore.makeFloatingAnnuityNominalLeg([coupon])) == 1


def test_interpolated_ibor_coupon_pricer_and_accessors() -> None:
    """Construct and price an interpolated Ibor coupon."""
    evaluation_date = ore.Date(15, ore.January, 2026)
    ore.Settings.instance().evaluationDate = evaluation_date
    curve = ore.FlatForward(evaluation_date, 0.03, ore.Actual365Fixed())
    curve_handle = ore.YieldTermStructureHandle(curve)
    short_index = ore.Euribor3M(curve_handle)
    long_index = ore.Euribor6M(curve_handle)
    fixing_date = ore.Date(13, ore.January, 2026)
    short_index.addFixing(fixing_date, 0.03)
    long_index.addFixing(fixing_date, 0.03)
    index = ore.InterpolatedIborIndex(short_index, long_index, 90)
    coupon = ore.InterpolatedIborCoupon(
        ore.Date(15, ore.July, 2026),
        1_000.0,
        ore.Date(15, ore.January, 2026),
        ore.Date(15, ore.July, 2026),
        2,
        index,
        1.0,
        0.0,
        ore.Date(),
        ore.Date(),
        ore.Actual365Fixed(),
        False,
        ore.Date(),
        short_index,
    )
    coupon.setPricer(ore.BlackInterpolatedIborCouponPricer())

    # interpolatedIborIndex() should return the index we passed in
    assert coupon.interpolatedIborIndex() is not None
    # amount and rate are computed via the flat 3% curve
    assert coupon.amount() > 0.0
    assert coupon.rate() > 0.0


def test_black_interpolated_ibor_coupon_pricer_enum_values() -> None:
    """BlackInterpolatedIborCouponPricer enum values are accessible."""
    assert hasattr(ore.BlackInterpolatedIborCouponPricer, "Black76")
    assert hasattr(ore.BlackInterpolatedIborCouponPricer, "BivariateLognormal")
    pricer = ore.BlackInterpolatedIborCouponPricer()
    # capletVolatility returns an empty Handle (valid, not None)
    assert pricer.capletVolatility() is not None


def test_ibor_fra_coupon_amount() -> None:
    """Construct an Ibor FRA coupon and evaluate its amount."""
    evaluation_date = ore.Date(15, ore.January, 2026)
    ore.Settings.instance().evaluationDate = evaluation_date
    curve = ore.FlatForward(evaluation_date, 0.03, ore.Actual365Fixed())
    index = ore.Euribor3M(ore.YieldTermStructureHandle(curve))
    coupon = ore.IborFraCoupon(
        ore.Date(15, ore.April, 2026),
        ore.Date(15, ore.July, 2026),
        1_000.0,
        index,
        0.02,
    )
    coupon.setPricer(ore.BlackIborCouponPricer())

    # With 3% forecast and 2% strike the FRA payoff is positive
    amt = coupon.amount()
    assert isinstance(amt, float)
    assert amt > 0.0


def test_leg_builder_wrappers_construct() -> None:
    """Construct the three newly exposed QuantExt leg builders."""
    start = ore.Date(15, ore.January, 2026)
    end = ore.Date(15, ore.January, 2027)
    schedule = ore.Schedule(
        start,
        end,
        ore.Period(3, ore.Months),
        ore.TARGET(),
        ore.ModifiedFollowing,
        ore.ModifiedFollowing,
        ore.DateGeneration.Forward,
        False,
    )
    curve = ore.FlatForward(start, 0.03, ore.Actual365Fixed())
    index = ore.Euribor3M(ore.YieldTermStructureHandle(curve))

    sub_periods = ore.SubPeriodsLeg(
        schedule=schedule,
        index=index,
        notionals=[1_000.0],
        paymentDayCounter=ore.Actual365Fixed(),
    )
    trs = ore.TRSLeg(
        valuationDates=[start, end],
        paymentDates=[end],
        notional=1_000.0,
        index=index,
    )

    equity = ore.EquityIndex2(
        "TEST-EQ",
        ore.TARGET(),
        ore.USDCurrency(),
        ore.QuoteHandle(ore.SimpleQuote(100.0)),
        ore.YieldTermStructureHandle(curve),
        ore.YieldTermStructureHandle(curve),
    )
    equity_margin = ore.EquityMarginLeg(
        schedule=schedule,
        equityCurve=equity,
        couponRates=[0.02],
        couponDayCounter=ore.Actual365Fixed(),
        notionals=[1_000.0],
        paymentDayCounter=ore.Actual365Fixed(),
    )

    assert len(sub_periods) == 4
    assert len(trs) == 1
    assert len(equity_margin) == 4
