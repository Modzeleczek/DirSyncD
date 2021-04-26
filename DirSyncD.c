#include "DirSyncD.h"

#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <syslog.h>

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
wysłanie sygnału SIGUSR1 do demona:
- podczas spania - przedwczesne obudzenie
- podczas synchronizacji - wymuszenie ponownej synchronizacji natychmiast po zakończeniu aktualnej
wysłanie sygnału SIGTERM do demona:
- podczas spania - zakończenie demona
- podczas synchronizacji - zakończenie demona po zakończeniu synchronizacji, o ile podczas niej nie zostanie wysłany również SIGUSR1
*/
int main(int argc, char **argv)
{
    char *source, *destination;
    unsigned int interval;
    char recursive;
    // Analizujemy (parsujemy) parametry podane przy uruchomieniu programu. Jeżeli wystąpił błąd
    if (parseParameters(argc, argv, &source, &destination, &interval, &recursive) < 0)
    {
        // Wypisujemy prawidłowy sposób użycia programu.
        printf("sposob uzycia: DirSyncD [-i <czas_spania>] [-R] [-t <prog_duzego_pliku>] sciezka_zrodlowa sciezka_docelowa\n");
        // Kończymy proces rodzicielski.
        return -1;
    }
    // Sprawdzamy, czy katalog źródłowy jest prawidłowy. Jeżeli jest nieprawidłowy
    if (directoryValid(source) < 0)
    {
        // Wypisujemy błąd o kodzie umieszczonym w zmiennej errno.
        perror(source);
        // Kończymy proces rodzicielski.
        return -2;
    }
    // Sprawdzamy, czy katalog docelowy jest prawidłowy. Jeżeli jest nieprawidłowy
    if (directoryValid(destination) < 0)
    {
        // Wypisujemy błąd o kodzie umieszczonym w zmiennej errno.
        perror(destination);
        // Kończymy proces rodzicielski.
        return -3;
    }

    // Uruchamiamy demona.
    runDaemon(source, destination, interval, recursive);

    return 0;
}

struct element
{
    // Następny element listy.
    element *next;
    // Wskaźnik na element katalogu (plik lub podkatalog).
    struct dirent *entry;
};
int cmp(element *a, element *b)
{
    // Funkcja porównująca elementy listy (porządek leksykograficzny).
    return strcmp(a->entry->d_name, b->entry->d_name);
}

struct list
{
    // Pierwszy i ostatni element. Zapisujemy wskaźnik na ostatni element, aby w czasie stałym dodawać elementy do listy.
    element *first, *last;
    // Liczba elementów listy.
    unsigned int count;
};
void initialize(list *l)
{
    // Ustawiamy wskaźniki do pierwszego i ostatnego elementu na NULL.
    l->first = l->last = NULL;
    // Zerujemy liczbę elementów.
    l->count = 0;
}
int pushBack(list *l, struct dirent *newEntry)
{
    element *new = NULL;
    // Rezerwujemy pamięć na nowy element listy. Jeżeli wystąpił błąd
    if ((new = malloc(sizeof(element))) == NULL)
        // Zwracamy kod błędu.
        return -1;
    // Zapisujemy w elemencie listy wskaźnik na element katalogu.
    new->entry = newEntry;
    // Ustawiamy na NULL wskaźnik na następny element listy.
    new->next = NULL;
    // Jeżeli lista jest pusta, czyli first == NULL i last == NULL
    if (l->first == NULL)
    {
        // Ustawiamy nowy element jako pierwszy
        l->first = new;
        // i ostatni.
        l->last = new;
    }
    // Jeżeli lista nie jest pusta
    else
    {
        // Ustawiamy nowy element jako następny po aktualnie ostatnim.
        l->last->next = new;
        // Przestawiamy aktualnie ostatni na nowy element.
        l->last = new;
    }
    // Zwiększamy liczbę elementów.
    ++l->count;
    // Zwracamy kod poprawnego zakończenia.
    return 0;
}
void clear(list *l)
{
    // Zapisujemy wskaźnik na pierwszy element listy.
    element *cur = l->first, *next;
    while (cur != NULL)
    {
        // Zapisujemy wskaźnik na następny element.
        next = cur->next;
        // Zwalniamy pamięć elementu.
        free(cur);
        // Przesuwamy wskaźnik na następny element.
        cur = next;
    }
    // Zerujemy pola listy.
    initialize(l);
}
void listMergeSort(list *l)
{
    element *p, *q, *e, *tail, *list = l->first;
    int insize, nmerges, psize, qsize, i;
    if (list == NULL) // Silly special case: if `list' was passed in as NULL, return immediately.
        return;
    insize = 1;
    while (1)
    {
        p = list;
        list = NULL;
        tail = NULL;
        nmerges = 0; // count number of merges we do in this pass
        while (p)
        {
            nmerges++; // there exists a merge to be done
            // step `insize' places along from p
            q = p;
            psize = 0;
            for (i = 0; i < insize; i++)
            {
                psize++;
                q = q->next;
                if (!q)
                    break;
            }
            // if q hasn't fallen off end, we have two lists to merge
            qsize = insize;
            // now we have two lists; merge them
            while (psize > 0 || (qsize > 0 && q))
            {
                // decide whether next element of merge comes from p or q
                if (psize == 0)
                {
                    // p is empty; e must come from q.
                    e = q;
                    q = q->next;
                    qsize--;
                }
                else if (qsize == 0 || !q)
                {
                    // q is empty; e must come from p.
                    e = p;
                    p = p->next;
                    psize--;
                }
                else if (cmp(p, q) <= 0)
                {
                    // First element of p is lower (or same); e must come from p.
                    e = p;
                    p = p->next;
                    psize--;
                }
                else
                {
                    // First element of q is lower; e must come from q.
                    e = q;
                    q = q->next;
                    qsize--;
                }
                // add the next element to the merged list
                if (tail)
                {
                    tail->next = e;
                }
                else
                {
                    list = e;
                }
                tail = e;
            }
            // now p has stepped `insize' places along, and q has too
            p = q;
        }
        tail->next = NULL;
        // If we have done only one merge, we're finished.
        if (nmerges <= 1) // allow for nmerges==0, the empty list case
        {
            l->last = tail;
            l->first = list;
            return;
        }
        // Otherwise repeat, merging lists twice the size
        insize *= 2;
    }
}

// static - zmienna globalna widoczna tylko w tym pliku od momentu deklaracji w dół
// Poziom dużego pliku. Jeżeli rozmiar pliku jest mniejszy od threshold, to plik podczas kopiowania jest traktowany jako mały, w przeciwnym razie jako duży.
static unsigned long long threshold;
// Rozmiar bufora do kopiowania pliku.
#define BUFFERSIZE 4096

void stringAppend(char *dst, const size_t offset, const char *src)
{
    // Przesuwamy się o offset bajtów względem początku napisu dst. Począwszy od obliczonej pozycji wstawiamy napis src.
    strcpy(dst + offset, src);
    // Można użyć strcat, ale on dopisuje src na końcu dst i prawdopodobnie oblicza długość dst strlenem, marnując czas.
}
size_t appendSubdirectoryName(char *path, const size_t pathLength, const char *subName)
{
    // Dopisujemy nazwę podkatalogu do ścieżki katalogu źródłowego.
    stringAppend(path, pathLength, subName);
    // Wyznaczamy długość ścieżki podkatalogu jako sumę długości ścieżki katalogu źródłowego i nazwy podkatalogu.
    size_t subPathLength = pathLength + strlen(subName);
    // Dopisujemy '/' do utworzonej ścieżki podkatalogu.
    stringAppend(path, subPathLength, "/");
    // Zwiększamy długość ścieżki podkatalogu o 1, bo dopisaliśmy '/'.
    subPathLength += 1;
    // Zwracamy długość utworzonej ścieżki podkatalogu.
    return subPathLength;
}

int parseParameters(int argc, char **argv, char **source, char **destination, unsigned int *interval, char *recursive)
{
    // Jeżeli nie podano żadnych parametrów
    if (argc <= 1)
        // Zwracamy kod błędu.
        return -1;
    // Zapisujemy domyślny czas spania w sekundach równy 5*60 s = 5 min.
    *interval = 5 * 60;
    // Zapisujemy domyślny brak rekurencyjnej synchronizacji katalogów.
    *recursive = 0;
    // Zapisujemy domyślny próg dużego pliku równy maksymalnej wartości zmiennej typu unsigned long long int.
    threshold = ULLONG_MAX;
    int option;
    // Umieszczamy ':' na początku __shortopts, aby móc rozróżniać między '?' (nieznaną opcją) i ':' (brakiem podania wartości dla opcji)
    while ((option = getopt(argc, argv, ":Ri:t:")) != -1)
    {
        switch (option)
        {
        case 'R':
            // Ustawiamy rekurencyjną synchronizację katalogów.
            *recursive = (char)1;
            break;
        case 'i':
            // Ciąg znaków optarg jest czasem spania w sekundach. Zamieniamy go na liczbę typu unsigned int. Jeżeli sscanf nie wypełnił poprawnie zmiennej interval, to podana wartość czasu ma niepoprawny format i
            if (sscanf(optarg, "%u", interval) < 1)
                // Zwracamy kod błędu.
                return -2;
            break;
        case 't':
            // Ciąg znaków optarg jest progiem dużego pliku. Zamieniamy go na unsigned long long int. Jeżeli sscanf nie wypełnił poprawnie zmiennej THRESHOLD, to podana wartość rozmiaru pliku ma niepoprawny format i
            if (sscanf(optarg, "%llu", &threshold) < 1)
                // Zwracamy kod błędu.
                return -3;
            break;
        case ':':
            // Jeżeli podano opcję -i lub -t, ale nie podano jej wartości, to wypisujemy komunikat
            printf("opcja wymaga podania wartosci\n");
            // Zwracamy kod błędu.
            return -4;
            break;
        case '?':
            // Jeżeli podano opcję inną niż -R, -i, -t
            printf("nieznana opcja: %c\n", optopt);
            // Zwracamy kod błędu.
            return -5;
            break;
        default:
            // Jeżeli getopt zwróciło wartość inną niż powyższe, co nie powinno się nigdy zdarzyć
            printf("blad");
            // Zwracamy kod błędu.
            return -6;
            break;
        }
    }
    // Wyznaczamy liczbę argumentów, które nie są opcjami (powinny być dokładnie 2: ścieżka źródłowa i docelowa).
    int remainingArguments = argc - optind;
    if (remainingArguments != 2) // Jeżeli nie mamy dokładnie dwóch argumentów
        // zwracamy kod błędu.
        return -7;
    // optind jest indeksem pierwszego argumentu niebędącego opcją sparsowaną przez getopt, czyli ścieżki źródłowej. Zapisujemy ścieżkę źródłową.
    *source = argv[optind];
    // Zapisujemy ścieżkę docelową.
    *destination = argv[optind + 1];
    // Zwracamy kod poprawnego zakończenia.
    return 0;
}

