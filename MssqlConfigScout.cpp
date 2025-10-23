/*******************************************************************************
 * MssqlConfigScout - Outil de reconnaissance de configurations SQL Server
 *
 * Développé par: Ayi NEDJIMI Consultants
 * Date: 2025
 *
 * Description:
 *   Se connecte via ODBC et rapporte les configurations serveur SQL Server
 *   (authentication mode, xp_cmdshell, clr enabled, remote access, etc.)
 *
 * AVERTISSEMENT LEGAL:
 *   Cet outil est fourni UNIQUEMENT pour des environnements LAB-CONTROLLED.
 *   L'utilisation sur des systèmes non autorisés est STRICTEMENT INTERDITE.
 *   L'utilisateur assume l'entière responsabilité légale de l'usage de ce logiciel.
 *
 ******************************************************************************/

#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commctrl.h>
#include <sql.h>
#include <sqlext.h>
#include <thread>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <ctime>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "odbc32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// IDs des contrôles
#define IDC_LISTVIEW        1001
#define IDC_EDIT_SERVER     1002
#define IDC_EDIT_DATABASE   1003
#define IDC_EDIT_USER       1004
#define IDC_EDIT_PASSWORD   1005
#define IDC_BTN_CONNECT     1006
#define IDC_BTN_EXPORT      1007
#define IDC_STATUSBAR       1008

// Classe RAII pour handles
template<typename T>
class AutoHandle {
    T handle;
    void(*deleter)(T);
public:
    AutoHandle(T h, void(*d)(T)) : handle(h), deleter(d) {}
    ~AutoHandle() { if (handle) deleter(handle); }
    T get() const { return handle; }
    operator bool() const { return handle != nullptr && handle != INVALID_HANDLE_VALUE; }
};

// Structure pour une configuration SQL
struct SqlConfig {
    std::wstring name;
    std::wstring value;
    std::wstring defaultValue;
    std::wstring runningValue;
    std::wstring description;
};

// Variables globales
HWND g_hMainWindow = nullptr;
HWND g_hListView = nullptr;
HWND g_hEditServer = nullptr;
HWND g_hEditDatabase = nullptr;
HWND g_hEditUser = nullptr;
HWND g_hEditPassword = nullptr;
HWND g_hStatusBar = nullptr;
std::vector<SqlConfig> g_configs;
std::wstring g_logPath;

// Fonction de logging
void LogMessage(const std::wstring& message) {
    std::wofstream logFile(g_logPath, std::ios::app);
    if (logFile.is_open()) {
        time_t now = time(nullptr);
        wchar_t timeStr[64];
        struct tm timeInfo;
        localtime_s(&timeInfo, &now);
        wcsftime(timeStr, sizeof(timeStr) / sizeof(wchar_t), L"%Y-%m-%d %H:%M:%S", &timeInfo);
        logFile << L"[" << timeStr << L"] " << message << std::endl;
    }
}

// Fonction pour obtenir le chemin de log
std::wstring GetLogPath() {
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring logPath = tempPath;
    logPath += L"WinTools_MssqlConfigScout_log.txt";
    return logPath;
}

// Mettre à jour la barre de statut
void UpdateStatus(const std::wstring& status) {
    if (g_hStatusBar) {
        SendMessageW(g_hStatusBar, SB_SETTEXTW, 0, (LPARAM)status.c_str());
    }
    LogMessage(status);
}

// Ajouter une configuration à la ListView
void AddConfigToListView(const SqlConfig& config) {
    LVITEMW lvi = { 0 };
    lvi.mask = LVIF_TEXT;
    lvi.iItem = ListView_GetItemCount(g_hListView);

    // Nom de configuration
    lvi.iSubItem = 0;
    lvi.pszText = const_cast<LPWSTR>(config.name.c_str());
    int index = ListView_InsertItem(g_hListView, &lvi);

    // Valeur
    ListView_SetItemText(g_hListView, index, 1, const_cast<LPWSTR>(config.value.c_str()));

    // Valeur par défaut
    ListView_SetItemText(g_hListView, index, 2, const_cast<LPWSTR>(config.defaultValue.c_str()));

    // Valeur en cours d'exécution
    ListView_SetItemText(g_hListView, index, 3, const_cast<LPWSTR>(config.runningValue.c_str()));

    // Description
    ListView_SetItemText(g_hListView, index, 4, const_cast<LPWSTR>(config.description.c_str()));
}

