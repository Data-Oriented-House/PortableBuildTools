package PortableBuildTools

import "core:os"
import "core:fmt"
import "core:mem"
import "core:mem/virtual"
import "core:slice"
import "core:strings"
import "core:runtime"
import "core:encoding/json"
import "core:path/filepath"
import "core:path/slashpath"

import "winhttp"
import w "widgets"
import mz "miniz"
import win32 "core:sys/windows"

DEFAULT_INSTALL_PATH :: `C:\BuildTools`
MANIFEST_URL :: `https://aka.ms/vs/17/release/channel`
Payload :: struct {
	file_name: string,
	url: string,
	size: i64,
	sha256: string,
}
Package_Info :: struct {
	id: string,
	version: string,
	target: string,
	host: string,
	payloads: []Payload,
}
Debug_Package_Info :: struct {
	host: string,
	msi: string,
	payloads: []Payload,
}
Tools_Info :: struct {
	license_url: string,

	msvc_versions: []string,
	sdk_versions: []string,

	msvc_packages: []Package_Info,
	sdk_packages: []Package_Info,

	debug_crt_runtime: []Debug_Package_Info,
}
tools_info: Tools_Info
// other architectures may or may not work - not really tested
hosts: []string = {"x64", "x86"}
targets: []string = {"x64", "x86", "arm", "arm64"}

msvc_version, sdk_version, target_arch, host_arch: string
accept_license: bool
install_path: string
gui_mode: bool

First_Manifest :: struct {
	channel_items: []struct {
		id: string `json:"id"`,

		payloads: []struct {
			file_name: string `json:"fileName"`,
			sha256: string `json:"sha256"`,
			size: int `json:"size"`,
			url: string `json:"url"`,
		} `json:"payloads"`,

		localized_resources: []struct {
			language: string,
			license: string,
		} `json:"localizedResources"`,
	} `json:"channelItems"`,
}

msvc_packages_selector := []string {
	// MSVC binaries
	"microsoft.vc.{VER}.tools.host{HOST}.target{TARGET}.base",
	"microsoft.vc.{VER}.tools.host{HOST}.target{TARGET}.res.base",
	// MSVC headers
	"microsoft.vc.{VER}.crt.headers.base",
	// MSVC libs
	"microsoft.vc.{VER}.crt.{TARGET}.desktop.base",
	"microsoft.vc.{VER}.crt.{TARGET}.store.base",
	// MSVC runtime source
	"microsoft.vc.{VER}.crt.source.base",
	// ASAN
	"microsoft.vc.{VER}.asan.headers.base",
	"microsoft.vc.{VER}.asan.{TARGET}.base",
	// MSVC redist
	//"microsoft.vc.{VER}.crt.redist.x64.base",
}

sdk_packages_selector := []string {
	// Windows SDK tools (like rc.exe & mt.exe)
	"Windows SDK for Windows Store Apps Tools-x86_en-us.msi",
	// Windows SDK headers
	"Windows SDK for Windows Store Apps Headers-x86_en-us.msi",
	"Windows SDK Desktop Headers x86-x86_en-us.msi",
	// Windows SDK libs
	"Windows SDK for Windows Store Apps Libs-x86_en-us.msi",
	"Windows SDK Desktop Libs {TARGET}-x86_en-us.msi",
	// CRT headers & libs
	"Universal CRT Headers Libraries and Sources-x86_en-us.msi",
	// CRT redist
	//"Universal CRT Redistributable-x86_en-us.msi",
}

// returns false if has anything except numbers and a dot
is_numeric_version :: proc(s: string) -> bool {
	for r in s do switch r {
		case '0'..='9', '.': continue
		case: return false
	}
	return true
}

get_msi_cabs :: proc(msi: []byte, allocator := context.allocator) -> []string {
	msi := msi
	cabs := make([dynamic]string, allocator)

	for {
		idx := strings.index(string(msi), ".cab")
		if idx < 0 do break

		cab := string(msi[idx-32:idx+4])
		append(&cabs, strings.clone(cab, allocator))
		msi = msi[idx+4:]
	}

	return cabs[:]
}

get_temp_dir :: proc(allocator := context.allocator) -> (string, runtime.Allocator_Error) {
	n := win32.GetTempPathW(0, nil)
	if n == 0 {
		return "", nil
	}
	b := make([]u16, max(win32.MAX_PATH, n), context.temp_allocator)
	n = win32.GetTempPathW(u32(len(b)), raw_data(b))

	if n == 3 && b[1] == ':' && b[2] == '\\' {

	} else if n > 0 && b[n-1] == '\\' {
		n -= 1
	}
	return win32.utf16_to_utf8(b[:n], allocator)
}

// Converts package object into an array of Payload
get_payloads :: proc(pkg: json.Object, allocator := context.allocator) -> (payloads: [dynamic]Payload) {
	payloads = make([dynamic]Payload, allocator)

	for p in pkg["payloads"].(json.Array) {
		p := p.(json.Object)

		payload: Payload = {
			file_name = p["fileName"].(json.String),
			sha256 = p["sha256"].(json.String),
			size = i64(p["size"].(json.Float)),
			url = p["url"].(json.String),
		}

		append(&payloads, payload)
	}

	return
}

write_to_pipe :: #force_inline proc(pipe: win32.HANDLE, message: string) {
	if pipe != nil {
		os.write(auto_cast pipe, transmute([]byte)message)
	} else {
		fmt.println(message)
	}
}