int directoryValid(const char *path)
{
    // Otwieramy katalog.
    DIR *d = opendir(path);
    // Jeżeli wystąpił błąd (m. in. kiedy katalog nie istnieje)
    if (d == NULL)
        // Zwracamy kod błędu.
        return -1;
    // Zamykamy katalog. Jeżeli wystąpił błąd
    if (closedir(d) == -1)
        // Zwracamy kod błędu.
        return -2;
    // Zwracamy kod poprawnego zakończenia, oznaczający, że katalog istnieje i operacje na nim nie powodują błędów.
    return 0;
}

// Flaga wymuszonej synchronizacji ustawiana w funkcji obsługi sygnału SIGUSR1.
char forcedSynchronization;
// Funkcja obsługi sygnału SIGUSR1.
void sigusr1Handler(int signo)
{
    // Ustawiamy flagę wymuszonej synchronizacji.
    forcedSynchronization = 1;
}

// Flaga zakończenia ustawiana w funkcji obsługi sygnału SIGTERM.
char stop;
// Funkcja obsługi sygnału SIGTERM.
void sigtermHandler(int signo)
{
    // Ustawiamy flagę zakończenia.
    stop = 1;
}

void runDaemon(char *source, char *destination, unsigned int interval, char recursive)
{
    // Tworzymy proces potomny.
    pid_t pid = fork();
    // Jeżeli wystąpił błąd podczas wywołania fork jeszcze w procesie rodzicielskim, czyli nie powstał proces potomny.
    if (pid == -1)
    {
        // Wypisujemy błąd o kodzie umieszczonym w zmiennej errno.
        perror("fork");
        // Zamykamy proces rodzicielski ze statusem oznaczającym błąd.
        exit(-1);
    }
    // W procesie rozdzicielskim zmienna pid ma wartość równą PID utworzonego procesu potomnego.
    else if (pid > 0)
    {
        // Będąc wciąż w procesie rodzicielskim, wypisujemy PID procesu potomnego.
        printf("PID procesu potomnego: %i\n", pid);
        // Zamykamy proces rodzicielski ze statusem oznaczającym brak błędów.
        exit(0);
    }
    // Poniższy kod wykonuje się w procesie potomnym, bo w nim pid == 0. Przekształcamy proces potomny w demona (Love R. - "Linux. Programowanie systemowe." str. 177). Wstępnie ustawiamy status oznaczający brak błędu.
    int ret = 0;
    char *sourcePath = NULL, *destinationPath = NULL;
    // W systemie plików ext4 ścieżka bezwzględna może mieć maksymalnie PATH_MAX (4096) bajtów. Rezerwujemy PATH_MAX bajtów na ścieżkę katalogu źródłowego. Jeżeli wystąpił błąd
    if ((sourcePath = malloc(sizeof(char) * PATH_MAX)) == NULL)
        // Ustawiamy status oznaczający błąd. Po tym program natychmiast przechodzi na koniec funkcji.
        ret = -1;
    // Rezerwujemy PATH_MAX bajtów na ścieżkę katalogu docelowego. Jeżeli wystąpił błąd
    else if ((destinationPath = malloc(sizeof(char) * PATH_MAX)) == NULL)
        // Ustawiamy status oznaczający błąd.
        ret = -2;
    // Wyznaczamy ścieżkę bezwzględną katalogu źródłowego. Jeżeli wystąpił błąd
    else if (realpath(source, sourcePath) == NULL)
    {
        // Wypisujemy błąd o kodzie umieszczonym w zmiennej errno. Wciąż możemy to zrobić, bo jeszcze nie przeadresowaliśmy deskryptorów procesu potomnego.
        perror("realpath; source");
        // Ustawiamy status oznaczający błąd.
        ret = -3;
    }
    // Wyznaczamy ścieżkę bezwzględną katalogu docelowego. Jeżeli wystąpił błąd
    else if (realpath(destination, destinationPath) == NULL)
    {
        // Wypisujemy błąd o kodzie umieszczonym w zmiennej errno.
        perror("realpath; destination");
        // Ustawiamy status oznaczający błąd.
        ret = -4;
    }
    // Tworzymy nową sesję i grupę procesów. Jeżeli wystąpił błąd
    else if (setsid() == -1)
        // Ustawiamy status oznaczający błąd.
        ret = -5;
    // Ustawiamy katalog roboczy procesu na "/". Jeżeli wystąpił błąd
    else if (chdir("/") == -1)
        // Ustawiamy status oznaczający błąd.
        ret = -6;
    else
    {
        int i;
        // Obowiązkowo zamykamy stdin, stdout, stderr (deskryptory 0, 1, 2)
        for (i = 0; i <= 2; ++i)
            // Jeżeli wystąpił błąd
            if (close(i) == -1)
            {
                // Ustawiamy status oznaczający błąd.
                ret = -(50 + i);
                break;
            }
    }
    // Jeżeli jeszcze nie wystąpił żaden błąd
    if (ret >= 0)
    {
        int i;
        // Jeżeli są otwarte, to zamykamy dalsze deskryptory (od 3 do 1023), bo domyślnie w Linuxie proces może mieć otwarte maksymalnie 1024 deskryptory.
        for (i = 3; i <= 1023; ++i)
            // Zamykamy deskryptor i. Jeżeli wystąpił błąd, to go ignorujemy.
            close(i);
        sigset_t set;
        // Przeadresowujemy deskryptory 0, 1, 2 na "/dev/null". Ustawiamy deskryptor 0 (stdin, czyli najmniejszy zamknięty) na "/dev/null". Jeżeli wystąpił błąd
        if (open("/dev/null", O_RDWR) == -1)
            // Już nie wywołujemy perror, bo w procesie potomnym nie możemy już wypisać błędu do terminala uruchamiającego proces rodzicielski. Ustawiamy status oznaczający błąd.
            ret = -8;
        // Ustawiamy deskryptor 1 (stdout) na to samo co deskryptor 0 - na "/dev/null". Jeżeli wystąpił błąd
        else if (dup(0) == -1)
            // Ustawiamy status oznaczający błąd.
            ret = -9;
        // Ustawiamy deskryptor 2 (stderr) na to samo co deskryptor 0 - na "/dev/null". Jeżeli wystąpił błąd
        else if (dup(0) == -1)
            // Ustawiamy status oznaczający błąd.
            ret = -10;
        // W tym momencie proces potomny jest już demonem. Rejestrujemy funkcję obsługującą sygnał SIGUSR1. Jeżeli wystąpił błąd
        else if (signal(SIGUSR1, sigusr1Handler) == SIG_ERR)
            // Ustawiamy status oznaczający błąd.
            ret = -11;
        // Rejestrujemy funkcję obsługującą sygnał SIGTERM. Jeżeli wystąpił błąd
        else if (signal(SIGTERM, sigtermHandler) == SIG_ERR)
            // Ustawiamy status oznaczający błąd.
            ret = -12;
        // Inicjujemy zbiór sygnałów jako pusty. Jeżeli wystąpił błąd
        else if (sigemptyset(&set) == -1)
            // Ustawiamy status oznaczający błąd.
            ret = -13;
        // Dodajemy SIGUSR1 do zbioru sygnałów. Jeżeli wystąpił błąd
        else if (sigaddset(&set, SIGUSR1) == -1)
            // Ustawiamy status oznaczający błąd.
            ret = -14;
        // Dodajemy SIGTERM do zbioru sygnałów. Jeżeli wystąpił błąd
        else if (sigaddset(&set, SIGTERM) == -1)
            // Ustawiamy status oznaczający błąd.
            ret = -15;
        else
        {
            // Wyznaczamy długość ścieżki bezwzględnej katalogu źródłowego.
            size_t sourcePathLength = strlen(sourcePath);
            // Jeżeli bezpośrednio przed '\0' (null terminatorem) nie ma '/'
            if (sourcePath[sourcePathLength - 1] != '/')
                // Wstawiamy '/' na miejscu '\0'. Zwiększamy długość ścieżki o 1. Wstawiamy '\0' po '/'.
                stringAppend(sourcePath, sourcePathLength++, "/");
            // Wyznaczamy długość ścieżki bezwzględnej katalogu docelowego.
            size_t destinationPathLength = strlen(destinationPath);
            // Jeżeli bezpośrednio przed '\0' nie ma '/'
            if (destinationPath[destinationPathLength - 1] != '/')
                // Wstawiamy '/' na miejscu '\0'. Zwiększamy długość ścieżki o 1. Wstawiamy '\0' po '/'.
                stringAppend(destinationPath, destinationPathLength++, "/");
            synchronizer synchronize;
            // Jeżeli ustawiona jest synchronizacja nierekurencyjna
            if (recursive == 0)
                // Zapisujemy wskaźnik do funkcji synchronizującej nierekurencyjnie.
                synchronize = synchronizeNonRecursively;
            // Jeżeli ustawiona jest synchronizacja rekurencyjna
            else
                // Zapisujemy wskaźnik do funkcji synchronizującej rekurencyjnie.
                synchronize = synchronizeRecursively;
            // Wstępnie ustawiamy 0, bo zmienna służy do zakończenia demona (przerwania pętli) sygnałem SIGTERM.
            stop = 0;
            // Wstępnie ustawiamy 0, bo zmienna służy do pomijania startu spania sygnałem SIGUSR1.
            forcedSynchronization = 0;
            while (1)
            {
                // Jeżeli nie wymuszono synchronizacji sygnałem SIGUSR1
                if (forcedSynchronization == 0)
                {
                    // Otwieramy połączenie z logiem ("/var/log/syslog").
                    openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON);
                    // Zapisujemy do logu informację o uśpieniu.
                    syslog(LOG_INFO, "uspienie");
                    // Zamykamy połączenie z logiem.
                    closelog();
                    // Usypiamy demona.
                    unsigned int timeLeft = sleep(interval);
                    // Otwieramy połączenie z logiem.
                    openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON);
                    // Zapisujemy do logu informację o obudzeniu z liczbą przespanych sekund.
                    syslog(LOG_INFO, "obudzenie; przespano %u s", interval - timeLeft);
                    // Zamykamy połączenie z logiem.
                    closelog();
                    // Jeżeli spanie zostało przerwane odebraniem SIGTERM
                    if (stop == 1)
                        // Przerywamy pętlę.
                        break;
                }
                // Włączamy blokowanie sygnałów ze zbioru, czyli SIGUSR1 i SIGTERM. Jeżeli wystąpił błąd
                if (sigprocmask(SIG_BLOCK, &set, NULL) == -1)
                {
                    // Ustawiamy status oznaczający błąd.
                    ret = -16;
                    // Przerywamy pętlę.
                    break;
                }
                // Rozpoczynamy synchronizację wybraną funkcją. Pomijamy błędy, ale zapisujemy do logu status. 0 oznacza, że cała synchronizacja przebiegła bez błędów. Wartość różna od 0 oznacza, że katalogi mogą nie być zsynchronizowane.
                int status = synchronize(sourcePath, sourcePathLength, destinationPath, destinationPathLength);
                // Otwieramy połączenie z logiem.
                openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON);
                // Zapisujemy do logu informację o zakończeniu synchronizacji ze statusem.
                syslog(LOG_INFO, "koniec synchronizacji; %i", status);
                // Zamykamy połączenie z logiem.
                closelog();
                // Niezależnie, czy synchronizacja była wymuszona, czy samoczynna po przespaniu całego czasu, ustawiamy 0 po jej zakończeniu.
                forcedSynchronization = 0;
                // Wyłączamy blokowanie sygnałów ze zbioru, czyli SIGUSR1 i SIGTERM. Jeżeli wystąpił błąd
                if (sigprocmask(SIG_UNBLOCK, &set, NULL) == -1)
                {
                    // Ustawiamy status oznaczający błąd.
                    ret = -17;
                    // Przerywamy pętlę.
                    break;
                }
                // Jeżeli podczas synchronizacji odebraliśmy SIGUSR1 lub SIGTERM, to po wyłączeniu ich blokowania zostaną wykonane funkcje ich obsługi.
                // Jeżeli podczas synchronizacji odebraliśmy SIGUSR1, to po jej zakończeniu od razu zostanie wykonana kolejna synchronizacja.
                // Jeżeli podczas synchronizacji nie odebraliśmy SIGUSR1 i odebraliśmy SIGTERM, to po jej zakończeniu demon się zakończy.
                // Jeżeli podczas synchronizacji odebraliśmy SIGUSR1 i SIGTERM, to po jej zakończeniu najpierw zostanie wykonana kolejna synchronizacja, a jeżeli podczas niej nie odbierzemy SIGUSR1, to po niej demon się zakończy. Jeżeli jednak podczas tej drugiej synchronizacji odbierzemy SIGUSR1, to zostanie wykonana trzecia synchronizacja, itd.. Po zakończeniu pierwszej synchronizacji, podczas której nie odbierzemy SIGUSR1, demon się zakończy
                // Jeżeli nie wymuszono kolejnej synchronizacji i wymuszono zakończenie
                if (forcedSynchronization == 0 && stop == 1)
                    // Przerywamy pętlę.
                    break;
            }
        }
    }
    // Jeżeli w którymś momencie wystąpił błąd, to przechodzimy do tego miejsca.
    // Jeżeli pamięć na ścieżkę katalogu źródłowego została zarezerwowana
    if (sourcePath != NULL)
        // Zwalniamy pamięć.
        free(sourcePath);
    // Jeżeli pamięć na ścieżkę katalogu docelowego została zarezerwowana
    if (destinationPath != NULL)
        // Zwalniamy pamięć.
        free(destinationPath);
    // Otwieramy połączenie z logiem.
    openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON);
    // Zapisujemy do logu informację o zakończeniu demona ze statusem.
    syslog(LOG_INFO, "zakonczenie; %i", ret);
    // Zamykamy połączenie z logiem.
    closelog();
    // Kończymy proces demona.
    exit(ret);
}

