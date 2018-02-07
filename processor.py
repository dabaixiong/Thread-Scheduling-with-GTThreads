import numpy

lines = []

with open('./output.txt', 'rb') as f:
    content = f.readlines()
    content = [x.strip() for x in content]

    for line in content:
        if "finished" in line:
            lines.append(line)

f.close()

times = []

for line in lines:
    words = line.split(" ")

    tid = int(words[1])
    s = float(words[9])
    us = float(words[12])
    ms = s * 1000 + us / 1000

    times.append((tid, ms))

times.sort()

weights = [25, 50, 75, 100, 25, 50, 75, 100, 25, 50, 75, 100, 25, 50, 75, 100]
matrix_sizes = [32, 32, 32, 32, 64, 64, 64, 64, 128, 128, 128, 128, 256, 256, 256, 256]


print("Summary Statistic")

for x in range(16):
    group = []
    for y in range(8):
        group.append(times[x * 8 + y][1])

    arr = numpy.array(group)

    mean_runtime = str(numpy.mean(arr))
    runtime_sd = str(numpy.std(arr))
    weight = str(weights[x])
    matrix_size = str(matrix_sizes[x])

    print("Weight: " + weight + "\tMatrixSize: " + matrix_size + "\tMean_Runtime: " + mean_runtime + "\tSD: " + runtime_sd)