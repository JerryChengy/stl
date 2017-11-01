#pragma once

typedef enum StackWalkOptions
{
	// No addition info will be retrived 
	// (only the address is available)
	RetrieveNone = 0,

	// Try to get the symbol-name
	RetrieveSymbol = 1,

	// Try to get the line for this symbol
	RetrieveLine = 2,

	// Try to retrieve the module-infos
	RetrieveModuleInfo = 4,

	// Also retrieve the version for the DLL/EXE
	RetrieveFileVersion = 8,

	// Contains all the abouve
	RetrieveVerbose = 0xF,

	// Generate a "good" symbol-search-path
	SymBuildPath = 0x10,

	// Also use the public Microsoft-Symbol-Server
	SymUseSymSrv = 0x20,

	// Contains all the abouve "Sym"-options
	SymAll = 0x30,

	// Contains all options (default)
	OptionsAll = 0x3F
} StackWalkOptions;

