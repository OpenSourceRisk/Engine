Example using experimental xva cg engine.

1) ore_ad.xml         : XVA-CG engine with backward AD CVA Sensitivities
2) ore_bump.xml       : same, but using bump and revalue
3) ore_gpu.xml        : same, but using external device (basic cpu by default, replace by your gpu)
4) ore_cgcube.xml     : same, but using cube generation mode with legacy post processor
5) ore_cgcube_gpu.xml : same, but using cube generation + external device (as in 3), with legacy post processor
6) ore_amc_legacy     : AMC legacy engine with comparable setup as in 1) - 4)

Differences between 1) and 2) are driven by:

- Indicator derivatives (from the calculation step EPE = max( E, 0 ) = 1_{E>0} x E) – I think these should be turned off in our context here, since we are interested in T0 – expectations (CVA) ultimately. The calculation of indicator derivatives (when necessary) is notoriously difficult.

- Approximation error of conditional expectations E( | F) calculated by polynomial regression models. The regression models are trained under the base and bump scenario and generate different errors under each. With AAD we only look at the base scenario. This effect is hard to eliminate in the testing.

- Number of paths, i.e. the usual MC error (I looked at 232k in addition to the original 8k samples)

- Bump shift size, 1E-7 for IR curve and normal swaption vol actually gives better match than the original 1E-4
