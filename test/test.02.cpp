// Created by Grigore Stefan <g_stefan@yahoo.com>
// Public domain (Unlicense) <http://unlicense.org>
// SPDX-FileCopyrightText: 2016-2026 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: Unlicense

// Service in console mode (--run), no administrator rights required:
// worker sequence, exit codes, stop, command line and SCM command errors

#include <XYO/WinService.hpp>

using namespace XYO::WinService;

enum {
	ModeWorkReturns,
	ModeInitFails,
	ModeWorkThrows,
	ModeCOMFails,
	ModeWaitStop
};

class TestService : public virtual Service {
		XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(TestService);

	public:
		static int mode;
		static int initCalls;
		static int workCalls;
		static int comInitializeCalls;
		static int comUninitializeCalls;
		static int stopPolls;
		static bool stopSeenAtStart;
		static bool stopSeenAtEnd;

		TestService();
		DWORD serviceInit();
		void serviceWork();
		HRESULT serviceCOMInitialize();
		void serviceCOMUninitialize();
		int main(int cmdN, char *cmdS[]);

		static void resetCounters(int mode_);
		static bool stopAfterPolls();
};

int TestService::mode = ModeWorkReturns;
int TestService::initCalls = 0;
int TestService::workCalls = 0;
int TestService::comInitializeCalls = 0;
int TestService::comUninitializeCalls = 0;
int TestService::stopPolls = 0;
bool TestService::stopSeenAtStart = false;
bool TestService::stopSeenAtEnd = false;

TestService::TestService() {
	serviceName = "XYO-WinService-Test02-NotInstalled";
};

void TestService::resetCounters(int mode_) {
	mode = mode_;
	initCalls = 0;
	workCalls = 0;
	comInitializeCalls = 0;
	comUninitializeCalls = 0;
	stopPolls = 0;
	stopSeenAtStart = false;
	stopSeenAtEnd = false;
	stopRun = nullptr;
};

bool TestService::stopAfterPolls() {
	++stopPolls;
	return stopPolls >= 3;
};

DWORD TestService::serviceInit() {
	++initCalls;
	if (mode == ModeInitFails) {
		return ERROR_INVALID_DATA;
	};
	return NO_ERROR;
};

void TestService::serviceWork() {
	++workCalls;
	stopSeenAtStart = serviceStopEvent.peek();
	if (mode == ModeWorkThrows) {
		throw 1;
	};
	if (mode == ModeWaitStop) {
		// Bounded, a missing stop must fail the test, not hang it
		for (int k = 0; k < 1000; ++k) {
			if (serviceStopEvent.peek()) {
				break;
			};
			Thread::sleep(10);
		};
		stopSeenAtEnd = serviceStopEvent.peek();
	};
};

HRESULT TestService::serviceCOMInitialize() {
	++comInitializeCalls;
	if (mode == ModeCOMFails) {
		return E_FAIL;
	};
	return Service::serviceCOMInitialize();
};

