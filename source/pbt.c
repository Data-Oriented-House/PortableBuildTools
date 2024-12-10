#include "base.c"
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_ZLIB_APIS
#define MINIZ_NO_ZLIB_COMPATIBLE_NAME
#define MINIZ_NO_MALLOC
#define NDEBUG
#include "miniz/miniz.c"
#include "window.h"
#include <shobjidl_core.h>
#include <shlwapi.h>
#include <wininet.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "wininet.lib")
#define utf8_to_utf16(str, str_count, wbuf, wbuf_count) \
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str, str_count, wbuf, wbuf_count)
#define utf16_to_utf8(wstr, wstr_count, buf, buf_count) \
	WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstr, wstr_count, buf, buf_count, null, null)
#define MAX_ENV_LEN 32767
#define MAX_CMDLINE_LEN 32767

//[of]:Strings
bool string_has_char(const char* s, char x)
{
	bool match = false;
	while (*s != 0) {
		match = (*s == x) ? true : match;
	}
	return (match);
}

//[c]Check if the string includes any of the bytes in the buffer
bool string_has_any(const char* s, buffer(const char, buf))
{
	i64 s_count = string_count(s);
	bool match = false;
	for (i64 i = 0; i < s_count; i++) {
		for (i64 j = 0; j < buf_count; j++) {
			match = (s[i] == buf[j]) ? true : match;
		}
	}
	return (match);
}

//[c]Check if the string includes only the bytes in the buffer, and no other bytes
bool string_has_only(const char* s, buffer(const char, buf))
{
	i64 s_count = string_count(s);
	bool match = true;
	for (i64 i = 0; i < s_count; i++) {
		bool match_byte = false;
		for (i64 j = 0; j < buf_count; j++) {
			match_byte = (s[i] == buf[j]) ? true : match_byte;
		}
		match = match_byte ? match : false;
	}
	return (match);
}

bool string_starts_with(const char* s, const char* x)
{
	bool match = true;
	while (*x != 0) {
		match = (*s != *x) ? false : match;
		if (*s == 0) {
			break;
		}
		s++;
		x++;
	}
	return (match);
}

bool string_ends_with(const char* s, const char* x) {
	i64 s_count = string_count(s);
	i64 x_count = string_count(x);
	i64 diff = clamp_bot(s_count - x_count, 0);
	return string_starts_with(s + diff, x);
}

bool string_trim_start(char* s, const char* x)
{
	i64 s_count = string_count(s);
	i64 x_count = string_count(x);
	bool trim = string_starts_with(s, x);
	i64 copy_size = s_count - x_count + 1;
	copy_size = trim ? copy_size : 0;
	memcpy(s, s + x_count, copy_size);
	return (trim);
}

bool string_trim_end(char* s, const char* x)
{
	i64 s_count = string_count(s);
	i64 x_count = string_count(x);
	bool trim = string_ends_with(s, x);
	char byte = s[s_count - x_count];
	s[s_count - x_count] = trim ? 0 : byte;
	return (trim);
}

void string_lower(char* s)
{
	while (*s != 0) {
		char lower = *s + ('a' - 'A');
		*s = ((*s >= 'A') & (*s <= 'Z')) ? lower : *s;
		s++;
	}
}

//[c]TODO: minus sign, check if number is outside the representable range, check if non-digits, etcetc. This is barely usable right now.
i64 string_to_i64(const char* s)
{
	i64 base = 1;
	i64 out = 0;
	i64 s_count = string_count(s);
	for (i64 i = s_count - 1; i >= 0; i -= 1) {
		i64 digit = s[i] - '0';
		out += (base * digit);
		base *= 10;
	}
	return (out);
}
//[cf]
//[of]:Files
typedef struct {
	void*	handle;
	i64	size;
	i64	pos;
} file_handle;

typedef enum {
	file_mode_read,
	file_mode_write,
} file_mode;

bool file_exists(const char* path)
{
	WCHAR wpath[MAX_PATH];
	int n = utf8_to_utf16(path, -1, wpath, MAX_PATH);
	hope(n > 0, "path unicode conversion bug");
	DWORD err = GetFileAttributesW(wpath);
	bool ok = (err != INVALID_FILE_ATTRIBUTES) && !(err & FILE_ATTRIBUTE_DIRECTORY);
	return (ok);
}

void file_delete(const char* path)
{
	if (!file_exists(path)) {
		return;
	}
	WCHAR wpath[MAX_PATH];
	int n = utf8_to_utf16(path, -1, wpath, MAX_PATH);
	hope(n > 0, "path unicode conversion bug");
	hope(DeleteFileW(wpath), "failed to delete the file: ", path);
}

void file_create(const char* path)
{
	WCHAR wpath[MAX_PATH];
	int n = utf8_to_utf16(path, -1, wpath, MAX_PATH);
	hope(n > 0, "path unicode conversion bug");
	DWORD err = GetFileAttributesW(wpath);
	bool exists = (err != INVALID_FILE_ATTRIBUTES) && !(err & FILE_ATTRIBUTE_DIRECTORY);
	if (exists) {
		hope(DeleteFileW(wpath), "failed to delete the file: ", path);
	}
	HANDLE handle = CreateFileW(wpath, 0, 0, null, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, null);
	hope(handle != INVALID_HANDLE_VALUE, "failed to create the file: ", path);
	hope(CloseHandle(handle), "failed to close the file: ", path);
}

file_handle file_open(const char* path, file_mode mode)
{
	file_handle f = (file_handle){0};
	WCHAR wpath[MAX_PATH];
	int n = utf8_to_utf16(path, -1, wpath, MAX_PATH);
	hope(n > 0, "path unicode conversion bug");
	DWORD access = 0;
	access = (mode == file_mode_read) ? FILE_GENERIC_READ : access;
	access = (mode == file_mode_write) ? FILE_GENERIC_WRITE : access;
	SECURITY_ATTRIBUTES sa = (SECURITY_ATTRIBUTES){0};
	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = true;
	DWORD attrs = FILE_ATTRIBUTE_NORMAL;
	attrs = (mode == file_mode_read) ? FILE_ATTRIBUTE_READONLY : attrs;
	f.handle = CreateFileW(wpath, access, FILE_SHARE_READ, &sa, OPEN_EXISTING, attrs, null);
	hope(f.handle != INVALID_HANDLE_VALUE, "failed to open the file: ", path);
	hope(GetFileSizeEx(f.handle, cast(LARGE_INTEGER*, &f.size)), "failed to get the file size: ", path);
	f.pos = (mode == file_mode_read) ? 0 : f.size;
	SetFilePointerEx(f.handle, bitcast(LARGE_INTEGER, f.pos), null, FILE_BEGIN);
	return (f);
}

void file_close(file_handle* f)
{
	CloseHandle(f->handle);
}

i64 file_read(file_handle* f, buffer(char, buf))
{
	DWORD n = 0;
	ReadFile(f->handle, buf, cast(DWORD, buf_count), &n, null);
	f->pos += n;
	return (n);
}

void file_write(file_handle* f, buffer(const char, data))
{
	DWORD n = 0;
	WriteFile(f->handle, data, cast(DWORD, data_count), &n, null);
	hope(n == data_count, "file did not write enough: ", itos(n), "/", itos(data_count));
	f->pos += n;
}

void file_seek(file_handle* f, i64 pos)
{
	SetFilePointerEx(f->handle, bitcast(LARGE_INTEGER, pos), cast(LARGE_INTEGER*, &f->pos), FILE_BEGIN);
}

void file_flush(file_handle* f)
{
	FlushFileBuffers(f->handle);
}

void file_copy(const char* dst_path, const char* src_path)
{
	file_handle src = file_open(src_path, file_mode_read);
	file_handle dst = file_open(dst_path, file_mode_write);
	char chunk[mem_page_size];
	while (true) {
		i64 n = file_read(&src, chunk, countof(chunk));
		if (n == 0) {
			break;
		}
		file_write(&dst, chunk, n);
	}
	file_close(&dst);
	file_close(&src);
}
//[cf]
//[of]:Folders
typedef struct {
	HANDLE	handle;
	WIN32_FIND_DATAW	data;
} folder_handle;

bool folder_exists(const char* path)
{
	WCHAR wpath[MAX_PATH];
	utf8_to_utf16(path, -1, wpath, MAX_PATH);
	DWORD err = GetFileAttributesW(wpath);
	bool ok = (err != INVALID_FILE_ATTRIBUTES) && (err & FILE_ATTRIBUTE_DIRECTORY);
	return (ok);
}

void folder_delete(const char* path)
{
//[c]	NOTE: Resulting string must be double-null-terminated
	WCHAR wpath[MAX_PATH + 1];
	int n = utf8_to_utf16(path, -1, wpath, MAX_PATH);
	wpath[n] = 0;
	SHFILEOPSTRUCTW op = (SHFILEOPSTRUCTW){0};
	op.wFunc = FO_DELETE;
	op.pFrom = wpath;
	op.fFlags = FOF_NO_UI;
	hope(!SHFileOperationW(&op), "failed to delete the file:", path);
}

void folder_create(const char* path)
{
	WCHAR wpath[MAX_PATH];
	utf8_to_utf16(path, -1, wpath, MAX_PATH);
	DWORD err = GetFileAttributesW(wpath);
	bool exists = (err != INVALID_FILE_ATTRIBUTES) && (err & FILE_ATTRIBUTE_DIRECTORY);
	if (exists) {
		folder_delete(path);
	}
	BOOL ok = CreateDirectoryW(wpath, null);
	hope(ok, "failed to create the directory:", path);
}

folder_handle folder_open(const char* path)
{
	WCHAR wpath[MAX_PATH];
	char path_fixed[MAX_PATH * 3];
	string_format(path_fixed, countof(path_fixed), path, "\\*");
	utf8_to_utf16(path_fixed, -1, wpath, MAX_PATH);
	folder_handle f = (folder_handle){0};
//[c]	Skip .
	f.handle = FindFirstFileW(wpath, &f.data);
	hope(f.handle != INVALID_HANDLE_VALUE, "Failed to open folder: ", path);
//[c]	Skip ..
	FindNextFileW(f.handle, &f.data);
	return (f);
}

void folder_close(folder_handle* f)
{
	FindClose(f->handle);
	*f = (folder_handle){0};
}

bool folder_next(folder_handle* f, char (*file_name)[MAX_PATH * 3])
{
	bool ok = FindNextFileW(f->handle, &f->data);
	if (ok) {
		utf16_to_utf8(f->data.cFileName, -1, *file_name, countof(*file_name));
	}
	return (ok);
}
//[cf]
//[of]:JSON parser
typedef uchar (*json_peek_proc)(void* context, bool advance);
typedef struct {
	json_peek_proc	peek;
	void*	context;
} json_parser;

json_parser json_parser_begin(json_peek_proc peek, void* context)
{
	json_parser p;
	p.peek = peek;
	p.context = context;
	return (p);
}

typedef enum {
	json_none,
	json_word,
	json_number,
	json_string,
	json_object,
	json_array,
} json_value;

json_value json_value_skip(json_parser* p);

