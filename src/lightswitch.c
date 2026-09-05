#define UNICODE
#define _UNICODE
#include <windows.h>

typedef HRESULT (WINAPI *DwmSetWindowAttributeFn)(HWND, DWORD, LPCVOID, DWORD);

#define WINDOW_WIDTH  320
#define WINDOW_HEIGHT 112
#define TOGGLE_ID     1001

static BOOL g_dark = FALSE;
static const WCHAR g_key[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";

typedef struct {
    const WCHAR *title, *heading, *label, *on, *off, *error;
} Strings;

static const Strings languages[] = {
    {L"Lightswitch", L"Windows appearance", L"Dark mode", L"On", L"Off", L"Windows could not update your theme settings."},
    {L"Dunkelmodus", L"Windows-Darstellung", L"Dunkelmodus", L"Ein", L"Aus", L"Windows konnte die Designeinstellungen nicht ändern."},
    {L"Mode sombre", L"Apparence de Windows", L"Mode sombre", L"Activé", L"Désactivé", L"Windows n’a pas pu modifier les paramètres du thème."},
    {L"Modo oscuro", L"Apariencia de Windows", L"Modo oscuro", L"Activado", L"Desactivado", L"Windows no pudo cambiar la configuración del tema."},
    {L"Modalità scura", L"Aspetto di Windows", L"Modalità scura", L"Attiva", L"Disattiva", L"Windows non ha potuto modificare le impostazioni del tema."},
    {L"Modo escuro", L"Aparência do Windows", L"Modo escuro", L"Ativado", L"Desativado", L"O Windows não pôde alterar as configurações do tema."},
    {L"Donkere modus", L"Windows-weergave", L"Donkere modus", L"Aan", L"Uit", L"Windows kon de thema-instellingen niet wijzigen."},
    {L"Tryb ciemny", L"Wygląd systemu Windows", L"Tryb ciemny", L"Wł.", L"Wył.", L"System Windows nie mógł zmienić ustawień motywu."},
    {L"Тёмный режим", L"Оформление Windows", L"Тёмный режим", L"Вкл.", L"Выкл.", L"Windows не удалось изменить параметры темы."},
    {L"ダーク モード", L"Windows の外観", L"ダーク モード", L"オン", L"オフ", L"Windows でテーマ設定を変更できませんでした。"},
    {L"다크 모드", L"Windows 모양", L"다크 모드", L"켜짐", L"꺼짐", L"Windows에서 테마 설정을 변경하지 못했습니다."},
    {L"深色模式", L"Windows 外观", L"深色模式", L"开", L"关", L"Windows 无法更改主题设置。"},
    {L"深色模式", L"Windows 外觀", L"深色模式", L"開", L"關", L"Windows 無法變更佈景主題設定。"},
    {L"Koyu Mod", L"Windows görünümü", L"Koyu mod", L"Açık", L"Kapalı", L"Windows tema ayarlarını değiştiremedi."}
};
static const Strings *g_text = &languages[0];

static void update_title_bar(HWND hwnd) {
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    if (!dwm) return;
    DwmSetWindowAttributeFn setAttribute =
        (DwmSetWindowAttributeFn)GetProcAddress(dwm, "DwmSetWindowAttribute");
    if (setAttribute) {
        BOOL enabled = g_dark;
        /* Windows 10 20H1+ and Windows 11. */
        if (setAttribute(hwnd, 20, &enabled, sizeof(enabled)) < 0) {
            /* Older supported Windows 10 builds used attribute 19. */
            setAttribute(hwnd, 19, &enabled, sizeof(enabled));
        }
    }
    FreeLibrary(dwm);
}

static void select_language(void) {
    LANGID id = GetUserDefaultUILanguage();
    switch (PRIMARYLANGID(id)) {
    case LANG_GERMAN:     g_text = &languages[1]; break;
    case LANG_FRENCH:     g_text = &languages[2]; break;
    case LANG_SPANISH:    g_text = &languages[3]; break;
    case LANG_ITALIAN:    g_text = &languages[4]; break;
    case LANG_PORTUGUESE: g_text = &languages[5]; break;
    case LANG_DUTCH:      g_text = &languages[6]; break;
    case LANG_POLISH:     g_text = &languages[7]; break;
    case LANG_RUSSIAN:    g_text = &languages[8]; break;
    case LANG_JAPANESE:   g_text = &languages[9]; break;
    case LANG_KOREAN:     g_text = &languages[10]; break;
    case LANG_CHINESE:
        g_text = (SUBLANGID(id) == SUBLANG_CHINESE_TRADITIONAL ||
                  SUBLANGID(id) == SUBLANG_CHINESE_HONGKONG ||
                  SUBLANGID(id) == SUBLANG_CHINESE_MACAU) ? &languages[12] : &languages[11];
        break;
    case LANG_TURKISH:    g_text = &languages[13]; break;
    default:              g_text = &languages[0]; break;
    }
}

static BOOL read_dark_mode(void) {
    HKEY key;
    DWORD apps = 1, system = 1, size = sizeof(DWORD), type = 0;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, g_key, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return FALSE;
    RegQueryValueExW(key, L"AppsUseLightTheme", 0, &type, (BYTE*)&apps, &size);
    size = sizeof(DWORD);
    RegQueryValueExW(key, L"SystemUsesLightTheme", 0, &type, (BYTE*)&system, &size);
    RegCloseKey(key);
    return apps == 0 && system == 0;
}

static BOOL write_dark_mode(BOOL dark) {
    HKEY key;
    DWORD disposition, value = dark ? 0 : 1;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, g_key, 0, NULL, 0, KEY_SET_VALUE,
                        NULL, &key, &disposition) != ERROR_SUCCESS)
        return FALSE;
    LONG a = RegSetValueExW(key, L"AppsUseLightTheme", 0, REG_DWORD,
                            (const BYTE*)&value, sizeof(value));
    LONG b = RegSetValueExW(key, L"SystemUsesLightTheme", 0, REG_DWORD,
                            (const BYTE*)&value, sizeof(value));
    RegCloseKey(key);
    DWORD_PTR ignored;
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
                        (LPARAM)L"ImmersiveColorSet", SMTO_ABORTIFHUNG, 1000, &ignored);
    return a == ERROR_SUCCESS && b == ERROR_SUCCESS;
}

