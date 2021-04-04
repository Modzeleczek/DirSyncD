#include "Anna.h"
#include "Lukasz.h"
#include "Mariusz.h"

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

static unsigned long long THRESHOLD; // static - zmienna globalna widoczna tylko w tym pliku; extern - zmienna globalna widoczna we wszystkich plikach
#define BUFFERSIZE 4096

struct element
{
    element *next;
    struct dirent *entry; // element katalogu
};
int cmp(element *a, element *b)
{
    return strcmp(a->entry->d_name, b->entry->d_name); // funkcja porównująca elementy; porządek leksykograficzny
}

struct list
{
    element *first, *last;
    unsigned int count;
};
// w metodach struktury list zakładamy, że podano prawidłowy wskaźnik do listy
void initialize(list *l)
{
    l->first = l->last = NULL;
    l->count = 0;
}
void pushBack(list *l, struct dirent *newEntry)
{
    element *new = malloc(sizeof(element));
    new->entry = newEntry;
    new->next = NULL;
    if(l->first == NULL) // jeżeli lista jest pusta, to first i last są NULLami
    {
        l->first = new;
        l->last = new;
    }
    else // jeżeli lista nie jest pusta, to ani first ani last nie są NULLami
    {
        l->last->next = new; // ustawiamy aktualnemu lastowi nowy jako następny
        l->last = new; // przestawiamy aktualny last na nowy
    }
    ++l->count;
}
void clear(list *l)
{
    element *cur = l->first, *next;
    while(cur != NULL)
    {
        next = cur->next;
        free(cur);
        cur = next;
    }
    initialize(l);
}
void listMergeSort(list *l) {
    element *p, *q, *e, *tail, *list = l->first;
    int insize, nmerges, psize, qsize, i;
    if (list == NULL) // Silly special case: if `list' was passed in as NULL, return immediately.
        return;
    insize = 1;
    while (1) {
        p = list;
        list = NULL;
        tail = NULL;
        nmerges = 0; // count number of merges we do in this pass
        while (p) {
            nmerges++; // there exists a merge to be done
            // step `insize' places along from p
            q = p;
            psize = 0;
            for (i = 0; i < insize; i++) {
                psize++;
                    q = q->next;
                if (!q) break;
            }
            // if q hasn't fallen off end, we have two lists to merge
            qsize = insize;
            // now we have two lists; merge them
            while (psize > 0 || (qsize > 0 && q)) {
                // decide whether next element of merge comes from p or q
                if (psize == 0) {
                    // p is empty; e must come from q.
                    e = q; q = q->next; qsize--;
                } else if (qsize == 0 || !q) {
                    // q is empty; e must come from p.
                    e = p; p = p->next; psize--;
                } else if (cmp(p,q) <= 0) {
                    // First element of p is lower (or same); e must come from p.
                    e = p; p = p->next; psize--;
                } else {
                    // First element of q is lower; e must come from q.
                    e = q; q = q->next; qsize--;
                }
                // add the next element to the merged list
                if (tail) {
                    tail->next = e;
                } else {
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

int copySmallFile(const char *srcFilePath, const char *dstFilePath, const mode_t dstMode, const struct timespec *dstAccessTime, const struct timespec *dstModificationTime)
{
    int ret = 0, in = -1, out = -1;
    if((in = open(srcFilePath, O_RDONLY)) == -1) // deskryptor pliku wejściowego; O_RDONLY - tylko odczyt
        ret = -1;
    else // jeżeli in != -1, czyli plik źródłowy został poprawnie otwarty, to open(dstFilePath, ...) się wykona;
    if((out = open(dstFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0000)) == -1) // deskryptor pliku wyjściowego; O_WRONLY - tylko zapis; O_CREAT - utworzenie pliku, jeżeli jeszcze nie istnieje; O_TRUNC - jeżeli plik istnieje, to usunięcie jego zawartości; jeżeli plik jeszcze nie istnieje, to na początku nie dostanie żadnych uprawnień
        ret = -2;
    else // jeżeli in != -1 i out != -1, czyli plik źródłowy i docelowy zostały prawidłowo otwarte
    if(fchmod(out, dstMode) == -1) // ustawiamy plikowi docelowemu uprawnienia dstMode
        ret = -3;
    else
    {
        if(posix_fadvise(in, 0, 0, POSIX_FADV_SEQUENTIAL) == -1) // str. 124; wysyłamy jądru wskazówkę (poradę), że plik wejściowy będzie odczytywany sekwencyjnie i dlatego należy go wczytywać z wyprzedzeniem
            ret = 1; // błąd niekrytyczny
        char *buffer = NULL;
        if((buffer = malloc(sizeof(char) * BUFFERSIZE)) == NULL) // optymalny rozmiar bufora operacji wejścia-wyjścia dla danego pliku można sprawdzić w metadanych pobranych statem
            ret = -4;
        else
        {
            while(1) 
            {
                // brak obsługi błędów
                // int readBytes = read(in, buffer, BUFFERSIZE);
                // write(out, buffer, readBytes);
                // if(readBytes < BUFFERSIZE)
                //     break;
                char *position = buffer; // str. 45; pozycja w buforze wyjściowym
                size_t remainingBytes = BUFFERSIZE;
                ssize_t bytesRead;
                while(remainingBytes != 0 && (bytesRead = read(in, position, remainingBytes)) != 0)
                {
                    if(bytesRead == -1)
                    {
                        if(errno == EINTR)
                            continue;
                        ret = -5;
                        remainingBytes = BUFFERSIZE; // BUFFERSIZE - BUFFERSIZE == 0, więc druga pętla się nie wykona
                        bytesRead = 0; // ustawiamy na 0, aby if(bytesRead == 0) zakończył zewnętrzną pętlę
                        break;
                    }
                    remainingBytes -= bytesRead;
                    position += bytesRead;
                }
                position = buffer; // str. 48
                remainingBytes = BUFFERSIZE - remainingBytes; // zapisujemy całkowitą liczbę odczytanych bajtów, która zawsze jest mniejsza lub równa rozmiarowi bufora
                ssize_t bytesWritten;
                while(remainingBytes != 0 && (bytesWritten = write(out, position, remainingBytes)) != 0)
                {
                    if(bytesWritten == -1)
                    {
                        if(errno == EINTR)
                            continue;
                        ret = -6;
                        bytesRead = 0; // ustawiamy na 0, aby if(bytesRead == 0) zakończył zewnętrzną pętlę
                        break;
                    }
                    remainingBytes -= bytesWritten;
                    position += bytesWritten;
                }
                if(bytesRead == 0) // doszliśmy do końca pliku (EOF) lub wystąpił błąd
                    break;
            }
            free(buffer);
            if(ret >= 0) // jeżeli nie wystąpiły błędy podczas kopiowania
            {
                const struct timespec times[2] = { *dstAccessTime, *dstModificationTime };
                if(futimens(out, times) == -1) // ustawiamy plikowi wyjściowemu czasy ostatniego dostępu i modyfikacji; czas modyfikacji musi być ustawiony po skończeniu zapisów do pliku wyjściowego, bo zmieniają one go na aktualny czas systemowy
                    ret = -7;
            }
        }
    }
    if(in != -1 && close(in) == -1)
        ret = -8;
    if(out != -1 && close(out) == -1)
        ret = -9;
    return ret;
}

int copyBigFile(const char *srcFilePath, const char *dstFilePath, const unsigned long long fileSize, const mode_t dstMode, const struct timespec *dstAccessTime, const struct timespec *dstModificationTime)
{
    int ret = 0, in = -1, out = -1;
    if((in = open(srcFilePath, O_RDONLY)) == -1) // deskryptor pliku wejściowego; O_RDONLY - tylko odczyt
        ret = -1;
    else // jeżeli in != -1, czyli plik źródłowy został poprawnie otwarty, to open(dstFilePath, ...) się wykona;
    if((out = open(dstFilePath, O_WRONLY | O_CREAT | O_TRUNC, 0000)) == -1) // deskryptor pliku wyjściowego; O_WRONLY - tylko zapis; O_CREAT - utworzenie pliku, jeżeli jeszcze nie istnieje; O_TRUNC - jeżeli plik istnieje, to usunięcie jego zawartości; jeżeli plik jeszcze nie istnieje, to na początku nie dostanie żadnych uprawnień
        ret = -2;
    else // jeżeli in != -1 i out != -1, czyli plik źródłowy i docelowy zostały prawidłowo otwarte
    if(fchmod(out, dstMode) == -1) // ustawiamy plikowi docelowemu uprawnienia dstMode
        ret = -3;
    else
    {
        char *map;
        if((map = mmap(0, fileSize, PROT_READ, MAP_SHARED, in, 0)) == MAP_FAILED) // odwzorowujemy (mapujemy) w pamięci plik wejściowy w trybie do odczytu; jeżeli nie udało się zmapować pliku
            ret = -4;
        else
        {
            if(madvise(map, fileSize, MADV_SEQUENTIAL) == -1) // str. 121; wysyłamy jądru wskazówkę (poradę), że plik wejściowy będzie odczytywany sekwencyjnie
                ret = 1; // błąd niekrytyczny
            char *buffer = NULL;
            if((buffer = malloc(sizeof(char) * BUFFERSIZE)) == NULL) // optymalny rozmiar bufora operacji wejścia-wyjścia dla danego pliku można sprawdzić w metadanych pobranych statem; jeżeli nie udało się zarezerwować pamięci na bufor
                ret = -5;
            else
            {
                unsigned long long b; // numer bajtu w pliku wejściowym
                char *position; // pozycja w buforze wyjściowym
                size_t remainingBytes;
                ssize_t bytesWritten;
                for(b = 0; b + BUFFERSIZE < fileSize; b += BUFFERSIZE) // nie może być (b < fileSize - BUFFERSIZE), bo b i fileSize są unsigned, więc jeżeli fileSize < BUFFERSIZE i odejmiemy, to mamy przepełnienie
                {
                    memcpy(buffer, map + b, BUFFERSIZE); // kopiujemy BUFFERSIZE (rozmiar bufora) bajtów ze zmapowanej pamięci do bufora zapisu
                    position = buffer; // str. 48
                    remainingBytes = BUFFERSIZE; // zapisujemy całkowitą liczbę odczytanych bajtów, która zawsze jest równa rozmiarowi bufora
                    while(remainingBytes != 0 && (bytesWritten = write(out, position, remainingBytes)) != 0)
                    {
                        if(bytesWritten == -1)
                        {
                            if(errno == EINTR)
                                continue;
                            ret = -6;
                            b = ULLONG_MAX - BUFFERSIZE; // dla pewności można ustawić b = ULLONG_MAX - BUFFERSIZE; w praktyce fileSize zawsze jest mniejsze od ULLONG_MAX (nie ma plików o rozmiarze 2^64 B), więc można ustawić b = fileSize, aby zakończyć fora i spowodować, że przed poniższą pętlą remainingBytes == 0 - wtedy memcpy nie skopiuje bajtów, a pętla się nie wykona
                            break;
                        }
                        remainingBytes -= bytesWritten;
                        position += bytesWritten;
                    }
                }
                if(ret >= 0) // jeżeli nie wystąpił błąd podczas dotychczasowego kopiowania
                {
                    remainingBytes = fileSize - b; // liczba bajtów z końca pliku, które nie zmieściły się w buforze
                    memcpy(buffer, map + b, remainingBytes);
                    position = buffer;
                    while(remainingBytes != 0 && (bytesWritten = write(out, position, remainingBytes)) != 0)
                    {
                        if(bytesWritten == -1)
                        {
                            if(errno == EINTR)
                                continue;
                            ret = -7;
                            break;
                        }
                        remainingBytes -= bytesWritten;
                        position += bytesWritten;
                    }
                }
                free(buffer);
                if(ret >= 0) // jeżeli nie wystąpiły błędy podczas kopiowania
                {
                    const struct timespec times[2] = { *dstAccessTime, *dstModificationTime };
                    if(futimens(out, times) == -1) // ustawiamy plikowi wyjściowemu czasy ostatniego dostępu i modyfikacji
                        ret = -8;
                }
            }
            if(munmap(map, fileSize) == -1)
                ret = -9;
        }
    }
    if(in != -1 && close(in) == -1)
        ret = -10;
    if(out != -1 && close(out) == -1)
        ret = -11;
    return ret;
}

int removeFile(const char *path)
{
    return unlink(path); // unlink służy tylko do usuwania plików; katalog trzeba usunąć rmdirem, ale najpierw trzeba zapewnić, aby był pusty
}

int createEmptyDirectory(const char *path, mode_t mode)
{
    return mkdir(path, mode);
}

// ścieżka musi być zakończona '/'
int removeDirectoryRecursively(const char *path, const size_t pathLength)
{
    int ret = 0;
    DIR *dir = NULL;
    if((dir = opendir(path)) == NULL)
        ret = -1;
    else
    {
        list dirs, files;
        initialize(&dirs);
        initialize(&files);
        if(listFilesAndDirectories(dir, &files, &dirs) < 0)
            ret = -2;
        else
        {
            char *subPath = malloc(sizeof(char) * PATH_MAX); // rezerwujemy PATH_MAX bajtów na ścieżki podkatalogów i plików
            strcpy(subPath, path); // kopiujemy path do nextPath
            element *cur = dirs.first;
            while(cur != NULL)
            {
                stringAppend(subPath, pathLength, cur->entry->d_name); // dopisujemy nazwę podkatalogu do aktualnej ścieżki katalogu
                size_t subPathLength = pathLength + strlen(cur->entry->d_name);
                stringAppend(subPath, subPathLength++, "/"); // dopisujemy '/' do utworzonej ścieżki
                if(removeDirectoryRecursively(subPath, subPathLength) < 0) // rekurencyjnie wywołujemy usuwanie podkatalogów; jeżeli nie udało się usunąć któregoś podkatalogu
                    ret = -3; // zaznaczamy błąd wyższemu wywołaniu
                cur = cur->next;
            }
            cur = files.first;
            while(cur != NULL) // usuwamy pliki z aktualnego katalogu
            {
                stringAppend(subPath, pathLength, cur->entry->d_name); // dopisujemy nazwę pliku do aktualnej ścieżki katalogu
                if(removeFile(subPath) == -1) // usuwamy plik
                    ret = -4;
                cur = cur->next;
            }
            free(subPath);
        }
        clear(&dirs); // czyścimy listę podkatalogów
        clear(&files); // czyścimy listę plików
    }
    if(dir != NULL && closedir(dir) == -1) // zamykamy aktualny katalog
        ret = 1; // jeżeli nie uda się zamknąć, to zaznaczamy liczbą dodatnią błąd niekrytyczny wyższemu wywołaniu
    // błąd krytyczny w funkcji występuje, jeżeli nie uda się usunąć któregokolwiek elementu z podkatalogów aktualnego katalogu
    if(ret >= 0 && rmdir(path) == -1) // jeżeli nie wystąpił żaden błąd krytyczny, usuwamy aktualny katalog; jeżeli nie uda się usunąć aktualnego katalogu
        ret = -5; // zaznaczamy błąd krytyczny wyższemu wywołaniu
    return ret;
}

// zakładamy, że podano prawidłowy wskaźnik do listy i do katalogu
int listFiles(DIR *dir, list *files)
{
    struct dirent *entry;
    errno = 0;
    while((entry = readdir(dir)) != NULL)
    {
        if(entry->d_type == DT_REG) // jeżeli element jest zwykłym plikiem (regular file)
            pushBack(files, entry);
    }
    if(errno != 0) // jeżeli wystąpił błąd podczas odczytywania elementu
        return -1;
    return 0;
}
int listFilesAndDirectories(DIR *dir, list *files, list *dirs)
{
    struct dirent *entry;
    errno = 0;
    while((entry = readdir(dir)) != NULL)
    {
        if(entry->d_type == DT_REG) // jeżeli element jest zwykłym plikiem (regular file)
            pushBack(files, entry);
        else if(entry->d_type == DT_DIR) // jeżeli element jest katalogiem (directory)
        {
            if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) // jeżeli katalog ma nazwę różną od "." i różną od ".."
                pushBack(dirs, entry); // dodajemy go do listy katalogów
        }
        // elementy o innych typach (symlinki; urządzenia blokowe, tekstowe; sockety; itp.) ignorujemy
    }
    if(errno != 0) // jeżeli wystąpił błąd podczas odczytywania elementu
        return -1;
    return 0;
}

int parseParameters(int argc, char **argv, char **source, char **destination, unsigned int *interval, char *recursive)
{
    // parsujemy dodatkowe opcje i argumenty programu
    if(argc <= 1)
        return -1;
    *interval = 5 * 60; // domyślny czas spania: 5*60 s = 5 min
    *recursive = 0; // domyślnie brak rekurencyjnego synchronizowania katalogów
    THRESHOLD = ULLONG_MAX; // domyślnie próg dużego pliku wynosi 2^64 - 1 bajtów; jeżeli podamy opcję -t 4096, to pliki o rozmiarze >= 4096 B będą czytane mmapem i zapisywane w folderze docelowym writem
    int option;
    // umieszczamy ':' na początku __shortopts, aby program mógł rozróżniać między '?' (nieznanym argumentem) i ':' (brakiem podania wartości dla opcji)
    while((option = getopt(argc, argv, ":Ri:t:")) != -1)
    {
        switch(option)
        {
            case 'R':
                *recursive = (char)1; // rekurencyjna synchronizacja katalogów
                break;
            case 'i':
                if(sscanf(optarg, "%u", interval) < 1) // ciąg znaków optarg jest czasem spania w sekundach; zamieniamy ciąg znaków na unsigned int; jeżeli sscanf nie wypełnił poprawnie zmiennej interval, to wartość przekazana do programu ma niepoprawny format 
                    return -2;
                break;
            case 't':
                if(sscanf(optarg, "%llu", &THRESHOLD) < 1) // ciąg znaków optarg jest progiem dużego pliku; zamieniamy ciąg znaków na unsigned long long; jeżeli sscanf nie wypełnił poprawnie zmiennej threshold, to wartość przekazana do programu ma niepoprawny format
                    return -3;
                break;
            case ':':
                printf("opcja wymaga podania wartosci\n");
                return -4;
                break;
            case '?':
                printf("nieznana opcja: %c\n", optopt);
                return -5;
                break;
            default:
                printf("blad");
                return -6;
                break;
        }
    }
    int remainingArguments = argc - optind; // wyznaczamy liczbę argumentów, które nie są opcjami
    if(remainingArguments != 2) // jeżeli nie mamy dokładnie dwóch argumentów (ściezki źródłowej i docelowej), to kończymy
        return -7;
    // optind - indeks pierwszego argumentu niesparsowanego przez getopt
    *source = argv[optind];
    *destination = argv[optind + 1];
    return 0;
}

int directoryValid(const char *path)
{
    DIR *d = opendir(path); // otwieramy katalog
    if(d == NULL) // jeżeli błąd podczas otwierania (m. in. kiedy katalog nie istnieje)
        return -1;
    if(closedir(d) == -1) // zamykamy katalog; jeżeli błąd podczas zamykania
        return -2;
    return 0; // katalog istnieje i operacje na nim nie powodują błędów
}

void handler(int signo) // funkcja obsługi sygnału SIGUSR1; nie musi nic robić, bo służy tylko do przerwania spania - powrót procesu demona ze stanu oczekiwania do stanu gotowego
{ }

// Love R. - "Linux. Programowanie systemowe." strona 177
// tworzymy proces potomny, kończymy proces rodzicielski (uruchamiacz demona), przekształcamy proces potomny w demona
void startDaemon(char *source, char *destination, unsigned int interval, char recursive)
{
    pid_t pid = fork(); // tworzymy proces potomny
    if(pid == -1) // błąd wywołania fork jeszcze w procesie rodzicielskim; nie powstał proces potomny
    {
        perror("fork");
        exit(-1); // zamykamy proces rodzicielski ze statusem -1 (błąd)
    }
    else if(pid > 0) // w procesie rozdzicielskim zmienna pid ma wartość równą ID (PID) utworzonego procesu potomnego
    {
        printf("PID procesu potomnego: %i\n", pid); // wypisujemy PID procesu potomnego - przyszłego demona
        exit(0); // zamykamy proces rodzicielski ze statusem 0 (brak błędów)
    }
    // poniższy kod wykonuje się w procesie potomnym, bo w nim pid == 0; przekształcamy proces potomny w demona
    int ret = 0;
    char *sourceAbsolutePath = NULL, *destinationAbsolutePath = NULL;
    if((sourceAbsolutePath = realpath(source, NULL)) == NULL) // wyznaczamy ścieżkę bezwzględną katalogu źródłowego
    {
        perror("realpath; source");
        ret = -1;
    }
    else if((destinationAbsolutePath = realpath(destination, NULL)) == NULL) // wyznaczamy ścieżkę bezwzględną katalogu docelowego
    {
        perror("realpath; destination");
        ret = -2;
    }
    else if(setsid() == -1) // tworzymy nową sesję i grupę procesów
        ret = -3; // nie wywołujemy perror, bo w procesie potomnym nie możemy wypisać błędu do terminala uruchamiającego proces rodzicielski
    else if(chdir("/") == -1) // ustawiamy katalog roboczy procesu na /
        ret = -4;
    else
    {
        int i;
        for(i = 0; i < 1023; ++i) // zamykamy stdin, stdout, stderr (deskryptory 0, 1, 2) i dalsze deskryptory - łącznie od 0 do 1023, bo domyślnie w Linuxie proces może mieć otwarte maksymalnie 1024 deskryptory
            if(close(i) == -1) // jeżeli nie uda się zamknąć któregoś deskryptora
            {
                ret = -5;
                break;
            }
    }
    if(ret >= 0)
    {
        // przeadresowujemy deskryptory 0, 1, 2 na /dev/null
        if(open("/dev/null", O_RDWR) == -1) // deskryptor 0 (stdin) wskazuje teraz na /dev/null
            ret = -6;
        else if(dup(0) == -1) // deskryptor 1 (stdout) wskazuje teraz na to samo co deskryptor 0 - na /dev/null
            ret = -7;
        else if(dup(0) == -1) // deskryptor 2 (stderr) wskazuje teraz na to samo co deskryptor 0 - na /dev/null
            ret = -8;
        // jeżeli nie wystąpił błąd, to w tym momencie proces potomny jest już demonem
        else if(signal(SIGUSR1, handler) == SIG_ERR) // błąd podczas rejestrowania funkcji obsługującej sygnał SIGUSR1
            ret = -9;
    }
    if(sourceAbsolutePath != NULL)
        free(sourceAbsolutePath);
    if(destinationAbsolutePath != NULL)
        free(destinationAbsolutePath);
    exit(ret); // zamykamy proces demona
}