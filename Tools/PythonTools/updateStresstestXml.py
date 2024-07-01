import argparse
import xml.etree.ElementTree as ET

def convert_cap_floor_scenario_definitions(stress_test_file, output_file):
    tree = ET.parse(stress_test_file)
    root = tree.getroot()
    for cap_floor_scenario_node in root.findall('.//CapFloorVolatility'):
        if 'key' in cap_floor_scenario_node.attrib.keys():
            print("Convert ", cap_floor_scenario_node.attrib['key'])
        else:
            print("Convert ", cap_floor_scenario_node.attrib['ccy'])
        expiries = cap_floor_scenario_node.find('ShiftExpiries')
        shifts = cap_floor_scenario_node.find('Shifts')
        if expiries is not None and shifts is not None and len(list(shifts)) == 0:
            tenor_values = expiries.text.replace(" ", "").strip().split(',')
            shift_values = shifts.text.replace(" ", "").strip().split(',')
            if len(tenor_values) == len(shift_values):
                shifts.clear()
                for tenor, shift in zip(tenor_values, shift_values):
                    shift_node = ET.Element('Shift', tenor=tenor)
                    shift_node.text = shift
                    shifts.append(shift_node)
            else:
                print("Error, mismatch between tenor and shift values. Skip")
        else:
            if 'key' in cap_floor_scenario_node.attrib.keys():
                print("Skip ", cap_floor_scenario_node.attrib['key'])
            else:
                print("Skip ", cap_floor_scenario_node.attrib['ccy'])

    tree.write(output_file)


if __name__=="__main__":
    parser = argparse.ArgumentParser(
                    prog='Update StressTest-XML',
                    description='Converts the capfloorVolatility Shifts into the new format')
    parser.add_argument('input_file')
    parser.add_argument('ouput_file')
    args = parser.parse_args()
    print(args.input_file, args.ouput_file)
    convert_cap_floor_scenario_definitions(args.input_file, args.ouput_file)