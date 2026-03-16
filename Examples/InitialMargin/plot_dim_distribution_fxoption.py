import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import statistics as stats
import math

def getSimmData(df, date, depth):
    # filter the date
    df2 = df[ df["AsOfDate"] == date ]
    # filter depth = 0, total SIMM
    df3 = df2[ df2["Depth"] == depth ]
    # filter the SIMM column
    x = df3["InitialMargin"]
    return x

def computeBins(min_val, max_val, desired_bin_size):
    min_boundary = -1.0 * (min_val % desired_bin_size - min_val)
    max_boundary = max_val - max_val % desired_bin_size + desired_bin_size
    n_bins = int((max_boundary - min_boundary) / desired_bin_size) + 1
    bins = np.linspace(min_boundary, max_boundary, n_bins)
    return bins

# Regression
df_0 = pd.read_csv("Output/Dim2/AmcReg/dim_cube.csv")
# Benchmark I
df_1 = pd.read_csv("Output/DimValidation/simm_cube.csv")
# Benchmark II
df_2 = pd.read_csv("Output/Dim2/SimmAnalytic/dim_cube.csv")
# Dynamic SIMM
df = pd.read_csv("Output/Dim2/AmcCg/dim_cube.csv")

asofDates = df["AsOfDate"].unique()
print("dates:", asofDates)

depths = df["Depth"].unique()
print("depths:", depths)

#dates = ["2030-02-10"]
dates = ["2029-02-10"]
#dates = ["2028-02-10"]
#dates = ["2027-02-10"]
#dates = ["2026-02-10"]
#dates = ["2025-03-10"]
bins = 40
stdbins = 10

#fig = plt.figure(figsize=plt.figaspect(0.4))
fig = plt.figure(figsize=plt.figaspect(0.75))

for d in dates:
    x = getSimmData(df, d, 0) 
    x_0 = getSimmData(df_0, d, 0) 
    x_1 = getSimmData(df_1, d, 0) 
    x_2 = getSimmData(df_2, d, 0)
    # force common bins using benchmark 2 as reference
    mini = min(x_2)
    std = stats.stdev(x_2)
    width = std / stdbins
    upperLimit = max(x_2) + std
    lowerLimit = mini - std
    bins = computeBins(mini, maxi, width)
    print("Series length for date", d, ":", len(x))
    plt.hist(x, density=True, bins=bins, color="red", alpha=0.75)
    plt.hist(x_2, density=True, bins=bins, histtype="step",linewidth=2,color="black")
    plt.xlim(xmin=lowerLimit, xmax=upperLimit)
    plt.ylim(ymax=6e-5)

#plt.legend(frameon=True, shadow=True)
plt.xlabel("SIMM")
plt.ylabel("Density")
plt.title("SIMM Distribution")

#plt.show()

file = "Output/mpl_simm_distribution.pdf"
plt.savefig(file)
plt.close()


# Plot error and compute rmse across all paths

#fig = plt.figure(figsize=plt.figaspect(0.4))
fig = plt.figure(figsize=plt.figaspect(0.75))

for d in dates:
    x = getSimmData(df, d, 0)    
    x_2 = getSimmData(df_2, d, 0)
    x = x.tolist()
    x_2 = x_2.tolist()

    z = []
    rmse = 0
    mean = 0;
    var = 0
    n = len(x)
    for i in range (0, n):
        z.append(x[i] - x_2[i])
        print(i, x[i], x_2[i], x[i] - x_2[i])
        rmse += (x[i] - x_2[i])**2
        mean += x_2[i]
        var += x_2[i] * x_2[i]
    mean = mean/n
    var = var/n - mean**2
    std = math.sqrt(var)
    rmse = math.sqrt(rmse/n)

    maxi = max(x_2)
    mini = min(x_2)
    std = stats.stdev(x_2)
    width = std / stdbins
    upperLimit = maxi + std
    lowerLimit = mini - std
    bins = computeBins(lowerLimit, upperLimit, width)

    plt.hist(z, density=True, bins=bins, color="blue", alpha=0.5, label="SIMM Error")
    plt.hist(x, density=True, bins=bins, color="red", label="SIMM", alpha=0.75)
    plt.hist(x_2, density=True, bins=bins, histtype="step", linewidth=2, color="black", label="Benchmark")
    plt.xlim(xmin=-30000, xmax=90000)
    plt.ylim(ymax=1.5e-4)

plt.legend(frameon=True, shadow=True)
plt.xlabel("SIMM, Error")
plt.ylabel("Density")
plt.title("SIMM Error Distribution")

print("mean:", mean)
print("std: ", std)
print("rmse:", rmse)

file = "Output/mpl_simm_error.pdf"
plt.savefig(file)
plt.close()
