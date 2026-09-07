#pragma once
#include <dbghelp.h>
#include <fstream>
#pragma comment(lib, "dbghelp.lib")

inline LONG WINAPI ReferenceCrashTrace(EXCEPTION_POINTERS* exception)
{
    std::ofstream trace("crash-trace.txt");
    trace << "exception=" << std::hex << exception->ExceptionRecord->ExceptionCode << '\n';
    const HANDLE process = GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    if (!SymInitialize(process, nullptr, TRUE)) return EXCEPTION_EXECUTE_HANDLER;
    CONTEXT context = *exception->ContextRecord;
    STACKFRAME64 frame{};
    frame.AddrPC = {context.Rip, 0, AddrModeFlat};
    frame.AddrFrame = {context.Rbp, 0, AddrModeFlat};
    frame.AddrStack = {context.Rsp, 0, AddrModeFlat};
    for (unsigned index = 0; index < 32 && frame.AddrPC.Offset; ++index)
    {
        alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
        auto symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_SYM_NAME;
        DWORD64 displacement = 0;
        trace << frame.AddrPC.Offset << ' ';
        if (SymFromAddr(process, frame.AddrPC.Offset, &displacement, symbol)) trace << symbol->Name;
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD offset = 0;
        if (SymGetLineFromAddr64(process, frame.AddrPC.Offset, &offset, &line))
            trace << ' ' << line.FileName << ':' << std::dec << line.LineNumber << std::hex;
        trace << std::endl;
        if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, GetCurrentThread(), &frame, &context,
                nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) break;
    }
    SymCleanup(process);
    return EXCEPTION_EXECUTE_HANDLER;
}