// Convertir SQLWCHAR* vers std::wstring
std::wstring SqlToWString(SQLWCHAR* sqlStr) {
    if (!sqlStr) return L"";
    return std::wstring(reinterpret_cast<wchar_t*>(sqlStr));
}

// Exécuter une requête SQL et récupérer les configurations
bool ExecuteConfigQuery(SQLHDBC hdbc, const std::wstring& configName) {
    SQLHSTMT hstmt;
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
    if (!SQL_SUCCEEDED(ret)) return false;

    std::wstring query = L"SELECT name, CAST(value AS VARCHAR(100)) AS value, "
                        L"CAST(value_in_use AS VARCHAR(100)) AS value_in_use, "
                        L"CAST(minimum AS VARCHAR(100)) AS minimum, "
                        L"CAST(maximum AS VARCHAR(100)) AS maximum, "
                        L"description FROM sys.configurations WHERE name = ?";

    ret = SQLPrepareW(hstmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) {
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        return false;
    }

    ret = SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_WCHAR, SQL_WVARCHAR,
                          configName.length(), 0, (SQLPOINTER)configName.c_str(),
                          configName.length() * sizeof(WCHAR), nullptr);

    ret = SQLExecute(hstmt);
    if (!SQL_SUCCEEDED(ret)) {
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        return false;
    }

    SQLWCHAR name[256], value[256], valueInUse[256], minimum[256], maximum[256], desc[512];
    SQLLEN nameLen, valueLen, valueInUseLen, minLen, maxLen, descLen;

    SQLBindCol(hstmt, 1, SQL_C_WCHAR, name, sizeof(name), &nameLen);
    SQLBindCol(hstmt, 2, SQL_C_WCHAR, value, sizeof(value), &valueLen);
    SQLBindCol(hstmt, 3, SQL_C_WCHAR, valueInUse, sizeof(valueInUse), &valueInUseLen);
    SQLBindCol(hstmt, 4, SQL_C_WCHAR, minimum, sizeof(minimum), &minLen);
    SQLBindCol(hstmt, 5, SQL_C_WCHAR, maximum, sizeof(maximum), &maxLen);
    SQLBindCol(hstmt, 6, SQL_C_WCHAR, desc, sizeof(desc), &descLen);

    while (SQLFetch(hstmt) == SQL_SUCCESS) {
        SqlConfig config;
        config.name = SqlToWString(name);
        config.value = SqlToWString(value);
        config.defaultValue = SqlToWString(minimum);
        config.runningValue = SqlToWString(valueInUse);
        config.description = SqlToWString(desc);

        g_configs.push_back(config);
        AddConfigToListView(config);
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
    return true;
}

