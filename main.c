/*
argumenty:
sciezka_zrodlowa - ścieżka do katalogu, z którego kopiujemy
sciezka_docelowa - ścieżka do katalogu, do którego kopiujemy
dodatkowe opcje:
-i <czas_spania> - czas spania
-R - rekurencyjna synchronizacja katalogów
-t <prog_duzego_pliku> - minimalny rozmiar pliku, żeby był on potraktowany jako duży
sposób użycia:
DirSyncD [-i <czas_spania>] [-R] [-t <prog_duzego_pliku>] sciezka_zrodlowa sciezka_docelowa
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
    unsigned long long threshold;
    if(parseParameters(argc, argv, &source, &destination, &interval, &recursive, &threshold) < 0) // jeżeli błąd, to kończymy
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

    startDaemon(); // program ze startDaemon już nie wraca do maina

    return 0; // ani proces rodzicielski ani potomny nie dochodzą do tego miejsca, ale piszemy dla zasady, bo main zwraca int
}