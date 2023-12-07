# This program takes two paths as input and executes the benchmarking and the number of processes to use
# Basically first check that the parallel version of the program gives the same of the serial one (that is correct)

# To execute the program: path_input1/scripts/launch.sh path_input1/build/main path_input1/data/molecules.smi path_input1/output.csv 1
# And for the parallel version: path_input2/scripts/launch.sh path_input2/build/main path_input2/data/molecules.smi path_input2/output.csv number_of_processes

import sys
import os
import time
import matplotlib.pyplot as plt
import subprocess
import shlex

def build_command(path, number_processes):
    command = path + "/scripts/launch.sh " + path + "/build/main " + path + "/data/molecules.smi " + path + "/output.csv " + str(number_processes)
    return shlex.split(command)

# Check the number of arguments
if len(sys.argv) != 4:
    print("The number of arguments is not correct\n\tUsage: python benchmarking.py path_to_serial path_to_parallel number_processes")
    sys.exit(1)

# Check if the paths are correct
if not os.path.exists(sys.argv[1]):
    print("The path_to_serial does not exist")
    sys.exit(1)

if not os.path.exists(sys.argv[2]):
    print("The path_to_parallel does not exist")
    sys.exit(1)

# Check if the number of processes is correct
if int(sys.argv[3]) <= 0:
    print("The number of processes must be greater than 0")
    sys.exit(1)

path_serial = sys.argv[1]
path_parallel = sys.argv[2]
max_number_processes = int(sys.argv[3])

# Serial version
subprocess.check_call(build_command(path_serial, 1), stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
# Parallel version
subprocess.check_call(build_command(path_parallel, max_number_processes), stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
# Check if the two files are the same if not print an error
if os.system("diff " + path_serial + "/output.csv " + path_parallel + "/output.csv") != 0:
    print("The two files are not the same")
    sys.exit(1)
else:
    print("Parallel version yields the same results as the serial one")
# Now execute the benchmarking: see if the parallel version scales on the number of processes

times = []
for i in range(1, max_number_processes + 1):
    start = time.time()
    subprocess.check_call(build_command(path_parallel, i), stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    end = time.time()
    times.append(end - start)
    print("Time for " + str(i) + " processes: " + str(end - start))

plt.plot(range(1, max_number_processes + 1), times, marker='o')
plt.xlabel("Number of processes")
plt.ylabel("Time")
plt.title("Benchmarking")
plt.show()