// Write progress to either progress bar or to a named pipe
write_progress :: proc(pipe: win32.HANDLE, state: w.Progress_Bar_State, message: string, percentage: int = -1) {
	if pipe != nil {
		if percentage >= 0 {
			write_to_pipe(pipe, fmt.tprintf("PRC{}", percentage))
		}

		#partial switch state {
		case .Error:
			write_to_pipe(pipe, fmt.tprintf("ERR{}", message))
		case .Unknown:
			write_to_pipe(pipe, "UNK")
			write_to_pipe(pipe, fmt.tprintf("MSG{}", message))
		case .Normal:
			write_to_pipe(pipe, fmt.tprintf("MSG{}", message))
		}

		return
	}

	// from another function
	if gui_mode {
		w.set_taskbar_progress(percentage, state)
		w.set_progress_bar(widgets[.InstallProgress], percentage, state)
		if message != "" {
			w.console_println(widgets[.Console], message)
		}
		return
	}

	// from console
	if percentage >= 0 {
		fmt.printf("[{}%%] {}\n", percentage, message)
		return
	}

	if message != "" {
		#partial switch state {
		case .Error:
			fmt.println("[ERROR]", message)
		case:
			fmt.println(message)
		}
	}
	return
}

fully_qualify_path :: proc(p: string, allocator := context.allocator) -> string {
	if filepath.volume_name(p) != "" do return p

	cur_dir := os.get_current_directory(context.temp_allocator)
	return filepath.join({cur_dir, p}, allocator)
}

copy_file :: proc(from, to: string) -> i32 {
	from := from
	to := to
	from = fully_qualify_path(from, context.temp_allocator)
	to = fully_qualify_path(to, context.temp_allocator)

	file_op: win32.SHFILEOPSTRUCTW = {
		nil,
		win32.FO_COPY,
		win32.utf8_to_wstring(fmt.tprintf("{}\x00", from)), // the string must be double-null terminated
		win32.utf8_to_wstring(fmt.tprintf("{}\x00", to)),
		win32.FOF_NOCONFIRMATION | win32.FOF_NOERRORUI | win32.FOF_SILENT,
		false,
		nil,
		nil,
	}
	return win32.SHFileOperationW(&file_op)
}

move_file :: proc(from, to: string) -> i32 {
	from := from
	to := to
	from = fully_qualify_path(from, context.temp_allocator)
	to = fully_qualify_path(to, context.temp_allocator)

	file_op: win32.SHFILEOPSTRUCTW = {
		nil,
		win32.FO_MOVE,
		win32.utf8_to_wstring(fmt.tprintf("{}\x00", from)), // the string must be double-null terminated
		win32.utf8_to_wstring(fmt.tprintf("{}\x00", to)),
		win32.FOF_NOCONFIRMATION | win32.FOF_NOERRORUI | win32.FOF_SILENT,
		false,
		nil,
		nil,
	}
	return win32.SHFileOperationW(&file_op)
}

