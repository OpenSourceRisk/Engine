import sys, time, math
import pandas as pd

def checkErrorsAndRunTime(app):
    errors = app.getErrors()
    print ("Run time: %.2f sec" % app.getRunTime())       
    print ("Errors: %d" % len(errors))
    if len(errors) > 0:
        for e in errors:
            print(e)        

def writeList(lst):
    print()
    for r in lst:
        print("-", r)
        
def checkReportStructure(name, report):
    # These are the column types values and meaning, see PlainInMemoryReport definition 
    columnTypes = { 0: "Size",
                    1: "Real",
                    2: "string",
                    3: "Date",
                    4: "Period" }

    print("Report: ", name)
    print("columns:", report.columns())
    print("rows:   ", report.rows())
    print()
    print ("%-6s %-20s %-8s %-10s" % ("Column", "Header", "Type", "TypeString"))
    for i in range(0, report.columns()):
        print("%-6d %-20s %-8s %-10s" % (i,
                                         report.header(i),
                                         report.columnType(i),
                                         columnTypes[report.columnType(i)]))
        sys.stdout.flush()

# example: writeReport(report, [0, 1, 3, 5]);
def writeReport(report, columnNumbers):
    column = [None] * report.columns()
    for c in range(0, report.columns() - 1):
        typ = report.columnType(c)
        if typ == 0:
            column[c] = report.dataAsSize(c);
        elif typ == 1:
            column[c] = report.dataAsReal(c);
        elif typ == 2:
            column[c] = report.dataAsString(c);
        elif typ == 3:
            column[c] = report.dataAsDate(c);
        elif typ == 4:
            column[c] = report.dataAsPeriod(c);

    print()
    
    for i in range(0, report.rows()):
        if i == 0:
            for c in columnNumbers:
                if c == columnNumbers[-1]:
                    eol = "\n"
                else:
                    eol = ""
                print ("%-17s  " % report.header(c), end=eol)

        for c in columnNumbers:
            typ = report.columnType(c)
            if c == columnNumbers[-1]:
                eol = "\n"
            else:
                eol = ""
            if typ == 0:
                print ("%-17d  " % column[c][i], end=eol)
            elif typ == 1:
                print ("%-17.2f  " % column[c][i], end=eol)
            elif typ == 2:
                print ("%-17s  " % column[c][i], end=eol)
            elif typ == 3:
                print ("%-17s  " % column[c][i].ISO(), end=eol)
            elif typ == 4:
                print ("%-17s  " % column[c][i], end=eol)       

def format_report(report):
    report_headers = [[i, report.header(i), report.columnType(i)] for i in range(report.columns())]

    formatted_report = dict()
    for i, column_name, data_type in report_headers:
        if data_type == 0:
            formatted_report[column_name] = list(report.dataAsSize(i))
        if data_type == 1:
            formatted_report[column_name] = list(report.dataAsReal(i))
        if data_type == 2:
            formatted_report[column_name] = list(report.dataAsString(i))
        if data_type == 3:
            formatted_report[column_name] = [d.ISO() for d in report.dataAsDate(i)]
        if data_type == 4:
            formatted_report[column_name] = list(report.dataAsPeriod(i))
    
    formatted_report = pd.DataFrame(formatted_report)
    
    # formatted_report.loc[
    # (formatted_report['MarketObjectType']=='yieldCurve') \
    # & (formatted_report['MarketObjectId'].str.startswith('Equity')),
    # 'MarketObjectType'] = 'equityCurve'
    
    return formatted_report

def display_reports(reports, header_length=140):
    for report_name, report in reports.items():
        L = header_length
        l = len(report_name)
        s = int((L - l + 2) / 2 - 1)
        e = L - l - s - 2

        print("\n" + "*"*L)
        print("*"*s + " " + report_name + " " + "*"*e, sep="")
        print("*"*L + "\n")
        display(format_report(report))
        print()

