#! /usr/bin/env Rscript
#
# Copyright (C) 2016 Quaternion Risk Management Ltd.
# All rights reserved.
#
# don't forget to chmod +x
# this skript takes trade-identifier or 'all' as argument from command line
args = commandArgs(TRUE)
system.time({
require(data.table)

dat = fread("Data/cube.csv")
dat$Date = as.Date(dat$Date)
ins = unique(dat$Id)
id = args[[1]]
request_all = (id == 'all')
request_instrument = (id %in% ins)
stopifnot(request_all || request_instrument)

bPlot = function(id) boxplot(Value ~ DateIndex, dat[Id == id], main = id, border = "black", plot = T)

file = paste0('Rplots/', id,'.pdf')
#par(mfrow = c(2, 5))
pdf(file)
if (request_all) sapply(ins, bPlot) else if (request_instrument) bPlot(args[[1]])
dev.off()
system(paste('open', file))

# save cube.Rdata
save(dat, bPlot, file = "Data/cube.RData")
})
