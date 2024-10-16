# -*- coding: utf-8 -*-
"""
Created on Mon Aug  5 18:37:15 2024

@author: flori
"""
from datetime import datetime
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import math as mt
from scipy.interpolate import make_interp_spline
#import pdb; pdb.set_trace()


f = pd.read_csv("stream.txt", skiprows=10, nrows=2000)
#f = pd.read_csv("stream.txt") 
f = f.loc[:, ~f.columns.str.contains('^Unnamed')]

ampl = 20
nbr_pas = 2**16
val_pas = (ampl / nbr_pas) * 1000 

f["u_a"] = f["ADC_A"] * val_pas
f["u_b"] = f["ADC_B"] * val_pas
 
idx = range(len(f))
xnew = np.linspace(min(idx), max(idx), 20*len(f))
spl = make_interp_spline(idx, f["u_b"], k=3)
smooth = spl(xnew)

#%% plot

plt.scatter(range(len(f["u_a"])),f["u_a"])
plt.scatter(range(len(f["u_b"])),f["u_b"])
#plt.plot(f["u_a"], f["u_b"])
#plt.plot(xnew, smooth)
#%% conversion dbea
current_dateTime = datetime.now()
fi = open("stream.txt", "r")
if fi:
    line = fi.readline()
    line = fi.readline()
    if len(line)>0:
        plt.title(line)
    fi.close()
else:
    plt.title("picoscope %s"%current_dateTime)
plt.show()
db = 25
u = (10**(db/20)) * 1e-6
