// WhatsUI Monkey Runner — random-input fuzzer for the WhatsUI GLFW example
// applications.
//
// What it does
// ------------
// 1. Spawns a target executable as a child process, attached as a debugger
//    (DEBUG_ONLY_THIS_PROCESS). This gives us first- and second-chance
//    exceptions without any registry/WER setup.
// 2. Waits for the target's main window to appear, records its client rect.
// 3. Streams pseudo-random pointer clicks, drags, and (optionally) keystrokes
//    into the window via PostMessage. Every event is logged to events.jsonl.
// 4. A debug-event loop runs concurrently. On a fatal second-chance exception
//    it writes a minidump with `MiniDumpWriteDump` and marks the run as a
//    crash. Normal Continue is returned for everything else so the process
//    keeps running.
// 5. On exit, writes a crash_report.txt (or run_report.txt for clean exits)
//    summarising the outcome, the last N events, and pointers to the log
//    files.
//
// The whole tool is deliberately a single translation unit and depends only
// on Win32 + Dbghelp. It does NOT link against WhatsUI, so it survives target
// crashes and can be used against any GLFW/Win32 application.
//
// Usage
// -----
//   WhatsUIMonkeyRunner <target.exe> [options] [-- <target args...>]
//
// Options
//   --seed N                 PRNG seed (default: time-based)
//   --iterations N           Stop after N events (default: unlimited)
//   --interval-ms N          Delay between events (default: 25)
//   --duration-sec N         Max wall-clock duration (default: 60)
//   --wait-startup-ms N      Poll interval before window appears (default: 5000)
//   --include-keys           Also fuzz keyboard input
//   --no-dump                Skip debugger attachment (fast, no minidump)
//   --log-dir DIR            Where to write reports (default: ./monkey_logs)
//   --max-logged-events N    Ring-buffer size for crash report (default: 200)
//
// Exit codes
//   0   Clean run — target exited normally or we hit the iteration/time limit
//       and terminated it ourselves.
//   1   Bad arguments / target not found.
//   2   Target crashed during the run. See `<log-dir>/<run>/crash_report.txt`.
//   3   Runner internal error (window never appeared, debug loop failed, ...).

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dbghelp.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "User32.lib")

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Options
// ---------------------------------------------------------------------------

struct Options {
    std::wstring targetExe;
    std::vector<std::wstring> targetArgs;
    unsigned seed = 0;
    bool seedFromClock = true;
    int iterations = 0; // 0 = unlimited
    int intervalMs = 25;
    int durationSec = 60;
    int waitStartupMs = 5000;
    bool includeKeys = false;
    bool attachDebugger = true;
    fs::path logDir;
    int maxLoggedEvents = 200;
};

static void printUsage()
{
    std::fputs(
        "WhatsUIMonkeyRunner <target.exe> [options] [-- <target args...>]\n"
        "  --seed N                 PRNG seed (default: time-based)\n"
        "  --iterations N           Stop after N events (default: unlimited)\n"
        "  --interval-ms N          Delay between events (default: 25)\n"
        "  --duration-sec N         Max wall-clock duration (default: 60)\n"
        "  --wait-startup-ms N      Wait budget for target window (default: 5000)\n"
        "  --include-keys           Also fuzz keyboard input\n"
        "  --no-dump                Skip debugger attachment (no minidump)\n"
        "  --log-dir DIR            Report directory (default: ./monkey_logs)\n"
        "  --max-logged-events N    Ring-buffer size (default: 200)\n",
        stderr);
}

static std::wstring utf8ToWide(const std::string& s)
{
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), n);
    return out;
}

static bool parseInt(const char* s, int& out)
{
    if (!s || !*s) return false;
    try { out = std::stoi(s); return true; } catch (...) { return false; }
}

static bool parseUnsigned(const char* s, unsigned& out)
{
    if (!s || !*s) return false;
    try { out = static_cast<unsigned>(std::stoul(s)); return true; } catch (...) { return false; }
}

static bool parseOptions(int argc, char** argv, Options& opt)
{
    if (argc < 2) { printUsage(); return false; }
    opt.targetExe = utf8ToWide(argv[1]);
    bool sawSeparator = false;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (sawSeparator) {
            opt.targetArgs.push_back(utf8ToWide(a));
            continue;
        }
        if (a == "--") { sawSeparator = true; continue; }
        auto next = [&](int& out) {
            if (i + 1 >= argc) return false;
            return parseInt(argv[++i], out);
        };
        if (a == "--seed") {
            if (i + 1 >= argc) return false;
            if (!parseUnsigned(argv[++i], opt.seed)) return false;
            opt.seedFromClock = false;
        } else if (a == "--iterations") { if (!next(opt.iterations)) return false; }
        else if (a == "--interval-ms")  { if (!next(opt.intervalMs)) return false; }
        else if (a == "--duration-sec") { if (!next(opt.durationSec)) return false; }
        else if (a == "--wait-startup-ms") { if (!next(opt.waitStartupMs)) return false; }
        else if (a == "--include-keys") { opt.includeKeys = true; }
        else if (a == "--no-dump")      { opt.attachDebugger = false; }
        else if (a == "--log-dir") {
            if (i + 1 >= argc) return false;
            opt.logDir = fs::path(utf8ToWide(argv[++i]));
        }
        else if (a == "--max-logged-events") { if (!next(opt.maxLoggedEvents)) return false; }
        else if (a == "-h" || a == "--help") { printUsage(); return false; }
        else {
            std::fprintf(stderr, "Unknown option: %s\n", a.c_str());
            printUsage();
            return false;
        }
    }
    if (opt.logDir.empty()) opt.logDir = fs::current_path() / "monkey_logs";
    if (opt.seedFromClock) {
        opt.seed = static_cast<unsigned>(
            std::chrono::steady_clock::now().time_since_epoch().count());
    }
    return true;
}

