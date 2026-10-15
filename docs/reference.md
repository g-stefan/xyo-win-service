# API reference

Header: `#include <XYO/WinService.hpp>` — namespace `XYO::WinService`
(contains `using namespace XYO::System; using namespace XYO::Win;`).
Windows only.

## Types

| Symbol | Description |
|--------|-------------|
| `Semaphore` | `typedef XYO::Multithreading::Semaphore` — the type of `serviceStopEvent` |
| `Service` | the service application base class, derives from `IApplication` (virtual), not copyable |
| `Service::StopRun` | `typedef bool (*StopRun)()` — console stop check for `--run` |

## Service — static state

One service per process: these are shared by the whole program.

| Member | Description |
|--------|-------------|
| `static const char *serviceName` | SCM name and display name; default `"XYO-WinService"`, set it in the derived constructor (pointer kept, not copied) |
| `static StopRun stopRun` | polled every 100 ms by `--run`, `true` stops the run; default never stops; `nullptr` allowed |
| `static Semaphore serviceStopEvent` | signaled when the service must stop (SCM stop / shutdown, or `stopRun` in `--run`); reset when a run / service starts |
| `static SERVICE_STATUS status` | last status reported to the SCM; all zero when not running as a service |
| `static SERVICE_STATUS_HANDLE statusHandle` | SCM status handle, `NULL` when not running as a service |
| `static Service *serviceThis` | the application object (set by the constructor) |

## Service — virtual hooks (override)

| Member | Default | Description |
|--------|---------|-------------|
| `virtual void serviceWork()` | returns at once | the service; runs while the service is running; must return soon after `serviceStopEvent` is signaled |
| `virtual DWORD serviceInit()` | `NO_ERROR` | runs before the service is reported running; return a Win32 error code to fail the start (`serviceWork()` is then not called) |
| `virtual HRESULT serviceCOMInitialize()` | `CoInitializeEx(MULTITHREADED)` + `CoInitializeSecurity` (default authentication, impersonate; `RPC_E_TOO_LATE` accepted) | runs first on the worker thread; a failure ends the service with `ERROR_SERVICE_SPECIFIC_ERROR` / the `HRESULT` |
| `virtual void serviceCOMUninitialize()` | `CoUninitialize()` | runs last, only if `serviceCOMInitialize()` succeeded; override together with it |
| `int main(int cmdN, char *cmdS[])` | see below | `IApplication::main`; override to add options or usage, then call `Service::main` |

## Service — commands

All static. `DWORD` results are `NO_ERROR` or a Win32 error code.

| Member | Description |
|--------|-------------|
| `int main(int cmdN, char *cmdS[])` | processes `--install`, `--uninstall`, `--start`, `--stop` in order (stops at the first error, prints `"<serviceName>: --<option> failed, error <n>"` on `stderr`, returns it); `--run` / `--service` return their result at once; other arguments are ignored; no command → `1` |
| `static DWORD cmdInstall()` | create the service (`"<exe>" --service`, own process, auto start, normal error control, `LocalSystem`); if it exists, stop it and update command line / type / start / error control only |
| `static DWORD cmdUninstall()` | stop and delete; not installed or already marked for delete → `NO_ERROR` |
| `static DWORD cmdStart()` | start and wait for `SERVICE_RUNNING`; already running → `NO_ERROR`; stopped during start → its exit code (service-specific code when set); no progress → `ERROR_SERVICE_REQUEST_TIMEOUT` |
| `static DWORD cmdStop()` | stop and wait for `SERVICE_STOPPED`; not running → `NO_ERROR`; already stopping → waits |
| `static int cmdRun()` | run the worker sequence on a thread in the console until it ends or `stopRun()` returns `true`; returns the exit code (the service-specific code instead of `ERROR_SERVICE_SPECIFIC_ERROR`); `ERROR_SERVICE_NO_THREAD` if the thread can not start |
| `static int cmdService()` | `StartServiceCtrlDispatcher` with `serviceMain`; returns `0` after the service stopped, or the error (`1063` outside the SCM) |

## Service — status