// Downloads info to temp directory
download_info :: proc(pipe: win32.HANDLE = nil, allocator := context.allocator) -> Maybe(string) {
	arena: virtual.Arena
	// I hope Microsoft doesn't make their .json files bigger than this
	// But who knows, might need to bump up to 64GB later
	_ = virtual.arena_init_static(&arena, 4 * mem.Gigabyte)
	defer virtual.arena_destroy(&arena)
	context.allocator = virtual.arena_allocator(&arena)

	// 0.1. Find temp directory
	temp_dir: string
	{
		err: runtime.Allocator_Error
		temp_dir, err = get_temp_dir()
		if err != nil {
			write_progress(pipe, .Error, fmt.tprint("Finding temp directory error:", err))
			return nil
		}
	}

	// 1. Download MANIFEST_URL
	write_progress(pipe, .Unknown, "Downloading first manifest...")

	first_manifest_path := filepath.join({temp_dir, "VSManifest.json"})
	{
		ok := download_file(pipe = pipe, url = MANIFEST_URL, to = first_manifest_path)
		if !ok {
			write_progress(pipe, .Error, fmt.tprint("Downloading first manifest failed with code:", win32.GetLastError()))
			return nil
		}
	}

	// 2. Parse manifest file, get license url and second manifest url
	write_progress(pipe, .Normal, "Reading...", 10)

	first_manifest: First_Manifest
	{
		data, ok := os.read_entire_file(first_manifest_path)
		if !ok {
			write_progress(pipe, .Error, "Reading manifest file failed.")
			return nil
		}

		err := json.unmarshal(data, &first_manifest)
		if err != nil {
			write_progress(pipe, .Error, fmt.tprint("First manifest unmarshal error:", err))
			return nil
		}
	}

	second_manifest_url: string
	second_manifest_name: string
	// NOTE: Seems that manifest reports the size wrong, unless I'm supposed to do some conversion or something.
	// Thus, this is more of a suggestion, not an actual accurate size.
	second_manifest_size: int
	license_url: string
	{
		write_progress(pipe, .Normal, "Parsing...", 20)

		OUTER: for i in first_manifest.channel_items {
			switch i.id {
			case "Microsoft.VisualStudio.Product.BuildTools":
				for r in i.localized_resources {
					if r.language != "en-us" do continue

					license_url = r.license
					break
				}
			case "Microsoft.VisualStudio.Manifests.VisualStudio":
				second_manifest_name = i.payloads[0].file_name
				second_manifest_url = i.payloads[0].url
				second_manifest_size = i.payloads[0].size
			}
		}
	}

	// 3. Download second manifest
	write_progress(pipe, .Unknown, "Downloading second manifest...")

	second_manifest_path := filepath.join({temp_dir, second_manifest_name})
	{
		ok := download_file(pipe = pipe, url = second_manifest_url, to = second_manifest_path, total_size = cast(uint)second_manifest_size)
		if !ok {
			write_progress(pipe, .Error, fmt.tprint("Downloading second manifest failed with code:", win32.GetLastError()))
			return nil
		}
	}

	// 4. Parse second manifest file, get versions and packages
	write_progress(pipe, .Normal, "Reading...", 40)

	second_manifest: json.Value
	{
		data, ok := os.read_entire_file(second_manifest_path)
		if !ok {
			write_progress(pipe, .Error, fmt.tprint("Reading", second_manifest_name, "failed."))
			return nil
		}

		err: json.Error
		second_manifest, err = json.parse(data)
		if err != nil {
			write_progress(pipe, .Error, fmt.tprint("Second manifest unmarshal error:", err))
			return nil
		}
	}

	msvc_versions := make([dynamic]string)
	sdk_versions := make([dynamic]string)
	sdk_long_versions := make([dynamic]string)
	msvc_packages := make([dynamic]Package_Info)
	sdk_packages := make([dynamic]Package_Info)
	debug_crt_runtime := make([dynamic]Debug_Package_Info)
	{
		write_progress(pipe, .Normal, "Parsing...", 50)

		manifest_packages := second_manifest.(json.Object)["packages"].(json.Array)

		// Collect all possible verions
		for pkg in manifest_packages {
			pkg := pkg.(json.Object)
			pkg_id := pkg["id"].(json.String)
			id := strings.to_lower(pkg_id, context.temp_allocator)

			if strings.has_prefix(id, "microsoft.visualstudio.component.vc.") && strings.has_suffix(id, ".x86.x64") {
				fields := strings.split(id, ".", context.temp_allocator)
				if len(fields) < 10 do continue

				ver := strings.join(fields[4:8], ".", context.temp_allocator)
				if !is_numeric_version(ver) do continue
				ver = strings.clone(ver)

				append(&msvc_versions, ver)
				continue
			}

			if strings.has_prefix(id, "microsoft.visualstudio.component.windows10sdk.") || strings.has_prefix(id, "microsoft.visualstudio.component.windows11sdk.") {
				fields := strings.split(id, ".", context.temp_allocator)
				if len(fields) < 5 do continue
				if !is_numeric_version(fields[4]) do continue

				deps := pkg["dependencies"].(json.Object)
				// NOTE: length should always be 1
				keys, _ := slice.map_keys(deps, context.temp_allocator)
				pid := strings.clone(keys[0])
				ver := strings.clone(fields[4])

				append(&sdk_long_versions, pid)
				append(&sdk_versions, ver)
				continue
			}
		}

		write_progress(pipe, .Normal, "Parsing MSVC packages...", 60)
		// Collect all MSVC packages
		for pkg in manifest_packages {
			pkg := pkg.(json.Object)
			pkg_id := pkg["id"].(json.String)

			// Do not add packages that have language field that is not english
			if "language" in pkg && pkg["language"].(json.String) != "" && pkg["language"].(json.String) != "en-US" do continue

			id := strings.to_lower(pkg_id, context.temp_allocator)

			if id == "microsoft.visualcpp.runtimedebug.14" {
				debug_package := Debug_Package_Info{
					host = pkg["chip"].(json.String),
				}

				payloads := get_payloads(pkg)
				debug_package.payloads = payloads[:]
				append(&debug_crt_runtime, debug_package)
				continue
			}

			for sel in msvc_packages_selector do for ver in msvc_versions {
				pid, _ := strings.replace_all(sel, "{VER}", ver, context.temp_allocator)

				if !strings.contains(pid, "{TARGET}") {
					if pid != id do continue

					payloads := get_payloads(pkg)
					append(&msvc_packages, Package_Info{
						id = pkg_id,
						version = ver,
						payloads = payloads[:],
					})

					continue
				}

				for target in targets {
					pid, _ := strings.replace_all(pid, "{TARGET}", target, context.temp_allocator)

					if !strings.contains(pid, "{HOST}") {
						if pid != id do continue

						payloads := get_payloads(pkg)
						append(&msvc_packages, Package_Info{
							id = pkg_id,
							version = ver,
							target = target,
							payloads = payloads[:],
						})

						continue
					}

					for host in hosts {
						pid, _ := strings.replace_all(pid, "{HOST}", host, context.temp_allocator)
						if pid != id do continue

						payloads := get_payloads(pkg)
						append(&msvc_packages, Package_Info{
							id = pkg_id,
							version = ver,
							target = target,
							host = host,
							payloads = payloads[:],
						})
					}
				}
			}
		}

		write_progress(pipe, .Normal, "Parsing SDK packages...", 70)
		// Collect all SDK packages
		for pkg in manifest_packages {
			pkg := pkg.(json.Object)
			pkg_id := pkg["id"].(json.String)

			for ver, index in sdk_versions {
				pid := sdk_long_versions[index]

				if pkg_id != pid do continue

				payloads := get_payloads(pkg)

				target: string
				if "machineArch" in pkg do target = pkg["machineArch"].(json.String)
				target = strings.to_lower(target)

				host: string
				if "productArch" in pkg do host = pkg["productArch"].(json.String)
				switch strings.to_lower(host, context.temp_allocator) {
				case "", "neutral": host = ""
				case: host = strings.to_lower(host)
				}

				append(&sdk_packages, Package_Info{
					id = pid,
					version = ver,
					target = target,
					host = host,
					payloads = payloads[:],
				})
			}
		}
	}
	write_progress(pipe, .Normal, "Saving info...", 90)

	info: Tools_Info = {
		license_url = license_url,
		msvc_versions = msvc_versions[:],
		sdk_versions = sdk_versions[:],
		msvc_packages = msvc_packages[:],
		sdk_packages = sdk_packages[:],
		debug_crt_runtime = debug_crt_runtime[:],
	}

	info_path: string
	// 5. Marshal manifest into a file in a temporary folder
	{
		data, err := json.marshal(info, {pretty = true})
		if err != nil {
			write_progress(pipe, .Error, fmt.tprint("Marshaling info error:", err))
			return nil
		}

		info_path = filepath.join({temp_dir, "BuildToolsInfo.json"})

		if !os.write_entire_file(info_path, data) {
			write_progress(pipe, .Error, fmt.tprint("Writing info file failed:", info_path))
			return nil
		}
	}

	info_path = strings.clone(info_path, allocator)

	write_progress(pipe, .Normal, "Download completed!", 100)

	return info_path
}