// ---------------------------------------------------------------------------
// Event log
// ---------------------------------------------------------------------------

struct MonkeyEvent {
    long long tMs = 0;
    const char* kind = "click"; // "click", "drag", "wheel", "key"
    int button = 0;             // 1=L, 2=R
    int x = 0, y = 0;
    int x2 = 0, y2 = 0;
    int key = 0;
    int wheelDelta = 0;
};

class EventLog {
public:
    EventLog(fs::path path, std::size_t ringCap)
        : ringCap_(ringCap)
    {
        stream_.open(path, std::ios::out | std::ios::trunc);
    }

    void push(const MonkeyEvent& e)
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (stream_.is_open()) {
            stream_
                << "{\"t\":" << e.tMs
                << ",\"kind\":\"" << e.kind << "\""
                << ",\"button\":" << e.button
                << ",\"x\":" << e.x << ",\"y\":" << e.y
                << ",\"x2\":" << e.x2 << ",\"y2\":" << e.y2
                << ",\"key\":" << e.key
                << ",\"wheel\":" << e.wheelDelta
                << "}\n";
            stream_.flush();
        }
        if (ring_.size() < ringCap_) {
            ring_.push_back(e);
        } else {
            ring_[ringIdx_] = e;
            ringIdx_ = (ringIdx_ + 1) % ringCap_;
        }
    }

    std::vector<MonkeyEvent> tail() const
    {
        std::lock_guard<std::mutex> lock(mu_);
        std::vector<MonkeyEvent> out;
        if (ring_.size() < ringCap_) return ring_;
        out.reserve(ringCap_);
        for (std::size_t i = 0; i < ringCap_; ++i) {
            out.push_back(ring_[(ringIdx_ + i) % ringCap_]);
        }
        return out;
    }

private:
    mutable std::mutex mu_;
    std::ofstream stream_;
    std::vector<MonkeyEvent> ring_;
    std::size_t ringCap_;
    std::size_t ringIdx_ = 0;
};

// ---------------------------------------------------------------------------
// Window discovery
// ---------------------------------------------------------------------------

struct FindWindowState {
    DWORD pid = 0;
    HWND hwnd = nullptr;
};

static BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM lp)
{
    auto* state = reinterpret_cast<FindWindowState*>(lp);
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != state->pid) return TRUE;
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return TRUE;
    wchar_t title[256] = {};
    GetWindowTextW(hwnd, title, 255);
    if (title[0] == L'\0') return TRUE;
    state->hwnd = hwnd;
    return FALSE;
}

static HWND waitForTargetWindow(DWORD pid, int budgetMs)
{
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(budgetMs);
    while (std::chrono::steady_clock::now() < deadline) {
        FindWindowState st{pid, nullptr};
        EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&st));
        if (st.hwnd) return st.hwnd;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Debug loop — writes minidumps on second-chance fatal exceptions
// ---------------------------------------------------------------------------

struct CrashInfo {
    std::atomic<bool> crashed{false};
    std::atomic<DWORD> exceptionCode{0};
    std::atomic<uintptr_t> exceptionAddress{0};
    std::atomic<bool> processExited{false};
    std::atomic<DWORD> exitCode{0};
    fs::path dumpPath;
};

static bool isFatalException(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        return true;
    default:
        return false;
    }
}

static const char* exceptionName(DWORD code)
{
    switch (code) {
    case EXCEPTION_ACCESS_VIOLATION:       return "ACCESS_VIOLATION";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:  return "ARRAY_BOUNDS_EXCEEDED";
    case EXCEPTION_ILLEGAL_INSTRUCTION:    return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR:          return "IN_PAGE_ERROR";
    case EXCEPTION_INT_DIVIDE_BY_ZERO:     return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_INT_OVERFLOW:           return "INT_OVERFLOW";
    case EXCEPTION_PRIV_INSTRUCTION:       return "PRIV_INSTRUCTION";
    case EXCEPTION_STACK_OVERFLOW:         return "STACK_OVERFLOW";
    case EXCEPTION_BREAKPOINT:             return "BREAKPOINT";
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:return "NONCONTINUABLE";
    case 0xE06D7363:                       return "MS_CPP_EXCEPTION"; // C++ throw
    default:                               return "UNKNOWN";
    }
}

