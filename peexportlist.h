/*
	Copyright (C) 2019 Arves100

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
	
	Changes in Version 0.1.1:
		Changed a variable to "unsigned int" in order to fix warnings
*/

/*
Original header
-------------------------------
PEExportList 0.1.1
by Luigi Auriemma
e-mail: aluigi@autistici.org
web:    alugi.org


INTRODUCTION
------------
This code, originally written by Matt Pietrek in the far 1997 (Hood1197), gets
all the exported functions of a given executable or DLL.
I have only optimized the original source code and ported it to C


USAGE EXAMPLE
-------------
int main(int argc, char *argv[]) {
    PEExportList        pel;
    ExportedSymbolInfo  *pCurrent;

    if(argc < 2) exit(1);

    PEExportList_init(&pel);

    if(LoadExportInfo(argv[1], &pel) < 0) {
        printf("\nError during the loading of the input file\n");
        exit(1);
    }

    pCurrent = NULL;
    while((pCurrent = PEExportList_GetNextSymbol(&pel, pCurrent))) {
        printf("  %-5hu %s\n", pCurrent->m_ordinal, pCurrent->m_pszName);
    }

    PEExportList_init(&pel);

    return(0);
}


LICENSE
-------
The original Matt's code had no license (only "// Matt Pietrek // Microsoft Systems Journal, November 1997")
so the following is the license applied to my derived work:
    Copyright 2008 Luigi Auriemma

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

    http://www.gnu.org/licenses/gpl-2.0.txt
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <imagehlp.h>



#define PEExportList_GrowSize   100



typedef struct {
    char                *m_pszName;
    WORD                m_ordinal;
} ExportedSymbolInfo;



typedef struct {
    ExportedSymbolInfo  *m_pSymbols;
    unsigned            m_symbolCount;
} PEExportList;



void PEExportList_init(PEExportList *pel) {
    pel->m_pSymbols    = NULL;
    pel->m_symbolCount = 0;
}



void PEExportList_free(PEExportList *pel) {
    ExportedSymbolInfo  *pCurrent;
    unsigned int                 i;

    pCurrent = pel->m_pSymbols;

    for(i = 0; i < pel->m_symbolCount; i++) {
        if(pCurrent->m_pszName) free(pCurrent->m_pszName);
        pCurrent++;
    }
    if(pel->m_pSymbols) free(pel->m_pSymbols);
}



ExportedSymbolInfo *PEExportList_AddSymbol(PEExportList *pel, char *pszSymbolName, int ordinal) {
    ExportedSymbolInfo  *pCurrent;

    if(!(pel->m_symbolCount % PEExportList_GrowSize)) {
        pel->m_pSymbols = (ExportedSymbolInfo *)realloc(pel->m_pSymbols, (pel->m_symbolCount + PEExportList_GrowSize) * sizeof(ExportedSymbolInfo));
    }

    pCurrent = &pel->m_pSymbols[pel->m_symbolCount];

    if(pszSymbolName) {
        pCurrent->m_pszName = _strdup(pszSymbolName);
    } else {
        pCurrent->m_pszName = NULL;
    }
    pCurrent->m_ordinal = ordinal;
    pel->m_symbolCount++;
    return(pCurrent);
}



ExportedSymbolInfo *PEExportList_GetNextSymbol(PEExportList *pel, ExportedSymbolInfo *pSymbol) {
    ExportedSymbolInfo *pEndSymbol;

    pEndSymbol = &pel->m_pSymbols[pel->m_symbolCount];
    if(!pSymbol && pel->m_symbolCount) return(pel->m_pSymbols);
    if(++pSymbol < pEndSymbol) return(pSymbol);
    return(NULL);
}



ExportedSymbolInfo *PEExportList_LookupSymbol(PEExportList *pel, char *pszSymbolName) {
    ExportedSymbolInfo  *pSymbol;

    pSymbol = NULL;
    while((pSymbol = PEExportList_GetNextSymbol(pel, pSymbol))) {
        if(!strcmp(pSymbol->m_pszName, pszSymbolName)) break;
    }
    return(pSymbol);
}



int LoadExportInfo(char *pszFilename, PEExportList *pel) {
    PIMAGE_EXPORT_DIRECTORY pExpDir;
    LOADED_IMAGE    li;
    PDWORD      *pExpNames,
                pFunctions;
    PWORD       pNameOrdinals,
                pCurrOrd;
    DWORD       cNamedOrdinals,
                cFunctions;
    PBYTE       pNamedFuncs;
    unsigned    i;

    if(!MapAndLoad(pszFilename, "", &li, TRUE, TRUE)) return(-1);

    pExpDir   = (PIMAGE_EXPORT_DIRECTORY)(li.FileHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
    if(!pExpDir) return(-1);
    pExpDir   = ImageRvaToVa(li.FileHeader, li.MappedAddress, (DWORD)pExpDir, 0);

    pExpNames      = ImageRvaToVa(
                     li.FileHeader, li.MappedAddress,
                     pExpDir->AddressOfNames, 0);
    pNameOrdinals  = ImageRvaToVa(
                     li.FileHeader, li.MappedAddress,
                     pExpDir->AddressOfNameOrdinals, 0);
    pFunctions     = ImageRvaToVa(
                     li.FileHeader, li.MappedAddress,
                     pExpDir->AddressOfFunctions, 0);
    cNamedOrdinals = pExpDir->NumberOfNames;
    cFunctions     = pExpDir->NumberOfFunctions;
    pNamedFuncs    = calloc(cFunctions, sizeof(BYTE));
    pCurrOrd       = pNameOrdinals;

    for(i = 0; i < pExpDir->NumberOfNames; i++) {
        PEExportList_AddSymbol(pel,
            ImageRvaToVa(li.FileHeader, li.MappedAddress, (DWORD)*pExpNames, 0),
            *pCurrOrd + 1);

        pNamedFuncs[*pCurrOrd] = 1;
        pExpNames++;
        pCurrOrd++;
    }

    for(i = 0; i < cFunctions; i++) {
        if(pNamedFuncs[i]) continue;

        // if(!pFunctions[i]) continue;

        PEExportList_AddSymbol(pel, NULL, pExpDir->Base + i);
    }

    free(pNamedFuncs);
    UnMapAndLoad(&li);
    return(0);
}