// Vérifier le mode d'authentification
void CheckAuthenticationMode(SQLHDBC hdbc) {
    SQLHSTMT hstmt;
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
    if (!SQL_SUCCEEDED(ret)) return;

    std::wstring query = L"SELECT CASE SERVERPROPERTY('IsIntegratedSecurityOnly') "
                        L"WHEN 1 THEN 'Windows Authentication' "
                        L"WHEN 0 THEN 'Mixed Mode' END AS AuthMode";

    ret = SQLExecDirectW(hstmt, (SQLWCHAR*)query.c_str(), SQL_NTS);
    if (SQL_SUCCEEDED(ret)) {
        SQLWCHAR authMode[256];
        SQLLEN authModeLen;
        SQLBindCol(hstmt, 1, SQL_C_WCHAR, authMode, sizeof(authMode), &authModeLen);

        if (SQLFetch(hstmt) == SQL_SUCCESS) {
            SqlConfig config;
            config.name = L"Authentication Mode";
            config.value = SqlToWString(authMode);
            config.defaultValue = L"N/A";
            config.runningValue = SqlToWString(authMode);
            config.description = L"Mode d'authentification du serveur SQL";

            g_configs.push_back(config);
            AddConfigToListView(config);
        }
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
}

// Thread de connexion et scan
void ConnectAndScanThread() {
    UpdateStatus(L"Connexion au serveur SQL...");

    // Récupérer les paramètres de connexion
    wchar_t server[256], database[256], user[256], password[256];
    GetWindowTextW(g_hEditServer, server, 256);
    GetWindowTextW(g_hEditDatabase, database, 256);
    GetWindowTextW(g_hEditUser, user, 256);
    GetWindowTextW(g_hEditPassword, password, 256);

    // Allouer l'environnement ODBC
    SQLHENV henv;
    SQLRETURN ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
    if (!SQL_SUCCEEDED(ret)) {
        UpdateStatus(L"Erreur: Impossible d'allouer l'environnement ODBC");
        EnableWindow(GetDlgItem(g_hMainWindow, IDC_BTN_CONNECT), TRUE);
        return;
    }

    SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);

    // Allouer la connexion
    SQLHDBC hdbc;
    ret = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
    if (!SQL_SUCCEEDED(ret)) {
        UpdateStatus(L"Erreur: Impossible d'allouer la connexion");
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
        EnableWindow(GetDlgItem(g_hMainWindow, IDC_BTN_CONNECT), TRUE);
        return;
    }

    // Construire la chaîne de connexion
    std::wstring connStr = L"Driver={SQL Server};Server=" + std::wstring(server) +
                          L";Database=" + std::wstring(database);

    if (wcslen(user) > 0) {
        connStr += L";Uid=" + std::wstring(user) + L";Pwd=" + std::wstring(password) + L";";
    } else {
        connStr += L";Trusted_Connection=yes;";
    }

    SQLWCHAR outConnStr[1024];
    SQLSMALLINT outConnStrLen;

    ret = SQLDriverConnectW(hdbc, nullptr, (SQLWCHAR*)connStr.c_str(), SQL_NTS,
                           outConnStr, 1024, &outConnStrLen, SQL_DRIVER_NOPROMPT);

    if (!SQL_SUCCEEDED(ret)) {
        SQLWCHAR sqlState[6], message[256];
        SQLINTEGER nativeError;
        SQLSMALLINT msgLen;
        SQLGetDiagRecW(SQL_HANDLE_DBC, hdbc, 1, sqlState, &nativeError, message, 256, &msgLen);

        std::wstring errMsg = L"Erreur de connexion: " + SqlToWString(message);
        UpdateStatus(errMsg);
        LogMessage(errMsg);

        SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
        EnableWindow(GetDlgItem(g_hMainWindow, IDC_BTN_CONNECT), TRUE);
        return;
    }

    UpdateStatus(L"Connexion réussie. Récupération des configurations...");

    // Nettoyer la liste
    ListView_DeleteAllItems(g_hListView);
    g_configs.clear();

    // Vérifier le mode d'authentification
    CheckAuthenticationMode(hdbc);

    // Requêter les configurations critiques
    std::vector<std::wstring> criticalConfigs = {
        L"xp_cmdshell",
        L"clr enabled",
        L"remote access",
        L"Ole Automation Procedures",
        L"Ad Hoc Distributed Queries",
        L"Database Mail XPs",
        L"SMO and DMO XPs",
        L"SQL Mail XPs",
        L"Agent XPs"
    };

    for (const auto& configName : criticalConfigs) {
        ExecuteConfigQuery(hdbc, configName);
    }

    // Nettoyer
    SQLDisconnect(hdbc);
    SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
    SQLFreeHandle(SQL_HANDLE_ENV, henv);

    std::wstringstream status;
    status << L"Scan terminé. " << g_configs.size() << L" configurations récupérées.";
    UpdateStatus(status.str());

    EnableWindow(GetDlgItem(g_hMainWindow, IDC_BTN_CONNECT), TRUE);
}