static void writeMinidump(HANDLE hProcess, DWORD pid, const DEBUG_EVENT& ev,
                          const fs::path& dumpPath)
{
    HANDLE dumpFile = CreateFileW(dumpPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (dumpFile == INVALID_HANDLE_VALUE) return;

    // The exception record and pointers are in our (debugger) address space.
    EXCEPTION_RECORD localRecord = ev.u.Exception.ExceptionRecord;
    EXCEPTION_POINTERS pointers{};
    pointers.ExceptionRecord = &localRecord;
    pointers.ContextRecord = nullptr;

    MINIDUMP_EXCEPTION_INFORMATION mei{};
    mei.ThreadId = ev.dwThreadId;
    mei.ExceptionPointers = &pointers;
    mei.ClientPointers = FALSE;

    MINIDUMP_TYPE type = static_cast<MINIDUMP_TYPE>(
        MiniDumpNormal
        | MiniDumpWithThreadInfo
        | MiniDumpWithHandleData);
    MiniDumpWriteDump(hProcess, pid, dumpFile, type, &mei, nullptr, nullptr);
    CloseHandle(dumpFile);
}

// ---------------------------------------------------------------------------
// Minidump summariser — best-effort analysis without a debugger. Reads the
// module list from the dump and resolves the exception address to
// `<module>!<offset>`. Also prints the exception parameters (for
// ACCESS_VIOLATION these are direction+addr, which are the actionable
// clues 90% of the time).
// ---------------------------------------------------------------------------

struct StackFrame {
    ULONG64 address = 0;
    std::string module;
    std::string symbol;
    ULONG64 symbolDisplacement = 0;
    std::string sourceFile;
    unsigned sourceLine = 0;
};

struct DumpSummary {
    std::string faultingModule = "<unknown>";
    ULONG64 moduleBase = 0;
    ULONG64 moduleSize = 0;
    ULONG64 offsetInModule = 0;
    std::string violationDirection; // "read" / "write" / "execute"
    ULONG64 violationAddress = 0;
    bool haveViolation = false;
    std::vector<std::pair<std::string, ULONG64>> modules; // name + base
    // Populated when the loader can find PDBs next to the modules.
    std::string faultingSymbol;
    std::string faultingSourceFile;
    unsigned faultingSourceLine = 0;
    ULONG64 symbolDisplacement = 0;
    // Populated when StackWalk64 succeeds against the crashing thread.
    std::vector<StackFrame> stack;
};

// Memory reader state for StackWalk64. Dbghelp's PREAD_PROCESS_MEMORY_ROUTINE64
// callback has no context pointer, so we stash a per-thread pointer here.
struct DumpMemoryReader {
    const std::uint8_t* dumpBase = nullptr;
    const MINIDUMP_MEMORY_LIST* memList = nullptr;
    const MINIDUMP_MEMORY64_LIST* mem64List = nullptr;
};
thread_local DumpMemoryReader g_reader;

static BOOL CALLBACK dumpReadMemory(HANDLE, DWORD64 addr, PVOID buf, DWORD size,
                                    LPDWORD outRead)
{
    if (outRead) *outRead = 0;
    if (!g_reader.dumpBase || size == 0) return FALSE;

    // MemoryListStream first (used by MiniDumpNormal).
    if (g_reader.memList) {
        for (ULONG i = 0; i < g_reader.memList->NumberOfMemoryRanges; ++i) {
            const auto& m = g_reader.memList->MemoryRanges[i];
            const ULONG64 lo = m.StartOfMemoryRange;
            const ULONG64 hi = lo + m.Memory.DataSize;
            if (addr < lo || addr >= hi) continue;
            const ULONG64 offInRange = addr - lo;
            const DWORD available = static_cast<DWORD>(
                std::min<ULONG64>(size, hi - addr));
            std::memcpy(buf,
                        g_reader.dumpBase + m.Memory.Rva + offInRange,
                        available);
            if (outRead) *outRead = available;
            return TRUE;
        }
    }
    // Memory64ListStream for full-memory dumps.
    if (g_reader.mem64List) {
        ULONG64 running = g_reader.mem64List->BaseRva;
        for (ULONG64 i = 0; i < g_reader.mem64List->NumberOfMemoryRanges; ++i) {
            const auto& m = g_reader.mem64List->MemoryRanges[i];
            const ULONG64 lo = m.StartOfMemoryRange;
            const ULONG64 hi = lo + m.DataSize;
            if (addr < lo || addr >= hi) { running += m.DataSize; continue; }
            const ULONG64 offInRange = addr - lo;
            const DWORD available = static_cast<DWORD>(
                std::min<ULONG64>(size, hi - addr));
            std::memcpy(buf,
                        g_reader.dumpBase + running + offInRange,
                        available);
            if (outRead) *outRead = available;
            return TRUE;
        }
    }
    return FALSE;
}

// Resolve a single address (symbol + source line) using an already-initialised
// Sym handle. Falls back to `<module>+0xNNN` when no symbol is available.
static void resolveAddress(HANDLE symHandle,
                          const std::vector<std::pair<std::string, ULONG64>>& modules,
                          ULONG64 addr, StackFrame& out)
{
    out.address = addr;
    // Nearest-earlier module by base (modules already sorted-by-base in
    // insertion order? No — use linear scan and pick the largest base <= addr).
    ULONG64 bestBase = 0;
    for (const auto& [name, base] : modules) {
        if (base <= addr && base > bestBase) {
            bestBase = base;
            out.module = name;
        }
    }

    alignas(SYMBOL_INFO) unsigned char symBuf[sizeof(SYMBOL_INFO)
        + 512 * sizeof(TCHAR)] = {};
    auto* symInfo = reinterpret_cast<SYMBOL_INFO*>(symBuf);
    symInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
    symInfo->MaxNameLen = 512;
    DWORD64 disp = 0;
    if (SymFromAddr(symHandle, addr, &disp, symInfo)) {
        out.symbol = symInfo->Name;
        out.symbolDisplacement = disp;
    }
    IMAGEHLP_LINE64 line{};
    line.SizeOfStruct = sizeof(line);
    DWORD lineDisp = 0;
    if (SymGetLineFromAddr64(symHandle, addr, &lineDisp, &line)) {
        out.sourceFile = line.FileName ? line.FileName : "";
        out.sourceLine = line.LineNumber;
    }
}

static std::string moduleBaseName(const wchar_t* fullPath)
{
    std::wstring w(fullPath);
    auto pos = w.find_last_of(L"/\\");
    std::wstring base = (pos == std::wstring::npos) ? w : w.substr(pos + 1);
    return std::string(base.begin(), base.end());
}

static std::optional<DumpSummary> summariseDump(const fs::path& dumpPath)
{
    HANDLE file = CreateFileW(dumpPath.c_str(), GENERIC_READ, FILE_SHARE_READ,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return std::nullopt;
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping) { CloseHandle(file); return std::nullopt; }
    void* base = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (!base) { CloseHandle(mapping); CloseHandle(file); return std::nullopt; }

    DumpSummary out;
    ULONG64 exceptionAddr = 0;

    void* streamData = nullptr;
    ULONG streamSize = 0;
    MINIDUMP_DIRECTORY* dir = nullptr;

    if (MiniDumpReadDumpStream(base, ExceptionStream, &dir, &streamData, &streamSize)
        && streamData) {
        auto* ex = static_cast<MINIDUMP_EXCEPTION_STREAM*>(streamData);
        exceptionAddr = ex->ExceptionRecord.ExceptionAddress;
        if (ex->ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION
            && ex->ExceptionRecord.NumberParameters >= 2) {
            out.haveViolation = true;
            const auto op = ex->ExceptionRecord.ExceptionInformation[0];
            switch (op) {
            case 0: out.violationDirection = "read"; break;
            case 1: out.violationDirection = "write"; break;
            case 8: out.violationDirection = "execute (DEP)"; break;
            default: out.violationDirection = "unknown"; break;
            }
            out.violationAddress = ex->ExceptionRecord.ExceptionInformation[1];
        }
    }

    // Cache the memory streams for StackWalk64's memory reader callback.
    const MINIDUMP_MEMORY_LIST* memList = nullptr;
    const MINIDUMP_MEMORY64_LIST* mem64List = nullptr;
    if (MiniDumpReadDumpStream(base, MemoryListStream, &dir, &streamData, &streamSize)
        && streamData) {
        memList = static_cast<const MINIDUMP_MEMORY_LIST*>(streamData);
    }
    if (MiniDumpReadDumpStream(base, Memory64ListStream, &dir, &streamData, &streamSize)
        && streamData) {
        mem64List = static_cast<const MINIDUMP_MEMORY64_LIST*>(streamData);
    }

    // Capture the crashing thread's CONTEXT for StackWalk64. Prefer the one
    // attached to the ExceptionStream because it is guaranteed to describe the
    // crash frame; fall back to the ThreadListStream entry if needed.
    const CONTEXT* crashContext = nullptr;
    DWORD crashThreadId = 0;
    if (MiniDumpReadDumpStream(base, ExceptionStream, &dir, &streamData, &streamSize)
        && streamData) {
        auto* ex = static_cast<MINIDUMP_EXCEPTION_STREAM*>(streamData);
        crashThreadId = ex->ThreadId;
        if (ex->ThreadContext.DataSize >= sizeof(CONTEXT)) {
            crashContext = reinterpret_cast<const CONTEXT*>(
                static_cast<const std::uint8_t*>(base) + ex->ThreadContext.Rva);
        }
    }

    if (MiniDumpReadDumpStream(base, ModuleListStream, &dir, &streamData, &streamSize)
        && streamData) {
        auto* modules = static_cast<MINIDUMP_MODULE_LIST*>(streamData);
        // Collect (name, path, base, size) for each module. The full path is
        // what SymLoadModuleExW needs so it can find the sibling .pdb.
        struct DumpModule {
            std::string baseName;
            std::wstring fullPath;
            ULONG64 baseAddr = 0;
            ULONG64 size = 0;
        };
        std::vector<DumpModule> dumpModules;
        dumpModules.reserve(modules->NumberOfModules);
        for (ULONG i = 0; i < modules->NumberOfModules; ++i) {
            const auto& m = modules->Modules[i];
            const auto* nameRec = reinterpret_cast<const MINIDUMP_STRING*>(
                static_cast<const std::uint8_t*>(base) + m.ModuleNameRva);
            std::wstring wname(nameRec->Buffer, nameRec->Length / sizeof(wchar_t));
            std::string name = moduleBaseName(wname.c_str());
            out.modules.emplace_back(name, m.BaseOfImage);
            dumpModules.push_back({name, wname, m.BaseOfImage, m.SizeOfImage});
            if (exceptionAddr >= m.BaseOfImage
                && exceptionAddr < m.BaseOfImage + m.SizeOfImage) {
                out.faultingModule = name;
                out.moduleBase = m.BaseOfImage;
                out.moduleSize = m.SizeOfImage;
                out.offsetInModule = exceptionAddr - m.BaseOfImage;
            }
        }

        // Symbol resolution: load EVERY module so StackWalk64 can name frames
        // deep in the call chain (not just the faulting IP). SymLoadModuleExW
        // finds the .pdb next to each image using the file paths recorded in
        // the dump — which is why we build our own binaries with /Zi /DEBUG.
        if (out.moduleBase != 0 && exceptionAddr != 0) {
            HANDLE symHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0x1234));
            SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
            if (SymInitializeW(symHandle, nullptr, FALSE)) {
                for (const auto& m : dumpModules) {
                    (void)SymLoadModuleExW(
                        symHandle, nullptr, m.fullPath.c_str(), nullptr,
                        m.baseAddr, static_cast<DWORD>(m.size), nullptr, 0);
                }

                // Faulting-IP resolution.
                {
                    StackFrame f;
                    resolveAddress(symHandle, out.modules, exceptionAddr, f);
                    out.faultingSymbol = f.symbol;
                    out.symbolDisplacement = f.symbolDisplacement;
                    out.faultingSourceFile = f.sourceFile;
                    out.faultingSourceLine = f.sourceLine;
                }

                // Stack walk of the crashing thread. The frames are appended
                // in top-down order (innermost first, caller last).
                if (crashContext) {
                    g_reader.dumpBase = static_cast<const std::uint8_t*>(base);
                    g_reader.memList = memList;
                    g_reader.mem64List = mem64List;

                    CONTEXT ctxCopy = *crashContext;
                    STACKFRAME64 frame{};
                    frame.AddrPC.Offset = ctxCopy.Rip;
                    frame.AddrPC.Mode = AddrModeFlat;
                    frame.AddrFrame.Offset = ctxCopy.Rbp;
                    frame.AddrFrame.Mode = AddrModeFlat;
                    frame.AddrStack.Offset = ctxCopy.Rsp;
                    frame.AddrStack.Mode = AddrModeFlat;

                    const DWORD machine = IMAGE_FILE_MACHINE_AMD64;
                    HANDLE fakeThread = reinterpret_cast<HANDLE>(
                        static_cast<uintptr_t>(crashThreadId ? crashThreadId : 1));
                    for (int i = 0; i < 64; ++i) {
                        BOOL ok = StackWalk64(
                            machine, symHandle, fakeThread,
                            &frame, &ctxCopy,
                            dumpReadMemory,
                            SymFunctionTableAccess64,
                            SymGetModuleBase64,
                            nullptr);
                        if (!ok || frame.AddrPC.Offset == 0) break;
                        StackFrame sf;
                        resolveAddress(symHandle, out.modules,
                                       frame.AddrPC.Offset, sf);
                        out.stack.push_back(std::move(sf));
                    }

                    g_reader = {};
                }
                SymCleanup(symHandle);
            }
        }
    }

    UnmapViewOfFile(base);
    CloseHandle(mapping);
    CloseHandle(file);
    return out;
}

