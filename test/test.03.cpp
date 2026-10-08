// Created by Grigore Stefan <g_stefan@yahoo.com>
// Public domain (Unlicense) <http://unlicense.org>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: Unlicense

// Service Control Manager integration: install, start, stop, reinstall,
// uninstall a real service. Requires administrator rights, skipped otherwise.
// The same executable is the service, started by the SCM with --service.

#include <XYO/WinService.hpp>

#include <string>

using namespace XYO::WinService;

static const char *testServiceName = "XYO-WinService-Test03";
static const char *testDescription = "XYO WinService test, safe to remove";

class TestService : public virtual Service {
		XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(TestService);

	public:
		TestService();
		DWORD serviceInit();
		void serviceWork();
		int main(int cmdN, char *cmdS[]);
};

TestService::TestService() {
	serviceName = testServiceName;
};

static std::string getExecutablePath() {
	char buffer[MAX_PATH];
	DWORD length = GetModuleFileNameA(NULL, buffer, MAX_PATH);
	if (length == 0 || length >= MAX_PATH) {
		return std::string();
	};
	return std::string(buffer, length);
};

// Start fails while this file exists
static std::string getFailInitMarker() {
	return getExecutablePath() + ".fail-init";
};

DWORD TestService::serviceInit() {
	if (GetFileAttributesA(getFailInitMarker().c_str()) != INVALID_FILE_ATTRIBUTES) {
		return ERROR_INVALID_DATA;
	};
	return NO_ERROR;
};

void TestService::serviceWork() {
	serviceStopEvent.wait();
};

static int failures = 0;

static void check(bool condition, const char *name) {
	if (condition) {
		printf("[ OK ] %s\n", name);
		return;
	};
	printf("[FAIL] %s\n", name);
	++failures;
};

static bool queryStatus(SERVICE_STATUS_PROCESS &svcStatus) {
	bool retV = false;
	SC_HANDLE scManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
	if (scManager != NULL) {
		SC_HANDLE scService = OpenServiceA(scManager, testServiceName, SERVICE_QUERY_STATUS);
		if (scService != NULL) {
			DWORD bytesNeeded;
			retV = QueryServiceStatusEx(scService, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&svcStatus), sizeof(svcStatus), &bytesNeeded) != FALSE;
			CloseServiceHandle(scService);
		};
		CloseServiceHandle(scManager);
	};
	return retV;
};

static bool setDescription(const char *description) {
	bool retV = false;
	SC_HANDLE scManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
	if (scManager != NULL) {
		SC_HANDLE scService = OpenServiceA(scManager, testServiceName, SERVICE_CHANGE_CONFIG);
		if (scService != NULL) {
			SERVICE_DESCRIPTIONA info;
			info.lpDescription = const_cast<LPSTR>(description);
			retV = ChangeServiceConfig2A(scService, SERVICE_CONFIG_DESCRIPTION, &info) != FALSE;
			CloseServiceHandle(scService);
		};
		CloseServiceHandle(scManager);
	};
	return retV;
};

static std::string getDescription() {
	std::string retV;
	SC_HANDLE scManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CONNECT);
	if (scManager != NULL) {
		SC_HANDLE scService = OpenServiceA(scManager, testServiceName, SERVICE_QUERY_CONFIG);
		if (scService != NULL) {
			BYTE buffer[8192];
			DWORD bytesNeeded;
			if (QueryServiceConfig2A(scService, SERVICE_CONFIG_DESCRIPTION, buffer, sizeof(buffer), &bytesNeeded)) {
				SERVICE_DESCRIPTIONA *info = reinterpret_cast<SERVICE_DESCRIPTIONA *>(buffer);
				if (info->lpDescription != NULL) {
					retV = info->lpDescription;
				};
			};
			CloseServiceHandle(scService);
		};
		CloseServiceHandle(scManager);
	};
	return retV;
};

