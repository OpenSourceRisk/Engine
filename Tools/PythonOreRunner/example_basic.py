import os

from py_ore_tools import OreBasic

# setup your folders (the Examples in this case)
my_example_folder = r"D:\github\ore\Examples\Example_7"

# attach ore config folders to Python object
my_ore = OreBasic.from_folders(
    input_folder=os.path.join(my_example_folder, 'Input'),
    output_folder=os.path.join(my_example_folder, 'Output'),
    execution_folder=my_example_folder
)

# kick off a run
my_ore.run(ore_exe=r'D:\Apps\ORE-1.8\App\bin\x64\Release\ore.exe',
           delete_output_folder_before_run=False)

# inspect the output files
my_ore.parse_output()
print("Run produced output files: " + str(len(my_ore.output.locations)))

# extract data from the output file
print(my_ore.output.csv['npv'][['#TradeId', 'TradeType', 'NPV(Base)']])
print(my_ore.output.csv['xva'][['#TradeId', 'NettingSetId', 'BaselEEPE']])