static void runDebugLoop(HANDLE hProcess, DWORD childPid, const fs::path& logDir,
                        CrashInfo& info)
{
    for (;;) {
        DEBUG_EVENT ev{};
        if (!WaitForDebugEvent(&ev, INFINITE)) {
            break;
        }
        DWORD contStatus = DBG_CONTINUE;

        if (ev.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
            const auto& er = ev.u.Exception.ExceptionRecord;
            const DWORD code = er.ExceptionCode;
            const bool firstChance = ev.u.Exception.dwFirstChance != 0;

            if (isFatalException(code) && !firstChance) {
                // Second-chance fatal — process is about to die. Capture now.
                if (!info.crashed.exchange(true)) {
                    info.exceptionCode = code;
                    info.exceptionAddress =
                        reinterpret_cast<uintptr_t>(er.ExceptionAddress);
                    info.dumpPath = logDir / "crash.dmp";
                    writeMinidump(hProcess, childPid, ev, info.dumpPath);
                }
                contStatus = DBG_EXCEPTION_NOT_HANDLED;
            } else if (isFatalException(code)) {
                // First-chance fatal — let application handlers run.
                contStatus = DBG_EXCEPTION_NOT_HANDLED;
            } else {
                // Non-fatal (breakpoint, C++ exception cookie) — pass through.
                contStatus = (code == EXCEPTION_BREAKPOINT)
                    ? DBG_CONTINUE
                    : DBG_EXCEPTION_NOT_HANDLED;
            }
        } else if (ev.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT) {
            info.exitCode = ev.u.ExitProcess.dwExitCode;
            info.processExited = true;
            ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, DBG_CONTINUE);
            break;
        }
        // Every other event (CREATE/EXIT_THREAD, LOAD/UNLOAD_DLL,
        // OUTPUT_DEBUG_STRING, RIP_EVENT, ...) is passed through unchanged.
        ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, contStatus);
    }
}