json_value json_value_next(json_parser* p)
{
	json_value out = json_none;
	for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		bool skip = ((symbol == ' ') | (symbol == '\t') | (symbol == '\n') | (symbol == '\r') | (symbol == ','));
		out = (((symbol >= 'a') & (symbol <= 'z')) | ((symbol >= 'A') & (symbol <= 'Z'))) ? json_word : out;
		out = (((symbol >= '0') & (symbol <= '9')) | (symbol == '-') | (symbol == '.')) ? json_number : out;
		out = (symbol == '"') ? json_string : out;
		out = (symbol == '{') ? json_object : out;
		out = (symbol == '[') ? json_array : out;
		if (skip) {
			continue;
		}
		break;
	}
	return (out);
}

void json_word_skip(json_parser* p)
{
	for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (((symbol >= 'a') & (symbol <= 'z')) | ((symbol >= 'A') & (symbol <= 'Z'))) {
			continue;
		}
		break;
	}
}

i64 json_number_extract_i64(json_parser* p)
{
	char n[66] = {0};
	i64 pos = 0;
	for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (((symbol >= '0') & (symbol <= '9')) | (symbol == '-') | (symbol == '.')) {
			if (pos < cast(i64, countof(n))) {
				n[pos] = symbol;
				pos++;
			}
			continue;
		}
		break;
	}
	return (string_to_i64(n));
}

void json_number_skip(json_parser* p)
{
	for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (((symbol >= '0') & (symbol <= '9')) | (symbol == '-') | (symbol == '.')) {
			continue;
		}
		break;
	}
}

void json_string_extract(json_parser* p, buffer(char, buf))
{
	buf_count--;
	for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (symbol == '"') {
			p->peek(p->context, true);
			break;
		}
	}
	i64 pos = 0;
	bool buf_filled = false;
	bool escaping = false;
	for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if ((symbol == '\\') & !escaping) {
			escaping = true;
			continue;
		}
		if ((symbol == '"') & !escaping) {
			p->peek(p->context, true);
			break;
		}
		escaping = false;
		i64 char_size = uchar_size(c8);
		buf_filled = (pos + char_size > buf_count) ? true : buf_filled;
		i64 copy_size = buf_filled ? 0 : char_size;
		memcpy(buf + pos, &c8, copy_size);
		pos += copy_size;
	}
	buf[pos] = 0;
}

bool json_string_is(json_parser* p, const char* str)
{
	i64 str_count = string_count(str);
	for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (symbol == '"') {
			p->peek(p->context, true);
			break;
		}
	}
	i64 pos = 0;
	bool match = true;
	bool escaping = false;
	for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (symbol == '\\') {
			if (!escaping) {
				escaping = true;
				continue;
			}
		}
		if (symbol == '"') {
			if (!escaping) {
				p->peek(p->context, true);
				break;
			}
		}
		escaping = false;
		i64 space_left = str_count - pos;
		i64 char_size = uchar_size(c8);
		i64 compare_size = clamp_top(char_size, space_left);
		char* bytes = cast(char*, &c8);
		for (i64 i = 0; i < compare_size; i++) {
			match = (str[pos + i] != bytes[i]) ? false : match;
		}
		pos += char_size;
	}
	return ((pos == str_count) & (match));
}

void json_string_skip(json_parser* p)
{
	for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (symbol == '"') {
			p->peek(p->context, true);
			break;
		}
	}
	bool escaping = false;
	for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (symbol == '\\') {
			if (!escaping) {
				escaping = true;
				continue;
			}
		}
		if (symbol == '"') {
			if (!escaping) {
				p->peek(p->context, true);
				break;
			}
		}
		escaping = false;
	}
}

bool json_array_next(json_parser* p)
{
	bool match = false;
	for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if ((symbol == '[') | (symbol == ' ') | (symbol == '\t') | (symbol == '\n') | (symbol == '\r') | (symbol == ',')) {
			continue;
		}
		if (symbol == ']') {
			p->peek(p->context, true);
			break;
		}
		match = true;
		break;
	}
	return (match);
}

void json_array_skip(json_parser* p)
{
	while (json_array_next(p)) {
		json_value_skip(p);
	}
}

bool json_object_next(json_parser* p)
{
	bool match = false;
	for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if ((symbol == '{') | (symbol == ' ') | (symbol == '\t') | (symbol == '\n') | (symbol == '\r') | (symbol == ',')) {
			continue;
		}
		if (symbol == '}') {
			p->peek(p->context, true);
			break;
		}
		if (symbol == '"') {
			match = true;
		}
		break;
	}
	return (match);
}

void json_object_key_extract(json_parser* p, buffer(char, buf))
{
	json_string_extract(p, buf, buf_count);
	for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (symbol == ':') {
			p->peek(p->context, true);
			break;
		}
	}
}

void json_object_key_skip(json_parser* p)
{
	json_string_skip(p);
	for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
		char symbol = *cast(char*, &c8);
		if (symbol == ':') {
			p->peek(p->context, true);
			break;
		}
	}
}

bool json_object_key_find(json_parser* p, const char* key)
{
	bool match = false;
	while (json_object_next(p) & !match) {
		match = json_string_is(p, key);
		for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true)) {
			char symbol = *cast(char*, &c8);
			if (symbol == ':') {
				p->peek(p->context, true);
				break;
			}
		}
		if (!match) {
			json_value_skip(p);
		}
	}
	return (match);
}

void json_object_skip(json_parser* p)
{
	while (json_object_next(p)) {
		json_object_key_skip(p);
		json_value_skip(p);
	}
}

json_value json_value_skip(json_parser* p)
{
	json_value v = json_value_next(p);
	if (v == json_word) {
		json_word_skip(p);
	}
	if (v == json_number) {
		json_number_skip(p);
	}
	if (v == json_string) {
		json_string_skip(p);
	}
	if (v == json_object) {
		json_object_skip(p);
	}
	if (v == json_array) {
		json_array_skip(p);
	}
	return (v);
}
//[cf]
//[of]:JSON file parser
typedef struct {
	char	cache[mem_page_size];
	uchar	c8;
	i64	cache_size;
	i64	cache_pos;
	file_handle	f;
} json_context;

uchar json_file_peek(void* context, bool advance)
{
	json_context* jc = context;
//[c]	Advance if beginning of the file, or if asked to
	if ((!advance) & (jc->f.pos != 0)) {
		return (jc->c8);
	}
//[c]	If beginning of the file, reset all values
	jc->c8 = (jc->f.pos == 0) ? 0 : jc->c8;
	jc->cache_size = (jc->f.pos == 0) ? 0 : jc->cache_size;
	jc->cache_pos = (jc->f.pos == 0) ? 0 : jc->cache_pos;
//[c]	Chunk end
	if (jc->cache_pos >= jc->cache_size) {
		jc->cache_size = file_read(&jc->f, string_to_buffer(jc->cache));
		jc->cache[countof(jc->cache)] = 0;
		jc->cache_pos = 0;
	}
//[c]	File end
	if (jc->cache_size <= 0) {
		jc->cache_pos = 0;
		jc->c8 = 0;
		return (jc->c8);
	}
	const char* str = jc->cache + jc->cache_pos;
	i64 str_size = jc->cache_size - jc->cache_pos;
	jc->c8 = string_decode_uchar(str);
	i64 char_size = uchar_size(jc->c8);
//[c]	Valid char, or invalid char in the middle of a chunk
	if ((char_size > 0) | (str_size >= 4) | (jc->f.pos == jc->f.size)) {
		jc->cache_pos += clamp_bot(char_size, 1);
		return (jc->c8);
	}
//[c]	Either invalid char or chunk end, start a new chunk just to be sure
	i64 seek_pos = jc->f.pos - (jc->cache_size - jc->cache_pos);
	file_seek(&jc->f, seek_pos);
	jc->cache_size = 0;
	jc->cache_pos = 0;
	return json_file_peek(context, advance);
}

void json_file_context_restore(json_context*	jc, json_context context)
{
	*jc = context;
	file_seek(&jc->f, jc->f.pos);
}
//[cf]
//[of]:miniz helpers
void* miniz_malloc(void* opaque, size_t items, size_t size)
{
	return HeapAlloc(GetProcessHeap(), 0, items * size);
}

void miniz_free(void* opaque, void* address)
{
	HeapFree(GetProcessHeap(), 0, address);
}

void* miniz_realloc(void* opaque, void* address, size_t items, size_t size)
{
	if (!address) {
		return HeapAlloc(GetProcessHeap(), 0, items * size);
	} else if ((items == 0) | (size == 0)) {
		HeapFree(GetProcessHeap(), 0, address);
		return NULL;
	} else {
		return HeapReAlloc(GetProcessHeap(), 0, address, items * size);
	}
}

size_t miniz_file_read(void* context, mz_uint64 file_ofs, void* buf, size_t buf_count)
{
	file_handle* f = context;
	file_seek(f, file_ofs);
	return file_read(f, buf, buf_count);
}

size_t mz_file_write(void* context, mz_uint64 file_ofs, const void* buf, size_t buf_count)
{
	file_handle* f = context;
	file_write(f, buf, buf_count);
	return (buf_count);
}
//[cf]

const char* release_vsmanifest_url = "https://aka.ms/vs/17/release/channel";
const char* preview_vsmanifest_url = "https://aka.ms/vs/17/pre/channel";

char* targets[] = {"x64", "x86", "arm", "arm64",};
char* hosts[] = {"x64", "x86", "arm64"};

bool install_start;
bool is_preview;
n64 env_mode; // 0: none; 1: user; 2: global;
char msvc_version[32];
char sdk_version[16];
char target_arch[16] = "x64";
char host_arch[16] = "x64";
char install_path[MAX_PATH * 3] = "C:\\BuildTools";
bool license_accepted;
bool is_admin;
char temp_path[MAX_PATH * 3];

char release_manifest_url[L_MAX_URL_LENGTH * 3];
char release_license_url[L_MAX_URL_LENGTH * 3];
char preview_manifest_url[L_MAX_URL_LENGTH * 3];
char preview_license_url[L_MAX_URL_LENGTH * 3];

//[c]NOTE: 13/32
char release_msvc_versions[32][16];
i64 release_msvc_versions_count;
//[c]NOTE: 14/32
char preview_msvc_versions[32][16];
i64 preview_msvc_versions_count;

//[c]NOTE: 6/16
char release_sdk_versions[16][8];
i64 release_sdk_versions_count;
//[c]NOTE: 6/16
char preview_sdk_versions[16][8];
i64 preview_sdk_versions_count;

HINTERNET internet_session;

void env_set(HKEY location, LPCWSTR subkey, LPCWSTR key, const char* env)
{
	WCHAR wenv[MAX_ENV_LEN];
	int n = utf8_to_utf16(env, -1, wenv, countof(wenv));
	RegSetKeyValueW(location, subkey, key, REG_SZ, wenv, n * sizeof(WCHAR));
}

void cleanup(void)
{
	if (internet_session) {
		InternetCloseHandle(internet_session);
		internet_session = null;
	}
	CoUninitialize();
}