// Exporter vers CSV
void ExportToCSV() {
    wchar_t fileName[MAX_PATH] = L"MssqlConfigScout_Export.csv";
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hMainWindow;
    ofn.lpstrFilter = L"Fichiers CSV (*.csv)\0*.csv\0Tous les fichiers (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = L"Exporter les configurations SQL";
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"csv";

    if (GetSaveFileNameW(&ofn)) {
        std::wofstream file(fileName, std::ios::binary);
        if (file.is_open()) {
            // BOM UTF-8
            const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
            file.write(reinterpret_cast<const wchar_t*>(bom), sizeof(bom));

            // Convertir en UTF-8 pour l'export
            std::ofstream csvFile(fileName, std::ios::binary | std::ios::trunc);
            csvFile.write(reinterpret_cast<const char*>(bom), sizeof(bom));

            // Header
            csvFile << "ConfigName,Value,DefaultValue,RunningValue,Description\n";

            // Données
            for (const auto& config : g_configs) {
                std::wstring line = config.name + L"," +
                                   config.value + L"," +
                                   config.defaultValue + L"," +
                                   config.runningValue + L"," +
                                   config.description + L"\n";

                // Conversion UTF-16 vers UTF-8
                int size = WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, nullptr, 0, nullptr, nullptr);
                std::vector<char> buffer(size);
                WideCharToMultiByte(CP_UTF8, 0, line.c_str(), -1, buffer.data(), size, nullptr, nullptr);
                csvFile.write(buffer.data(), size - 1);
            }

            csvFile.close();
            UpdateStatus(L"Export CSV réussi: " + std::wstring(fileName));
            MessageBoxW(g_hMainWindow, L"Export CSV réussi!", L"Succès", MB_OK | MB_ICONINFORMATION);
        } else {
            UpdateStatus(L"Erreur: Impossible de créer le fichier CSV");
            MessageBoxW(g_hMainWindow, L"Erreur lors de l'export!", L"Erreur", MB_OK | MB_ICONERROR);
        }
    }
}

// Initialiser la ListView
void InitListView(HWND hwnd) {
    g_hListView = GetDlgItem(hwnd, IDC_LISTVIEW);

    // Style
    DWORD style = LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER;
    ListView_SetExtendedListViewStyle(g_hListView, style);

    // Colonnes
    LVCOLUMNW lvc = { 0 };
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;

    lvc.cx = 200;
    lvc.pszText = const_cast<LPWSTR>(L"Nom de Configuration");
    ListView_InsertColumn(g_hListView, 0, &lvc);

    lvc.cx = 100;
    lvc.pszText = const_cast<LPWSTR>(L"Valeur");
    ListView_InsertColumn(g_hListView, 1, &lvc);

    lvc.cx = 100;
    lvc.pszText = const_cast<LPWSTR>(L"Valeur par Défaut");
    ListView_InsertColumn(g_hListView, 2, &lvc);

    lvc.cx = 120;
    lvc.pszText = const_cast<LPWSTR>(L"Valeur en Cours");
    ListView_InsertColumn(g_hListView, 3, &lvc);

    lvc.cx = 300;
    lvc.pszText = const_cast<LPWSTR>(L"Description");
    ListView_InsertColumn(g_hListView, 4, &lvc);
}

