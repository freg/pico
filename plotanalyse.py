#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Created on Mon Oct 28 10:43:12 2024

@author: freg
"""

import pandas as pd
import plotly.express as px
import plotly.io as pio
from sklearn import linear_model
import seaborn as sns
import matplotlib.pyplot as plt


pio.renderers.default='browser'

df = pd.read_csv('/media/freg/f18f1c27-8657-4d6d-9236-8c3f8e175f55/ana1.txt', skiprows=2)
print(df.head())
# max_heartrate = df['thalachh']
# resting_blood_pressure = df['trtbps']
# cholesterol_level = df['chol']
# output = df['output']

x = df['Ligne']
decalage = df['Decalage']
#decalage *= 100
#import pdb;pdb.set_trace()
dmin = df['min']
dmax = df['max']
sns.regplot(x=x, y=decalage, fit_reg=True)
plt.show()
fig = px.scatter(df, x='Ligne', y=['Decalage', 'min', 'max'])
fig.show()
