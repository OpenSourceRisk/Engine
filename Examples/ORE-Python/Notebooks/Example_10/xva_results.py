"""
XVA Aggregation and Postprocess Results

Copyright (C) 2026 AcadiaSoft, Inc.
All rights reserved.

This script demonstrates the full aggregation and post-processing Python bindings. It runs an XVA workflow on a
multi-currency swap portfolio and extracts:

  - Trade-level and netting-set-level exposure profiles
  - XVA scalars (CVA, DVA, FBA, FCA, COLVA, CollateralFloor)
  - Allocated exposures and XVA

Prerequisites:
  - Python 3.10+
  - ORE Python module (pip install open-source-risk-engine)
"""

import sys
import os
from typing import Dict, List

# Ensure the working directory is the example folder
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
os.chdir(SCRIPT_DIR)

# Add parent directory for utilities module
sys.path.insert(0, os.path.join(SCRIPT_DIR, ".."))

from ORE import (  # noqa: E402
    OREApp,
    Parameters,
)


def run_ore(config_path: str = "Input/ore.xml") -> "OREApp":
    """
    Load ORE parameters and run the analytics engine.

    Parameters:
        config_path (str): Relative path to the ORE master config.

    Returns:
        OREApp: The completed OREApp instance with results.
    """
    params = Parameters()
    params.fromFile(config_path)
    app = OREApp(params)
    app.run()

    errors = app.getErrors()
    print(f"Run time: {app.getRunTime():.2f} sec")
    print(f"Errors:   {len(errors)}")
    if errors:
        for e in errors:
            print(f"  ERROR: {e}")
    return app


def display_trade_exposure_profiles(post_process, trade_ids: List[str]) -> None:
    """
    Extract and display trade-level exposure profiles from PostProcess.

    Parameters:
        post_process: ORE PostProcess object.
        trade_ids (List[str]): List of trade identifiers.
    """
    print("\n" + "=" * 70)
    print("TRADE-LEVEL EXPOSURE PROFILES")
    print("=" * 70)

    for tid in trade_ids:
        epe = list(post_process.tradeEPE(tid))
        ene = list(post_process.tradeENE(tid))
        pfe = list(post_process.tradePFE(tid))
        ee_b = list(post_process.tradeEE_B(tid))

        print(f"\n--- Trade: {tid} ---")
        print(f"  EPE  (first 5): {epe[:5]}")
        print(f"  ENE  (first 5): {ene[:5]}")
        print(f"  PFE  (first 5): {pfe[:5]}")
        print(f"  EE_B (first 5): {ee_b[:5]}")
        print(f"  EPE_B (Basel):  {post_process.tradeEPE_B(tid):.6f}")
        print(f"  EEPE_B (Basel): {post_process.tradeEEPE_B(tid):.6f}")


def display_nettingset_exposure_profiles(
    post_process, netting_set_ids: List[str]
) -> None:
    """
    Extract and display netting-set-level exposure profiles.

    Parameters:
        post_process: ORE PostProcess object.
        netting_set_ids (List[str]): Netting set identifiers.
    """
    print("\n" + "=" * 70)
    print("NETTING-SET-LEVEL EXPOSURE PROFILES")
    print("=" * 70)

    for ns_id in netting_set_ids:
        epe = list(post_process.netEPE(ns_id))
        ene = list(post_process.netENE(ns_id))
        pfe = list(post_process.netPFE(ns_id))
        coll = list(post_process.expectedCollateral(ns_id))

        print(f"\n--- Netting Set: {ns_id} ---")
        print(f"  Net EPE (first 5):             {epe[:5]}")
        print(f"  Net ENE (first 5):             {ene[:5]}")
        print(f"  Net PFE (first 5):             {pfe[:5]}")
        print(f"  Expected Collateral (first 5): {coll[:5]}")
        print(f"  Net EPE_B (Basel):             "
              f"{post_process.netEPE_B(ns_id):.6f}")
        print(f"  Net EEPE_B (Basel):            "
              f"{post_process.netEEPE_B(ns_id):.6f}")


def display_xva_scalars_trade(
    post_process, trade_ids: List[str]
) -> None:
    """
    Display trade-level XVA scalar results (CVA, DVA, FBA, FCA, MVA).

    Parameters:
        post_process: ORE PostProcess object.
        trade_ids (List[str]): Trade identifiers.
    """
    print("\n" + "=" * 70)
    print("TRADE-LEVEL XVA SCALARS")
    print("=" * 70)

    header = (
        f"{'Trade':<12} {'CVA':>12} {'DVA':>12} "
        f"{'FBA':>12} {'FCA':>12} {'MVA':>12}"
    )
    print(header)
    print("-" * len(header))

    for tid in trade_ids:
        cva = post_process.tradeCVA(tid)
        dva = post_process.tradeDVA(tid)
        fba = post_process.tradeFBA(tid)
        fca = post_process.tradeFCA(tid)
        mva = post_process.tradeMVA(tid)
        print(
            f"{tid:<12} {cva:>12.2f} {dva:>12.2f} "
            f"{fba:>12.2f} {fca:>12.2f} {mva:>12.2f}"
        )


