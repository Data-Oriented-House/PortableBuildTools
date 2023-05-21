package widgets

import "core:fmt"
import "core:slice"

import win32 "core:sys/windows"

L :: win32.L
OK :: win32.SUCCEEDED

Rect :: struct {x, y, w, h: int}

DEFAULT_FONT_SIZE :: 15

// NOTE: idk what the proper signal offset should be, this is almost a random number
SIGNAL_OFFSET :: 0x100
global_signal: u16

Widget_And_Proc :: struct {
	id: win32.HWND,
	wproc: win32.WNDPROC,
}

Widget_Type :: enum {
	Text,
	Link,
	Button,
	Edit,
	Read_Only_Edit,
	Frame,
	Combo_Box,
	Check_Box,
	Console,
	Progress_Bar,
}

Widget_Window_Name: map[Widget_Type]win32.wstring = {
	.Text = L("Static"),
	.Button = L("Button"),
	.Link = L("SysLink"),
	.Edit = L("Edit"),
	.Read_Only_Edit = L("Edit"),
	.Console = L("Edit"),
	.Frame = L("Button"),
	.Combo_Box = L("Combobox"),
	.Check_Box = L("Button"),
	.Progress_Bar = L("msctls_progress32"),
}

Widget_Window_Class: map[Widget_Type]win32.UINT = {
	.Text = win32.SS_LEFT | win32.WS_GROUP,
	.Button = win32.WS_TABSTOP,
	.Link = win32.WS_TABSTOP,
	.Edit = win32.WS_BORDER | win32.ES_AUTOHSCROLL | win32.WS_TABSTOP,
	.Read_Only_Edit = win32.WS_BORDER | win32.ES_AUTOHSCROLL | win32.ES_READONLY | win32.WS_TABSTOP,
	.Console = win32.WS_BORDER | win32.WS_VSCROLL | win32.ES_READONLY | win32.ES_MULTILINE,
	.Frame = win32.BS_GROUPBOX,
	.Combo_Box = win32.CBS_DROPDOWNLIST | win32.WS_TABSTOP,
	.Check_Box = win32.BS_CHECKBOX | win32.WS_TABSTOP,
	.Progress_Bar = win32.PBS_SMOOTH,
}

Widget_Page :: struct {
	id: win32.HWND,
	dc: win32.HDC,
	font: win32.WPARAM,
	font_height: int,

	widgets_buffer: [64]Widget_And_Proc,
	widgets: [dynamic]Widget_And_Proc,
}
page: Widget_Page

edit_proc :: proc "stdcall" (winid: win32.HWND, msg: win32.UINT, wparam: win32.WPARAM, lparam: win32.LPARAM) -> win32.LRESULT {
	switch msg {
	case win32.WM_CHAR:
		// disable beep
		if wparam == 0x01 {
			return 0
		}
	case win32.WM_GETDLGCODE:
		// disable bug when Escape closes the parent window
		if wparam == 0x1b {
			win32.SendMessageW(winid, win32.EM_SETSEL, ~uintptr(0), 0)
			return 0
		}
		ctrl_down := cast(u32)win32.GetKeyState(win32.VK_CONTROL) & u32(0x8000) != 0
		// enable Ctrl+A
		if ctrl_down && wparam == 'A' {
			win32.SendMessageW(winid, win32.EM_SETSEL, 0, -1)
		}
	}

	wproc: win32.WNDPROC
	for w in page.widgets {
		if w.id == winid {
			wproc = w.wproc
			break
		}
	}
	return win32.CallWindowProcW(wproc, winid, msg, wparam, lparam)
}

get_widget_from_wparam :: proc(winid: win32.HWND, wparam: win32.WPARAM) -> win32.HWND {
	return win32.GetDlgItem(winid, auto_cast win32.LOWORD(win32.DWORD(wparam)))
}

init_widget_page :: proc(winid: win32.HWND) {
	page.widgets = slice.into_dynamic(page.widgets_buffer[:])
	page.id = winid
	page.dc = win32.GetDC(winid)

	hFont := win32.CreateFontW(DEFAULT_FONT_SIZE, 0, 0, 0, win32.FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, L("Segoe UI"))

	holdFont := win32.SelectObject(page.dc, auto_cast hFont)
	defer win32.SelectObject(page.dc, holdFont)

	page.font = cast(win32.WPARAM)cast(uintptr)hFont
	tm: win32.TEXTMETRICW
	win32.GetTextMetricsW(page.dc, &tm)
	page.font_height = auto_cast tm.tmHeight
}

add_widget :: proc(t: Widget_Type, text: string, pos: Rect, style: win32.UINT = 0) -> win32.HWND {
	wtext := win32.utf8_to_wstring(text)

	signal := cast(win32.HMENU)cast(uintptr)(global_signal + u16(SIGNAL_OFFSET))
	global_signal += 1

	id := win32.CreateWindowW(
		Widget_Window_Name[t], wtext,
		win32.WS_VISIBLE | win32.WS_CHILD | Widget_Window_Class[t] | style,
		i32(pos.x), i32(pos.y), i32(pos.w), i32(pos.h), page.id,
		signal, nil, nil,
	)

	wproc: win32.WNDPROC
	if t == .Edit || t == .Read_Only_Edit || t == .Console {
		eproc_addr := cast(uintptr)cast(rawptr)edit_proc
		eproc := win32.SetWindowLongPtrW(id, win32.GWLP_WNDPROC, cast(win32.LONG_PTR)eproc_addr)
		wproc = cast(win32.WNDPROC)cast(rawptr)cast(uintptr)eproc
	}

	append(&page.widgets, Widget_And_Proc{id, wproc})

	win32.SendMessageW(id, win32.WM_SETFONT, page.font, 1)
	win32.InvalidateRect(page.id, &{i32(pos.x), i32(pos.y), i32(pos.x+pos.w), i32(pos.y+pos.h)}, false)

	return id
}

