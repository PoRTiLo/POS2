#define _POSIX_C_SOURCE 199506
#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED 1

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>

#define ASCII0 48                            /* v ASCII tabulce hodnota cisla '0' */

volatile int ticket;                         /* jiz prideleny listek */
volatile int ticketAct;                      /* aktualni hodnota listku v KS */
struct Spthread *threads;                    /* pole vlaken */
int countSynchr = 0;                         /* Pocet pruchodu kritickou sekci */
int countThreads = 0;                        /* Pocet vlaken */
struct Spthread {                            /* struktura obsahuji info o vlaknech */
   int volatile ticket;                      /* prideleny vstupni listekj do kriticke sekce */
   pthread_t id;                             /* id vlakna */
};
pthread_mutex_t var_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

int isNumber(char argv[]);                   /* Kontrol zda je cele cislo */
int randomWait(int threadID);                /* nahodn ecekani */
int waitMoment(void);                        /* cekani 0.05 s */
int checkArg(int argc, char *argv[]);        /* kontrola vstupnich argumentu */
void *threadFunc(void *thread_id);           /* funkce vlaken */
int getticket(void);                         /* prirazuje unikatni cislo listku */
void await(int aenter);                      /* vstup do KS */
void advance(void);                          /* vystup z KS */


int main(int argc, char *argv[]) {

   intptr_t i;                               /* pomocna promenna */
   int result;                               /* Promenna obsahujici vysledky funkci */
   ticket = -1;                              /* cislo jiz predeleno listku */
   ticketAct = 0;                            /* aktualni listek ktery muze do KS */
   if(checkArg(argc, argv) == -1 ) {         /* Kontrola vstupnich argumentu */
      return -1;                             /* Nastala chyba, koncim */
   }

   /* Tvorba vlaken */
   if( (threads = (struct Spthread*) malloc(sizeof(struct Spthread) * countThreads)) == NULL) { /* Dynamicka alokace pole vlaken*/
      printf("CHYBA: Nepovedla se dynamicka alokace pole sdruzujici informace o vlaknech.\n");
      return -1;
   }

   for(i = 0; i < countThreads; i++) {       /* Vytvoreni vlaken */
      threads[i].ticket = -1;                /* Inicializace listku */
      result = pthread_create(&threads[i].id, NULL, threadFunc, (void *) i);
      if(result == EAGAIN) {                 /* Tvorba vlaken docasne nedostupna, zkusim znovu */
         waitMoment();                       /* Pockam chvili */
         i--;                                /* zkusim stejne vlakno znovu */
      }
      else if(result) {                      /* Nepovedlo se vytvorit vlakno, koncim */
         printf("CHYBA: Nepovedlo se vytvorit vlakno. Kod chyby: %d.\n", result);
         free(threads);                      /* uvolneni pameti */
         return -1;
      }
   }

   for(i = 0; i < countThreads; i++) {       /* Prirazeni vlakna do sekce cekani na dokonceni */
      if((result = pthread_join(threads[i].id, NULL)) != 0 ) {   /* cekani na dokonceni vlakna */
         printf("CHYBA: Nepovedlo se nasatvit cekani na konec tohoto vlakna. Kod chyby: %d.\n", result);
         free(threads);                      /* uvolneni pameti */
         return -1;
      }
   }
   free(threads);                            /* uvolneni pameti */
   return 0;
}


/**
 * @brief   Funkce, kterou provadi vsechna vlakna.
 * @param   Identifikacni cislo vlakna.
 */
void *threadFunc(void *threadId) {
   int localTic;
   while ((localTic = getticket()) < countSynchr) { /* Prideleni listku */
      threads[(intptr_t)threadId].ticket = localTic;
      randomWait((intptr_t)threadId);        /* Nahodne cekana v intervalu <0,0 s, 0,5 s> */
      await(threads[(intptr_t)threadId].ticket);  /* Vstup do KS */
      printf("%d\t(%d)\n", threads[(intptr_t)threadId].ticket, (intptr_t)threadId+1); /* fflush(stdout); */
      advance();                             /* Vystup z KS */
      randomWait((intptr_t)threadId);        /* Nahodne cekani v intervalu <0,0 s, 0,5 s> */
   }
   return (NULL);
}

/**
 * @brief   Prideluje unikatni cislo listku do vstupu do KS.
 * @return  Unikatni cislo listku, ktery urcuje poradi do KS.
 */
int getticket(void) {
   pthread_mutex_lock(&var_mutex);           /* ziskani vyhradniho pristupu */
   ticket++;                                 /* operace se sdilenou promennou */
   pthread_mutex_unlock(&var_mutex);         /* uvolneni pristupu */
   return ticket;
}

