/*
   ESP32 ‑ Multi‑RGB‑LED web controller (digital ON/OFF, no PWM)
   ▸ Works from HTTPS pages (GitHub Pages, Chrome ≥ 119, Brave…)
   ▸ Common‑cathode by default – uncomment COMMON_ANODE for common‑anode
   ▸ URL format:  /set?L=<index>&R=0|1&G=0|1&B=0|1
                   /blink         (cycles all LEDs)
                   OPTIONS …      (handled for CORS / Private‑Network)
*/

//#define COMMON_ANODE                      // <-- UNCOMMENT for common‑anode LEDs

#include <Arduino.h>
#include <WiFi.h>

/* ---------- Wi‑Fi credentials ---------- */
const char* ssid = "WIFI ID";
const char* pass = "WIFI PASS";

/* ---------- How many RGB LEDs? ---------- */
constexpr uint8_t NUM_LEDS = 3;    // change to match your build

/* ---------- GPIO lists (R,G,B per LED) -- */
const uint8_t R_PIN[NUM_LEDS] = {25, 32, 14};   // LED‑0, LED‑1, LED‑2 …
const uint8_t G_PIN[NUM_LEDS] = {26, 33, 12};
const uint8_t B_PIN[NUM_LEDS] = {27, 19, 13};

/* ---------- server ---------- */
WiFiServer server(80);

/* ---------- helpers ---------- */
void sendCORS      (WiFiClient& c, const String& origin);
void sendPreflight (WiFiClient& c, const String& origin);
void sendPlain     (WiFiClient& c, const String& origin, const char* msg);
void sendHTML      (WiFiClient& c, const String& origin);
int  getParam      (const String& src, const char* key, int def = 0);
void setColor      (uint8_t led, uint8_t r, uint8_t g, uint8_t b);
void blinkTest     ();

/* ---------- SETUP ---------- */
void setup() {
  Serial.begin(115200);
  Serial.println("\nBooting…");

  for (uint8_t i = 0; i < NUM_LEDS; ++i) {
    pinMode(R_PIN[i], OUTPUT);
    pinMode(G_PIN[i], OUTPUT);
    pinMode(B_PIN[i], OUTPUT);
    setColor(i, 0, 0, 0);                     // off on power‑up
  }

  WiFi.begin(ssid, pass);
  Serial.printf("Connecting to %s", ssid);
  while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print('.'); }
  Serial.printf("\nConnected!  IP = %s\n", WiFi.localIP().toString().c_str());
  server.begin();
}

/* ---------- MAIN LOOP ---------- */
void loop() {
  WiFiClient client = server.available();
  if (!client) return;

  /* ---- request line ---- */
  String req = client.readStringUntil('\n'); req.trim();

  /* ---- collect Origin header ---- */
  String origin = "*";
  while (client.connected()) {
    String h = client.readStringUntil('\n');
  if (h == "\r" || h.isEmpty()) break;

    if (h.startsWith("Origin:")) { origin = h.substring(7); origin.trim(); }
  }

  /* ---- 1. CORS / PNA pre‑flight ---- */
  if (req.startsWith("OPTIONS ")) {
    sendPreflight(client, origin);
  }

  /* ---- 2. /set?L=…&R=…&G=…&B=… ---------- */
  else if (req.startsWith("GET /set?")) {
    uint8_t led = constrain(getParam(req,"L"), 0, NUM_LEDS - 1);
    uint8_t r   = getParam(req,"R");
    uint8_t g   = getParam(req,"G");
    uint8_t b   = getParam(req,"B");
    setColor(led, r, g, b);
    sendPlain(client, origin, "OK");
  }

  /* ---- 3. /blink ----------------------- */
  else if (req.startsWith("GET /blink")) {
    blinkTest();
    sendPlain(client, origin, "Blink test done");
  }

  /* ---- 4. any other path → built‑in page */
  else {
    sendHTML(client, origin);
  }

  client.stop();
}

/* ---------- helper: parse key=val -------- */
int getParam(const String& src, const char* key, int def) {
  String token = String(key) + '=';
  int i = src.indexOf(token);
  if (i == -1) return def;
  int end = src.indexOf('&', i);
  if (end == -1) end = src.indexOf(' ', i);
  return src.substring(i + token.length(), end).toInt();
}

