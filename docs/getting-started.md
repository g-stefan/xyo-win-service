# Getting started

## 1. Build and install

The library is built with [fabricare](https://github.com/g-stefan/fabricare),
the build tool used by all XYO C++ projects. The XYO layers up to
`xyo-win` must be installed to the SDK first. It builds on Windows only
(MSVC). From the repository root:

```bash
fabricare make       # build into output/
fabricare test       # build and run test/test.*.cpp (run make first)
fabricare install    # copy output/{bin,include,lib} to ~/.fabricare/<platform>
fabricare clean      # remove output/ and temp/
```

| Project           | Kind                                | Notes |
|-------------------|-------------------------------------|-------|
| `xyo-win-service` | DLL (`dll-or-lib`)                  | a static library when the platform is built static |
| `test.01`         | test executable                     | interactive sample: without arguments runs in the console, counts to 9, `q` stops it early |
| `test.02`         | test executable                     | console mode tests: worker sequence, exit codes, stop, command line, SCM errors on a service that does not exist; no rights needed |
| `test.03`         | test executable                     | real SCM tests: install, start, stop, failed start, reinstall, uninstall of `XYO-WinService-Test03`; needs an **elevated** prompt, prints "skipped" otherwise |

To run the SCM tests, open an administrator terminal and run
`fabricare test` (or `output\test\test.03.exe`) there. The test removes
its service at the end, also when a check fails.

## 2. Depend on it from another fabricare project

In the consumer's `fabricare.json`:

```json
{
	"name": "my-service",
	"make": "exe",
	"sourcePath": "XYO/MyService",
	"dependency": [
		"xyo-win-service"
	]
}
```

`xyo-win` and the layers below come in as transitive dependencies.

## 3. Include and namespaces

```cpp
#include <XYO/WinService.hpp>
```

The umbrella header includes `<XYO/Win.hpp>` (and so `<windows.h>`,
`<XYO/System.hpp>`, ...), the metadata headers (`Copyright.hpp`,
`License.hpp`, `Version.hpp`) and `Service.hpp`.

Namespace `XYO::WinService` contains

```cpp
using namespace XYO::System;
using namespace XYO::Win;
```

so `using namespace XYO::WinService;` is enough to write `Service`,
`Thread`, `Console`, `String`, `TPointer` unqualified.
`XYO::WinService::Semaphore` is a typedef of
`XYO::Multithreading::Semaphore`.

Name clashes inherited from `xyo-win`, once you write
`using namespace XYO::WinService;`:

| Write | Not | Because of |
|-------|-----|------------|
| `XYO::System::Shell::getExecutablePath()` | `Shell::getExecutablePath()` | `XYO::Win::Shell` |
| `XYO::Win::Registry::readString(...)` | `Registry::...` | `XYO::ManagedMemory::Registry` |
| `class MyService` | a global `class Service` / `Application` | `XYO::WinService::Service`, `XYO::Win::Application` |

The metadata namespaces exist in every XYO layer, so the short names are
ambiguous: write `XYO::WinService::Version::version()` in full.

## 4. First service

A service that writes a line to a log file next to its executable when it
starts, every 10 seconds while it runs, and when it stops:

```cpp
#include <XYO/WinService.hpp>

#include <stdio.h>
#include <string>

using namespace XYO::WinService;

class HeartbeatService : public virtual Service {
		XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(HeartbeatService);

	public:
		HeartbeatService();

		DWORD serviceInit();
		void serviceWork();
		int main(int cmdN, char *cmdS[]);

		static bool stopOnKeyQ();

	protected:
		std::string logFile;

		void log(const char *message);
};

HeartbeatService::HeartbeatService() {
	serviceName = "XYO-Heartbeat"; // SCM name and display name
	stopRun = stopOnKeyQ;          // ends a --run session
};

DWORD HeartbeatService::serviceInit() {
	// A service starts in C:\Windows\System32: use absolute paths
	char path[MAX_PATH];
	DWORD length = GetModuleFileNameA(NULL, path, MAX_PATH);
	if ((length == 0) || (length >= MAX_PATH)) {
		return ERROR_BAD_PATHNAME;
	};
	logFile = path;
	logFile += ".log";

	FILE *file = fopen(logFile.c_str(), "ab");
	if (file == nullptr) {
		return ERROR_OPEN_FAILED; // start fails, --start reports error 110
	};
	fclose(file);
	return NO_ERROR;
};

void HeartbeatService::log(const char *message) {
	SYSTEMTIME now;
	GetLocalTime(&now);
	FILE *file = fopen(logFile.c_str(), "ab");
	if (file != nullptr) {
		fprintf(file, "%04u-%02u-%02u %02u:%02u:%02u %s\r\n",
		        now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, message);
		fclose(file);
	};
	printf("%s\n", message); // visible with --run, discarded as a service
};

void HeartbeatService::serviceWork() {
	log("started");
	// waitFor returns true when the stop is signaled, false every 10 s
	while (!serviceStopEvent.waitFor(10000)) {
		log("alive");
	};
	log("stopped");
};

bool HeartbeatService::stopOnKeyQ() {
	return Console::keyHit() && (Console::getChar() == 'q');
};

int HeartbeatService::main(int cmdN, char *cmdS[]) {
	if (cmdN < 2) {
		printf("usage: %s --install | --uninstall | --start | --stop | --run\n", cmdS[0]);
		return 1;
	};
	return Service::main(cmdN, cmdS);
};

XYO_APPLICATION_MAIN(HeartbeatService);
```

`XYO_APPLICATION_MAIN(HeartbeatService)` (from `xyo-system`) defines
`main`: it initializes the XYO memory registry, creates the application
object and calls `HeartbeatService::main`. `Service::main` reads the
`--` options and does the work.

## 5. Try it in the console

```bash
my-service --run
```

The program does exactly what the service will do — COM initialization,
`serviceInit()`, `serviceWork()` on a worker thread — but in the
console, without the SCM and without administrator rights. Press `q`:
`stopRun` returns `true`, `serviceStopEvent` is signaled, `serviceWork()`
returns, the exit code is `0` (or the error of `serviceInit()`). Debug it
like any console program (`.vscode/launch.json` in this repository starts
`test.01 --run` under the debugger).

## 6. Install and control it

In an **administrator** terminal:

```bash
my-service --install --start
```

`--install` registers the executable (with its full path, quoted) as an
auto start service running as `LocalSystem`; `--start` starts it and
waits until it is running, or prints why it failed:

```
XYO-Heartbeat: --start failed, error 110
```

Then:

```bash
my-service --stop        # stop, wait until stopped
my-service --start       # start again
my-service --install     # after rebuilding: stops it and points the service to this executable
my-service --uninstall   # stop and remove
```

`sc query XYO-Heartbeat`, `services.msc` and the event log show the same
service. See [Command line and deployment](command-line.md) for the
details: rights, where the DLLs must be, the account, and how to set a
description or recovery options.

As a fabricare test project:

```json
{
	"name": "test.04",
	"make": "exe",
	"category": "test",
	"SPDX-License-Identifier": "Unlicense",
	"dependency": [
		"xyo-win-service"
	]
}
```

with the source in `test/test.04.cpp`.

## 7. Building without fabricare

`source/XYO/WinService.Amalgam.cpp` includes every `.cpp` of the library.
Compile it with your sources, together with `xyo-win` and the layers below
built the same way (see their getting started pages):

1. Put `source/` of every XYO layer up to `xyo-win` and of
   `xyo-win-service` on the include path.
2. Define `XYO_WINSERVICE_LIBRARY` and the `..._LIBRARY` macros of every
   layer below everywhere (plain static linking), and
   `XYO_PLATFORM_COMPILE_STATIC`.
3. Link `advapi32` (service API) and `ole32` (COM), plus the libraries
   `xyo-win` needs.
