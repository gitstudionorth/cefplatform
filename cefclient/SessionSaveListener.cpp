#include "SessionSaveListener.h"

#include <sstream>
#include <string>

#include "include/cef_dom.h"
#include "cefclient/util.h"
#include <direct.h>
#include "sqlite3.h"
#include "string_util.h"
#include "repository.h"

namespace binding_test {

namespace {

const char* kTestUrl = "http://sessions";
const char* kMessageName = "binding_test";

// Handle messages in the browser process.
class ProcessMessageDelegate : public ClientHandler::ProcessMessageDelegate {
 public:
  ProcessMessageDelegate() {
  }

  // From ClientHandler::ProcessMessageDelegate.
  virtual bool OnProcessMessageReceived(
      CefRefPtr<ClientHandler> handler,
      CefRefPtr<CefBrowser> browser,
      CefProcessId source_process,
      CefRefPtr<CefProcessMessage> message) OVERRIDE {
    std::string message_name = message->GetName();

    if (message_name == "delete") {
        //args->GetString(0);
    }
    else if (message_name == "create") {
    }

    if (message_name == kMessageName) {
      // Handle the message.
      std::string result;

      CefRefPtr<CefListValue> args = message->GetArgumentList();
      if (args->GetSize() > 0 && args->GetType(0) == VTYPE_STRING) {
        // Our result is a reverse of the original message.
        result = args->GetString(0);
        std::reverse(result.begin(), result.end());
      } else {
        result = "Invalid request";
      }

      storage::insertSession("test 1", "active ");
      //LoadSessions(browser);

      //browser->GetMainFrame()->LoadStringW(result, "about:blank");

      // Send the result back to the render process.
      //CefRefPtr<CefProcessMessage> response =
        //  CefProcessMessage::Create(kMessageName);
      //response->GetArgumentList()->SetString(0, result);
     // browser->SendProcessMessage(PID_RENDERER, response);

      return true;
    }

    return false;
  }

  IMPLEMENT_REFCOUNTING(ProcessMessageDelegate);
};

}  // namespace

void LoadSessions(CefRefPtr<CefBrowser> browser) {
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

    std::string sessiondata = ss.str();
    std::string placeholder = "\"placeholder\"";

    std::string sessions = storage::getSessions();
    
    size_t found = sessiondata.find(placeholder);
    if (found != std::string::npos)
        sessiondata.replace(sessiondata.find(placeholder), placeholder.length(), sessions);

    browser->GetMainFrame()->LoadStringW(sessiondata, "http://sessions/");
}

void CreateProcessMessageDelegates(
    ClientHandler::ProcessMessageDelegateSet& delegates) {
  delegates.insert(new ProcessMessageDelegate);
}

}  // namespace binding_test
