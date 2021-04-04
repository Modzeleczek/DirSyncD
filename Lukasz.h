// include guardy
#ifndef LUKASZ_H
#define LUKASZ_H

/*
odczytuje:
argc - liczba parametrów programu (opcji i argumentów razem)
argv - parametry programu

zapisuje:
source - ścieżka źródłowa
destination - ścieżka docelowa
interval - czas spania w sekundach
recursive - rekurecyjna synchronizacja katalogów
threshold - minimalna wielkość pliku, żeby był traktowany jako duży

zwraca:
< 0, jeżeli wystąpił błąd
0, jeżeli nie wystąpił błąd
*/
int parseParameters(int argc, char **argv, char **source, char **destination, unsigned int *interval, char *recursive);

/*
odczytuje:
path - ścieżka do katalogu

zwraca:
-1, jeżeli wystąpił błąd podczas otwierania katalogu
-2, jeżeli wystąpił błąd podczas zamykania katalogu
0, jeżeli nie wystąpił błąd
*/
int directoryValid(const char *path);

/*
odczytuje:
source - ścieżka źródłowa
destination - ścieżka docelowa
interval - czas spania w sekundach
recursive - rekurecyjna synchronizacja katalogów
threshold - minimalna wielkość pliku, żeby był traktowany jako duży
*/
void startDaemon(char *source, char *destination, unsigned int interval, char recursive);

typedef struct element element;
/*
Węzeł listy jednokierunkowej przechowujący wskaźnik do elementu katalogu.
*/
struct element;

typedef struct list list;
/*
Lista jednokierunkowa.
*/
struct list;
/*
odczytuje:
l - lista jednokierunkowa
*/
void initialize(list *l);

#endif