void refill_gui(HWND dlg)
{
//[c]	NOTE: iterating backwards, assuming versions are sorted backwards in the JSON
	{
		char (*msvc_versions)[16] = is_preview ? preview_msvc_versions : release_msvc_versions;
		i64 msvc_versions_count = is_preview ? preview_msvc_versions_count : release_msvc_versions_count;
		HWND item = GetDlgItem(dlg, ID_COMBO_MSVC);
		SendMessageW(item, CB_RESETCONTENT, 0, 0);
		for (i64 i = msvc_versions_count - 1; i >= 0; i--) {
			WCHAR wver[MAX_ENV_LEN];
			utf8_to_utf16(msvc_versions[i], -1, wver, countof(wver));
			SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, wver));
		}
		SendMessageW(item, CB_SETCURSEL, 0, 0);
		string_copy(msvc_version, countof(msvc_version), msvc_versions[msvc_versions_count - 1]);
	}
	{
		char (*sdk_versions)[8] = is_preview ? preview_sdk_versions : release_sdk_versions;
		i64 sdk_versions_count = is_preview ? preview_sdk_versions_count : release_sdk_versions_count;
		HWND item = GetDlgItem(dlg, ID_COMBO_SDK);
		SendMessageW(item, CB_RESETCONTENT, 0, 0);
		for (i64 i = sdk_versions_count - 1; i >= 0; i--) {
			WCHAR wver[MAX_ENV_LEN];
			utf8_to_utf16(sdk_versions[i], -1, wver, countof(wver));
			SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, wver));
		}
		SendMessageW(item, CB_SETCURSEL, 0, 0);
		string_copy(sdk_version, countof(sdk_version), sdk_versions[sdk_versions_count - 1]);
	}
	{
		char* license_url = is_preview ? preview_license_url : release_license_url;
		char link_url[64 + L_MAX_URL_LENGTH * 3] = {0};
		string_format(link_url, countof(link_url), "I accept the <a href=\"", license_url, "\">License Agreement</a>");
		WCHAR wlink[64 + L_MAX_URL_LENGTH];
		utf8_to_utf16(link_url, -1, wlink, countof(wlink));
		SetDlgItemTextW(dlg, ID_SYSLINK_LICENSE, wlink);
	}
}

//[c]Updates install_path, adds shield to install button if needed
void path_update(HWND dlg)
{
	WCHAR wpath[MAX_PATH] = {0};
	GetDlgItemTextW(dlg, ID_EDIT_PATH, wpath, MAX_PATH);
	int n = utf16_to_utf8(wpath, -1, install_path, countof(install_path));
//[c]	Check if path is accessible by creating temporary path/~ file
	if (folder_exists(install_path)) {
		WCHAR check_path[MAX_PATH] = {0};
		memcpy(check_path, wpath, n * sizeof(WCHAR));
		n--;
		check_path[n] = L'\\';
		n++;
		check_path[n] = L'~';
		n++;
		check_path[n] = 0;
		HANDLE tfile = CreateFileW(check_path, GENERIC_WRITE, 0, null, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, null);
		if (tfile != INVALID_HANDLE_VALUE) {
			CloseHandle(tfile);
			is_admin = false;
			SendMessageW(GetDlgItem(dlg, ID_BUTTON_INSTALL), BCM_SETSHIELD, 0, false);
			return;
		}
	} else {
		BOOL ok = CreateDirectoryW(wpath, null);
		if (ok) {
			folder_delete(install_path);
			is_admin = false;
			SendMessageW(GetDlgItem(dlg, ID_BUTTON_INSTALL), BCM_SETSHIELD, 0, false);
			return;
		}
	}
	is_admin = (GetLastError() == ERROR_ACCESS_DENIED) ? true : false;
	SendMessageW(GetDlgItem(dlg, ID_BUTTON_INSTALL), BCM_SETSHIELD, 0, is_admin);
}

bool is_folder_empty(const char* path)
{
	folder_handle f = folder_open(path);
	char file_name[MAX_PATH * 3];
	bool has_files = folder_next(&f, &file_name);
	folder_close(&f);
	return (!has_files);
}

BOOL WINAPI window_proc(HWND dlg, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message) {
		case WM_INITDIALOG: {
			SetForegroundWindow(dlg);
			{
				HICON icon = LoadIconW(GetModuleHandleW(null), cast(LPCWSTR, ID_ICON));
				SendMessageW(dlg, WM_SETICON, ICON_SMALL, cast(LPARAM, icon));
				SendMessageW(dlg, WM_SETICON, ICON_BIG, cast(LPARAM, icon));
			}
			{
				HWND item = GetDlgItem(dlg, ID_COMBO_ENV);
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"Create environment setup scripts"));
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"Create the scripts & add to user environment"));
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"Create the scripts & add to global environment"));
				SendMessageW(item, CB_SETCURSEL, 0, 0);
			}
			{
				HWND item = GetDlgItem(dlg, ID_COMBO_TARGET);
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"x64"));
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"x86"));
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"arm"));
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"arm64"));
				SendMessageW(item, CB_SETCURSEL, 0, 0);
			}
			{
				HWND item = GetDlgItem(dlg, ID_COMBO_HOST);
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"x64"));
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"x86"));
				SendMessageW(item, CB_ADDSTRING, 0, cast(LPARAM, L"arm64"));
				SendMessageW(item, CB_SETCURSEL, 0, 0);
			}
			{
				HWND item = GetDlgItem(dlg, ID_EDIT_PATH);
				SendMessageW(item, WM_SETTEXT, 0, cast(LPARAM, L"C:\\BuildTools"));
				SHAutoComplete(item, SHACF_FILESYSTEM);
			}
			refill_gui(dlg);
			path_update(dlg);
			EnableWindow(GetDlgItem(dlg, ID_BUTTON_INSTALL), false);
			return (true);
		} break;
		case WM_CLOSE: {
			EndDialog(dlg, 0);
			return (true);
		} break;
		case WM_COMMAND: {
			WORD* wparam_words = cast(WORD*, &wparam);
			switch (wparam_words[0]) {
				case ID_CHECK_PREVIEW: {
					is_preview = (IsDlgButtonChecked(dlg, ID_CHECK_PREVIEW) == BST_CHECKED);
					refill_gui(dlg);
				} break;
				case ID_COMBO_ENV: {
					env_mode = SendMessageW(GetDlgItem(dlg, ID_COMBO_ENV), CB_GETCURSEL, 0, 0);
				} break;
				case ID_COMBO_MSVC: {
					i64 index = SendMessageW(GetDlgItem(dlg, ID_COMBO_MSVC), CB_GETCURSEL, 0, 0);
					string_copy(msvc_version, countof(msvc_version), is_preview ? preview_msvc_versions[preview_msvc_versions_count - index - 1] : release_msvc_versions[release_msvc_versions_count - index - 1]);
				} break;
				case ID_COMBO_SDK: {
					i64 index = SendMessageW(GetDlgItem(dlg, ID_COMBO_SDK), CB_GETCURSEL, 0, 0);
					string_copy(sdk_version, countof(sdk_version), is_preview ? preview_sdk_versions[preview_sdk_versions_count - index - 1] : release_sdk_versions[release_sdk_versions_count - index - 1]);
				} break;
				case ID_COMBO_TARGET: {
					i64 index = SendMessageW(GetDlgItem(dlg, ID_COMBO_TARGET), CB_GETCURSEL, 0, 0);
					string_copy(target_arch, countof(target_arch), targets[index]);
				} break;
				case ID_COMBO_HOST: {
					i64 index = SendMessageW(GetDlgItem(dlg, ID_COMBO_HOST), CB_GETCURSEL, 0, 0);
					string_copy(host_arch, countof(host_arch), hosts[index]);
				} break;
				case ID_EDIT_PATH: {
					if (wparam_words[1] == EN_CHANGE) {
						path_update(dlg);
					}
				} break;
				case ID_BUTTON_BROWSE: {
					IFileOpenDialog* fd;
					CoCreateInstance(&CLSID_FileOpenDialog, null, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, cast(void**, &fd));
//[c]					NOTE: Randomly generated GUID
					vcall(fd, lpVtbl->SetClientGuid, &(GUID){0x65CDF695, 0x0A2C, 0x46DB, {0x9F, 0xE8, 0x73, 0xCF, 0xA7, 0xF7, 0x1E, 0x65}});
					DWORD options;
					vcall(fd, lpVtbl->GetOptions, &options);
					vcall(fd, lpVtbl->SetOptions, options | FOS_PICKFOLDERS);
					if (vcall(fd, lpVtbl->Show, null) == 0) {
						IShellItem* psi;
						vcall(fd, lpVtbl->GetResult, &psi);
						{
							LPWSTR wpath;
							vcall(psi, lpVtbl->GetDisplayName, SIGDN_DESKTOPABSOLUTEPARSING, &wpath);
							SetDlgItemTextW(dlg, ID_EDIT_PATH, wpath);
							CoTaskMemFree(wpath);
						}
						vcall(psi, lpVtbl->Release);
					}
					vcall(fd, lpVtbl->Release);
				} break;
				case ID_CHECK_LICENSE: {
					license_accepted = (IsDlgButtonChecked(dlg, ID_CHECK_LICENSE) == BST_CHECKED);
					EnableWindow(GetDlgItem(dlg, ID_BUTTON_INSTALL), license_accepted);
				} break;
				case ID_BUTTON_INSTALL: {
					if (folder_exists(install_path)) {
						if (!is_folder_empty(install_path)) {
							MessageBoxW(dlg, L"Destination folder is not empty", null, MB_TOPMOST | MB_ICONERROR);
							break;
						}
					} else {
//[c]						Creating a folder to check if it possible to create one
						WCHAR wpath[MAX_PATH];
						utf8_to_utf16(install_path, -1, wpath, MAX_PATH);
						if (CreateDirectoryW(wpath, null)) {
							folder_delete(install_path);
						} else if (GetLastError() != ERROR_ACCESS_DENIED) {
							MessageBoxW(dlg, L"Destination folder cannot be made", null, MB_TOPMOST | MB_ICONERROR);
							break;
						}
					}
					install_start = true;
					EndDialog(dlg, 0);
					return (true);
				} break;
			}
		} break;
		case WM_NOTIFY: {
			NMHDR* param = cast(NMHDR*, lparam);
			if ((param->code == NM_CLICK) || (param->code == NM_RETURN)) {
				NMLINK* link = cast(NMLINK*, lparam);
				if (link->hdr.idFrom == ID_SYSLINK_LICENSE) {
					ShellExecuteW(null, L"open", link->item.szUrl, null, null, SW_NORMAL);
				}
			}
		} break;
	}
	return (false);
}