// ---------------------------------------------------------------------------
// Synthetic input via PostMessage — GLFW's Win32 window proc handles these
// exactly like real input, and PostMessage does not require the target to be
// the foreground window.
// ---------------------------------------------------------------------------

static void postMove(HWND hwnd, int x, int y, WPARAM buttons = 0)
{
    PostMessageW(hwnd, WM_MOUSEMOVE, buttons, MAKELPARAM(x, y));
}

static void postClick(HWND hwnd, int x, int y, int button)
{
    UINT down = (button == 1) ? WM_LBUTTONDOWN : WM_RBUTTONDOWN;
    UINT up   = (button == 1) ? WM_LBUTTONUP   : WM_RBUTTONUP;
    WPARAM btn = (button == 1) ? MK_LBUTTON : MK_RBUTTON;
    postMove(hwnd, x, y);
    PostMessageW(hwnd, down, btn, MAKELPARAM(x, y));
    PostMessageW(hwnd, up,   0,   MAKELPARAM(x, y));
}

static void postDrag(HWND hwnd, int x1, int y1, int x2, int y2, int steps)
{
    postMove(hwnd, x1, y1);
    PostMessageW(hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x1, y1));
    for (int i = 1; i <= steps; ++i) {
        int x = x1 + (x2 - x1) * i / steps;
        int y = y1 + (y2 - y1) * i / steps;
        postMove(hwnd, x, y, MK_LBUTTON);
    }
    PostMessageW(hwnd, WM_LBUTTONUP, 0, MAKELPARAM(x2, y2));
}

static void postWheel(HWND hwnd, int x, int y, int delta)
{
    POINT screen{ x, y };
    ClientToScreen(hwnd, &screen);
    WPARAM wp = MAKEWPARAM(0, delta);
    LPARAM lp = MAKELPARAM(screen.x, screen.y);
    PostMessageW(hwnd, WM_MOUSEWHEEL, wp, lp);
}

static void postKey(HWND hwnd, WORD vk)
{
    // GLFW reads WM_KEYDOWN/WM_KEYUP directly.
    PostMessageW(hwnd, WM_KEYDOWN, vk, 0);
    PostMessageW(hwnd, WM_KEYUP,   vk, 0);
}

// ---------------------------------------------------------------------------
// The monkey itself
// ---------------------------------------------------------------------------

static long long nowMs(std::chrono::steady_clock::time_point start)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
}

