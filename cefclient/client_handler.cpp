// Copyright (c) 2011 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/client_handler.h"
#include <algorithm>
#include <stdio.h>
#include <direct.h>
#include <sstream>
#include <fstream>
#include <string>
#include "include/cef_browser.h"
#include "include/cef_frame.h"
#include "include/cef_path_util.h"
#include "include/cef_process_util.h"
#include "include/cef_runnable.h"
#include "include/wrapper/cef_stream_resource_handler.h"
#include "cefclient/cefclient.h"
#include "cefclient/client_renderer.h"
#include "cefclient/client_switches.h"
#include "cefclient/resource_util.h"
#include "cefclient/string_util.h"
#include "sqlite3.h"
#include "SessionSaveListener.h"


ClientHandler::ClientHandler()
  : m_MainHwnd(NULL),
    m_BrowserId(0),
    m_EditHwnd(NULL),
    m_BackHwnd(NULL),
    m_ForwardHwnd(NULL),
    m_StopHwnd(NULL),
    m_ReloadHwnd(NULL),
    m_bFocusOnEditableField(false) {
  CreateProcessMessageDelegates(process_message_delegates_);
  CreateRequestDelegates(request_delegates_);

  // Read command line settings.
  CefRefPtr<CefCommandLine> command_line =
      CefCommandLine::GetGlobalCommandLine();

  if (command_line->HasSwitch(cefclient::kUrl))
    m_StartupURL = command_line->GetSwitchValue(cefclient::kUrl);
  if (m_StartupURL.empty())
    m_StartupURL = "http://sessions/";

  m_bExternalDevTools = command_line->HasSwitch(cefclient::kExternalDevTools);
}

ClientHandler::~ClientHandler() {
}

bool ClientHandler::OnProcessMessageReceived(
    CefRefPtr<CefBrowser> browser,
    CefProcessId source_process,
    CefRefPtr<CefProcessMessage> message) {
  // Check for messages from the client renderer.
  std::string message_name = message->GetName();
  if (message_name == client_renderer::kFocusedNodeChangedMessage) {
    // A message is sent from ClientRenderDelegate to tell us whether the
    // currently focused DOM node is editable. Use of |m_bFocusOnEditableField|
    // is redundant with CefKeyEvent.focus_on_editable_field in OnPreKeyEvent
    // but is useful for demonstration purposes.
    m_bFocusOnEditableField = message->GetArgumentList()->GetBool(0);
    return true;
  }

  bool handled = false;

  // Execute delegate callbacks.
  ProcessMessageDelegateSet::iterator it = process_message_delegates_.begin();
  for (; it != process_message_delegates_.end() && !handled; ++it) {
    handled = (*it)->OnProcessMessageReceived(this, browser, source_process,
                                              message);
  }

  return handled;
}

void ClientHandler::OnLoadingStateChange(CefRefPtr<CefBrowser> browser,
                                         bool isLoading,
                                         bool canGoBack,
                                         bool canGoForward) {
  REQUIRE_UI_THREAD();
  SetLoading(isLoading);
  SetNavState(canGoBack, canGoForward);
}

bool ClientHandler::OnConsoleMessage(CefRefPtr<CefBrowser> browser,
                                     const CefString& message,
                                     const CefString& source,
                                     int line) {
  REQUIRE_UI_THREAD();

  bool first_message;
  std::string logFile;

  {
    AutoLock lock_scope(this);

    first_message = m_LogFile.empty();
    if (first_message) {
      std::stringstream ss;
      ss << AppGetWorkingDirectory();
#if defined(OS_WIN)
      ss << "\\";
#else
      ss << "/";
#endif
      ss << "console.log";
      m_LogFile = ss.str();
    }
    logFile = m_LogFile;
  }

  FILE* file = fopen(logFile.c_str(), "a");
  if (file) {
    std::stringstream ss;
    ss << "Message: " << std::string(message) << "\r\nSource: " <<
        std::string(source) << "\r\nLine: " << line <<
        "\r\n-----------------------\r\n";
    fputs(ss.str().c_str(), file);
    fclose(file);

    if (first_message)
      SendNotification(NOTIFY_CONSOLE_MESSAGE);
  }

  return false;
}

void ClientHandler::OnBeforeDownload(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDownloadItem> download_item,
    const CefString& suggested_name,
    CefRefPtr<CefBeforeDownloadCallback> callback) {
  REQUIRE_UI_THREAD();
  // Continue the download and show the "Save As" dialog.
  callback->Continue(GetDownloadPath(suggested_name), true);
}

void ClientHandler::OnDownloadUpdated(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefDownloadItem> download_item,
    CefRefPtr<CefDownloadItemCallback> callback) {
  REQUIRE_UI_THREAD();
  if (download_item->IsComplete()) {
    SetLastDownloadFile(download_item->GetFullPath());
    SendNotification(NOTIFY_DOWNLOAD_COMPLETE);
  }
}