void parse_vsmanifest(const char* path, bool preview)
{
	echoln("Parsing ", preview ? "Preview" : "Release", " Visual Studio Manifest...");
	char manifest_url[countof(release_manifest_url)];
	char license_url[countof(release_license_url)];
	json_context jc;
	jc.f = file_open(path, file_mode_read);
	json_parser p = json_parser_begin(json_file_peek, &jc);
	hope(json_object_key_find(&p, "channelItems"), "channelItems not found");
	while (json_array_next(&p)) {
		json_context obj_state = jc;
		if (!json_object_key_find(&p, "id")) {
			json_object_skip(&p);
			continue;
		}
		char id[64];
		json_string_extract(&p, id, countof(id));
		if (string_is(id, preview ? "Microsoft.VisualStudio.Manifests.VisualStudioPreview" : "Microsoft.VisualStudio.Manifests.VisualStudio")) {
			json_file_context_restore(&jc, obj_state);
			hope(json_object_key_find(&p, "payloads"), "payloads not found");
			json_array_next(&p);
			hope(json_object_key_find(&p, "url"), "manifest url not found");
			json_string_extract(&p, manifest_url, countof(manifest_url));
			json_object_skip(&p);
			json_array_next(&p);
		}
		if (string_is(id, "Microsoft.VisualStudio.Product.BuildTools")) {
			json_file_context_restore(&jc, obj_state);
			hope(json_object_key_find(&p, "localizedResources"), "localizedResources not found");
			while (json_array_next(&p)) {
				json_context res_state = jc;
				hope(json_object_key_find(&p, "language"), "license language not found");
				char language[8];
				json_string_extract(&p, language, countof(language));
				if (string_is(language, "en-us")) {
					json_file_context_restore(&jc, res_state);
					hope(json_object_key_find(&p, "license"), "license url not found");
					json_string_extract(&p, license_url, countof(license_url));
					json_object_skip(&p);
					break;
				}
				json_object_skip(&p);
			}
		}
		json_object_skip(&p);
	}
	file_close(&jc.f);
	memcpy(preview ? preview_manifest_url : release_manifest_url, manifest_url, sizeof(manifest_url));
	memcpy(preview ? preview_license_url : release_license_url, license_url, sizeof(license_url));
}

void parse_manifest(const char* path, bool preview)
{
	echoln("Parsing ", preview ? "Preview" : "Release", " Build Tool Manifest...");
	char msvc_versions[32][16] = {0};
	i64 msvc_versions_count = 0;
	char sdk_versions[16][8] = {0};
	i64 sdk_versions_count = 0;
	json_context jc;
	jc.f = file_open(path, file_mode_read);
	json_parser p = json_parser_begin(json_file_peek, &jc);
	hope(json_object_key_find(&p, "packages"), "packages not found");
	while (json_array_next(&p)) {
		hope(json_object_key_find(&p, "id"), "id not found");
		char id[64];
		json_string_extract(&p, id, countof(id));
		char id_lower[countof(id)];
		memcpy(id_lower, id, sizeof(id));
		string_lower(id_lower);
		bool is_msvc_version = true;
		is_msvc_version = string_trim_start(id_lower, "microsoft.visualstudio.component.vc.") ? is_msvc_version : false;
		is_msvc_version = string_trim_end(id_lower, ".x86.x64") ? is_msvc_version : false;
		bool is_sdk_version = false;
		is_sdk_version = string_trim_start(id_lower, "microsoft.visualstudio.component.windows10sdk.") ? true : is_sdk_version;
		is_sdk_version = string_trim_start(id_lower, "microsoft.visualstudio.component.windows11sdk.") ? true : is_sdk_version;
		bool is_digit = string_has_only(id_lower, string_to_buffer(".0123456789"));
		if (is_msvc_version & is_digit) {
			string_copy(msvc_versions[msvc_versions_count], countof(msvc_versions[msvc_versions_count]), id_lower);
			msvc_versions_count++;
		}
		if (is_sdk_version & is_digit) {
			string_copy(sdk_versions[sdk_versions_count], countof(sdk_versions[sdk_versions_count]), id_lower);
			sdk_versions_count++;
		}
		json_object_skip(&p);
	}
	file_close(&jc.f);
	memcpy(preview ? preview_sdk_versions : release_sdk_versions, sdk_versions, sizeof(sdk_versions));
	memcpy(preview ? preview_msvc_versions : release_msvc_versions, msvc_versions, sizeof(msvc_versions));
	release_msvc_versions_count = preview ? release_msvc_versions_count : msvc_versions_count;
	preview_msvc_versions_count = preview ? msvc_versions_count : preview_msvc_versions_count;
	release_sdk_versions_count = preview ? release_sdk_versions_count : sdk_versions_count;
	preview_sdk_versions_count = preview ? sdk_versions_count : preview_sdk_versions_count;
}

bool download_file(const char* file_url, const char* file_path, const char* display_name)
{
	WCHAR wurl[L_MAX_URL_LENGTH];
	utf8_to_utf16(file_url, -1, wurl, countof(wurl));
	DWORD flags = INTERNET_FLAG_DONT_CACHE;
	if(string_starts_with(file_url, "https://")) {
		flags |= INTERNET_FLAG_SECURE;
	}
	HINTERNET connection = InternetOpenUrlW(internet_session, wurl, null, 0, flags, 0);
	if (!connection) {
		echoln("open url failed");
		return (false);
	}
	DWORD content_size;
	if (!HttpQueryInfoW(connection, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &content_size, &(DWORD){sizeof(content_size)}, null)) {
		content_size = 0;
	}
	file_create(file_path);
	file_handle f = file_open(file_path, file_mode_write);
	i64 current_size = 0;
	DWORD n = 0;
	char chunk[16 * mem_page_size];
	while (true) {
		if (!InternetReadFile(connection, chunk, countof(chunk), &n)){
			file_close(&f);
			echoln("\ndownload failed...");
			return (false);
		}
		if (n == 0) {
			break;
		}
		file_write(&f, chunk, n);
		current_size += n;
		if (content_size > 0) {
			i64 p = (current_size * 100) / content_size;
			p = clamp_top(p, 100);
			echo("\r[", itos(p), "%] ", display_name);
		} else {
			echo("\r[", itos(current_size / mem_kilobyte), "kb] ", display_name);
		}
	}
	if (content_size > 0) {
		echo("\r[100%] ", display_name);
	}
	echoln("");
	file_close(&f);
	InternetCloseHandle(connection);
	return (true);
}

void download_file_always(const char* file_url, const char* file_path, const char* display_name)
{
	while (!download_file(file_url, file_path, display_name)) {
		echoln("retrying...");
	}
}

void extract_payload_info(json_context* jc, json_parser* p, char (*file_name)[MAX_PATH * 3], char (*url)[L_MAX_URL_LENGTH * 3])
{
//[c]	TODO: extract and validate sha256 (given sha256 doesn't seem to correspond to the file's actual sha256)
	json_context payload_state = *jc;
	hope(json_object_key_find(p, "fileName"), "fileName not found");
	json_string_extract(p, array_to_buffer(*file_name));
	json_file_context_restore(jc, payload_state);
	hope(json_object_key_find(p, "url"), "url not found");
	json_string_extract(p, array_to_buffer(*url));
	json_file_context_restore(jc, payload_state);
}

