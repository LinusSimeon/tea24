#include "module.h"

#include <fmt/format.h>

/* .bss Segment */
int foo;
int foo2 = 0;

/* .data Segment */
int data = 45;
int data2 = 43;

/* .rodata Segment */

const int rodata = 45;
const int rodata2 = 43;

const int big_data_blob[] = {42,0xff};

/*
 * Teil 2: module-lokale Variablen (internal linkage)
 * Durch "static" sind diese Variablen NUR in dieser Datei (module.cpp) sichtbar.
 * Sie werden bewusst NICHT in module.h deklariert.
 */
static int moduleCounter;          /* ohne Initialisierung -> .bss  */
static int moduleValue = 123;      /* mit Initialisierung   -> .data */
static const int moduleConst = 77; /* konstant              -> .rodata */

void incrementModuleCounter()
{
    moduleCounter = moduleCounter + 1;
}

void printModuleVariables()
{
    fmt::print("moduleCounter, value {}, address {}\n", moduleCounter, fmt::ptr(&moduleCounter));
    fmt::print("moduleValue,   value {}, address {}\n", moduleValue, fmt::ptr(&moduleValue));
    fmt::print("moduleConst,   value {}, address {}\n", moduleConst, fmt::ptr(&moduleConst));
}

void doSomething(int value)
{ 
    value = value + 1;
    fmt::print("doSomething, value {}\n", value);
}

void doSomethingPtr(int* ptr)
{
    *ptr = *ptr + 1;
    fmt::print("doSomethingPtr, value {}\n", *ptr);
}

int* doReturnAPtr()
{
    int value = 4711;
    fmt::print("doReturnAPtr, value {}\n", value);
    /* 
     * This causes a warning and shall be avoided by all means
     * warning: address of stack memory associated with local variable 'value' returned [-Wreturn-stack-address]
     */
    return &value;
}

void doSomethingStatic()
{
    static int value = 56;
    value = value + 1;
    fmt::print("doSomethingStatic, value {}, address {}\n", value, fmt::ptr(&value));

    /* Vergleich: normale (nicht-statische) lokale Variable auf dem Stack */
    int normal = 56;
    normal = normal + 1;
    fmt::print("  normal,           value {}, address {}\n", normal, fmt::ptr(&normal));
}
 