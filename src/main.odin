package PortableBuildTools

import "core:os"
import "core:fmt"
import "core:mem"
import "core:thread"
import "core:runtime"
import "core:strconv"
import "core:strings"
import "core:path/filepath"
import "core:dynlib"

import win32 "core:sys/windows"

import "winhttp"
import w "widgets"

L :: win32.L

WINDOW_TITLE :: "PortableBuildTools Setup"
SPACE_UNKNOWN :: "Space available: Unknown"
SPACE_REQUIRED :: 2 * mem.Gigabyte

PREP_DONE :: win32.WM_APP + 1

internet_session: winhttp.HINTERNET

devcmd_script_name := fmt.aprint(`devcmd`)
add_to_env: bool
env_is_global: bool // install for all users or not
info_path: string
admin: bool
free_space: int = -1

allow_closing: bool = true
window_rect, window_client: Rect
window_w, window_h: i32 = 380, 310
program_icon: win32.HICON
window_id: win32.HWND

Side :: enum {
	Left,
	Right,
}

Widget_ID :: enum {
	None,
	AcceptLicense,
	MSVCPicker,
	SDKPicker,
	TargetPicker,
	HostPicker,
	CreateSetupScript,
	ScriptName,
	ScriptNameFrame,
	AddToEnv,
	AllUsers,
	FolderPath,
	ChooseFolder,
	FreeSpace,
	Install,
	Console,
	InstallProgress,
}

widgets: [Widget_ID]win32.HWND

rect_cut_top_button :: proc(r: ^Rect) -> (b: Rect) {
	return rect_cut_top(r, w.page.font_height + 4)
}

rect_cut_bottom_button :: proc(r: ^Rect) -> (b: Rect) {
	return rect_cut_bottom(r, w.page.font_height + 4)
}

add_button_autow :: proc(text: string, layout: ^Rect, side: Side, style: win32.UINT = 0) -> win32.HWND {
	f := rect_cut_left if side == .Left else rect_cut_right
	width, _ := w.get_text_size(text)
	return w.add_widget(.Button, text, f(layout, int(width) + 24), style) // 24 is space for potential icon
}

new_page :: proc() -> Rect {
	w.remove_widgets()
	widgets = {}

	layout: Rect = {0, 0, window_client.w, window_client.h}
	rect_cut_margins(&layout, 10)
	return layout
}

prep :: proc() {
	ok: bool
	info_path, ok = download_info().?
	if !ok do return
	if !load_info(info_path) do return

	free_all(context.temp_allocator)

	msvc_version = tools_info.msvc_versions[0]
	sdk_version = tools_info.sdk_versions[0]
	target_arch = targets[0]
	host_arch = hosts[0]

	win32.PostMessageW(w.page.id, PREP_DONE, 0, 0)
}

show_console_page :: proc() {
	layout := new_page()

	widgets[.InstallProgress] = w.add_widget(.Progress_Bar, "", rect_cut_bottom(&layout, 23))
	rect_cut_bottom(&layout, 10)

	widgets[.Console] = w.add_widget(.Console, "", layout)
}

