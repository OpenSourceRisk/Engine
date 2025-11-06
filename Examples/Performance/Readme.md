# Performance

This section contains several aproaches to boost ORE's computational performance.


## Multi-threading

ORE's multi-threaded valuation engine works around QuantLib's thread-safety limitations. Parallelisation is achieved
by splitting the portfolio into several parts processed by separate threads. The example
portfolio used here consists of 8 identical Swaps, the number of threads is set to 2 (see nThreads in ore.xml).

Run with <code>python run_multithreading.py</code>

This case shows the effect on exposure simulation/XVA, but the multi-threaded valuation engine in ORE also supports other analytics such as general
sensitivity analysis or backtesting. 


## AAD and GPUs for Exotics Sensitivities

This case demonstrates the application of AAD and GPUs to accelerate Exotics sensitivity analysis. The portfolio comprises
- a European Equity Option
- an Equity Barrier Option
- an Equity Accumulator
- an Asian Equity Basket Option
- two FX TaRFs
all represented as Scripted Trades and priced using AMC.

Run with: <code>python run_sensi.py</code>

This executes four batches
- Bump & reval sensis as a benchmark, "legacy" AMC framework (19 sec on Apple M2 Max)
- Bump & reval sensis, Computation Graph framework (14 sec on Apple M2 Max)
- AAD sensis (2.2 sec on Apple M2 Max)
- Bump & reval sensis utilising a GPU (2.3 sec on Apple M2 Max with "OpenCL/Apple/Apple M2 Max" device) 

See the corresponding Juypter notebook included here with comparison of results and further comments: <code>python -m jupyterlab ore_sensi.ipynb & </code>


## AAD and GPUs for CVA Sensitivities

This case demonstrates a proof-of-concept implementation of CVA Sensitivity calculations. The portfolio is a single Swap.
Exposure is generated with AMC, and Exposure/XVA is based on ORE's Computation Graph (CG) implementation which is required
for AAD and as interface to external compute devices.
We run the following batches:
- Exposure with "legacy" AMC: As a reminder of the raw AMC performance in generating the Swap exposure (about 9 sec, Apple M2 Max)
- Benchmark sensi run (slow): Bump & reval sensitivities, Exposure and XVA are computed on the CG (about 48 sec, Apple M2 Max)
- AAD Sensi (should be significantly faster): AAD sensitivities, Exposure and XVA are computed on the CG (about 2 sec, Apple M2 Max) 
- GPU Sensi (work in progress): Bump & reval sensis using a GPU to parallelise processing, Exposure and XVA are computed on the CG;
set xvaCgExternalComputeDevice in ore_gpu.xml accordingly (55 sec on Apple M2 Max with the "OpenCL/Apple/Apple M2 Max" device); this
does not show any speedup (yet) against the benchmark above, because the dominating conditional expectation
calculations have not been ported to the GPU yet, still processed on the CPU; this is work in progress.

Run all batches with <code>python run_cvasensi.py</code>

Run the corresponding Juypter notebook included here: <code>python -m jupyterlab ore_cvasensi.ipynb & </code> 

Further batches inlcuded here test the effect of using the CG implementation on Exposure performance 
- CG Cube: Exposure with CG 
- Exposure with CG and GPU

Uncomment the corresponding lines in the <code>run_cvasensi.py</code> script and re-run.

