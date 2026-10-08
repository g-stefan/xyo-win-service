// Win Service
// Copyright (c) 2023-2026 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/WinService/Service.hpp>
#include <XYO/Multithreading/CriticalSectionLock.hpp>

#include <string>

namespace XYO::WinService {
	typedef XYO::Multithreading::CriticalSectionLock CriticalSectionLock;

	SERVICE_STATUS Service::status;
	SERVICE_STATUS_HANDLE Service::statusHandle;
	Semaphore Service::serviceStopEvent;
	const char *Service::serviceName;
	Service *Service::serviceThis;
	Service::StopRun Service::stopRun;

	// Wait hint reported while start / stop is pending, and the time a
	// pending state may last without progress before a command gives up
	static const DWORD pendingWaitHint_ = 30000;

	// status is written by the service thread and by the control handler
	// (dispatcher thread), statusSection keeps updates ordered
	static CriticalSection statusSection_;
	static DWORD statusCheckPoint_ = 0;

	// Closes a service / SCM handle at end of scope
	class ServiceHandle_ {
			XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(ServiceHandle_);

		public:
			SC_HANDLE handle;

			inline ServiceHandle_(SC_HANDLE handle_) : handle(handle_) {};

			inline ~ServiceHandle_() {
				if (handle != NULL) {
					CloseServiceHandle(handle);
				};
			};

			inline operator SC_HANDLE() const {
				return handle;
			};
	};

	struct WorkerResult_ {
			DWORD win32ExitCode;
			DWORD serviceSpecificExitCode;
	};

	static bool noStop() {
		return false;
	};

	Service::Service() {
		serviceThis = this;
		stopRun = noStop;
		init();
	};

	void Service::serviceWork() {

	};

	DWORD Service::serviceInit() {
		return NO_ERROR;
	};

	HRESULT Service::serviceCOMInitialize() {
		HRESULT hres = CoInitializeEx(NULL, COINIT_MULTITHREADED);
		if (FAILED(hres)) {
			return hres;
		};

		hres = CoInitializeSecurity(
		    NULL,
		    -1,
		    NULL,
		    NULL,
		    RPC_C_AUTHN_LEVEL_DEFAULT,
		    RPC_C_IMP_LEVEL_IMPERSONATE,
		    NULL,
		    EOAC_NONE,
		    NULL);

		// RPC_E_TOO_LATE - security already set for this process, keep it
		if (FAILED(hres) && (hres != RPC_E_TOO_LATE)) {
			CoUninitialize();
			return hres;
		};

		return S_OK;
	};

	void Service::serviceCOMUninitialize() {
		CoUninitialize();
	};

	void Service::init() {
		CriticalSectionLock lock(statusSection_);

		memset(&status, 0, sizeof(SERVICE_STATUS));
		statusHandle = NULL;
		statusCheckPoint_ = 0;

		serviceName = "XYO-WinService";
	};

	// statusSection_ must be entered
	static void setStatus_(DWORD currentState, DWORD win32ExitCode, DWORD serviceSpecificExitCode, DWORD waitHint) {
		SERVICE_STATUS &status = Service::status;

		if (Service::statusHandle == NULL) {
			return;
		};

		// Stopped is final, and a stop in progress is not undone
		if (status.dwCurrentState == SERVICE_STOPPED) {
			return;
		};
		if ((status.dwCurrentState == SERVICE_STOP_PENDING) && (currentState != SERVICE_STOPPED)) {
			return;
		};

		status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		status.dwCurrentState = currentState;
		status.dwWin32ExitCode = win32ExitCode;
		status.dwServiceSpecificExitCode = serviceSpecificExitCode;
		status.dwWaitHint = waitHint;

		if (currentState == SERVICE_RUNNING) {
			status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
		} else {
			status.dwControlsAccepted = 0;
		};

		// Check point must grow while pending, and be 0 otherwise
		if ((currentState == SERVICE_START_PENDING) || (currentState == SERVICE_STOP_PENDING)) {
			++statusCheckPoint_;
		} else {
			statusCheckPoint_ = 0;
		};
		status.dwCheckPoint = statusCheckPoint_;

		SetServiceStatus(Service::statusHandle, &status);
	};

	void Service::reportStatus(DWORD currentState, DWORD win32ExitCode, DWORD serviceSpecificExitCode, DWORD waitHint) {
		CriticalSectionLock lock(statusSection_);
		setStatus_(currentState, win32ExitCode, serviceSpecificExitCode, waitHint);
	};

