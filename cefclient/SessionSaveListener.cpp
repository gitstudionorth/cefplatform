#include "SessionSaveListener.h"

#include <sstream>
#include <string>

#include "include/cef_dom.h"
#include "cefclient/util.h"
#include "sqlite3.h"
#include "string_util.h"

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

      browser->GetMainFrame()->LoadStringW(result, "about:blank");

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

void CreateProcessMessageDelegates(
    ClientHandler::ProcessMessageDelegateSet& delegates) {
  delegates.insert(new ProcessMessageDelegate);
}

void RunTest(CefRefPtr<CefBrowser> browser) {
  // Load the test URL.
  browser->GetMainFrame()->LoadURL(kTestUrl);
}

}  // namespace binding_test
