import glob
import argparse

outfilename = "Input/portfolio.xml"
filenames = sorted(glob.glob("Example_Trades/*.xml"))

parser = argparse.ArgumentParser()
parser.add_argument("--n", type=int, help="the pick n'th trade from the list",
                    nargs='?', default=-1, const=0)
args = parser.parse_args()

#print(filenames)

if (args.n >= 0):
    filenames = [filenames[args.n]] 

print(filenames, "\n")

text="Credit_"
filteredFileNames = list(filter(lambda k: text in k, filenames))
print("Filtered:", filteredFileNames, "\n")
#filenames = filteredFileNames

with open(outfilename, 'w') as outfile:
    outfile.write('<Portfolio>\n')
    for fname in filenames:
        with open(fname) as readfile:
            print("Add file:", fname)
            for line in readfile:
                if ('Portfolio>' not in line and 'xml version' not in line):
                    outfile.write(line)
        outfile.write("\n\n")
    outfile.write('</Portfolio>\n')
