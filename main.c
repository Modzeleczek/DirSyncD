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

    return 0;
}