# pico
capture de données depuis un picoscope 5244b

## installation 
voir picosdk-c-wrappers-binaries dans https://github.com/picotech

frps5000a.c premiere version depuis l'exemple puis selon ma lecture de la doc... pas fameux.
pico_002.c version c du code csharp... pas fameux
exemple.c seconde version presque maitrisee issue de l'exemple fourni pas la lib... en cours d'amelioration

## compilation
gcc <source>.c -g -o <bin exe>  -lm -lps5000a  -Wno-format -Wl,-s  -u,pthread_atfork -L/opt/picoscope/lib -I/opt/picoscope/include

# analyse de qualité des données

pseudo algo:
1: identifier les passages a 0 = inversion de signe
2: compter les donnees entre les passages a 0
3: seuil/elastique pour identifier les periodes courte et longue (actuellement 1020/1060)
4: seuil/elastique pour identifier les periodes ultra courte et ultra longue (actuellement 40/1500)
5: seuil/elastique pour identifier les sauts de croissance dans les donnees (actuellement 450)

# TODO

1: mettre en paramètres les seuils et le taux d'échantillonnage
2: projeter une courbe des demi périodes et des erreurs
3: fichier des demi periodes et analyse/courbe
4: automate de réglage des seuils sur un taux de rejet donné
