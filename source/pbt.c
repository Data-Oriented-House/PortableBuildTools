#define appid "PortableBuildTools"
#include "base.h"
#define MINIZ_NO_STDIO
#define MINIZ_NO_TIME
#define MINIZ_NO_DEFLATE_APIS
#define MINIZ_NO_ZLIB_APIS
#define MINIZ_NO_ZLIB_COMPATIBLE_NAME
#define MINIZ_NO_MALLOC
#define NDEBUG
#include "miniz/miniz.c"
#include "dialog.h"
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

#pragma region strings

bool string_has_char(const char* s, char x)
{
 bool match = false;
 while (*s != 0)
  match = (*s == x) ? true : match;
 return (match);
}

void string_lower(char* s)
{
 while (*s != 0)
 {
  char lower = *s + ('a' - 'A');
  *s = ((*s >= 'A') & (*s <= 'Z')) ? lower : *s;
  s++;
 }
}

// check if the string includes any of the bytes in the buffer
bool string_has_any(const char* s, slice(const char, buf))
{
 i64 s_count = strlen(s);
 bool match = false;
 for (i64 i = 0; i < s_count; i++)
 {
  for (i64 j = 0; j < buf_count; j++)
  {
   match = (s[i] == buf[j]) ? true : match;
  }
 }
 return (match);
}

// check if the string includes only the bytes in the buffer, and no other bytes
bool string_has_only(const char* s, slice(const char, buf))
{
 i64 s_count = strlen(s);
 bool match = true;
 for (i64 i = 0; i < s_count; i++)
 {
  bool match_byte = false;
  for (i64 j = 0; j < buf_count; j++)
  {
   match_byte = (s[i] == buf[j]) ? true : match_byte;
  }
  match = match_byte ? match : false;
 }
 return (match);
}

// TODO: minus sign, check if number is outside the representable range, check if non-digits, etcetc. This is barely usable right now.
i64 string_to_i64(const char* s)
{
 i64 base = 1;
 i64 out = 0;
 i64 s_count = strlen(s);
 for (i64 i = s_count - 1; i >= 0; i -= 1)
 {
  i64 digit = s[i] - '0';
  out += (base * digit);
  base *= 10;
 }
 return (out);
}

#pragma endregion

#pragma region json parser

typedef uchar (*json_peek_proc)(void* context, bool advance);
typedef struct {
 json_peek_proc peek;
 void* context;
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
 for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true))
 {
  char symbol = *cast(char*, &c8);
  bool skip = ((symbol == ' ') | (symbol == '\t') | (symbol == '\n') | (symbol == '\r') | (symbol == ','));
  out = (((symbol >= 'a') & (symbol <= 'z')) | ((symbol >= 'A') & (symbol <= 'Z'))) ? json_word : out;
  out = (((symbol >= '0') & (symbol <= '9')) | (symbol == '-') | (symbol == '.')) ? json_number : out;
  out = (symbol == '"') ? json_string : out;
  out = (symbol == '{') ? json_object : out;
  out = (symbol == '[') ? json_array : out;
  if (skip)
   continue;
  break;
 }
 return (out);
}

void json_word_skip(json_parser* p)
{
 for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true))
 {
  char symbol = *cast(char*, &c8);
  if (((symbol >= 'a') & (symbol <= 'z')) | ((symbol >= 'A') & (symbol <= 'Z')))
   continue;
  break;
 }
}

i64 json_number_extract_i64(json_parser* p)
{
 char n[66] = {0};
 i64 pos = 0;
 for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true))
 {
  char symbol = *cast(char*, &c8);
  if (((symbol >= '0') & (symbol <= '9')) | (symbol == '-') | (symbol == '.'))
  {
   if (pos < cast(i64, countof(n)))
   {
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
 for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true))
 {
  char symbol = *cast(char*, &c8);
  if (((symbol >= '0') & (symbol <= '9')) | (symbol == '-') | (symbol == '.'))
   continue;
  break;
 }
}

#define json_string_extract(p, b) \
 _json_string_extract(p, b, countof(b))
void _json_string_extract(json_parser* p, slice(char, buf))
{
 buf_count--;
 for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true))
 {
  char symbol = *cast(char*, &c8);
  if (symbol == '"')
  {
   p->peek(p->context, true);
   break;
  }
 }
 i64 pos = 0;
 bool buf_filled = false;
 bool escaping = false;
 for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true))
 {
  char symbol = *cast(char*, &c8);
  if ((symbol == '\\') & !escaping)
  {
   escaping = true;
   continue;
  }
  if ((symbol == '"') & !escaping)
  {
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
 i64 str_count = strlen(str);
 for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true))
 {
  char symbol = *cast(char*, &c8);
  if (symbol == '"')
  {
   p->peek(p->context, true);
   break;
  }
 }
 i64 pos = 0;
 bool match = true;
 bool escaping = false;
 for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true))
 {
  char symbol = *cast(char*, &c8);
  if (symbol == '\\')
  {
   if (!escaping)
   {
    escaping = true;
    continue;
   }
  }
  if (symbol == '"')
  {
   if (!escaping)
   {
    p->peek(p->context, true);
    break;
   }
  }
  escaping = false;
  i64 space_left = str_count - pos;
  i64 char_size = uchar_size(c8);
  i64 compare_size = clamp_top(char_size, space_left);
  char* bytes = cast(char*, &c8);
  for (i64 i = 0; i < compare_size; i++)
  {
   match = (str[pos + i] != bytes[i]) ? false : match;
  }
  pos += char_size;
 }
 return ((pos == str_count) & (match));
}

void json_string_skip(json_parser* p)
{
 for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true))
 {
  char symbol = *cast(char*, &c8);
  if (symbol == '"')
  {
   p->peek(p->context, true);
   break;
  }
 }
 bool escaping = false;
 for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true))
 {
  char symbol = *cast(char*, &c8);
  if (symbol == '\\')
  {
   if (!escaping)
   {
    escaping = true;
    continue;
   }
  }
  if (symbol == '"')
  {
   if (!escaping)
   {
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
 for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true))
 {
  char symbol = *cast(char*, &c8);
  if ((symbol == '[') | (symbol == ' ') | (symbol == '\t') | (symbol == '\n') | (symbol == '\r') | (symbol == ','))
  {
   continue;
  }
  if (symbol == ']')
  {
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
 while (json_array_next(p))
  json_value_skip(p);
}

bool json_object_next(json_parser* p)
{
 bool match = false;
 for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true))
 {
  char symbol = *cast(char*, &c8);
  if ((symbol == '{') | (symbol == ' ') | (symbol == '\t') | (symbol == '\n') | (symbol == '\r') | (symbol == ','))
   continue;
  if (symbol == '}')
  {
   p->peek(p->context, true);
   break;
  }
  if (symbol == '"')
   match = true;
  break;
 }
 return (match);
}

#define json_object_key_extract(p, b) \
 _json_object_key_extract(p, b, countof(b))
void _json_object_key_extract(json_parser* p, slice(char, buf))
{
 _json_string_extract(p, buf, buf_count);
 for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true))
 {
  char symbol = *cast(char*, &c8);
  if (symbol == ':')
  {
   p->peek(p->context, true);
   break;
  }
 }
}

void json_object_key_skip(json_parser* p)
{
 json_string_skip(p);
 for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true))
 {
  char symbol = *cast(char*, &c8);
  if (symbol == ':')
  {
   p->peek(p->context, true);
   break;
  }
 }
}

bool json_object_key_find(json_parser* p, const char* key)
{
 bool match = false;
 while (json_object_next(p) & !match)
 {
  match = json_string_is(p, key);
  for (uchar c8 = p->peek(p->context, false); c8 != 0; c8 = p->peek(p->context, true))
  {
   char symbol = *cast(char*, &c8);
   if (symbol == ':')
   {
    p->peek(p->context, true);
    break;
   }
  }
  if (!match)
   json_value_skip(p);
 }
 return (match);
}

void json_object_skip(json_parser* p)
{
 while (json_object_next(p))
 {
  json_object_key_skip(p);
  json_value_skip(p);
 }
}

json_value json_value_skip(json_parser* p)
{
 json_value v = json_value_next(p);
 if (v == json_word)
  json_word_skip(p);
 if (v == json_number)
  json_number_skip(p);
 if (v == json_string)
  json_string_skip(p);
 if (v == json_object)
  json_object_skip(p);
 if (v == json_array)
  json_array_skip(p);
 return (v);
}

#pragma endregion

#pragma region json file parser

typedef struct {
 char cache[mem_page_size];
 uchar c8;
 i64 cache_size;
 i64 cache_pos;
 struct file f;
} json_context;

uchar json_file_peek(void* context, bool advance)
{
 json_context* jc = context;
 // advance if beginning of the file, or if asked to
 if ((!advance) & (jc->f.pos != 0))
  return (jc->c8);
 // if beginning of the file, reset all values
 jc->c8 = (jc->f.pos == 0) ? 0 : jc->c8;
 jc->cache_size = (jc->f.pos == 0) ? 0 : jc->cache_size;
 jc->cache_pos = (jc->f.pos == 0) ? 0 : jc->cache_pos;
 // chunk end
 if (jc->cache_pos >= jc->cache_size)
 {
  jc->cache_size = file_read(&jc->f, string_slice(jc->cache));
  jc->cache[countof(jc->cache)] = 0;
  jc->cache_pos = 0;
 }
 // file end
 if (jc->cache_size <= 0)
 {
  jc->cache_pos = 0;
  jc->c8 = 0;
  return (jc->c8);
 }
 const char* str = jc->cache + jc->cache_pos;
 i64 str_size = jc->cache_size - jc->cache_pos;
 jc->c8 = string_decode_uchar(str);
 i64 char_size = uchar_size(jc->c8);
 // valid char, or invalid char in the middle of a chunk
 if ((char_size > 0) | (str_size >= 4) | (jc->f.pos == jc->f.size))
 {
  jc->cache_pos += clamp_bot(char_size, 1);
  return (jc->c8);
 }
 // either invalid char or chunk end, start a new chunk just to be sure
 i64 seek_pos = jc->f.pos - (jc->cache_size - jc->cache_pos);
 file_seek(&jc->f, seek_pos);
 jc->cache_size = 0;
 jc->cache_pos = 0;
 return json_file_peek(context, advance);
}

