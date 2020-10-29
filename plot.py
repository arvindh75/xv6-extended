import matplotlib.pyplot as plt

x1, y1 = [], []
x2, y2 = [], []
x3, y3 = [], []
x4, y4 = [], []
x5, y5 = [], []

count = 0
initial = 0
for line in open("TEST2.txt", "r"):
    values = [float(s) for s in line.split()]
    if count == 0:
        initial = values[0]
    if values[1] == 4:
        x1.append(values[0] - initial)
        y1.append(values[3])
    if values[1] == 5:
        x2.append(values[0] - initial)
        y2.append(values[3])
    if values[1] == 6:
        x3.append(values[0] - initial)
        y3.append(values[3])
    if values[1] == 7:
        x4.append(values[0] - initial)
        y4.append(values[3])
    if values[1] == 8:
        x5.append(values[0] - initial)
        y5.append(values[3])
    count += 1

plt.plot(x1, y1, label="proc 1")
plt.plot(x2, y2, label="proc 2")
plt.plot(x3, y3, label="proc 3")
plt.plot(x4, y4, label="proc 4")
plt.plot(x5, y5, label="proc 5")
plt.xlabel("Ticks")
plt.ylabel("Level")
plt.legend()
plt.show()