def plotNpvPaths(netCubeReport, numberOfPaths):
    #utilities.checkReportStructure("netcube", cubeReport)
    dateIndexColumn = netCubeReport.dataAsSize(2)
    sampleColumn = netCubeReport.dataAsSize(4)
    depthColumn = netCubeReport.dataAsSize(5)
    valueColumn = netCubeReport.dataAsReal(6)
    # Loop over data and find unique dates, samples and depths
    dateSet = set()
    sampleSet = set()
    depthSet = set()
    for i in range(0, netCubeReport.rows()):
        dateSet.add(dateIndexColumn[i])
        sampleSet.add(sampleColumn[i])
        depthSet.add(depthColumn[i])
    print("dates:", len(dateSet))
    print("samples:", len(sampleSet))
    print("depths:", len(depthSet))
    #print(dateSet)
    #print(sampleSet)
    dates = list(dateSet)
    values = []
    for i in range(0, len(sampleSet)):
        values.append([0] * len(dateSet))
    
    for i in range(0, netCubeReport.rows()):
        d = dateIndexColumn[i]
        s = sampleColumn[i]
        v = valueColumn[i]
        if s == 0 & d == 0:
            for j in range(0, len(sampleSet)):
                values[j][0] = v
        else:
            values[s][d] = v

    #print(values[1])
    #for d in range(0, len(dateSet)):
    #    print(d, values[1][d], values[2][d], values[3][d])
    
    import matplotlib.pyplot as plt
    from matplotlib.gridspec import GridSpec

    fig = plt.figure(figsize=(12, 5))
    gs = GridSpec(nrows=1, ncols=1)
    ax0 = fig.add_subplot(gs[0, 0])

    for p in range(1, numberOfPaths+1):
        ax0.plot(dates, values[p], label='')
    ax0.set(xlabel='Time', ylabel='NPV')
    ax0.set_title('Selected NPV Paths')
    #ax0.legend()
    
    plt.show()
    
def getNpvScenarios(netCubeReport, dateIndex):
    #utilities.checkReportStructure("netcube", cubeReport)
    dateIndexColumn = netCubeReport.dataAsSize(2)
    sampleColumn = netCubeReport.dataAsSize(4)
    depthColumn = netCubeReport.dataAsSize(5)
    valueColumn = netCubeReport.dataAsReal(6)

    sampleSet = set()
    for i in range(0, netCubeReport.rows()):
        if (sampleColumn[i] > 0):
            sampleSet.add(sampleColumn[i])
    print("samples:", len(sampleSet))

    values = [0] * (len(sampleSet) + 1)
    for i in range(0, netCubeReport.rows()):
        date = dateIndexColumn[i]
        depth = depthColumn[i]
        sample = sampleColumn[i]
        value = valueColumn[i]
        if date == dateIndex and depth == 0 and sample > 0: 
            values[sample] = value

    values[0] = values[1]
    
    return values

def plotScenarioDataPaths(gzFileName, keyNumber, numberOfPaths, fixing):
    import gzip
    import csv
    dateSet = set()
    sampleSet = set()
    keySet = set()
    # scenario data file starts with date = 1, i.e. does NOT contain t0
    dateSet.add(0)

    with gzip.open(gzFileName, mode='rt') as file:
        reader = csv.reader(file, delimiter = ',', quotechar="'")
        for row in reader:
            if (not row[0].startswith('#')):
                #print("data", row)
                date = int(row[0])
                sample = int(row[1])
                key = int(row[2])
                value = float(row[3])
                dateSet.add(int(date))
                sampleSet.add(int(sample))
                keySet.add(int(key))
    print("dates:  ", len(dateSet))
    print("samples:", len(sampleSet))
    print("keys:   ", len(keySet))

    dates = list(dateSet)
    values = []
    for i in range(0, len(sampleSet)):
        values.append([0] * len(dateSet))

    with gzip.open("Output/scenariodata.csv.gz", mode='rt') as file:
        reader = csv.reader(file, delimiter = ',', quotechar="'")
        for row in reader:
            if (not row[0].startswith('#')):
                date = int(row[0])
                sample = int(row[1])
                key = int(row[2])
                value = float(row[3])
                if key == keyNumber:
                    values[sample][date] = value
                    # FIXME: copy to the left for now
                    if date == 1:
                        values[sample][0] = fixing

    import matplotlib.pyplot as plt
    from matplotlib.gridspec import GridSpec

    fig = plt.figure(figsize=(12, 5))
    gs = GridSpec(nrows=1, ncols=1)
    ax0 = fig.add_subplot(gs[0, 0])

    for p in range(1, numberOfPaths+1):
        ax0.plot(dates, values[p], label='')
        ax0.set(xlabel='Time', ylabel='Rate')
        ax0.set_title('Selected Market Data Paths')
        #ax0.legend()
    
    plt.show()
    
def getStateScenarios(gzFileName, keyNumber, dateIndex):
    import gzip
    import csv
    dateSet = set()
    sampleSet = set()
    keySet = set()
    # scenario data file starts with date = 1, i.e. does NOT contain t0
    dateSet.add(0)

    with gzip.open(gzFileName, mode='rt') as file:
        reader = csv.reader(file, delimiter = ',', quotechar="'")
        for row in reader:
            if (not row[0].startswith('#')):
                #print("data", row)
                date = int(row[0])
                sample = int(row[1])
                key = int(row[2])
                value = float(row[3])
                dateSet.add(int(date))
                sampleSet.add(int(sample))
                keySet.add(int(key))
    #print("dates:  ", len(dateSet))
    print("samples:", len(sampleSet))
    #print("keys:   ", len(keySet))

    values = [0] * (len(sampleSet) + 1)
    
    with gzip.open("Output/scenariodata.csv.gz", mode='rt') as file:
        reader = csv.reader(file, delimiter = ',', quotechar="'")
        for row in reader:
            if (not row[0].startswith('#')):
                date = int(row[0])
                sample = int(row[1])
                key = int(row[2])
                value = float(row[3])
                if key == keyNumber and date == dateIndex:
                    values[sample] = value

    values[0] = values[1]
    
    return values

