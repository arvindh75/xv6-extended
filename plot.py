import matplotlib.pyplot as plt

x = []
y = []

num_proc = input("Enter the number of processes: ")
num_proc = int(num_proc)


for i in range(num_proc):
    x.append([])
    y.append([])

count = 0
initial = 0
for line in open("TEST512.txt", "r"):
    values = [int(s) for s in line.split()]
    if count == 0:
        initial = values[0]
    x[values[1] - 4].append(values[0] - initial)
    y[values[1] - 4].append(values[3])
    count += 1

for i in range(num_proc):
    plt.plot(x[i], y[i])  # , label="proc " + str(i + 1))

plt.xlabel("Ticks")
plt.ylabel("Level")
plt.legend()
plt.show()