void TestService::serviceCOMUninitialize() {
	++comUninitializeCalls;
	Service::serviceCOMUninitialize();
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

int TestService::main(int, char *cmdS[]) {
	int retV;

	// Normal run, default COM initialization
	resetCounters(ModeWorkReturns);
	retV = cmdRun();
	check(retV == 0, "run: returns 0");
	check(initCalls == 1 && workCalls == 1, "run: serviceInit and serviceWork called once");
	check(comInitializeCalls == 1 && comUninitializeCalls == 1, "run: COM initialize / uninitialize balanced");

	// Second run in the same process, CoInitializeSecurity returns RPC_E_TOO_LATE
	resetCounters(ModeWorkReturns);
	retV = cmdRun();
	check(retV == 0 && workCalls == 1, "run again: COM security already set is not an error");

	// serviceInit failure is returned, serviceWork not called
	resetCounters(ModeInitFails);
	retV = cmdRun();
	check(retV == ERROR_INVALID_DATA, "init fails: returns its error code");
	check(workCalls == 0, "init fails: serviceWork not called");
	check(comUninitializeCalls == 1, "init fails: COM uninitialized");

	// Exception in serviceWork
	resetCounters(ModeWorkThrows);
	retV = cmdRun();
	check(retV == ERROR_EXCEPTION_IN_SERVICE, "work throws: returns ERROR_EXCEPTION_IN_SERVICE");
	check(comUninitializeCalls == 1, "work throws: COM uninitialized");

	// COM initialization failure
	resetCounters(ModeCOMFails);
	retV = cmdRun();
	check(retV == static_cast<int>(E_FAIL), "COM fails: returns the HRESULT");
	check(initCalls == 0 && workCalls == 0, "COM fails: serviceInit and serviceWork not called");
	check(comUninitializeCalls == 0, "COM fails: COM uninitialize not called");

	// stopRun() ends the run through serviceStopEvent
	resetCounters(ModeWaitStop);
	stopRun = stopAfterPolls;
	retV = cmdRun();
	check(retV == 0, "stopRun: returns 0");
	check(stopPolls >= 3, "stopRun: polled until it returned true");
	check(stopSeenAtEnd, "stopRun: serviceWork saw serviceStopEvent");

	// The stop of the previous run does not leak into the next one
	resetCounters(ModeWorkReturns);
	retV = cmdRun();
	check(retV == 0 && !stopSeenAtStart, "run after stop: serviceStopEvent reset");

	// Command line
	char option0[] = "test.02";
	char optionUnknown[] = "--unknown";
	char optionPlain[] = "plain";
	char optionRun[] = "--run";
	char optionStop[] = "--stop";

	char *cmdNone[] = {option0};
	check(Service::main(1, cmdNone) == 1, "main: no command returns 1");

	char *cmdUnknown[] = {option0, optionUnknown, optionPlain};
	check(Service::main(3, cmdUnknown) == 1, "main: unknown options ignored, no command returns 1");

	resetCounters(ModeWorkReturns);
	char *cmdRunArgs[] = {option0, optionUnknown, optionRun};
	check(Service::main(3, cmdRunArgs) == 0 && workCalls == 1, "main: --run after unknown option runs");

	// SCM commands on a service that is not installed, no rights needed
	check(cmdStop() == ERROR_SERVICE_DOES_NOT_EXIST, "stop: not installed returns ERROR_SERVICE_DOES_NOT_EXIST");
	check(cmdStart() == ERROR_SERVICE_DOES_NOT_EXIST, "start: not installed returns ERROR_SERVICE_DOES_NOT_EXIST");
	check(cmdUninstall() == NO_ERROR, "uninstall: not installed is not an error");

	char *cmdStopArgs[] = {option0, optionStop};
	check(Service::main(2, cmdStopArgs) == ERROR_SERVICE_DOES_NOT_EXIST, "main: --stop failure returned as exit code");

	// Status reporting and control handler outside the SCM
	reportStatus(SERVICE_RUNNING, NO_ERROR, 0, 0);
	reportPending();
	check(status.dwCurrentState == 0, "reportStatus: no-op when not running as service");

	serviceStopEvent.reset();
	check(serviceCtrlHandler(SERVICE_CONTROL_STOP, 0, NULL, NULL) == NO_ERROR, "handler: stop returns NO_ERROR");
	check(!serviceStopEvent.peek(), "handler: stop ignored when service is not running");
	check(serviceCtrlHandler(SERVICE_CONTROL_INTERROGATE, 0, NULL, NULL) == NO_ERROR, "handler: interrogate returns NO_ERROR");
	check(serviceCtrlHandler(SERVICE_CONTROL_PAUSE, 0, NULL, NULL) == ERROR_CALL_NOT_IMPLEMENTED, "handler: pause not implemented");

	if (failures) {
		printf("%s: %d test(s) failed\n", cmdS[0], failures);
		return 1;
	};
	printf("%s: all tests passed\n", cmdS[0]);
	return 0;
};

XYO_APPLICATION_MAIN(TestService);
