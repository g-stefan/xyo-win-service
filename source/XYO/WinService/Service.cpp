// Win Service
// Copyright (c) 2023 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/WinService/Service.hpp>

namespace XYO::WinService {

	SERVICE_STATUS Service::status;
	SERVICE_STATUS_HANDLE Service::statusHandle;
	Multithreading::Semaphore Service::serviceStopEvent;
	const char *Service::serviceName;
	bool Service::workerTerminated;
	Service *Service::serviceThis;

	Service::Service() {
		serviceThis = this;
		init();
	};

	void Service::serviceWork(){

	};

	void Service::init() {
		memset(&status, 0, sizeof(SERVICE_STATUS));
		statusHandle = NULL;

		serviceName = "XYO-WinService";

		workerTerminated = false;
	};

	void Service::worker() {

		HRESULT hres;

		hres = CoInitializeEx(0, COINIT_MULTITHREADED);
		if (FAILED(hres)) {
			return;
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

		if (FAILED(hres)) {
			return;
		};

		serviceThis->serviceWork();

		CoUninitialize();
	};

	VOID WINAPI Service::serviceCtrlHandler(DWORD ctrlCode) {
		if (ctrlCode == SERVICE_CONTROL_STOP) {

			status.dwControlsAccepted = 0;
			status.dwCurrentState = SERVICE_STOP_PENDING;
			status.dwWin32ExitCode = 0;
			status.dwCheckPoint = 4;

			SetServiceStatus(statusHandle, &status);
			serviceStopEvent.notify();
		};
	};

	VOID WINAPI Service::serviceMain(DWORD argc, LPTSTR *argv) {
		RegistryThread::threadBegin();

		statusHandle = RegisterServiceCtrlHandler(serviceName, serviceCtrlHandler);

		if (statusHandle == NULL) {
			RegistryThread::threadEnd();
			return;
		};

		memset(&status, 0, sizeof(SERVICE_STATUS));
		status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		status.dwControlsAccepted = 0;
		status.dwCurrentState = SERVICE_START_PENDING;
		status.dwWin32ExitCode = 0;
		status.dwServiceSpecificExitCode = 0;
		status.dwCheckPoint = 0;

		SetServiceStatus(statusHandle, &status);

		status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
		status.dwCurrentState = SERVICE_RUNNING;
		status.dwWin32ExitCode = 0;
		status.dwCheckPoint = 0;

		SetServiceStatus(statusHandle, &status);

		worker();

		status.dwControlsAccepted = 0;
		status.dwCurrentState = SERVICE_STOPPED;
		status.dwWin32ExitCode = 0;
		status.dwCheckPoint = 3;

		SetServiceStatus(statusHandle, &status);

		RegistryThread::threadEnd();
	};

	void Service::install() {
		String cmd;

		char executablePath[MAX_PATH];
		GetModuleFileName(NULL, executablePath, MAX_PATH);

		cmd = "sc stop \"";
		cmd << serviceName;
		cmd << "\"";

		XYO::System::Shell::executeHidden(cmd);

		cmd = "sc delete \"";
		cmd << serviceName;
		cmd << "\"";

		XYO::System::Shell::executeHidden(cmd);

		cmd = "sc create \"";
		cmd << serviceName;
		cmd << "\" binPath= \"";
		cmd << executablePath;
		cmd << " --service\"";

		XYO::System::Shell::executeHidden(cmd);

		cmd = "sc config \"";
		cmd << serviceName;
		cmd << "\" start= auto";

		XYO::System::Shell::executeHidden(cmd);
	};

	void Service::uninstall() {
		String cmd;

		cmd = "sc stop \"";
		cmd << serviceName;
		cmd << "\"";

		XYO::System::Shell::executeHidden(cmd);

		cmd = "sc delete \"";
		cmd << serviceName;
		cmd << "\"";

		XYO::System::Shell::executeHidden(cmd);
	};

	void Service::start() {
		String cmd;

		cmd = "sc start \"";
		cmd << serviceName;
		cmd << "\"";

		XYO::System::Shell::executeHidden(cmd);
	};

	void Service::stop() {
		String cmd;

		cmd = "sc stop \"";
		cmd << serviceName;
		cmd << "\"";

		XYO::System::Shell::executeHidden(cmd);
	};

	void Service::workerThread(void *) {
		worker();
		workerTerminated = true;
	};

	int Service::main(int cmdN, char *cmdS[]) {

		int i;
		char *opt;

		for (i = 1; i < cmdN; ++i) {
			if (strncmp(cmdS[i], "--", 2) == 0) {
				opt = &cmdS[i][2];
				if (strcmp(opt, "install") == 0) {
					install();
					continue;
				};
				if (strcmp(opt, "uninstall") == 0) {
					uninstall();
					continue;
				};
				if (strcmp(opt, "start") == 0) {
					start();
					continue;
				};
				if (strcmp(opt, "stop") == 0) {
					stop();
					continue;
				};
				if (strcmp(opt, "run") == 0) {
					printf("Press 'q' to end service\n");
					Thread thread;
					workerTerminated = false;
					thread.start(workerThread, NULL);
					while (!workerTerminated) {
						WaitForSingleObject(GetCurrentThread(), 100);
						if (Console::keyHit()) {
							if (Console::getChar() == 'q') {
								break;
							};
						};
					};
					serviceStopEvent.notify();
					while (!workerTerminated) {
						WaitForSingleObject(GetCurrentThread(), 100);
					};
					thread.join();
					return 0;
				};
				if (strcmp(opt, "service") == 0) {
					SERVICE_TABLE_ENTRY serviceTable[] = {
					    {(LPSTR)serviceName, (LPSERVICE_MAIN_FUNCTION)serviceMain},
					    {NULL, NULL}};

					if (StartServiceCtrlDispatcher(serviceTable) == FALSE) {
						return GetLastError();
					};
					return 0;
				};
				continue;
			};
		};

		return 1;
	};

};