// Procédure de fenêtre
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Champs de saisie
            CreateWindowExW(0, L"STATIC", L"Serveur:", WS_CHILD | WS_VISIBLE,
                           10, 10, 80, 20, hwnd, nullptr, nullptr, nullptr);
            g_hEditServer = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"localhost",
                                           WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                           100, 10, 200, 20, hwnd, (HMENU)IDC_EDIT_SERVER, nullptr, nullptr);

            CreateWindowExW(0, L"STATIC", L"Base de données:", WS_CHILD | WS_VISIBLE,
                           310, 10, 100, 20, hwnd, nullptr, nullptr, nullptr);
            g_hEditDatabase = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"master",
                                             WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                             420, 10, 200, 20, hwnd, (HMENU)IDC_EDIT_DATABASE, nullptr, nullptr);

            CreateWindowExW(0, L"STATIC", L"Utilisateur:", WS_CHILD | WS_VISIBLE,
                           10, 40, 80, 20, hwnd, nullptr, nullptr, nullptr);
            g_hEditUser = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                         WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                         100, 40, 200, 20, hwnd, (HMENU)IDC_EDIT_USER, nullptr, nullptr);

            CreateWindowExW(0, L"STATIC", L"Mot de passe:", WS_CHILD | WS_VISIBLE,
                           310, 40, 100, 20, hwnd, nullptr, nullptr, nullptr);
            g_hEditPassword = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                             WS_CHILD | WS_VISIBLE | ES_PASSWORD | ES_AUTOHSCROLL,
                                             420, 40, 200, 20, hwnd, (HMENU)IDC_EDIT_PASSWORD, nullptr, nullptr);

            // Boutons
            CreateWindowExW(0, L"BUTTON", L"Connecter et Scanner",
                           WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                           640, 10, 160, 50, hwnd, (HMENU)IDC_BTN_CONNECT, nullptr, nullptr);

            CreateWindowExW(0, L"BUTTON", L"Exporter CSV",
                           WS_CHILD | WS_VISIBLE,
                           820, 10, 120, 50, hwnd, (HMENU)IDC_BTN_EXPORT, nullptr, nullptr);

            // ListView
            CreateWindowExW(0, WC_LISTVIEW, L"",
                           WS_CHILD | WS_VISIBLE | LVS_REPORT | WS_BORDER,
                           10, 80, 930, 480, hwnd, (HMENU)IDC_LISTVIEW, nullptr, nullptr);

            InitListView(hwnd);

            // Barre de statut
            g_hStatusBar = CreateWindowExW(0, STATUSCLASSNAME, nullptr,
                                          WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
                                          0, 0, 0, 0, hwnd, (HMENU)IDC_STATUSBAR, nullptr, nullptr);

            UpdateStatus(L"Prêt. Entrez les informations de connexion SQL.");
            return 0;
        }

        case WM_SIZE: {
            if (g_hStatusBar) {
                SendMessageW(g_hStatusBar, WM_SIZE, 0, 0);
            }
            return 0;
        }

        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case IDC_BTN_CONNECT: {
                    EnableWindow(GetDlgItem(hwnd, IDC_BTN_CONNECT), FALSE);
                    std::thread(ConnectAndScanThread).detach();
                    break;
                }
                case IDC_BTN_EXPORT: {
                    ExportToCSV();
                    break;
                }
            }
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// Point d'entrée
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    // Initialiser le log
    g_logPath = GetLogPath();
    LogMessage(L"=== MssqlConfigScout démarré ===");

    // Initialiser Common Controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(icex);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    // Enregistrer la classe de fenêtre
    WNDCLASSEXW wc = { 0 };
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MssqlConfigScoutClass";
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(nullptr, IDI_APPLICATION);

    RegisterClassExW(&wc);

    // Créer la fenêtre
    g_hMainWindow = CreateWindowExW(
        0,
        L"MssqlConfigScoutClass",
        L"MssqlConfigScout - Reconnaissance Configurations SQL Server - Ayi NEDJIMI Consultants",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 960, 640,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!g_hMainWindow) {
        LogMessage(L"Erreur: Impossible de créer la fenêtre principale");
        return 1;
    }

    ShowWindow(g_hMainWindow, nCmdShow);
    UpdateWindow(g_hMainWindow);

    // Boucle de messages
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    LogMessage(L"=== MssqlConfigScout terminé ===");
    return (int)msg.wParam;
}
