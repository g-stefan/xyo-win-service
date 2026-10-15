# Writing a service

## The class

```cpp
class MyService : public virtual Service {
		XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(MyService);

	public:
		MyService();                  // set serviceName, stopRun
		DWORD serviceInit();          // optional
		void serviceWork();           // the service
		HRESULT serviceCOMInitialize(); // optional
		void serviceCOMUninitialize();  // optional
		int main(int cmdN, char *cmdS[]); // optional
};

XYO_APPLICATION_MAIN(MyService);
```

`Service` derives from `IApplication` (`xyo-system`); the application
object is created once by `XYO_APPLICATION_MAIN`. Most of the state of
`Service` is **static** (`serviceName`, `stopRun`, `serviceStopEvent`,
`status`, `serviceThis`): there is **one service per process** (the
service is registered as `SERVICE_WIN32_OWN_PROCESS`).

The `Service` constructor sets `serviceThis = this`, `stopRun` to a
function that never stops, and `serviceName = "XYO-WinService"`. Set your
own values in **your** constructor, which runs after it:

```cpp
MyService::MyService() {
	serviceName = "MyCompany-MyService";
	stopRun = stopOnKeyQ;
};
```

`serviceName` is a `const char *` that is kept, not copied: use a string
literal or storage that lives as long as the process. It is the SCM key
name **and** the display name. It must not contain `/` or `\` and can be
at most 256 characters; the SCM compares names without case.

## Life cycle

The same sequence runs in both modes, on a worker thread:

```
serviceCOMInitialize()           failed  -> exit code = the HRESULT, stop
serviceInit()                    error   -> exit code = the error, skip to COM uninitialize
    report SERVICE_RUNNING
serviceWork()                    returns -> exit code NO_ERROR
serviceCOMUninitialize()         (only if serviceCOMInitialize succeeded)
```

An exception thrown by `serviceInit()` or `serviceWork()` is caught; the
exit code becomes `ERROR_EXCEPTION_IN_SERVICE` (1064).

**As a service** (`--service`, started by the SCM):

```
main --service
  StartServiceCtrlDispatcher            the main thread waits here until the service stops
    serviceMain                         SCM thread, registered with the XYO memory registry
      RegisterServiceCtrlHandlerEx
      report SERVICE_START_PENDING      wait hint 30 s
      worker sequence above             SERVICE_RUNNING after serviceInit succeeds
      report SERVICE_STOPPED            with the exit code
```

The control handler runs on the dispatcher thread. For
`SERVICE_CONTROL_STOP`, `SERVICE_CONTROL_SHUTDOWN` and
`SERVICE_CONTROL_PRESHUTDOWN`, **when the service is running**, it
reports `SERVICE_STOP_PENDING` (wait hint 30 s) and signals
`serviceStopEvent`. A stop during the start (while `serviceInit()` runs)
is refused by the SCM: the service accepts stop and shutdown only once it
is running. `SERVICE_CONTROL_INTERROGATE` is answered; pause, continue
and the other controls return `ERROR_CALL_NOT_IMPLEMENTED`.

**In the console** (`--run`):

```
main --run
  serviceStopEvent.reset()
  start the worker thread               worker sequence above, reportStatus is a no-op
  every 100 ms: stopRun() ?             until it returns true, or the worker ends
  serviceStopEvent.notify()
  join the worker, return the exit code
```

## serviceWork() and the stop signal

`serviceWork()` **is** the service: while it runs the service is
running; when it returns the service stops (exit code `NO_ERROR`). It must
return soon after `serviceStopEvent` is signaled — the SCM, `--stop`,
and a system shutdown all wait for it.

`serviceStopEvent` is an `XYO::Multithreading::Semaphore`:

| Call | Use |
|------|-----|
| `serviceStopEvent.wait()` | block until stop (nothing else to do on this thread) |
| `serviceStopEvent.waitFor(ms)` | periodic work: `false` after `ms`, `true` on stop |
| `serviceStopEvent.peek()` | check inside a loop, lock free, does not clear the signal |

`wait()` / `waitFor()` **consume** the signal (it is cleared when they
return `true`); `peek()` does not. Wait on it from one thread only (the
worker); other threads should `peek()`.

Patterns:

```cpp
// Periodic job
void MyService::serviceWork() {
	do {
		runJob();
	} while (!serviceStopEvent.waitFor(60 * 1000));
};