int listFiles(DIR *dir, list *files)
{
    struct dirent *entry;
    // Wstępnie ustawiamy errno na 0.
    errno = 0;
    // Odczytujemy element katalogu. Jeżeli nie wystąpił błąd
    while ((entry = readdir(dir)) != NULL)
    {
        // Jeżeli element jest zwykłym plikiem (regular file), to dodajemy go do listy plików. Jeżeli wystąpił błąd
        if (entry->d_type == DT_REG && pushBack(files, entry) < 0)
            // Przerywamy, zwracając kod błędu, bo listy elementów muszą być kompletne do porównywania katalogów podczas synchronizacji.
            return -1;
    }
    // Jeżeli wystąpił błąd podczas odczytywania elementu, to readdir zwrócił NULL i ustawił errno na wartość różną od 0.
    if (errno != 0)
        // Zwracamy kod błędu.
        return -2;
    // Zwracamy kod poprawnego zakończenia.
    return 0;
}
int listFilesAndDirectories(DIR *dir, list *files, list *subdirs)
{
    struct dirent *entry;
    // Wstępnie ustawiamy errno na 0.
    errno = 0;
    // Odczytujemy element katalogu. Jeżeli nie wystąpił błąd
    while ((entry = readdir(dir)) != NULL)
    {
        // Jeżeli element jest zwykłym plikiem (regular file)
        if (entry->d_type == DT_REG)
        {
            // Dodajemy go do listy plików. Jeżeli wystąpił błąd
            if (pushBack(files, entry) < 0)
                // Przerywamy, zwracając kod błędu, bo listy elementów muszą być kompletne do porównywania katalogów podczas synchronizacji.
                return -1;
        }
        // Jeżeli element jest katalogiem (directory)
        else if (entry->d_type == DT_DIR)
        {
            // Jeżeli ma nazwę różną od "." i ".."
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 &&
                // Dodajemy go do listy katalogów. Jeżeli wystąpił błąd
                pushBack(subdirs, entry) < 0)
                // Zwracamy kod błędu.
                return -2;
        }
        // Ignorujemy elementy o innych typach (symlinki; urządzenia blokowe, tekstowe; sockety; itp.).
    }
    // Jeżeli wystąpił błąd podczas odczytywania elementu, to readdir zwrócił NULL i ustawił errno na wartość różną od 0.
    if (errno != 0)
        // Zwracamy kod błędu.
        return -3;
    // Zwracamy kod poprawnego zakończenia.
    return 0;
}

int createEmptyDirectory(const char *path, mode_t mode)
{
    // Tworzymy pusty katalog i zwracamy status.
    return mkdir(path, mode);
}
int removeDirectoryRecursively(const char *path, const size_t pathLength)
{
    // Wstępnie zapisujemy status oznaczający brak błędu.
    int ret = 0;
    DIR *dir = NULL;
    // Otwieramy katalog źródłowy. Jeżeli wystąpił błąd
    if ((dir = opendir(path)) == NULL)
        // Ustawiamy kod błędu. Po tym program natychmiast przechodzi na koniec funkcji.
        ret = -1;
    else
    {
        // Tworzymy listy na pliki i podkatalogi z katalogu.
        list files, subdirs;
        // Inicjujemy listę plików.
        initialize(&files);
        // Inicjujemy listę podkatalogów.
        initialize(&subdirs);
        // Wypełniamy listy plików i podkatalogów. Jeżeli wystąpił błąd
        if (listFilesAndDirectories(dir, &files, &subdirs) < 0)
            // Ustawiamy kod błędu.
            ret = -2;
        else
        {
            char *subPath = NULL;
            // Rezerwujemy pamięć na ścieżki podkatalogów i plików. Jeżeli wystąpił błąd
            if ((subPath = malloc(sizeof(char) * PATH_MAX)) == NULL)
                // Ustawiamy kod błędu.
                ret = -3;
            else
            {
                // Przepisujemy ścieżkę katalogu jako początek ścieżek jego plików i podkatalogów.
                strcpy(subPath, path);
                // Zapisujemy wskaźnik na pierwszy podkatalog.
                element *cur = subdirs.first;
                // Rekurencyjnie usuwamy podkatalogi.
                while (cur != NULL)
                {
                    // Dopisujemy nazwę podkatalogu do ścieżki jego katalogu. Funkcja removeDirectoryRecursively wymaga, aby ścieżka do katalogu była zakończona '/'.
                    size_t subPathLength = appendSubdirectoryName(subPath, pathLength, cur->entry->d_name);
                    // Rekurencyjnie usuwamy podkatalogi. Jeżeli wystąpił błąd
                    if (removeDirectoryRecursively(subPath, subPathLength) < 0)
                        // Ustawiamy kod błędu przeznaczony dla wyższego wywołania.
                        ret = -4;
                    // Przesuwamy wskaźnik na następny podkatalog.
                    cur = cur->next;
                }
                // Zapisujemy wskaźnik na pierwszy plik.
                cur = files.first;
                // Usuwamy pliki.
                while (cur != NULL)
                {
                    // Dopisujemy nazwę pliku do ścieżki jego katalogu.
                    stringAppend(subPath, pathLength, cur->entry->d_name);
                    // Usuwamy plik. Jeżeli wystąpił błąd
                    if (removeFile(subPath) == -1)
                        // Ustawiamy kod błędu.
                        ret = -5;
                    // Przesuwamy wskaźnik na następny plik.
                    cur = cur->next;
                }
                // Zwalniamy pamięć ścieżek plików i podkatalogów.
                free(subPath);
            }
        }
        // Czyścimy listę plików.
        clear(&files);
        // Czyścimy listę podkatalogów.
        clear(&subdirs);
    }
    // Błąd krytyczny występuje, jeżeli nie uda się usunąć któregokolwiek elementu z katalogu.
    // Jeżeli katalog został otwarty, to go zamykamy. Jeżeli wystąpił błąd i kod błędu jeszcze nie jest ujemny, czyli jeszcze nie wystąpił błąd krytyczny
    if (dir != NULL && closedir(dir) == -1 && ret >= 0)
        // Ustawiamy dodatni kod błędu, czyli błąd niekrytyczny.
        ret = 1;
    // Jeżeli nie wystąpił żaden błąd krytyczny, to usuwamy katalog. Jeżeli wystąpił błąd
    if (ret >= 0 && rmdir(path) == -1)
        // Ustawiamy kod błędu.
        ret = -6;
    // Zwracamy status.
    return ret;
}

