# XYO Win Service — Documentation

`xyo-win-service` is a small C++ library that turns an XYO console
application into a **Windows service**. You derive one class from
`XYO::WinService::Service`, give it a name, put the work in
`serviceWork()`, and the same executable can then:

- **install** itself in the Service Control Manager (SCM), auto start,
- **start** / **stop** the installed service and wait for the result,
- **uninstall** itself,
- **run in the console** for development and debugging (`--run`), with the
  exact same startup sequence as the real service,
- **be the service** when the SCM launches it (`--service`).

```
my-service.exe --install --start     # register and start (administrator)
my-service.exe --stop                # stop and wait until stopped
my-service.exe --run                 # same code, in the console, no SCM
my-service.exe --uninstall           # stop and remove
```

It sits on top of `xyo-win` (Windows helpers) and so of the whole XYO
stack:

```
your service application
xyo-win-service       <-- this library
xyo-win               (Windows: COM, registry, windows, shell)
xyo-pixel32, xyo-cryptography
xyo-system            (IApplication, Shell, File, Console)
xyo-encoding, xyo-multithreading (Thread, Semaphore)
xyo-data-structures, xyo-managed-memory, xyo-platform
```

## Why it exists

The Win32 service API is verbose and easy to get subtly wrong: a service
must register a control handler from the right thread, report states in
the right order with growing check points, never undo a stop, report
`SERVICE_STOPPED` last, and the install / start / stop tools must wait
for pending states without hanging forever. Every XYO program that needs
to run in the background repeated this code. `xyo-win-service` does it
once:

| Problem | What `xyo-win-service` does |
|---------|-----------------------------|
| A service needs `main` → `StartServiceCtrlDispatcher` → `ServiceMain` → control handler → worker | `Service::main(cmdN, cmdS)` handles all of it; you write `serviceWork()` |
| Debugging a real service is painful | `--run` runs the same `serviceInit` / `serviceWork` sequence on a thread in the console |
| A slow start makes the SCM give up (error 1053) | the start is reported pending with a 30 s wait hint; `reportPending()` extends it during a long `serviceInit()` |
| The service fails to start, the operator only sees "error 1066" | the error code returned by `serviceInit()` is reported to the SCM and returned by `--start` |
| An exception escapes the worker and kills the process silently | exceptions from `serviceInit()` / `serviceWork()` are caught and reported as `ERROR_EXCEPTION_IN_SERVICE` (1064) |
| Stop / shutdown / pre-shutdown arrive on another thread | the handler sets `SERVICE_STOP_PENDING` and signals `serviceStopEvent`; the state machine never goes back from stopping |
| `--start` / `--stop` return before the service is really running / stopped | they wait on the check point / wait hint protocol and fail with `ERROR_SERVICE_REQUEST_TIMEOUT` if the service stops making progress |
| Reinstalling a new build loses the account, description, recovery options | `--install` on an existing service stops it and updates only the path and start type |
| Paths with spaces in the service command line | the executable path is quoted |
| COM in a service | COM (multithreaded) and process security are initialized on the worker thread by default; override to change or disable |

## Concepts at a glance

| Need | Use | Notes |
|------|-----|-------|
| A service | `class MyService : public virtual Service` | one per process, ends with `XYO_APPLICATION_MAIN(MyService);` |
| Its name | `serviceName = "MyService";` in the constructor | SCM key name and display name |
| The work | override `void serviceWork()` | must return soon after `serviceStopEvent` is set |
| Wait for stop | `serviceStopEvent.wait()` / `.waitFor(ms)` / `.peek()` | `XYO::Multithreading::Semaphore` |
| Setup that may fail | override `DWORD serviceInit()` | return `NO_ERROR` or a Win32 error code; the service is reported running only after it succeeds |
| Long setup | call `Service::reportPending()` from `serviceInit()` | each call gives 30 s more |
| No COM / other COM model | override `serviceCOMInitialize()` **and** `serviceCOMUninitialize()` | |
| Stop a `--run` session | `stopRun = myStopFunction;` | polled every 100 ms, e.g. "key `q` pressed" |
| Own command line options | override `main`, call `Service::main(cmdN, cmdS)` | unknown `--options` are ignored by the base |
| Install / start / stop from code | `Service::cmdInstall()` ... `cmdStop()` | return `NO_ERROR` or a Win32 error code |

## Contents

| Document | What it covers |
|----------|----------------|
| [Getting started](getting-started.md) | Build it, depend on it, include and namespaces, the first service, try it in the console, install it, building without fabricare |
| [Writing a service](writing-a-service.md) | Life cycle, every hook, stopping, long start, errors and exit codes, COM, threads, patterns (periodic job, server, own options) |
| [Command line and deployment](command-line.md) | `--install` / `--uninstall` / `--start` / `--stop` / `--run` / `--service` in detail, rights, what is registered, the service environment (account, working folder, DLLs, output), `sc.exe` recipes, troubleshooting |
| [API reference](reference.md) | Every public symbol on one page |

The types used here come from the layers below: `IApplication`,
`XYO_APPLICATION_MAIN`, `Console`, `Shell` from `xyo-system`; `Thread`,
`Semaphore` from `xyo-multithreading`; the Windows helpers from
`xyo-win`. Each repository has its own `docs/`.

## Source map

```
source/XYO/WinService.hpp              umbrella header, include this
source/XYO/WinService.Amalgam.cpp      every .cpp in one unit (build without fabricare)
source/XYO/WinService/
    Dependency.hpp                     xyo-win, export macro, namespaces
    Service.hpp / Service.cpp          the Service class: commands, SCM glue, state machine
    Copyright / License / Version      library metadata
    Library.rc / *.rh                  Windows DLL version resource
test/test.01.cpp                       interactive sample: counts in the console, 'q' stops it
test/test.02.cpp                       console mode tests: worker sequence, exit codes, stop, options (no rights needed)
test/test.03.cpp                       SCM tests: install, start, stop, reinstall, uninstall (administrator, skipped otherwise)
```

## AI assistant skill

A Claude Code skill describing how to use this library lives in
[`.claude/skills/xyo-win-service/`](../.claude/skills/xyo-win-service/SKILL.md).
It is picked up automatically inside this repository; copy the folder to
`~/.claude/skills/` to have it available in the projects that depend on
`xyo-win-service`.