/* ---------- LED helper ------------------- */
void setColor(uint8_t led, uint8_t r, uint8_t g, uint8_t b) {
#if defined(COMMON_ANODE)
  r = !r; g = !g; b = !b;
#endif
  digitalWrite(R_PIN[led], r ? HIGH : LOW);
  digitalWrite(G_PIN[led], g ? HIGH : LOW);
  digitalWrite(B_PIN[led], b ? HIGH : LOW);
  Serial.printf("LED %u  R=%u  G=%u  B=%u\n", led, r, g, b);
}

/* ---------- blink all LEDs --------------- */
void blinkTest() {
  for (uint8_t n = 0; n < 3; ++n) {
    for (uint8_t i = 0; i < NUM_LEDS; ++i) setColor(i, 1,0,0);
    delay(220);
    for (uint8_t i = 0; i < NUM_LEDS; ++i) setColor(i, 0,1,0);
    delay(220);
    for (uint8_t i = 0; i < NUM_LEDS; ++i) setColor(i, 0,0,1);
    delay(220);
    for (uint8_t i = 0; i < NUM_LEDS; ++i) setColor(i, 0,0,0);
    delay(180);
  }
}

/* ---------- CORS helpers ----------------- */
void sendCORS(WiFiClient& c, const String& origin) {
  c.print(F("Access-Control-Allow-Origin: ")); c.println(origin);
  c.println(F("Access-Control-Allow-Methods: GET, OPTIONS"));
  c.println(F("Access-Control-Allow-Headers: Content-Type"));
  c.println(F("Access-Control-Allow-Private-Network: true"));
}

void sendPreflight(WiFiClient& c, const String& origin) {
  c.println(F("HTTP/1.1 204 No Content")); sendCORS(c, origin);
  c.println(F("Content-Length: 0\r\n"));
}

void sendPlain(WiFiClient& c, const String& origin, const char* msg) {
  c.println(F("HTTP/1.1 200 OK")); sendCORS(c, origin);
  c.println(F("Content-Type: text/plain\r\n")); c.println(msg);
}

/* ---------- built‑in HTML page ----------- */
void sendHTML(WiFiClient& c, const String& origin) {
  c.println(F("HTTP/1.1 200 OK")); sendCORS(c, origin);
  c.println(F("Content-Type: text/html\r\n"));

  /* ---------- minimal UI (checkboxes) ----- */
  c.println(R"html(
<!DOCTYPE html>
<html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>ESP32 · Multi RGB‑LED</title>
<style>
 body{font-family:Arial;background:#111;color:#fff;text-align:center;padding:20px}
 select,label{margin:8px}
 input[type=checkbox]{transform:scale(1.3);margin-left:6px}
 #sample{width:90px;height:90px;margin:18px auto;border-radius:10px;border:2px solid #666}
 button{margin:14px;padding:8px 22px;border:none;border-radius:8px;cursor:pointer}
 a{color:#0af}
</style></head>
<body>
<h2>Multi RGB‑LED – ON/OFF</h2>

<select id="led"></select>
<div id="sample"></div>

<label>R <input type="checkbox" id="r"></label>
<label>G <input type="checkbox" id="g"></label>
<label>B <input type="checkbox" id="b"></label><br>

<button onclick="setLed()">Set LED</button>
<p><a href="/blink">Run blink test</a></p>

<script>
const NUM_LEDS = %NUM%;          // <‑‑ injected by sketch
const sel = document.getElementById('led');
for (let i = 0; i < NUM_LEDS; i++)
  sel.add(new Option('LED ' + i, i));

const cks = ['r','g','b'].map(id => document.getElementById(id));
cks.forEach(c => c.onchange = preview); sel.onchange = preview;

function preview(){
  const col = cks.map(c => c.checked?255:0);
  sample.style.background = `rgb(${col})`;
}

function setLed(){
  const led = sel.value,
        [r,g,b] = cks.map(c => c.checked ? 1 : 0);
  fetch(`/set?L=${led}&R=${r}&G=${g}&B=${b}`).catch(console.error);
}
preview();
</script>
</body></html>
)html");

  /* tiny hack to inject NUM_LEDS into the HTML sent */
  c.printf("<script>document.currentScript.previousElementSibling.innerHTML="
           "document.currentScript.previousElementSibling.innerHTML.replace('%%NUM%%','%u');</script>", NUM_LEDS);
}