static void runMonkey(HWND hwnd, HANDLE hProcess,
                     const Options& opt, EventLog& log,
                     std::atomic<bool>& stop)
{
    std::mt19937 rng(opt.seed);
    std::uniform_int_distribution<int> kindDist(0, opt.includeKeys ? 99 : 79);
    // Distribution buckets for the normal (non-scroll) phase:
    //   0-59  click (left)
    //   60-69 click (right)
    //   70-74 wheel (small nudge, biased downward)
    //   75-79 drag
    //   80-99 key (if enabled)
    const WORD safeKeys[] = {
        VK_TAB, VK_RETURN, VK_ESCAPE, VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN,
        VK_HOME, VK_END, VK_PRIOR, VK_NEXT, VK_SPACE,
        'A', 'B', 'C', '1', '2', '3'
    };

    const auto start = std::chrono::steady_clock::now();
    const auto deadline = start + std::chrono::seconds(opt.durationSec);
    int count = 0;
    int lastProgressReport = 0;
    // Scroll-phase pacing. Every kScrollBurstEvery events we run a short burst
    // of wheel-only events to teleport the ScrollView to a different vertical
    // position. Every kResetHomeEvery events we scroll back to the top so a
    // 30-minute run keeps sampling the whole page instead of camping at the
    // bottom.
    constexpr int kScrollBurstEvery = 150;
    constexpr int kResetHomeEvery   = 1000;

    auto sendScrollBurst = [&](int direction, int steps) {
        RECT rc{};
        if (!GetClientRect(hwnd, &rc)) return;
        const int cx = (rc.right - rc.left) / 2;
        const int cy = (rc.bottom - rc.top) / 2;
        for (int i = 0; i < steps && !stop.load(); ++i) {
            postWheel(hwnd, cx, cy, direction * WHEEL_DELTA);
            MonkeyEvent ev{};
            ev.tMs = nowMs(start);
            ev.kind = "wheel";
            ev.x = cx;
            ev.y = cy;
            ev.wheelDelta = direction * WHEEL_DELTA;
            log.push(ev);
            ++count;
            std::this_thread::sleep_for(
                std::chrono::milliseconds(std::max(5, opt.intervalMs / 3)));
        }
    };

    while (!stop.load()) {
        if (opt.iterations > 0 && count >= opt.iterations) break;
        if (std::chrono::steady_clock::now() >= deadline) break;
        if (WaitForSingleObject(hProcess, 0) == WAIT_OBJECT_0) break;

        RECT client{};
        if (!GetClientRect(hwnd, &client) || !IsWindow(hwnd)) break;
        const int w = client.right - client.left;
        const int h = client.bottom - client.top;
        if (w <= 4 || h <= 4) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        std::uniform_int_distribution<int> xDist(0, w - 1);
        std::uniform_int_distribution<int> yDist(0, h - 1);

        // Periodic scroll bursts explore the whole scrollable content, then
        // let the normal random stream hit whatever is now visible.
        if (count > 0 && (count % kResetHomeEvery) == 0) {
            // Ctrl+Home is a common "go to top" shortcut, but WhatsUI's
            // ScrollView doesn't consume it. A big upward burst is reliable.
            sendScrollBurst(+1, 40); // positive wheel = up
        } else if (count > 0 && (count % kScrollBurstEvery) == 0) {
            std::uniform_int_distribution<int> stepDist(6, 20);
            const int direction = (rng() % 4 == 0) ? +1 : -1; // 25% up, 75% down
            sendScrollBurst(direction, stepDist(rng));
        }

        MonkeyEvent ev{};
        ev.tMs = nowMs(start);

        const int kind = kindDist(rng);
        if (kind < 60) {
            ev.kind = "click";
            ev.button = 1;
            ev.x = xDist(rng);
            ev.y = yDist(rng);
            postClick(hwnd, ev.x, ev.y, 1);
        } else if (kind < 70) {
            ev.kind = "click";
            ev.button = 2;
            ev.x = xDist(rng);
            ev.y = yDist(rng);
            postClick(hwnd, ev.x, ev.y, 2);
        } else if (kind < 75) {
            ev.kind = "wheel";
            ev.x = xDist(rng);
            ev.y = yDist(rng);
            // 75% chance of scroll-down for the normal stream so the target
            // gradually explores deeper sections without a scripted burst.
            const int direction = (rng() % 4 == 0) ? +1 : -1;
            ev.wheelDelta = direction * WHEEL_DELTA;
            postWheel(hwnd, ev.x, ev.y, ev.wheelDelta);
        } else if (kind < 80) {
            ev.kind = "drag";
            ev.x = xDist(rng);
            ev.y = yDist(rng);
            ev.x2 = xDist(rng);
            ev.y2 = yDist(rng);
            postDrag(hwnd, ev.x, ev.y, ev.x2, ev.y2, 6);
        } else {
            ev.kind = "key";
            std::uniform_int_distribution<int> keyDist(
                0, (int)(sizeof(safeKeys) / sizeof(safeKeys[0])) - 1);
            ev.key = safeKeys[keyDist(rng)];
            postKey(hwnd, static_cast<WORD>(ev.key));
        }
        log.push(ev);
        ++count;

        // Heartbeat so long runs are visibly alive without spamming stderr.
        if (count - lastProgressReport >= 500) {
            const long long elapsedSec = nowMs(start) / 1000;
            std::fprintf(stderr, "[monkey] progress: %d events, %llds elapsed\n",
                         count, elapsedSec);
            lastProgressReport = count;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(opt.intervalMs));
    }
    std::fprintf(stderr, "[monkey] sent %d events\n", count);
}

// ---------------------------------------------------------------------------
// Report
// ---------------------------------------------------------------------------

static std::string toIso(std::chrono::system_clock::time_point tp)
{
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_buf{};
    localtime_s(&tm_buf, &tt);
    std::ostringstream os;
    os << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    return os.str();
}