static HFONT make_font(int height, int weight) {
    return CreateFontW(height, 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

static void draw_smooth_toggle(HDC dc, int x, int y) {
    const int scale = 4, width = 50, height = 28, thumb = 22, inset = 3;
    HDC hi = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateCompatibleBitmap(dc, width * scale, height * scale);
    HGDIOBJ oldBitmap = SelectObject(hi, bitmap);
    RECT surface = { 0, 0, width * scale, height * scale };
    HBRUSH surfaceBrush = CreateSolidBrush(g_dark ? RGB(32, 32, 34) : RGB(245, 245, 247));
    FillRect(hi, &surface, surfaceBrush);
    HBRUSH background = CreateSolidBrush(g_dark ? RGB(10, 132, 255) : RGB(174, 174, 178));
    HPEN noPen = CreatePen(PS_NULL, 0, 0);
    HGDIOBJ oldBrush = SelectObject(hi, background);
    HGDIOBJ oldPen = SelectObject(hi, noPen);
    RoundRect(hi, 0, 0, width * scale, height * scale, height * scale, height * scale);

    int thumbX = (g_dark ? width - inset - thumb : inset) * scale;
    HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
    SelectObject(hi, white);
    Ellipse(hi, thumbX, inset * scale, thumbX + thumb * scale, (inset + thumb) * scale);

    SetStretchBltMode(dc, HALFTONE);
    SetBrushOrgEx(dc, 0, 0, NULL);
    StretchBlt(dc, x, y, width, height, hi, 0, 0, width * scale, height * scale, SRCCOPY);
    SelectObject(hi, oldBrush);
    SelectObject(hi, oldPen);
    SelectObject(hi, oldBitmap);
    DeleteObject(background);
    DeleteObject(surfaceBrush);
    DeleteObject(white);
    DeleteObject(noPen);
    DeleteObject(bitmap);
    DeleteDC(hi);
}

static void paint_window(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    RECT client;
    GetClientRect(hwnd, &client);

    COLORREF background = g_dark ? RGB(32, 32, 34) : RGB(245, 245, 247);
    COLORREF primary = g_dark ? RGB(245, 245, 247) : RGB(29, 29, 31);
    COLORREF secondary = g_dark ? RGB(174, 174, 178) : RGB(110, 110, 115);
    HBRUSH bg = CreateSolidBrush(background);
    FillRect(dc, &client, bg);
    DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);

    HFONT labelFont = make_font(-17, FW_MEDIUM);
    HFONT statusFont = make_font(-14, FW_NORMAL);
    HFONT oldFont = (HFONT)SelectObject(dc, labelFont);
    SetTextColor(dc, primary);
    RECT label = { 24, 27, 220, 52 };
    DrawTextW(dc, g_text->label, -1, &label, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    SelectObject(dc, statusFont);
    SetTextColor(dc, secondary);
    RECT status = { 24, 53, 220, 77 };
    DrawTextW(dc, g_dark ? g_text->on : g_text->off, -1, &status, DT_LEFT | DT_SINGLELINE);

    draw_smooth_toggle(dc, 246, 33);
    SelectObject(dc, oldFont);
    DeleteObject(labelFont);
    DeleteObject(statusFont);
    EndPaint(hwnd, &ps);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_PAINT:
        paint_window(hwnd);
        return 0;
    case WM_LBUTTONUP: {
        int x = (short)LOWORD(lparam), y = (short)HIWORD(lparam);
        if (x >= 235 && x <= 305 && y >= 22 && y <= 74) {
            BOOL next = !g_dark;
            if (write_dark_mode(next)) {
                g_dark = next;
                update_title_bar(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            } else {
                MessageBoxW(hwnd, g_text->error, L"Lightswitch", MB_OK | MB_ICONERROR);
            }
        }
        return 0;
    }
    case WM_SETCURSOR: {
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(hwnd, &p);
        if (p.x >= 235 && p.x <= 305 && p.y >= 22 && p.y <= 74) {
            SetCursor(LoadCursorW(NULL, IDC_HAND));
            return TRUE;
        }
        break;
    }
    case WM_KEYUP:
        if (wparam == VK_SPACE || wparam == VK_RETURN) {
            BOOL next = !g_dark;
            if (write_dark_mode(next)) {
                g_dark = next;
                update_title_bar(hwnd);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

static int run_app(void) {
    HINSTANCE instance = GetModuleHandleW(NULL);
    SetProcessDPIAware();
    select_language();
    const WCHAR className[] = L"WindowsDarkModeToggleWindow";
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    if (!wc.hIcon) wc.hIcon = LoadIconW(NULL, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    wc.lpszClassName = className;
    if (!RegisterClassExW(&wc)) return 1;

    g_dark = read_dark_mode();
    RECT rect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    AdjustWindowRect(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    int width = rect.right - rect.left, height = rect.bottom - rect.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
    HWND hwnd = CreateWindowExW(0, className, L"Lightswitch",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, width, height, NULL, NULL, instance, NULL);
    if (!hwnd) return 2;
    update_title_bar(hwnd);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

void WinMainCRTStartup(void) {
    ExitProcess((UINT)run_app());
}