| Member | Description |
|--------|-------------|
| `static void reportStatus(DWORD currentState, DWORD win32ExitCode, DWORD serviceSpecificExitCode, DWORD waitHint)` | report a state to the SCM (thread safe). No-op when not running as a service. `SERVICE_STOPPED` is final; from `SERVICE_STOP_PENDING` only `SERVICE_STOPPED` is accepted. Accepts stop + shutdown only while `SERVICE_RUNNING`; the check point grows while pending, `0` otherwise. Normally called by the library only |
| `static void reportPending()` | progress during `SERVICE_START_PENDING` / `SERVICE_STOP_PENDING`: next check point, wait hint 30 s. No-op in any other state and in `--run` |

## Service — internals (public for the SCM callbacks)

| Member | Description |
|--------|-------------|
| `Service()` | sets `serviceThis`, default `stopRun`, calls `init()` |
| `static void init()` | resets `status`, `statusHandle`, sets `serviceName = "XYO-WinService"` |
| `static VOID WINAPI serviceMain(DWORD argc, LPSTR *argv)` | `ServiceMain`: registers the thread with the XYO memory registry and the control handler, reports start pending, runs `worker`, reports stopped |
| `static DWORD WINAPI serviceCtrlHandler(DWORD control, DWORD eventType, LPVOID eventData, LPVOID context)` | `HandlerEx`: stop / shutdown / pre-shutdown while running → stop pending + `serviceStopEvent.notify()`; interrogate → `NO_ERROR`; others → `ERROR_CALL_NOT_IMPLEMENTED` |
| `static DWORD worker(DWORD &serviceSpecificExitCode)` | the worker sequence: COM initialize, `serviceInit()`, report running, `serviceWork()`, COM uninitialize; exceptions → `ERROR_EXCEPTION_IN_SERVICE` |
| `static void workerThread(void *)` | thread procedure of `cmdRun` |

## Exit codes

| Code | Name | When |
|------|------|------|
| `0` | `NO_ERROR` | success; `serviceWork()` returned |
| any | — | returned by `serviceInit()` |
| `1053` | `ERROR_SERVICE_REQUEST_TIMEOUT` | `--start` / `--stop`: no progress within the wait hint |
| `1054` | `ERROR_SERVICE_NO_THREAD` | `--run`: worker thread not started |
| `1063` | `ERROR_FAILED_SERVICE_CONTROLLER_CONNECT` | `--service` outside the SCM |
| `1064` | `ERROR_EXCEPTION_IN_SERVICE` | exception in `serviceInit()` / `serviceWork()` |
| `1066` | `ERROR_SERVICE_SPECIFIC_ERROR` | reported to the SCM when COM initialization failed (the `HRESULT` is the service-specific code; `--start` and `--run` return the `HRESULT`) |
| `1` | — | `main` with no command |

## Timing constants

| Value | Use |
|-------|-----|
| 30 000 ms | wait hint reported while start / stop is pending; `--start` / `--stop` give up after this long without check point progress |
| 100 ms | `stopRun` polling period in `--run` |
| 100 – 1000 ms | `--start` / `--stop` status polling (a tenth of the wait hint) |

## Metadata

Included by `<XYO/WinService.hpp>`. Write the names in full: the
`Version` / `Copyright` / `License` namespaces exist in every XYO layer.

| Symbol | Description |
|--------|-------------|
| `XYO::WinService::Version::version()`, `build()`, `versionWithBuild()`, `datetime()` | library version (`const char *`) |
| `XYO::WinService::Copyright::copyright()`, `publisher()`, `company()`, `contact()` | copyright strings |
| `XYO::WinService::License::license()`, `shortLicense()` | license text (`std::string`) |

## Macros

| Macro | Description |
|-------|-------------|
| `XYO_WINSERVICE_EXPORT` | DLL export / import of the library symbols |
| `XYO_WINSERVICE_INTERNAL` | defined when building the library itself (`XYO_WIN_SERVICE_INTERNAL` is accepted too) |
| `XYO_WINSERVICE_LIBRARY` | define for a static build: `XYO_WINSERVICE_EXPORT` becomes empty |
