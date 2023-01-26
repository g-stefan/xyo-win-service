// Created by Grigore Stefan <g_stefan@yahoo.com>
// Public domain (Unlicense) <http://unlicense.org>
// SPDX-FileCopyrightText: 2016-2023 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: Unlicense

#include <XYO/WinService.hpp>

using namespace XYO::WinService;

class TestService : public virtual Service {
		XYO_DISALLOW_COPY_ASSIGN_MOVE(TestService);

	public:
		TestService();
		void serviceWork();
		int main(int cmdN, char *cmdS[]);
};

TestService::TestService() {
	serviceName = "TestService";
};

void TestService::serviceWork() {
	while (!serviceStopEvent.peek()) {
		Thread::sleep(1000);
	};
};

int TestService::main(int cmdN, char *cmdS[]) {
	if (cmdN == 1) {
		char *cmdSX[2] = {cmdS[0], "--run"};
		return Service::main(2, cmdSX);
	};
	return Service::main(cmdN, cmdS);
};

XYO_APPLICATION_MAIN(TestService);
