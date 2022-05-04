import os
import sys
import subprocess

root = "C:\\Users\\cian.obrien\\dev\\openxva\\xsd"


os.chdir(root)
print("Generate Java class files:")
subprocess.call("xjc trade.xsd -b ore_types_bindings.xjb")
os.mkdir(os.getcwd() + "\\generated\\classes")

print("\nBegin compiling classes:")
for file in os.listdir(os.getcwd() + "\\generated"):
	print("Now in file: ", file)
	if file.endswith(".java"):
		subprocess.call("javac " + "generated\\" + file + " -d generated\\classes")

print("End of compilation")
