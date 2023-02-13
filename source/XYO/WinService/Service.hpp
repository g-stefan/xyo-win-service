// Win Service
// Copyright (c) 2023 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_WINSERVICE_LIBRARY_HPP
#define XYO_WINSERVICE_LIBRARY_HPP

#ifndef XYO_WINSERVICE_DEPENDENCY_HPP
#	include <XYO/WinService/Dependency.hpp>
#endif

namespace XYO::WinService {

	class Service : public virtual IApplication {
			XYO_DISALLOW_COPY_ASSIGN_MOVE(Service);

		public:
			typedef bool (*StopRun)();

			XYO_WINSERVICE_EXPORT static const char *serviceName;
			XYO_WINSERVICE_EXPORT static StopRun stopRun;
			XYO_WINSERVICE_EXPORT static Multithreading::Semaphore serviceStopEvent;
			XYO_WINSERVICE_EXPORT virtual void serviceWork();
			//
			XYO_WINSERVICE_EXPORT static SERVICE_STATUS status;
			XYO_WINSERVICE_EXPORT static SERVICE_STATUS_HANDLE statusHandle;
			XYO_WINSERVICE_EXPORT static bool workerTerminated;
			XYO_WINSERVICE_EXPORT static Service *serviceThis;
			//
			XYO_WINSERVICE_EXPORT static void init();
			XYO_WINSERVICE_EXPORT static VOID WINAPI serviceMain(DWORD argc, LPTSTR *argv);
			XYO_WINSERVICE_EXPORT static VOID WINAPI serviceCtrlHandler(DWORD);
			XYO_WINSERVICE_EXPORT static void worker();
			XYO_WINSERVICE_EXPORT static void cmdInstall();
			XYO_WINSERVICE_EXPORT static void cmdUninstall();
			XYO_WINSERVICE_EXPORT static void cmdStart();
			XYO_WINSERVICE_EXPORT static void cmdStop();
			XYO_WINSERVICE_EXPORT static void cmdRun();
			XYO_WINSERVICE_EXPORT static int cmdService();
			XYO_WINSERVICE_EXPORT static void workerThread(void *);
			//
			XYO_WINSERVICE_EXPORT Service();
			XYO_WINSERVICE_EXPORT int main(int cmdN, char *cmdS[]);
	};

};

#endif