// Loop with short steps
void MyService::serviceWork() {
	while (!serviceStopEvent.peek()) {
		processNextItem(); // a few hundred ms at most
	};
};

// Server: own threads do the work, the worker waits for stop
void MyService::serviceWork() {
	server.start();          // starts its threads
	serviceStopEvent.wait();
	server.stop();           // tell them to end, join them
};
```

If a step can take long, make it check `serviceStopEvent.peek()` too, or
close the handle / socket it blocks on when the stop arrives. A long
cleanup after the stop can call `Service::reportPending()` to keep the SCM
waiting (see below).

## serviceInit() — start that can fail

`serviceInit()` runs before the service is reported running. Load the
configuration, open files, bind ports here: if something is wrong return
a Win32 error code and the start fails cleanly with that code —
`--start` prints it, the SCM shows it ("The service ... terminated with
the following error"), the event log records it.

```cpp
DWORD MyService::serviceInit() {
	if (!config.load(configFile)) {
		return ERROR_BAD_CONFIGURATION;   // 1610
	};
	if (!server.bind(port)) {
		return ERROR_ADDRESS_ALREADY_ASSOCIATED;
	};
	return NO_ERROR;
};
```

Return a real Win32 code (`ERROR_*` from `winerror.h`), not
`ERROR_SERVICE_SPECIFIC_ERROR`: there is no way to give the
service-specific code from `serviceInit()`. `serviceWork()` is not called
when it fails; `serviceCOMUninitialize()` still is.

**Long start.** The start is reported pending with a 30 s wait hint. If
`serviceInit()` needs more, call `Service::reportPending()` at least every
30 s; each call increments the check point and gives another 30 s:

```cpp
DWORD MyService::serviceInit() {
	for (size_t k = 0; k < databaseCount; ++k) {
		if (!openDatabase(k)) {
			return ERROR_OPEN_FAILED;
		};
		reportPending(); // progress, 30 s more
	};
	return NO_ERROR;
};
```

`reportPending()` does nothing outside `SERVICE_START_PENDING` /
`SERVICE_STOP_PENDING`, and nothing in `--run` mode, so it is always safe
to call. Use the same call during a long cleanup after the stop.

## Exit codes

| Situation | Exit code reported to the SCM | `--run` / `--start` return |
|-----------|-------------------------------|----------------------------|
| `serviceWork()` returned | `NO_ERROR` | `0` |
| `serviceInit()` returned `e` | `e` | `e` |
| exception in `serviceInit()` / `serviceWork()` | `ERROR_EXCEPTION_IN_SERVICE` (1064) | `1064` |
| `serviceCOMInitialize()` returned `hr` | `ERROR_SERVICE_SPECIFIC_ERROR`, service-specific = `hr` | `hr` (as `int`) |

A service that returns from `serviceWork()` on its own (no stop) ends
normally with `NO_ERROR`: the SCM shows it stopped, not failed. Return an
error from `serviceInit()`, or throw, when the service should be seen as
failed. The SCM recovery actions ("restart the service") run by default
only when the process dies without reporting `SERVICE_STOPPED`; to run
them also for a stop with an error exit code set the failure actions flag
(`sc failureflag <name> 1`, see [Command line and deployment](command-line.md)).

## COM

By default the worker thread is initialized for COM with
`CoInitializeEx(NULL, COINIT_MULTITHREADED)` and
`CoInitializeSecurity(...)` with default authentication and
impersonation level `RPC_C_IMP_LEVEL_IMPERSONATE` (needed for WMI and
most COM servers). If security was already set in the process
(`RPC_E_TOO_LATE`, e.g. a second `--run` in the same process) that is
not an error. `serviceCOMUninitialize()` calls `CoUninitialize()`.

Override the pair to change it. **Override both** — the default
uninitialize calls `CoUninitialize()`, which must match a successful
initialize:

```cpp
// No COM at all
HRESULT MyService::serviceCOMInitialize() {
	return S_OK;
};
void MyService::serviceCOMUninitialize() {
};

