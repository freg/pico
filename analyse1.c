/****************************************************************************
 * @freg
 * charge les données par bloc en tuillage 
 * cherche les min/max les passages à 0
 * pour estimer la sinusoide
 * ...
 ****************************************************************************/
#define DATASZ 200000



#include <stdio.h> 
#include <unistd.h> 
#include <sys/types.h> 
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


int change_signe(int d2, int d3)
{
  if (d2*d3 < 0) return 1;
  return 0;
}
int passage_a_zero(int fi, int fo, int fer, int16_t**data, long sifi)
{
  static int16_t d1, d2,
    maxd=0,mind=0,
    derpositionl=0; // pour se remettre en debut de ligne au prochain read
  static char*c,*dl, // caractère courant et premier caractère de la ligne
    *buff = NULL;
  char tbuf[200];
  int dif;
  
  static long positionfic = 0; //position dans le fichier lseek
  static long gnligne=0,nligne=0, dpz=0, dpcs=0, difl=0; // compteur, derniere position du zero, der changement de sens
  short ok=0,sens=0,lsens=0; // fin entete et sens de la courbe -1 decroit 1 croit
  static short entete = 0; // entente passee ou pas
  if (!buff) { buff =  calloc(DATASZ+1, sizeof(char)); printf("alloc\n"); }
  if (derpositionl) // retour en debut de ligne
    lseek(fi,-derpositionl,SEEK_CUR);
  if (read(fi,buff,DATASZ)<1)
    {
      printf("%s\n",strerror(errno));
      free(buff);
      return 0;
    }
  buff[DATASZ] = 0;
  c=buff;
  dl = c;
  nligne=0;
  while(c&&*c)
    {
      switch (*c)
	{
	default:
	  break;
	case 0:
	  break;
	case '\n':
	  nligne++;
	  gnligne++;
	  if (!entete && !ok) // passe l'entete
	    {
	      printf("cherche l'entete %ld\n",nligne);
	      if (!strncmp(dl,"ADC_A,ADC_B",11))
		{
		  ok = 1;
		  entete = 1;
		}
	      nligne = gnligne = 0;
	      if (nligne > 50)
		{
		  printf("nada\n");
		  return 0;
		}
	    }
	  else
	    if (ok||entete)
	    {
	      sscanf(dl, "%hd,%hd", &data[0][nligne-1], &data[1][nligne-1]);
	      if (data[0][nligne-1]>d2) sens = 1;
	      if (data[0][nligne-1]<d2) sens = -1;
	      if (sens==1 && maxd<data[0][nligne-1])
		maxd = data[0][nligne-1] ;
	      else
		if (sens==-1 && mind>data[0][nligne-1])
		  mind = data[0][nligne-1] ;
	      
	      if (nligne>2 && change_signe(d2,data[0][nligne-1]))
		{
		  difl = nligne-dpz ;
		if (difl<0)
		    {
		      printf("Erreur décalage négatif: ligne: %ld nligne: %ld dpz: %ld dec: %ld\n",
			     gnligne, nligne, dpz, nligne-dpz);
		    }
		  sprintf(tbuf, "%ld,%ld,%ld,%hd,%d,%d,%d,%d,%hd\n",gnligne,nligne-dpz,
			  positionfic+c-buff,sens,mind,maxd,d1,d2,data[0][nligne-1]);
		  write(fo,tbuf,strlen(tbuf));
		  if (difl < 1020)
		    {
		      sprintf(tbuf, "demi periode courte: dper=%ld, ligne=%ld, passage=%ld\n",
			      difl, gnligne, dpz);
		      write(fer, tbuf, strlen(tbuf));
		    }
		  else
		    if (difl > 1060)
		      {
			sprintf(tbuf, "demi periode longue: dper=%ld, ligne=%ld, passage=%ld\n",
				difl, gnligne, dpz);
			write(fer, tbuf, strlen(tbuf));
		      }
		  dpz = nligne ;
		  if (sens==1)
		    maxd = 0;
		  else
		    if (sens==-1)
		      mind = 0;
		}
	      
	      d1=d2;
	      d2=data[0][nligne-1];
	      dif = d2-d1;
	      if (dif<0) dif *= -1;
	      if (dif>450)
		{
		  sprintf(buff, "diff: dif=%d, line=%ld, position=%ld, sens=%d, dn-1=%d, dn=%d\n",
			  dif, gnligne,positionfic+c-buff,sens,d1,d2);
		  write(fer, buff, strlen(buff));
		}
	    }
	  dl = c+1;
	  break;
	}
      
      c++;
    }
  
  positionfic+=DATASZ;
  derpositionl = c-dl;
  dpz -= nligne;
  printf("fin de passage... ligne:%ld, reste:%ld, decl:%d\n",gnligne,sifi-positionfic,derpositionl);
  if (positionfic>=sifi-derpositionl) // évite de boucler sur la dernière ligne
    {
      printf("sortie sur dépassement: pfic: %ld filesz: %ld derligne: %d\n",
	     positionfic, sifi, derpositionl);
      return 0;
    }
  return 1;
}

int main(int nba, char ** args, char ** env)
{
  char nfiin[200] = "stream.txt" ;
  char nfiout[200] = "ana1.txt" ;
  char nfier[200] = "errana.txt" ;
  int fi,fo,fer;
  int16_t * data[2];
  long fdatasz ; 
  data[0] = (int16_t*) calloc(DATASZ, sizeof(int16_t));
  data[1] = (int16_t*) calloc(DATASZ, sizeof(int16_t));
  fi = open(nfiin, O_RDONLY);
  lseek(fi, (size_t)0, SEEK_CUR); // reste la
  fdatasz = lseek (fi, (size_t)0, SEEK_END); // va à la fin
  printf("DataFile size: %ld\n", fdatasz);
  lseek(fi, (size_t)0, SEEK_SET); // remise au début
  fo = open(nfiout, O_CREAT|O_WRONLY|O_TRUNC, 0666);
  if (fo==-1)
    {
      printf("%s:%s\n",nfiout,strerror(errno));
    }
  fer = open(nfier, O_CREAT|O_WRONLY|O_TRUNC, 0666);
  if (fer==-1)
    {
      printf("%s:%s\n",nfier,strerror(errno));
    }
  char tbuf[] = "Analyse/parcours des données stream.txt\ntrace des changements de signe\nLigne,Decalage,PositionFi,Sens,min,max,data-2,data-1,data\n";
  write(fo,tbuf,strlen(tbuf));
  while(passage_a_zero(fi, fo, fer, data, fdatasz));
  close(fer);
  close(fo);
  close(fi);
}