load_info :: proc(info_path: string, pipe: win32.HANDLE = nil) -> (success: bool) {
	write_progress(pipe, .Normal, "Loading info...", 10)

	data, ok := os.read_entire_file(info_path)
	if !ok {
		write_progress(pipe, .Error, fmt.tprintf("Reading {} failed.", info_path))
		return
	}
	defer delete(data)

	write_progress(pipe, .Normal, "Parsing info...", 50)

	err := json.unmarshal(data, &tools_info)
	if err != nil {
		write_progress(pipe, .Error, fmt.tprint("Unmarshal error:", err))
		return
	}

	slice.reverse_sort(tools_info.msvc_versions)
	slice.reverse_sort(tools_info.sdk_versions)

	success = true

	write_progress(pipe, .Normal, "Info loaded!", 100)

	return
}

// NOTE: this function assumes that directory for the output file already exists
download_file :: proc(pipe: win32.HANDLE = nil, url, to: string, total_size: uint = 0) -> (success: bool) {
	context.allocator = runtime.default_allocator()

	p := slashpath.split_elements(strings.trim_prefix(url, "https://"))
	defer delete(p)

	site_url := p[0]
	rest_of_the_url := slashpath.join(p[1:])
	defer delete(rest_of_the_url)

	// Specify an HTTPS server.
	hconnect := winhttp.Connect(internet_session, win32.utf8_to_wstring(site_url), winhttp.INTERNET_DEFAULT_HTTPS_PORT, 0)
	if hconnect == nil {
		write_progress(pipe, .Unknown, fmt.tprint("Connect failed"))
		return false
	}
	defer winhttp.CloseHandle(hconnect)

	// Create an HTTPS request handle.
	hrequest := winhttp.OpenRequest(hconnect, win32.L("GET"), win32.utf8_to_wstring(rest_of_the_url), nil, nil, nil, winhttp.WINHTTP_FLAG_SECURE)
	if hrequest == nil {
		write_progress(pipe, .Unknown, fmt.tprint("Request failed"))
		return false
	}
	defer winhttp.CloseHandle(hrequest)

	// Send a request.
	if !winhttp.SendRequest(hrequest, nil, 0, nil, 0, 0, 0) {
		return false
	}
	// End the request.
	if !winhttp.ReceiveResponse(hrequest, nil) {
		return false
	}

	fd, err := os.open(to, os.O_WRONLY|os.O_CREATE, 0)
	if err != os.ERROR_NONE {
		return false
	}
	defer os.close(fd)

	out_buf := make([dynamic]byte, 8 * mem.Kilobyte)
	defer delete(out_buf)
	downloaded: uint
	last_percentage: int
	for {
		chunk_size: win32.DWORD
		if !winhttp.QueryDataAvailable(hrequest, &chunk_size) {
			return false
		}

		if chunk_size <= 0 do break

		resize(&out_buf, cast(int)chunk_size)

		dl: win32.DWORD
		if !winhttp.ReadData(hrequest, raw_data(out_buf), chunk_size, &dl) {
			return false
		}

		os.write(fd, out_buf[:])
		downloaded += uint(dl)
		percentage := int(f32(downloaded)/f32(total_size) * 100)

		// Skip if total_size is unknown; or percentage didn't change (to prevent progress bar flickering).
		if total_size == 0 || last_percentage == percentage {
			continue
		}

		last_percentage = percentage

		if gui_mode || percentage % 10 == 0 {
			write_progress(pipe, .Normal, "", percentage)
		}
	}

	return true
}

