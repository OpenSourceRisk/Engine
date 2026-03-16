import pandas as pd


def decodeXML(filename):
    return etree.tostring(etree.parse(filename)).decode('UTF-8')


def report_to_df(report):
    headers = [[i, report.header(i), report.columnType(i)] for i in range(report.columns())]

    formatted_report = dict()
    for i, column_name, data_type in headers:
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
    
    return pd.DataFrame(formatted_report)