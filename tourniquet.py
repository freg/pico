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
tourniquetA = []
tourniquetB = []
taille = 4000
nline = 0
lastA = []
lastB = []
def tourniquet_fenetre(tourniquet, nline, last, canal):
    new = [np.min(tourniquet), np.max(tourniquet),
           np.average(tourniquet), 
           np.var(tourniquet), 
           np.std(tourniquet)]
    #print ("new:", new, "last:", last)
    if len(last) and new != last:
        print("anomalie ligne %d : %r\n"%(nline, new))
    last = new
    return last
        
    
def tourniquet_lire(fi, tourniqueta, tourniquetb, nline, lasta, lastb):
    try:
        line = fi.readline()
        data = line.split(',')
        nline += 1
        if len(tourniqueta) < taille:
            tourniqueta.append(eval(data[1]))
        else:
            lasta = tourniquet_fenetre(tourniqueta, nline, lasta, "A")
            tourniqueta = tourniqueta[1:]
        if len(tourniquetb) < taille:
            tourniquetb.append(eval(data[3]))
        else:
            lastb = tourniquet_fenetre(tourniquetb, nline, lastb, "B")
            tourniquetb = tourniquetb[1:]
            tourniquetb.append(eval(data[3]))
    except Exception as e:
        print(e)
        import pdb;pdb.set_trace()
    return nline
        
cpt = 0
fi = open("/media/freg/4883122971EA658C/stream.txt", "r")
for i in range(1,12):
    fi.readline()

while fi:
    if cpt%10000 == 0:
        memoryUse = python_process.memory_info()[0]/2.**30  # memory use in GB...I think
        print('memory use:', memoryUse, ' nline:', nline) 
    cpt+=1
    nline = tourniquet_lire(fi, tourniquetA, tourniquetB, nline, lastA, lastB)