void install(void)
{
//[c]	Create temporary folders
	char msvc_path[MAX_PATH * 3];
	char sdk_path[MAX_PATH * 3];
	string_format(msvc_path, countof(msvc_path), temp_path, "\\msvc");
	string_format(sdk_path, countof(sdk_path), temp_path, "\\sdk");
	folder_create(msvc_path);
	folder_create(sdk_path);
	{
		char msvc_packages[32][128];
		i64 msvc_packages_count = 0;
		string_format(msvc_packages[msvc_packages_count], countof(msvc_packages[msvc_packages_count]), "microsoft.visualcpp.dia.sdk");
		msvc_packages_count++;
		string_format(msvc_packages[msvc_packages_count], countof(msvc_packages[msvc_packages_count]), "microsoft.vc.", msvc_version, ".crt.headers.base");
		msvc_packages_count++;
		string_format(msvc_packages[msvc_packages_count], countof(msvc_packages[msvc_packages_count]), "microsoft.vc.", msvc_version, ".crt.source.base");
		msvc_packages_count++;
		string_format(msvc_packages[msvc_packages_count], countof(msvc_packages[msvc_packages_count]), "microsoft.vc.", msvc_version, ".asan.headers.base");
		msvc_packages_count++;
		string_format(msvc_packages[msvc_packages_count], countof(msvc_packages[msvc_packages_count]), "microsoft.vc.", msvc_version, ".pgo.headers.base");
		msvc_packages_count++;
		string_format(msvc_packages[msvc_packages_count], countof(msvc_packages[msvc_packages_count]), "microsoft.vc.", msvc_version, ".tools.host", host_arch, ".target", target_arch, ".base");
		msvc_packages_count++;
		string_format(msvc_packages[msvc_packages_count], countof(msvc_packages[msvc_packages_count]), "microsoft.vc.", msvc_version, ".tools.host", host_arch, ".target", target_arch, ".res.base");
		msvc_packages_count++;
		string_format(msvc_packages[msvc_packages_count], countof(msvc_packages[msvc_packages_count]), "microsoft.vc.", msvc_version, ".crt.", target_arch, ".desktop.base");
		msvc_packages_count++;
		string_format(msvc_packages[msvc_packages_count], countof(msvc_packages[msvc_packages_count]), "microsoft.vc.", msvc_version, ".crt.", target_arch, ".store.base");
		msvc_packages_count++;
		string_format(msvc_packages[msvc_packages_count], countof(msvc_packages[msvc_packages_count]), "microsoft.vc.", msvc_version, ".premium.tools.host", host_arch, ".target", target_arch, ".base");
		msvc_packages_count++;
		string_format(msvc_packages[msvc_packages_count], countof(msvc_packages[msvc_packages_count]), "microsoft.vc.", msvc_version, ".pgo.", target_arch, ".base");
		msvc_packages_count++;
		string_format(msvc_packages[msvc_packages_count], countof(msvc_packages[msvc_packages_count]), "microsoft.vc.", msvc_version, ".asan.x86.base");
		msvc_packages_count++;
		string_format(msvc_packages[msvc_packages_count], countof(msvc_packages[msvc_packages_count]), "microsoft.vc.", msvc_version, ".asan.x64.base");
		msvc_packages_count++;
		char sdk_packages[32][128];
		i64 sdk_packages_count = 0;
		string_format(sdk_packages[sdk_packages_count], countof(sdk_packages[sdk_packages_count]), "Windows SDK for Windows Store Apps Tools-x86_en-us.msi");
		sdk_packages_count++;
		string_format(sdk_packages[sdk_packages_count], countof(sdk_packages[sdk_packages_count]), "Windows SDK for Windows Store Apps Headers-x86_en-us.msi");
		sdk_packages_count++;
		string_format(sdk_packages[sdk_packages_count], countof(sdk_packages[sdk_packages_count]), "Windows SDK for Windows Store Apps Headers OnecoreUap-x86_en-us.msi");
		sdk_packages_count++;
		string_format(sdk_packages[sdk_packages_count], countof(sdk_packages[sdk_packages_count]), "Windows SDK for Windows Store Apps Libs-x86_en-us.msi");
		sdk_packages_count++;
		string_format(sdk_packages[sdk_packages_count], countof(sdk_packages[sdk_packages_count]), "Universal CRT Headers Libraries and Sources-x86_en-us.msi");
		sdk_packages_count++;
		string_format(sdk_packages[sdk_packages_count], countof(sdk_packages[sdk_packages_count]), "Windows SDK Desktop Libs ", target_arch, "-x86_en-us.msi");
		sdk_packages_count++;
		for (i64 i = 0; i < countof(targets); i++) {
			char* t = targets[i];
			string_format(sdk_packages[sdk_packages_count], countof(sdk_packages[sdk_packages_count]), "Windows SDK Desktop Headers ", t, "-x86_en-us.msi");
			sdk_packages_count++;
			string_format(sdk_packages[sdk_packages_count], countof(sdk_packages[sdk_packages_count]), "Windows SDK OnecoreUap Headers ", t, "-x86_en-us.msi");
			sdk_packages_count++;
		}
		char sdk_package[32] = {0};
		json_context jc;
		char manifest_path[MAX_PATH * 3];
		string_format(manifest_path, countof(manifest_path), temp_path, "\\", is_preview ? "BuildToolsManifest.Preview.json" : "BuildToolsManifest.Release.json");
		echoln("Parsing ", is_preview ? "Preview" : "Release", " Build Tool Manifest...");
		jc.f = file_open(manifest_path, file_mode_read);
		json_parser p = json_parser_begin(json_file_peek, &jc);
		hope(json_object_key_find(&p, "packages"), "packages not found");
		json_context packages_state = jc;
		while (json_array_next(&p)) {
			hope(json_object_key_find(&p, "id"), "id not found");
			char id[64];
			json_string_extract(&p, id, countof(id));
			char id_lower[countof(id)];
			memcpy(id_lower, id, sizeof(id));
			string_lower(id_lower);
			bool is_sdk = false;
			is_sdk = string_starts_with(id_lower, "microsoft.visualstudio.component.windows10sdk.") ? true : is_sdk;
			is_sdk = string_starts_with(id_lower, "microsoft.visualstudio.component.windows11sdk.") ? true : is_sdk;
			if (is_sdk & string_ends_with(id_lower, sdk_version)) {
				hope(json_object_key_find(&p, "dependencies"), "dependencies not found");
				hope(json_object_next(&p), "dependencies is empty");
				json_object_key_extract(&p, sdk_package, countof(sdk_package));
				break;
			}
			json_object_skip(&p);
		}
		json_file_context_restore(&jc, packages_state);
		bool redist_found = false;
		char redist_suffix[32] = {0};
		string_format(redist_suffix, countof(redist_suffix), string_is(target_arch, "arm") ? ".onecore.desktop" : "");
		char redist_name[128];
		string_format(redist_name, countof(redist_name), "microsoft.vc.", msvc_version, ".crt.redist.", target_arch, redist_suffix, ".base");
		while (json_array_next(&p)) {
			hope(json_object_key_find(&p, "id"), "id not found");
			char id[64];
			json_string_extract(&p, id, countof(id));
			char id_lower[countof(id)];
			memcpy(id_lower, id, sizeof(id));
			string_lower(id_lower);
			if (string_is(id_lower, redist_name)) {
				redist_found = true;
				string_format(msvc_packages[msvc_packages_count], countof(msvc_packages[msvc_packages_count]), redist_name);
				msvc_packages_count++;
				break;
			}
			json_object_skip(&p);
		}
		json_file_context_restore(&jc, packages_state);
		if (!redist_found) {
			string_format(redist_name, countof(redist_name), "microsoft.visualcpp.crt.redist.", target_arch, redist_suffix);
			while (json_array_next(&p)) {
				hope(json_object_key_find(&p, "id"), "id not found");
				char id[64];
				json_string_extract(&p, id, countof(id));
				char id_lower[countof(id)];
				memcpy(id_lower, id, sizeof(id));
				string_lower(id_lower);
				if (string_is(id_lower, redist_name)) {
					redist_found = true;
					hope(json_object_key_find(&p, "dependencies"), "dependencies not found");
					while (json_object_next(&p)) {
						char redist_package[128];
						json_object_key_extract(&p, redist_package, countof(redist_package));
						if (string_ends_with(redist_package, ".base")) {
							string_lower(redist_package);
							string_format(msvc_packages[msvc_packages_count], countof(msvc_packages[msvc_packages_count]), redist_package);
							msvc_packages_count++;
							break;
						}
						json_object_skip(&p);
					}
					break;
				}
				json_object_skip(&p);
			}
		}
		json_file_context_restore(&jc, packages_state);
		echoln("Downloading the packages...");
		while (json_array_next(&p)) {
			json_context pkg_state = jc;
			hope(json_object_key_find(&p, "id"), "id not found");
			char id[64];
			json_string_extract(&p, array_to_buffer(id));
			json_file_context_restore(&jc, pkg_state);
			char lang[8] = {0};
			if (json_object_key_find(&p, "language")) {
				json_string_extract(&p, array_to_buffer(lang));
			}
			json_file_context_restore(&jc, pkg_state);
			char id_lower[countof(id)];
			memcpy(id_lower, id, sizeof(id));
			string_lower(id_lower);
			bool msvc_pkg = false;
			for (i64 i = 0; i < msvc_packages_count; i++) {
				msvc_pkg = string_is(id_lower, msvc_packages[i]) ? true : msvc_pkg;
			}
			msvc_pkg = (string_is(lang, "en-US") | string_is(lang, "")) ? msvc_pkg : false;
			if (msvc_pkg) {
				hope(json_object_key_find(&p, "payloads"), "payloads not found");
				while (json_array_next(&p)) {
					char file_name[MAX_PATH * 3];
					char url[L_MAX_URL_LENGTH * 3];
					extract_payload_info(&jc, &p, &file_name, &url);
					json_object_skip(&p);
					char path[MAX_PATH * 3];
					string_format(path, countof(path), msvc_path, "\\", file_name);
					download_file_always(url, path, file_name);
				}
			}
			if (string_is(id, sdk_package)) {
				char msi_cabs[64][37];
				i64 msi_cabs_count = 0;
				hope(json_object_key_find(&p, "payloads"), "payloads not found");
				json_context payloads_state = jc;
//[c]				Download SDK payloads
				while (json_array_next(&p)) {
					char file_name[MAX_PATH * 3];
					char url[L_MAX_URL_LENGTH * 3];
					extract_payload_info(&jc, &p, &file_name, &url);
					json_object_skip(&p);
					if (string_trim_start(file_name, "Installers\\")) {
						bool sdk_pkg = false;
						for (i64 i = 0; i < sdk_packages_count; i++) {
							sdk_pkg = string_is(file_name, sdk_packages[i]) ? true : sdk_pkg;
						}
						if (sdk_pkg) {
							char path[MAX_PATH * 3];
							string_format(path, countof(path), sdk_path, "\\", file_name);
							download_file_always(url, path, file_name);
//[c]							Extract .cab file info
							file_handle msi = file_open(path, file_mode_read);
							char chunk[mem_page_size];
							while (true) {
								i64 n = file_read(&msi, chunk, countof(chunk));
								if (n <= 0) {
									break;
								}
								for (i64 i = 0; i < n; i++) {
									char str[4];
									str[0] = chunk[i];
									str[1] = ((i + 1) < n) ? chunk[i + 1] : 0;
									str[2] = ((i + 2) < n) ? chunk[i + 2] : 0;
									str[3] = ((i + 3) < n) ? chunk[i + 3] : 0;
									bool match = (str[0] == '.') & (str[1] == 'c') & (str[2] == 'a') & (str[3] == 'b');
									if (match) {
										i64 index = msi.pos - countof(chunk) + i;
										file_handle cabs_reader = file_open(path, file_mode_read);
										file_seek(&cabs_reader, index - 32);
										file_read(&cabs_reader, msi_cabs[msi_cabs_count], 36);
										file_close(&cabs_reader);
										msi_cabs[msi_cabs_count][36] = 0;
										msi_cabs_count++;
									}
								}
								if (msi.pos == msi.size) {
									break;
								}
								file_seek(&msi, msi.pos - 3);
							}
							file_close(&msi);
						}
					}
				}
				json_file_context_restore(&jc, payloads_state);
//[c]				Download .cab packages
				while (json_array_next(&p)) {
					char file_name[MAX_PATH * 3];
					char url[L_MAX_URL_LENGTH * 3];
					extract_payload_info(&jc, &p, &file_name, &url);
					json_object_skip(&p);
					if (string_trim_start(file_name, "Installers\\")) {
						bool cab_pkg = false;
						for (i64 i = 0; i < msi_cabs_count; i++) {
							cab_pkg = string_is(file_name, msi_cabs[i]) ? true : cab_pkg;
						}
						if (cab_pkg) {
							char path[MAX_PATH * 3];
							string_format(path, countof(path), sdk_path, "\\", file_name);
							download_file_always(url, path, file_name);
						}
					}
				}
			}
			json_object_skip(&p);
		}
		file_close(&jc.f);
	}
	echoln("Installing MSVC...");
	{
		folder_handle msvc_folder = folder_open(msvc_path);
		char file_name[MAX_PATH * 3];
		while (folder_next(&msvc_folder, &file_name)) {
			char file_path[MAX_PATH * 3];
			string_format(file_path, countof(file_path), msvc_path, "\\", file_name);
			file_handle msvc_file = file_open(file_path, file_mode_read);
			mz_zip_archive zip_file = (mz_zip_archive){0};
			zip_file.m_pAlloc = miniz_malloc;
			zip_file.m_pFree = miniz_free;
			zip_file.m_pRealloc = miniz_realloc;
			zip_file.m_pRead = miniz_file_read;
			zip_file.m_pIO_opaque = &msvc_file;
			hope(mz_zip_reader_init(&zip_file, msvc_file.size, 0), "Failed to unzip file: ", file_name);
			i64 n = mz_zip_reader_get_num_files(&zip_file);
			for (i64 i = 0; i < n; i++) {
				char zipped_file_name[MAX_PATH * 3];
				if (mz_zip_reader_is_file_a_directory(&zip_file, i)) {
					continue;
				}
				mz_zip_reader_get_filename(&zip_file, i, zipped_file_name, countof(zipped_file_name));
				if (!string_trim_start(zipped_file_name, "Contents/")) {
					continue;
				}
				char zipped_file_path[MAX_PATH * 3];
				string_format(zipped_file_path, countof(zipped_file_path), install_path, "\\", zipped_file_name);
//[c]				Go through the path, creating directories if needed, and converting forward slashes into backslashes
				char* s = zipped_file_path;
				while (*s != 0) {
					if (*s == '/') {
						*s = 0;
						if (!folder_exists(zipped_file_path)) {
							folder_create(zipped_file_path);
						}
						*s = '\\';
					}
					s++;
				}
				file_create(zipped_file_path);
				file_handle zf = file_open(zipped_file_path, file_mode_write);
				hope(mz_zip_reader_extract_to_callback(&zip_file, i, mz_file_write, &zf, 0), "Failed to extract file: ", zipped_file_path);
				file_close(&zf);
			}
			hope(mz_zip_end(&zip_file), "Failed to close zip file gracefully: ", file_name);
			file_close(&msvc_file);
		}
		folder_close(&msvc_folder);
	}
	echoln("Installing SDK...");
	{
		folder_handle sdk_folder = folder_open(sdk_path);
		char file_name[MAX_PATH * 3];
		while (folder_next(&sdk_folder, &file_name)) {
			if (!string_ends_with(file_name, ".msi")) {
				continue;
			}
			char file_path[MAX_PATH * 3];
			string_format(file_path, countof(file_path), sdk_path, "\\", file_name);
			char params[MAX_CMDLINE_LEN * 3];
			string_format(params, countof(params), "/a \"", file_path, "\" /quiet TARGETDIR=\"", install_path, "\"");
			WCHAR wparams[MAX_CMDLINE_LEN];
			utf8_to_utf16(params, -1, wparams, countof(wparams));
			SHELLEXECUTEINFOW info = (SHELLEXECUTEINFOW){0};
			info.cbSize = sizeof(SHELLEXECUTEINFOW);
			info.fMask = SEE_MASK_NOCLOSEPROCESS;
			info.lpVerb = L"open";
			info.lpFile = L"msiexec.exe";
			info.lpParameters = wparams;
			info.nShow = SW_NORMAL;
			bool ok = ShellExecuteExW(&info);
			hope(ok, "Failed to run msiexec");
			WaitForSingleObject(info.hProcess, INFINITE);
		}
		folder_close(&sdk_folder);
	}
	char msvcv[MAX_PATH * 3];
	{
		string_format(msvcv, countof(msvcv), install_path, "\\VC\\Tools\\MSVC");
		folder_handle msvcv_folder = folder_open(msvcv);
		folder_next(&msvcv_folder, &msvcv);
		folder_close(&msvcv_folder);
	}
	char sdkv[MAX_PATH * 3];
	{
		string_format(sdkv, countof(sdkv), install_path, "\\Windows Kits\\10\\bin");
		folder_handle sdkv_folder = folder_open(sdkv);
		folder_next(&sdkv_folder, &sdkv);
		folder_close(&sdkv_folder);
	}
	echoln("Installing debug info...");
	char redist_path[MAX_PATH * 3];
	string_format(redist_path, countof(redist_path), install_path, "\\VC\\Redist");
	if (folder_exists(redist_path)) {
		char debug_version_path[MAX_PATH * 3];
		string_format(debug_version_path, countof(debug_version_path), redist_path, "\\MSVC");
		folder_handle debug_version_folder = folder_open(debug_version_path);
		char debug_version[MAX_PATH * 3];
		folder_next(&debug_version_folder, &debug_version);
		folder_close(&debug_version_folder);
		char debug_path[MAX_PATH * 3];
		string_format(debug_path, countof(debug_path), debug_version_path, "\\", debug_version, "\\debug_nonredist");
		folder_handle debug_folder = folder_open(debug_path);
		char debug_target[MAX_PATH * 3];
		while (folder_next(&debug_folder, &debug_target)) {
			char debug_target_path[MAX_PATH * 3];
			string_format(debug_target_path, countof(debug_target_path), debug_path, "\\", debug_target);
			folder_handle debug_target_folder = folder_open(debug_target_path);
			char debug_package[MAX_PATH * 3];
			while (folder_next(&debug_target_folder, &debug_package)) {
				char debug_package_path[MAX_PATH * 3];
				string_format(debug_package_path, countof(debug_package_path), debug_target_path, "\\", debug_package);
				folder_handle debug_package_folder = folder_open(debug_package_path);
				char file_name[MAX_PATH * 3];
				while (folder_next(&debug_package_folder, &file_name)) {
					char file_path[MAX_PATH * 3];
					string_format(file_path, countof(file_path), debug_package_path, "\\", file_name);
					char dst_file_path[MAX_PATH * 3];
					string_format(dst_file_path, countof(dst_file_path), install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\bin\\Host", host_arch, "\\", debug_target, "\\", file_name);
					file_create(dst_file_path);
					file_copy(dst_file_path, file_path);
				}
				folder_close(&debug_package_folder);
			}
			folder_close(&debug_target_folder);
		}
		folder_close(&debug_folder);
	}
	echoln("Installing DIA...");
	{
		char msdia[64] = {0};
		if (string_is(host_arch, "x86")) {
			string_copy(array_to_buffer(msdia), "msdia140.dll");
		} else if (string_is(host_arch, "x64")) {
			string_copy(array_to_buffer(msdia), "amd64\\msdia140.dll");
		} else if (string_is(host_arch, "arm")) {
			string_copy(array_to_buffer(msdia), "arm\\msdia140.dll");
		} else if (string_is(host_arch, "arm64")) {
			string_copy(array_to_buffer(msdia), "arm64\\msdia140.dll");
		}
		char msdia_path[MAX_PATH * 3];
		string_format(msdia_path, countof(msdia_path), install_path, "\\DIA%20SDK\\bin\\", msdia);
		char dst_path[MAX_PATH * 3];
		string_format(dst_path, countof(dst_path), install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\bin\\Host", host_arch);
		folder_handle dst_folder = folder_open(dst_path);
		char dst_target[MAX_PATH * 3];
		while (folder_next(&dst_folder, &dst_target)) {
			char dst_file_path[MAX_PATH * 3];
			string_format(dst_file_path, countof(dst_file_path), dst_path, "\\", dst_target, "\\msdia140.dll");
			file_create(dst_file_path);
			file_copy(dst_file_path, msdia_path);
		}
		folder_close(&dst_folder);
	}
	echoln("Creating a batch setup script...");
	{
		char bat_path[MAX_PATH * 3];
		string_format(bat_path, countof(bat_path), install_path, "\\devcmd.bat");
		file_create(bat_path);
		file_handle f = file_open(bat_path, file_mode_write);
		char buf[mem_page_size];
		file_write(&f, string_to_buffer("@echo off\n"));
		file_write(&f, string_to_buffer("set BUILD_TOOLS_ROOT=%~dp0\n"));
		file_write(&f, string_to_buffer("set WindowsSDKDir=%BUILD_TOOLS_ROOT%Windows Kits\\10\n"));
		string_format(buf, countof(buf), "set VCToolsInstallDir=%BUILD_TOOLS_ROOT%VC\\Tools\\MSVC\\", msvcv, "\n");
		file_write(&f, buf, string_count(buf));
		string_format(buf, countof(buf), "set WindowsSDKVersion=", sdkv, "\n");
		file_write(&f, buf, string_count(buf));
		string_format(buf, countof(buf), "set VSCMD_ARG_TGT_ARCH=", target_arch, "\n");
		file_write(&f, buf, string_count(buf));
		string_format(buf, countof(buf), "set VSCMD_ARG_HOST_ARCH=", host_arch, "\n");
		file_write(&f, buf, string_count(buf));
		file_write(&f, string_to_buffer("set INCLUDE=%VCToolsInstallDir%\\include;"));
		file_write(&f, string_to_buffer("%WindowsSDKDir%\\Include\\%WindowsSDKVersion%\\ucrt;"));
		file_write(&f, string_to_buffer("%WindowsSDKDir%\\Include\\%WindowsSDKVersion%\\shared;"));
		file_write(&f, string_to_buffer("%WindowsSDKDir%\\Include\\%WindowsSDKVersion%\\um;"));
		file_write(&f, string_to_buffer("%WindowsSDKDir%\\Include\\%WindowsSDKVersion%\\winrt;"));
		file_write(&f, string_to_buffer("%WindowsSDKDir%\\Include\\%WindowsSDKVersion%\\cppwinrt;\n"));
		file_write(&f, string_to_buffer("set LIB=%VCToolsInstallDir%\\lib\\%VSCMD_ARG_TGT_ARCH%;"));
		file_write(&f, string_to_buffer("%WindowsSDKDir%\\Lib\\%WindowsSDKVersion%\\ucrt\\%VSCMD_ARG_TGT_ARCH%;"));
		file_write(&f, string_to_buffer("%WindowsSDKDir%\\Lib\\%WindowsSDKVersion%\\um\\%VSCMD_ARG_TGT_ARCH%\n"));
		file_write(&f, string_to_buffer("set BUILD_TOOLS_BIN=%VCToolsInstallDir%\\bin\\Host%VSCMD_ARG_HOST_ARCH%\\%VSCMD_ARG_TGT_ARCH%;"));
		file_write(&f, string_to_buffer("%WindowsSDKDir%\\bin\\%WindowsSDKVersion%\\%VSCMD_ARG_TGT_ARCH%;"));
		file_write(&f, string_to_buffer("%WindowsSDKDir%\\bin\\%WindowsSDKVersion%\\%VSCMD_ARG_TGT_ARCH%\\ucrt\n"));
		file_write(&f, string_to_buffer("set PATH=%BUILD_TOOLS_BIN%;%PATH%\n"));
		file_close(&f);
	}
	echoln("Creating a PowerShell setup script...");
	{
		char bat_path[MAX_PATH * 3];
		string_format(bat_path, countof(bat_path), install_path, "\\devcmd.ps1");
		file_create(bat_path);
		file_handle f = file_open(bat_path, file_mode_write);
		char buf[mem_page_size];
		file_write(&f, string_to_buffer("#Requires -Version 5\n"));
		file_write(&f, string_to_buffer("param([string]$InstallPath = $PSScriptRoot)\n"));
		file_write(&f, string_to_buffer("$env:BUILD_TOOLS_ROOT = $InstallPath\n"));
		file_write(&f, string_to_buffer("$env:WindowsSDKDir = (Join-Path $InstallPath '\\Windows Kits\\10')\n"));
		file_write(&f, string_to_buffer("$VCToolsVersion = (Get-ChildItem -Directory (Join-Path $InstallPath '\\VC\\Tools\\MSVC' | Sort-Object -Descending LastWriteTime | Select-Object -First 1) -ErrorAction SilentlyContinue).Name\n"));
		file_write(&f, string_to_buffer("if (!$VCToolsVersion) { throw 'VCToolsVersion cannot be determined.' }\n"));
		file_write(&f, string_to_buffer("$env:VCToolsInstallDir = Join-Path (Join-Path $InstallPath '\\VC\\Tools\\MSVC') $VCToolsVersion\n"));
		file_write(&f, string_to_buffer("$env:WindowsSDKVersion = (Get-ChildItem -Directory (Join-Path $env:WindowsSDKDir 'bin') -ErrorAction SilentlyContinue | Sort-Object -Descending LastWriteTime | Select-Object -First 1).Name\n"));
		file_write(&f, string_to_buffer("if (!$env:WindowsSDKVersion ) { throw 'WindowsSDKVersion cannot be determined.' }\n"));
		string_format(buf, countof(buf), "$env:VSCMD_ARG_TGT_ARCH = '", target_arch, "'\n");
		file_write(&f, buf, string_count(buf));
		string_format(buf, countof(buf), "$env:VSCMD_ARG_HOST_ARCH = '", host_arch, "'\n");
		file_write(&f, buf, string_count(buf));
		file_write(&f, string_to_buffer("'Portable Build Tools environment started.'\n"));
		file_write(&f, string_to_buffer("'* BUILD_TOOLS_ROOT   : {0}' -f $env:BUILD_TOOLS_ROOT\n"));
		file_write(&f, string_to_buffer("'* WindowsSDKDir      : {0}' -f $env:WindowsSDKDir\n"));
		file_write(&f, string_to_buffer("'* WindowsSDKVersion  : {0}' -f $env:WindowsSDKVersion\n"));
		file_write(&f, string_to_buffer("'* VCToolsInstallDir  : {0}' -f $env:VCToolsInstallDir\n"));
		file_write(&f, string_to_buffer("'* VSCMD_ARG_TGT_ARCH : {0}' -f $env:VSCMD_ARG_TGT_ARCH\n"));
		file_write(&f, string_to_buffer("'* VSCMD_ARG_HOST_ARCH: {0}' -f $env:VSCMD_ARG_HOST_ARCH\n"));
		file_write(&f, string_to_buffer("$env:INCLUDE =\"$env:VCToolsInstallDir\\include;"));
		file_write(&f, string_to_buffer("$env:WindowsSDKDir\\Include\\$env:WindowsSDKVersion\\ucrt;"));
		file_write(&f, string_to_buffer("$env:WindowsSDKDir\\Include\\$env:WindowsSDKVersion\\shared;"));
		file_write(&f, string_to_buffer("$env:WindowsSDKDir\\Include\\$env:WindowsSDKVersion\\um;"));
		file_write(&f, string_to_buffer("$env:WindowsSDKDir\\Include\\$env:WindowsSDKVersion\\winrt;"));
		file_write(&f, string_to_buffer("$env:WindowsSDKDir\\Include\\$env:WindowsSDKVersion\\cppwinrt\"\n"));
		file_write(&f, string_to_buffer("$env:LIB = \"$env:VCToolsInstallDir\\lib\\$env:VSCMD_ARG_TGT_ARCH;"));
		file_write(&f, string_to_buffer("$env:WindowsSDKDir\\Lib\\$env:WindowsSDKVersion\\ucrt\\$env:VSCMD_ARG_TGT_ARCH;"));
		file_write(&f, string_to_buffer("$env:WindowsSDKDir\\Lib\\$env:WindowsSDKVersion\\um\\$env:VSCMD_ARG_TGT_ARCH\"\n"));
		file_write(&f, string_to_buffer("$env:BUILD_TOOLS_BIN = \"$env:VCToolsInstallDir\\bin\\Host$env:VSCMD_ARG_HOST_ARCH\\$env:VSCMD_ARG_TGT_ARCH;"));
		file_write(&f, string_to_buffer("$env:WindowsSDKDir\\bin\\$env:WindowsSDKVersion\\$env:VSCMD_ARG_TGT_ARCH;"));
		file_write(&f, string_to_buffer("$env:WindowsSDKDir\\bin\\$env:WindowsSDKVersion\\$env:VSCMD_ARG_TGT_ARCH\\ucrt\"\n"));
		file_write(&f, string_to_buffer("$env:PATH = \"$env:BUILD_TOOLS_BIN;$env:PATH\"\n"));
		file_close(&f);
	}
	echoln("Cleanup...");
	{
		if (folder_exists(redist_path)) {
			folder_delete(redist_path);
		}
		folder_handle install_folder = folder_open(install_path);
		char file_name[MAX_PATH * 3];
		while (folder_next(&install_folder, &file_name)) {
			char file_path[MAX_PATH * 3];
			string_format(file_path, countof(file_path), install_path, "\\", file_name);
			if (string_ends_with(file_name, ".msi")) {
				file_delete(file_path);
			}
			if (string_starts_with(file_name, "DIA%20SDK")) {
				folder_delete(file_path);
			}
			if (string_is(file_name, "Common7")) {
				folder_delete(file_path);
			}
		}
		folder_close(&install_folder);
		char folder_to_remove[MAX_PATH * 3];
		string_format(folder_to_remove, countof(folder_to_remove), install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\Auxiliary");
		folder_delete(folder_to_remove);
		string_format(folder_to_remove, countof(folder_to_remove), install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\bin\\Host", target_arch, "\\", host_arch, "\\onecore");
		if (folder_exists(folder_to_remove)) {
			folder_delete(folder_to_remove);
		}
		string_format(folder_to_remove, countof(folder_to_remove), install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\lib\\", target_arch, "\\store");
		if (folder_exists(folder_to_remove)) {
			folder_delete(folder_to_remove);
		}
		string_format(folder_to_remove, countof(folder_to_remove), install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\lib\\", target_arch, "\\uwp");
		if (folder_exists(folder_to_remove)) {
			folder_delete(folder_to_remove);
		}
		string_format(folder_to_remove, countof(folder_to_remove), install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\lib\\", target_arch, "\\enclave");
		if (folder_exists(folder_to_remove)) {
			folder_delete(folder_to_remove);
		}
		string_format(folder_to_remove, countof(folder_to_remove), install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\lib\\", target_arch, "\\onecore");
		if (folder_exists(folder_to_remove)) {
			folder_delete(folder_to_remove);
		}
		string_format(folder_to_remove, countof(folder_to_remove), install_path, "\\Windows Kits\\10\\Catalogs");
		folder_delete(folder_to_remove);
		string_format(folder_to_remove, countof(folder_to_remove), install_path, "\\Windows Kits\\10\\DesignTime");
		folder_delete(folder_to_remove);
		string_format(folder_to_remove, countof(folder_to_remove), install_path, "\\Windows Kits\\10\\bin\\", sdkv, "\\chpe");
		folder_delete(folder_to_remove);
		string_format(folder_to_remove, countof(folder_to_remove), install_path, "\\Windows Kits\\10\\Lib\\", sdkv, "\\ucrt_enclave");
		folder_delete(folder_to_remove);
		{
			string_format(folder_to_remove, countof(folder_to_remove), install_path, "\\Windows Kits\\10\\Lib\\", sdkv, "\\ucrt");
			folder_handle f = folder_open(folder_to_remove);
			char file_name[MAX_PATH * 3];
			while (folder_next(&f, &file_name)) {
				if (string_is(file_name, target_arch)) {
					continue;
				}
				char file_path[MAX_PATH * 3];
				string_format(file_path, countof(file_path), folder_to_remove, "\\", file_name);
				folder_delete(file_path);
			}
			folder_close(&f);
		}
		{
			string_format(folder_to_remove, countof(folder_to_remove), install_path, "\\Windows Kits\\10\\Lib\\", sdkv, "\\um");
			folder_handle f = folder_open(folder_to_remove);
			char file_name[MAX_PATH * 3];
			while (folder_next(&f, &file_name)) {
				if (string_is(file_name, target_arch)) {
					continue;
				}
				char file_path[MAX_PATH * 3];
				string_format(file_path, countof(file_path), folder_to_remove, "\\", file_name);
				folder_delete(file_path);
			}
			folder_close(&f);
		}
		{
			string_format(folder_to_remove, countof(folder_to_remove), install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\bin");
			folder_handle f = folder_open(folder_to_remove);
			char file_name[MAX_PATH * 3];
			while (folder_next(&f, &file_name)) {
				if (string_ends_with(file_name, host_arch)) {
					continue;
				}
				char file_path[MAX_PATH * 3];
				string_format(file_path, countof(file_path), folder_to_remove, "\\", file_name);
				folder_delete(file_path);
			}
			folder_close(&f);
		}
		{
			string_format(folder_to_remove, countof(folder_to_remove), install_path, "\\Windows Kits\\10\\bin\\", sdkv);
			folder_handle f = folder_open(folder_to_remove);
			char file_name[MAX_PATH * 3];
			while (folder_next(&f, &file_name)) {
				if (string_is(file_name, host_arch)) {
					continue;
				}
				char file_path[MAX_PATH * 3];
				string_format(file_path, countof(file_path), folder_to_remove, "\\", file_name);
				folder_delete(file_path);
			}
			folder_close(&f);
		}
		char telemetry_path[MAX_PATH * 3];
		string_format(telemetry_path, countof(telemetry_path), install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\bin\\Host", host_arch, "\\", target_arch, "\\vctip.exe");
		file_delete(telemetry_path);
		folder_delete(temp_path);
	}
	if (env_mode != 0) {
		echoln("Setting up the environment...");
		HKEY location = (env_mode == 2) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
		LPCWSTR subkey = (env_mode == 2) ? L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment" : L"Environment";
		char buf[MAX_ENV_LEN * 3];
		string_format(buf, countof(buf), install_path, "\\VC\\Tools\\MSVC\\", msvcv);
		env_set(location, subkey, L"VCToolsInstallDir", buf);
		string_format(buf, countof(buf), install_path, "\\Windows Kits\\10");
		env_set(location, subkey, L"WindowsSDKDir", buf);
		env_set(location, subkey, L"WindowsSDKVersion", sdkv);
		env_set(location, subkey, L"VSCMD_ARG_HOST_ARCH", host_arch);
		env_set(location, subkey, L"VSCMD_ARG_TGT_ARCH", target_arch);
		string_format(buf, countof(buf),
			install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\include;",
			install_path, "\\Windows Kits\\10\\Include\\", sdkv, "\\ucrt;",
			install_path, "\\Windows Kits\\10\\Include\\", sdkv, "\\shared;",
			install_path, "\\Windows Kits\\10\\Include\\", sdkv, "\\um;",
			install_path, "\\Windows Kits\\10\\Include\\", sdkv, "\\winrt;",
			install_path, "\\Windows Kits\\10\\Include\\", sdkv, "\\cppwinrt"	
		);
		env_set(location, subkey, L"INCLUDE", buf);
		string_format(buf, countof(buf),
			install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\lib\\", target_arch, ";",
			install_path, "\\Windows Kits\\10\\Lib\\", sdkv, "\\ucrt\\", target_arch, ";",
			install_path, "\\Windows Kits\\10\\Lib\\", sdkv, "\\um\\", target_arch
		);
		env_set(location, subkey, L"LIB", buf);
		string_format(buf, countof(buf),
			install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\bin\\Host", host_arch, "\\", target_arch, ";",
			install_path, "\\Windows Kits\\10\\bin\\", sdkv, "\\", target_arch, ";",
			install_path, "\\Windows Kits\\10\\bin\\", sdkv, "\\", target_arch, "\\ucrt\n",
		);
		env_set(location, subkey, L"BUILD_TOOLS_BIN", buf);

		WCHAR wpath[MAX_ENV_LEN];
		char path[countof(wpath) * 3];
		DWORD wpath_size = sizeof(wpath);
		LSTATUS err = RegGetValueW(location, subkey, L"Path", RRF_RT_REG_SZ | RRF_NOEXPAND, null, wpath, &wpath_size);
		err = (err == ERROR_UNSUPPORTED_TYPE) ? 0 : err;
		i64 wpath_count = (err != 0) ? 0 : (wpath_size / sizeof(WCHAR));
		utf16_to_utf8(wpath, wpath_count, path, countof(path));
//[c]		Split path into strings by ;
		i64 path_count = string_count(path);
		for (i64 i = 0; i < path_count; i++) {
			path[i] = (path[i] == ';') ? 0 : path[i];
		}
//[c]		Check if %BUILD_TOOLS_BIN% is among them
		bool has_build_tools_bin = false;
		for (i64 i = 0; i < path_count; i++) {
			char* entry = &path[i];
			has_build_tools_bin = string_is(entry, "%BUILD_TOOLS_BIN%") ? true : has_build_tools_bin;
			i += string_count(entry);
		}
//[c]		Join the strings back
		for (i64 i = 0; i < path_count; i++) {
			path[i] = (path[i] == 0) ? ';' : path[i];
		}
//[c]		Add %BUILD_TOOLS_BIN% if not present
		string_append(path, countof(path), has_build_tools_bin ? "" : ";%BUILD_TOOLS_BIN%");
		string_trim_end(path, ";");
		string_trim_start(path, ";");
		{
			WCHAR wpath[MAX_ENV_LEN];
			int n = utf8_to_utf16(path, -1, wpath, countof(wpath));
			RegSetKeyValueW(location, subkey, L"Path", REG_EXPAND_SZ, wpath, n * sizeof(WCHAR));
		}
	}
	echoln("Done!");
}

void start(void)
{
	bool is_subprocess = false;
	bool list_versions = false;
	for (nat i = 0; i < args_count; i++)
	{
		if (i == 0)
		 continue;
		char* arg = console_get_arg(i);
		if (string_is(arg, "!")) {
			is_subprocess = true;
		} else if (string_is(arg, "gui")) {
			is_cli = false;
		} else if (string_is(arg, "list")) {
			list_versions = true;
		} else if (string_is(arg, "accept_license")) {
			license_accepted = true;
		} else if (string_is(arg, "preview")) {
			is_preview = true;
		} else if (string_starts_with(arg, "msvc=")) {
			string_trim_start(arg, "msvc=");
			string_copy(msvc_version, countof(msvc_version), arg);
		} else if (string_starts_with(arg, "sdk=")) {
			string_trim_start(arg, "sdk=");
			string_copy(sdk_version, countof(sdk_version), arg);
		} else if (string_starts_with(arg, "target=")) {
			string_trim_start(arg, "target=");
			string_copy(target_arch, countof(target_arch), arg);
		} else if (string_starts_with(arg, "host=")) {
			string_trim_start(arg, "host=");
			string_copy(host_arch, countof(host_arch), arg);
		} else if (string_starts_with(arg, "env=")) {
			string_trim_start(arg, "env=");
			if (string_is(arg, "none")) {
				env_mode = 0;
			} else if (string_is(arg, "user")) {
				env_mode = 1;
			} else if (string_is(arg, "global")) {
				env_mode = 2;
			} else {
				hopeless("Invalid environment string: ", arg);
			}
		} else if (string_starts_with(arg, "path=")) {
			string_trim_start(arg, "path=");
			string_copy(install_path, countof(install_path), arg);
			string_trim_end(install_path, "\"");
			string_trim_start(install_path, "\"");
		} else {
			echo(
			"usage: PortableBuildTools.exe [gui] [list] [accept_license] [preview] [msvc=MSVC version] [sdk=SDK version] [target=x64/x86/arm/arm64] [host=x64/x86/arm64] [env=none/user/global] [path=\"C:\\BuildTools\"]\n",
				"\n",
				"*gui: run PortableBuildTools in GUI mode\n",
				"*list: show all available Build Tools versions\n",
				"*accept_license: auto-accept the license [default: ask]\n",
				"*preview: install from the Preview channel, instead of the Release channel [default: Release channel]\n",
				"*msvc: which MSVC toolchain version to install [default: whatever is latest]\n",
				"*sdk: which Windows SDK version to install [default: whatever is latest]\n",
				"*target: target architecture [default: x64]\n",
				"*host: host architecture [default: x64]\n",
				"*env: if supplied, then the installed path will be added to PATH environment variable, for the current user or for all users [default: none]\n"
				"*path: installation path [default: C:\\BuildTools]\n"
			);
			cleanup();
			exit(1);
		}
	}
	CoInitializeEx(null, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	internet_session = InternetOpenW(L"PortableBuildTools/2.9", INTERNET_OPEN_TYPE_PRECONFIG, null, null, 0);
	hope(internet_session != null, "Failed to open internet session");
	{
		WCHAR wtemp_path[MAX_PATH];
		GetTempPathW(countof(wtemp_path), wtemp_path);
		utf16_to_utf8(wtemp_path, -1, temp_path, countof(temp_path));
		string_append(temp_path, countof(temp_path), "BuildTools");
	}
	if (is_subprocess) {
		folder_create(install_path);
		install();
		WCHAR* msg = L"PortableBuildTools was installed successfully.";
		msg = (env_mode == 1) ? L"Please log out and back in to finish installation." : msg;
		msg = (env_mode == 2) ? L"Please reboot the machine to finish installation." : msg;
		MessageBoxW(GetConsoleWindow(), msg, L"Success!", MB_TOPMOST | MB_ICONINFORMATION);
		cleanup();
		exit(0);
	}
//[c]	Download manifest files
	folder_create(temp_path);
	{
		echoln("Downloading manifest files...");
		char manifest_path[MAX_PATH * 3];
		string_format(manifest_path, countof(manifest_path), temp_path, "\\Manifest.Release.json");
		hope(download_file(release_vsmanifest_url, manifest_path, "Manifest.Release.json"), "Failed to download release manifest");
		string_format(manifest_path, countof(manifest_path), temp_path, "\\Manifest.Preview.json");
		hope(download_file(preview_vsmanifest_url, manifest_path, "Manifest.Preview.json"), "Failed to download preview manifest");
		echoln("Parsing manifest files...");
		string_format(manifest_path, countof(manifest_path), temp_path, "\\Manifest.Release.json");
		parse_vsmanifest(manifest_path, false);
		string_format(manifest_path, countof(manifest_path), temp_path, "\\Manifest.Preview.json");
		parse_vsmanifest(manifest_path, true);
		echoln("Downloading Build Tools manifest files...");
		string_format(manifest_path, countof(manifest_path), temp_path, "\\BuildToolsManifest.Release.json");	
		hope(download_file(release_manifest_url, manifest_path, "BuildToolsManifest.Release.json"), "Failed to download Build Tools release manifest");
		string_format(manifest_path, countof(manifest_path), temp_path, "\\BuildToolsManifest.Preview.json");
		hope(download_file(preview_manifest_url, manifest_path, "BuildToolsManifest.Preview.json"), "Failed to download Build Tools preview manifest");
		echoln("Parsing Build Tools manifest files...");
		string_format(manifest_path, countof(manifest_path), temp_path, "\\BuildToolsManifest.Release.json");
		parse_manifest(manifest_path, false);
		string_format(manifest_path, countof(manifest_path), temp_path, "\\BuildToolsManifest.Preview.json");
		parse_manifest(manifest_path, true);
	}
	echoln("");
	if (list_versions) {
		echoln("MSVC Versions (Release): ");
		for (i64 i = release_msvc_versions_count - 1; i >= 0; i--) {
			echoln(release_msvc_versions[i]);
		}
		echoln("SDK Versions (Release): ");
		for (i64 i = release_sdk_versions_count - 1; i >= 0; i--) {
			echoln(release_sdk_versions[i]);
		}
		echoln("MSVC Versions (Preview): ");
		for (i64 i = preview_msvc_versions_count - 1; i >= 0; i--) {
			echoln(preview_msvc_versions[i]);
		}
		echoln("SDK Versions (Preview): ");
		for (i64 i = preview_sdk_versions_count - 1; i >= 0; i--) {
			echoln(preview_sdk_versions[i]);
		}
		cleanup();
		exit(0);
	}
	if (is_cli) {
//[c]		Sanity checks
		{
			bool found = false;
			for (i64 i = 0; i < countof(targets); i++) {
				found = string_is(target_arch, targets[i]) ? true : found;
			}
			hope(found, "Invalid target architecture: ", target_arch);
		}
		{
			bool found = false;
			for (i64 i = 0; i < countof(hosts); i++) {
				found = string_is(host_arch, hosts[i]) ? true : found;
			}
			hope(found, "Invalid host architecture: ", host_arch);
		}
		{
			char (*msvc_versions)[16] = is_preview ? preview_msvc_versions : release_msvc_versions;
			i64 msvc_versions_count = is_preview ? preview_msvc_versions_count : release_msvc_versions_count;
			if (msvc_version[0] == 0) {
				string_copy(msvc_version, countof(msvc_version), msvc_versions[msvc_versions_count - 1]);
			}
			bool found = false;
			for (i64 i = 0; i < msvc_versions_count; i++) {
				found = string_is(msvc_version, msvc_versions[i]) ? true : found;
			}
			hope(found, "Invalid MSVC version: ", msvc_version);
		}
		{
			char (*sdk_versions)[8] = is_preview ? preview_sdk_versions : release_sdk_versions;
			i64 sdk_versions_count = is_preview ? preview_sdk_versions_count : release_sdk_versions_count;
			if (sdk_version[0] == 0) {
				string_copy(sdk_version, countof(sdk_version), sdk_versions[sdk_versions_count - 1]);
			}
			bool found = false;
			for (i64 i = 0; i < sdk_versions_count; i++) {
				found = string_is(sdk_version, sdk_versions[i]) ? true : found;
			}
			hope(found, "Invalid SDK version: ", sdk_version);
		}
		echoln("Preview channel: ", is_preview ? "true" : "false");
		echoln("MSVC version: ", msvc_version);
		echoln("SDK version: ", sdk_version);
		echoln("Target arch: ", target_arch);
		echoln("Host arch: ", host_arch);
		echoln("Install path: ", install_path);
		if (folder_exists(install_path)) {
			if (!is_folder_empty(install_path)) {
				hopeless("Destination folder is not empty");
			}
		} else {
			WCHAR wpath[MAX_PATH];
			utf8_to_utf16(install_path, -1, wpath, MAX_PATH);
			if (!CreateDirectoryW(wpath, null)) {
				const char* error = (GetLastError() == ERROR_ACCESS_DENIED) ? "No access to the destination folder, try running this program as administrator" : "Destination folder cannot be made";
				hopeless(error);
			}
		}
		if (license_accepted) {
			install_start = true;
			echoln("");
		} else {
			echoln("Do you accept the license agreement? [Y/n] ", is_preview ? preview_license_url : release_license_url);
			char answer[4];
			console_read(answer, countof(answer));
			string_lower(answer);
			install_start = string_is(answer, "") | string_is(answer, "y") | string_is(answer, "yes");
		}
		hope(install_start, "License was not accepted, cannot proceed further.");
		install();
		const char* msg = "PortableBuildTools was installed successfully.";
		msg = (env_mode == 1) ? "Please log out and back in to finish installation." : msg;
		msg = (env_mode == 2) ? "Please reboot the machine to finish installation." : msg;
		echoln(msg);
	} else {
		{
			FreeConsole();
			INITCOMMONCONTROLSEX info = (INITCOMMONCONTROLSEX){0};
			info.dwSize = sizeof(INITCOMMONCONTROLSEX);
			info.dwICC = ICC_STANDARD_CLASSES | ICC_LINK_CLASS;
			InitCommonControlsEx(&info);
			DialogBoxW(GetModuleHandleW(null), cast(LPCWSTR, ID_DIALOG_WINDOW), null, cast(DLGPROC, window_proc));
			if (!install_start) {
				cleanup();
				exit(1);
			}
		}
		{
			char* env_mode_str = "none";
			env_mode_str = (env_mode == 1) ? "user" : env_mode_str;
			env_mode_str = (env_mode == 2) ? "global" : env_mode_str;
			char params[MAX_CMDLINE_LEN * 3];
			string_format(params, countof(params), "! ", is_preview ? "preview" : "", " msvc=", msvc_version, " sdk=", sdk_version, " target=", target_arch, " host=", host_arch, " env=", env_mode_str, " path=\"", install_path, "\"");
			WCHAR wprogram[MAX_PATH];
			char* program = console_get_arg(0);
			utf8_to_utf16(program, -1, wprogram, countof(wprogram));
			WCHAR wparams[MAX_CMDLINE_LEN];
			utf8_to_utf16(params, -1, wparams, countof(wparams));
			SHELLEXECUTEINFOW info = (SHELLEXECUTEINFOW){0};
			info.cbSize = sizeof(SHELLEXECUTEINFOW);
			info.fMask = SEE_MASK_NOCLOSEPROCESS;
			info.lpVerb = is_admin ? L"runas" : L"open";
			info.lpFile = wprogram;
			info.lpParameters = wparams;
			info.nShow = SW_NORMAL;
			bool ok = ShellExecuteExW(&info);
			hope(ok, "Failed to run the installer");
		}
	}
	cleanup();
}
