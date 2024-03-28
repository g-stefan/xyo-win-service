// Created by Grigore Stefan <g_stefan@yahoo.com>
// Public domain (Unlicense) <http://unlicense.org>
// SPDX-FileCopyrightText: 2016-2024 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: Unlicense

#include <XYO/WinService.hpp>

using namespace XYO::WinService;

class TestService : public virtual Service {
		XYO_PLATFORM_DISALLOW_COPY_ASSIGN_MOVE(TestService);

	public:
		TestService();
		void serviceWork();
		int main(int cmdN, char *cmdS[]);
		static bool doStop();
};

TestService::TestService() {
	serviceName = "TestService";
	stopRun = doStop;
};

void TestService::serviceWork() {
	int count = 1;
	printf("Begin\r\n");
	while (!serviceStopEvent.peek()) {
		printf("%d\r", count);
		Thread::sleep(1000);
		++count;
		if (count >= 10) {
			break;
		};
	};
	printf("\nDone.\r\n");
};

bool TestService::doStop() {
	if (Console::keyHit()) {
		if (Console::getChar() == 'q') {
			return true;
		};
	};
	return false;
};

int TestService::main(int cmdN, char *cmdS[]) {
	if (cmdN == 1) {
		printf("Press 'q' to stop service\n");
		char *cmdSX[2] = {cmdS[0], "--run"};
		return Service::main(2, cmdSX);
	};
	return Service::main(cmdN, cmdS);
};

XYO_APPLICATION_MAIN(TestService);