/**
 * @brief   Vstup do kriticke sekce.
 * @param   aenter je cislo prideleneho listku funkci getticket().
 */
void await(int aenter) {
   pthread_mutex_lock(&var_mutex);           /* ziskani vyhradniho pristupu */
   while(aenter <= countSynchr) {            /* dokud nebylo v KS moc vlaken */
      if(aenter == ticketAct) {              /* muj listek se shoduje s aktualnim */
         return;                             /* vstoupim do KS */
      }
      pthread_cond_wait(&cond, &var_mutex);  /* uspim se, cekam, na uvoleni KS */
   }
}

/**
 * @brief   Vystup z kriticke sekce, coz umozni vstup jinemu vlaknu pres funkci
 *          await() s listkem o jednicku vyssim, nez melo vlakno kritickou 
 *          sekci prave opoustejici.
 */
void advance(void) {
   pthread_mutex_unlock(&var_mutex);         /* odemknu */
   ticketAct++;                              /* zvysim ticket o jedno */
   if(pthread_cond_broadcast(&cond) != 0) {  /* poslu vsem ze jsem prosel KS, muzou vstoupit */
      printf("CHYBA: u Brodcastu.\n");
   }
}

/**
 * @brief   Kontroluje a prevadi pole znaku na jedno cislo.
 * @param   Pole znaku.
 * @return  Cislena hodnota.
 */
int isNumber(char argv[]) {
   int i;
   int num = 0;
   int m = 1;
   for(i = 0; argv[i] != '\0'; i++) {        /* zjisteni delky pole */
      ;
   }
   for(i--; i >= 0; i--) {                   /* vypocet cisla */
      if(argv[i] - ASCII0 >= 0 && argv[i] - ASCII0 <= 9) {
         num = num + ((argv[i] - ASCII0) *  m);
         m = m * 10;
      }
      else {
         return -1;
      }
   }
   return num;                               /* vysledne cislo */
}

/**
 * @brief   Funkce ktera ceka 0.05s.
 * @return  Stav provedeni, -1 chyba, 0 ok.
 */
int waitMoment(void) {
   struct timespec rem;
   struct timespec req;
   req.tv_sec = 0;
   req.tv_nsec = 5000000;
   if(nanosleep(&req, &rem) != 0) {
      /*printf("VAROVANI: Cekani se nezdarilo!\n");*/
      /*return -1;*/
   }
   return 0;
}

/**
 * @brief   Nahodne cekani v rozmezi od 0-0.5s.
 * @param   Id vlakna./
 * @return  Stav provedeneho cekani.
 */
int randomWait(int threadId) {
   struct timespec rem;
   struct timespec req;
   unsigned int seed = (threadId+1) * getpid();/* Inicializace SEED - podle id vlakna i PID procesu */
   req.tv_sec = 0;
   req.tv_nsec = rand_r(&seed) % 500000000;  /* Pseudogenerator, nastaveni podle man stranky */
   if(nanosleep(&req, &rem) != 0) {
      /*printf("VAROVANI: Cekani se nezdarilo");*/
      /*return -1;*/
   }
   return 0;
}

/**
 * @brief   Kontrola poctu a spravnosti argumentu.
 * @param   Pocet a obsah argumentu.
 * @return  Stav kontroly.
 */
int checkArg(int argc, char *argv[]) {
   if(argc == 1 || argc == 2) {              /* Tisk napovedy */
      printf("--------------------------------------------------\n \
 NAPOVEDA:\n \
         ./proj02 N M\n\
          N ... pocet vlaken, ktera se maji vytvorit\n\
          M ... celkovy pocet pruchodu kritickoou sekci\n\
         \n \
 Pr: ./proj02 1024 100 vytvori 1024 vlaken schopnych se vzajemne \n \
 synchronizovat a v kriticke sekci se ocitne postupn¿ 100 vlaken. \n \
\n \
 Pokud nebude pocet vlaken nebo pocet pruchodu zadan, vypise se zpusob \n \
 pouziti programu.\n\
--------------------------------------------------\n \
");
      return -1;
   }
   else if(argc == 3) {
      if( (countThreads = isNumber(argv[1])) == -1) {  /* Neni cislo, ukoncim */
         printf("CHYBA: Pocet vlaken neni zadan jako cislo.\n");
         return -1;
      }
      if( (countSynchr = isNumber(argv[2])) == -1) {  /* Neni cislo, ukoncim */
         printf("CHYBA: Pocet pruchodu kritickou sekci neni zadan jako cislo.\n");
         return -1;
      }
   }
   else {                                    /* Chyba spatny pocet argumentu*/
      printf("CHYBA: Spatny pocet argumentu.\n");
      return -1;
   }
   return 0;
}
