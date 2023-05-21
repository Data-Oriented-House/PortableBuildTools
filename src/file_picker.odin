package PortableBuildTools

import "core:mem"
import win32 "core:sys/windows"

OK :: win32.SUCCEEDED

// randomly generated
PROGRAM_GUID: win32.GUID = {0x65CDF695, 0x0A2C, 0x46DB, {0x9F, 0xE8, 0x73, 0xCF, 0xA7, 0xF7, 0x1E, 0x65}}
// TODO: generate different GUIDs for open/save/etc dialogs, pass them to these functions from main

_get_name_from_shellitem :: proc(si: ^win32.IShellItem, allocator := context.temp_allocator) -> (name: string, succeeded: bool) {
	name_wstring: win32.LPWSTR
	OK(si->GetDisplayName(.DESKTOPABSOLUTEPARSING, &name_wstring)) or_return
	defer win32.CoTaskMemFree(name_wstring)

	err: mem.Allocator_Error
	name, err = win32.wstring_to_utf8(name_wstring, -1, allocator)
	succeeded = (err == nil)
	return
}

_get_names_from_shellitemarray :: proc(sia: ^win32.IShellItemArray, allocator := context.temp_allocator) -> (names: []string, succeeded: bool) {
	items_len: win32.DWORD
	OK(sia->GetCount(&items_len)) or_return
	items_arr := make([dynamic]string, 0, items_len, allocator)
	defer {
		if !succeeded {
			for it in items_arr do delete(it, allocator)
			delete(items_arr)
		}
	}

	for i in 0..<items_len {
		si: ^win32.IShellItem
		OK(sia->GetItemAt(i, &si)) or_return
		defer si->Release()

		name := _get_name_from_shellitem(si, allocator) or_return
		append(&items_arr, name)
	}

	return items_arr[:], true
}

select_file_to_save :: proc(
	title := "",
	file_types: [][2]string = {{"All Files", "*.*"}},
	default_ext := "",
	allocator := context.temp_allocator,
) -> (file: string, succeeded: bool) {
	OK(win32.CoInitializeEx(nil, .APARTMENTTHREADED)) or_return
	defer win32.CoUninitialize()

	fd: ^win32.IFileSaveDialog
	OK(win32.CoCreateInstance(
		win32.CLSID_FileSaveDialog, nil, win32.CLSCTX_INPROC_SERVER,
		win32.IID_IFileSaveDialog, cast(^rawptr)(&fd),
	)) or_return
	defer fd->Release()
	OK(fd->SetClientGuid(PROGRAM_GUID)) or_return

	if len(title) > 0 {
		OK(fd->SetTitle(win32.utf8_to_wstring(title))) or_return
	}

	if len(file_types) > 0 {
		file_types_wstrings := make([]win32.COMDLG_FILTERSPEC, len(file_types), context.temp_allocator)
		for _, i in file_types_wstrings {
			wname := win32.utf8_to_wstring(file_types[i][0])
			wspec := win32.utf8_to_wstring(file_types[i][1])
			file_types_wstrings[i] = {wname, wspec}
		}
		OK(fd->SetFileTypes(auto_cast len(file_types_wstrings), raw_data(file_types_wstrings))) or_return
	}

	if len(default_ext) > 0 {
		OK(fd->SetDefaultExtension(win32.utf8_to_wstring(default_ext))) or_return
	}

	OK(fd->Show(nil)) or_return

	psi: ^win32.IShellItem
	OK(fd->GetResult(&psi)) or_return
	defer psi->Release()

	return _get_name_from_shellitem(psi, allocator)
}

select_file_to_open :: proc(
	title := "",
	file_types: [][2]string = {},
	allocator := context.temp_allocator,
) -> (file: string, succeeded: bool) {
	OK(win32.CoInitializeEx(nil, .APARTMENTTHREADED)) or_return
	defer win32.CoUninitialize()

	fd: ^win32.IFileOpenDialog
	OK(win32.CoCreateInstance(
		win32.CLSID_FileOpenDialog, nil, win32.CLSCTX_INPROC_SERVER,
		win32.IID_IFileOpenDialog, cast(^rawptr)(&fd),
	)) or_return
	defer fd->Release()
	OK(fd->SetClientGuid(PROGRAM_GUID)) or_return

	if len(title) > 0 {
		OK(fd->SetTitle(win32.utf8_to_wstring(title))) or_return
	}

	if len(file_types) > 0 {
		file_types_wstrings := make([]win32.COMDLG_FILTERSPEC, len(file_types), context.temp_allocator)
		for _, i in file_types_wstrings {
			wname := win32.utf8_to_wstring(file_types[i][0])
			wspec := win32.utf8_to_wstring(file_types[i][1])
			file_types_wstrings[i] = {wname, wspec}
		}
		OK(fd->SetFileTypes(auto_cast len(file_types_wstrings), raw_data(file_types_wstrings))) or_return
	}

	OK(fd->Show(nil)) or_return

	psi: ^win32.IShellItem
	OK(fd->GetResult(&psi)) or_return
	defer psi->Release()

	return _get_name_from_shellitem(psi, allocator)
}

