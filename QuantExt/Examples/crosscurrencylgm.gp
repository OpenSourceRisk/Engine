unset key

cd "/Users/peter/openxva/QuantExt/Examples"

# fx
plot for [n=0:4] 'mcpaths.dat' u 1:(column(2+4*n)) w l
# eur
plot for [n=0:4] 'mcpaths.dat' u 1:(column(3+4*n)) w l
# usd
plot for [n=0:4] 'mcpaths.dat' u 1:(column(4+4*n)) w l

# usd domestic measure (anderer seed !)
plot for [n=0:4] 'mcpaths.dat' u 1:(column(5+4*n)) w l

# fx process
plot for [n=0:4] 'mcpaths.dat' u 1:(column(2+4*n)) w l

# eur and usd lgm process
plot for [n=0:0] 'mcpaths.dat' u 1:(column(3+4*n)) w l lt 1, for [n=0:0] '' u 1:(column(4+4*n)) w l lt 2