def display_xva_scalars_nettingset(
    post_process, netting_set_ids: List[str]
) -> None:
    """
    Display netting-set-level XVA scalar results.

    Parameters:
        post_process: ORE PostProcess object.
        netting_set_ids (List[str]): Netting set identifiers.
    """
    print("\n" + "=" * 70)
    print("NETTING-SET-LEVEL XVA SCALARS")
    print("=" * 70)

    for ns_id in netting_set_ids:
        print(f"\n--- Netting Set: {ns_id} ---")
        print(f"  CVA:              {post_process.nettingSetCVA(ns_id):>14.2f}")
        print(f"  DVA:              {post_process.nettingSetDVA(ns_id):>14.2f}")
        print(f"  FBA:              {post_process.nettingSetFBA(ns_id):>14.2f}")
        print(f"  FCA:              {post_process.nettingSetFCA(ns_id):>14.2f}")
        print(f"  MVA:              {post_process.nettingSetMVA(ns_id):>14.2f}")
        print(f"  COLVA:            "
              f"{post_process.nettingSetCOLVA(ns_id):>14.2f}")
        print(f"  CollateralFloor:  "
              f"{post_process.nettingSetCollateralFloor(ns_id):>14.2f}")
        print(f"  FBA (ex own SP):  "
              f"{post_process.nettingSetFBA_exOwnSP(ns_id):>14.2f}")
        print(f"  FCA (ex own SP):  "
              f"{post_process.nettingSetFCA_exOwnSP(ns_id):>14.2f}")
        print(f"  FBA (ex all SP):  "
              f"{post_process.nettingSetFBA_exAllSP(ns_id):>14.2f}")
        print(f"  FCA (ex all SP):  "
              f"{post_process.nettingSetFCA_exAllSP(ns_id):>14.2f}")


def display_allocated_exposures(
    post_process, trade_ids: List[str]
) -> None:
    """
    Display allocated trade-level exposure profiles and XVA.

    Parameters:
        post_process: ORE PostProcess object.
        trade_ids (List[str]): Trade identifiers.
    """
    print("\n" + "=" * 70)
    print("ALLOCATED EXPOSURES & XVA")
    print("=" * 70)

    for tid in trade_ids:
        alloc_epe = list(post_process.allocatedTradeEPE(tid))
        alloc_ene = list(post_process.allocatedTradeENE(tid))
        alloc_cva = post_process.allocatedTradeCVA(tid)
        alloc_dva = post_process.allocatedTradeDVA(tid)

        print(f"\n--- Trade: {tid} ---")
        print(f"  Allocated EPE (first 5): {alloc_epe[:5]}")
        print(f"  Allocated ENE (first 5): {alloc_ene[:5]}")
        print(f"  Allocated CVA: {alloc_cva:.2f}")
        print(f"  Allocated DVA: {alloc_dva:.2f}")


def display_summary(post_process) -> None:
    """
    Display a summary of identifiers and metadata from PostProcess.

    Parameters:
        post_process: ORE PostProcess object.
    """
    print("\n" + "=" * 70)
    print("POSTPROCESS SUMMARY")
    print("=" * 70)

    trade_ids = dict(post_process.tradeIds())
    netting_set_ids = dict(post_process.nettingSetIds())
    counterparty_map = dict(post_process.counterpartyId())

    print(f"  Trade IDs:        {trade_ids}")
    print(f"  Netting Set IDs:  {netting_set_ids}")
    print(f"  Counterparty Map: {counterparty_map}")

    portfolio = post_process.portfolio()
    if portfolio:
        print(f"  Portfolio trades: {portfolio.size()}")


def main() -> None:
    """Run the full XVA master results demonstration."""
    print("=" * 70)
    print("ORE XVA MASTER RESULTS — CAPSTONE EXAMPLE")
    print("ORE Aggregation & PostProcess Bindings Demonstration")
    print("=" * 70)

    # 1. Run ORE
    print("\n[1] Running ORE XVA workflow...")
    app = run_ore()

    # 2. Access the XVA analytic and get PostProcess
    print("\n[2] Accessing PostProcess results...")
    analytic = app.getAnalytic("XVA")
    post_process = analytic.postProcess()

    if post_process is None:
        print("ERROR: PostProcess not available. Check XVA config.")
        sys.exit(1)

    # 3. Display summary metadata
    display_summary(post_process)

    # 4. Get IDs for iteration
    trade_ids_map = dict(post_process.tradeIds())
    trade_ids = sorted(trade_ids_map.keys())
    netting_set_ids_map = dict(post_process.nettingSetIds())
    netting_set_ids = sorted(netting_set_ids_map.keys())

    # 5. Trade-level exposure profiles
    display_trade_exposure_profiles(post_process, trade_ids)

    # 6. Netting-set-level exposure profiles
    display_nettingset_exposure_profiles(post_process, netting_set_ids)

    # 7. Trade-level XVA scalars
    display_xva_scalars_trade(post_process, trade_ids)

    # 8. Netting-set-level XVA scalars
    display_xva_scalars_nettingset(post_process, netting_set_ids)

    # 9. Allocated exposures
    display_allocated_exposures(post_process, trade_ids)

    print("\n" + "=" * 70)
    print("XVA MASTER RESULTS — COMPLETE")
    print("=" * 70)


if __name__ == "__main__":
    main()