	void Service::reportPending() {
		CriticalSectionLock lock(statusSection_);
		if ((status.dwCurrentState == SERVICE_START_PENDING) || (status.dwCurrentState == SERVICE_STOP_PENDING)) {
			setStatus_(status.dwCurrentState, NO_ERROR, 0, pendingWaitHint_);
		};
	};

	DWORD Service::worker(DWORD &serviceSpecificExitCode) {
		DWORD retV;
		HRESULT hres;

		serviceSpecificExitCode = 0;

		hres = serviceThis->serviceCOMInitialize();
		if (FAILED(hres)) {
			serviceSpecificExitCode = (DWORD)hres;
			return ERROR_SERVICE_SPECIFIC_ERROR;
		};

		try {
			retV = serviceThis->serviceInit();
			if (retV == NO_ERROR) {
				reportStatus(SERVICE_RUNNING, NO_ERROR, 0, 0);
				serviceThis->serviceWork();
			};
		} catch (...) {
			retV = ERROR_EXCEPTION_IN_SERVICE;
		};

		serviceThis->serviceCOMUninitialize();

		return retV;
	};

	DWORD WINAPI Service::serviceCtrlHandler(DWORD control, DWORD, LPVOID, LPVOID) {
		switch (control) {
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
		case SERVICE_CONTROL_PRESHUTDOWN: {
			CriticalSectionLock lock(statusSection_);
			// Only a running service can be stopped, ignore if already stopping or stopped
			if (status.dwCurrentState == SERVICE_RUNNING) {
				setStatus_(SERVICE_STOP_PENDING, NO_ERROR, 0, pendingWaitHint_);
				serviceStopEvent.notify();
			};
			return NO_ERROR;
		};
		case SERVICE_CONTROL_INTERROGATE:
			return NO_ERROR;
		default:
			break;
		};
		return ERROR_CALL_NOT_IMPLEMENTED;
	};

	VOID WINAPI Service::serviceMain(DWORD, LPSTR *) {
		DWORD win32ExitCode;
		DWORD serviceSpecificExitCode;

		RegistryThread::threadBegin();

		serviceStopEvent.reset();

		{
			CriticalSectionLock lock(statusSection_);
			memset(&status, 0, sizeof(SERVICE_STATUS));
			statusCheckPoint_ = 0;
			statusHandle = RegisterServiceCtrlHandlerExA(serviceName, serviceCtrlHandler, NULL);
		};

		if (statusHandle == NULL) {
			RegistryThread::threadEnd();
			return;
		};

		reportStatus(SERVICE_START_PENDING, NO_ERROR, 0, pendingWaitHint_);

		win32ExitCode = worker(serviceSpecificExitCode);

		// The process can be terminated at any time after SERVICE_STOPPED
		RegistryThread::threadEnd();

		reportStatus(SERVICE_STOPPED, win32ExitCode, serviceSpecificExitCode, 0);
	};

	// Wait while the service is in pendingState, give up if it makes no
	// progress (check point) within its wait hint
	static DWORD waitForState_(SC_HANDLE scService, DWORD pendingState, DWORD targetState) {
		SERVICE_STATUS_PROCESS svcStatus;
		DWORD bytesNeeded;
		DWORD waitHint;
		DWORD waitTime;
		DWORD lastCheckPoint;
		ULONGLONG lastProgress;
		ULONGLONG now;

		if (!QueryServiceStatusEx(scService, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&svcStatus), sizeof(svcStatus), &bytesNeeded)) {
			return GetLastError();
		};

		lastCheckPoint = svcStatus.dwCheckPoint;
		lastProgress = GetTickCount64();

		while (svcStatus.dwCurrentState == pendingState) {
			waitHint = svcStatus.dwWaitHint;
			if (waitHint == 0) {
				waitHint = pendingWaitHint_;
			};

			// Poll at a tenth of the wait hint, between 100 ms and 1 s
			waitTime = waitHint / 10;
			if (waitTime < 100) {
				waitTime = 100;
			};
			if (waitTime > 1000) {
				waitTime = 1000;
			};
			Thread::sleep(static_cast<int>(waitTime));

			if (!QueryServiceStatusEx(scService, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&svcStatus), sizeof(svcStatus), &bytesNeeded)) {
				return GetLastError();
			};