void json_file_context_restore(json_context* jc, json_context context)
{
 *jc = context;
 file_seek(&jc->f, jc->f.pos);
}

#pragma endregion

#pragma region miniz helpers

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
 if (!address)
 {
  return HeapAlloc(GetProcessHeap(), 0, items * size);
 }
 else if ((items == 0) | (size == 0))
 {
  HeapFree(GetProcessHeap(), 0, address);
  return (null);
 }
 else
 {
  return HeapReAlloc(GetProcessHeap(), 0, address, items * size);
 }
}

size_t miniz_file_read(void* context, mz_uint64 file_ofs, void* buf, size_t buf_count)
{
 struct file* file = context;
 file_seek(file, file_ofs);
 return file_read(file, buf, buf_count);
}

size_t mz_file_write(void* context, mz_uint64 file_ofs, const void* buf, size_t buf_count)
{
 struct file* file = context;
 file_write(file, buf, buf_count);
 return (buf_count);
}

#pragma endregion

enum download_status
{
 download_status_unknown,
 download_status_downloading,
 download_status_finished,
 download_status_error_open_url,
 download_status_error_read_file,
};

struct download_service
{
 HWND handle;
 nat content_size;
 nat downloaded_size;
 enum download_status status;
};
struct download_service download_service;

struct download_info
{
 const char* file_url;
 const char* file_path;
};

const char* release_vsmanifest_url = "https://aka.ms/vs/17/release/channel";
const char* preview_mode_vsmanifest_url = "https://aka.ms/vs/17/pre/channel";

char* targets[] = {"x64", "x86", "arm", "arm64",};
char* hosts[] = {"x64", "x86", "arm64"};

bool install_start;
bool subprocess_mode;
bool gui_mode;
bool preview_mode;
n64 env_mode; // 0: none; 1: user; 2: global;

char msvc_version[32];

char sdk_version[16];
char target_arch[16] = "x64";
char host_arch[16] = "x64";
char install_path[(MAX_PATH * 3) + 8] = "C:\\BuildTools"; // +8 extra padding just in case
bool license_accepted;
bool is_admin;
char temp_path[MAX_PATH * 3];

char release_manifest_url[L_MAX_URL_LENGTH * 3];
char release_license_url[L_MAX_URL_LENGTH * 3];
char preview_mode_manifest_url[L_MAX_URL_LENGTH * 3];
char preview_mode_license_url[L_MAX_URL_LENGTH * 3];

// 14/32
char release_msvc_versions[32][16];
i64 release_msvc_versions_count;
// 14/32
char preview_mode_msvc_versions[32][16];
i64 preview_mode_msvc_versions_count;

// 6/16
char release_sdk_versions[16][8];
i64 release_sdk_versions_count;
// 6/16
char preview_mode_sdk_versions[16][8];
i64 preview_mode_sdk_versions_count;

void env_set(HKEY location, LPCWSTR subkey, LPCWSTR key, const char* env)
{
 WCHAR wenv[MAX_ENV_LEN];
 int n = utf8_to_utf16(env, -1, wenv, countof(wenv));
 RegSetKeyValue(location, subkey, key, REG_SZ, wenv, n * sizeof(WCHAR));
}

bool is_folder_empty(const char* path)
{
 struct folder folder = folder_open(path);
 bool has_files = folder_next(&folder);
 folder_close(&folder);
 return (!has_files);
}

LRESULT CALLBACK download_service_proc(HWND service, UINT message, WPARAM wparam, LPARAM lparam)
{
 if (message == WM_CREATE)
 {
  HINTERNET internet_session = InternetOpen(L"PortableBuildTools/2.10", INTERNET_OPEN_TYPE_PRECONFIG, null, null, 0);
  SetWindowLongPtr(service, GWLP_USERDATA, cast(LONG_PTR, internet_session));
 }

 if (message == WM_CLOSE)
 {
  HINTERNET internet_session = autocast(GetWindowLongPtr(service, GWLP_USERDATA));
  if (internet_session != null)
   InternetCloseHandle(internet_session);
 }

 if (message == WM_APP)
 {
  HINTERNET internet_session = autocast(GetWindowLongPtr(service, GWLP_USERDATA));
  struct download_info* info = autocast(wparam);
  WCHAR wurl[L_MAX_URL_LENGTH];
  utf8_to_utf16(info->file_url, -1, wurl, countof(wurl));
  DWORD flags = INTERNET_FLAG_DONT_CACHE;
  if (string_starts_with(info->file_url, "https://"))
   flags |= INTERNET_FLAG_SECURE;
  HINTERNET connection = InternetOpenUrl(internet_session, wurl, null, 0, flags, 0);
  if (!connection)
  {
   download_service.status = download_status_error_open_url;
   return (1);
  }
  DWORD content_size = 0;
  if (!HttpQueryInfo(connection, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &content_size, &(DWORD){sizeof(DWORD)}, null))
   content_size = 0;
  download_service.content_size = content_size;
  download_service.status = download_status_downloading;
  file_delete(info->file_path);
  file_create(info->file_path);
  struct file file = file_open(info->file_path, true);
  DWORD n = 0;
  char chunk[16 * mem_page_size];
  while (true)
  {
   if (!InternetReadFile(connection, chunk, countof(chunk), &n))
   {
    file_close(&file);
    download_service.status = download_status_error_read_file;
    return (1);
   }
   if (n == 0)
    break;
   file_write(&file, chunk, n);
   download_service.downloaded_size += n;
  }
  file_close(&file);
  InternetCloseHandle(connection);
  download_service.status = download_status_finished;
  return (0);
 }

 if (message == WM_DESTROY)
 {
  PostQuitMessage(0);
  return (0);
 }

 return DefWindowProc(service, message, wparam, lparam);
}

void download_service_thread(HANDLE signal)
{
 download_service.handle = CreateWindow(TEXT(appid) L":download_service", null, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_MESSAGE, null, GetModuleHandle(null), null);
 SetEvent(signal);
 MSG msg = {0};
 while (GetMessage(&msg, null, 0, 0))
 {
  TranslateMessage(&msg);
  DispatchMessage(&msg);
 }
}

void download_service_start(void)
{
 HANDLE signal = CreateEvent(null, false, false, null);
 thread_start(download_service_thread, signal);
 WaitForSingleObject(signal, INFINITE);
 CloseHandle(signal);
}

void download_service_stop(void)
{
 SendMessage(download_service.handle, WM_CLOSE, 0, 0);
}

void download_file(const char* file_url, const char* file_path, const char* display_name)
{
 struct download_info info = {0};
 info.file_url = file_url;
 info.file_path = file_path;
 while (true)
 {
  nat old_p = 0;
  nat old_kb = 0;
  download_service.content_size = 0;
  download_service.downloaded_size = 0;
  download_service.status = download_status_unknown;
  echo("[?] ", display_name);
  PostMessage(download_service.handle, WM_APP, cast(WPARAM, &info), 0);
  while (true)
  {
   enum download_status status = download_service.status;
   if (status >= download_status_downloading)
   {
    if (download_service.content_size > 0)
    {
     nat p = clamp_top((download_service.downloaded_size * 100) / download_service.content_size, 100);
     if (p != old_p)
      echo("\r[", itos(p), "%] ", display_name);
     old_p = p;
    }
    else
    {
     nat kb = (download_service.downloaded_size / mem_kilobyte);
     if (kb != old_kb)
      echo("\r[", itos(kb), "kb] ", display_name);
     old_kb = kb;
    }
   }
   if (status >= download_status_finished)
   {
    echoln();
    break;
   }
   sleep(1 * millisecond);
  }
  if (download_service.status == download_status_finished)
   break;
  sleep(3 * second);
  if (download_service.status == download_status_error_open_url)
   echoln("[ERROR] InternetOpenUrl failed, retrying...");
  if (download_service.status == download_status_error_read_file)
   echoln("[ERROR] InternetReadFile failed, retrying...");
 }
}

#define hope(cond, err) \
 do { if (!cond) die(err, TEXT(err)); } while(0)
void die(const char* err, LPCWSTR werr)
{
 download_service_stop();
 if (gui_mode || subprocess_mode)
 {
  MessageBox(GetConsoleWindow(), werr, null, MB_TOPMOST | MB_ICONERROR);
 }
 else
 {
  echoln(err);
 }
 exit(1);
}

LRESULT CALLBACK _dialog_service_proc(HWND service, UINT message, WPARAM wparam, LPARAM lparam)
{
 if (message == WM_APP)
 {
  HWND dlg = autocast(GetWindowLongPtr(service, GWLP_USERDATA));
  SetForegroundWindow(dlg);
  return (0);
 }

 if (message == WM_DESTROY)
 {
  PostQuitMessage(0);
  return (0);
 }

 return DefWindowProc(service, message, wparam, lparam);
}

void dialog_refill(HWND dlg)
{
 // iterating backwards, assuming versions are sorted backwards in the JSON
 {
  char (*msvc_versions)[16] = preview_mode ? preview_mode_msvc_versions : release_msvc_versions;
  i64 msvc_versions_count = preview_mode ? preview_mode_msvc_versions_count : release_msvc_versions_count;
  HWND item = GetDlgItem(dlg, ID_COMBO_MSVC);
  SendMessage(item, CB_RESETCONTENT, 0, 0);
  for (i64 i = msvc_versions_count - 1; i >= 0; i--)
  {
   WCHAR wver[MAX_ENV_LEN];
   utf8_to_utf16(msvc_versions[i], -1, wver, countof(wver));
   SendMessage(item, CB_ADDSTRING, 0, cast(LPARAM, wver));
  }
  SendMessage(item, CB_SETCURSEL, 0, 0);
  string_copy(msvc_version, msvc_versions[msvc_versions_count - 1]);
 }
 {
  char (*sdk_versions)[8] = preview_mode ? preview_mode_sdk_versions : release_sdk_versions;
  i64 sdk_versions_count = preview_mode ? preview_mode_sdk_versions_count : release_sdk_versions_count;
  HWND item = GetDlgItem(dlg, ID_COMBO_SDK);
  SendMessage(item, CB_RESETCONTENT, 0, 0);
  for (i64 i = sdk_versions_count - 1; i >= 0; i--)
  {
   WCHAR wver[MAX_ENV_LEN];
   utf8_to_utf16(sdk_versions[i], -1, wver, countof(wver));
   SendMessage(item, CB_ADDSTRING, 0, cast(LPARAM, wver));
  }
  SendMessage(item, CB_SETCURSEL, 0, 0);
  string_copy(sdk_version, sdk_versions[sdk_versions_count - 1]);
 }
 {
  char* license_url = preview_mode ? preview_mode_license_url : release_license_url;
  char* link_url = string_join(64 + L_MAX_URL_LENGTH * 3, "I accept the <a href=\"", license_url, "\">License Agreement</a>");
  WCHAR wlink_url[64 + L_MAX_URL_LENGTH];
  utf8_to_utf16(link_url, -1, wlink_url, countof(wlink_url));
  SetDlgItemText(dlg, ID_SYSLINK_LICENSE, wlink_url);
 }
}

