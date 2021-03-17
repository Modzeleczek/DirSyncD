/*
argumenty:
sciezka_zrodlowa - ścieżka do katalogu, z którego kopiujemy
sciezka_docelowa - ścieżka do katalogu, do którego kopiujemy
dodatkowe opcje:
-i <wartosc> - czas spania
-R - rekurencyjna synchronizacja katalogów
sposób użycia:
DirSyncD [-i <wartosc>] [-R] sciezka_zrodlowa sciezka_docelowa
*/

#include "Anna.h"
#include "Lukasz.h"
#include "Mariusz.h"

#include <stdio.h>

int main(int argc, char **argv)
{
    char *source, *destination;
    unsigned int interval;
    char recursive;
    if(parseParameters(argc, argv, &source, &destination, &interval, &recursive) < 0) // jeżeli błąd, to kończymy
    {
        printUsage();
        return -1;
    }
    if(directoryValid(source) < 0) // jeżeli operacje na katalogu źródłowym powodują błąd, to kończymy
    {
        perror(source); // wypisujemy błąd umieszczony w zmiennej errno
        return -2;
    }
    if(directoryValid(destination) < 0) // jeżeli operacje na katalogu docelowym powodują błąd, to kończymy
    {
        perror(destination); // wypisujemy błąd umieszczony w zmiennej errno
        return -3;
    }

    return 0;
}