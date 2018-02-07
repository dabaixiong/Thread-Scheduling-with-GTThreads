import numpy

# Read in datasets
lines = []

with open('./runtime_data.txt', 'rb') as f:
    content = f.readlines()
    content = [x.strip() for x in content]

    for line in content:
        if "tid" in line:
            lines.append(line)

f.close()

lines2 = []

with open('./execution_time_data.txt', 'rb') as f:
    content = f.readlines()
    content = [x.strip() for x in content]

    for line in content:
        if "tid" in line:
            lines2.append(line)

f.close()

# Extract times
runtime = []
execution_time = []

for line in lines:
    words = line.split(" ")

    tid = int(words[1])
    us = float(words[3])

    runtime.append((tid, us))

for line in lines2:
    words = line.split(" ")

    tid = int(words[1])
    us = float(words[3])

    execution_time.append((tid, us))

runtime.sort()
execution_time.sort()

weights = [25, 50, 75, 100, 25, 50, 75, 100, 25, 50, 75, 100, 25, 50, 75, 100]
matrix_sizes = [32, 32, 32, 32, 64, 64, 64, 64, 128, 128, 128, 128, 256, 256, 256, 256]

# Calculate statistics
print("Summary Statistic")

for x in range(16):
    runtime_group = []
    exec_time_group = []
    for y in range(8):
        runtime_group.append(runtime[x * 8 + y][1])
        exec_time_group.append(execution_time[x * 8 + y][1])
    
    runtime_arr = numpy.array(runtime_group)
    exec_time_arr = numpy.array(exec_time_group)

    mean_runtime = str(numpy.mean(runtime_arr))
    runtime_sd = str(numpy.std(runtime_arr))
    mean_exec = str(numpy.mean(exec_time_arr))
    exec_sd = str(numpy.std(exec_time_arr))

    weight = str(weights[x])
    matrix_size = str(matrix_sizes[x])

    print("Weight: " + weight + "\tMatrixSize: " + matrix_size + "\tMean_Runtime: " + mean_runtime + "\tRuntime_SD: " + runtime_sd + "\tMean_Exec_Time: " + mean_exec + "\tExec_Time_SD: " + exec_sd)
