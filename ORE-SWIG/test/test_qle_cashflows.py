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
