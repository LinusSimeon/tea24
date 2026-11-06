# Ergebnisse exercise-002: `static` in Funktion und auf Modulebene

## Umgebung

Alle Ergebnisse stammen aus dem Dev-Container (Linux, `g++`, CMake mit Ninja, Build-Typ `RelWithDebInfo`). Gebaut und ausgeführt wurde aus dem Hauptverzeichnis:

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/bin/exercise-002
```

Die Adressen ändern sich bei jedem Programmstart, weil Linux das Programm per **ASLR** (Address Space Layout Randomization) an eine zufällige Basisadresse lädt. Innerhalb eines Programmlaufs bleiben sie konstant, und die Abstände zwischen den Variablen ändern sich nicht.

> **Nebenbeobachtung: Dangling Pointer**
> Beim ersten Start stürzte das Programm mit `Segmentation fault` ab. Die Ursache ist `doReturnAPtr()`: Die Funktion gibt die Adresse einer lokalen Stack-Variable zurück, die nach dem Rücksprung nicht mehr existiert. `g++` erkennt das (`-Wreturn-local-addr`) und gibt stattdessen `nullptr` zurück. Die Dereferenzierung in `main.cpp` führt dann zum Absturz. Die betroffenen Zeilen in `main.cpp` sind auskommentiert.

---

## Teil 1: `static` innerhalb einer Funktion

### Umsetzung

```cpp
void doSomethingStatic()
{
    static int value = 56;
    value = value + 1;
    fmt::print("doSomethingStatic, value {}, address {}\n", value, fmt::ptr(&value));

    /* Vergleich: normale (nicht-statische) lokale Variable */
    int normal = 56;
    normal = normal + 1;
    fmt::print("  normal,           value {}, address {}\n", normal, fmt::ptr(&normal));
}
```

### Ausgabe (vier Aufrufe)

```
doSomethingStatic, value 57, address 0x58d8414d3018
  normal,           value 57, address 0x7ffc33883b4c
doSomethingStatic, value 58, address 0x58d8414d3018
  normal,           value 57, address 0x7ffc33883b4c
doSomethingStatic, value 59, address 0x58d8414d3018
  normal,           value 57, address 0x7ffc33883b4c
doSomethingStatic, value 60, address 0x58d8414d3018
  normal,           value 57, address 0x7ffc33883b4c
```

### Auswertung

- **Bleibt die Adresse konstant?** Ja. Es gibt nur eine einzige Variable `value`. Sie liegt fest im Datenbereich des Programms, direkt neben `data` und `data2` im `.data`-Segment, und nicht auf dem Stack.
- **Warum wird der Wert weiter erhöht?** Die Initialisierung `= 56` findet nur einmal statt. Die Variable existiert während der gesamten Programmlaufzeit und behält ihren Wert zwischen den Aufrufen.
- **Vergleich mit der nicht-statischen Variable:** `normal` liegt auf dem Stack (`0x7ffc...`) und wird bei jedem Aufruf neu angelegt und initialisiert. Deshalb ist der Wert immer 57. Dass auch ihre Adresse gleich bleibt, ist Zufall: Die Funktion wird jedes Mal aus derselben Stack-Tiefe aufgerufen.

|                 | `static int value`       | `int normal`            |
|-----------------|--------------------------|-------------------------|
| Wert            | 57, 58, 59, 60           | immer 57                |
| Speicherort     | `.data`                  | Stack                   |
| Initialisierung | einmal                   | bei jedem Aufruf        |
| Lebensdauer     | gesamte Programmlaufzeit | nur während des Aufrufs |
| Sichtbarkeit    | nur in der Funktion      | nur in der Funktion     |

---

## Teil 2: `static` auf Modulebene

### Umsetzung

Die Variablen stehen in `module.cpp` und sind bewusst **nicht** in `module.h` deklariert:

```cpp
static int moduleCounter;          /* ohne Initialisierung */
static int moduleValue = 123;      /* mit Initialisierung  */
static const int moduleConst = 77; /* konstant             */

void incrementModuleCounter();     /* erhöht moduleCounter */
void printModuleVariables();       /* gibt Werte und Adressen aus */
```

In `module.h` stehen nur die beiden Funktionen. `main.cpp` greift ausschließlich über diese Funktionen auf die Variablen zu.

### Ausgabe

```
moduleCounter, value 0, address 0x61e508f8c038
moduleValue,   value 123, address 0x61e508f8c01c
moduleCounter, value 2, address 0x61e508f8c038   <- nach 2x incrementModuleCounter()
moduleValue,   value 123, address 0x61e508f8c01c
```

### Nachweis der internal linkage

**Linker-Test:** Eine zweite Übersetzungseinheit versucht, per `extern int moduleCounter;` auf die Variable zuzugreifen:

```
undefined reference to `moduleCounter'
collect2: error: ld returned 1 exit status
```

Der Linker findet das Symbol nicht, weil es durch `static` nur innerhalb von `module.cpp` existiert.

**Symboltabelle (`nm`):** Großbuchstaben bedeuten globale Symbole (external linkage), Kleinbuchstaben lokale Symbole (internal linkage):

```
D data            <- global, .data
B foo             <- global, .bss
d moduleValue     <- lokal,  .data
b moduleCounter   <- lokal,  .bss
```

---

## Teil 3: Segmentzuordnung

### `nm -S` (Adresse, Größe, Typ, Name)

```
0000000000002320 0000000000000004 r moduleConst
000000000000401c 0000000000000004 d moduleValue
0000000000004038 0000000000000004 b moduleCounter
0000000000004018 0000000000000004 d doSomethingStatic()::value
```