remove_widgets :: proc() {
	for w in page.widgets {
		win32.SendMessageW(w.id, win32.WM_CLOSE, 0, 0)
	}
	clear(&page.widgets)
	global_signal = 0
}

change_tab_order :: proc(ids: ..win32.HWND) {
	lid := ids[0]
	for v in ids[1:] {
		win32.SetWindowPos(v, lid, 0, 0, 0, 0, win32.SWP_NOMOVE | win32.SWP_NOSIZE)
		lid = v
	}
}

get_text_size :: proc(text: string) -> (w, h: int) {
	wtext := win32.utf8_to_utf16(text)
	sz: win32.SIZE

	win32.GetTextExtentPoint32W(page.dc, raw_data(wtext), cast(i32)len(wtext), &sz)
	return int(sz.cx), int(sz.cy)
}

// This prints a line to the console and scrolls to it
console_println :: proc(id: win32.HWND, line: string) {
	// Select all
	win32.SendMessageW(id, win32.EM_SETSEL, 0, -1)
	// Unselect and stay at the end pos
	win32.SendMessageW(id, win32.EM_SETSEL, ~uintptr(0), -1)

	wline := win32.utf8_to_wstring(fmt.tprintf("{}\r\n", line))
	// Append text to current pos and scroll down
	win32.SendMessageW(id, win32.EM_REPLACESEL, 0, cast(win32.LPARAM)uintptr(&wline[0]))
}

Progress_Bar_State :: enum {
	Normal,
	Error,
	Unknown,
	Paused,
	Disabled,
}

set_progress_bar :: proc(id: win32.HWND, value: int, state: Progress_Bar_State = .Normal) {
	value := value
	if value < 0 {
		value = win32.SendMessageW(id, win32.PBM_GETPOS, 0, 0)
	}

	style := cast(u32)win32.GetWindowLongPtrW(id, win32.GWL_STYLE)

	if state == .Unknown {
		style |= win32.PBS_MARQUEE
		win32.SetWindowLongPtrW(id, win32.GWL_STYLE, cast(int)style)
		win32.SendMessageW(id, win32.PBM_SETMARQUEE, auto_cast uintptr(1), 0)
		return
	}

	style &~= win32.PBS_MARQUEE
	win32.SetWindowLongPtrW(id, win32.GWL_STYLE, cast(int)style)

	if state == .Disabled {
		win32.EnableWindow(id, false)
		return
	}

	win_state: win32.WPARAM
	#partial switch state {
	case .Normal:
		win_state = win32.PBST_NORMAL
	case .Error:
		win_state = win32.PBST_ERROR
	case .Paused:
		win_state = win32.PBST_PAUSED
	}

	win32.EnableWindow(id, true)
	win32.SendMessageW(id, win32.PBM_SETPOS, uintptr(value), 0)
	win32.SendMessageW(id, win32.PBM_SETSTATE, win_state, 0)
}

set_taskbar_progress :: proc(value: int, state: Progress_Bar_State = .Normal) -> (succeeded: bool) {
	OK(win32.CoInitializeEx(nil, .APARTMENTTHREADED)) or_return
	defer win32.CoUninitialize()

	tl: ^win32.ITaskbarList3
	OK(win32.CoCreateInstance(
		win32.CLSID_TaskbarList, nil, win32.CLSCTX_INPROC_SERVER,
		win32.IID_ITaskbarList3, cast(^rawptr)(&tl),
	)) or_return
	defer tl->Release()

	win_state: win32.TBPFLAG
	#partial switch state {
	case .Normal:
		win_state = .NORMAL
	case .Error:
		win_state = .ERROR
	case .Unknown:
		win_state = .INDETERMINATE
	case .Paused:
		win_state = .PAUSED
	case .Disabled:
		win_state = .NOPROGRESS
	}

	OK(tl->SetProgressState(page.id, win_state)) or_return

	if value < 0 do return

	OK(tl->SetProgressValue(page.id, auto_cast value, 100)) or_return

	return
}

fill_combo_box :: proc(id: win32.HWND, options: []string, default := 0) {
	for v in options {
		wv := win32.utf8_to_wstring(v)
		win32.SendMessageW(id, win32.CB_ADDSTRING, 0, cast(win32.LPARAM)cast(uintptr)wv)
	}
	win32.SendMessageW(id, win32.CB_SETCURSEL, uintptr(default), 0)
}

set_text :: proc(id: win32.HWND, str: string) {
	win32.SetWindowTextW(id, win32.utf8_to_wstring(str))
}

get_text :: proc(id: win32.HWND, buf: []win32.WCHAR, allocator := context.temp_allocator) -> (str: string, ok: bool) {
	l := win32.GetWindowTextLengthW(id)
	if win32.GetWindowTextW(id, &buf[0], l + 1) < 1 do return

	folder, err := win32.utf16_to_utf8(buf[:l], allocator)
	if err != nil do return

	ok = true
	str = folder
	return
}

set_checkbox :: proc(id: win32.HWND, checked: bool) {
	check: win32.WPARAM = win32.BST_CHECKED if checked else win32.BST_UNCHECKED
	win32.SendMessageW(id, win32.BM_SETCHECK, check, 0)
}

set_widget_enabled :: proc(id: win32.HWND, enabled: bool) {
	win32.EnableWindow(id, auto_cast enabled)
}

set_widget_admin :: proc(id: win32.HWND, admin: bool) {
	win32.SendMessageW(id, win32.BCM_SETSHIELD, 0, auto_cast admin)
}
