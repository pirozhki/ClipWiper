#include <atlbase.h>
#include <atlwin.h>
#include <shellapi.h>
#include <string>
#include "resource.h"

// タスクトレイ用のカスタムメッセージ
const UINT WM_TRAYICON = WM_USER + 1;

// ATLウィンドウモジュールの初期化
CAtlWinModule _AtlWinModule;

class CMainDialog : public ATL::CDialogImpl<CMainDialog>
{
public:
    enum { IDD = IDD_MAINDIALOG };

    BEGIN_MSG_MAP(CMainDialog)
        MESSAGE_HANDLER(WM_INITDIALOG, OnInitDialog)
        MESSAGE_HANDLER(WM_TRAYICON, OnTrayIcon)
        MESSAGE_HANDLER(WM_TIMER, OnTimer)
        COMMAND_HANDLER(IDC_APPLY_BTN, BN_CLICKED, OnApplySeconds)
        COMMAND_HANDLER(ID_MENU_SHOW, 0, OnMenuShow)
        COMMAND_HANDLER(ID_MENU_CLEAR, 0, OnMenuClear)
        COMMAND_HANDLER(ID_MENU_EXIT, 0, OnMenuExit)
        COMMAND_ID_HANDLER(IDCANCEL, OnCancel)
    END_MSG_MAP()

private:
    NOTIFYICONDATA m_nid = { 0 };
    DWORD m_lastSequence = 0;
    int m_elapsedSeconds = 0;
    int m_autoClearMinutes = 5; // デフォルト5分
    bool m_hasData = false;

    // 実行ファイルと同じフォルダにある INI ファイルのフルパスを取得
    std::wstring GetIniFilePath() {
        wchar_t path[MAX_PATH] = { 0 };
        ::GetModuleFileName(NULL, path, MAX_PATH);
        std::wstring iniPath = path;
        size_t pos = iniPath.find_last_of(L"\\/");
        if (pos != std::wstring::npos) {
            iniPath = iniPath.substr(0, pos + 1) + L"ClipWiper.ini";
        }
        return iniPath;
    }

    // INI ファイルから設定を読み込み
    void LoadSettings() {
        std::wstring iniPath = GetIniFilePath();
        // セクション: [Settings], キー: AutoClearMinutes, デフォルト値: 5
        m_autoClearMinutes = ::GetPrivateProfileInt(L"Settings", L"AutoClearMinutes", 5, iniPath.c_str());
    }

    // INI ファイルへ設定を保存
    void SaveSettings() {
        std::wstring iniPath = GetIniFilePath();
        wchar_t valStr[16] = { 0 };
        swprintf_s(valStr, L"%d", m_autoClearMinutes);
        ::WritePrivateProfileString(L"Settings", L"AutoClearMinutes", valStr, iniPath.c_str());
    }

    // クリップボード消去ロジック
    void ClearClipboard() {
        if (::OpenClipboard(m_hWnd)) {
            ::EmptyClipboard();
            ::CloseClipboard();
        }
        m_hasData = false;
        m_elapsedSeconds = 0;
    }

    // ダイアログ起動時の処理
    LRESULT OnInitDialog(UINT, WPARAM, LPARAM, BOOL&) {
        CenterWindow();

        // INIファイルからの設定読み込み
        LoadSettings();

        // エディットボックスに読み込んだ値をセット
        SetDlgItemInt(IDC_MIN_EDIT, m_autoClearMinutes, FALSE);

        // タスクトレイアイコンの設定
        m_nid.cbSize = sizeof(NOTIFYICONDATA);
        m_nid.hWnd = m_hWnd;
        m_nid.uID = 1;
        m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        m_nid.uCallbackMessage = WM_TRAYICON;

        HICON hIcon = ::LoadIcon(_AtlBaseModule.GetResourceInstance(), MAKEINTRESOURCE(IDI_APPICON));
        if (!hIcon) {
            hIcon = ::LoadIcon(NULL, IDI_SHIELD);
        }
        m_nid.hIcon = hIcon;
        lstrcpy(m_nid.szTip, L"ClipWiper (クリップボード自動消去)");
        ::Shell_NotifyIcon(NIM_ADD, &m_nid);

        // タイマー始動
        m_lastSequence = ::GetClipboardSequenceNumber();
        SetTimer(1, 1000, NULL);

        return TRUE;
    }

    // 1秒ごとのタイマー処理
    LRESULT OnTimer(UINT, WPARAM, LPARAM, BOOL&) {
        DWORD currentSequence = ::GetClipboardSequenceNumber();
        if (currentSequence != m_lastSequence) {
            m_lastSequence = currentSequence;
            if (::CountClipboardFormats() > 0) {
                m_elapsedSeconds = 0;
                m_hasData = true;
            }
            else {
                m_hasData = false;
            }
        }

        if (m_hasData) {
            m_elapsedSeconds++;
            if (m_elapsedSeconds >= m_autoClearMinutes * 60) {
                ClearClipboard();
            }
        }
        return 0;
    }

    // タスクトレイのクリックイベント
    LRESULT OnTrayIcon(UINT, WPARAM, LPARAM lp, BOOL&) {
        if (lp == WM_LBUTTONDBLCLK) {
            ShowWindow(SW_SHOWNORMAL);
            ::SetForegroundWindow(m_hWnd);
        }
        else if (lp == WM_RBUTTONUP) {
            POINT pt;
            ::GetCursorPos(&pt);
            HMENU hMenu = ::CreatePopupMenu();
            ::AppendMenu(hMenu, MF_STRING, ID_MENU_SHOW, L"設定を開く");
            ::AppendMenu(hMenu, MF_STRING, ID_MENU_CLEAR, L"今すぐクリア");
            ::AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
            ::AppendMenu(hMenu, MF_STRING, ID_MENU_EXIT, L"終了");

            ::SetForegroundWindow(m_hWnd);
            ::TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hWnd, NULL);
            ::DestroyMenu(hMenu);
        }
        return 0;
    }

    // 時間の変更ボタンが押されたとき
    LRESULT OnApplySeconds(WORD, WORD, HWND, BOOL&) {
        BOOL translated = FALSE;
        UINT minVal = GetDlgItemInt(IDC_MIN_EDIT, &translated, FALSE);
        if (translated && minVal > 0) {
            m_autoClearMinutes = minVal;
            m_elapsedSeconds = 0;

            // INIファイルへ保存
            SaveSettings();

            ShowWindow(SW_HIDE);
        }
        return 0;
    }

    LRESULT OnMenuShow(WORD, WORD, HWND, BOOL&) {
        ShowWindow(SW_SHOWNORMAL);
        return 0;
    }

    LRESULT OnMenuClear(WORD, WORD, HWND, BOOL&) {
        ClearClipboard();
        return 0;
    }

    LRESULT OnMenuExit(WORD, WORD, HWND, BOOL&) {
        KillTimer(1);
        ::Shell_NotifyIcon(NIM_DELETE, &m_nid);
        EndDialog(0);
        ::PostQuitMessage(0);
        return 0;
    }

    LRESULT OnCancel(WORD, WORD, HWND, BOOL&) {
        ShowWindow(SW_HIDE);
        return 0;
    }
};

// --- WinMain（エントリーポイント） ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    CMainDialog dlg;
    if (dlg.Create(NULL) == NULL) {
        return 0;
    }

    MSG msg;
    while (::GetMessage(&msg, NULL, 0, 0)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}