### Gegenprobe mit `size --format=sysv`

| Segment   | Größe | Startadresse | Bereich (hex)        |
|-----------|------:|-------------:|----------------------|
| `.rodata` |   824 |         8192 | `0x2000` bis `0x2338` |
| `.data`   |    32 |        16392 | `0x4008` bis `0x4028` |
| `.bss`    |    24 |        16424 | `0x4028` bis `0x4040` |

Alle Adressen aus `nm` liegen in dem Segment, das der Buchstabe angibt.

### Ergebnis

| Variable                             | Segment   | Begründung                     |
|--------------------------------------|-----------|--------------------------------|
| `static int moduleCounter;`          | `.bss`    | keine Initialisierung, also 0  |
| `static int moduleValue = 123;`      | `.data`   | Startwert ≠ 0                  |
| `static const int moduleConst = 77;` | `.rodata` | konstant, Adresse wird benutzt |
| `static int value = 56;` (lokal)     | `.data`   | Startwert ≠ 0                  |

### Was man daraus lernt

- **`.bss`:** Variablen ohne Initialisierung oder mit `= 0`. Sie belegen keinen Platz in der Programmdatei, der Bereich wird beim Start mit Nullen gefüllt.
- **`.data`:** Variablen mit einem Startwert ungleich 0. Der Wert wird in der Programmdatei gespeichert und beim Start in den RAM kopiert. Auf einem Mikrocontroller kostet das Flash **und** RAM.
- **`.rodata`:** `const`-Daten, sofern sie tatsächlich Speicher brauchen. Sie liegen im Flash und brauchen keinen RAM.
- Ob eine `static`-Variable in einer Funktion oder auf Modulebene steht, ist für das Segment egal. Entscheidend sind Initialisierung und `const`.

---

## Bonus 4: Null- und Nicht-Null-Initialisierung

Die vier Varianten aus der Aufgabe sind in `module.cpp` bereits vorhanden:

| Aufgabe            | in `module.cpp`                 | `nm` | Segment   |
|--------------------|---------------------------------|------|-----------|
| `int a;`           | `int foo;`                      | `B`  | `.bss`    |
| `int b = 0;`       | `int foo2 = 0;`                 | `B`  | `.bss`    |
| `int c = 1;`       | `int data = 45;`                | `D`  | `.data`   |
| `const int d = 1;` | `extern const int rodata = 45;` | `R`  | `.rodata` |

- **Warum liegen `a` und `b` in `.bss`?** Globale und statische Variablen werden laut C++-Standard ohnehin mit 0 initialisiert. Für den Compiler ist `int a;` daher dasselbe wie `int b = 0;`, und es muss kein Wert gespeichert werden.
- **Warum liegt `c` in `.data`?** Der Startwert 1 muss in der Programmdatei abgelegt und beim Start in den beschreibbaren RAM kopiert werden.
- **Wann erscheint `d` in `.rodata`?** Separate Testprogramme haben Folgendes ergeben:

| Situation                                     | Ergebnis                          |
|-----------------------------------------------|-----------------------------------|
| nur der Wert wird benutzt (`return d;`)       | kein Symbol, Wert direkt im Code  |
| Adresse wird benutzt (`&d`)                   | `r`: `.rodata` (lokal)            |
| `extern const int d = 1;`                     | `R`: `.rodata` (global)           |
| unbenutzt, ohne Optimierung (`-O0`)           | bleibt oft in `.rodata`           |

`const`-Daten landen also nur in `.rodata`, wenn sie wirklich Speicher brauchen. Sonst setzt der Compiler den Zahlenwert direkt ein. Außerdem hat `const int` auf Dateiebene in C++ automatisch internal linkage, genau wie `static`. Erst `extern` macht das Symbol global.

---

## Fazit

### 1. Unterschied zwischen `static` in einer Funktion und auf Modulebene

In einer **Funktion** verändert `static` die **Lebensdauer**: Die Variable liegt nicht mehr auf dem Stack, sondern existiert während der gesamten Programmlaufzeit. Sie wird nur einmal initialisiert und behält ihren Wert zwischen Aufrufen. Sichtbar ist sie weiterhin nur in der Funktion.

Auf **Modulebene** leben Variablen ohnehin die ganze Laufzeit lang. Dort verändert `static` die **Sichtbarkeit**: Die Variable ist nur in der eigenen Übersetzungseinheit bekannt (internal linkage). Andere Dateien können sie auch mit `extern` nicht erreichen.

### 2. Segmentzuordnung der Beispielvariablen

- **`.bss`:** `moduleCounter`, `foo`, `foo2` (nicht oder mit 0 initialisiert)
- **`.data`:** `moduleValue`, `data`, `data2` und die statische lokale Variable `value` aus `doSomethingStatic()` (Startwert ungleich 0)
- **`.rodata`:** `moduleConst`, `rodata`, `rodata2`, `big_data_blob` (konstante Daten)

### 3. Warum `static` für Kapselung in Modulen nützlich ist

Mit `static` bleiben die internen Daten eines Moduls privat. Andere Module greifen nur über die im Header veröffentlichten Funktionen darauf zu und können den Zustand nicht unkontrolliert verändern. Außerdem gibt es keine Namenskonflikte: Zwei Module können gleichnamige `static`-Variablen haben, ohne dass der Linker einen Fehler meldet. So bleibt die Schnittstelle klein und die Implementierung austauschbar.
