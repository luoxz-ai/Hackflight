#!/usr/bin/python3

import numpy as np
import matplotlib.pyplot as plt


def main():

    data = np.loadtxt('controllers/cplusplus/snufa.csv',
            delimiter=',', skiprows=1)

    time = data[:, 0]
    setpoint = data[:, 1]
    dy = data[:, 2]
    phi = data[:, 3]
    dphi = data[:, 4]
    output = data[:, 5]

    plt.subplot(3, 1, 1)
    plt.plot(time, setpoint)
    plt.plot(time, dy)
    plt.xticks([], [])
    plt.ylabel('m/s')
    plt.legend(['Target', 'Actual'])

    '''
    plt.subplot(3, 1, 2)
    plt.plot(time, phi)
    plt.xticks([], [])
    plt.ylabel('Angle (deg)')
    plt.xlim([lotime, time[-1]])

    plt.subplot(3, 1, 3)
    plt.plot(time, dphi)
    plt.xlabel('Time (sec)')
    plt.ylabel('Angular velocity (deg/sec)')
    plt.xlim([lotime, time[-1]])
    '''

    plt.show()


main()
