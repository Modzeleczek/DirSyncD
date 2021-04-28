# DirSyncD
Demon okresowo synchronizujący 2 katalogi.

Program, który otrzymuje co najmniej dwa argumenty: ścieżkę źródłową oraz ścieżkę docelową. Jeżeli któraś ze ścieżek nie jest katalogiem, program powraca natychmiast z komunikatem błędu. W przeciwnym wypadku staje się demonem. Demon wykonuje następujące czynności: śpi przez pięć minut (czas spania można zmieniać przy pomocy dodatkowego opcjonalnego argumentu), po czym po obudzeniu się porównuje katalog źródłowy z katalogiem docelowym. Pozycje które nie są zwykłymi plikami są ignorowane (np. katalogi i dowiązania symboliczne). Jeżeli demon  
(a) napotka na nowy plik w katalogu źródłowym i tego pliku brak w katalogu docelowym lub  
(b) plik w katalogu źrodłowym ma wcześniejszą lub późniejszą datę ostatniej modyfikacji,  
demon wykonuje kopię pliku z katalogu źródłowego do katalogu docelowego, ustawiając w katalogu docelowym datę modyfikacji, tak aby przy kolejnym obudzeniu nie trzeba było wykonać kopii (chyba ze plik w katalogu źródłowym lub docelowym zostanie zmieniony). Jeżeli zaś demon odnajdzie plik w katalogu docelowym, którego nie ma w katalogu źródłowym, to usuwa ten plik z katalogu docelowego. Możliwe jest również natychmiastowe obudzenie się demona poprzez wysłanie mu sygnału SIGUSR1. Wyczerpująca informacja o każdej akcji typu uśpienie/obudzenie się demona (naturalne lub w wyniku sygnału), wykonanie kopii lub usunięcie pliku jest przesłana do logu systemowego. Informacja ta zawiera aktualną datę.

Dodatkowa opcja -R pozwalająca na rekurencyjną synchronizację katalogów (teraz pozycje będące katalogami nie są ignorowane). W szczególności jeżeli demon znajdzie w katalogu docelowym podkatalog, którego brak w katalogu źródłowym, powinien usunąć go wraz z zawartością.

W zależności od rozmiaru pliku: dla małych plików wykonywane jest kopiowanie przy pomocy read/write, a w przypadku dużych przy pomocy mmap/write - plik źródłowy zostaje zmapowany w całości w pamięci. Próg dzielący pliki małe od dużych może być przekazany jako opcjonalny argument.

Niezbędne argumenty:  
sciezka_zrodlowa - ścieżka do katalogu, z którego kopiujemy  
sciezka_docelowa - ścieżka do katalogu, do którego kopiujemy

Dodatkowe opcje:  
-i <czas_spania> - czas spania  
-R - rekurencyjna synchronizacja katalogów  
-t <prog_duzego_pliku> - minimalny rozmiar pliku, żeby był on potraktowany jako duży

Sposób użycia:  
DirSyncD [-i <czas_spania>] [-R] [-t <prog_duzego_pliku>] sciezka_zrodlowa sciezka_docelowa

Wysłanie sygnału SIGUSR1 do demona:  
- podczas spania - przedwczesne obudzenie  
- podczas synchronizacji - wymuszenie ponownej synchronizacji natychmiast po zakończeniu aktualnej

Wysłanie sygnału SIGTERM do demona:  
- podczas spania - zakończenie demona  
- podczas synchronizacji - zakończenie demona po zakończeniu synchronizacji, o ile podczas niej nie zostanie wysłany również SIGUSR1