show_main_page :: proc() {
	w.set_taskbar_progress(0, .Normal)
	layout := new_page()

	lid: win32.HWND

	{
		pickers := rect_cut_top(&layout, 50)
		{
			picker := rect_cut_left(&pickers, 80)
			w.add_widget(.Text, "MSVC Version", rect_cut_top_button(&picker))
			widgets[.MSVCPicker] = w.add_widget(.Combo_Box, "", picker)
			w.fill_combo_box(widgets[.MSVCPicker], tools_info.msvc_versions)
		}
		rect_cut_left(&pickers, 10)
		{
			picker := rect_cut_left(&pickers, 80)
			w.add_widget(.Text, "SDK Version", rect_cut_top_button(&picker))
			widgets[.SDKPicker] = w.add_widget(.Combo_Box, "", picker)
			w.fill_combo_box(widgets[.SDKPicker], tools_info.sdk_versions)
		}
		rect_cut_left(&pickers, 10)
		{
			picker := rect_cut_left(&pickers, 80)
			w.add_widget(.Text, "Target Arch", rect_cut_top_button(&picker))
			widgets[.TargetPicker] = w.add_widget(.Combo_Box, "", picker)
			w.fill_combo_box(widgets[.TargetPicker], targets)
		}
		rect_cut_left(&pickers, 10)
		{
			picker := rect_cut_left(&pickers, 80)
			w.add_widget(.Text, "Host Arch", rect_cut_top_button(&picker))
			widgets[.HostPicker] = w.add_widget(.Combo_Box, "", picker)
			w.fill_combo_box(widgets[.HostPicker], hosts)
		}
	}

	{
		layout := rect_cut_top(&layout, 50)
		widgets[.ScriptNameFrame] = w.add_widget(.Frame, `"Developer Command Prompt" Batch Script name`, layout)
		{
			layout := layout
			rect_cut_margins(&layout, 10)
			layout = rect_cut_bottom_button(&layout)
			widgets[.ScriptName] = w.add_widget(.Edit, devcmd_script_name, layout)
		}
	}
	rect_cut_top(&layout, 10)

	widgets[.AddToEnv] = w.add_widget(.Check_Box, "Add to Environment (avoids having to use a script)", rect_cut_top_button(&layout))
	{
		layout := rect_cut_top_button(&layout)
		rect_cut_left(&layout, 10)
		widgets[.AllUsers] = w.add_widget(.Check_Box, "For all users", layout, win32.WS_DISABLED)
	}
	rect_cut_top(&layout, 5)

	{
		layout := rect_cut_top(&layout, 60)
		w.add_widget(.Frame, "Destination Folder", layout)
		{
			layout := layout
			rect_cut_margins(&layout, 10)
			layout = rect_cut_bottom(&layout, 23)
			widgets[.ChooseFolder] = add_button_autow("Browse...", &layout, .Right)
			rect_cut_right(&layout, 10)
			widgets[.FolderPath] = w.add_widget(.Edit, install_path, layout)
			if win32.SUCCEEDED(win32.CoInitializeEx(nil, .APARTMENTTHREADED)) {
				win32.SHAutoComplete(widgets[.FolderPath], win32.SHACF_FILESYSTEM)
				win32.CoUninitialize()
			}
		}
	}
	rect_cut_top(&layout, 5)

	w.add_widget(.Text, fmt.aprint("Space required:", SPACE_REQUIRED / mem.Gigabyte, "GB"), rect_cut_top_button(&layout))
	widgets[.FreeSpace] = w.add_widget(.Text, SPACE_UNKNOWN, rect_cut_top_button(&layout))
	rect_cut_top(&layout, 5)
	w.add_widget(.Text, "", rect_cut_top(&layout, 1), win32.WS_BORDER)

	{
		bottom_panel := rect_cut_bottom(&layout, 23)

		widgets[.Install] = add_button_autow("Install", &bottom_panel, .Right)

		// NOTE: Ideally I would like the text to be clickable as in other checkboxes;
		// But unfortunately the size measurements Windows reports for text are completely randomized for some reason;
		// It is randomized between Windows versions, and even between builds of the same major version.
		// So I settled on only having only the checkbox clickable for consistent GUI.
		// Consider this a feature: user needs to hit the checkbox exactly right to prove that he read the License!
		checkbox_width := win32.GetSystemMetrics(win32.SM_CXMENUCHECK) + win32.GetSystemMetrics(win32.SM_CXEDGE)
		widgets[.AcceptLicense] = w.add_widget(.Check_Box, "", rect_cut_left(&bottom_panel, cast(int)checkbox_width))
		w.set_checkbox(widgets[.AcceptLicense], accept_license)

		rect_cut_top(&bottom_panel, 4) // Align (kinda) the following text with the checkbox

		license := strings.concatenate({`I accept the <a href="`, tools_info.license_url, `">License Agreement</a>`})
		lid = w.add_widget(.Link, license, bottom_panel)
	}

	w.change_tab_order(widgets[.FolderPath], widgets[.ChooseFolder], widgets[.AcceptLicense], lid, widgets[.Install])

	update_install_button_admin()
	update_disk_space()
	update_install_button_enabled()
}

enable_close_button :: proc(state: bool) {
	hMenu: win32.HMENU
	dwExtra: win32.UINT

	hMenu = win32.GetSystemMenu(window_id, false)
	if hMenu == nil do return

	dwExtra = state ? win32.MF_ENABLED : (win32.MF_DISABLED | win32.MF_GRAYED)
	win32.EnableMenuItem(hMenu, win32.SC_CLOSE, win32.MF_BYCOMMAND | dwExtra)
	allow_closing = state
}

