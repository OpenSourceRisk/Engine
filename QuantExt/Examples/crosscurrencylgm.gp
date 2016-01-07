unset key

cd "/Users/peter/openxva/QuantExt/Examples"

# fx
plot for [n=0:4] 'mcpaths.dat' u 1:(column(2+4*n)) w l
# eur
plot for [n=0:4] 'mcpaths.dat' u 1:(column(3+4*n)) w l
# usd
plot for [n=0:4] 'mcpaths.dat' u 1:(column(4+4*n)) w l

# usd domestic measure (different path generation!)
plot for [n=0:4] 'mcpaths.dat' u 1:(column(5+4*n)) w l

# fx process
plot for [n=0:4] 'mcpaths.dat' u 1:(column(2+4*n)) w l

# eur and usd lgm process
plot for [n=0:0] 'mcpaths.dat' u 1:(column(3+4*n)) w l lt 1, for [n=0:0] '' u 1:(column(4+4*n)) w l lt 2


# compare exact vs euler paths
# fx, 1st path
plot 'mcpaths_euler.dat' u 1:2 w l, 'mcpaths_exact.dat' u 1:2 w l
# difference of the both fx paths
plot '< paste mcpaths_euler.dat mcpaths_exact.dat' u 1:(log($2-$7)) w l
# eur, 1st path
plot 'mcpaths_euler.dat' u 1:3 w l, 'mcpaths_exact.dat' u 1:3 w p
# usd, 1st path
plot 'mcpaths_euler.dat' u 1:4 w l, 'mcpaths_exact.dat' u 1:4 w p