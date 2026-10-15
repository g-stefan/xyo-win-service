# Command line and deployment

## The options

`Service::main` handles these options; the program can add its own (see
[Writing a service](writing-a-service.md#your-own-command-line)).

| Option | Rights | Does | Returns |
|--------|--------|------|---------|
| `--install` | administrator | register the service, or update an existing one | `0` or a Win32 error |
| `--uninstall` | administrator | stop and remove the service | `0` (also when it was not installed) or a Win32 error |
| `--start` | administrator (\*) | start the service and wait until it runs | `0` (also when already running), the service's exit code, or a Win32 error |
| `--stop` | administrator (\*) | stop the service and wait until it is stopped | `0` (also when not running) or a Win32 error |
| `--run` | none | run the service code in the console | the service's exit code |
| `--service` | — | used by the SCM only | `0`, or `1063` when started from a console |

(\*) by default; an administrator can grant start / stop to other users
by changing the service security descriptor (`sc sdset`).

`--install`, `--uninstall`, `--start`, `--stop` can be combined and run
in order: `--uninstall --install --start`. The first one that fails stops
the processing; its error is printed on `stderr`

```
MyService: --start failed, error 1053
```

and is the exit code of the program. `--run` and `--service` end the
processing (options after them are ignored). Arguments that are not one of
these are ignored; with none of them `main` returns `1` silently.

To see the text of an error: `net helpmsg 1053`, or
`certutil -error 0x80004005` for an `HRESULT`.

## --install

Creates the service, if it does not exist, with:

| Setting | Value |
|---------|-------|
| Name and display name | `serviceName` |
| Command line | `"<full path of this executable>" --service` (quoted) |
| Type | own process (`SERVICE_WIN32_OWN_PROCESS`) |
| Start | automatic (`SERVICE_AUTO_START`), at boot |
| Error control | normal (logged, boot continues) |
| Account | `LocalSystem` |
| Description, dependencies, recovery | none |

It does **not** start the service: add `--start`.

If the service already exists (a new build, or the executable moved), it
**stops it**, waits for the stop, and updates only the command line, the
type, the start type (back to automatic) and the error control. The
account and password, the description, the dependencies, the recovery
options, the delayed start flag and the security settings an administrator
set are kept. Then start it again: `--install --start`.

The SCM runs the executable from the path it was installed from. Copy the
program (and its DLLs, see below) to its final folder, e.g.
`C:\Program Files\MyCompany\MyService\`, and run `--install` from
**there**, not from a build folder.

## --uninstall

Stops the service (waits for the stop), then deletes it. When some other
program still has the service open (`services.msc`, an `sc` command in
progress, the event viewer), Windows only marks it for deletion; this is
treated as success, but the name cannot be installed again until those
handles are closed (error 1072).

## --start

Starts the service and waits while it is `SERVICE_START_PENDING`. The
wait follows the SCM protocol: as long as the service makes progress (its
check point grows, see `reportPending()`) within its wait hint (30 s), it
keeps waiting. Results:

| Service | `--start` returns |
|---------|-------------------|
| reached `SERVICE_RUNNING` | `0` |
| was already running | `0` |
| stopped during the start | its exit code: the error from `serviceInit()`, `1064` for an exception, the `HRESULT` when COM initialization failed |
| no progress within the wait hint | `1053` (`ERROR_SERVICE_REQUEST_TIMEOUT`) |
| not installed / disabled / no rights | `1060` / `1058` / `5` |

## --stop

Sends the stop control and waits while the service is
`SERVICE_STOP_PENDING`, with the same progress rule. A service that is
not running is not an error; a service that is already stopping is waited
for. `1053` means `serviceWork()` did not return (and did not call
`reportPending()`) within 30 s; `1060` that it is not installed.

## --run

Runs `serviceCOMInitialize()`, `serviceInit()`, `serviceWork()`,
`serviceCOMUninitialize()` on a worker thread, in the console, as the
current user, in the current folder. `stopRun()` is called every 100 ms on
the main thread; when it returns `true` the stop is signaled. No SCM, no
rights needed. The exit code is the one the service would report. Use it
to develop, debug, and to run the program as a plain console tool.

## --service

The command line registered by `--install`. It connects the process to
the SCM (`StartServiceCtrlDispatcher`) and returns when the service has
stopped. Run from a console it fails at once with `1063`
(`ERROR_FAILED_SERVICE_CONTROLLER_CONNECT`).

## The service environment

What is different when the program runs as a service:

| Topic | As a service | What to do |
|-------|--------------|------------|
| Account | `LocalSystem`: full control of the machine; on the network it is the computer account | to reduce rights: `sc config MyService obj= "NT AUTHORITY\LocalService"` (or `NetworkService`, or a user with `password=`) — kept by a reinstall |
| Current folder | `C:\Windows\System32` | build absolute paths from the executable's folder (`XYO::System::Shell::getExecutablePath()` returns it) or from `GetModuleFileName` |
| Console | none: `printf` / `stdout` / `stderr` go nowhere | log to a file or the event log; keep `printf` for `--run` |
| Desktop | session 0, no visible windows or message boxes | never wait for user input in the service |
| Environment | the system environment, not your user's `PATH` | see DLLs below; extra variables: `Environment` (`REG_MULTI_SZ`) value in `HKLM\SYSTEM\CurrentControlSet\Services\<name>` |
| DLLs | searched first in the executable's folder | put the XYO DLLs (`xyo-win-service.dll`, `xyo-win.dll`, ...) next to the executable, or link statically |
| Drives | no mapped network drives | use UNC paths (`\\server\share\...`) |
| `HKEY_CURRENT_USER` | the registry of the service account | keep machine configuration in `HKEY_LOCAL_MACHINE` or in a file |
| Shutdown | stop arrives as `SERVICE_CONTROL_SHUTDOWN`, Windows waits only a few seconds | make `serviceWork()` return quickly |
| Arguments | only `--service` | read configuration from a file or the registry, not from the command line |

A missing DLL makes the process fail before `main` runs: `--start` then
reports an error (often `1053`) and nothing is logged by your program.
Check with `--run` from a plain `cmd.exe` whose `PATH` does not contain
the SDK.

`test/test.03.cpp` shows the `Environment` registry value: it gives the
test service the `PATH` of the test run, because the test executable is
in `output/test` and the DLLs are in `output/bin`.

## Settings with sc.exe

`--install` sets only the minimum. Everything else is set once by an
administrator (or by your installer) and survives a reinstall
(administrator terminal; the space after `=` is required by `sc`):

```bash
sc description MyService "What my service does"
sc config MyService DisplayName= "My Service"
sc config MyService start= delayed-auto
sc config MyService depend= Tcpip/Dnscache
sc config MyService obj= "NT AUTHORITY\LocalService" password= ""
sc failure MyService reset= 86400 actions= restart/5000/restart/5000/restart/60000
sc failureflag MyService 1
```

- `start= delayed-auto`: start shortly after boot instead of during it.
  `start= demand` (manual) or `start= disabled` are reset to automatic by
  the next `--install`.
- `sc failure` sets the recovery actions (here: restart after 5 s, 5 s,
  then 60 s, counter reset after a day). They run when the process ends
  without reporting `SERVICE_STOPPED` (a crash). `sc failureflag ... 1`
  makes them run also when the service stops with an error exit code
  (failed `serviceInit()`, exception).
- `sc query MyService`, `sc qc MyService`, `sc qfailure MyService` show
  the state, the configuration, the recovery actions.

The same can be done from code with `ChangeServiceConfig2` after
`cmdInstall()` (see `setDescription` in `test/test.03.cpp`).

## Troubleshooting

| Error | Meaning | Usual cause |
|-------|---------|-------------|
| `5` | access denied | not an elevated (administrator) terminal |
| `2` / `3` (start) | file / path not found | the executable moved after `--install`: run `--install` again from the new place |
| `1053` | request timeout | `serviceInit()` longer than 30 s without `reportPending()`; `serviceWork()` does not return on stop; the process can not start (missing DLL) |
| `1058` | service disabled | `sc config MyService start= auto` or `--install` |
| `1060` | service does not exist | not installed, or a different `serviceName` |
| `1063` | can not connect to the SCM | `--service` run from a console: use `--run` |
| `1064` | exception in service | an exception escaped `serviceInit()` / `serviceWork()` |
| `1069` | logon failure | wrong password of the service account |
| `1072` | marked for delete | close `services.msc` / other tools, then install again |
| `0x8...` (large number) | an `HRESULT` | COM initialization failed (`serviceCOMInitialize()`) |

The SCM also writes to the **System** event log, source *Service Control
Manager*: 7000 (failed to start), 7009 (timeout), 7023 (terminated with an
error — the code from `serviceInit()`), 7024 (service-specific error — the
COM `HRESULT`), 7031 / 7034 (terminated unexpectedly — a crash).