launch_installer :: proc() {
	enable_close_button(false)

	defer {
		enable_close_button(true)
	}

	PIPE_PATH :: `\\.\pipe\BuildTools`
	pipe := win32.CreateNamedPipeW(
		L(PIPE_PATH),
		win32.PIPE_ACCESS_INBOUND, // messages sent from client to server
		win32.PIPE_TYPE_MESSAGE | win32.PIPE_READMODE_MESSAGE | win32.PIPE_WAIT, // every WriteFile is a message unit with blocking wait
		1, // 1 instance max
		0, 0, // no outbound or inbound buffers
		0, // default timeout
		nil, // default security attributes
	)

	if pipe == win32.INVALID_HANDLE_VALUE {
		fmt.printf("failed to create a pipe [{}]: {}\n", pipe, win32.GetLastError())
		return
	}

	env_mode := ""
	if add_to_env {
		env_mode = "global" if env_is_global else "local"
	}
	params := fmt.tprintf(`accept_license msvc={} sdk={} target={} host={} install_path="{}" script="{}" env={} pipe={} info="{}"`,
		msvc_version, sdk_version, target_arch, host_arch, filepath.clean(install_path, context.temp_allocator), devcmd_script_name, env_mode,
		PIPE_PATH, info_path,
	)
	inst := win32.ShellExecuteW(nil, L("runas") if admin else nil, win32.utf8_to_wstring(os.args[0]), win32.utf8_to_wstring(params), nil, 0)
	if uintptr(inst) <= 32 {
		fmt.println("ERROR RUNNING A PROGRAM", inst)
	}

	win32.ConnectNamedPipe(pipe, nil)
	for {
		data, ok := os.read_entire_file(auto_cast pipe)
		if !ok {
			w.console_println(widgets[.Console], "something went wrong when reading the pipe")
			return
		}

		if len(data) <= 0 do continue

		msg := string(data)
		if msg == "Q" do break

		switch {
		case strings.has_prefix(msg, "PRC"):
			msg = strings.trim_prefix(msg, "PRC")
			percentage := strconv.atoi(msg)
			w.set_progress_bar(widgets[.InstallProgress], percentage, .Normal)

		case strings.has_prefix(msg, "MSG"):
			msg = strings.trim_prefix(msg, "MSG")
			w.console_println(widgets[.Console], msg)

			if msg == "Done!" {
				if add_to_env {
					if env_is_global {
						show_message_box(.Info, "Success!", "Please reboot the machine to finish installation.", window_id)
					} else {
						show_message_box(.Info, "Success!", "Please log out and back in to finish installation.", window_id)
					}
				} else {
					show_message_box(.Info, "Success!", "PortableBuildTools installed successfully.", window_id)
				}
			}

		case strings.has_prefix(msg, "ERR"):
			msg = strings.trim_prefix(msg, "ERR")
			w.console_println(widgets[.Console], fmt.tprint("[ERROR]", msg))
			w.set_progress_bar(widgets[.InstallProgress], -1, .Error)

			show_message_box(.Error, "Error", "PortableBuildTools failed to install.", window_id)

		case strings.has_prefix(msg, "UNK"):
			w.set_progress_bar(widgets[.InstallProgress], -1, .Unknown)
		}
	}
}

is_valid_filename :: proc(n: string) -> bool {
	if strings.has_suffix(n, " ") || strings.has_prefix(n, " ") || strings.has_suffix(n, ".") {
		return false
	}

	switch n {
	case "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4",
		"LPT5", "LPT6", "LPT7", "LPT8", "LPT9":
		return false
	}

	return !strings.contains_any(n, `<>:"/\|*?`)
}

// Windows does not allow spaces before or after file name, I think
is_valid_path :: proc(p: string) -> bool {
	p := p
	p = filepath.clean(strings.trim_space(p), context.temp_allocator)

	for {
		np, f := filepath.split(p)

		if !is_valid_filename(f) do return false
		if f == "." do return os.exists(filepath.volume_name(np))
		if np == p do return os.exists(filepath.volume_name(np))
		if f == p do break

		p = filepath.clean(np, context.temp_allocator)
	}

	return true
}

