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

zwraca:
< 0, jeżeli wystąpił błąd
0, jeżeli nie wystąpił błąd
*/
int parseParameters(int argc, char **argv, char **source, char **destination, unsigned int *interval, char *recursive);

/*
wypisuje sposób użycia programu na stdout
*/
void printUsage();

/*
odczytuje:
path - ścieżka do katalogu

zwraca:
-1, jeżeli wystąpił błąd podczas otwierania katalogu
-2, jeżeli wystąpił błąd podczas zamykania katalogu
0, jeżeli nie wystąpił błąd
*/
int directoryValid(const char *path);

#endif