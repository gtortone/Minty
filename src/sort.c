
#include <stdbool.h>
#include <stdint.h>

// Struttura per tenere insieme i dati correlati
typedef struct {
    unsigned int mapfrom;
    unsigned int mapto;
    unsigned int maprom;
    int mapdelta;  // Nota: nel tuo log è DEC, quindi signed
    unsigned int mapsize;
    unsigned int page;
    unsigned int tipo;
} SlotData;

// Funzione di confronto per qsort
static int compareSlots(const void *a, const void *b) {
    SlotData *slotA = (SlotData *)a;
    SlotData *slotB = (SlotData *)b;
    
    // 1. Ordina per maprom (crescente)
    if (slotA->maprom < slotB->maprom) return -1;
    if (slotA->maprom > slotB->maprom) return 1;
    
    // 2. Se maprom è uguale, ordina per tipo (crescente)
    if (slotA->tipo < slotB->tipo) return -1;
    if (slotA->tipo > slotB->tipo) return 1;
    
    // 3. Se anche tipo è uguale, ordina per pagina (crescente)
    if (slotA->page < slotB->page) return -1;
    if (slotA->page > slotB->page) return 1;
    
    return 0; // Tutti i campi sono uguali
}

// Funzione di ordinamento con bubble sort (senza malloc)
void sortSlotsSimple(uint32_t mapfrom[], uint32_t mapto[], uint32_t maprom[], 
                     int32_t mapdelta[], uint32_t mapsize[], uint8_t page[], 
                     uint8_t tipo[], int numSlots) {

    unsigned int Slot0 = 0;
    unsigned int Slot1 = 0;
    
    // Conta gli slot per tipo prima dell'ordinamento
    for (int i = 0; i < numSlots; i++) {
        if (tipo[i] == 0) {
            Slot0++;
        } else if (tipo[i] == 1) {
            Slot1++;
        }
    }
    
    // Ordina gli array
    bool swapped;
    do {
        swapped = false;
        for (int i = 0; i < numSlots - 1; i++) {
            bool needSwap = false;
            
            // Confronta secondo i criteri: tipo, maprom, page
            if (tipo[i] > tipo[i + 1]) {
                needSwap = true;
            } else if (tipo[i] == tipo[i + 1]) {
                if (maprom[i] > maprom[i + 1]) {
                    needSwap = true;
                } else if (maprom[i] == maprom[i + 1]) {
                    if (page[i] > page[i + 1]) {
                        needSwap = true;
                    }
                }
            }
            
            // Scambia se necessario
            if (needSwap) {
                // Scambia tutti i valori
                unsigned int tempUint;
                int tempInt;
                
                // mapfrom
                tempUint = mapfrom[i];
                mapfrom[i] = mapfrom[i + 1];
                mapfrom[i + 1] = tempUint;
                
                // mapto
                tempUint = mapto[i];
                mapto[i] = mapto[i + 1];
                mapto[i + 1] = tempUint;
                
                // maprom
                tempUint = maprom[i];
                maprom[i] = maprom[i + 1];
                maprom[i + 1] = tempUint;
                
                // mapdelta
                tempInt = mapdelta[i];
                mapdelta[i] = mapdelta[i + 1];
                mapdelta[i + 1] = tempInt;
                
                // mapsize
                tempUint = mapsize[i];
                mapsize[i] = mapsize[i + 1];
                mapsize[i + 1] = tempUint;
                
                // page
                tempUint = page[i];
                page[i] = page[i + 1];
                page[i + 1] = tempUint;
                
                // tipo
                tempUint = tipo[i];
                tipo[i] = tipo[i + 1];
                tipo[i + 1] = tempUint;
                
                swapped = true;
            }
        }
    } while (swapped);
}

