// Win Service
// Copyright (c) 2023 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#include <XYO/WinService/Copyright.hpp>
#include <XYO/WinService/Copyright.rh>

namespace XYO::WinService::Copyright {

	static const char *copyright_ = XYO_WINSERVICE_COPYRIGHT;
	static const char *publisher_ = XYO_WINSERVICE_PUBLISHER;
	static const char *company_ = XYO_WINSERVICE_COMPANY;
	static const char *contact_ = XYO_WINSERVICE_CONTACT;

	std::string copyright() {
		return copyright_;
	};

	std::string publisher() {
		return publisher_;
	};

	std::string company() {
		return company_;
	};

	std::string contact() {
		return contact_;
	};

};