			now = GetTickCount64();
			if (svcStatus.dwCheckPoint != lastCheckPoint) {
				lastCheckPoint = svcStatus.dwCheckPoint;
				lastProgress = now;
				continue;
			};
			if ((svcStatus.dwCurrentState == pendingState) && (now - lastProgress > waitHint)) {
				return ERROR_SERVICE_REQUEST_TIMEOUT;
			};
		};

		if (svcStatus.dwCurrentState == targetState) {
			return NO_ERROR;
		};

		// Stopped instead of running, return why
		if (svcStatus.dwCurrentState == SERVICE_STOPPED) {
			if (svcStatus.dwWin32ExitCode == ERROR_SERVICE_SPECIFIC_ERROR) {
				return svcStatus.dwServiceSpecificExitCode;
			};
			return svcStatus.dwWin32ExitCode;
		};

		return ERROR_INVALID_SERVICE_CONTROL;
	};

	// scService needs SERVICE_STOP | SERVICE_QUERY_STATUS
	static DWORD stopService_(SC_HANDLE scService) {
		SERVICE_STATUS svcStatus;
		DWORD retV;

		if (ControlService(scService, SERVICE_CONTROL_STOP, &svcStatus)) {
			return waitForState_(scService, SERVICE_STOP_PENDING, SERVICE_STOPPED);
		};

		retV = GetLastError();
		if (retV == ERROR_SERVICE_NOT_ACTIVE) {
			return NO_ERROR;
		};

		// Already stopping, wait for it
		if (retV == ERROR_SERVICE_CANNOT_ACCEPT_CTRL) {
			if (waitForState_(scService, SERVICE_STOP_PENDING, SERVICE_STOPPED) == NO_ERROR) {
				return NO_ERROR;
			};
		};

		return retV;
	};

	DWORD Service::cmdInstall() {
		std::string executablePath;
		DWORD pathLength;

		executablePath.resize(MAX_PATH);
		for (;;) {
			pathLength = GetModuleFileNameA(NULL, &executablePath[0], static_cast<DWORD>(executablePath.size()));
			if (pathLength == 0) {
				return GetLastError();
			};
			if (pathLength < executablePath.size()) {
				executablePath.resize(pathLength);
				break;
			};
			// Truncated, retry with a bigger buffer, up to the longest path
			if (executablePath.size() >= 32768) {
				return ERROR_INSUFFICIENT_BUFFER;
			};
			executablePath.resize(executablePath.size() * 2);
		};

		// binPath = "<executable>" --service
		// The executable is quoted so that paths containing spaces are handled correctly.
		std::string binPath;
		binPath = "\"";
		binPath += executablePath;
		binPath += "\" --service";

		ServiceHandle_ scManager(OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE));
		if (scManager == NULL) {
			return GetLastError();
		};

		ServiceHandle_ scService(OpenServiceA(scManager, serviceName, SERVICE_CHANGE_CONFIG | SERVICE_STOP | SERVICE_QUERY_STATUS));
		if (scService != NULL) {
			// Already installed: stop it and update the configuration in place,
			// keeps account, description, dependencies and recovery options
			stopService_(scService);

			if (!ChangeServiceConfigA(
			        scService,
			        SERVICE_WIN32_OWN_PROCESS,
			        SERVICE_AUTO_START,
			        SERVICE_ERROR_NORMAL,
			        binPath.c_str(),
			        NULL,
			        NULL,
			        NULL,
			        NULL,
			        NULL,
			        NULL)) {
				return GetLastError();
			};
			return NO_ERROR;
		};

		DWORD retV = GetLastError();
		if (retV != ERROR_SERVICE_DOES_NOT_EXIST) {
			return retV;
		};

		ServiceHandle_ scNewService(CreateServiceA(
		    scManager,
		    serviceName,
		    serviceName,
		    SERVICE_QUERY_STATUS,
		    SERVICE_WIN32_OWN_PROCESS,
		    SERVICE_AUTO_START,
		    SERVICE_ERROR_NORMAL,
		    binPath.c_str(),
		    NULL,
		    NULL,
		    NULL,
		    NULL,
		    NULL));

		if (scNewService == NULL) {
			return GetLastError();
		};

		return NO_ERROR;
	};

	DWORD Service::cmdUninstall() {
		ServiceHandle_ scManager(OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT));
		if (scManager == NULL) {
			return GetLastError();
		};

		ServiceHandle_ scService(OpenServiceA(scManager, serviceName, SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE));
		if (scService == NULL) {
			DWORD retV = GetLastError();
			if (retV == ERROR_SERVICE_DOES_NOT_EXIST) {
				return NO_ERROR;
			};
			return retV;
		};

		// Stop first so deletion completes now, instead of being deferred
		// until the service ends and the last handle is closed
		stopService_(scService);

		if (!DeleteService(scService)) {
			DWORD retV = GetLastError();
			if (retV == ERROR_SERVICE_MARKED_FOR_DELETE) {
				return NO_ERROR;
			};
			return retV;
		};

		return NO_ERROR;
	};

	DWORD Service::cmdStart() {
		ServiceHandle_ scManager(OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT));
		if (scManager == NULL) {
			return GetLastError();
		};

		ServiceHandle_ scService(OpenServiceA(scManager, serviceName, SERVICE_START | SERVICE_QUERY_STATUS));
		if (scService == NULL) {
			return GetLastError();
		};

		if (!StartServiceA(scService, 0, NULL)) {
			DWORD retV = GetLastError();
			if (retV == ERROR_SERVICE_ALREADY_RUNNING) {
				return NO_ERROR;
			};
			return retV;
		};

		return waitForState_(scService, SERVICE_START_PENDING, SERVICE_RUNNING);
	};

	DWORD Service::cmdStop() {
		ServiceHandle_ scManager(OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT));
		if (scManager == NULL) {
			return GetLastError();
		};

		ServiceHandle_ scService(OpenServiceA(scManager, serviceName, SERVICE_STOP | SERVICE_QUERY_STATUS));
		if (scService == NULL) {
			return GetLastError();
		};

		return stopService_(scService);
	};

	int Service::cmdRun() {
		Thread thread;
		WorkerResult_ result;

		result.win32ExitCode = NO_ERROR;
		result.serviceSpecificExitCode = 0;

		serviceStopEvent.reset();
		if (!thread.start(workerThread, &result)) {
			return ERROR_SERVICE_NO_THREAD;
		};

		while (thread.isRunning()) {
			if ((stopRun != nullptr) && stopRun()) {
				break;
			};
			Thread::sleep(100);
		};

		serviceStopEvent.notify();
		thread.join();

		if (result.win32ExitCode == ERROR_SERVICE_SPECIFIC_ERROR) {
			return static_cast<int>(result.serviceSpecificExitCode);
		};
		return static_cast<int>(result.win32ExitCode);
	};

	int Service::cmdService() {
		SERVICE_TABLE_ENTRYA serviceTable[] = {
		    {const_cast<LPSTR>(serviceName), serviceMain},
		    {NULL, NULL}};

		if (!StartServiceCtrlDispatcherA(serviceTable)) {
			return GetLastError();
		};
		return 0;
	};

	void Service::workerThread(void *result_) {
		WorkerResult_ *result = static_cast<WorkerResult_ *>(result_);
		result->win32ExitCode = worker(result->serviceSpecificExitCode);
	};

	int Service::main(int cmdN, char *cmdS[]) {
		int i;
		char *opt;
		bool hasCommand = false;
		DWORD retV;

		for (i = 1; i < cmdN; ++i) {
			if (strncmp(cmdS[i], "--", 2) != 0) {
				continue;
			};
			opt = &cmdS[i][2];
			if (strcmp(opt, "install") == 0) {
				retV = cmdInstall();
			} else if (strcmp(opt, "uninstall") == 0) {
				retV = cmdUninstall();
			} else if (strcmp(opt, "start") == 0) {
				retV = cmdStart();
			} else if (strcmp(opt, "stop") == 0) {
				retV = cmdStop();
			} else if (strcmp(opt, "run") == 0) {
				return cmdRun();
			} else if (strcmp(opt, "service") == 0) {
				return cmdService();
			} else {
				// Not ours, a derived application may handle it
				continue;
			};
			hasCommand = true;
			if (retV != NO_ERROR) {
				fprintf(stderr, "%s: --%s failed, error %lu\n", serviceName, opt, retV);
				return static_cast<int>(retV);
			};
		};

		// No command, the derived application shows its own usage
		if (!hasCommand) {
			return 1;
		};

		return 0;
	};

};
