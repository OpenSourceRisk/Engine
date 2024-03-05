import os
import xml.etree.ElementTree as ET

def process_xml_file(file_path):
    # Process the XML file
    print(file_path)
    parser = ET.XMLParser(target=ET.TreeBuilder(insert_comments=True)) # to include the same comments...
    tree = ET.parse(file_path, parser)

    for node in tree.iter():
        if node.tag == "TenorBasisSwap":
            for child in list(node):
                if(child.tag == "LongIndex"):
                    child.tag = "PayIndex"
                if(child.tag == "LongPayTenor"):
                    child.tag = "PayFrequency"
                if(child.tag == "ShortIndex"):
                    child.tag = "ReceiveIndex"
                if(child.tag == "ShortPayTenor"):
                    child.tag = "ReceiveFrequency"
                if(child.tag == "SpreadOnShort"):
                    child.tag = "SpreadOnRec"
    tree.write(file_path, short_empty_elements= True)

def change_format(file_path, firstLine, lastLine):

    with open(file_path, 'r') as file:
        content = file.read()

    # erase white spaces on empty elements created by eTree
    target_whitespace =' /'
    replace_whitespace ='/'
    modified_content = content.replace(target_whitespace, replace_whitespace)

    # add empty lines at the end
    modified_content = modified_content + '\n'
    for _ in range(lastLine):
        modified_content = modified_content + '\n'

    # add first line if necessary and close
    with open(file_path, 'w') as file:
        if firstLine.startswith('<?xml'):
            file.write(firstLine + '\n' + modified_content)
        else:
            file.write(modified_content)

def get_first_line(file_path):
    with open(file_path, 'r') as file:
        first_line = file.readline().strip()
    return first_line

def count_empty_last_lines(file_path):
    count = 0
    with open(file_path, 'rb') as file:
        # Read the file line by line from the end
        for line in reversed(list(file)):
            line = line.strip()
            if not line:
                count += 1
            elif line == b'\r' or line == b'\n':
                count += 1
            else:
                break
    return count

def main():
    folder_path = os.getcwd()
    # Loop through all files in the folder
    for root, dirs, files in os.walk(folder_path):
        for file_name in files:
            if file_name.endswith("conventions.xml"):
                file_path = os.path.join(root, file_name)
                firstLine = get_first_line(file_path)
                lastLine = count_empty_last_lines(file_path)
                process_xml_file(file_path)
                change_format(file_path, firstLine, lastLine)

if __name__ == "__main__":
    main()