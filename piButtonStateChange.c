#define _GNU_SOURCE
#define DEV_INPUT_EVENT "/dev/input"
#define EVENT_DEV_NAME "event"
#define DEV_FB "/dev"
#define FB_DEV_NAME "fb"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>
#include <poll.h>
#include <dirent.h>
#include <string.h>

#include <linux/input.h>
#include <linux/fb.h>
#include <unistd.h>

// ========================================================================================================================================= //

typedef struct
{
  struct pollfd evpoll;
}
  Joystick;

int estUnEventDevice (const struct dirent* dir)
{
  return (int) ( strncmp ( "event", dir->d_name, strlen ("event")-1) == 0 );
}


Joystick* Joystick_creer()
{
  Joystick* joystick = (Joystick*) malloc ( sizeof (Joystick) );

  if (joystick)
    {
      /*
        La structure dirent est déclarée comme suit :

        struct dirent
        {
        long d_ino;                 // inode number                => le numéro d'i-node
        off_t d_off;                // offset to this dirent       => distance entre le début du répertoire et cette structure dirent
        unsigned short d_reclen;    // length of this d_name       => d_reclen est la longueur d_name, sans compter le caractère nul final.
        char d_name [NAME_MAX+1];   // file name (null-terminated) => d_name est le nom de fichier terminé par un caractère nul
        }
      */

      struct dirent** listeDeNoms;

      /*
        La fonction scandir() examine le répertoire dir, en appelant la fonction de filtrage, passée en 3ème paramètre, pour chaque élément rencontré.
        Les entrées, pour lesquelles la fonction de filtrage renvoie une valeur non nulle, sont

        - stockées dans une table temporaire allouée avec malloc ;
        - triées avec qsort en utilisant la fonction de comparaison passée en 4ème paramètre ;
        - regroupées dans la table listeDeNoms allouée avec malloc (2ème paramètre).

        Si la fonction de filtrage est NULL, toutes les entrées sont sélectionnées.

        Les fonctions alphasort() et versionsort() peuvent être utilisées comme fonction de comparaison (4ème paramètre) :

        - la première trie les entrées du répertoire en ordre alphabétique en utilisant strcoll ;
        - la seconde  trie les entrées du répertoire en utilisant strverscmp sur les chaînes (*a)->d_name et (*b)->d_name.

      */

      int nbEventDevice = scandir (
				   "/dev/input"     ,    // Répertoire examiné
				   &listeDeNoms     ,    // Liste des noms des event device trouvés
				   estUnEventDevice ,    // Fonction qui filtre les entrées du répertoire (pour ne garder que les event devices)
				   versionsort           // Fonction de trie des entrées (déjà fournie)
				   );

      int descripteurDeFichierRecherche = -1;

      if ( nbEventDevice > 0 )
	{
	  int numDevice;

	  for ( numDevice = 0 ; numDevice < nbEventDevice ; numDevice++ )
	    {
	      char nomDeFichier [64];

	      snprintf (
			nomDeFichier                   ,
			sizeof (nomDeFichier)          ,  // On limite la taille du texte copié dans nomDeFichier à 64 (la taille du tableau)
			"/dev/input/%s"                ,  // On ajouter /dev/input devant le nom du device pour obtenir le nom du fichier
			listeDeNoms[numDevice]->d_name    // le nom complet de l'event device
			);

	      int descripteurDeFichier = open (nomDeFichier, O_RDONLY);

	      if ( descripteurDeFichier < 0)
		continue;

	      char nom [256];

	      /*
                La fonction ioctl() modifie le comportement des périphériques sous-jacents des fichiers spéciaux.
                Par exemple pour rendre bloquant ou non bloquant l'accès à un fichier

                Dans le cas présent, EVIOCGNAME permet d'obtenir le nom du device => http://www.linuxjournal.com/article/6429
	      */

	      ioctl (
		     descripteurDeFichier       , // Le descripteur de fichier désignant l'event device testé
		     EVIOCGNAME ( sizeof(nom) ) , // Une fonction permettant de récupérer le nom de l'event device
		     nom                          // Le pointeur vers la zone de stockage du nom
		     );

	      printf ("%s : %d : %s\n", nomDeFichier , descripteurDeFichier , nom);

	      if ( strcmp ( "Raspberry Pi Sense HAT Joystick" ,  nom ) == 0 )
		descripteurDeFichierRecherche = descripteurDeFichier;
	      else close (descripteurDeFichier);
	    }
	}

      // On désalloue la mémoire allouée par scandir

      for ( int numDevice = 0 ; numDevice < nbEventDevice ; numDevice++ )
	free ( listeDeNoms [numDevice] );

      if ( descripteurDeFichierRecherche < 0 )
	{
	  printf ( "L'event device n'a pas été trouvé");

	  return NULL;
	}

      joystick->evpoll.events = POLLIN;
      joystick->evpoll.fd     = descripteurDeFichierRecherche;
    }

  return joystick;
}

void Joystick_detruire ( Joystick* j)
{
  if ( j )
    {
      close (j->evpoll.fd);

      free (j);
    }
}

bool onClick_event (Joystick* j, int *status)
{
  int currentStatus = *status;
  if (j)
    {
      struct input_event ev[64];
      int i, rd;

      rd = read(j->evpoll.fd, ev, sizeof(struct input_event) * 64);

      if (rd < (int) sizeof(struct input_event))
	{
	  fprintf(stderr, "expected %d bytes, got %d\n",
		  (int) sizeof(struct input_event), rd);
	  return 0;
	}

      for (i = 0; i < rd / sizeof(struct input_event); i++)
	{
	  if (ev->type != EV_KEY)
	    continue;

	  if (ev->value != 1)
	    continue;

	  if (ev->code == KEY_ENTER)
	    {
	      switch(currentStatus)
		{
		case 1 : *status = 2; break;
		case 2 : *status = 3; break;
		case 3 : *status = 1; break;
		}
	      return 1;
	    }
	}
    }
  return 0;
}


int main (int argc, char** argv)
{
  Joystick* j = Joystick_creer ();

  int status = 1;

  while (1)
    {
      if(onClick_event(j, &status)){
	continue;
      }
      printf ("%d\n", status);
    }

  Joystick_detruire (j);

  return 0;
}
