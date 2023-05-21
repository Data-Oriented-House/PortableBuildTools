// +build windows
package winhttp

import win32 "core:sys/windows"

foreign import winhttp "system:winhttp.lib"

HINTERNET :: distinct rawptr
INTERNET_PORT :: distinct win32.WORD

INTERNET_DEFAULT_PORT :: 0  // use the protocol-specific default
INTERNET_DEFAULT_HTTP_PORT :: 80  //    "     "  HTTP   "
INTERNET_DEFAULT_HTTPS_PORT :: 443  //    "     "  HTTPS  "

// WinHttpOpen dwAccessType values (also for WINHTTP_PROXY_INFO::dwAccessType)
ACCESS_TYPE :: enum win32.DWORD {
	DEFAULT_PROXY = 0,
	NO_PROXY = 1,
	NAMED_PROXY = 3,
	AUTOMATIC_PROXY = 4,
}

// flags for WinHttpOpenRequest():
WINHTTP_FLAG_SECURE :: 0x00800000  // use SSL if applicable (HTTPS)
WINHTTP_FLAG_ESCAPE_PERCENT :: 0x00000004  // if escaping enabled, escape percent as well
WINHTTP_FLAG_NULL_CODEPAGE :: 0x00000008  // assume all symbols are ASCII, use fast convertion
WINHTTP_FLAG_ESCAPE_DISABLE :: 0x00000040  // disable escaping
WINHTTP_FLAG_ESCAPE_DISABLE_QUERY :: 0x00000080  // if escaping enabled escape path part, but do not escape query
WINHTTP_FLAG_BYPASS_PROXY_CACHE :: 0x00000100  // add "pragma: no-cache" request header
WINHTTP_FLAG_REFRESH :: WINHTTP_FLAG_BYPASS_PROXY_CACHE
WINHTTP_FLAG_AUTOMATIC_CHUNKING :: 0x00000200  // Send request without content-length header or chunked TE

@(default_calling_convention="stdcall", link_prefix="WinHttp")
foreign winhttp {
	Open :: proc(
		pszAgentW: win32.LPCWSTR,
		dwAccessType: ACCESS_TYPE,
		pszProxyW: win32.LPCWSTR,
		pszProxyBypassW: win32.LPCWSTR,
		dwFlags: win32.DWORD,
	) -> HINTERNET ---

	Connect :: proc(
		hSession: HINTERNET,
		pswzServerName: win32.LPCWSTR,
		nServerPort: INTERNET_PORT,
		dwReserved: win32.DWORD,
	) -> HINTERNET ---

	OpenRequest :: proc(
		hConnect: HINTERNET,
		pwszVerb: win32.LPCWSTR,
		pwszObjectName: win32.LPCWSTR,
		pwszVersion: win32.LPCWSTR,
		pwszReferrer: win32.LPCWSTR,
		ppwszAcceptTypes: ^win32.LPCWSTR,
		dwFlags: win32.DWORD,
	) -> HINTERNET ---

	SendRequest :: proc(
		hRequest: HINTERNET,
		lpszHeaders: win32.LPCWSTR,
		dwHeadersLength: win32.DWORD,
		lpOptional: win32.LPVOID,
		dwOptionalLength: win32.DWORD,
		dwTotalLength: win32.DWORD,
		dwContext: win32.DWORD_PTR,
	) -> win32.BOOL ---

	ReceiveResponse :: proc(
		hRequest: HINTERNET,
		lpReserved: win32.LPVOID,
	) -> win32.BOOL ---

	QueryDataAvailable :: proc(
		hRequest: HINTERNET,
		lpdwNumberOfBytesAvailable: win32.LPDWORD,
	) -> win32.BOOL ---

	ReadData :: proc(
		hRequest: HINTERNET,
		lpBuffer: win32.LPVOID,
		dwNumberOfBytesToRead: win32.DWORD,
		lpdwNumberOfBytesRead: win32.LPDWORD,
	) -> win32.BOOL ---

	CloseHandle :: proc(hInternet: HINTERNET) -> win32.BOOL ---
}