void ClientHandler::OnRequestGeolocationPermission(
      CefRefPtr<CefBrowser> browser,
      const CefString& requesting_url,
      int request_id,
      CefRefPtr<CefGeolocationCallback> callback) {
  // Allow geolocation access from all websites.
  callback->Continue(true);
}

bool ClientHandler::OnPreKeyEvent(CefRefPtr<CefBrowser> browser,
                                  const CefKeyEvent& event,
                                  CefEventHandle os_event,
                                  bool* is_keyboard_shortcut) {
  ASSERT(m_bFocusOnEditableField == event.focus_on_editable_field);
  if (!event.focus_on_editable_field && event.windows_key_code == 0x20) {
    // Special handling for the space character when an input element does not
    // have focus. Handling the event in OnPreKeyEvent() keeps the event from
    // being processed in the renderer. If we instead handled the event in the
    // OnKeyEvent() method the space key would cause the window to scroll in
    // addition to showing the alert box.
    if (event.type == KEYEVENT_RAWKEYDOWN) {
    }
    return true;
  }

  return false;
}

void ClientHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser) {
  REQUIRE_UI_THREAD();

  AutoLock lock_scope(this);
  if (!m_Browser.get())   {
    // We need to keep the main child window, but not popup windows
    m_Browser = browser;
    m_BrowserId = browser->GetIdentifier();
  }
}

bool ClientHandler::DoClose(CefRefPtr<CefBrowser> browser) {
  REQUIRE_UI_THREAD();

  if (m_BrowserId == browser->GetIdentifier()) {
    // Since the main window contains the browser window, we need to close
    // the parent window instead of the browser window.
    CloseMainWindow();

    // Return true here so that we can skip closing the browser window
    // in this pass. (It will be destroyed due to the call to close
    // the parent above.)
    return true;
  }

  // A popup browser window is not contained in another window, so we can let
  // these windows close by themselves.
  return false;
}

void ClientHandler::OnBeforeClose(CefRefPtr<CefBrowser> browser) {
  REQUIRE_UI_THREAD();

  if (m_BrowserId == browser->GetIdentifier()) {
    // Free the browser pointer so that the browser can be destroyed
    m_Browser = NULL;
  } else if (browser->IsPopup()) {
    // Remove the record for DevTools popup windows.
    std::set<std::string>::iterator it =
        m_OpenDevToolsURLs.find(browser->GetMainFrame()->GetURL());
    if (it != m_OpenDevToolsURLs.end())
      m_OpenDevToolsURLs.erase(it);
  }
}

void ClientHandler::OnLoadStart(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame) {
  REQUIRE_UI_THREAD();

  if (m_BrowserId == browser->GetIdentifier() && frame->IsMain()) {
    // We've just started loading a page
    SetLoading(true);
  }
}

void ClientHandler::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              int httpStatusCode) {
  REQUIRE_UI_THREAD();

  if (m_BrowserId == browser->GetIdentifier() && frame->IsMain()) {
    // We've just finished loading a page
    SetLoading(false);
  }
}

void ClientHandler::OnLoadError(CefRefPtr<CefBrowser> browser,
                                CefRefPtr<CefFrame> frame,
                                ErrorCode errorCode,
                                const CefString& errorText,
                                const CefString& failedUrl) {
  REQUIRE_UI_THREAD();

  // Don't display an error for downloaded files.
  if (errorCode == ERR_ABORTED)
    return;

  // Don't display an error for external protocols that we allow the OS to
  // handle. See OnProtocolExecution().
  if (errorCode == ERR_UNKNOWN_URL_SCHEME) {
    std::string urlStr = frame->GetURL();
    if (urlStr.find("spotify:") == 0)
      return;
  }

  // Display a load error message.
  std::stringstream ss;
  ss << "<html><body><h2>Failed " << std::string(failedUrl) <<
        " with error " << std::string(errorText) << " (" << errorCode <<
        ").</h2></body></html>";
  frame->LoadString(ss.str(), failedUrl);
}

void ClientHandler::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                              TerminationStatus status) {
  // Load the startup URL if that's not the website that we terminated on.
  CefRefPtr<CefFrame> frame = browser->GetMainFrame();
  std::string url = frame->GetURL();
  std::transform(url.begin(), url.end(), url.begin(), tolower);

  std::string startupURL = GetStartupURL();
  if (url.find(startupURL) != 0)
    frame->LoadURL(startupURL);
}

std::string sessions;
static int callback(void *NotUsed, int argc, char **argv, char **azColName){
    int i;
    sessions.append("{");
    for(i=0; i<argc; i++) {        
        sessions.append("'");
        sessions.append(azColName[i]);
        sessions.append("'");
        sessions.append(": ");
        sessions.append("'");
        sessions.append(argv[i]);
        sessions.append("',");
    }
    sessions.append("},");

    return 0;
}

