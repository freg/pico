#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Wed Oct 16 21:28:14 2024

@author: freg
"""

import os
import psutil
import numpy as np
import sys

print(sys.argv)

pid = os.getpid()
python_process = psutil.Process(pid)
tourniquetA = []
tourniquetB = []
taille = 1000
nline = 0
lastA = []
lastB = []
pileA = []
pileB = []
minA = []
minB = []
ncyclea = 0
ncycleb = 0
valcyclea = 1000000
valcycleb = 1000000

def tourniquet_fenetre(tourniquet, nline, last, canal):
    new = [float(np.min(tourniquet)), float(np.max(tourniquet)),
           float(np.average(tourniquet)), 
           float(np.var(tourniquet)), 
           float(np.std(tourniquet))]
    print ("canal ", canal, "new:", new, "last:", last)
    if len(last) and new != last:
        print("anomalie ligne %d : %r\n"%(nline, new))
    return new
        
    
def tourniquet_lire(fi, tourniqueta, tourniquetb, nline):
    global valcyclea, valcycleb, ncyclea, ncycleb, pileA, pileB
    try:
        #import pdb;pdb.set_trace()
        line = fi.readline()
        data = line.split(',')
        if not len(data):
            sys.exit(0) #return 0, [], []
        nline += 1
        a = float(eval(data[0]))
        if valcyclea == a or a in pileA:
            ncyclea += 1
            print("A cycle ", ncyclea, " val ", a, " pos prec ", pileA.index(a), " tpile ", len(pileA), " ligne ", nline )
            valcyclea = a
        elif valcyclea == 1000000:
            pileA.append(a)
        b = float(eval(data[1]))
        if valcycleb == b or b in pileB:
            ncycleb += 1
            print("B cycle ", ncycleb, " val ", b, " pos prec ", pileB.index(b), " tpile ", len(pileB), " ligne ", nline )
            valcycleb = b
        elif valcycleb == 1000000:
            pileB.append(b)
    except Exception as e:
        print(e, nline, line)
        import pdb;pdb.set_trace()
    return nline
        
cpt = 0
nfi = "/media/freg/4883122971EA658C/stream.txt"
if "-f" in sys.argv:
    nfi = sys.argv[1+sys.argv.index("-f")]
fi = open(nfi, "r")

while fi:
    line = fi.readline()
    if "ADC_A"  in line:
        break

while fi:
    if cpt%10000 == 0:
        memoryUse = python_process.memory_info()[0]/2.**30  # memory use in GB...I think
        print('memory use:', memoryUse, ' nline:', nline) 
    cpt+=1
    nline = tourniquet_lire(fi, tourniquetA, tourniquetB, nline)