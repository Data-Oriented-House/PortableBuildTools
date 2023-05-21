package PortableBuildTools

import win32 "core:sys/windows"

Message_Box_Flag :: enum {
	Error = win32.MB_ICONERROR,
	Warning = win32.MB_ICONWARNING,
	Info = win32.MB_ICONINFORMATION,
	OkCancel = win32.MB_OKCANCEL,
	AbortRetryIgnore = win32.MB_ABORTRETRYIGNORE,
	YesNoCancel = win32.MB_YESNOCANCEL,
	YesNo = win32.MB_YESNO,
	RetryCancel = win32.MB_RETRYCANCEL,
	CancelTryAgainContinue = win32.MB_CANCELTRYCONTINUE,
}

Message_Box_Result :: enum {
	None,
	Ok = win32.IDOK,
	Cancel = win32.IDCANCEL,
	Abort = win32.IDABORT,
	Retry = win32.IDRETRY,
	Ignore = win32.IDIGNORE,
	Yes = win32.IDYES,
	No = win32.IDNO,
	Close = win32.IDCLOSE,
	Help = win32.IDHELP,
	TryAgain = win32.IDTRYAGAIN,
	Continue = win32.IDCONTINUE,
	Timeout = win32.IDTIMEOUT,
}

show_message_box :: proc(type: Message_Box_Flag, title, message: string, winid: win32.HWND = nil) -> Message_Box_Result {
	utype: win32.UINT = win32.MB_TOPMOST | win32.UINT(type)

	return cast(Message_Box_Result)win32.MessageBoxW(winid, win32.utf8_to_wstring(message), win32.utf8_to_wstring(title), utype)
}
