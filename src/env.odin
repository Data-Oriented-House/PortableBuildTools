package PortableBuildTools

import "core:fmt"
import "core:slice"
import "core:strings"

import win32 "core:sys/windows"

// An environment variable has a maximum size limit of 32,767 characters, including the null-terminating character.
MAX_ENV_LEN :: 32_767

LOCAL_SUBKEY  := L(`Environment`)
GLOBAL_SUBKEY := L(`SYSTEM\CurrentControlSet\Control\Session Manager\Environment`)

Env_Location :: enum {
	Local,
	Global,
}

Env_Error :: enum {
	None,
	Unknown,
	Not_Found,
	Alloc,
	Access_Denied,
}

add_to_env_if_not_exists :: proc(env, s: string, allocator := context.allocator) -> string {
	env := env

	entries := make([dynamic]string, context.temp_allocator)
	for v in strings.split_iterator(&env, ";") do append(&entries, v)
	if !slice.contains(entries[:], s) do append(&entries, s)
	return strings.join(entries[:], ";", allocator)
}

get_env_from_registry :: proc(env: string, location: Env_Location, allocator := context.temp_allocator) -> (string, Env_Error) {
	env_buf: [MAX_ENV_LEN]win32.WCHAR
	env_size := win32.DWORD(size_of(win32.WCHAR) * MAX_ENV_LEN)

	res := win32.RegGetValueW(
		win32.HKEY_CURRENT_USER if location == .Local else win32.HKEY_LOCAL_MACHINE,
		LOCAL_SUBKEY if location == .Local else GLOBAL_SUBKEY,
		win32.utf8_to_wstring(env), win32.RRF_RT_REG_EXPAND_SZ|win32.RRF_NOEXPAND, nil,
		&env_buf[0], &env_size,
	)
	if res != auto_cast win32.NO_ERROR {
		return "", .Not_Found if res == auto_cast win32.ERROR_FILE_NOT_FOUND else .Unknown
	}

	env_string, err := win32.wstring_to_utf8(&env_buf[0], len(env_buf), allocator)
	if err != .None {
		return "", .Alloc
	}

	return env_string, .None
}

set_env_in_registry :: proc(env, value: string, location: Env_Location, expand := false) -> Env_Error {
	value_utf16 := win32.utf8_to_utf16(value)
	if value_utf16 == nil {
		return .Alloc
	}

	value_size := win32.DWORD(size_of(win32.WCHAR) * len(value_utf16))

	res := win32.RegSetKeyValueW(
		win32.HKEY_CURRENT_USER if location == .Local else win32.HKEY_LOCAL_MACHINE,
		LOCAL_SUBKEY if location == .Local else GLOBAL_SUBKEY,
		win32.utf8_to_wstring(env), win32.REG_EXPAND_SZ if expand else win32.REG_SZ, raw_data(value_utf16), value_size,
	)
	if res != win32.NO_ERROR {
		fmt.eprintf("error setting value: {:x}\n", res)
		return .Access_Denied if res == auto_cast win32.ERROR_ACCESS_DENIED else .Unknown
	}

	return .None
}