is_valid_install_path :: proc(p: string) -> bool {
	if !is_valid_path(p) {
		return false
	}

	if os.is_file(p) {
		return false
	}

	return true
}

update_install_button_admin :: proc() {
	admin = false

	if env_is_global {
		admin = true
		w.set_widget_admin(widgets[.Install], admin)
		return
	}

	if !is_valid_install_path(install_path) {
		w.set_widget_admin(widgets[.Install], admin)
		return
	}

	p := first_existing_folder(install_path)
	// TODO: test if all possible paths are correctly checked
	//fmt.println("found path", p)

	admin = !can_access_folder(p)
	w.set_widget_admin(widgets[.Install], admin)
}

update_disk_space :: proc() {
	free_space = -1

	if !is_valid_install_path(install_path) {
		w.set_text(widgets[.FreeSpace], SPACE_UNKNOWN)
		return
	}

	p := first_existing_folder(install_path)

	bytes_avail, total_bytes, total_free_bytes: win32.ULARGE_INTEGER
	if !win32.GetDiskFreeSpaceExW(win32.utf8_to_wstring(p), &bytes_avail, &total_bytes, &total_free_bytes) {
		w.set_text(widgets[.FreeSpace], SPACE_UNKNOWN)
		return
	}

	free_space = int(total_free_bytes) if admin else int(bytes_avail)

	w.set_text(widgets[.FreeSpace], fmt.tprintf("Space available: {} GB", free_space / mem.Gigabyte))
}

update_install_button_enabled :: proc() {
	if !accept_license {
		w.set_widget_enabled(widgets[.Install], false)
		return
	}

	// NOTE: install_path is always valid here, it was already checked in update_disk_space

	if free_space < SPACE_REQUIRED {
		w.set_widget_enabled(widgets[.Install], false)
		return
	}

	w.set_widget_enabled(widgets[.Install], true)
}

// Goes up the install_path tree until it hits a folder (or volume) that exists
first_existing_folder :: proc(folder: string) -> string {
	p := strings.trim_space(folder)
	p = filepath.clean(p, context.temp_allocator)
	for {
		if os.is_file(p) {
			p, _ = filepath.split(p)
			break
		}

		if os.exists(p) do break

		np, _ := filepath.split(p)
		if np == p do break
		p = filepath.clean(np, context.temp_allocator)
	}

	return p
}

