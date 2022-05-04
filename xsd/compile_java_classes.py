import os
import sys
import subprocess

root = "C:\\Users\\cian.obrien\\dev\\openxva\\xsd"

print("Begin compiling classes")
os.chdir(root)
os.mkdir(os.getcwd() + "\\generated\\classes")

for file in os.listdir(os.getcwd() + "\\generated"):
	print("Now in file: ", file)
	if file.endswith(".java"):
		subprocess.call("javac " + "generated\\" + file + " -d generated\\classes")

print("End of compilation")
