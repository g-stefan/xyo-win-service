# Simple Windows Service

C++ library
- Turns an XYO console application into a Windows service: derive from `Service`,
put the work in `serviceWork()`, and the same executable installs, starts, stops
and uninstalls itself.
- Command line: `--install`, `--uninstall`, `--start`, `--stop` (wait for the result,
return the Win32 error), `--run` (the same service code in the console, for
development and debugging), `--service` (used by the Service Control Manager).
- Handles the SCM protocol: pending states with check points, stop / shutdown,
failed starts reported with their error code, exceptions caught, COM initialized
on the service thread.

Built on `xyo-win`. Windows only.

```cpp
class MyService : public virtual Service {
	...
	MyService() { serviceName = "MyService"; };
	void serviceWork() { serviceStopEvent.wait(); };
};

XYO_APPLICATION_MAIN(MyService);
```

## Documentation

- [Overview](docs/README.md) - purpose and design
- [Getting started](docs/getting-started.md) - build, depend on it, first service, try it, install it
- [Writing a service](docs/writing-a-service.md) - life cycle, hooks, stopping, long start, errors, COM, threads
- [Command line and deployment](docs/command-line.md) - every option, rights, the service environment, `sc.exe`, troubleshooting
- [API reference](docs/reference.md)

A Claude Code skill for this library is in
[.claude/skills/xyo-win-service](.claude/skills/xyo-win-service/SKILL.md).

## License

Copyright (c) 2023-2026 Grigore Stefan
Licensed under the [MIT](LICENSE) license.
