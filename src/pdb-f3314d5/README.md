# pdb
Reads Microsoft PDB (Program Database) files. Enables stacktracing on Windows for The Odin Programming Language.

## Demo

Run `demo.odin` with `odin run demo.odin -file -debug` to check the result:

```log
H:/projects/pdbReader/demo.odin:23:18: Index 8 is out of range 0..<8
Stacktrace:12
C:\WINDOWS\System32\KERNELBASE.dll:0:0:()
C:\programs\odin-windows_amd64_dev-2022-05\core\runtime\procs_windows_amd64.odin:15:2:runtime.windows_trap_array_bounds-694()
C:\programs\odin-windows_amd64_dev-2022-05\core\runtime\error_checks.odin:5:3:runtime.bounds_trap()
C:\programs\odin-windows_amd64_dev-2022-05\core\runtime\error_checks.odin:31:3:runtime.bounds_check_error.handle_error-0()
C:\programs\odin-windows_amd64_dev-2022-05\core\runtime\error_checks.odin:19:95:runtime.bounds_check_error()
H:\projects\pdbReader\demo.odin:23:9:demo.bar()
H:\projects\pdbReader\demo.odin:15:5:demo.foo()
H:\projects\pdbReader\demo.odin:6:15:demo.main()
C:\programs\odin-windows_amd64_dev-2022-05\core\runtime\entry_windows.odin:40:15:main()
C:\WINDOWS\System32\KERNEL32.DLL:0:0:()
C:\WINDOWS\SYSTEM32\ntdll.dll:0:0:()
```
## Usage

Add the library to your project, then put `pdb.SetUnhandledExceptionFilter(pdb.dump_stack_trace_on_exception)` on top of your `main` function.

The custom handler `pdb.dump_stack_trace_on_exception` loads debug info, parses the stacktrace, and dumps source line information if possible.

You need to build the project with the `-debug` flag in order to generate pdb files for the executable.

## Limitations

- Inlined procedual information might be incorrect when compiled with `-o:speed`. I'm not sure if this is because of llvm generating incorrect debug infos or because of some bugs on my side.
- No OMAP-based address translation at the moment. Odin compiler doesn't seem to generate any executable requiring it, so this shouldn't be a big issue right now.

This is not intended to be a full-featured pdb-library. I only grabbed what's necessary to make developing with Odin on Windows a bit more managable. I'd recommend looking into [llvm's documentation of pdb](https://llvm.org/docs/PDB/index.html),  [willglynn's well-documented pdb parse in Rust](https://github.com/willglynn/pdb) as well as [Microsoft's pdb repo](https://github.com/Microsoft/microsoft-pdb) if you're looking for a more feature-complete pdb parser.

Hope you'd find this useful. Happy coding!
