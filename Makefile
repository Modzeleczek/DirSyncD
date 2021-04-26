all: build/DirSyncD

build/DirSyncD: build/DirSyncD.o
	cd build; \
	gcc -o DirSyncD DirSyncD.o

build/DirSyncD.o: DirSyncD.c
	gcc -c DirSyncD.c -o build/DirSyncD.o

clean:
	cd build; \
	rm DirSyncD DirSyncD.o