can_access_folder :: proc(folder: string) -> (ok: bool) {
	folder := folder
	folder = filepath.clean(folder, context.temp_allocator)
	// C:\ has to be turned into C:. to check access properly
	if strings.has_suffix(folder, `:\`) {
		folder = strings.trim_suffix(folder, `\`)
		folder = filepath.clean(folder, context.temp_allocator)
	}

	length: win32.DWORD
	generic_access_rights := win32.GENERIC_READ | win32.GENERIC_WRITE
	flags: win32.DWORD = win32.OWNER_SECURITY_INFORMATION | win32.GROUP_SECURITY_INFORMATION | win32.DACL_SECURITY_INFORMATION

	folder_name := win32.utf8_to_wstring(folder)

	if win32.GetFileSecurityW(folder_name, flags, nil, 0, &length) {
		return
	}
	if win32.GetLastError() != win32.ERROR_INSUFFICIENT_BUFFER {
		return
	}

	// NOTE: Assume no allocation error
	desc_mem, _ := mem.alloc(auto_cast length)
	security := win32.PSECURITY_DESCRIPTOR(desc_mem)
	if security == nil {
		return
	}
	defer free(security)

	if !win32.GetFileSecurityW(folder_name, flags, security, length, &length) {
		return
	}

	token: win32.HANDLE
	token_flags := win32.TOKEN_IMPERSONATE | win32.TOKEN_QUERY | win32.TOKEN_DUPLICATE | win32.STANDARD_RIGHTS_READ
	if !win32.OpenProcessToken(win32.GetCurrentProcess(), token_flags, &token) {
		return
	}
	defer win32.CloseHandle(token)

	impersonated_token: win32.HANDLE
	if !win32.DuplicateToken(token, .SecurityImpersonation, &impersonated_token) {
		return
	}
	defer win32.CloseHandle(impersonated_token)

	mapping: win32.GENERIC_MAPPING = {
		GenericRead = win32.FILE_GENERIC_READ,
		GenericWrite = win32.FILE_GENERIC_WRITE,
		GenericExecute = win32.FILE_GENERIC_EXECUTE,
		GenericAll = win32.FILE_ALL_ACCESS,
	}
	privileges: win32.PRIVILEGE_SET
	granted_access, privileges_len: win32.DWORD
	privileges_len = size_of(privileges)
	result: win32.BOOL

	win32.MapGenericMask(&generic_access_rights, &mapping)
	if !win32.AccessCheck(security, impersonated_token, generic_access_rights, &mapping, &privileges, &privileges_len, &granted_access, &result) {
		return
	}

	ok = auto_cast result
	return
}

update_script_name :: proc() {
	fb: [win32.MAX_PATH_WIDE]win32.WCHAR
	script, ok := w.get_text(widgets[.ScriptName], fb[:], context.allocator)
	if ok {
		delete(devcmd_script_name)
		devcmd_script_name = script
	}
}

process_command :: proc(winid: win32.HWND, wparam: win32.WPARAM, lparam: win32.LPARAM) {
	hiword := win32.HIWORD(win32.DWORD(wparam))

	switch id := w.get_widget_from_wparam(winid, wparam); id {
	case widgets[.ChooseFolder]:
		folder, ok := select_folder(allocator = context.allocator)
		if ok {
			w.set_text(widgets[.FolderPath], folder)
		}
	case widgets[.AddToEnv]:
		add_to_env = !add_to_env
		w.set_checkbox(id, add_to_env)
		w.set_widget_enabled(widgets[.AllUsers], add_to_env)

	case widgets[.AllUsers]:
		env_is_global = !env_is_global
		w.set_checkbox(id, env_is_global)
		update_install_button_admin()

	case widgets[.AcceptLicense]:
		accept_license = !accept_license
		w.set_checkbox(id, accept_license)
		update_install_button_enabled()

	case widgets[.Install]:
		if os.exists(install_path) {
			dir, _ := os.open(install_path)
			defer os.close(dir)

			files_info, _ := os.read_dir(dir, -1, allocator = context.temp_allocator)
			if len(files_info) > 0 {
				show_message_box(.Error, "Error", "Installation directory is not empty.", window_id)
				break
			}
		}

		show_console_page()
		thread.run(launch_installer, runtime.default_context(), .High)

	case widgets[.FolderPath]:
		if hiword == win32.EN_CHANGE {
			fb: [win32.MAX_PATH_WIDE]win32.WCHAR
			delete(install_path)
			folder, ok := w.get_text(widgets[.FolderPath], fb[:], context.allocator)
			if ok {
				install_path = folder
			} else { // NOTE: during testing ok returned false only when the path was empty
				install_path = fmt.aprintln("")
			}

			update_install_button_admin()
			update_disk_space()
			update_install_button_enabled()
		}

	case widgets[.ScriptName]:
		if hiword == win32.EN_CHANGE {
			update_script_name()
		}

	case widgets[.MSVCPicker], widgets[.SDKPicker], widgets[.TargetPicker], widgets[.HostPicker]:
		if hiword == win32.CBN_SELCHANGE {
			index := win32.SendMessageW(id, win32.CB_GETCURSEL, 0, 0)
			switch id {
			case widgets[.MSVCPicker]:
				msvc_version = tools_info.msvc_versions[index]
			case widgets[.SDKPicker]:
				sdk_version = tools_info.sdk_versions[index]
			case widgets[.TargetPicker]:
				target_arch = targets[index]
			case widgets[.HostPicker]:
				host_arch = hosts[index]
			}
		}
	}
}

wnd_proc :: proc "stdcall" (winid: win32.HWND, msg: win32.UINT, wparam: win32.WPARAM, lparam: win32.LPARAM) -> win32.LRESULT {
	context = runtime.default_context()

	switch msg {
	case win32.WM_CREATE:
		wr, cr: win32.RECT
		point: win32.POINT
		win32.GetWindowRect(winid, &wr)
		win32.GetClientRect(winid, &cr)
		win32.ClientToScreen(winid, &point)

		window_rect = {
			cast(int)wr.left,
			cast(int)wr.top,
			cast(int)wr.right - cast(int)wr.left,
			cast(int)wr.bottom - cast(int)wr.top,
		}
		window_client = {
			cast(int)point.x,
			cast(int)point.y,
			cast(int)cr.right,
			cast(int)cr.bottom,
		}

		w.init_widget_page(winid)
		show_console_page()
		thread.run(prep, runtime.default_context(), .High)

	case win32.WM_DESTROY:
		win32.PostQuitMessage(0)

	case win32.WM_CLOSE:
		if !allow_closing {
			return 0
		}

	case win32.WM_COMMAND:
		process_command(winid, wparam, lparam)

	case win32.WM_WINDOWPOSCHANGED:
		pos := cast(^win32.WINDOWPOS)cast(uintptr)lparam

		rect: win32.RECT
		point: win32.POINT
		win32.GetClientRect(winid, &rect)
		win32.ClientToScreen(winid, &point)

		window_rect = {cast(int)pos.x, cast(int)pos.y, cast(int)pos.cx, cast(int)pos.cy}
		window_client = {cast(int)point.x, cast(int)point.y, cast(int)rect.right, cast(int)rect.bottom}

	case win32.WM_NOTIFY: // link pressed (with click or Enter)
		nmhdr := cast(^win32.NMHDR)cast(uintptr)lparam
		switch auto_cast nmhdr.code {
		case win32.NM_CLICK:
			fallthrough
		case win32.NM_RETURN:
			nmlink := cast(^win32.NMLINK)cast(uintptr)lparam
			item := nmlink.item
			if item.iLink == 0 {
				url := raw_data(item.szUrl[:])
				fmt.printf("open {:s}\n", url)
				win32.ShellExecuteW(nil, L("open"), url, nil, nil, win32.SW_SHOW)
			} else { // what happened here?
				id := raw_data(item.szID[:])
				show_message_box(.Error, "Error", fmt.tprintf("What did you do? {}", id), winid)
				unreachable()
			}
		}

	case PREP_DONE:
		show_main_page()
	}

	return win32.DefWindowProcW(winid, msg, wparam, lparam)
}

main :: proc() {
	internet_session = winhttp.Open(win32.L("WinHTTP/1.0"), .DEFAULT_PROXY, nil, nil, 0)
	if internet_session == nil {
		show_message_box(.Error, "Error", fmt.tprint("WinHttpOpen failed with error code:", win32.GetLastError()))
		os.exit(1)
	}
	defer winhttp.CloseHandle(internet_session)

	if len(os.args) > 1 {
		cli()
		return
	}

	// We are not doing CLI, free console so it never is shown.
	win32.FreeConsole()
	
	gui_mode = true
	install_path = fmt.aprint(DEFAULT_INSTALL_PATH)

	rich_text, ok := dynlib.load_library("riched20.dll")
	defer if ok do dynlib.unload_library(rich_text)

	window_style := win32.WS_VISIBLE | win32.WS_OVERLAPPEDWINDOW &~ win32.WS_THICKFRAME &~ win32.WS_MAXIMIZEBOX

	wc: win32.WNDCLASSW = {
		lpszClassName = L(WINDOW_TITLE),
		hInstance = cast(win32.HINSTANCE)win32.GetModuleHandleW(nil),
		hbrBackground = win32.GetSysColorBrush(win32.COLOR_3DFACE),
		lpfnWndProc = wnd_proc,
		hCursor = win32.LoadCursorA(nil, win32.IDC_ARROW),
	}
	win32.RegisterClassW(&wc)

	{
		crect: win32.RECT = {0, 0, window_w, window_h}
		win32.AdjustWindowRect(&crect, window_style, false)

		window_w = crect.right - crect.left
		window_h = crect.bottom - crect.top
	}
	window_id = win32.CreateWindowW(wc.lpszClassName, L(WINDOW_TITLE), window_style,
		win32.CW_USEDEFAULT, win32.CW_USEDEFAULT, window_w, window_h, nil, nil, wc.hInstance, nil,
	)

	program_icon = win32.LoadIconW(wc.hInstance, L("icon"))
	win32.SetClassLongPtrW(window_id, win32.GCLP_HICON, auto_cast cast(uintptr)program_icon)

	msg: win32.MSG = ---
	for win32.GetMessageW(&msg, nil, 0, 0) {
		if win32.IsDialogMessageW(window_id, &msg) do continue

		win32.TranslateMessage(&msg)
		win32.DispatchMessageW(&msg)
	}
}