def match_sensi_reports(ore1, ore2, deltaHeader1, deltaHeader2, debug=False):
    sensi1 = ore1.getReport("sensitivity")
    sensi2 = ore2.getReport("sensitivity")

    if (debug == True):
        print("ORE 1:")
        display(format_report(sensi1))
        print("ORE 2:")
        display(format_report(sensi2))

    trade1 = sensi1.dataAsString(0)
    factor1 = sensi1.dataAsString(2)
    currency1 = sensi1.dataAsString(6)
    delta1 = sensi1.dataAsReal(8) 

    trade2 = sensi2.dataAsString(0)
    factor2 = sensi2.dataAsString(2)
    currency2 = sensi2.dataAsString(6)
    delta2 = sensi2.dataAsReal(8) 

    print ("%-25s %-45s %10s %10s %10s %10s" % ("Trade", "Factor", "Currency", deltaHeader1, deltaHeader2, "Diff"))
    for j in range(0, sensi2.rows()):
        matched = False
        for i in range(0, sensi1.rows()):
            if (trade1[i] == trade2[j] and factor1[i] == factor2[j]):
                diff = delta1[i] - delta2[j]
                print("%-25s %-45s %10s %10.2f %10.2f %10.2f" % (trade1[i], factor1[i], currency1[i], delta1[i], delta2[j], diff))
                matched = True
        if (matched == False):      
            print("%-25s %-45s %10s %10s %10.2f %10s" % (trade2[j], factor2[j], currency2[j], "----", delta2[j], "----"))

    for i in range(0, sensi1.rows()):
        matched = False
        for j in range(0, sensi2.rows()):
            if (trade1[i] == trade2[j] and factor1[i] == factor2[j]):
                matched = True
        if (matched == False):      
            print("%-25s %-45s %10s %10.2f %10s %10s" % (trade1[i], factor1[i], currency1[i], delta1[i], "----", "----"))


def match_pricingstats_12(ore1, ore2, header2, debug = False):
    pricingstats1 = ore1.getReport("pricingstats")
    pricingstats2 = ore2.getReport("pricingstats")

    if (debug == True):
        print("pricing stats 1 (micro seconds):")
        display(format_report(pricingstats1))
        print("pricing stats 2 (micro seconds):")
        display(format_report(pricingstats2))

    tradeid = pricingstats1.dataAsString(0)
    tradetype = pricingstats1.dataAsString(1)
    pricings = pricingstats1.dataAsSize(2)
    cumulative = pricingstats1.dataAsSize(3) 
    average = pricingstats1.dataAsSize(4) 
    average2 = pricingstats2.dataAsSize(4) 
    cumulative2 = pricingstats2.dataAsSize(3)

    print()
    print ("%-25s %-9s %21s %14s %12s" % ("TradeId", "Pricings", "CumulativeTimingBump", "AvgTimingBump", header2))
    for i in range(1, pricingstats1.rows()):
        print("%-25s %9d %21d %14d %12.2f" % (tradeid[i], pricings[i], cumulative[i], average[i], cumulative[i]/cumulative2[i]))
    print()

def match_pricingstats_123(ore1, ore2, ore3, header2, header3, debug = False):
    pricingstats1 = ore1.getReport("pricingstats")
    pricingstats2 = ore2.getReport("pricingstats")
    pricingstats3 = ore3.getReport("pricingstats")

    if (debug == True):
        print("pricing stats 1 (micro seconds):")
        display(format_report(pricingstats1))
        print("pricing stats 2 (micro seconds):")
        display(format_report(pricingstats2))
        print("pricing stats 3 (micro seconds):")
        display(format_report(pricingstats3))

    tradeid = pricingstats1.dataAsString(0)
    tradetype = pricingstats1.dataAsString(1)
    pricings = pricingstats1.dataAsSize(2)
    cumulative = pricingstats1.dataAsSize(3) 
    average = pricingstats1.dataAsSize(4) 
    cumulative2 = pricingstats2.dataAsSize(3)
    cumulative3 = pricingstats3.dataAsSize(3)

    print()
    print ("%-25s %-9s %21s %14s %12s %13s" % ("TradeId", "Pricings", "CumulativeTimingBump", "AvgTimingBump", header2, header3))
    for i in range(1, pricingstats1.rows()):
        print("%-25s %9d %21d %14d %12.2f %13.2f" % (tradeid[i], pricings[i], cumulative[i], average[i], cumulative[i]/cumulative2[i], cumulative[i]/cumulative3[i]))
    print()