int copySmallFile(const char *srcFilePath, const char *dstFilePath, const mode_t dstMode, const struct timespec *dstAccessTime, const struct timespec *dstModificationTime)
{
    // Wstępnie zapisujemy status oznaczający brak błędu.
    int ret = 0, in = -1, out = -1;
    // Otwieramy plik źródłowy do odczytu i zapisujemy jego deskryptor. Jeżeli wystąpił błąd
    if ((in = open(srcFilePath, O_RDONLY)) == -1)
        // Ustawiamy kod błędu. Po tym program natychmiast przechodzi na koniec funkcji.
        ret = -1;
    // Otwieramy plik docelowy do zapisu. Jeżeli nie istnieje, to go tworzymy, nadając puste uprawnienia, a jeżeli już istnieje, to go czyścimy. Zapisujemy jego deskryptor. Jeżeli wystąpił błąd
    else if ((out = open(dstFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0000)) == -1)
        // Ustawiamy kod błędu.
        ret = -2;
    // Ustawiamy plikowi docelowemu uprawnienia dstMode. Jeżeli wystąpił błąd
    else if (fchmod(out, dstMode) == -1)
        // Ustawiamy kod błędu.
        ret = -3;
    else
    {
        // (str. 124) Wysyłamy jądru wskazówkę (poradę), że plik źródłowy będzie odczytywany sekwencyjnie i dlatego należy go wczytywać z wyprzedzeniem.
        if (posix_fadvise(in, 0, 0, POSIX_FADV_SEQUENTIAL) == -1)
            // Ustawiamy kod błędu niekrytycznego, bo bez porady też można odczytywać plik źródłowy, ale mniej wydajnie.
            ret = 1;
        char *buffer = NULL;
        // Optymalny rozmiar bufora operacji wejścia-wyjścia dla danego pliku można sprawdzić w jego metadanych pobranych za pomocą stat, ale używamy odgórnie ustalonego rozmiaru.
        // Rezerwujemy pamięć bufora. Jeżeli wystąpił błąd
        if ((buffer = malloc(sizeof(char) * BUFFERSIZE)) == NULL)
            // Ustawiamy kod błędu.
            ret = -4;
        else
        {
            while (1)
            {
                // Poniższy algorytm jest na str. 45.
                // Pozycja w buforze.
                char *position = buffer;
                // Zapisujemy całkowitą liczbę bajtów pozostałych do odczytania.
                size_t remainingBytes = BUFFERSIZE;
                ssize_t bytesRead;
                // Dopóki liczby bajtów pozostałych do odczytania i bajtów odczytanych w aktualnej iteracji są niezerowe.
                while (remainingBytes != 0 && (bytesRead = read(in, position, remainingBytes)) != 0)
                {
                    // Jeżeli wystąpił błąd w funkcji read.
                    if (bytesRead == -1)
                    {
                        // Jeżeli funkcja read została przerwana odebraniem sygnału. Blokujemy SIGUSR1 i SIGTERM na czas synchronizacji, więc te sygnały nie mogą spowodować tego błędu.
                        if (errno == EINTR)
                            // Ponawiamy próbę odczytu.
                            continue;
                        // Jeżeli wystąpił inny błąd
                        // Ustawiamy kod błędu.
                        ret = -5;
                        // BUFFERSIZE - BUFFERSIZE == 0, więc druga pętla się nie wykona
                        remainingBytes = BUFFERSIZE;
                        // Ustawiamy 0, aby warunek if(bytesRead == 0) przerwał zewnętrzną pętlę while(1).
                        bytesRead = 0;
                        // Przerywamy pętlę.
                        break;
                    }
                    // O liczbę bajtów odczytanych w aktualnej iteracji zmniejszamy liczbę pozostałych bajtów i
                    remainingBytes -= bytesRead;
                    // przesuwamy pozycję w buforze.
                    position += bytesRead;
                }
                position = buffer;                            // str. 48
                remainingBytes = BUFFERSIZE - remainingBytes; // zapisujemy całkowitą liczbę odczytanych bajtów, która zawsze jest mniejsza lub równa rozmiarowi bufora
                ssize_t bytesWritten;
                // Dopóki liczby bajtów pozostałych do zapisania i bajtów zapisanych w aktualnej iteracji są niezerowe.
                while (remainingBytes != 0 && (bytesWritten = write(out, position, remainingBytes)) != 0)
                {
                    // Jeżeli wystąpił błąd w funkcji write.
                    if (bytesWritten == -1)
                    {
                        // Jeżeli funkcja write została przerwana odebraniem sygnału.
                        if (errno == EINTR)
                            // Ponawiamy próbę zapisu.
                            continue;
                        // Jeżeli wystąpił inny błąd
                        // Ustawiamy kod błędu.
                        ret = -6;
                        // Ustawiamy 0, aby warunek if(bytesRead == 0) przerwał zewnętrzną pętlę while(1).
                        bytesRead = 0;
                        // Przerywamy pętlę.
                        break;
                    }
                    // O liczbę bajtów zapisanych w aktualnej iteracji zmniejszamy liczbę pozostałych bajtów i
                    remainingBytes -= bytesWritten;
                    // przesuwamy pozycję w buforze.
                    position += bytesWritten;
                }
                // Jeżeli doszliśmy do końca pliku (EOF) lub wystąpił błąd
                if (bytesRead == 0)
                    // Przerywamy pętlę while(1).
                    break;
            }
            // Zwalniamy pamięć bufora.
            free(buffer);
            // Jeżeli nie wystąpił błąd podczas kopiowania
            if (ret >= 0)
            {
                // Tworzymy strukturę zawierającą czasy ostatniego dostępu i modyfikacji.
                const struct timespec times[2] = {*dstAccessTime, *dstModificationTime};
                // Muszą być ustawione po zakończeniu zapisów do pliku docelowego, bo zapisy ustawiają czas modyfikacji na aktualny czas systemowy. Ustawiamy je mu. Jeżeli wystąpił błąd
                if (futimens(out, times) == -1)
                    // Ustawiamy kod błędu.
                    ret = -7;
            }
        }
    }
    // Jeżeli plik źródłowy został otwarty, to go zamykamy. Jeżeli wystąpił błąd
    if (in != -1 && close(in) == -1)
        // Ustawiamy kod błędu. Nie traktujemy go jako błąd niekrytyczny, bo każdy niezamknięty plik zajmuje deskryptor, których proces może mieć tylko ograniczoną liczbę.
        ret = -8;
    // Jeżeli plik docelowy został otwarty, to go zamykamy. Jeżeli wystąpił błąd
    if (out != -1 && close(out) == -1)
        // Ustawiamy kod błędu.
        ret = -9;
    // Zwracamy status.
    return ret;
}
int copyBigFile(const char *srcFilePath, const char *dstFilePath, const unsigned long long fileSize, const mode_t dstMode, const struct timespec *dstAccessTime, const struct timespec *dstModificationTime)
{
    // Wstępnie zapisujemy status oznaczający brak błędu.
    int ret = 0, in = -1, out = -1;
    // Otwieramy plik źródłowy do odczytu i zapisujemy jego deskryptor. Jeżeli wystąpił błąd
    if ((in = open(srcFilePath, O_RDONLY)) == -1)
        // Ustawiamy kod błędu. Po tym program natychmiast przechodzi na koniec funkcji.
        ret = -1;
    // Otwieramy plik docelowy do zapisu. Jeżeli nie istnieje, to go tworzymy, nadając puste uprawnienia, a jeżeli już istnieje, to go czyścimy. Zapisujemy jego deskryptor. Jeżeli wystąpił błąd
    else if ((out = open(dstFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0000)) == -1)
        // Ustawiamy kod błędu.
        ret = -2;
    // Ustawiamy plikowi docelowemu uprawnienia dstMode. Jeżeli wystąpił błąd
    else if (fchmod(out, dstMode) == -1)
        // Ustawiamy kod błędu.
        ret = -3;
    else
    {
        char *map;
        // Odwzorowujemy (mapujemy) w pamięci plik źródłowy w trybie do odczytu. Jeżeli wystąpił błąd
        if ((map = mmap(0, fileSize, PROT_READ, MAP_SHARED, in, 0)) == MAP_FAILED)
            // Ustawiamy kod błędu.
            ret = -4;
        else
        {
            // (str. 121) Wysyłamy jądru wskazówkę (poradę), że plik źródłowy będzie odczytywany sekwencyjnie. Jeżeli wystąpił błąd
            if (madvise(map, fileSize, MADV_SEQUENTIAL) == -1)
                // Ustawiamy kod błędu niekrytycznego, bo bez porady też można odczytywać plik źródłowy, ale mniej wydajnie.
                ret = 1;
            char *buffer = NULL;
            // Optymalny rozmiar bufora operacji wejścia-wyjścia dla danego pliku można sprawdzić w jego metadanych pobranych za pomocą stat, ale używamy odgórnie ustalonego rozmiaru.
            // Rezerwujemy pamięć bufora. Jeżeli wystąpił błąd
            if ((buffer = malloc(sizeof(char) * BUFFERSIZE)) == NULL)
                // Ustawiamy kod błędu.
                ret = -5;
            else
            {
                // Numer bajtu w pliku źródłowym.
                unsigned long long b;
                // Pozycja w buforze.
                char *position;
                size_t remainingBytes;
                ssize_t bytesWritten;
                // Nie może być (b < fileSize - BUFFERSIZE), bo b i fileSize są typu unsigned, więc jeżeli fileSize < BUFFERSIZE i odejmiemy, to mamy przepełnienie.
                for (b = 0; b + BUFFERSIZE < fileSize; b += BUFFERSIZE)
                {
                    // Kopiujemy BUFFERSIZE (rozmiar bufora) bajtów ze zmapowanej pamięci do bufora.
                    memcpy(buffer, map + b, BUFFERSIZE);
                    // Poniższy algorytm jest na str. 48.
                    position = buffer;
                    // Zapisujemy całkowitą bajtów pozostałych do zapisania, która zawsze jest równa rozmiarowi bufora.
                    remainingBytes = BUFFERSIZE;
                    // Dopóki liczby bajtów pozostałych do zapisania i bajtów zapisanych w aktualnej iteracji są niezerowe.
                    while (remainingBytes != 0 && (bytesWritten = write(out, position, remainingBytes)) != 0)
                    {
                        // Jeżeli wystąpił błąd w funkcji write.
                        if (bytesWritten == -1)
                        {
                            // Jeżeli funkcja write została przerwana odebraniem sygnału. Blokujemy SIGUSR1 i SIGTERM na czas synchronizacji, więc te sygnały nie mogą spowodować tego błędu.
                            if (errno == EINTR)
                                // Ponawiamy próbę zapisu.
                                continue;
                            // Jeżeli wystąpił inny błąd
                            // Ustawiamy kod błędu.
                            ret = -6;
                            // Ustawiamy b aby przerwać pętlę for.
                            b = ULLONG_MAX - BUFFERSIZE;
                            // Przerywamy pętlę.
                            break;
                        }
                        // O liczbę bajtów zapisanych w aktualnej iteracji zmniejszamy liczbę pozostałych bajtów i
                        remainingBytes -= bytesWritten;
                        // przesuwamy pozycję w buforze.
                        position += bytesWritten;
                    }
                }
                // Jeżeli jeszcze nie wystąpił błąd podczas kopiowania
                if (ret >= 0)
                {
                    // Zapisujemy liczbę bajtów z końca pliku, które nie zmieściły się w jednym całym buforze.
                    remainingBytes = fileSize - b;
                    // Kopiujemy je ze zmapowanej pamięci do bufora.
                    memcpy(buffer, map + b, remainingBytes);
                    // Zapisujemy pozycję pierwszego bajtu bufora.
                    position = buffer;
                    // Dopóki liczby bajtów pozostałych do zapisania i bajtów zapisanych w aktualnej iteracji są niezerowe.
                    while (remainingBytes != 0 && (bytesWritten = write(out, position, remainingBytes)) != 0)
                    {
                        // Jeżeli wystąpił błąd w funkcji write.
                        if (bytesWritten == -1)
                        {
                            // Jeżeli funkcja write została przerwana odebraniem sygnału.
                            if (errno == EINTR)
                                // Ponawiamy próbę zapisu.
                                continue;
                            // Ustawiamy kod błędu.
                            ret = -7;
                            // Przerywamy pętlę.
                            break;
                        }
                        // O liczbę bajtów zapisanych w aktualnej iteracji zmniejszamy liczbę pozostałych bajtów i
                        remainingBytes -= bytesWritten;
                        // przesuwamy pozycję w buforze.
                        position += bytesWritten;
                    }
                }
                // Zwalniamy pamięć bufora.
                free(buffer);
                // Jeżeli nie wystąpił błąd podczas kopiowania
                if (ret >= 0)
                {
                    // Tworzymy strukturę zawierającą czasy ostatniego dostępu i modyfikacji.
                    const struct timespec times[2] = {*dstAccessTime, *dstModificationTime};
                    // Muszą być ustawione po zakończeniu zapisów do pliku docelowego, bo zapisy ustawiają czas modyfikacji na aktualny czas systemowy. Ustawiamy je mu. Jeżeli wystąpił błąd
                    if (futimens(out, times) == -1)
                        // Ustawiamy kod błędu.
                        ret = -8;
                }
            }
            // Usuwamy odwzorowanie pliku źródłowego. Jeżeli wystąpił błąd
            if (munmap(map, fileSize) == -1)
                // Ustawiamy kod błędu.
                ret = -9;
        }
    }
    // Jeżeli plik źródłowy został otwarty, to go zamykamy. Jeżeli wystąpił błąd
    if (in != -1 && close(in) == -1)
        // Ustawiamy kod błędu. Nie traktujemy go jako błąd niekrytyczny, bo każdy niezamknięty plik zajmuje deskryptor, których proces może mieć tylko ograniczoną liczbę.
        ret = -10;
    // Jeżeli plik docelowy został otwarty, to go zamykamy. Jeżeli wystąpił błąd
    if (out != -1 && close(out) == -1)
        // Ustawiamy kod błędu.
        ret = -11;
    // Zwracamy status.
    return ret;
}
int removeFile(const char *path)
{
    // Unlink służy tylko do usuwania plików. Katalog usuwamy za pomocą rmdir, ale najpierw musimy zapewnić, aby był pusty. Usuwamy plik i zwracamy status.
    return unlink(path);
}