download_payload :: proc(pipe: win32.HANDLE, payload: Payload, dir: string) -> (success: bool) {
	file_name := strings.trim_prefix(payload.file_name, `Installers\`)

	to_path := filepath.join({dir, file_name})
	defer delete(to_path)

	return download_file(url = payload.url, to = to_path, total_size = cast(uint)payload.size)
}

// Always make a directory and all directories above it, if needed, do not err if it already exists
make_directory_always :: proc(p: string) -> i32 {
	p := p
	p = fully_qualify_path(p, context.temp_allocator)

	err := win32.SHCreateDirectoryExW(nil, win32.utf8_to_wstring(p), nil)
	if err == auto_cast win32.ERROR_ALREADY_EXISTS {
		return 0
	}

	return err
}

// Remove file or directory recursively
remove_recursively :: proc(p: string) -> i32 {
	p := p
	p = fully_qualify_path(p, context.temp_allocator)

	file_op: win32.SHFILEOPSTRUCTW = {
		nil,
		win32.FO_DELETE,
		win32.utf8_to_wstring(fmt.tprintf("{}\x00", p)), // the string must be double-null terminated
		nil,
		win32.FOF_NOCONFIRMATION | win32.FOF_NOERRORUI | win32.FOF_SILENT,
		false,
		nil,
		nil,
	}
	return win32.SHFileOperationW(&file_op)
}

unpack_msvc_package :: proc(pipe: win32.HANDLE, src, dst: string) -> bool {
	FILE_NAME_BUFFER_SIZE :: 512
	file_name_buffer: [FILE_NAME_BUFFER_SIZE]byte

	data, ok := os.read_entire_file(src)
	if !ok {
		return false
	}
	defer delete(data)

	zip_file: mz.zip_archive
	ok = cast(bool)mz.zip_reader_init_mem(&zip_file, raw_data(data), len(data), 0)
	if !ok {
		return false
	}

	num_files := mz.zip_reader_get_num_files(&zip_file)
	for i in 0..<num_files {
		if mz.zip_reader_is_file_a_directory(&zip_file, i) {
			continue
		}

		mz.zip_reader_get_filename(&zip_file, i, &file_name_buffer[0], FILE_NAME_BUFFER_SIZE)
		file_name := string(cstring(&file_name_buffer[0]))
		if !strings.has_prefix(file_name, "Contents/") {
			continue
		}

		file_name = strings.trim_prefix(file_name, "Contents/")
		dst_filename := filepath.join({dst, file_name}, context.temp_allocator)
		dir := strings.trim_suffix(dst_filename, filepath.base(dst_filename))
		make_directory_always(dir)
		if !mz.zip_reader_extract_to_file(&zip_file, i, dst_filename, 0) {
			fmt.println("BAD ERROR")
		}
	}

	result := cast(bool)mz.zip_end(&zip_file)
	return result
}

install_msi_package :: proc(pipe: win32.HANDLE, msi_path, install_path: string) -> bool {
	params := fmt.tprintf(`/a "{}" /quiet /qn TARGETDIR="{}"`, fully_qualify_path(msi_path, context.temp_allocator), fully_qualify_path(install_path, context.temp_allocator))

	ShExecInfo: win32.SHELLEXECUTEINFOW = {
		cbSize = size_of(win32.SHELLEXECUTEINFOW),
		fMask = win32.SEE_MASK_NOCLOSEPROCESS,
		lpFile = L("msiexec.exe"),
		lpParameters = win32.utf8_to_wstring(params),
		nShow = win32.SW_SHOW,
	}
	ok := win32.ShellExecuteExW(&ShExecInfo)
	if !ok {
		return false
	}
	win32.WaitForSingleObject(ShExecInfo.hProcess, win32.INFINITE)
	return true
}

install_debug_runtime :: proc(pipe: win32.HANDLE, cab_path, install_path: string) -> bool {
	params := fmt.tprintf(`"{}" -F:* "{}"`, cab_path, fully_qualify_path(install_path, context.temp_allocator))

	ShExecInfo: win32.SHELLEXECUTEINFOW = {
		cbSize = size_of(win32.SHELLEXECUTEINFOW),
		fMask = win32.SEE_MASK_NOCLOSEPROCESS,
		lpFile = L("expand.exe"),
		lpParameters = win32.utf8_to_wstring(params),
		nShow = win32.SW_SHOW,
	}
	ok := win32.ShellExecuteExW(&ShExecInfo)
	if !ok {
		return false
	}
	win32.WaitForSingleObject(ShExecInfo.hProcess, win32.INFINITE)
	return true
}

cli :: proc() {
	pipe_path, env_type: string
	script_name := devcmd_script_name

	for arg in os.args[1:] {
		a := strings.to_lower(arg)

		// if there are no arguments, program launches a GUI, so this is a placeholder argument to launch it in CLI mode
		if a == "cli" {
			continue
		}

		if a == "accept_license" {
			accept_license = true
			continue
		}

		if strings.has_prefix(a, "msvc=") {
			msvc_version = strings.trim_prefix(a, "msvc=")
			continue
		}
		if strings.has_prefix(a, "sdk=") {
			sdk_version = strings.trim_prefix(a, "sdk=")
			continue
		}
		if strings.has_prefix(a, "target=") {
			target_arch = strings.trim_prefix(a, "target=")
			continue
		}
		if strings.has_prefix(a, "host=") {
			host_arch = strings.trim_prefix(a, "host=")
			continue
		}

		if strings.has_prefix(a, "install_path=") {
			install_path = strings.trim_prefix(arg, "install_path=")
			install_path = strings.trim(install_path, `"`)
			install_path = filepath.clean(install_path)
			continue
		}

		if strings.has_prefix(a, "script=") {
			script_name = strings.trim_prefix(arg, "script=")
			script_name = strings.trim(script_name, `"`)
			continue
		}

		if strings.has_prefix(a, "env=") {
			env_type = strings.trim_prefix(a, "env=")
			continue
		}

		if strings.has_prefix(a, "pipe=") {
			pipe_path = strings.trim_prefix(arg, "pipe=")
			continue
		}

		if strings.has_prefix(a, "info=") {
			info_path = strings.trim_prefix(arg, "info=")
			info_path = strings.trim(info_path, `"`)
			continue
		}

		usage := `
usage: PortableBuildTools.exe [cli] [accept_license] [msvc=MSVC version] [sdk=SDK version] [target=x64/x86/arm/arm64] [host=x64/x86] [install_path=C:\BuildTools] [script=name] [env=local/global]

*cli: make PortableBuildTools run in command-line interface mode

*accept_license: auto-accept the license if including in argument list [default: ask]

*msvc=MSVC version: Which MSVC toolchain version to install. See GUI application for available versions [default: latest in GUI]

*sdk=SDK Version: Which Windows SDK version to install. See GUI application for available versions [default: latest in GUI]

*target=x64/x86/arm/arm64: [default: x64]

*host=x64/x86: [default: x64]

*install_path=path: [default: C:\BuildTools]

*script=filename to give developer command prompt batch file. Do not include .bat extension [default: devcmd]

*env=local/global: If supplied, then the installed path will be added to PATH environment variable, locally (for the current user) or globally (for all users) [default: not set, which means don't add to PATH]`
		
		if a != "--help" && a != "/?" && a != "-h" {
			fmt.printf("Argument {} is unknown\n", arg)
		}

		fmt.println(usage)
		return
	}

	pipe: win32.HANDLE
	if pipe_path != "" {
		pipe = win32.CreateFileW(
			win32.utf8_to_wstring(pipe_path),
			win32.GENERIC_WRITE,
			0, nil,
			win32.OPEN_EXISTING,
			win32.FILE_ATTRIBUTE_NORMAL,
			nil,
		)

		if pipe == win32.INVALID_HANDLE_VALUE {
			show_message_box(.Error, "PIPE ERROR", fmt.tprintf("Unable to open the pipe: error code {}", win32.GetLastError()))
			return
		}
	}
	defer if pipe != nil do write_to_pipe(pipe, "Q")

	if install_path == "" {
		install_path = DEFAULT_INSTALL_PATH
		write_progress(pipe, .Normal, fmt.tprint("[default installation path]:", install_path))
	}
	install_path = filepath.clean(install_path)
	if !is_valid_install_path(install_path) {
		write_progress(pipe, .Error, "Installation path is invalid.")
		return
	}

	if info_path == "" {
		ok: bool
		info_path, ok = download_info(pipe).?
		if !ok do return
	}

	if !load_info(info_path, pipe) do return

	free_all(context.temp_allocator)

	temp_dir, err := get_temp_dir()
	if err != nil {
		write_progress(pipe, .Error, "Unable to get temp dir.")
		return
	}

	if msvc_version == "" {
		msvc_version = tools_info.msvc_versions[0]
		write_progress(pipe, .Normal, fmt.tprint("[default msvc version]:", msvc_version))
	}
	if sdk_version == "" {
		sdk_version = tools_info.sdk_versions[0]
		write_progress(pipe, .Normal, fmt.tprint("[default sdk version]:", sdk_version))
	}
	if target_arch == "" {
		target_arch = targets[0]
		write_progress(pipe, .Normal, fmt.tprint("[default target arch]:", target_arch))
	}
	if host_arch == "" {
		host_arch = hosts[0]
		write_progress(pipe, .Normal, fmt.tprint("[default host arch]:", host_arch))
	}

	// NOTE: these should never trigger when launched from GUI, so we use os.read
	if !accept_license {
		write_progress(pipe, .Normal, fmt.tprint("Do you accept the license? [Y/n]", tools_info.license_url))
		buf: [32]byte
		n, _ := os.read(os.stdin, buf[:]) // NOTE: assume no error
		answer := strings.trim_space(string(buf[:n]))
		switch strings.to_lower(answer, context.temp_allocator) {
		case "y", "yes", "":
			break
		case:
			write_progress(pipe, .Error, "You cannot install build tools without accepting the license.")
			return
		}
	}

	write_progress(pipe, .Normal, "Creating installation path...", 0)
	// 1. Create install path
	{
		if err := make_directory_always(install_path); err != 0 {
			write_progress(pipe, .Error, fmt.tprint("Failed to create install path:", err))
			return
		}
	}

	// 2. Create path in temp directory
	packages_path := filepath.join({temp_dir, "packages"})
	{
		if os.exists(packages_path) {
			if err := remove_recursively(packages_path); err != 0 {
				write_progress(pipe, .Error, fmt.tprint("Failed to clear packages path:", err))
				return
			}
		}

		if err := make_directory_always(packages_path); err != 0 {
			write_progress(pipe, .Error, fmt.tprint("Failed to create packages path:", err))
			return
		}
	}

	// 3. Download msvc packages
	write_progress(pipe, .Normal, "Downloading MSVC packages...", 10)
	for p in tools_info.msvc_packages {
		if p.version != "" && p.version != msvc_version do continue
		if p.target != "" && p.target != target_arch do continue
		if p.host != "" && p.host != host_arch do continue

		for payload in p.payloads {
			write_progress(pipe, .Normal, payload.file_name)

			if !download_payload(pipe, payload, packages_path) {
				write_progress(pipe, .Error, fmt.tprintf("Failed to download msvc package {} [{}] with code: {}", payload.file_name, payload.url, win32.GetLastError()))
				return
			}

			package_path := filepath.join({packages_path, payload.file_name}, context.temp_allocator)
			if !unpack_msvc_package(pipe = pipe, src = package_path, dst = install_path) {
				write_progress(pipe, .Error, fmt.tprint("Failed to unpack msvc package:", package_path))
				return
			}
		}
	}

	// 4. Download sdk packages
	cabs: [dynamic]string
	msis: [dynamic]string
	write_progress(pipe, .Normal, "Downloading SDK packages...", 50)
	for p in tools_info.sdk_packages {
		if p.version != "" && p.version != sdk_version do continue
		if p.target != "" && p.target != target_arch do continue

		for payload in p.payloads do for selector in sdk_packages_selector {
			s, _ := strings.replace_all(selector, "{TARGET}", target_arch, context.temp_allocator)
			if !strings.has_prefix(payload.file_name, `Installers\`) || !strings.has_suffix(payload.file_name, s) {
				continue
			}

			write_progress(pipe, .Normal, filepath.base(payload.file_name))

			if !download_payload(pipe, payload, packages_path) {
				write_progress(pipe, .Error, fmt.tprint("Failed to download sdk package:", payload.file_name))
				return
			}

			// Get a list of '.cab's from downloaded '.msi' packages
			msi_package_path := filepath.join({packages_path, s})
			data, ok := os.read_entire_file(msi_package_path)
			if !ok {
				write_progress(pipe, .Error, fmt.tprint("Failed to read sdk package:", payload.file_name))
				return
			}
			defer delete(data)

			package_cabs := get_msi_cabs(data)
			append(&cabs, ..package_cabs[:])
			append(&msis, strings.trim_prefix(payload.file_name, `Installers\`))
		}

		// 5. Download needed cabs
		write_progress(pipe, .Normal, "Downloading CAB packages...", 70)
		for payload in p.payloads do for cab in cabs {
			if !strings.has_suffix(payload.file_name, cab) {
				continue
			}

			write_progress(pipe, .Normal, cab)

			if !download_payload(pipe, payload, packages_path) {
				write_progress(pipe, .Error, fmt.tprint("Failed to download cab:", payload.file_name, payload.url))
				return
			}
		}
	}

	// 6. Install msi packages
	write_progress(pipe, .Normal, "Installing MSI packages...", 80)
	for msi, idx in msis {
		msi_path := filepath.join({packages_path, msi}, context.temp_allocator)

		write_progress(pipe, .Normal, fmt.tprint(filepath.base(msi_path)))

		if !install_msi_package(pipe, msi_path, install_path) {
			write_progress(pipe, .Error, fmt.tprint("Failed to install msi:", msi))
			return
		}
	}

	msvcv_matches, _ := filepath.glob(filepath.join({install_path, "VC/Tools/MSVC/*"}))
	sdkv_matches, _ := filepath.glob(filepath.join({install_path, "Windows Kits/10/bin/*"}))
	msvcv := filepath.base(msvcv_matches[0])
	sdkv := filepath.base(sdkv_matches[0])

	// 7. Download and install debug crt runtime
	for debug_package in tools_info.debug_crt_runtime {
		if debug_package.host != host_arch {
			continue
		}

		write_progress(pipe, .Normal, "Installing debug runtime...", 85)

		for payload in debug_package.payloads {
			if !download_payload(pipe, payload, packages_path) {
				write_progress(pipe, .Error, fmt.tprint("Failed to download debug package:", payload.file_name, payload.url))
				return
			}
		}

		dst := filepath.join({install_path, "VC/Tools/MSVC", msvcv, strings.join({"bin/Host", host_arch}, ""), target_arch})
		if err := make_directory_always(dst); err != 0 {
			write_progress(pipe, .Error, fmt.tprint("Failed to create debug package path:", err))
			return
		}
		dst_tmp := filepath.join({packages_path, "TMP_DBG"}, context.temp_allocator)
		if err := make_directory_always(dst_tmp); err != 0 {
			write_progress(pipe, .Error, fmt.tprint("Failed to create temporary debug package path:", err))
			return
		}

		msi: string
		for payload in debug_package.payloads {
			if strings.has_suffix(payload.file_name, ".msi") {
				msi = payload.file_name
				break
			}
		}
		msi_path := filepath.join({packages_path, msi}, context.temp_allocator)
		if !install_msi_package(pipe, msi_path, dst_tmp) {
			write_progress(pipe, .Error, fmt.tprint("Failed to install msi:", msi))
			return
		}

		dir, _ := os.open(dst_tmp)
		defer os.close(dir)

		files_info, _ := os.read_dir(dir, -1)
		for fi in files_info {
			if strings.has_prefix(fi.name, "System") && fi.is_dir {
				msi_dir, _ := os.open(fi.fullpath)
				defer os.close(msi_dir)

				msi_files_info, _ := os.read_dir(msi_dir, -1)
				for msi_fi in msi_files_info {
					if move_file(msi_fi.fullpath, filepath.join({dst, msi_fi.name}, context.temp_allocator)) != 0 {
						write_progress(pipe, .Error, fmt.tprint("Failed to move file:", msi_fi.fullpath))
						return
					}
				}

				break
			}
		}

		break
	}

	// TODO: In accordance with Martin's script DIA SDK needs to be added here
	// https://gist.github.com/mmozeiko/7f3162ec2988e81e56d5c4e22cde9977/revisions#diff-a6303b82561a4061c71e8838976143f06f295cc9a8a60cf53fc170cb5b9f3f1dR250-R271
	{
		//write_progress(pipe, .Normal, "Installing DIA SDK...", 90)
	}

	// 9. Variable preparations
	install_path = fully_qualify_path(install_path)
	msvc_arch := target_arch
	sdk_arch := target_arch

	msvc_root := filepath.join({install_path, "VC", "Tools", "MSVC", msvcv})
	sdk_dir := filepath.join({install_path, "Windows Kits", "10"})
	sdk_include := filepath.join({sdk_dir, "Include", sdkv})
	sdk_libs := filepath.join({sdk_dir, "Lib", sdkv})

	vc_tools_install_dir := fmt.tprintf(`{}\`, msvc_root)
	windows_sdk_dir := fmt.tprintf(`{}\`, sdk_dir)

	msvc_bin := filepath.join({msvc_root, "bin", strings.join({"Host", msvc_arch}, ""), msvc_arch})
	sdk_bin_paths: []string = {
		filepath.join({sdk_dir, "bin", sdkv, sdk_arch}),
		filepath.join({sdk_dir, "bin", sdkv, sdk_arch, "ucrt"}),
	}

	include_paths: []string = {
		filepath.join({msvc_root, "include"}),
		filepath.join({sdk_include, "ucrt"}),
		filepath.join({sdk_include, "shared"}),
		filepath.join({sdk_include, "um"}),
		filepath.join({sdk_include, "winrt"}),
		filepath.join({sdk_include, "cppwinrt"}),
	}

	lib_paths: []string = {
		filepath.join({msvc_root, "lib", msvc_arch}),
		filepath.join({sdk_libs, "ucrt", sdk_arch}),
		filepath.join({sdk_libs, "um", sdk_arch}),
	}

	sdk_bin := strings.join(sdk_bin_paths, ";")
	include := strings.join(include_paths, ";")
	lib := strings.join(lib_paths, ";")

	// 10. Creating setup script if needed
	if script_name != "" {
		write_progress(pipe, .Normal, "Creating setup script...")

SETUP_SCRIPT :: `@echo off

set WindowsSDKDir={}
set WindowsSDKVersion={}
set VCToolsInstallDir={}
set VSCMD_ARG_TGT_ARCH={}

set MSVC_BIN={}
set SDK_BIN={}
set PATH=%%MSVC_BIN%%;%%SDK_BIN%%;%%PATH%%
set INCLUDE={}
set LIB={}
`

		data := fmt.aprintf(SETUP_SCRIPT, sdk_dir, sdkv, vc_tools_install_dir, target_arch, msvc_bin, sdk_bin, include, lib)
		file_name := fmt.tprintf("{}.bat", script_name)
		file_path := filepath.join({install_path, file_name})
		os.write_entire_file(file_path, transmute([]byte)data)
	}

	// 11. Adding to environment if needed
	ADD_TO_ENV: if env_type == "local" || env_type == "global" {
		write_progress(pipe, .Normal, "Adding environment variables...")

		location: Env_Location = .Global if env_type == "global" else .Local

		// These 3 are for Odin, but they are not necessary.
		if set_env_in_registry("VCToolsInstallDir", vc_tools_install_dir, location) != .None {
			write_progress(pipe, .Error, fmt.tprint("Failed to save VCToolsInstallDir"))
		}
		if set_env_in_registry("WindowsSDKDir", windows_sdk_dir, location) != .None {
			write_progress(pipe, .Error, fmt.tprint("Failed to save WindowsSDKDir"))
		}
		if set_env_in_registry("WindowsSDKVersion", sdkv, location) != .None {
			write_progress(pipe, .Error, fmt.tprint("Failed to save WindowsSDKVersion"))
		}
		if set_env_in_registry("VSCMD_ARG_TGT_ARCH", target_arch, location) != .None {
			write_progress(pipe, .Error, fmt.tprint("Failed to save VSCMD_ARG_TGT_ARCH"))
		}

		// These 2 are necessary
		if set_env_in_registry("INCLUDE", include, location) != .None {
			write_progress(pipe, .Error, fmt.tprint("Failed to save INCLUDE"))
		}
		if set_env_in_registry("LIB", lib, location) != .None {
			write_progress(pipe, .Error, fmt.tprint("Failed to save LIB"))
		}

		if set_env_in_registry("MSVC_BIN", msvc_bin, location) != .None {
			write_progress(pipe, .Error, fmt.tprint("Failed to save MSVC_BIN"))
		}
		if set_env_in_registry("SDK_BIN", sdk_bin, location) != .None {
			write_progress(pipe, .Error, fmt.tprint("Failed to save SDK_BIN"))
		}

		env_path, err := get_env_from_registry("PATH", location)
		if err != .None && err != .Not_Found {
			write_progress(pipe, .Error, fmt.tprint("Failed to get PATH"))
			break ADD_TO_ENV
		}

		path := add_to_env_if_not_exists(env_path, "%MSVC_BIN%;%SDK_BIN%")
		if set_env_in_registry("PATH", path, location, true) != .None {
			write_progress(pipe, .Error, fmt.tprint("Failed to save PATH"))
		}
	}

	// 8. Cleanup
	write_progress(pipe, .Normal, "Doing cleanup...", 95)
	{
		remove_recursively(filepath.join({install_path, "Common7"}))
		remove_recursively(filepath.join({install_path, "VC/Tools/MSVC", msvcv, "Auxiliary"}))
		remove_recursively(filepath.join({install_path, "VC/Tools/MSVC", msvcv, "lib", target_arch, "store"}))
		remove_recursively(filepath.join({install_path, "VC/Tools/MSVC", msvcv, "lib", target_arch, "uwp"}))

		msi_glob, _ := filepath.glob(filepath.join({install_path, "*.msi"}))
		for msi in msi_glob {
			remove_recursively(msi)
		}

		remove_recursively(filepath.join({install_path, "Windows Kits/10", "Catalogs"}))
		remove_recursively(filepath.join({install_path, "Windows Kits/10", "DesignTime"}))
		remove_recursively(filepath.join({install_path, "Windows Kits/10", "bin", sdkv, "chpe"}))
		remove_recursively(filepath.join({install_path, "Windows Kits/10", "Lib", sdkv, "ucrt_enclave"}))

		for arch in targets {
			if arch == target_arch do continue

			remove_recursively(filepath.join({install_path, "VC/Tools/MSVC", msvcv, strings.join({"bin/Host", arch}, "")}))
			remove_recursively(filepath.join({install_path, "Windows Kits/10/bin", sdkv, arch}))
			remove_recursively(filepath.join({install_path, "Windows Kits/10/Lib", sdkv, "ucrt", arch}))
			remove_recursively(filepath.join({install_path, "Windows Kits/10/Lib", sdkv, "um", arch}))
		}

		// Remove vctip.exe, so it doesn't start when running cl.exe or link.exe.
		remove_recursively(filepath.join({msvc_bin, "vctip.exe"}))

		if err := remove_recursively(packages_path); err != 0 {
			write_progress(pipe, .Error, fmt.tprint("Failed to clear packages path:", err))
			return
		}
	}

	write_progress(pipe, .Normal, "Done!", 100)
}
