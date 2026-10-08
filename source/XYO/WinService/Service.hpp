// Win Service
// Copyright (c) 2023-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_WINSERVICE_SERVICE_HPP
#define XYO_WINSERVICE_SERVICE_HPP

#ifndef XYO_WINSERVICE_DEPENDENCY_HPP
#	include <XYO/WinService/Dependency.hpp>
#endif

namespace XYO::WinService {
	typedef XYO::Multithreading::Semaphore Semaphore;

	//
	// Windows service application, one per process.
	//
	// Command line (processed in order, unknown options are ignored so a
	// derived application can handle its own):
	//   --install    install (or update) the service, auto start
	//   --uninstall  stop and remove the service
	//   --start      start the service and wait until it is running
	//   --stop       stop the service and wait until it is stopped
	//   --run        run serviceWork() in console, stopRun() ends it
	//   --service    entry point used by the Service Control Manager
	//
	// cmd*() return NO_ERROR or a Win32 error code, main() returns it.
	//
	// Worker sequence, on the service thread:
	//   serviceCOMInitialize(), serviceInit(), report running,
	//   serviceWork(), serviceCOMUninitialize()
	// serviceWork() must return after serviceStopEvent is set.
	//
	class Service : public virtual IApplication {
			XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(Service);

		public:
			typedef bool (*StopRun)();

			XYO_WINSERVICE_EXPORT static const char *serviceName;
			XYO_WINSERVICE_EXPORT static StopRun stopRun;
			XYO_WINSERVICE_EXPORT static Semaphore serviceStopEvent;
			XYO_WINSERVICE_EXPORT virtual void serviceWork();
			// Before the service is reported running, NO_ERROR or a Win32 error code to fail the start
			XYO_WINSERVICE_EXPORT virtual DWORD serviceInit();
			// Default: multithreaded COM and process COM security
			XYO_WINSERVICE_EXPORT virtual HRESULT serviceCOMInitialize();
			// Called only if serviceCOMInitialize() succeeded
			XYO_WINSERVICE_EXPORT virtual void serviceCOMUninitialize();
			//
			XYO_WINSERVICE_EXPORT static SERVICE_STATUS status;
			XYO_WINSERVICE_EXPORT static SERVICE_STATUS_HANDLE statusHandle;
			XYO_WINSERVICE_EXPORT static Service *serviceThis;
			//
			XYO_WINSERVICE_EXPORT static void init();
			XYO_WINSERVICE_EXPORT static VOID WINAPI serviceMain(DWORD argc, LPSTR *argv);
			XYO_WINSERVICE_EXPORT static DWORD WINAPI serviceCtrlHandler(DWORD control, DWORD eventType, LPVOID eventData, LPVOID context);
			// Report status to the Service Control Manager, no-op when not running as service
			XYO_WINSERVICE_EXPORT static void reportStatus(DWORD currentState, DWORD win32ExitCode, DWORD serviceSpecificExitCode, DWORD waitHint);
			// Signal progress of a long start (serviceInit) or stop, keeps the SCM from timing out
			XYO_WINSERVICE_EXPORT static void reportPending();
			// Returns the Win32 exit code, serviceSpecificExitCode is set for ERROR_SERVICE_SPECIFIC_ERROR
			XYO_WINSERVICE_EXPORT static DWORD worker(DWORD &serviceSpecificExitCode);
			XYO_WINSERVICE_EXPORT static DWORD cmdInstall();
			XYO_WINSERVICE_EXPORT static DWORD cmdUninstall();
			XYO_WINSERVICE_EXPORT static DWORD cmdStart();
			XYO_WINSERVICE_EXPORT static DWORD cmdStop();
			XYO_WINSERVICE_EXPORT static int cmdRun();
			XYO_WINSERVICE_EXPORT static int cmdService();
			XYO_WINSERVICE_EXPORT static void workerThread(void *);
			//
			XYO_WINSERVICE_EXPORT Service();
			XYO_WINSERVICE_EXPORT int main(int cmdN, char *cmdS[]);
	};

};

#endif
