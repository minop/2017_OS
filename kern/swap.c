#include <kern/swap.h>

//vytvaram pole struktur Mapping, pole zretazenych zoznamov predstavujucich mapovanie na fyzicku stranku. Kazdy prvok pola predstavuje fyzicku stranku. Pole ma teda velkost MAXSWAPPEDPAGES.
struct Mapping *swap_pages = NULL;
