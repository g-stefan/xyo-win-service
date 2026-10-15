---
name: xyo-win-service
description: >-
  How to use the xyo-win-service C++ library (namespace XYO::WinService),
  the Windows service layer of the XYO C++ stack on top of xyo-win: derive
  from Service (an IApplication, one per process, XYO_APPLICATION_MAIN),
  set serviceName / stopRun in the constructor, override serviceWork,
  serviceInit, serviceCOMInitialize / serviceCOMUninitialize, main; the
  stop signal serviceStopEvent (Semaphore wait / waitFor / peek);
  reportPending / reportStatus; the command line --install / --uninstall /
  --start / --stop / --run / --service and cmdInstall / cmdUninstall /
  cmdStart / cmdStop / cmdRun / cmdService; exit codes (1053, 1060, 1063,
  1064, 1066), the service environment (LocalSystem, System32 working
  folder, no console, DLLs next to the executable), sc.exe settings and
  troubleshooting. Use when writing, reviewing, installing or debugging a
  program that includes <XYO/WinService.hpp>, depends on "xyo-win-service"
  in fabricare.json, derives from XYO::WinService::Service, or when
  working inside the xyo-win-service repository.
---

# xyo-win-service

Windows service layer of the XYO C++ libraries, on top of `xyo-win` (see
the `xyo-system`, `xyo-multithreading` and `xyo-managed-memory` skills;
their rules apply — `IApplication`, `Thread`, `Semaphore`, `TPointer`).
Purpose: **one executable that is both the service and its own
installer / controller**, with a console mode (`--run`) that runs the
exact same code for development. Windows only (MSVC).

Full documentation: `docs/` in the xyo-win-service repository
(`X:\Storage\XYO\Gitea\CPP\xyo-win-service\docs` on this machine):
README, getting-started, **writing-a-service**, **command-line**,
reference. Read the matching page when you need more than this summary.
The whole implementation is `source/XYO/WinService/Service.cpp`;
`test/test.02.cpp` (console mode) and `test/test.03.cpp` (real SCM) show
every behavior.

## Pick a tool

| Need | Use |
|------|-----|
| A service | `class MyService : public virtual Service` + `XYO_APPLICATION_MAIN(MyService);` |
| Name it | `serviceName = "MyCompany-MyService";` in the constructor |
| The work | override `void serviceWork()` |
| Block until stop | `serviceStopEvent.wait();` |
| Periodic job | `do { job(); } while (!serviceStopEvent.waitFor(ms));` |
| Check in a loop | `while (!serviceStopEvent.peek()) { step(); }` |
| Setup that can fail | override `DWORD serviceInit()`, return `NO_ERROR` or `ERROR_*` |
| Setup longer than 30 s | call `reportPending()` at least every 30 s inside `serviceInit()` |
| Long cleanup after stop | call `reportPending()` during it |
| No COM | override `serviceCOMInitialize()` → `S_OK` **and** `serviceCOMUninitialize()` → nothing |
| STA COM | `serviceCOMInitialize()` → `CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)` |
| Stop `--run` with a key | `stopRun = f;` with `f` = `Console::keyHit() && Console::getChar() == 'q'` |
| Own options / usage | override `main`, handle yours, then `return Service::main(cmdN, cmdS);` |
| Install + start | `my.exe --install --start` (elevated) |
| Update after rebuild | `my.exe --install --start` (stops, updates path, keeps admin settings) |
| Remove | `my.exe --uninstall` |
| Debug | `my.exe --run` (no rights, console, same sequence) |
| From code | `Service::cmdInstall()` / `cmdStart()` / `cmdStop()` / `cmdUninstall()` → `DWORD` |

## Hard rules