// update install_path, add shield to install button if needed
void dialog_path_update(HWND dlg)
{
 WCHAR wpath[MAX_PATH] = {0};
 GetDlgItemText(dlg, ID_EDIT_PATH, wpath, MAX_PATH);
 utf16_to_utf8(wpath, -1, install_path, countof(install_path));
 // check if path is accessible by creating temporary path/~ file
 if (folder_exists(install_path))
 {
  nat n = 0;
  while (wpath[n] != 0)
   n++;
  wpath[n] = L'\\';
  n++;
  wpath[n] = L'~';
  n++;
  wpath[n] = 0;
  HANDLE tfile = CreateFile(wpath, GENERIC_WRITE, 0, null, CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, null);
  if (tfile != INVALID_HANDLE_VALUE)
  {
   CloseHandle(tfile);
   is_admin = false;
   SendMessage(GetDlgItem(dlg, ID_BUTTON_INSTALL), BCM_SETSHIELD, 0, false);
   return;
  }
 }
 else
 {
  bool ok = folder_create(install_path);
  if (ok)
  {
   folder_delete(install_path);
   is_admin = false;
   SendMessage(GetDlgItem(dlg, ID_BUTTON_INSTALL), BCM_SETSHIELD, 0, false);
   return;
  }
 }
 is_admin = (GetLastError() == ERROR_ACCESS_DENIED) ? true : false;
 SendMessage(GetDlgItem(dlg, ID_BUTTON_INSTALL), BCM_SETSHIELD, 0, is_admin);
}

INT_PTR CALLBACK dialog_proc(HWND dlg, UINT message, WPARAM wparam, LPARAM lparam)
{
 if (message == WM_INITDIALOG)
 {
  HWND service = find_app_service();
  SetWindowLongPtr(service, GWLP_USERDATA, cast(LONG_PTR, dlg));
  SetWindowLongPtr(service, GWLP_WNDPROC, cast(LONG_PTR, _dialog_service_proc));
  SetForegroundWindow(dlg);
  {
   HICON icon = LoadIcon(GetModuleHandle(null), L"icon");
   SendMessage(dlg, WM_SETICON, ICON_SMALL, cast(LPARAM, icon));
   SendMessage(dlg, WM_SETICON, ICON_BIG, cast(LPARAM, icon));
  }
  {
   HWND item = GetDlgItem(dlg, ID_COMBO_ENV);
   SendMessage(item, CB_ADDSTRING, 0, cast(LPARAM, L"Create environment setup scripts"));
   SendMessage(item, CB_ADDSTRING, 0, cast(LPARAM, L"Create the scripts & add to user environment"));
   SendMessage(item, CB_ADDSTRING, 0, cast(LPARAM, L"Create the scripts & add to global environment"));
   SendMessage(item, CB_SETCURSEL, 0, 0);
  }
  {
   HWND item = GetDlgItem(dlg, ID_COMBO_TARGET);
   SendMessage(item, CB_ADDSTRING, 0, cast(LPARAM, L"x64"));
   SendMessage(item, CB_ADDSTRING, 0, cast(LPARAM, L"x86"));
   SendMessage(item, CB_ADDSTRING, 0, cast(LPARAM, L"arm"));
   SendMessage(item, CB_ADDSTRING, 0, cast(LPARAM, L"arm64"));
   SendMessage(item, CB_SETCURSEL, 0, 0);
  }
  {
   HWND item = GetDlgItem(dlg, ID_COMBO_HOST);
   SendMessage(item, CB_ADDSTRING, 0, cast(LPARAM, L"x64"));
   SendMessage(item, CB_ADDSTRING, 0, cast(LPARAM, L"x86"));
   SendMessage(item, CB_ADDSTRING, 0, cast(LPARAM, L"arm64"));
   SendMessage(item, CB_SETCURSEL, 0, 0);
  }
  {
   HWND item = GetDlgItem(dlg, ID_EDIT_PATH);
   SendMessage(item, WM_SETTEXT, 0, cast(LPARAM, L"C:\\BuildTools"));
   SHAutoComplete(item, SHACF_FILESYSTEM);
  }
  dialog_refill(dlg);
  dialog_path_update(dlg);
  EnableWindow(GetDlgItem(dlg, ID_BUTTON_INSTALL), false);
  return (true);
 }

 if (message == WM_CLOSE)
 {
  HWND service = find_app_service();
  SetWindowLongPtr(service, GWLP_USERDATA, 0);
  SetWindowLongPtr(service, GWLP_WNDPROC, cast(LONG_PTR, _service_proc));
  EndDialog(dlg, 0);
  return (true);
 }

 if (message == WM_COMMAND)
 {
  DWORD command = LOWORD(wparam);

  if (command == ID_CHECK_PREVIEW)
  {
   preview_mode = (IsDlgButtonChecked(dlg, ID_CHECK_PREVIEW) == BST_CHECKED);
   dialog_refill(dlg);
  }

  if (command == ID_COMBO_ENV)
  {
   env_mode = SendMessage(GetDlgItem(dlg, ID_COMBO_ENV), CB_GETCURSEL, 0, 0);
  }

  if (command == ID_COMBO_MSVC)
  {
   i64 index = SendMessage(GetDlgItem(dlg, ID_COMBO_MSVC), CB_GETCURSEL, 0, 0);
   string_copy(msvc_version, preview_mode ? preview_mode_msvc_versions[preview_mode_msvc_versions_count - index - 1] : release_msvc_versions[release_msvc_versions_count - index - 1]);
  }

  if (command == ID_COMBO_SDK)
  {
   i64 index = SendMessage(GetDlgItem(dlg, ID_COMBO_SDK), CB_GETCURSEL, 0, 0);
   string_copy(sdk_version, preview_mode ? preview_mode_sdk_versions[preview_mode_sdk_versions_count - index - 1] : release_sdk_versions[release_sdk_versions_count - index - 1]);
  }

  if (command == ID_COMBO_TARGET)
  {
   i64 index = SendMessage(GetDlgItem(dlg, ID_COMBO_TARGET), CB_GETCURSEL, 0, 0);
   string_copy(target_arch, targets[index]);
  }

  if (command == ID_COMBO_HOST)
  {
   i64 index = SendMessage(GetDlgItem(dlg, ID_COMBO_HOST), CB_GETCURSEL, 0, 0);
   string_copy(host_arch, hosts[index]);
  }

  if (command == ID_EDIT_PATH)
  {
   if (HIWORD(wparam) == EN_CHANGE)
    dialog_path_update(dlg);
  }

  if (command == ID_BUTTON_BROWSE)
  {
   IFileOpenDialog* fd;
   CoCreateInstance(&CLSID_FileOpenDialog, null, CLSCTX_INPROC_SERVER, &IID_IFileOpenDialog, cast(void**, &fd));
   // randomly generated GUID
   vcall(fd, lpVtbl->SetClientGuid, &(GUID){0x65CDF695, 0x0A2C, 0x46DB, {0x9F, 0xE8, 0x73, 0xCF, 0xA7, 0xF7, 0x1E, 0x65}});
   DWORD options;
   vcall(fd, lpVtbl->GetOptions, &options);
   vcall(fd, lpVtbl->SetOptions, options | FOS_PICKFOLDERS);
   if (vcall(fd, lpVtbl->Show, null) == 0)
   {
    IShellItem* psi;
    vcall(fd, lpVtbl->GetResult, &psi);
    {
     LPWSTR wpath;
     vcall(psi, lpVtbl->GetDisplayName, SIGDN_DESKTOPABSOLUTEPARSING, &wpath);
     SetDlgItemText(dlg, ID_EDIT_PATH, wpath);
     CoTaskMemFree(wpath);
    }
    vcall(psi, lpVtbl->Release);
   }
   vcall(fd, lpVtbl->Release);
  }

  if (command == ID_CHECK_LICENSE)
  {
   license_accepted = (IsDlgButtonChecked(dlg, ID_CHECK_LICENSE) == BST_CHECKED);
   EnableWindow(GetDlgItem(dlg, ID_BUTTON_INSTALL), license_accepted);
  }

  if (command == ID_BUTTON_INSTALL)
  {
   if (folder_exists(install_path))
   {
    if (!is_folder_empty(install_path))
    {
     MessageBox(dlg, L"Destination folder is not empty.", null, MB_TOPMOST | MB_ICONERROR);
    }
   }
   else
   {
    // creating a folder to check if it possible to create one
    if (folder_create(install_path))
    {
     folder_delete(install_path);
    }
    else if (GetLastError() != ERROR_ACCESS_DENIED)
    {
     MessageBox(dlg, L"Destination folder cannot be made.", null, MB_TOPMOST | MB_ICONERROR);
    }
   }
   install_start = true;
   SendMessage(dlg, WM_CLOSE, 0, 0);
  }
 }

 if (message == WM_NOTIFY)
 {
  NMHDR* param = cast(NMHDR*, lparam);
  if ((param->code == NM_CLICK) || (param->code == NM_RETURN))
  {
   NMLINK* link = cast(NMLINK*, lparam);
   if (link->hdr.idFrom == ID_SYSLINK_LICENSE)
    ShellExecute(null, L"open", link->item.szUrl, null, null, SW_NORMAL);
  }
 }

 return (false);
}

