// Win Service
// Copyright (c) 2023 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_WINSERVICE_LICENSE_HPP
#define XYO_WINSERVICE_LICENSE_HPP

#ifndef XYO_WINSERVICE_DEPENDENCY_HPP
#	include <XYO/WinService/Dependency.hpp>
#endif

namespace XYO::WinService::License {

	XYO_WINSERVICE_EXPORT std::string license();
	XYO_WINSERVICE_EXPORT std::string shortLicense();

};

#endif
