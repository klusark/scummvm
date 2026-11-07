/* ScummVM is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

// Clean-room tests only: writes a minidump to %LOCALAPPDATA%\CrashDumps\scummvm.exe.<pid>.dmp when the process
// crashes (Windows writes none for scummvm.exe), where cleanroom/tools/crashstack.py reads it.

#if defined(_WIN32) || defined(__MINGW32__)

#define FORBIDDEN_SYMBOL_ALLOW_ALL
#include <windows.h>
#include <dbghelp.h>
#include <stdio.h>
#include <string.h>

namespace Grim {

typedef BOOL (WINAPI *MiniDumpWriteDumpFn)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE, PMINIDUMP_EXCEPTION_INFORMATION, PMINIDUMP_USER_STREAM_INFORMATION, PMINIDUMP_CALLBACK_INFORMATION);

static LONG WINAPI crashDumpFilter(EXCEPTION_POINTERS *info) {
	char dir[MAX_PATH], path[MAX_PATH];
	DWORD n = GetEnvironmentVariableA("LOCALAPPDATA", dir, sizeof(dir));
	if (n == 0 || n >= sizeof(dir))
		return EXCEPTION_CONTINUE_SEARCH;
	snprintf(path, sizeof(path), "%s\\CrashDumps", dir);
	CreateDirectoryA(path, nullptr);
	snprintf(path, sizeof(path), "%s\\CrashDumps\\scummvm.exe.%lu.dmp", dir, (unsigned long)GetCurrentProcessId());
	HMODULE dbghelp = LoadLibraryA("dbghelp.dll");
	MiniDumpWriteDumpFn write = dbghelp ? (MiniDumpWriteDumpFn)GetProcAddress(dbghelp, "MiniDumpWriteDump") : nullptr;
	HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (write && file != INVALID_HANDLE_VALUE) {
		MINIDUMP_EXCEPTION_INFORMATION ex;
		ex.ThreadId = GetCurrentThreadId();
		ex.ExceptionPointers = info;
		ex.ClientPointers = FALSE;
		write(GetCurrentProcess(), GetCurrentProcessId(), file, MiniDumpNormal, &ex, nullptr, nullptr);
	}
	if (file != INVALID_HANDLE_VALUE)
		CloseHandle(file);
	// The call chain, unwound with the exe's and the DLLs' unwind tables (a scan of the stack finds stale return addresses):
	// "<module> <rva>" per line in the .txt next to the dump.
#if defined(_M_X64) || defined(__x86_64__)
	snprintf(path, sizeof(path), "%s\\CrashDumps\\scummvm.exe.%lu.txt", dir, (unsigned long)GetCurrentProcessId());
	FILE *out = fopen(path, "w");
	if (out) {
		CONTEXT ctx = *info->ContextRecord;
		for (int i = 0; i < 64 && ctx.Rip; i++) {
			HMODULE mod = nullptr;
			char name[MAX_PATH] = "?";
			if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)ctx.Rip, &mod)) {
				GetModuleFileNameA(mod, name, sizeof(name));
				const char *slash = strrchr(name, '\\');
				fprintf(out, "%s %llx\n", slash ? slash + 1 : name, (unsigned long long)(ctx.Rip - (DWORD64)mod));
			} else {
				fprintf(out, "? %llx\n", (unsigned long long)ctx.Rip);
			}
			DWORD64 base = 0;
			PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx.Rip, &base, nullptr);
			if (fn) {
				PVOID handlerData = nullptr;
				DWORD64 frame = 0;
				RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, ctx.Rip, fn, &ctx, &handlerData, &frame, nullptr);
			} else {   // (a leaf function without unwind data: the return address is on top of the stack)
				ctx.Rip = *(DWORD64 *)ctx.Rsp;
				ctx.Rsp += 8;
			}
		}
		fclose(out);
	}
#endif
	return EXCEPTION_CONTINUE_SEARCH;
}

void installCrashDump() {
	SetUnhandledExceptionFilter(crashDumpFilter);
}

} // End of namespace Grim

#else

namespace Grim {
void installCrashDump() {}
} // End of namespace Grim

#endif