void parse_vsmanifest(const char* path, bool preview_mode)
{
 echoln("Parsing ", preview_mode ? "Preview" : "Release", " Visual Studio Manifest...");
 char manifest_url[countof(release_manifest_url)];
 char license_url[countof(release_license_url)];
 json_context jc;
 jc.f = file_open(path, false);
 json_parser p = json_parser_begin(json_file_peek, &jc);
 hope(json_object_key_find(&p, "channelItems"), "channelItems not found");
 while (json_array_next(&p))
 {
  json_context obj_state = jc;
  if (!json_object_key_find(&p, "id"))
  {
   json_object_skip(&p);
   continue;
  }
  char id[64];
  json_string_extract(&p, id);
  if (string_is(id, preview_mode ? "Microsoft.VisualStudio.Manifests.VisualStudioPreview" : "Microsoft.VisualStudio.Manifests.VisualStudio"))
  {
   json_file_context_restore(&jc, obj_state);
   hope(json_object_key_find(&p, "payloads"), "payloads not found");
   json_array_next(&p);
   hope(json_object_key_find(&p, "url"), "manifest url not found");
   json_string_extract(&p, manifest_url);
   json_object_skip(&p);
   json_array_next(&p);
  }
  if (string_is(id, "Microsoft.VisualStudio.Product.BuildTools"))
  {
   json_file_context_restore(&jc, obj_state);
   hope(json_object_key_find(&p, "localizedResources"), "localizedResources not found");
   while (json_array_next(&p))
   {
    json_context res_state = jc;
    hope(json_object_key_find(&p, "language"), "license language not found");
    char language[8];
    json_string_extract(&p, language);
    if (string_is(language, "en-us"))
    {
     json_file_context_restore(&jc, res_state);
     hope(json_object_key_find(&p, "license"), "license url not found");
     json_string_extract(&p, license_url);
     json_object_skip(&p);
     break;
    }
    json_object_skip(&p);
   }
  }
  json_object_skip(&p);
 }
 file_close(&jc.f);
 memcpy(preview_mode ? preview_mode_manifest_url : release_manifest_url, manifest_url, sizeof(manifest_url));
 memcpy(preview_mode ? preview_mode_license_url : release_license_url, license_url, sizeof(license_url));
}

void parse_manifest(const char* path, bool preview_mode)
{
 echoln("Parsing ", preview_mode ? "Preview" : "Release", " Build Tool Manifest...");
 char msvc_versions[32][16] = {0};
 i64 msvc_versions_count = 0;
 char sdk_versions[16][8] = {0};
 i64 sdk_versions_count = 0;
 json_context jc;
 jc.f = file_open(path, false);
 json_parser p = json_parser_begin(json_file_peek, &jc);
 hope(json_object_key_find(&p, "packages"), "packages not found");
 while (json_array_next(&p))
 {
  hope(json_object_key_find(&p, "id"), "id not found");
  char id[64];
  json_string_extract(&p, id);
  char id_lower[countof(id)];
  memcpy(id_lower, id, sizeof(id));
  string_lower(id_lower);
  bool is_msvc_version = true;
  is_msvc_version = string_trim_left(id_lower, "microsoft.visualstudio.component.vc.") ? is_msvc_version : false;
  is_msvc_version = string_trim_right(id_lower, ".x86.x64") ? is_msvc_version : false;
  bool is_sdk_version = false;
  is_sdk_version = string_trim_left(id_lower, "microsoft.visualstudio.component.windows10sdk.") ? true : is_sdk_version;
  is_sdk_version = string_trim_left(id_lower, "microsoft.visualstudio.component.windows11sdk.") ? true : is_sdk_version;
  bool is_digit = string_has_only(id_lower, string_slice(".0123456789"));
  if (is_msvc_version & is_digit)
   _string_copy(msvc_versions[msvc_versions_count++], sizeof(msvc_versions[0]), id_lower);
  if (is_sdk_version & is_digit)
   _string_copy(sdk_versions[sdk_versions_count++], sizeof(sdk_versions[0]), id_lower);
  json_object_skip(&p);
 }
 file_close(&jc.f);
 memcpy(preview_mode ? preview_mode_sdk_versions : release_sdk_versions, sdk_versions, sizeof(sdk_versions));
 memcpy(preview_mode ? preview_mode_msvc_versions : release_msvc_versions, msvc_versions, sizeof(msvc_versions));
 release_msvc_versions_count = preview_mode ? release_msvc_versions_count : msvc_versions_count;
 preview_mode_msvc_versions_count = preview_mode ? msvc_versions_count : preview_mode_msvc_versions_count;
 release_sdk_versions_count = preview_mode ? release_sdk_versions_count : sdk_versions_count;
 preview_mode_sdk_versions_count = preview_mode ? sdk_versions_count : preview_mode_sdk_versions_count;
}

void extract_payload_info(json_context* jc, json_parser* p, char (*file_name)[MAX_PATH * 3], char (*url)[L_MAX_URL_LENGTH * 3])
{
 // TODO: extract and validate sha256 (given sha256 doesn't seem to correspond to the file's actual sha256)
 json_context payload_state = *jc;
 hope(json_object_key_find(p, "fileName"), "fileName not found");
 json_string_extract(p, *file_name);
 json_file_context_restore(jc, payload_state);
 hope(json_object_key_find(p, "url"), "url not found");
 json_string_extract(p, *url);
 json_file_context_restore(jc, payload_state);
}