select_files_to_open :: proc(
	title := "",
	file_types: [][2]string = {},
	allocator := context.temp_allocator,
) -> (files: []string, succeeded: bool) {
	OK(win32.CoInitializeEx(nil, .APARTMENTTHREADED)) or_return
	defer win32.CoUninitialize()

	fd: ^win32.IFileOpenDialog
	OK(win32.CoCreateInstance(
		win32.CLSID_FileOpenDialog, nil, win32.CLSCTX_INPROC_SERVER,
		win32.IID_IFileOpenDialog, cast(^rawptr)(&fd),
	)) or_return
	defer fd->Release()
	OK(fd->SetClientGuid(PROGRAM_GUID)) or_return

	if len(title) > 0 {
		OK(fd->SetTitle(win32.utf8_to_wstring(title))) or_return
	}

	if len(file_types) > 0 {
		file_types_wstrings := make([]win32.COMDLG_FILTERSPEC, len(file_types), context.temp_allocator)
		for _, i in file_types_wstrings {
			wname := win32.utf8_to_wstring(file_types[i][0])
			wspec := win32.utf8_to_wstring(file_types[i][1])
			file_types_wstrings[i] = {wname, wspec}
		}
		OK(fd->SetFileTypes(auto_cast len(file_types_wstrings), raw_data(file_types_wstrings))) or_return
	}

	options: win32.DWORD
	if OK(fd->GetOptions(&options)) {
		fd->SetOptions(options | win32.FOS_ALLOWMULTISELECT)
	}

	OK(fd->Show(nil)) or_return

	psi: ^win32.IShellItemArray
	OK(fd->GetResults(&psi)) or_return
	defer psi->Release()

	return _get_names_from_shellitemarray(psi, allocator)
}

select_folder :: proc(title := "", allocator := context.temp_allocator) -> (folder: string, succeeded: bool) {
	OK(win32.CoInitializeEx(nil, .APARTMENTTHREADED)) or_return
	defer win32.CoUninitialize()

	fd: ^win32.IFileOpenDialog
	OK(win32.CoCreateInstance(
		win32.CLSID_FileOpenDialog, nil, win32.CLSCTX_INPROC_SERVER, // TODO: what is this?
		win32.IID_IFileOpenDialog, cast(^rawptr)(&fd),
	)) or_return
	defer fd->Release()
	OK(fd->SetClientGuid(PROGRAM_GUID)) or_return

	if len(title) > 0 {
		OK(fd->SetTitle(win32.utf8_to_wstring(title))) or_return
	}

	options: win32.DWORD
	if OK(fd->GetOptions(&options)) {
		fd->SetOptions(options | win32.FOS_PICKFOLDERS)
	}

	OK(fd->Show(nil)) or_return

	psi: ^win32.IShellItem
	OK(fd->GetResult(&psi)) or_return
	defer psi->Release()

	return _get_name_from_shellitem(psi, allocator)
}

select_folders :: proc(title := "", allocator := context.temp_allocator) -> (folders: []string, succeeded: bool) {
	OK(win32.CoInitializeEx(nil, .APARTMENTTHREADED)) or_return
	defer win32.CoUninitialize()

	fd: ^win32.IFileOpenDialog
	OK(win32.CoCreateInstance(
		win32.CLSID_FileOpenDialog, nil, win32.CLSCTX_INPROC_SERVER,
		win32.IID_IFileOpenDialog, cast(^rawptr)(&fd),
	)) or_return
	defer fd->Release()
	OK(fd->SetClientGuid(PROGRAM_GUID)) or_return

	if len(title) > 0 {
		OK(fd->SetTitle(win32.utf8_to_wstring(title))) or_return
	}

	options: win32.DWORD
	if OK(fd->GetOptions(&options)) {
		fd->SetOptions(options | win32.FOS_ALLOWMULTISELECT | win32.FOS_PICKFOLDERS)
	}

	OK(fd->Show(nil)) or_return

	psi: ^win32.IShellItemArray
	OK(fd->GetResults(&psi)) or_return
	defer psi->Release()

	return _get_names_from_shellitemarray(psi, allocator)
}