// Single threaded apartment (e.g. for components that require it)
HRESULT MyService::serviceCOMInitialize() {
	return CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
};
// default serviceCOMUninitialize() is right here
```

COM is initialized only on the worker thread: threads you start yourself
initialize COM themselves.

## Threads

- `serviceInit()`, `serviceWork()` and the COM hooks run on the worker
  thread; `stopRun` on the main thread (`--run`); the control handler on
  the SCM dispatcher thread. Share data between them only through
  `serviceStopEvent` or your own synchronization.
- The worker thread is registered with the XYO managed memory registry in
  both modes, so `TPointer`, `String` and the XYO containers work in it.
  Start your own threads with `XYO::Multithreading::Thread` (or the
  `Worker` classes) so they are registered too.
- Join every thread you start before `serviceWork()` returns: after
  `SERVICE_STOPPED` is reported the SCM may end the process at any time.

## Your own command line

`Service::main(cmdN, cmdS)` reads the arguments in order and only looks at
those that start with `--`:

- `--install`, `--uninstall`, `--start`, `--stop` run, in order, and
  processing stops at the first one that fails (its error is printed on
  `stderr` and returned);
- `--run` and `--service` return at once with their result (anything
  after them is not processed);
- every other argument is ignored, so you can add your own options.

With no command at all it returns `1` and prints nothing: show your own
usage. Typical `main`:

```cpp
int MyService::main(int cmdN, char *cmdS[]) {
	for (int i = 1; i < cmdN; ++i) {
		if (strncmp(cmdS[i], "--config=", 9) == 0) {
			configFile = &cmdS[i][9];
		};
		if (strcmp(cmdS[i], "--version") == 0) {
			printf("%s\n", XYO::WinService::Version::version());
			return 0;
		};
	};
	if (cmdN < 2) {
		printf("usage: %s --install | --uninstall | --start | --stop | --run [--config=file]\n", cmdS[0]);
		return 1;
	};
	return Service::main(cmdN, cmdS);
};
```

Remember that the SCM starts the program as `"<exe>" --service` only:
options given on the `--install` command line are **not** passed to the
service. Read the configuration from a file next to the executable or
from the registry instead (see [Command line and deployment](command-line.md)).

Running without arguments in the console is also common for a sample or
a tool (as in `test/test.01.cpp`):

```cpp
if (cmdN == 1) {
	static char runOption[] = "--run";
	char *cmdSX[2] = {cmdS[0], runOption};
	return Service::main(2, cmdSX);
};
```

## stopRun

`Service::stopRun` is a `bool (*)()` polled every 100 ms by `--run` on
the main thread; returning `true` stops the run. `nullptr` is allowed
(only the end of `serviceWork()` stops the run). Common choices:

```cpp
// A key
static bool stopOnKeyQ() {
	return Console::keyHit() && (Console::getChar() == 'q');
};

// Ctrl+C
static TAtomic<bool> ctrlC(false);
static BOOL WINAPI onConsoleCtrl(DWORD type) {
	if ((type == CTRL_C_EVENT) || (type == CTRL_BREAK_EVENT)) {
		ctrlC.set(true);
		return TRUE;
	};
	return FALSE;
};
static bool stopOnCtrlC() {
	return ctrlC.get();
};
// in the constructor: SetConsoleCtrlHandler(onConsoleCtrl, TRUE); stopRun = stopOnCtrlC;
```

`stopRun` is not used when running as a service.
