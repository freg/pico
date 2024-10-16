#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Wed Oct 16 21:28:14 2024

@author: freg
"""

import os
import psutil
import numpy as np

pid = os.getpid()
python_process = psutil.Process(pid)
tourniquet = []
taille = 1000
nline = 0
last = []
def tourniquet_fenetre(tourniquet, nline, last):
    new = [np.min(tourniquet), np.max(tourniquet),
           np.average(tourniquet), 
           np.var(tourniquet), 
           np.std(tourniquet)]
    if len(last) and new != last:
        print("anomalie ligne %d : %r\n"%(nline, new))
    last = new
        
    
def tourniquet_lire(fi, tourniquet, nline, last):
    try:
        line = fi.readline()
        data = line.split(',')
        nline += 1
        if len(tourniquet) < taille:
            tourniquet.append(eval(data[1]))
        else:
            tourniquet_fenetre(tourniquet, nline, last)
            tourniquet = tourniquet[1:]
            tourniquet.append(eval(data[1]))
    except Exception as e:
        print(e)
        import pdb;pdb.set_trace()
    return nline
        
cpt = 0
fi = open("/media/freg/4883122971EA658C/stream.txt", "r")
for i in range(1,12):
    fi.readline()

while fi:
    if cpt%100000 == 0:
        memoryUse = python_process.memory_info()[0]/2.**30  # memory use in GB...I think
        print('memory use:', memoryUse, ' nline:', nline) 
    cpt+=1
    nline = tourniquet_lire(fi, tourniquet, nline, last)