static void writeReport(const fs::path& reportPath, bool crashed,
                       const Options& opt, const CrashInfo& info,
                       const std::vector<MonkeyEvent>& tail)
{
    std::ofstream f(reportPath);
    f << "WhatsUI Monkey Runner report\n";
    f << "generated: " << toIso(std::chrono::system_clock::now()) << "\n";
    f << "seed: " << opt.seed << "\n";
    f << "target: ";
    std::wstring targetU8Path = opt.targetExe;
    f << std::string(targetU8Path.begin(), targetU8Path.end()) << "\n";
    f << "outcome: " << (crashed ? "CRASH" : "CLEAN") << "\n";

    if (crashed) {
        DWORD code = info.exceptionCode.load();
        f << "exception: 0x" << std::hex << std::setw(8) << std::setfill('0')
          << code << std::dec << " (" << exceptionName(code) << ")\n";
        f << "address: 0x" << std::hex << info.exceptionAddress.load()
          << std::dec << "\n";
        if (!info.dumpPath.empty()) {
            std::string dumpStr;
            for (auto c : info.dumpPath.wstring()) dumpStr.push_back((char)c);
            f << "minidump: " << dumpStr << "\n";

            // Best-effort automatic analysis of the just-written dump.
            if (auto s = summariseDump(info.dumpPath)) {
                f << "\n-- Automatic minidump analysis --\n";
                f << "faulting_module: " << s->faultingModule << "\n";
                if (s->moduleBase != 0) {
                    f << "module_base: 0x" << std::hex << s->moduleBase
                      << std::dec << "\n";
                    f << "module_size: 0x" << std::hex << s->moduleSize
                      << std::dec << "\n";
                    f << "offset_in_module: 0x" << std::hex
                      << s->offsetInModule << std::dec << "\n";
                }
                if (s->haveViolation) {
                    f << "violation: " << s->violationDirection
                      << " at 0x" << std::hex << s->violationAddress
                      << std::dec << "\n";
                }
                if (!s->faultingSymbol.empty()) {
                    f << "symbol: " << s->faultingSymbol
                      << " + 0x" << std::hex << s->symbolDisplacement
                      << std::dec << "\n";
                }
                if (!s->faultingSourceFile.empty()) {
                    f << "source: " << s->faultingSourceFile
                      << ":" << s->faultingSourceLine << "\n";
                }
                if (!s->stack.empty()) {
                    f << "\nStack trace (innermost first):\n";
                    int idx = 0;
                    for (const auto& fr : s->stack) {
                        f << "  #" << idx++ << "  0x" << std::hex
                          << std::setw(16) << std::setfill('0') << fr.address
                          << std::dec << std::setfill(' ') << "  ";
                        if (!fr.module.empty()) f << fr.module;
                        else                    f << "?";
                        f << "!";
                        if (!fr.symbol.empty()) {
                            f << fr.symbol;
                            if (fr.symbolDisplacement)
                                f << "+0x" << std::hex << fr.symbolDisplacement
                                  << std::dec;
                        } else {
                            f << "?";
                        }
                        if (!fr.sourceFile.empty()) {
                            f << "  (" << fr.sourceFile << ":"
                              << fr.sourceLine << ")";
                        }
                        f << "\n";
                    }
                }
                f << "\nLoaded modules (name -> base) at crash time:\n";
                for (const auto& [name, mbase] : s->modules) {
                    f << "  0x" << std::hex << std::setw(16) << std::setfill('0')
                      << mbase << std::dec << std::setfill(' ')
                      << "  " << name << "\n";
                }
            }
        }
    }
    if (info.processExited.load()) {
        f << "exit_code: 0x" << std::hex << info.exitCode.load() << std::dec << "\n";
    }
    f << "\nLast " << tail.size() << " events (most recent last):\n";
    for (const auto& e : tail) {
        f << "  t=" << e.tMs << "ms  " << e.kind;
        if (std::string(e.kind) == "click") {
            f << " btn=" << e.button << " @(" << e.x << "," << e.y << ")";
        } else if (std::string(e.kind) == "drag") {
            f << " (" << e.x << "," << e.y << ") -> (" << e.x2 << "," << e.y2 << ")";
        } else if (std::string(e.kind) == "wheel") {
            f << " delta=" << e.wheelDelta << " @(" << e.x << "," << e.y << ")";
        } else if (std::string(e.kind) == "key") {
            f << " vk=0x" << std::hex << e.key << std::dec;
        }
        f << "\n";
    }
    if (crashed) {
        f << "\nReplay: re-run with the same --seed to reproduce the same "
             "event stream. events.jsonl in this folder contains every event "
             "with its client-relative coordinate.\n";
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // Standalone dump analysis mode:
    //   WhatsUIMonkeyRunner --analyze <path/to/crash.dmp>
    if (argc == 3 && std::string(argv[1]) == "--analyze") {
        fs::path dumpPath = fs::path(utf8ToWide(argv[2]));
        auto summary = summariseDump(dumpPath);
        if (!summary) {
            std::fprintf(stderr, "Could not read dump: %ls\n",
                         dumpPath.wstring().c_str());
            return 1;
        }
        std::printf("faulting_module: %s\n", summary->faultingModule.c_str());
        if (summary->moduleBase) {
            std::printf("module_base: 0x%llx\n", (unsigned long long)summary->moduleBase);
            std::printf("offset_in_module: 0x%llx\n",
                        (unsigned long long)summary->offsetInModule);
        }
        if (summary->haveViolation) {
            std::printf("violation: %s at 0x%llx\n",
                        summary->violationDirection.c_str(),
                        (unsigned long long)summary->violationAddress);
        }
        if (!summary->faultingSymbol.empty()) {
            std::printf("symbol: %s + 0x%llx\n",
                        summary->faultingSymbol.c_str(),
                        (unsigned long long)summary->symbolDisplacement);
        }
        if (!summary->faultingSourceFile.empty()) {
            std::printf("source: %s:%u\n",
                        summary->faultingSourceFile.c_str(),
                        summary->faultingSourceLine);
        }
        if (!summary->stack.empty()) {
            std::printf("\nStack trace (innermost first):\n");
            int idx = 0;
            for (const auto& fr : summary->stack) {
                std::printf("  #%d  0x%016llx  %s!%s",
                            idx++,
                            (unsigned long long)fr.address,
                            fr.module.empty() ? "?" : fr.module.c_str(),
                            fr.symbol.empty() ? "?" : fr.symbol.c_str());
                if (fr.symbolDisplacement)
                    std::printf("+0x%llx", (unsigned long long)fr.symbolDisplacement);
                if (!fr.sourceFile.empty())
                    std::printf("  (%s:%u)", fr.sourceFile.c_str(), fr.sourceLine);
                std::printf("\n");
            }
        }
        std::printf("modules loaded: %zu\n", summary->modules.size());
        for (const auto& [name, mbase] : summary->modules) {
            std::printf("  0x%016llx  %s\n",
                        (unsigned long long)mbase, name.c_str());
        }
        return 0;
    }

    Options opt;
    if (!parseOptions(argc, argv, opt)) return 1;

    if (!fs::exists(opt.targetExe)) {
        std::fprintf(stderr, "Target not found: %ls\n", opt.targetExe.c_str());
        return 1;
    }

    // One run directory per invocation. Keeps all artifacts co-located.
    auto tp = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tmBuf{};
    localtime_s(&tmBuf, &tt);
    std::ostringstream stamp;
    stamp << std::put_time(&tmBuf, "run_%Y%m%d_%H%M%S");
    fs::create_directories(opt.logDir);
    fs::path runDir = opt.logDir / stamp.str();
    fs::create_directories(runDir);
    std::fprintf(stderr, "[monkey] artifacts: %ls\n", runDir.wstring().c_str());
    std::fprintf(stderr, "[monkey] seed: %u\n", opt.seed);

    // Redirect child's stdout / stderr to files so any warnings survive a crash.
    fs::path stdoutPath = runDir / "child_stdout.log";
    fs::path stderrPath = runDir / "child_stderr.log";
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
    HANDLE hStdout = CreateFileW(stdoutPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                 &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE hStderr = CreateFileW(stderrPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                                 &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    // Build the child command line: "target.exe" arg1 arg2 ...
    std::wstring cmd = L"\"" + opt.targetExe + L"\"";
    for (const auto& a : opt.targetArgs) cmd += L" " + a;

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = hStdout;
    si.hStdError = hStderr;
    PROCESS_INFORMATION pi{};

    DWORD creationFlags = 0;
    if (opt.attachDebugger) creationFlags |= DEBUG_ONLY_THIS_PROCESS;

    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    if (!CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
                        creationFlags, nullptr, nullptr, &si, &pi)) {
        std::fprintf(stderr, "CreateProcess failed: %lu\n", GetLastError());
        return 3;
    }
    if (hStdout != INVALID_HANDLE_VALUE) CloseHandle(hStdout);
    if (hStderr != INVALID_HANDLE_VALUE) CloseHandle(hStderr);

    CrashInfo info;
    EventLog log(runDir / "events.jsonl", static_cast<std::size_t>(opt.maxLoggedEvents));
    std::atomic<bool> monkeyStop{false};
    HWND hwnd = nullptr;

    if (opt.attachDebugger) {
        // Critical Win32 constraint: WaitForDebugEvent only works on the
        // thread that started the debug session (the thread that called
        // CreateProcess with DEBUG_ONLY_THIS_PROCESS). So the debug loop
        // stays here on the main thread and the monkey runs on a worker.
        std::thread monkeyDriver([&] {
            HWND h = waitForTargetWindow(pi.dwProcessId, opt.waitStartupMs);
            if (h == nullptr) {
                std::fprintf(stderr,
                             "[monkey] target window did not appear within %d ms\n",
                             opt.waitStartupMs);
                TerminateProcess(pi.hProcess, 3);
                return;
            }
            hwnd = h;
            std::fprintf(stderr, "[monkey] target hwnd=0x%p pid=%lu\n",
                         (void*)h, pi.dwProcessId);
            runMonkey(h, pi.hProcess, opt, log, monkeyStop);
            // Give the target a moment to process any queued messages so a
            // delayed crash still lands inside our debug loop.
            WaitForSingleObject(pi.hProcess, 1500);
            if (!info.crashed.load()) {
                PostMessageW(h, WM_CLOSE, 0, 0);
            }
        });

        runDebugLoop(pi.hProcess, pi.dwProcessId, runDir, info);
        monkeyStop = true;
        if (monkeyDriver.joinable()) monkeyDriver.join();
    } else {
        // No debugger: window discovery and event dispatch stay on this
        // thread. Crash detection falls back to inspecting the exit code.
        hwnd = waitForTargetWindow(pi.dwProcessId, opt.waitStartupMs);
        if (hwnd == nullptr) {
            std::fprintf(stderr,
                         "[monkey] target window did not appear within %d ms\n",
                         opt.waitStartupMs);
            TerminateProcess(pi.hProcess, 3);
            WaitForSingleObject(pi.hProcess, 2000);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return 3;
        }
        std::fprintf(stderr, "[monkey] target hwnd=0x%p pid=%lu\n",
                     (void*)hwnd, pi.dwProcessId);
        runMonkey(hwnd, pi.hProcess, opt, log, monkeyStop);
        WaitForSingleObject(pi.hProcess, 1500);
        if (!info.processExited.load()) {
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
            if (WaitForSingleObject(pi.hProcess, 3000) != WAIT_OBJECT_0) {
                TerminateProcess(pi.hProcess, 0);
                WaitForSingleObject(pi.hProcess, 1000);
            }
        }
    }

    bool clean = !info.crashed.load();

    // If we were not attached as a debugger, infer crash from exit code.
    if (!opt.attachDebugger) {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
            info.exitCode = exitCode;
            info.processExited = true;
            if (exitCode >= 0x80000000u) {
                info.crashed = true;
                info.exceptionCode = exitCode;
                clean = false;
            }
        }
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    fs::path reportPath = runDir /
        (clean ? "run_report.txt" : "crash_report.txt");
    writeReport(reportPath, !clean, opt, info, log.tail());
    std::fprintf(stderr, "[monkey] report: %ls\n", reportPath.wstring().c_str());

    return clean ? 0 : 2;
}
