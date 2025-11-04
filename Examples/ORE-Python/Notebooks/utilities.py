import sys, time, math
import pandas as pd
import gzip
import csv

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
def writeReport(report, columnNumbers=None):
    colNums = range(report.columns()) if columnNumbers is None else columnNumbers

    column = [None] * report.columns()
    for c in range(0, report.columns()):
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
            for c in colNums:
                if c == colNums[-1]:
                    eol = "\n"
                else:
                    eol = ""
                print ("%-17s  " % report.header(c), end=eol)

        for c in colNums:
            typ = report.columnType(c)
            if c == colNums[-1]:
                eol = "\n"
            else:
                eol = ""
            if typ == 0:
                print ("%-17d  " % column[c][i], end=eol)
            elif typ == 1:
                print ("%-17.4f  " % column[c][i], end=eol)
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

def is_gz_file(filepath):
    with open(filepath, 'rb') as test_f:
        return test_f.read(2) == b'\x1f\x8b'

def wait_for_gzip_ready(filepath, retries=5, delay=1):
    import gzip
    """Wait until a gzip file is fully written and readable."""
    for attempt in range(retries):
        try:
            with gzip.open(filepath, 'rt') as f:
                for _ in f:
                    pass
            print(f"✅ Gzip file is ready after {attempt + 1} attempt(s).")
            return        
        except (EOFError, OSError, gzip.BadGzipFile) as e:
            print(f"⏳ Gzip file not ready (attempt {attempt + 1}/{retries}): {e}")
            time.sleep(delay)
    raise RuntimeError(f"❌ Gzip file {filepath} is not readable after {retries} retries.")

def read_scenario_data(gzFileName):
    dateSet = set([0])  # scenario data file starts with date = 1, does NOT contain t0
    sampleSet = set()
    keySet = set()
    data_rows = []

    try:
        open_func = gzip.open if is_gz_file(gzFileName) else open
        with open_func(gzFileName, mode='rt') as file:
            reader = csv.reader(file, delimiter=',', quotechar="'")
            for row in reader:
                if not row[0].startswith('#'):
                    date = int(row[0])
                    sample = int(row[1])
                    key = int(row[2])
                    value = float(row[3])
                    dateSet.add(date)
                    sampleSet.add(sample)
                    keySet.add(key)
                    data_rows.append((date, sample, key, value))
    except EOFError as e:
        print(f"Error reading gzip file: {e}")
        return None, None, None, None

    return list(dateSet), list(sampleSet), list(keySet), data_rows

def plotScenarioDataPaths(gzFileName, keyNumber, numberOfPaths, fixing):
    import matplotlib.pyplot as plt
    from matplotlib.gridspec import GridSpec

    dates, samples, keys, data_rows = read_scenario_data(gzFileName)
    if data_rows is None:
        print("Failed to read scenario data. Aborting plot.")
        return

    print("dates:  ", len(dates))
    print("samples:", len(samples))
    print("keys:   ", len(keys))

    values = [[0] * len(dates) for _ in range(len(samples))]

    for date, sample, key, value in data_rows:
        if key == keyNumber:
            values[sample][date] = value
            if date == 1:
                values[sample][0] = fixing

    fig = plt.figure(figsize=(12, 5))
    gs = GridSpec(nrows=1, ncols=1)
    ax0 = fig.add_subplot(gs[0, 0])

    for p in range(1, numberOfPaths + 1):
        ax0.plot(dates, values[p], label='')
        ax0.set(xlabel='Time', ylabel='Rate')
        ax0.set_title('Selected Market Data Paths')

    plt.show()

def getStateScenarios(gzFileName, keyNumber, dateIndex):
    import gzip
    import csv
    dateSet = set()
    sampleSet = set()
    keySet = set()
    # scenario data file starts with date = 1, i.e. does NOT contain t0
    dateSet.add(0)

    if is_gz_file(gzFileName):
        file = gzip.open(gzFileName, mode='rt')
    else:
        file = open(gzFileName)        
    
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

    file.close()
    
    #print("dates:  ", len(dateSet))
    print("samples:", len(sampleSet))
    #print("keys:   ", len(keySet))

    values = [0] * (len(sampleSet) + 1)
    
    if is_gz_file(gzFileName):
        file = gzip.open(gzFileName, mode='rt')
    else:
        file = open(gzFileName)        

    reader = csv.reader(file, delimiter = ',', quotechar="'")
    for row in reader:
        if (not row[0].startswith('#')):
            date = int(row[0])
            sample = int(row[1])
            key = int(row[2])
            value = float(row[3])
            if key == keyNumber and date == dateIndex:
                values[sample] = value

    file.close()

    values[0] = values[1]
    
    return values