int updateDestinationFiles(const char *srcDirPath, const size_t srcDirPathLength, list *filesSrc, const char *dstDirPath, const size_t dstDirPathLength, list *filesDst)
{
    char *srcFilePath = NULL, *dstFilePath = NULL;
    // Rezerwujemy pamięć na ścieżki plików z katalogu źródłowego (pliki źródłowe). Jeżeli wystąpił błąd
    if ((srcFilePath = malloc(sizeof(char) * PATH_MAX)) == NULL)
        // Zwracamy kod błędu.
        return -1;
    // Rezerwujemy pamięć na ścieżki plików z katalogu docelowego (pliki docelowe). Jeżeli wystąpił błąd
    if ((dstFilePath = malloc(sizeof(char) * PATH_MAX)) == NULL)
    {
        // Zwalniamy już zarezerwowaną pamięć.
        free(srcFilePath);
        // Zwracamy kod błędu.
        return -2;
    }
    // Przepisujemy ścieżkę katalogu źródłowego jako początek ścieżek jego plików.
    strcpy(srcFilePath, srcDirPath);
    // Przepisujemy ścieżkę katalogu docelowego jako początek ścieżek jego plików.
    strcpy(dstFilePath, dstDirPath);
    // Zapisujemy wskaźniki na pierwszy plik źródłowy i docelowy.
    element *curS = filesSrc->first, *curD = filesDst->first;
    struct stat srcFile, dstFile;
    // Wstępnie zapisujemy status oznaczający brak błędu.
    int status = 0, ret = 0;
    // Otwieramy połączenie z logiem "/var/log/syslog".
    openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON);
    while (curS != NULL && curD != NULL)
    {
        char *srcFileName = curS->entry->d_name, *dstFileName = curD->entry->d_name;
        // Porównujemy w porządku leksykograficznym nazwy plików źródłowego i docelowego.
        int comparison = strcmp(srcFileName, dstFileName);
        // Jeżeli plik źródłowy jest w porządku późniejszy niż docelowy
        if (comparison > 0)
        {
            // Dopisujemy nazwę pliku docelowego do ścieżki jego katalogu.
            stringAppend(dstFilePath, dstDirPathLength, dstFileName);
            // Usuwamy plik docelowy.
            status = removeFile(dstFilePath);
            // Zapisujemy do logu informację o usunięciu.
            syslog(LOG_INFO, "usuwamy plik %s; %i\n", dstFilePath, status);
            // Jeżeli wystąpił błąd
            if (status != 0)
                // Ustawiamy dodatni kod błędu, aby zaznaczyć niepełną synchronizację, ale nie przerywamy pętli.
                ret = 1;
            // Przesuwamy wskaźnik na następny plik docelowy.
            curD = curD->next;
        }
        else
        {
            // Dopisujemy nazwę pliku źródłowego do ścieżki jego katalogu.
            stringAppend(srcFilePath, srcDirPathLength, srcFileName);
            // Odczytujemy metadane pliku źródłowego. Jeżeli wystąpił błąd, to plik źródłowy jest niedostępny i nie będziemy mogli go również skopiować, gdy comparison < 0.
            if (stat(srcFilePath, &srcFile) == -1)
            {
                // Jeżeli plik źródłowy jest w porządku wcześniejszy niż plik docelowy
                if (comparison < 0)
                {
                    // Zapisujemy do logu informację o nieudanym kopiowaniu. Status zapisany przez stat do errno jest liczbą dodatnią.
                    syslog(LOG_INFO, "kopiujemy plik %s do katalogu %s; %i\n", srcFilePath, dstDirPath, errno);
                    // Ustawiamy kod błędu.
                    ret = 2;
                }
                // Jeżeli plik źródłowy jest w porządku równy plikowi docelowemu
                else
                {
                    // Zapisujemy do logu informację o nieudanym odczycie metadanych.
                    syslog(LOG_INFO, "odczytujemy metadane pliku źródłowego %s; %i\n", srcFilePath, errno);
                    // Przesuwamy wskaźnik na następny plik docelowy.
                    curD = curD->next;
                    // Ustawiamy kod błędu.
                    ret = 3;
                }
                // Przesuwamy wskaźnik na następny plik źródłowy.
                curS = curS->next;
                // Przechodzimy do następnej iteracji pętli.
                continue;
            }
            // Jeżeli poprawnie odczytaliśmy metadane
            // Jeżeli plik źródłowy jest w porządku wcześniejszy niż docelowy
            if (comparison < 0)
            {
                // Dopisujemy nazwę pliku źródłowego do ścieżki katalogu docelowego.
                stringAppend(dstFilePath, dstDirPathLength, srcFileName);
                // Jeżeli plik źródłowy jest mniejszy niż poziom dużego pliku
                if (srcFile.st_size < threshold)
                    // Kopiujemy go jako mały plik. Przepisujemy uprawnienia i czas modyfikacji pliku źródłowego do pliku docelowego.
                    status = copySmallFile(srcFilePath, dstFilePath, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
                // Jeżeli plik źródłowy jest większy lub równy niż poziom dużego pliku
                else
                    // Kopiujemy go jako duży plik.
                    status = copyBigFile(srcFilePath, dstFilePath, srcFile.st_size, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
                // Zapisujemy do logu informację o kopiowaniu.
                syslog(LOG_INFO, "kopiujemy plik %s do katalogu %s; %i\n", srcFilePath, dstDirPath, status);
                // Jeżeli wystąpił błąd
                if (status != 0)
                    // Ustawiamy kod błędu.
                    ret = 4;
                // Przesuwamy wskaźnik na następny plik źródłowy.
                curS = curS->next;
            }
            // Jeżeli plik źródłowy jest w porządku równy docelowemu
            else
            {
                // Dopisujemy nazwę pliku docelowego do ścieżki jego katalogu.
                stringAppend(dstFilePath, dstDirPathLength, dstFileName);
                // Odczytujemy metadane pliku docelowego. Jeżeli wystąpił błąd, to plik docelowy jest niedostępny i nie będziemy mogli porównać czasów modyfikacji.
                if (stat(dstFilePath, &dstFile) == -1)
                {
                    // Zapisujemy do logu informację o nieudanym odczycie metadanych.
                    syslog(LOG_INFO, "odczytujemy metadane pliku docelowego %s; %i\n", dstFilePath, errno);
                    // Ustawiamy kod błędu.
                    ret = 5;
                }
                // Jeżeli poprawnie odczytaliśmy metadane i plik docelowy ma inny czas modyfikacji niż plik źródłowy (wcześniejszy - jest nieaktualny lub późniejszy - został edytowany)
                else if (srcFile.st_mtim.tv_sec != dstFile.st_mtim.tv_sec || srcFile.st_mtim.tv_nsec != dstFile.st_mtim.tv_nsec)
                {
                    // Przepisujemy plik źródłowy do istniejącego pliku docelowego.
                    // Jeżeli plik źródłowy jest mniejszy niż poziom dużego pliku
                    if (srcFile.st_size < threshold)
                        // Kopiujemy go jako mały plik.
                        status = copySmallFile(srcFilePath, dstFilePath, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
                    // Jeżeli plik źródłowy jest większy lub równy niż poziom dużego pliku
                    else
                        // Kopiujemy go jako duży plik.
                        status = copyBigFile(srcFilePath, dstFilePath, srcFile.st_size, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
                    // Zapisujemy do logu informację o kopiowaniu.
                    syslog(LOG_INFO, "przepisujemy %s do %s; %i\n", srcFilePath, dstFilePath, status);
                    // Jeżeli wystąpił błąd
                    if (status != 0)
                        // Ustawiamy kod błędu.
                        ret = 6;
                }
                // Przy kopiowaniu przepisujemy uprawnienia, ale jeżeli nie kopiowaliśmy pliku, to sprawdzamy, czy oba mają równe uprawnienia. Jeżeli pliki mają różne uprawnienia
                else if (srcFile.st_mode != dstFile.st_mode)
                {
                    // Przepisujemy uprawnienia pliku źródłowego do docelowego. Jeżeli wystąpił błąd
                    if (chmod(dstFilePath, srcFile.st_mode) == -1)
                    {
                        // Ustawiamy status różny od 0, bo errno ma wartość różną od 0.
                        status = errno;
                        // Ustawiamy kod błędu.
                        ret = 7;
                    }
                    // Jeżeli nie wystąpił błąd
                    else
                        // Ustawiamy status równy 0.
                        status = 0;
                    // Zapisujemy do logu informację o przepisaniu uprawnień.
                    syslog(LOG_INFO, "przepisujemy uprawnienia pliku %s do %s; %i\n", srcFilePath, dstFilePath, status);
                }
                // Przesuwamy wskaźnik na następny plik źródłowy.
                curS = curS->next;
                // Przesuwamy wskaźnik na następny plik docelowy.
                curD = curD->next;
            }
        }
    }
    // Usuwamy pozostałe pliki z katalogu docelowego, jeżeli istnieją, począwszy od aktualnie wskazywanego przez curD, bo nie istnieją one w katalogu źródłowym.
    while (curD != NULL)
    {
        char *dstFileName = curD->entry->d_name;
        // Dopisujemy nazwę pliku docelowego do ścieżki jego katalogu.
        stringAppend(dstFilePath, dstDirPathLength, dstFileName);
        // Usuwamy plik docelowy.
        status = removeFile(dstFilePath);
        // Zapisujemy do logu informację o usunięciu.
        syslog(LOG_INFO, "usuwamy plik %s; %i\n", dstFilePath, status);
        // Jeżeli wystąpił błąd
        if (status != 0)
            // Ustawiamy kod błędu.
            ret = 8;
        // Przesuwamy wskaźnik na następny plik docelowy.
        curD = curD->next;
    }
    // Kopiujemy pozostałe pliki z katalogu źródłowego, jeżeli istnieją, począwszy od aktualnie wskazywanego przez curS, bo nie istnieją one w katalogu docelowym.
    while (curS != NULL)
    {
        char *srcFileName = curS->entry->d_name;
        // Dopisujemy nazwę pliku źródłowego do ścieżki jego katalogu.
        stringAppend(srcFilePath, srcDirPathLength, srcFileName);
        // Odczytujemy metadane pliku źródłowego. Jeżeli wystąpił błąd
        if (stat(srcFilePath, &srcFile) == -1)
        {
            // Zapisujemy do logu informację o nieudanym kopiowaniu.
            syslog(LOG_INFO, "kopiujemy plik %s do katalogu %s; %i\n", srcFilePath, dstDirPath, errno);
            // Ustawiamy kod błędu.
            ret = 9;
        }
        else
        {
            // Dopisujemy nazwę pliku źródłowego do ścieżki katalogu docelowego.
            stringAppend(dstFilePath, dstDirPathLength, srcFileName);
            // Jeżeli plik źródłowy jest mniejszy niż poziom dużego pliku
            if (srcFile.st_size < threshold)
                // Kopiujemy go jako mały plik.
                status = copySmallFile(srcFilePath, dstFilePath, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
            // Jeżeli plik źródłowy jest większy lub równy niż poziom dużego pliku
            else
                // Kopiujemy go jako duży plik.
                status = copyBigFile(srcFilePath, dstFilePath, srcFile.st_size, srcFile.st_mode, &srcFile.st_atim, &srcFile.st_mtim);
            // Zapisujemy do logu informację o kopiowaniu.
            syslog(LOG_INFO, "kopiujemy plik %s do katalogu %s; %i\n", srcFilePath, dstDirPath, status);
            // Jeżeli wystąpił błąd
            if (status != 0)
                // Ustawiamy kod błędu.
                ret = 10;
        }
        // Przesuwamy wskaźnik na następny plik źródłowy.
        curS = curS->next;
    }
    // Zwalniamy pamięć ścieżek plików źródłowych.
    free(srcFilePath);
    // Zwalniamy pamięć ścieżek plików docelowych.
    free(dstFilePath);
    // Zamykamy połączenie z logiem.
    closelog();
    // Zwracamy status.
    return ret;
}
int updateDestinationDirectories(const char *srcDirPath, const size_t srcDirPathLength, list *subdirsSrc, const char *dstDirPath, const size_t dstDirPathLength, list *subdirsDst, char *isReady)
{
    char *srcSubdirPath = NULL, *dstSubdirPath = NULL;
    // Rezerwujemy pamięć na ścieżki podkatalogów z katalogu źródłowego (podkatalogi źródłowe). Jeżeli wystąpił błąd
    if ((srcSubdirPath = malloc(sizeof(char) * PATH_MAX)) == NULL)
        // Zwracamy kod błędu.
        return -1;
    // Rezerwujemy pamięć na ścieżki podkatalogów z katalogu docelowego (podkatalogi docelowe). Jeżeli wystąpił błąd
    if ((dstSubdirPath = malloc(sizeof(char) * PATH_MAX)) == NULL)
    {
        // Zwalniamy już zarezerwowaną pamięć.
        free(srcSubdirPath);
        // Zwracamy kod błędu.
        return -2;
    }
    // Przepisujemy ścieżkę katalogu źródłowego jako początek ścieżek jego podkatalogów.
    strcpy(srcSubdirPath, srcDirPath);
    // Przepisujemy ścieżkę katalogu docelowego jako początek ścieżek jego podkatalogów.
    strcpy(dstSubdirPath, dstDirPath);
    // Zapisujemy wskaźniki na pierwszy podkatalog źródłowy i docelowy.
    element *curS = subdirsSrc->first, *curD = subdirsDst->first;
    struct stat srcSubdir, dstSubdir;
    unsigned int i = 0;
    // Wstępnie zapisujemy status oznaczający brak błędu.
    int status = 0, ret = 0;
    // Otwieramy połączenie z logiem "/var/log/syslog".
    openlog("DirSyncD", LOG_ODELAY | LOG_PID, LOG_DAEMON);
    while (curS != NULL && curD != NULL)
    {
        char *srcSubdirName = curS->entry->d_name, *dstSubdirName = curD->entry->d_name;
        // Porównujemy w porządku leksykograficznym nazwy podkatalogów źródłowego i docelowego.
        int comparison = strcmp(srcSubdirName, dstSubdirName);
        // Jeżeli podkatalog źródłowy jest w porządku późniejszy niż docelowy
        if (comparison > 0)
        {
            // Dopisujemy nazwę podkatalogu docelowego do ścieżki jego katalogu. Funkcja removeDirectoryRecursively wymaga, aby ścieżka do katalogu była zakończona '/'.
            size_t length = appendSubdirectoryName(dstSubdirPath, dstDirPathLength, dstSubdirName);
            // Rekurencyjnie usuwamy podkatalog docelowy.
            status = removeDirectoryRecursively(dstSubdirPath, length);
            // Zapisujemy do logu informację o usunięciu.
            syslog(LOG_INFO, "usuwamy katalog %s; %i\n", dstSubdirPath, status);
            // Jeżeli wystąpił błąd
            if (status != 0)
                // Ustawiamy dodatni kod błędu, aby zaznaczyć niepełną synchronizację, ale nie przerywamy pętli.
                ret = 1;
            // Przesuwamy wskaźnik na następny podkatalog docelowy.
            curD = curD->next;
        }
        else
        {
            // Dopisujemy nazwę podkatalogu źródłowego do ścieżki jego katalogu.
            stringAppend(srcSubdirPath, srcDirPathLength, srcSubdirName);
            // Odczytujemy metadane podkatalogu źródłowego. Jeżeli wystąpił błąd, to podkatalog źródłowy jest niedostępny i nie będziemy mogli go również utworzyć, gdy comparison < 0, bo nawet gdybyśmy go utworzyli, to go nie zsynchronizujemy.
            if (stat(srcSubdirPath, &srcSubdir) == -1)
            {
                // Jeżeli podkatalog źródłowy jest w porządku wcześniejszy niż docelowy
                if (comparison < 0)
                {
                    // Zapisujemy do logu informację o nieudanym kopiowaniu. Status zapisany przez stat do errno jest liczbą dodatnią.
                    syslog(LOG_INFO, "tworzymy katalog %s; %i\n", dstSubdirPath, errno);
                    // Zaznaczamy, że podkatalog nie jest gotowy do synchronizacji, bo nie istnieje.
                    isReady[i++] = 0;
                    // Ustawiamy kod błędu.
                    ret = 2;
                }
                else
                {
                    // Zapisujemy do logu informację o nieudanym odczycie metadanych.
                    syslog(LOG_INFO, "odczytujemy metadane katalogu źródłowego %s; %i\n", srcSubdirPath, errno);
                    // Jeżeli nie udało się sprawdzić, czy podkatalogi źródłowy i docelowy mają takie same uprawnienia, to zakładamy, że mają. Zaznaczamy, że podkatalog jest gotowy do synchronizacji.
                    isReady[i++] = 1;
                    // Ustawiamy kod błędu.
                    ret = 3;
                    // Przesuwamy wskaźnik na następny podkatalog docelowy.
                    curD = curD->next;
                }
                // Przesuwamy wskaźnik na następny podkatalog źródłowy.
                curS = curS->next;
                // Przechodzimy do następnej iteracji pętli.
                continue;
            }
            // Jeżeli poprawnie odczytaliśmy metadane
            // Jeżeli podkatalog źródłowy jest w porządku wcześniejszy niż docelowy
            if (comparison < 0)
            {
                // Dopisujemy nazwę podkatalogu źródłowego do ścieżki katalogu docelowego.
                stringAppend(dstSubdirPath, dstDirPathLength, srcSubdirName);
                // Tworzymy w katalogu docelowym katalog o nazwie takiej jak podkatalogu źródłowego. Przepisujemy uprawnienia, ale nie przepisujemy czasu modyfikacji, bo nie zwracamy na niego uwagi przy synchronizacji - wszystkie podkatalogi i tak będą rekurencyjnie przejrzane w celu wykrycia zmian plików.
                status = createEmptyDirectory(dstSubdirPath, srcSubdir.st_mode);
                // Zapisujemy do logu informację o utworzeniu.
                syslog(LOG_INFO, "tworzymy katalog %s; %i\n", dstSubdirPath, status);
                // Jeżeli wystąpił błąd
                if (status != 0)
                {
                    // Zaznaczamy, że podkatalog nie jest gotowy do synchronizacji, bo nie istnieje.
                    isReady[i++] = 0;
                    // Ustawiamy kod błędu.
                    ret = 4;
                }
                else
                    // Zaznaczamy, że podkatalog jest gotowy do synchronizacji.
                    isReady[i++] = 1;
                // Przesuwamy wskaźnik na następny podkatalog źródłowy.
                curS = curS->next;
            }
            // Jeżeli podkatalog źródłowy jest w porządku równy docelowemu
            else
            {
                // Zaznaczamy, że podkatalog jest gotowy do synchronizacji, nawet jeżeli nie uda się porównać uprawnień.
                isReady[i++] = 1;
                // Dopisujemy nazwę podkatalogu docelowego do ścieżki jego katalogu.
                stringAppend(dstSubdirPath, dstDirPathLength, dstSubdirName);
                // Odczytujemy metadane podkatalogu docelowego. Jeżeli wystąpił błąd, to podkatalog docelowy jest niedostępny i nie będziemy mogli porównać uprawnień.
                if (stat(dstSubdirPath, &dstSubdir) == -1)
                {
                    // Zapisujemy do logu informację o nieudanym odczycie metadanych.
                    syslog(LOG_INFO, "odczytujemy metadane katalogu docelowego %s; %i\n", dstSubdirPath, errno);
                    // Ustawiamy kod błędu.
                    ret = 5;
                }
                // Ignorujemy czas modyfikacji podkatalogu (zmienia się on podczas tworzenia i usuwania plików z podkatalogu).
                // Jeżeli poprawnie odczytaliśmy metadane i podkatalogi mają różne uprawnienia
                else if (srcSubdir.st_mode != dstSubdir.st_mode)
                {
                    // Przepisujemy uprawnienia podkatalogu źródłowego do docelowego. Jeżeli wystąpił błąd
                    if (chmod(dstSubdirPath, srcSubdir.st_mode) == -1)
                    {
                        // Ustawiamy status różny od 0, bo errno ma wartość różną od 0.
                        status = errno;
                        // Ustawiamy kod błędu.
                        ret = 6;
                    }
                    // Jeżeli nie wystąpił błąd
                    else
                        // Ustawiamy status równy 0.
                        status = 0;
                    // Zapisujemy do logu informację o przepisaniu uprawnień.
                    syslog(LOG_INFO, "przepisujemy uprawnienia katalogu %s do %s; %i\n", srcSubdirPath, dstSubdirPath, status);
                }
                // Przesuwamy wskaźnik na następny podkatalog źródłowy.
                curS = curS->next;
                // Przesuwamy wskaźnik na następny podkatalog docelowy.
                curD = curD->next;
            }
        }
    }
    // Usuwamy pozostałe podkatalogi z katalogu docelowego, jeżeli istnieją, począwszy od aktualnie wskazywanego przez curD, bo nie istnieją one w katalogu źródłowym.
    while (curD != NULL)
    {
        char *dstSubdirName = curD->entry->d_name;
        // Dopisujemy nazwę podkatalogu docelowego do ścieżki jego katalogu.
        size_t length = appendSubdirectoryName(dstSubdirPath, dstDirPathLength, dstSubdirName);
        // Rekurencyjnie usuwamy podkatalog docelowy.
        status = removeDirectoryRecursively(dstSubdirPath, length);
        // Zapisujemy do logu informację o usunięciu.
        syslog(LOG_INFO, "usuwamy katalog %s; %i\n", dstSubdirPath, status);
        // Jeżeli wystąpił błąd
        if (status != 0)
            // Ustawiamy kod błędu.
            ret = 7;
        // Przesuwamy wskaźnik na następny podkatalog docelowy.
        curD = curD->next;
    }
    // Kopiujemy pozostałe podkatalogi z katalogu źródłowego, jeżeli istnieją, począwszy od aktualnie wskazywanego przez curS, bo nie istnieją one w katalogu docelowym.
    while (curS != NULL)
    {
        char *srcSubdirName = curS->entry->d_name;
        // Dopisujemy nazwę podkatalogu źródłowego do ścieżki jego katalogu.
        stringAppend(srcSubdirPath, srcDirPathLength, srcSubdirName);
        // Odczytujemy metadane podkatalogu źródłowego. Jeżeli wystąpił błąd
        if (stat(srcSubdirPath, &srcSubdir) == -1)
        {
            // Zapisujemy do logu informację o nieudanym utworzeniu.
            syslog(LOG_INFO, "tworzymy katalog %s; %i\n", dstSubdirPath, errno);
            // Zaznaczamy, że podkatalog nie jest gotowy do synchronizacji, bo nie istnieje.
            isReady[i++] = 0;
            // Ustawiamy kod błędu.
            ret = 8;
            // Przesuwamy wskaźnik na następny podkatalog źródłowy.
            curS = curS->next;
            // Przechodzimy do następnej iteracji pętli.
            continue;
        }
        // Dopisujemy nazwę podkatalogu źródłowego do ścieżki katalogu docelowego.
        stringAppend(dstSubdirPath, dstDirPathLength, srcSubdirName);
        // Tworzymy w katalogu docelowym katalog o nazwie takiej jak podkatalogu źródłowego.
        status = createEmptyDirectory(dstSubdirPath, srcSubdir.st_mode);
        // Zapisujemy do logu informację o utworzeniu.
        syslog(LOG_INFO, "tworzymy katalog %s; %i\n", dstSubdirPath, status);
        // Jeżeli wystąpił błąd
        if (status != 0)
        {
            // Zaznaczamy, że podkatalog nie jest gotowy do synchronizacji, bo nie istnieje.
            isReady[i++] = 0;
            // Ustawiamy kod błędu.
            ret = 9;
        }
        else
            // Zaznaczamy, że podkatalog jest gotowy do synchronizacji.
            isReady[i++] = 1;
        // Przesuwamy wskaźnik na następny podkatalog źródłowy.
        curS = curS->next;
    }
    // Zwalniamy pamięć ścieżek podkatalogów źródłowych.
    free(srcSubdirPath);
    // Zwalniamy pamięć ścieżek podkatalogów docelowych.
    free(dstSubdirPath);
    // Zamykamy połączenie z logiem.
    closelog();
    // Zwracamy status.
    return ret;
}

int synchronizeNonRecursively(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength)
{
    // Wstępnie ustawiamy status oznaczający brak błędu.
    int ret = 0;
    DIR *dirS = NULL, *dirD = NULL;
    // Otwieramy katalog źródłowy. Jeżeli wystąpił błąd
    if ((dirS = opendir(sourcePath)) == NULL)
        // Ustawiamy status oznaczający błąd. Po tym program natychmiast przechodzi na koniec funkcji.
        ret = -1;
    // Otwieramy katalog docelowy. Jeżeli wystąpił błąd
    else if ((dirD = opendir(destinationPath)) == NULL)
        // Ustawiamy status oznaczający błąd.
        ret = -2;
    else
    {
        // Tworzymy listy na pliki z katalogu źródłowego i docelowego.
        list filesS, filesD;
        // Inicjujemy listę plików z katalogu źródłowego.
        initialize(&filesS);
        // Inicjujemy listę plików z katalogu docelowego.
        initialize(&filesD);
        // Wypełniamy listę plików z katalogu źródłowego. Jeżeli wystąpił błąd
        if (listFiles(dirS, &filesS) < 0)
            // Ustawiamy status oznaczający błąd.
            ret = -3;
        // Wypełniamy listę plików z katalogu docelowego. Jeżeli wystąpił błąd
        else if (listFiles(dirD, &filesD) < 0)
            // Ustawiamy status oznaczający błąd.
            ret = -4;
        else
        {
            // Sortujemy listę plików z katalogu źródłowego.
            listMergeSort(&filesS);
            // Sortujemy listę plików z katalogu docelowego.
            listMergeSort(&filesD);
            // Sprawdzamy zgodność i ewentualnie aktualizujemy pliki w katalogu docelowym. Jeżeli wystąpił błąd
            if (updateDestinationFiles(sourcePath, sourcePathLength, &filesS, destinationPath, destinationPathLength, &filesD) != 0)
                // Ustawiamy status oznaczający błąd.
                ret = -5;
        }
        // Czyścimy listę plików z katalogu źródłowego.
        clear(&filesS);
        // Czyścimy listę plików z katalogu docelowego.
        clear(&filesD);
    }
    // Jeżeli w którymś momencie wystąpił błąd, to przechodzimy do tego miejsca.
    // Zamknąć katalog możemy dopiero, gdy skończymy używać obiektów typu dirent, które odczytaliśmy z niego za pomocą readdir, bo są one usuwane z pamięci w momencie wywołania closedir. Jeżeli katalog źródłowy został otwarty
    if (dirS != NULL)
        // Zamykamy katalog źródłowy. Jeżeli wystąpił błąd, to go ignorujemy.
        closedir(dirS);
    // Jeżeli katalog docelowy został otwarty
    if (dirD != NULL)
        // Zamykamy katalog docelowy. Jeżeli wystąpił błąd, to go ignorujemy.
        closedir(dirD);
    // Zwracamy status.
    return ret;
}
int synchronizeRecursively(const char *sourcePath, const size_t sourcePathLength, const char *destinationPath, const size_t destinationPathLength)
{
    // Wstępnie ustawiamy status oznaczający brak błędu.
    int ret = 0;
    DIR *dirS = NULL, *dirD = NULL;
    // Otwieramy katalog źródłowy. Jeżeli wystąpił błąd
    if ((dirS = opendir(sourcePath)) == NULL)
        // Ustawiamy status oznaczający błąd. Po tym program natychmiast przechodzi na koniec funkcji.
        ret = -1;
    // Otwieramy katalog docelowy. Jeżeli wystąpił błąd
    else if ((dirD = opendir(destinationPath)) == NULL)
        // Ustawiamy status oznaczający błąd.
        ret = -2;
    else
    {
        // Tworzymy listy na pliki i podkatalogi z katalogu źródłowego.
        list filesS, subdirsS;
        // Inicjujemy listę plików z katalogu źródłowego.
        initialize(&filesS);
        // Inicjujemy listę podkatalogów z katalogu źródłowego.
        initialize(&subdirsS);
        // Tworzymy listy na pliki i podkatalogi z katalogu docelowego.
        list filesD, subdirsD;
        // Inicjujemy listę plików z katalogu docelowego.
        initialize(&filesD);
        // Inicjujemy listę podkatalogów z katalogu docelowego.
        initialize(&subdirsD);
        // Wypełniamy listy plików i podkatalogów z katalogu źródłowego. Jeżeli wystąpił błąd
        if (listFilesAndDirectories(dirS, &filesS, &subdirsS) < 0)
            // Ustawiamy status oznaczający błąd.
            ret = -3;
        // Wypełniamy listy plików i podkatalogów z katalogu docelowego. Jeżeli wystąpił błąd
        else if (listFilesAndDirectories(dirD, &filesD, &subdirsD) < 0)
            // Ustawiamy status oznaczający błąd.
            ret = -4;
        else
        {
            // Sortujemy listę plików z katalogu źródłowego.
            listMergeSort(&filesS);
            // Sortujemy listę plików z katalogu docelowego.
            listMergeSort(&filesD);
            // Sprawdzamy zgodność i ewentualnie aktualizujemy pliki w katalogu docelowym. Jeżeli wystąpił błąd
            if (updateDestinationFiles(sourcePath, sourcePathLength, &filesS, destinationPath, destinationPathLength, &filesD) != 0)
                // Ustawiamy status oznaczający błąd.
                ret = -5;
            // Czyścimy listę plików z katalogu źródłowego.
            clear(&filesS);
            // Czyścimy listę plików z katalogu docelowego.
            clear(&filesD);

            // Sortujemy listę podkatalogów z katalogu źródłowego.
            listMergeSort(&subdirsS);
            // Sortujemy listę podkatalogów z katalogu docelowego.
            listMergeSort(&subdirsD);
            // W komórce i tablicy isReady wpiszemy 1, jeżeli i-ty podkatalog z katalogu źródłowego istnieje lub zostanie prawidłowo utworzony w katalogu docelowym podczas funkcji updateDestinationDirectories, czyli będzie gotowy do rekurencyjnej synchronizacji.
            char *isReady = NULL;
            // Rezerwujemy pamięć na tablicę o rozmiarze równym liczbie podkatalogów w katalogu źródłowym. Jeżeli wystąpił błąd
            if ((isReady = malloc(sizeof(char) * subdirsS.count)) == NULL)
                // Ustawiamy status oznaczający błąd.
                ret = -6;
            else
            {
                // Sprawdzamy zgodność i ewentualnie aktualizujemy podkatalogi w katalogu docelowym. Wypełniamy tablicę isReady. Jeżeli wystąpił błąd
                if (updateDestinationDirectories(sourcePath, sourcePathLength, &subdirsS, destinationPath, destinationPathLength, &subdirsD, isReady) != 0)
                    // Ustawiamy status oznaczający błąd.
                    ret = -7;
                // Jeszcze nie czyścimy listy podkatalogów z katalogu źródłowego, bo rekurencyjnie będziemy wywoływać funkcję synchronizeRecursively na tych podkatalogach.
                // Czyścimy listę podkatalogów z katalogu docelowego.
                clear(&subdirsD);

                char *nextSourcePath = NULL, *nextDestinationPath = NULL;
                // Rezerwujemy pamięć na ścieżki podkatalogów z katalogu źródłowego. Jeżeli wystąpił błąd
                if ((nextSourcePath = malloc(sizeof(char) * PATH_MAX)) == NULL)
                    // Ustawiamy status oznaczający błąd.
                    ret = -8;
                // Rezerwujemy pamięć na ścieżki podkatalogów z katalogu docelowego. Jeżeli wystąpił błąd
                else if ((nextDestinationPath = malloc(sizeof(char) * PATH_MAX)) == NULL)
                    // Ustawiamy status oznaczający błąd.
                    ret = -9;
                else
                {
                    // Przepisujemy ścieżkę katalogu źródłowego jako początek ścieżek jego podkatalogów.
                    strcpy(nextSourcePath, sourcePath);
                    // Przepisujemy ścieżkę katalogu docelowego jako początek ścieżek jego podkatalogów.
                    strcpy(nextDestinationPath, destinationPath);
                    // Zapisujemy wskaźnik na pierwszy podkatalog z katalogu źródłowego.
                    element *curS = subdirsS.first;
                    unsigned int i = 0;
                    while (curS != NULL)
                    {
                        // Jeżeli podkatalog jest gotowy do synchronizacji
                        if (isReady[i++] == 1)
                        {
                            // Tworzymy ścieżkę podkatalogu z katalogu źródłowego i zapisujemy jej długość.
                            size_t nextSourcePathLength = appendSubdirectoryName(nextSourcePath, sourcePathLength, curS->entry->d_name);
                            // Tworzymy ścieżkę podkatalogu z katalogu docelowego i zapisujemy jej długość.
                            size_t nextDestinationPathLength = appendSubdirectoryName(nextDestinationPath, destinationPathLength, curS->entry->d_name);
                            // Rekurencyjnie synchronizujemy podkatalogi. Jeżeli wystąpił błąd
                            if (synchronizeRecursively(nextSourcePath, nextSourcePathLength, nextDestinationPath, nextDestinationPathLength) < 0)
                                // Ustawiamy status oznaczający błąd.
                                ret = -10;
                        }
                        // Jeżeli podkatalog nie jest gotowy do synchronizacji, to go pomijamy.
                        // Przesuwamy wskaźnik na następny podkatalog.
                        curS = curS->next;
                    }
                }
                // Zwalniamy tablicę isReady.
                free(isReady);
                // Jeżeli pamięć na ścieżkę podkatalogu z katalogu źródłowego została zarezerwowana
                if (nextSourcePath != NULL)
                    // Zwalniamy pamięć.
                    free(nextSourcePath);
                // Jeżeli pamięć na ścieżkę podkatalogu z katalogu docelowego została zarezerwowana
                if (nextDestinationPath != NULL)
                    // Zwalniamy pamięć.
                    free(nextDestinationPath);
            }
        }
        // Czyścimy listę podkatalogów z katalogu źródłowego.
        clear(&subdirsS);
        // Jeżeli nie udało się zarezerwować pamięci na isReady, to lista dirsD podkatalogów z katalogu docelowego nie została jeszcze wyczyszczona. Jeżeli zawiera jakieś elementy
        if (subdirsD.count != 0)
            // Czyścimy ją.
            clear(&subdirsD);
    }
    // Jeżeli katalog źródłowy został otwarty
    if (dirS != NULL)
        // Zamykamy katalog źródłowy. Jeżeli wystąpił błąd, to go ignorujemy.
        closedir(dirS);
    // Jeżeli katalog docelowy został otwarty
    if (dirD != NULL)
        // Zamykamy katalog docelowy. Jeżeli wystąpił błąd, to go ignorujemy.
        closedir(dirD);
    // Zwracamy status.
    return ret;
}