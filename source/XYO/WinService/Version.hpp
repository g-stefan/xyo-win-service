// Win Service
// Copyright (c) 2023-2024 Grigore Stefan <g_stefan@yahoo.com>
// MIT License (MIT) <http://opensource.org/licenses/MIT>
// SPDX-FileCopyrightText: 2023-2024 Grigore Stefan <g_stefan@yahoo.com>
// SPDX-License-Identifier: MIT

#ifndef XYO_WINSERVICE_VERSION_HPP
#define XYO_WINSERVICE_VERSION_HPP

#ifndef XYO_WINSERVICE_DEPENDENCY_HPP
#	include <XYO/WinService/Dependency.hpp>
#endif

namespace XYO::WinService::Version {

	XYO_WINSERVICE_EXPORT const char *version();
	XYO_WINSERVICE_EXPORT const char *build();
	XYO_WINSERVICE_EXPORT const char *versionWithBuild();
	XYO_WINSERVICE_EXPORT const char *datetime();

};

#endif