// The service process does not inherit this PATH, give it the one used to
// find the test DLLs (output/bin, SDK bin). Removed with the service key.
static bool setServicePath() {
	char path[32767];
	DWORD length = GetEnvironmentVariableA("PATH", path, sizeof(path));
	if (length == 0 || length >= sizeof(path)) {
		return false;
	};
	std::string environment = "PATH=";
	environment.append(path, length);
	environment.push_back('\0');
	environment.push_back('\0');

	std::string key = "SYSTEM\\CurrentControlSet\\Services\\";
	key += testServiceName;
	return RegSetKeyValueA(HKEY_LOCAL_MACHINE, key.c_str(), "Environment", REG_MULTI_SZ, environment.data(), static_cast<DWORD>(environment.size())) == ERROR_SUCCESS;
};

static bool isAdministrator() {
	SC_HANDLE scManager = OpenSCManagerA(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
	if (scManager == NULL) {
		return false;
	};
	CloseServiceHandle(scManager);
	return true;
};

int TestService::main(int cmdN, char *cmdS[]) {
	SERVICE_STATUS_PROCESS svcStatus;

	// Started by the SCM
	for (int i = 1; i < cmdN; ++i) {
		if (strncmp(cmdS[i], "--", 2) == 0) {
			return Service::main(cmdN, cmdS);
		};
	};

	if (!isAdministrator()) {
		printf("%s: skipped, requires administrator rights\n", cmdS[0]);
		return 0;
	};

	DeleteFileA(getFailInitMarker().c_str());
	cmdUninstall();

	// Install and start
	check(cmdInstall() == NO_ERROR, "install");
	check(setServicePath(), "set service PATH");
	check(cmdStart() == NO_ERROR, "start");
	check(queryStatus(svcStatus) && svcStatus.dwCurrentState == SERVICE_RUNNING, "start: running");
	check((svcStatus.dwControlsAccepted & SERVICE_ACCEPT_STOP) != 0, "start: accepts stop");
	check((svcStatus.dwControlsAccepted & SERVICE_ACCEPT_SHUTDOWN) != 0, "start: accepts shutdown");
	check(svcStatus.dwCheckPoint == 0, "start: check point 0 when running");
	check(cmdStart() == NO_ERROR, "start: already running is not an error");

	// Stop
	check(cmdStop() == NO_ERROR, "stop");
	check(queryStatus(svcStatus) && svcStatus.dwCurrentState == SERVICE_STOPPED, "stop: stopped");
	check(svcStatus.dwWin32ExitCode == NO_ERROR, "stop: exit code 0");
	check(svcStatus.dwCheckPoint == 0, "stop: check point 0 when stopped");
	check(cmdStop() == NO_ERROR, "stop: already stopped is not an error");

	// A failed start is reported with its exit code
	HANDLE marker = CreateFileA(getFailInitMarker().c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (marker != INVALID_HANDLE_VALUE) {
		CloseHandle(marker);
	};
	check(cmdStart() == ERROR_INVALID_DATA, "start fails: returns the service exit code");
	check(queryStatus(svcStatus) && svcStatus.dwCurrentState == SERVICE_STOPPED, "start fails: stopped");
	check(svcStatus.dwWin32ExitCode == ERROR_INVALID_DATA, "start fails: exit code reported to SCM");
	DeleteFileA(getFailInitMarker().c_str());

	// Reinstall while running updates in place, administrator settings kept
	check(setDescription(testDescription), "set description");
	check(cmdStart() == NO_ERROR, "start before reinstall");
	check(cmdInstall() == NO_ERROR, "reinstall while running");
	check(queryStatus(svcStatus) && svcStatus.dwCurrentState == SERVICE_STOPPED, "reinstall: service stopped");
	check(getDescription() == testDescription, "reinstall: description kept");
	check(cmdStart() == NO_ERROR, "reinstall: starts");

	// Uninstall stops and removes
	check(cmdUninstall() == NO_ERROR, "uninstall while running");
	check(!queryStatus(svcStatus), "uninstall: service removed");
	check(cmdUninstall() == NO_ERROR, "uninstall: not installed is not an error");
	check(cmdStop() == ERROR_SERVICE_DOES_NOT_EXIST, "stop: not installed returns ERROR_SERVICE_DOES_NOT_EXIST");

	if (failures) {
		cmdUninstall();
		printf("%s: %d test(s) failed\n", cmdS[0], failures);
		return 1;
	};
	printf("%s: all tests passed\n", cmdS[0]);
	return 0;
};

XYO_APPLICATION_MAIN(TestService);