CefRefPtr<CefResourceHandler> ClientHandler::GetResourceHandler(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefRequest> request) {

    char szWorkingDir[MAX_PATH];  // The current working directory
    // Retrieve the current working directory.
    if (_getcwd(szWorkingDir, MAX_PATH) == NULL)
        szWorkingDir[0] = 0;

    std::string path;

    path.append(szWorkingDir);
    path.append("\\sessions.html");

    CefRefPtr<CefStreamReader> stream = CefStreamReader::CreateForFile(path.c_str());
    ASSERT(stream.get());  

    // read all of the stream data into a std::string.
    std::stringstream ss;
    char buff[100];
    size_t read;
    do {
        read = stream->Read(buff, sizeof(char), 100-1);
        if(read > 0) {
            buff[read] = 0;
            ss << buff;
        }
    }
    while(read > 0);

    sqlite3 *db; // sqlite3 db struct
    char *zErrMsg = 0;
    char *szSQL;
    int rc;

    // Open db
    rc = sqlite3_open("precedex_calc.sqlite", &db);

    if( rc ) {
        // failed
        // fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
    } else {
        // success
        // fprintf(stderr, "Open database successfully\n");
    }

    // create myTable
    // szSQL = "create table myTable (FirstName varchar(30), LastName varchar(30), Age smallint not null)";

    // rc = sqlite3_exec(db, szSQL, callback, 0, &zErrMsg);

    if( rc == SQLITE_OK ) {
        // insert 1 record into myTable
        // RunInsertParamSQL(db, "askyb", "com", 10);

        // fetch records
        szSQL = "select * from sessions";
        rc = sqlite3_exec(db, szSQL, callback, 0, &zErrMsg);
    }

    // Close test.db file
    sqlite3_close(db);

    std::string sessiondata = ss.str();
    std::string placeholder = "'placeholder'";
    
    sessiondata.replace(sessiondata.find(placeholder), placeholder.length(), sessions);

    browser->GetMainFrame()->LoadStringW(sessiondata, "http://sessions/");

    CefRefPtr<CefResourceHandler> handler;
    // Execute delegate callbacks.
    RequestDelegateSet::iterator it = request_delegates_.begin();

    for (; it != request_delegates_.end() && !handler.get(); ++it)
        handler = (*it)->GetResourceHandler(this, browser, frame, request);

    return handler;
}

// Insert record
void RunInsertParamSQL(sqlite3 *db, char *fn, char *ln, int age) {
  if (!db)
    return;

  char *zErrMsg = 0;
  sqlite3_stmt *stmt;
  const char *pzTest;
  char *szSQL;

  // Insert data item into myTable
  szSQL = "insert into myTable (FirstName, LastName, Age) values (?,?,?)";

  int rc = sqlite3_prepare(db, szSQL, strlen(szSQL), &stmt, &pzTest);

  if( rc == SQLITE_OK ) {
    // bind the value 
    sqlite3_bind_text(stmt, 1, fn, strlen(fn), 0);
    sqlite3_bind_text(stmt, 2, ln, strlen(ln), 0);
    sqlite3_bind_int(stmt, 3, age);

    // commit 
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
  }
}

void ClientHandler::OnProtocolExecution(CefRefPtr<CefBrowser> browser,
                                        const CefString& url,
                                        bool& allow_os_execution) {
  std::string urlStr = url;

  // Allow OS execution of Spotify URIs.
  if (urlStr.find("spotify:") == 0)
    allow_os_execution = true;
}

void ClientHandler::SetMainHwnd(CefWindowHandle hwnd) {
  AutoLock lock_scope(this);
  m_MainHwnd = hwnd;
}

void ClientHandler::SetEditHwnd(CefWindowHandle hwnd) {
  AutoLock lock_scope(this);
  m_EditHwnd = hwnd;
}

void ClientHandler::SetButtonHwnds(CefWindowHandle backHwnd,
                                   CefWindowHandle forwardHwnd,
                                   CefWindowHandle reloadHwnd,
                                   CefWindowHandle stopHwnd) {
  AutoLock lock_scope(this);
  m_BackHwnd = backHwnd;
  m_ForwardHwnd = forwardHwnd;
  m_ReloadHwnd = reloadHwnd;
  m_StopHwnd = stopHwnd;
}

std::string ClientHandler::GetLogFile() {
  AutoLock lock_scope(this);
  return m_LogFile;
}

void ClientHandler::SetLastDownloadFile(const std::string& fileName) {
  AutoLock lock_scope(this);
  m_LastDownloadFile = fileName;
}

std::string ClientHandler::GetLastDownloadFile() {
  AutoLock lock_scope(this);
  return m_LastDownloadFile;
}


// static
void ClientHandler::CreateProcessMessageDelegates(
      ProcessMessageDelegateSet& delegates) {
    // Create the binding test delegates.
    binding_test::CreateProcessMessageDelegates(delegates);
}

// static
void ClientHandler::CreateRequestDelegates(RequestDelegateSet& delegates) {
  // Create the binding test delegates.
}