1. **Include and namespaces**: `#include <XYO/WinService.hpp>`,
   `using namespace XYO::WinService;` (re-exports `XYO::System` and
   `XYO::Win`). Then `Shell::` and `Registry::` are ambiguous: write
   `XYO::System::Shell::...` / `XYO::Win::Registry::...`. Do not name your
   class `Service` or `Application` at global scope. Metadata in full:
   `XYO::WinService::Version::version()` (the short `Version::` is
   ambiguous with `XYO::System::Version` / `XYO::Win::Version`).
2. **One service per process.** `serviceName`, `stopRun`,
   `serviceStopEvent`, `status`, `serviceThis` are static. Set
   `serviceName` (a string literal — the pointer is kept) and `stopRun` in
   the **derived** constructor; the base constructor sets
   `"XYO-WinService"` and a never-stop function first. `serviceName` is
   the SCM key and display name (no `/` `\`, ≤ 256 chars).
3. **Sequence** (worker thread, same in `--run` and as service):
   `serviceCOMInitialize()` → `serviceInit()` → report running →
   `serviceWork()` → `serviceCOMUninitialize()` (only if COM init
   succeeded). `serviceInit()` error → `serviceWork()` skipped, exit code
   = that error. Exceptions in `serviceInit()` / `serviceWork()` are caught
   → `ERROR_EXCEPTION_IN_SERVICE` (1064). COM init failure →
   `ERROR_SERVICE_SPECIFIC_ERROR` with the `HRESULT` as specific code.
4. **`serviceWork()` is the service**: when it returns the service stops
   (`NO_ERROR`). It must return soon after `serviceStopEvent` is signaled
   (stop, shutdown — Windows gives only a few seconds at shutdown).
   Blocking calls must be interruptible (close the socket / handle on
   stop, or poll `peek()`). Join every thread you started before
   returning: after `SERVICE_STOPPED` the process can be killed.
5. **`serviceStopEvent`** is a binary `Semaphore`: `wait()` / `waitFor()`
   consume the signal (wait from the worker thread only); `peek()` does
   not (use it from other threads and inner loops). It is reset at the
   start of each run.
6. **Start / stop timing**: start and stop pending are reported with a
   30 s wait hint. Work longer than that in `serviceInit()` or after the
   stop needs `reportPending()` (≥ every 30 s) or `--start` / `--stop` /
   the SCM fail with 1053. `reportPending()` is a no-op outside pending
   states and in `--run` — always safe. Don't call `reportStatus()`
   yourself unless you really manage states (`SERVICE_STOPPED` is final,
   stop pending can't go back to running).
7. **Errors from `serviceInit()`** are plain Win32 codes (`ERROR_*`), not
   `ERROR_SERVICE_SPECIFIC_ERROR`. They reach `--start`, the SCM and the
   System event log (7023).
8. **COM hooks come in pairs**: overriding only `serviceCOMInitialize()`
   to skip COM leaves the default `CoUninitialize()` unbalanced. COM is
   initialized only on the worker thread; your threads initialize their
   own.
9. **Command line**: `Service::main` processes `--install`,
   `--uninstall`, `--start`, `--stop` in order, stops at the first error
   (prints `"<name>: --<opt> failed, error <n>"` on stderr, returns it);
   `--run` / `--service` return immediately; other arguments are ignored;
   no command → returns `1` **silently** — print usage in your `main`.
   The SCM runs `"<exe>" --service` only: options given at install time
   are not passed; read configuration from a file / `HKLM`.
10. **Install semantics**: `--install` = own process, auto start, normal
    error control, `LocalSystem`, command line with the quoted full path;
    does not start. On an existing service it stops it and updates path,
    type, start type (back to auto), error control only — account,
    description, dependencies, recovery, delayed start are kept. Install
    from the final folder, not a build folder. `--uninstall` on a missing
    service and `--start` on a running one / `--stop` on a stopped one
    succeed. Install / uninstall / start / stop need an elevated prompt
    (error 5 otherwise); `--run` needs nothing.
11. **Service environment**: working folder `C:\Windows\System32` (use
    absolute paths from `XYO::System::Shell::getExecutablePath()` — the
    folder — or `GetModuleFileName`); no console (`printf` is lost; log to
    a file / event log); session 0, no UI; system `PATH` only — put the XYO
    DLLs next to the executable or link statically (a missing DLL shows
    up as 1053 with nothing logged); no mapped drives (UNC); `HKCU` is the
    service account's.
12. **Threads**: worker thread and `serviceMain` thread are registered
    with the XYO memory registry; start your own threads with
    `XYO::Multithreading::Thread` / `Worker` so they are too. `stopRun`
    runs on the main thread (`--run`), the control handler on the SCM
    dispatcher thread.

## Recipes

Minimal service with usage, key stop and periodic work:

```cpp
#include <XYO/WinService.hpp>

using namespace XYO::WinService;

class MyService : public virtual Service {
		XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(MyService);

	public:
		MyService();
		DWORD serviceInit();
		void serviceWork();
		int main(int cmdN, char *cmdS[]);
		static bool stopOnKeyQ();
};

MyService::MyService() {
	serviceName = "MyCompany-MyService";
	stopRun = stopOnKeyQ;
};

DWORD MyService::serviceInit() {
	// load configuration (absolute paths), open resources
	return NO_ERROR; // or ERROR_* to fail the start
};

void MyService::serviceWork() {
	do {
		// one unit of work
	} while (!serviceStopEvent.waitFor(60 * 1000));
};

bool MyService::stopOnKeyQ() {
	return Console::keyHit() && (Console::getChar() == 'q');
};

int MyService::main(int cmdN, char *cmdS[]) {
	if (cmdN < 2) {
		printf("usage: %s --install | --uninstall | --start | --stop | --run\n", cmdS[0]);
		return 1;
	};
	return Service::main(cmdN, cmdS);
};

XYO_APPLICATION_MAIN(MyService);
```

Server with its own threads:

```cpp
void MyService::serviceWork() {
	server.start();          // XYO Thread(s)
	serviceStopEvent.wait();
	server.stop();           // signal + join; reportPending() if it can take > 30 s
};
```

No COM:

```cpp
HRESULT MyService::serviceCOMInitialize() {
	return S_OK;
};
void MyService::serviceCOMUninitialize() {
};
```

Deployment (elevated terminal; `sc` needs the space after `=`):

```bash
my-service --install --start
sc description MyCompany-MyService "What it does"
sc failure MyCompany-MyService reset= 86400 actions= restart/5000/restart/5000/restart/60000
sc failureflag MyCompany-MyService 1
```

## Error codes to recognize

`5` not elevated · `2`/`3` executable moved, reinstall · `1053` timeout
(long `serviceInit` without `reportPending`, `serviceWork` ignores stop,
missing DLL) · `1058` disabled · `1060` not installed / wrong name ·
`1063` `--service` from a console (use `--run`) · `1064` exception in the
service · `1066` / large `0x8...` value: COM init `HRESULT` · `1069` bad
account password · `1072` marked for delete (close `services.msc`).
`net helpmsg <n>` shows the text.

## Build and dependency

fabricare project, `make: dll-or-lib`, dependency `xyo-win`. Consumers add
`"xyo-win-service"` to `dependency` in `fabricare.json` (`make: exe`).
Tests: `test.01` (interactive sample, `q` stops), `test.02` (console mode,
no rights), `test.03` (real SCM install / start / stop / reinstall /
uninstall of `XYO-WinService-Test03`; elevated prompt, otherwise
"skipped"). See the `fabricare` skill for build commands.

When changing the library: keep the state machine rules in `setStatus_`
(stopped final, no return from stop pending, check point only while
pending), the waiting rule in `waitForState_`, `--install` updating in
place; add a check to `test/test.02.cpp` (no rights) or
`test/test.03.cpp` (SCM); update `docs/`.
