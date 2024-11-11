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
import sys
from scipy.interpolate import make_interp_spline
#import pdb; pdb.set_trace()
nfi = "/media/freg/4883122971EA658C/stream.txt"
nfi = "stream.txt"
if "-f" in sys.argv:
    nfi = sys.argv[1+sys.argv.index("-f")]
f = pd.read_csv(nfi, skiprows=10, nrows=28000)
f = f.loc[:, ~f.columns.str.contains('^Unnamed')]

ampl = 20
nbr_pas = 2**16
val_pas = (ampl / nbr_pas) * 1000 

f["u_a"] = f["ADC_A"] * val_pas
f["u_b"] = f["ADC_B"] * val_pas
mina = 0
minb = 0
maxa = 0
maxb = 0
# recherche du max et min de la série
for v in f["u_a"]:
    if mina > v:
        mina = v
    if maxa < v:
        maxa = v

for v in f["u_b"]:
    if minb > v:
        minb = v
    if maxb < v:
        maxb = v
amplitude_a = maxa - mina
amplitude_b = maxb - minb

print("mina ", mina, " maxa ", maxa, " amplitudea ", amplitude_a)
print("minb ", minb, " maxb ", maxb, " amplitudeb ", amplitude_b)

pilemaxa = []
p = 0
for v in f["u_a"]:
    if v >= maxa - 40 and v <= maxa + 40 :
        pilemaxa.append(p)
    p += 1
pilemaxb = []
p = 0
for v in f["u_b"]:
    if v >= maxb - 40 and v <= maxb + 40 :
        pilemaxb.append(p)
    p += 1
lp = -1


pileperiodea = []
for p in pilemaxa:
    if lp > -1:
        periodea = p - lp
        if periodea > 1500:
            pileperiodea.append(periodea)
    lp = p

lp = -1
pileperiodeb = []
for p in pilemaxb:
    if lp > -1:
        periodeb = p - lp
        if periodeb > 1500 and periodeb < 2500:
            pileperiodeb.append(periodeb)
    lp = p

pbma = 0
for v in pileperiodea:
    pbma += v
pbma /= len(pileperiodea)
pbmb = 0
for v in pileperiodeb:
    pbmb += v
pbmb /= len(pileperiodeb)

print("periodesa ", pileperiodea, " pmoyenne ", pbma)
print("periodesb ", pileperiodeb, " pmoyenne ", pbmb)

idx = range(len(f))
xnew = np.linspace(min(idx), max(idx), 20*len(f))
spl = make_interp_spline(idx, f["u_b"], k=3)
smooth = spl(xnew)
    
#%% plot

# ax = np.linspace(-np.pi,np.pi,1000)
# # the function, which is y = sin(x) here
# ay = 8000*np.sin(ax)

Fs = 10000
frq = 10.475
sample = 28000
x = np.arange(sample)
y = 8200*np.cos((2 * np.pi * frq * x / Fs) + f["u_a"][0]) + 100

print("y0 ", y[0], "x0", x[0])
print("ua0 ", f["u_a"][0])
plt.figure(100)
plt.scatter(range(len(f["u_a"])),f["u_a"], s=1)
plt.scatter(range(len(f["u_b"])),f["u_b"], s=1)
plt.scatter(x, y, s=1)
#plt.scatter(ax, ay, s=1)
#plt.plot(f["u_a"], f["u_b"])
#plt.plot(xnew, smooth)
#%% conversion dbea
plt.show()

# plt.figure(200)
# plt.plot(f['u_a'])
# plt.ylabel('ADC_A')
# plt.title('ADC_A Plot')
# plt.show()
# plt.figure(300)
# plt.plot(f['u_b'])
# plt.ylabel('ADC_B')
# plt.title('ADC_B plot')
# plt.show()
# plt.figure(400)
# plt.show()
# plt.scatter(range(len(f["u_a"])),f["u_a"])
# plt.show()
# plt.figure(500)
# plt.scatter(range(len(f["u_a"])),f["u_a"])
# plt.show()
# db = 25
# u = (10**(db/20)) * 1e-6
