# American Monte Carlo Examples

The cases in this section demonstrate how to use American Monte Carlo Simulation (AMC)
to generate exposures in ORE.

## Benchmarking against "classic" exposure simulation

We run both AMC and classic simulation on a small IR/FX portfolio, almost vanilla, that
consists of a Bermudan Swaption, Single and Cross Currency Swaps, FX Swap and FX Option,
and we compare the resulting AMC vs. classic exposures.

Run with <code>python run_benchmark.py</code>

## Scripted Bermudan Swaption and LPI Swap

The scripted trade framework works with AMC too, demonstrated here with a scripted Bermudan Swaption
and an LPI Swap. See the payoffs defined in scriptlibrary.xml. 

Run with <code>python run_scriptedberm.py</code>

## FX TaRF

The FxTaRF product is implemented using the scripted trade framework "under the hood" with the payoff script
embedded into C++, so that it is neither explicit in the trade XML nor in the script library.

Run with <code>python run_fxtarf.py</code>

## Forward Bond

Another "vanilla" product added that can be tackled with AMC exposure generation.

Run with <code>python run_forwardbond.py</code>

## Overlapping Close-Out Grid

Demonstrates the case where the primary and close-out grid overlap, with a daily simulation grid
up to 15 days; not allowed before ORE v13

Run with <code>python run_overlapping.py</code>

## Scenario Statistics

TODO: Add description here and in the User Guide.

Run with <code>python run_scenariostatistics.py</code>

See the user guide for a discussion of each case and expected results.

