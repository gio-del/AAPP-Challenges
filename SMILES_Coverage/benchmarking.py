import sys
import os
import time
import matplotlib.pyplot as plt
import subprocess
import shlex

def build_launch_sh(path, number_processes):
    command = path + "/scripts/launch.sh " + path + "/build/main " + path + "/data/molecules.smi " + path + "/output.csv " + str(number_processes)
    return shlex.split(command)

def call_and_hide(command):
    subprocess.check_call(command, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, stdin=subprocess.DEVNULL)

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

# Now execute the benchmarking: see if the parallel version scales on the number of processes, spoiler: it won't
times = []
for i in range(1, max_number_processes + 1):
    start = time.time()
    call_and_hide(build_launch_sh(path_parallel, i))
    end = time.time()
    times.append(end - start)
    print("Time for " + str(i) + " processes: " + str(end - start))

plt.plot(range(1, max_number_processes + 1), times, marker='o')
plt.xlabel("Number of processes")
plt.ylabel("Time")
plt.title("Benchmarking")
plt.show()