void start(void)
{
 bool gui_mode = !invoked_from_console;
 bool list_versions = false;
 for (nat i = 0; i < args_count; i++)
 {
  if (i == 0)
   continue;
  const char* arg = args[i];
  if (string_is(arg, "!"))
  {
   subprocess_mode = true;
  }
  else if (string_is(arg, "gui"))
  {
   gui_mode = true;
  }
  else if (string_is(arg, "list"))
  {
   list_versions = true;
  }
  else if (string_is(arg, "accept_license"))
  {
   license_accepted = true;
  }
  else if (string_is(arg, "preview_mode"))
  {
   preview_mode = true;
  }
  else if (string_starts_with(arg, "msvc="))
  {
   string_copy(msvc_version, arg);
   string_trim_left(msvc_version, "msvc=");
  }
  else if (string_starts_with(arg, "sdk="))
  {
   string_copy(sdk_version, arg);
   string_trim_left(sdk_version, "sdk=");
  }
  else if (string_starts_with(arg, "target="))
  {
   string_copy(target_arch, arg);
   string_trim_left(target_arch, "target=");
  }
  else if (string_starts_with(arg, "host="))
  {
   string_copy(host_arch, arg);
   string_trim_left(host_arch, "host=");
  }
  else if (string_starts_with(arg, "env="))
  {
   env_mode = 0;
   env_mode = string_is(arg, "env=global") ? 2 : env_mode;
   env_mode = string_is(arg, "env=user") ? 1 : env_mode;
  }
  else if (string_starts_with(arg, "path="))
  {
   string_copy(install_path, arg);
   string_trim_left(install_path, "path=");
   string_trim_left(install_path, "\"");
   string_trim_right(install_path, "\"");
  }
  else
  {
   echo(
   "usage: PortableBuildTools.exe [gui] [list] [accept_license] [preview_mode] [msvc=MSVC version] [sdk=SDK version] [target=x64/x86/arm/arm64] [host=x64/x86/arm64] [env=none/user/global] [path=\"C:\\BuildTools\"]\n"
    "\n"
    "*gui: run PortableBuildTools in GUI mode\n"
    "*list: show all available Build Tools versions\n"
    "*accept_license: auto-accept the license [default: ask]\n"
    "*preview_mode: install from the Preview channel, instead of the Release channel [default: Release channel]\n"
    "*msvc: which MSVC toolchain version to install [default: whatever is latest]\n"
    "*sdk: which Windows SDK version to install [default: whatever is latest]\n"
    "*target: target architecture [default: x64]\n"
    "*host: host architecture [default: x64]\n"
    "*env: if supplied, then the installed path will be added to PATH environment variable, for the current user or for all users [default: none]\n"
    "*path: installation path [default: C:\\BuildTools]\n"
   );
   exit(1);
  }
 }
 {
  WCHAR wtemp_path[MAX_PATH];
  GetTempPath(countof(wtemp_path), wtemp_path);
  utf16_to_utf8(wtemp_path, -1, temp_path, countof(temp_path));
  string_append(temp_path, "BuildTools");
 }
 CoInitializeEx(null, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
 RegisterClass(&(WNDCLASS){.lpfnWndProc = download_service_proc, .hInstance = GetModuleHandle(null), .lpszClassName = TEXT(appid) L":download_service"});
 if (!subprocess_mode)
 {
  appid_instance_guard();
  folder_create(temp_path);
  {
   download_service_start();
   echoln("Downloading manifest files...");
   char manifest_path[MAX_PATH * 3];
   download_file(release_vsmanifest_url, string_join_buf(manifest_path, temp_path, "\\Manifest.Release.json"), "Manifest.Release.json");
   download_file(preview_mode_vsmanifest_url, string_join_buf(manifest_path, temp_path, "\\Manifest.Preview.json"), "Manifest.Preview.json");
   echoln("Parsing manifest files...");
   parse_vsmanifest(string_join_buf(manifest_path, temp_path, "\\Manifest.Release.json"), false);
   parse_vsmanifest(string_join_buf(manifest_path, temp_path, "\\Manifest.Preview.json"), true);
   echoln("Downloading Build Tools manifest files...");
   download_file(release_manifest_url, string_join_buf(manifest_path, temp_path, "\\BuildToolsManifest.Release.json"), "BuildToolsManifest.Release.json");
   download_file(preview_mode_manifest_url, string_join_buf(manifest_path, temp_path, "\\BuildToolsManifest.Preview.json"), "BuildToolsManifest.Preview.json");
   echoln("Parsing Build Tools manifest files...");
   parse_manifest(string_join_buf(manifest_path, temp_path, "\\BuildToolsManifest.Release.json"), false);
   parse_manifest(string_join_buf(manifest_path, temp_path, "\\BuildToolsManifest.Preview.json"), true);
   echoln("");
   download_service_stop();
  }
  if (gui_mode)
  {
   FreeConsole();
   INITCOMMONCONTROLSEX controls = (INITCOMMONCONTROLSEX){0};
   controls.dwSize = sizeof(INITCOMMONCONTROLSEX);
   controls.dwICC = ICC_STANDARD_CLASSES | ICC_LINK_CLASS;
   InitCommonControlsEx(&controls);
   DialogBox(GetModuleHandle(null), L"dialog", null, dialog_proc);
   if (!install_start)
    exit(1);
   char* env_mode_str = "none";
   env_mode_str = (env_mode == 1) ? "user" : env_mode_str;
   env_mode_str = (env_mode == 2) ? "global" : env_mode_str;
   char* params = string_join(MAX_CMDLINE_LEN * 3, "! ", preview_mode ? "preview_mode" : "", " msvc=", msvc_version, " sdk=", sdk_version, " target=", target_arch, " host=", host_arch, " env=", env_mode_str, " path=\"", install_path, "\"");
   WCHAR wprogram[MAX_PATH];
   const char* program = args[0];
   utf8_to_utf16(program, -1, wprogram, countof(wprogram));
   WCHAR wparams[MAX_CMDLINE_LEN];
   utf8_to_utf16(params, -1, wparams, countof(wparams));
   SHELLEXECUTEINFO info = (SHELLEXECUTEINFO){0};
   info.cbSize = sizeof(SHELLEXECUTEINFO);
   info.fMask = SEE_MASK_NOCLOSEPROCESS;
   info.lpVerb = is_admin ? L"runas" : L"open";
   info.lpFile = wprogram;
   info.lpParameters = wparams;
   info.nShow = SW_NORMAL;
   if (!ShellExecuteEx(&info))
    exit(1);
   WaitForSingleObject(info.hProcess, INFINITE);
   CloseHandle(info.hProcess);
   return;
  }
  if (list_versions)
  {
   echoln("MSVC Versions (Release): ");
   for (i64 i = release_msvc_versions_count - 1; i >= 0; i--)
    echoln(release_msvc_versions[i]);
   echoln("SDK Versions (Release): ");
   for (i64 i = release_sdk_versions_count - 1; i >= 0; i--)
    echoln(release_sdk_versions[i]);
   echoln("MSVC Versions (Preview): ");
   for (i64 i = preview_mode_msvc_versions_count - 1; i >= 0; i--)
    echoln(preview_mode_msvc_versions[i]);
   echoln("SDK Versions (Preview): ");
   for (i64 i = preview_mode_sdk_versions_count - 1; i >= 0; i--)
    echoln(preview_mode_sdk_versions[i]);
   return;
  }
  // cli mode
  {
   bool found = false;
   for (i64 i = 0; i < countof(targets); i++)
    found = string_is(target_arch, targets[i]) ? true : found;
   if (!found)
   {
    echoln("Invalid target architecture: ", target_arch);
    exit(1);
   }
  }
  {
   bool found = false;
   for (i64 i = 0; i < countof(hosts); i++)
    found = string_is(host_arch, hosts[i]) ? true : found;
   if (!found)
   {
    echoln("Invalid host architecture: ", host_arch);
    exit(1);
   }
  }
  {
   char (*msvc_versions)[16] = preview_mode ? preview_mode_msvc_versions : release_msvc_versions;
   i64 msvc_versions_count = preview_mode ? preview_mode_msvc_versions_count : release_msvc_versions_count;
   if (msvc_version[0] == 0)
    string_copy(msvc_version, msvc_versions[msvc_versions_count - 1]);
   bool found = false;
   for (i64 i = 0; i < msvc_versions_count; i++)
    found = string_is(msvc_version, msvc_versions[i]) ? true : found;
   if (!found)
   {
    echoln("Invalid MSVC version: ", msvc_version);
    exit(1);
   }
  }
  {
   char (*sdk_versions)[8] = preview_mode ? preview_mode_sdk_versions : release_sdk_versions;
   i64 sdk_versions_count = preview_mode ? preview_mode_sdk_versions_count : release_sdk_versions_count;
   if (sdk_version[0] == 0)
    string_copy(sdk_version, sdk_versions[sdk_versions_count - 1]);
   bool found = false;
   for (i64 i = 0; i < sdk_versions_count; i++)
    found = string_is(sdk_version, sdk_versions[i]) ? true : found;
   if (!found)
   {
    echoln("Invalid SDK version: ", sdk_version);
    exit(1);
   }
  }
  echoln("Preview channel: ", preview_mode ? "true" : "false");
  echoln("MSVC version: ", msvc_version);
  echoln("SDK version: ", sdk_version);
  echoln("Target arch: ", target_arch);
  echoln("Host arch: ", host_arch);
  echoln("Install path: ", install_path);
  if (folder_exists(install_path))
  {
   if (!is_folder_empty(install_path))
   {
    echoln("Destination folder is not empty.");
    exit(1);
   }
  }
  else
  {
   WCHAR wpath[MAX_PATH];
   utf8_to_utf16(install_path, -1, wpath, MAX_PATH);
   if (!CreateDirectory(wpath, null))
   {
    const char* error = (GetLastError() == ERROR_ACCESS_DENIED) ? "No access to the destination folder, try running this program as administrator." : "Destination folder cannot be made.";
    echoln(error);
    exit(1);
   }
  }
  if (license_accepted)
  {
   install_start = true;
   echoln("");
  }
  else
  {
   echoln("Do you accept the license agreement? [y]/n ", preview_mode ? preview_mode_license_url : release_license_url);
   char answer[4];
   console_read(answer);
   string_lower(answer);
   install_start = string_is(answer, "") | string_is(answer, "y") | string_is(answer, "yes");
  }
  if (!install_start)
  {
   echoln("License was not accepted, cannot proceed further.");
   exit(1);
  }
 }
 // download install files
 download_service_start();
 char* msvc_path = string_join(MAX_PATH * 3, temp_path, "\\msvc");
 char* sdk_path = string_join(MAX_PATH * 3, temp_path, "\\sdk");
 folder_create(msvc_path);
 folder_create(sdk_path);
 {
  char msvc_packages[32][128];
  i64 msvc_packages_count = 0;
  _string_join(msvc_packages[msvc_packages_count++], sizeof(msvc_packages[0]), "microsoft.visualcpp.dia.sdk");
  _string_join(msvc_packages[msvc_packages_count++], sizeof(msvc_packages[0]), "microsoft.vc.", msvc_version, ".crt.headers.base");
  _string_join(msvc_packages[msvc_packages_count++], sizeof(msvc_packages[0]), "microsoft.vc.", msvc_version, ".crt.source.base");
  _string_join(msvc_packages[msvc_packages_count++], sizeof(msvc_packages[0]), "microsoft.vc.", msvc_version, ".asan.headers.base");
  _string_join(msvc_packages[msvc_packages_count++], sizeof(msvc_packages[0]), "microsoft.vc.", msvc_version, ".pgo.headers.base");
  _string_join(msvc_packages[msvc_packages_count++], sizeof(msvc_packages[0]), "microsoft.vc.", msvc_version, ".tools.host", host_arch, ".target", target_arch, ".base");
  _string_join(msvc_packages[msvc_packages_count++], sizeof(msvc_packages[0]), "microsoft.vc.", msvc_version, ".tools.host", host_arch, ".target", target_arch, ".res.base");
  _string_join(msvc_packages[msvc_packages_count++], sizeof(msvc_packages[0]), "microsoft.vc.", msvc_version, ".crt.", target_arch, ".desktop.base");
  _string_join(msvc_packages[msvc_packages_count++], sizeof(msvc_packages[0]), "microsoft.vc.", msvc_version, ".crt.", target_arch, ".store.base");
  _string_join(msvc_packages[msvc_packages_count++], sizeof(msvc_packages[0]), "microsoft.vc.", msvc_version, ".premium.tools.host", host_arch, ".target", target_arch, ".base");
  _string_join(msvc_packages[msvc_packages_count++], sizeof(msvc_packages[0]), "microsoft.vc.", msvc_version, ".pgo.", target_arch, ".base");
  _string_join(msvc_packages[msvc_packages_count++], sizeof(msvc_packages[0]), "microsoft.vc.", msvc_version, ".asan.x86.base");
  _string_join(msvc_packages[msvc_packages_count++], sizeof(msvc_packages[0]), "microsoft.vc.", msvc_version, ".asan.x64.base");
  char sdk_packages[32][128];
  i64 sdk_packages_count = 0;
  _string_join(sdk_packages[sdk_packages_count++], sizeof(sdk_packages[0]), "Windows SDK for Windows Store Apps Tools-x86_en-us.msi");
  _string_join(sdk_packages[sdk_packages_count++], sizeof(sdk_packages[0]), "Windows SDK for Windows Store Apps Headers-x86_en-us.msi");
  _string_join(sdk_packages[sdk_packages_count++], sizeof(sdk_packages[0]), "Windows SDK for Windows Store Apps Headers OnecoreUap-x86_en-us.msi");
  _string_join(sdk_packages[sdk_packages_count++], sizeof(sdk_packages[0]), "Windows SDK for Windows Store Apps Libs-x86_en-us.msi");
  _string_join(sdk_packages[sdk_packages_count++], sizeof(sdk_packages[0]), "Universal CRT Headers Libraries and Sources-x86_en-us.msi");
  _string_join(sdk_packages[sdk_packages_count++], sizeof(sdk_packages[0]), "Windows SDK Desktop Libs ", target_arch, "-x86_en-us.msi");
  for (i64 i = 0; i < countof(targets); i++)
  {
   char* t = targets[i];
   _string_join(sdk_packages[sdk_packages_count++], sizeof(sdk_packages[0]), "Windows SDK Desktop Headers ", t, "-x86_en-us.msi");
   _string_join(sdk_packages[sdk_packages_count++], sizeof(sdk_packages[0]), "Windows SDK OnecoreUap Headers ", t, "-x86_en-us.msi");
  }
  char sdk_package[32] = {0};
  json_context jc;
  char* manifest_path = string_join(MAX_PATH * 3, temp_path, "\\", preview_mode ? "BuildToolsManifest.Preview.json" : "BuildToolsManifest.Release.json");
  echoln("Parsing ", preview_mode ? "Preview" : "Release", " Build Tool Manifest...");
  jc.f = file_open(manifest_path, false);
  json_parser p = json_parser_begin(json_file_peek, &jc);
  hope(json_object_key_find(&p, "packages"), "packages not found");
  json_context packages_state = jc;
  while (json_array_next(&p))
  {
   hope(json_object_key_find(&p, "id"), "id not found");
   char id[64];
   json_string_extract(&p, id);
   char id_lower[countof(id)];
   memcpy(id_lower, id, sizeof(id));
   string_lower(id_lower);
   bool is_sdk = false;
   is_sdk = string_starts_with(id_lower, "microsoft.visualstudio.component.windows10sdk.") ? true : is_sdk;
   is_sdk = string_starts_with(id_lower, "microsoft.visualstudio.component.windows11sdk.") ? true : is_sdk;
   if (is_sdk & string_ends_with(id_lower, sdk_version))
   {
    hope(json_object_key_find(&p, "dependencies"), "dependencies not found");
    hope(json_object_next(&p), "dependencies is empty");
    json_object_key_extract(&p, sdk_package);
    break;
   }
   json_object_skip(&p);
  }
  json_file_context_restore(&jc, packages_state);
  bool redist_found = false;
  char* redist_suffix = string_join(32, string_is(target_arch, "arm") ? ".onecore.desktop" : "");
  char redist_name[128];
  string_join_buf(redist_name, "microsoft.vc.", msvc_version, ".crt.redist.", target_arch, redist_suffix, ".base");
  while (json_array_next(&p))
  {
   hope(json_object_key_find(&p, "id"), "id not found");
   char id[64];
   json_string_extract(&p, id);
   char id_lower[countof(id)];
   memcpy(id_lower, id, sizeof(id));
   string_lower(id_lower);
   if (string_is(id_lower, redist_name))
   {
    redist_found = true;
    _string_join(msvc_packages[msvc_packages_count++], sizeof(msvc_packages[0]), redist_name);
    break;
   }
   json_object_skip(&p);
  }
  json_file_context_restore(&jc, packages_state);
  if (!redist_found)
  {
   string_join_buf(redist_name, "microsoft.visualcpp.crt.redist.", target_arch, redist_suffix);
   while (json_array_next(&p))
   {
    hope(json_object_key_find(&p, "id"), "id not found");
    char id[64];
    json_string_extract(&p, id);
    char id_lower[countof(id)];
    memcpy(id_lower, id, sizeof(id));
    string_lower(id_lower);
    if (string_is(id_lower, redist_name))
    {
     redist_found = true;
     hope(json_object_key_find(&p, "dependencies"), "dependencies not found");
     while (json_object_next(&p))
     {
      char redist_package[128];
      json_object_key_extract(&p, redist_package);
      if (string_ends_with(redist_package, ".base"))
      {
       string_lower(redist_package);
       _string_join(msvc_packages[msvc_packages_count++], sizeof(msvc_packages[0]), redist_package);
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
  while (json_array_next(&p))
  {
   json_context pkg_state = jc;
   hope(json_object_key_find(&p, "id"), "id not found");
   char id[64];
   json_string_extract(&p, id);
   json_file_context_restore(&jc, pkg_state);
   char lang[8] = {0};
   if (json_object_key_find(&p, "language"))
    json_string_extract(&p, lang);
   json_file_context_restore(&jc, pkg_state);
   char id_lower[countof(id)];
   memcpy(id_lower, id, sizeof(id));
   string_lower(id_lower);
   bool msvc_pkg = false;
   for (i64 i = 0; i < msvc_packages_count; i++)
    msvc_pkg = string_is(id_lower, msvc_packages[i]) ? true : msvc_pkg;
   msvc_pkg = (string_is(lang, "en-US") | string_is(lang, "")) ? msvc_pkg : false;
   if (msvc_pkg)
   {
    hope(json_object_key_find(&p, "payloads"), "payloads not found");
    while (json_array_next(&p))
    {
     char file_name[MAX_PATH * 3];
     char url[L_MAX_URL_LENGTH * 3];
     extract_payload_info(&jc, &p, &file_name, &url);
     json_object_skip(&p);
     char* path = string_join(MAX_PATH * 3, msvc_path, "\\", file_name);
     download_file(url, path, file_name);
    }
   }
   if (string_is(id, sdk_package))
   {
    char msi_cabs[64][37];
    i64 msi_cabs_count = 0;
    hope(json_object_key_find(&p, "payloads"), "payloads not found");
    json_context payloads_state = jc;
    // download SDK payloads
    while (json_array_next(&p))
    {
     char file_name[MAX_PATH * 3];
     char url[L_MAX_URL_LENGTH * 3];
     extract_payload_info(&jc, &p, &file_name, &url);
     json_object_skip(&p);
     if (string_trim_left(file_name, "Installers\\"))
     {
      bool sdk_pkg = false;
      for (i64 i = 0; i < sdk_packages_count; i++)
       sdk_pkg = string_is(file_name, sdk_packages[i]) ? true : sdk_pkg;
      if (sdk_pkg)
      {
       char* path = string_join(MAX_PATH * 3, sdk_path, "\\", file_name);
       download_file(url, path, file_name);
       // extract .cab file info
       struct file msi = file_open(path, false);
       char chunk[mem_page_size];
       while (true)
       {
        i64 n = file_read(&msi, chunk, countof(chunk));
        if (n <= 0)
         break;
        for (i64 i = 0; i < n; i++)
        {
         char str[4];
         str[0] = chunk[i];
         str[1] = ((i + 1) < n) ? chunk[i + 1] : 0;
         str[2] = ((i + 2) < n) ? chunk[i + 2] : 0;
         str[3] = ((i + 3) < n) ? chunk[i + 3] : 0;
         bool match = (str[0] == '.') & (str[1] == 'c') & (str[2] == 'a') & (str[3] == 'b');
         if (match)
         {
          i64 index = msi.pos - countof(chunk) + i;
          struct file cabs_reader = file_open(path, false);
          file_seek(&cabs_reader, index - 32);
          file_read(&cabs_reader, msi_cabs[msi_cabs_count], 36);
          file_close(&cabs_reader);
          msi_cabs[msi_cabs_count][36] = 0;
          msi_cabs_count++;
         }
        }
        if (msi.pos == msi.size)
         break;
        file_seek(&msi, msi.pos - 3);
       }
       file_close(&msi);
      }
     }
    }
    json_file_context_restore(&jc, payloads_state);
    // download .cab packages
    while (json_array_next(&p))
    {
     char file_name[MAX_PATH * 3];
     char url[L_MAX_URL_LENGTH * 3];
     extract_payload_info(&jc, &p, &file_name, &url);
     json_object_skip(&p);
     if (string_trim_left(file_name, "Installers\\"))
     {
      bool cab_pkg = false;
      for (i64 i = 0; i < msi_cabs_count; i++)
       cab_pkg = string_is(file_name, msi_cabs[i]) ? true : cab_pkg;
      if (cab_pkg)
      {
       char* path = string_join(MAX_PATH * 3, sdk_path, "\\", file_name);
       download_file(url, path, file_name);
      }
     }
    }
   }
   json_object_skip(&p);
  }
  file_close(&jc.f);
 }
 download_service_stop();
 // install the files
 folder_create(install_path);
 echoln("Installing MSVC...");
 {
  struct folder msvc_folder = folder_open(msvc_path);
  while (folder_next(&msvc_folder))
  {
   char* file_path = string_join(MAX_PATH * 3, msvc_path, "\\", msvc_folder.file_name);
   struct file msvc_file = file_open(file_path, false);
   mz_zip_archive zip_file = (mz_zip_archive){0};
   zip_file.m_pAlloc = miniz_malloc;
   zip_file.m_pFree = miniz_free;
   zip_file.m_pRealloc = miniz_realloc;
   zip_file.m_pRead = miniz_file_read;
   zip_file.m_pIO_opaque = &msvc_file;
   mz_zip_reader_init(&zip_file, msvc_file.size, 0);
   i64 n = mz_zip_reader_get_num_files(&zip_file);
   for (i64 i = 0; i < n; i++)
   {
    char zipped_file_name[MAX_PATH * 3];
    if (mz_zip_reader_is_file_a_directory(&zip_file, i))
     continue;
    mz_zip_reader_get_filename(&zip_file, i, zipped_file_name, countof(zipped_file_name));
    if (!string_trim_left(zipped_file_name, "Contents/"))
     continue;
    char* zipped_file_path = string_join(MAX_PATH * 3, install_path, "\\", zipped_file_name);
    // walk the path, creating directories as needed, and converting forward slashes into backslashes
    char* s = zipped_file_path;
    while (*s != 0)
    {
     if (*s == '/')
     {
      *s = 0;
      if (!folder_exists(zipped_file_path))
       folder_create(zipped_file_path);
      *s = '\\';
     }
     s++;
    }
    file_create(zipped_file_path);
    struct file zf = file_open(zipped_file_path, true);
    mz_zip_reader_extract_to_callback(&zip_file, i, mz_file_write, &zf, 0);
    file_close(&zf);
   }
   mz_zip_end(&zip_file);
   file_close(&msvc_file);
  }
  folder_close(&msvc_folder);
 }
 echoln("Installing SDK...");
 {
  struct folder sdk_folder = folder_open(sdk_path);
  while (folder_next(&sdk_folder))
  {
   if (!string_ends_with(sdk_folder.file_name, ".msi"))
    continue;
   char* file_path = string_join(MAX_PATH * 3, sdk_path, "\\", sdk_folder.file_name);
   char* params = string_join(MAX_CMDLINE_LEN * 3, "/a \"", file_path, "\" /quiet TARGETDIR=\"", install_path, "\"");
   WCHAR wparams[MAX_CMDLINE_LEN];
   utf8_to_utf16(params, -1, wparams, countof(wparams));
   SHELLEXECUTEINFO info = (SHELLEXECUTEINFO){0};
   info.cbSize = sizeof(SHELLEXECUTEINFO);
   info.fMask = SEE_MASK_NOCLOSEPROCESS;
   info.lpVerb = L"open";
   info.lpFile = L"msiexec.exe";
   info.lpParameters = wparams;
   info.nShow = SW_NORMAL;
   ShellExecuteEx(&info);
   WaitForSingleObject(info.hProcess, INFINITE);
   CloseHandle(info.hProcess);
  }
  folder_close(&sdk_folder);
 }
 char msvcv[MAX_PATH * 3];
 {
  string_join_buf(msvcv, install_path, "\\VC\\Tools\\MSVC");
  struct folder msvcv_folder = folder_open(msvcv);
  folder_next(&msvcv_folder);
  memcpy(msvcv, msvcv_folder.file_name, sizeof(msvcv_folder.file_name));
  folder_close(&msvcv_folder);
 }
 char sdkv[MAX_PATH * 3];
 {
  string_join_buf(sdkv, install_path, "\\Windows Kits\\10\\bin");
  struct folder sdkv_folder = folder_open(sdkv);
  folder_next(&sdkv_folder);
  memcpy(sdkv, sdkv_folder.file_name, sizeof(sdkv_folder.file_name));
  folder_close(&sdkv_folder);
 }
 echoln("Installing debug info...");
 char* redist_path = string_join(MAX_PATH * 3, install_path, "\\VC\\Redist");
 if (folder_exists(redist_path))
 {
  char* debug_version_path = string_join(MAX_PATH * 3, redist_path, "\\MSVC");
  struct folder debug_version_folder = folder_open(debug_version_path);
  folder_next(&debug_version_folder);
  char* debug_path = string_join(MAX_PATH * 3, debug_version_path, "\\", debug_version_folder.file_name, "\\debug_nonredist");
  folder_close(&debug_version_folder);
  struct folder debug_folder = folder_open(debug_path);
  while (folder_next(&debug_folder))
  {
   char* debug_target_path = string_join(MAX_PATH * 3, debug_path, "\\", debug_folder.file_name);
   struct folder debug_target_folder = folder_open(debug_target_path);
   while (folder_next(&debug_target_folder))
   {
    char* debug_package_path = string_join(MAX_PATH * 3, debug_target_path, "\\", debug_target_folder.file_name);
    struct folder debug_package_folder = folder_open(debug_package_path);
    while (folder_next(&debug_package_folder))
    {
     char* file_path = string_join(MAX_PATH * 3, debug_package_path, "\\", debug_package_folder.file_name);
     char* dst_file_path = string_join(MAX_PATH * 3, install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\bin\\Host", host_arch, "\\", debug_folder.file_name, "\\", debug_package_folder.file_name);
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
  if (string_is(host_arch, "x86"))
  {
   string_copy(msdia, "msdia140.dll");
  }
  else if (string_is(host_arch, "x64"))
  {
   string_copy(msdia, "amd64\\msdia140.dll");
  }
  else if (string_is(host_arch, "arm"))
  {
   string_copy(msdia, "arm\\msdia140.dll");
  }
  else if (string_is(host_arch, "arm64"))
  {
   string_copy(msdia, "arm64\\msdia140.dll");
  }
  char* msdia_path = string_join(MAX_PATH * 3, install_path, "\\DIA%20SDK\\bin\\", msdia);
  char* dst_path = string_join(MAX_PATH * 3, install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\bin\\Host", host_arch);
  struct folder dst_folder = folder_open(dst_path);
  while (folder_next(&dst_folder))
  {
   char* dst_file_path = string_join(MAX_PATH * 3, dst_path, "\\", dst_folder.file_name, "\\msdia140.dll");
   file_create(dst_file_path);
   file_copy(dst_file_path, msdia_path);
  }
  folder_close(&dst_folder);
 }
 echoln("Creating a batch setup script...");
 {
  char* bat_path = string_join(MAX_PATH * 3, install_path, "\\devcmd.bat");
  file_create(bat_path);
  struct file f = file_open(bat_path, true);
  char buf[mem_page_size];
  file_write(&f, string_slice("@echo off\n"));
  file_write(&f, string_slice("set BUILD_TOOLS_ROOT=%~dp0\n"));
  file_write(&f, string_slice("set WindowsSDKDir=%BUILD_TOOLS_ROOT%Windows Kits\\10\n"));
  string_join_buf(buf, "set VCToolsInstallDir=%BUILD_TOOLS_ROOT%VC\\Tools\\MSVC\\", msvcv, "\n");
  file_write(&f, buf, strlen(buf));
  string_join_buf(buf, "set WindowsSDKVersion=", sdkv, "\n");
  file_write(&f, buf, strlen(buf));
  string_join_buf(buf, "set VSCMD_ARG_TGT_ARCH=", target_arch, "\n");
  file_write(&f, buf, strlen(buf));
  string_join_buf(buf, "set VSCMD_ARG_HOST_ARCH=", host_arch, "\n");
  file_write(&f, buf, strlen(buf));
  file_write(&f, string_slice("set INCLUDE=%VCToolsInstallDir%\\include;"));
  file_write(&f, string_slice("%WindowsSDKDir%\\Include\\%WindowsSDKVersion%\\ucrt;"));
  file_write(&f, string_slice("%WindowsSDKDir%\\Include\\%WindowsSDKVersion%\\shared;"));
  file_write(&f, string_slice("%WindowsSDKDir%\\Include\\%WindowsSDKVersion%\\um;"));
  file_write(&f, string_slice("%WindowsSDKDir%\\Include\\%WindowsSDKVersion%\\winrt;"));
  file_write(&f, string_slice("%WindowsSDKDir%\\Include\\%WindowsSDKVersion%\\cppwinrt;\n"));
  file_write(&f, string_slice("set LIB=%VCToolsInstallDir%\\lib\\%VSCMD_ARG_TGT_ARCH%;"));
  file_write(&f, string_slice("%WindowsSDKDir%\\Lib\\%WindowsSDKVersion%\\ucrt\\%VSCMD_ARG_TGT_ARCH%;"));
  file_write(&f, string_slice("%WindowsSDKDir%\\Lib\\%WindowsSDKVersion%\\um\\%VSCMD_ARG_TGT_ARCH%\n"));
  file_write(&f, string_slice("set BUILD_TOOLS_BIN=%VCToolsInstallDir%\\bin\\Host%VSCMD_ARG_HOST_ARCH%\\%VSCMD_ARG_TGT_ARCH%;"));
  file_write(&f, string_slice("%WindowsSDKDir%\\bin\\%WindowsSDKVersion%\\%VSCMD_ARG_TGT_ARCH%;"));
  file_write(&f, string_slice("%WindowsSDKDir%\\bin\\%WindowsSDKVersion%\\%VSCMD_ARG_TGT_ARCH%\\ucrt\n"));
  file_write(&f, string_slice("set PATH=%BUILD_TOOLS_BIN%;%PATH%\n"));
  file_close(&f);
 }
 echoln("Creating a PowerShell setup script...");
 {
  char* ps_path = string_join(MAX_PATH * 3, install_path, "\\devcmd.ps1");
  file_create(ps_path);
  struct file f = file_open(ps_path, true);
  char buf[mem_page_size];
  file_write(&f, string_slice("#Requires -Version 5\n"));
  file_write(&f, string_slice("param([string]$InstallPath = $PSScriptRoot)\n"));
  file_write(&f, string_slice("$env:BUILD_TOOLS_ROOT = $InstallPath\n"));
  file_write(&f, string_slice("$env:WindowsSDKDir = (Join-Path $InstallPath '\\Windows Kits\\10')\n"));
  file_write(&f, string_slice("$VCToolsVersion = (Get-ChildItem -Directory (Join-Path $InstallPath '\\VC\\Tools\\MSVC' | Sort-Object -Descending LastWriteTime | Select-Object -First 1) -ErrorAction SilentlyContinue).Name\n"));
  file_write(&f, string_slice("if (!$VCToolsVersion) { throw 'VCToolsVersion cannot be determined.' }\n"));
  file_write(&f, string_slice("$env:VCToolsInstallDir = Join-Path (Join-Path $InstallPath '\\VC\\Tools\\MSVC') $VCToolsVersion\n"));
  file_write(&f, string_slice("$env:WindowsSDKVersion = (Get-ChildItem -Directory (Join-Path $env:WindowsSDKDir 'bin') -ErrorAction SilentlyContinue | Sort-Object -Descending LastWriteTime | Select-Object -First 1).Name\n"));
  file_write(&f, string_slice("if (!$env:WindowsSDKVersion ) { throw 'WindowsSDKVersion cannot be determined.' }\n"));
  string_join_buf(buf, "$env:VSCMD_ARG_TGT_ARCH = '", target_arch, "'\n");
  file_write(&f, buf, strlen(buf));
  string_join_buf(buf, "$env:VSCMD_ARG_HOST_ARCH = '", host_arch, "'\n");
  file_write(&f, buf, strlen(buf));
  file_write(&f, string_slice("'Portable Build Tools environment started.'\n"));
  file_write(&f, string_slice("'* BUILD_TOOLS_ROOT   : {0}' -f $env:BUILD_TOOLS_ROOT\n"));
  file_write(&f, string_slice("'* WindowsSDKDir      : {0}' -f $env:WindowsSDKDir\n"));
  file_write(&f, string_slice("'* WindowsSDKVersion  : {0}' -f $env:WindowsSDKVersion\n"));
  file_write(&f, string_slice("'* VCToolsInstallDir  : {0}' -f $env:VCToolsInstallDir\n"));
  file_write(&f, string_slice("'* VSCMD_ARG_TGT_ARCH : {0}' -f $env:VSCMD_ARG_TGT_ARCH\n"));
  file_write(&f, string_slice("'* VSCMD_ARG_HOST_ARCH: {0}' -f $env:VSCMD_ARG_HOST_ARCH\n"));
  file_write(&f, string_slice("$env:INCLUDE =\"$env:VCToolsInstallDir\\include;"));
  file_write(&f, string_slice("$env:WindowsSDKDir\\Include\\$env:WindowsSDKVersion\\ucrt;"));
  file_write(&f, string_slice("$env:WindowsSDKDir\\Include\\$env:WindowsSDKVersion\\shared;"));
  file_write(&f, string_slice("$env:WindowsSDKDir\\Include\\$env:WindowsSDKVersion\\um;"));
  file_write(&f, string_slice("$env:WindowsSDKDir\\Include\\$env:WindowsSDKVersion\\winrt;"));
  file_write(&f, string_slice("$env:WindowsSDKDir\\Include\\$env:WindowsSDKVersion\\cppwinrt\"\n"));
  file_write(&f, string_slice("$env:LIB = \"$env:VCToolsInstallDir\\lib\\$env:VSCMD_ARG_TGT_ARCH;"));
  file_write(&f, string_slice("$env:WindowsSDKDir\\Lib\\$env:WindowsSDKVersion\\ucrt\\$env:VSCMD_ARG_TGT_ARCH;"));
  file_write(&f, string_slice("$env:WindowsSDKDir\\Lib\\$env:WindowsSDKVersion\\um\\$env:VSCMD_ARG_TGT_ARCH\"\n"));
  file_write(&f, string_slice("$env:BUILD_TOOLS_BIN = \"$env:VCToolsInstallDir\\bin\\Host$env:VSCMD_ARG_HOST_ARCH\\$env:VSCMD_ARG_TGT_ARCH;"));
  file_write(&f, string_slice("$env:WindowsSDKDir\\bin\\$env:WindowsSDKVersion\\$env:VSCMD_ARG_TGT_ARCH;"));
  file_write(&f, string_slice("$env:WindowsSDKDir\\bin\\$env:WindowsSDKVersion\\$env:VSCMD_ARG_TGT_ARCH\\ucrt\"\n"));
  file_write(&f, string_slice("$env:PATH = \"$env:BUILD_TOOLS_BIN;$env:PATH\"\n"));
  file_close(&f);
 }
 echoln("Cleanup...");
 {
  folder_delete(redist_path);
  struct folder install_folder = folder_open(install_path);
  while (folder_next(&install_folder))
  {
   char* file_path = string_join(MAX_PATH * 3, install_path, "\\", install_folder.file_name);
   if (string_ends_with(install_folder.file_name, ".msi"))
    file_delete(file_path);
   if (string_starts_with(install_folder.file_name, "DIA%20SDK"))
    folder_delete(file_path);
   if (string_is(install_folder.file_name, "Common7"))
    folder_delete(file_path);
  }
  folder_close(&install_folder);
  char folder_to_remove[MAX_PATH * 3];
  string_join_buf(folder_to_remove, install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\Auxiliary");
  folder_delete(folder_to_remove);
  string_join_buf(folder_to_remove, install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\bin\\Host", target_arch, "\\", host_arch, "\\onecore");
  folder_delete(folder_to_remove);
  string_join_buf(folder_to_remove, install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\lib\\", target_arch, "\\store");
  folder_delete(folder_to_remove);
  string_join_buf(folder_to_remove, install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\lib\\", target_arch, "\\uwp");
  folder_delete(folder_to_remove);
  string_join_buf(folder_to_remove, install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\lib\\", target_arch, "\\enclave");
  folder_delete(folder_to_remove);
  string_join_buf(folder_to_remove, install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\lib\\", target_arch, "\\onecore");
  folder_delete(folder_to_remove);
  string_join_buf(folder_to_remove, install_path, "\\Windows Kits\\10\\Catalogs");
  folder_delete(folder_to_remove);
  string_join_buf(folder_to_remove, install_path, "\\Windows Kits\\10\\DesignTime");
  folder_delete(folder_to_remove);
  string_join_buf(folder_to_remove, install_path, "\\Windows Kits\\10\\bin\\", sdkv, "\\chpe");
  folder_delete(folder_to_remove);
  string_join_buf(folder_to_remove, install_path, "\\Windows Kits\\10\\Lib\\", sdkv, "\\ucrt_enclave");
  folder_delete(folder_to_remove);
  {
   string_join_buf(folder_to_remove, install_path, "\\Windows Kits\\10\\Lib\\", sdkv, "\\ucrt");
   struct folder folder = folder_open(folder_to_remove);
   while (folder_next(&folder))
   {
    if (string_is(folder.file_name, target_arch))
     continue;
    char* file_path = string_join(MAX_PATH * 3, folder_to_remove, "\\", folder.file_name);
    folder_delete(file_path);
   }
   folder_close(&folder);
  }
  {
   string_join_buf(folder_to_remove, install_path, "\\Windows Kits\\10\\Lib\\", sdkv, "\\um");
   struct folder folder = folder_open(folder_to_remove);
   while (folder_next(&folder))
   {
    if (string_is(folder.file_name, target_arch))
     continue;
    char* file_path = string_join(MAX_PATH * 3, folder_to_remove, "\\", folder.file_name);
    folder_delete(file_path);
   }
   folder_close(&folder);
  }
  {
   string_join_buf(folder_to_remove, install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\bin");
   struct folder folder = folder_open(folder_to_remove);
   while (folder_next(&folder))
   {
    if (string_ends_with(folder.file_name, host_arch))
     continue;
    char* file_path = string_join(MAX_PATH * 3, folder_to_remove, "\\", folder.file_name);
    folder_delete(file_path);
   }
   folder_close(&folder);
  }
  {
   string_join_buf(folder_to_remove, install_path, "\\Windows Kits\\10\\bin\\", sdkv);
   struct folder folder = folder_open(folder_to_remove);
   while (folder_next(&folder))
   {
    if (string_is(folder.file_name, host_arch))
     continue;
    char* file_path = string_join(MAX_PATH * 3, folder_to_remove, "\\", folder.file_name);
    folder_delete(file_path);
   }
   folder_close(&folder);
  }
  {
   char* telemetry_path = string_join(MAX_PATH * 3, install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\bin\\Host", host_arch, "\\", target_arch, "\\vctip.exe");
   file_delete(telemetry_path);
   folder_delete(temp_path);
  }
  { // nvcc bodge
   char* vcvars_path = string_join(MAX_PATH * 3, install_path, "\\VC\\Auxiliary\\Build");
   folder_create(vcvars_path);
   char* vcvarsall_path = string_join(MAX_PATH * 3, vcvars_path, "\\vcvarsall.bat");
   file_create(vcvarsall_path);
   char* vcvars64_path = string_join(MAX_PATH * 3, vcvars_path, "\\vcvars64.bat");
   file_create(vcvars64_path);
  }
 }
 if (env_mode != 0)
 {
  echoln("Setting up the environment...");
  HKEY location = (env_mode == 2) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  LPCWSTR subkey = (env_mode == 2) ? L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment" : L"Environment";
  char buf[MAX_ENV_LEN * 3];
  string_join_buf(buf, install_path, "\\VC\\Tools\\MSVC\\", msvcv);
  env_set(location, subkey, L"VCToolsInstallDir", buf);
  string_join_buf(buf, install_path, "\\Windows Kits\\10");
  env_set(location, subkey, L"WindowsSDKDir", buf);
  env_set(location, subkey, L"WindowsSDKVersion", sdkv);
  env_set(location, subkey, L"VSCMD_ARG_HOST_ARCH", host_arch);
  env_set(location, subkey, L"VSCMD_ARG_TGT_ARCH", target_arch);
  string_join_buf(buf,
   install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\include;",
   install_path, "\\Windows Kits\\10\\Include\\", sdkv, "\\ucrt;",
   install_path, "\\Windows Kits\\10\\Include\\", sdkv, "\\shared;",
   install_path, "\\Windows Kits\\10\\Include\\", sdkv, "\\um;",
   install_path, "\\Windows Kits\\10\\Include\\", sdkv, "\\winrt;",
   install_path, "\\Windows Kits\\10\\Include\\", sdkv, "\\cppwinrt"
  );
  env_set(location, subkey, L"INCLUDE", buf);
  string_join_buf(buf,
   install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\lib\\", target_arch, ";",
   install_path, "\\Windows Kits\\10\\Lib\\", sdkv, "\\ucrt\\", target_arch, ";",
   install_path, "\\Windows Kits\\10\\Lib\\", sdkv, "\\um\\", target_arch
  );
  env_set(location, subkey, L"LIB", buf);
  string_join_buf(buf,
   install_path, "\\VC\\Tools\\MSVC\\", msvcv, "\\bin\\Host", host_arch, "\\", target_arch, ";",
   install_path, "\\Windows Kits\\10\\bin\\", sdkv, "\\", target_arch, ";",
   install_path, "\\Windows Kits\\10\\bin\\", sdkv, "\\", target_arch, "\\ucrt\n"
  );
  env_set(location, subkey, L"BUILD_TOOLS_BIN", buf);

  WCHAR wpath[MAX_ENV_LEN];
  char path[countof(wpath) * 3];
  DWORD wpath_size = sizeof(wpath);
  LSTATUS err = RegGetValue(location, subkey, L"Path", RRF_RT_REG_SZ | RRF_NOEXPAND, null, wpath, &wpath_size);
  err = (err == ERROR_UNSUPPORTED_TYPE) ? 0 : err;
  i64 wpath_count = (err != 0) ? 0 : (wpath_size / sizeof(WCHAR));
  utf16_to_utf8(wpath, wpath_count, path, countof(path));
  // split path into strings by ;
  i64 path_count = strlen(path);
  for (i64 i = 0; i < path_count; i++)
   path[i] = (path[i] == ';') ? 0 : path[i];
  // check if %BUILD_TOOLS_BIN% is among them
  bool has_build_tools_bin = false;
  for (i64 i = 0; i < path_count; i++)
  {
   char* entry = &path[i];
   has_build_tools_bin = string_is(entry, "%BUILD_TOOLS_BIN%") ? true : has_build_tools_bin;
   i += strlen(entry);
  }
  // join the strings back
  for (i64 i = 0; i < path_count; i++)
   path[i] = (path[i] == 0) ? ';' : path[i];
  // add %BUILD_TOOLS_BIN% if not present
  string_append(path, has_build_tools_bin ? "" : ";%BUILD_TOOLS_BIN%");
  string_trim_right(path, ";");
  string_trim_left(path, ";");
  {
   WCHAR wpath[MAX_ENV_LEN];
   int n = utf8_to_utf16(path, -1, wpath, countof(wpath));
   RegSetKeyValue(location, subkey, L"Path", REG_EXPAND_SZ, wpath, n * sizeof(WCHAR));
  }
 }
 echoln("Done!");
 if (subprocess_mode)
 {
  WCHAR* wmsg = L"PortableBuildTools was installed successfully.";
  wmsg = (env_mode == 1) ? L"Please log out and back in to finish installation." : wmsg;
  wmsg = (env_mode == 2) ? L"Please reboot the machine to finish installation." : wmsg;
  MessageBox(GetConsoleWindow(), wmsg, L"Success!", MB_TOPMOST | MB_ICONINFORMATION);
 }
 else
 {
  const char* msg = "PortableBuildTools was installed successfully.";
  msg = (env_mode == 1) ? "Please log out and back in to finish installation." : msg;
  msg = (env_mode == 2) ? "Please reboot the machine to finish installation." : msg;
  echoln(msg);
 }
}
