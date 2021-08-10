//
// include the standard Arduino WIFI definitions
//
#include <WiFi.h>

//
// also include the low-level ESP32 WIFI definitions
//
#include "esp_wifi.h"

/**
    Set the status update interval to be one second, in milliseconds.
*/
const int INTERVAL_MS = 1 * 1000;

/**
   Device configuration structure definition.
*/
typedef struct
{
  String name;
  uint8_t macid[6];
  int max_ttl_ms;
  int ttl_ms;
} DEVICE;

/**
   Access point configuration structure definition.
*/
typedef struct
{
  String ssid;
  String password;
} NETWORK;

/**
   WIFI payload header structure: 802.11 Wireless LAN MAC frame.
*/
typedef struct {
  int16_t fctl;
  int16_t duration;
  uint8_t dest_mac[6]; // the device MACID may appear here
  uint8_t source_mac[6]; // ...or it may appear here
  uint8_t bssid[6]; // ...or or it may appear here
  int16_t seqctl;
  uint8_t payload[];
} sniffer_payload_t;

/**
   Index to the current access point configuration record.
*/
int network = -1;

/**
   Access point channel on which to sniff packets.
*/
int channel = 5;

/**
 * All of the user-specific or site-specific configuration data.
 */
#include "Configuration.h"

/**
   Packet sniffer callback function.
*/
void sniffer (void* buffer, wifi_promiscuous_pkt_type_t type)
{
  //
  // this code runs as a callback _within_ the WiFi handler and must be as fast as possible,
  // if this code is slower than the input data rate then the code will crash
  //
  wifi_promiscuous_pkt_t *packet = (wifi_promiscuous_pkt_t*) buffer;
  int length = packet->rx_ctrl.sig_len - sizeof (sniffer_payload_t);
  sniffer_payload_t *payload = (sniffer_payload_t*) packet->payload;

  //
  // define an an array of parts of the frame to inspect as a way to simplify the code
  //
  uint8_t *macids[] = {
    payload->dest_mac,
    payload->source_mac,
    payload->bssid
  };

  //
  // loop over the devices and the items in the frame to compare
  //
  if (length > 0)
  {
    for (int loop = 0; loop < sizeof (devices) / sizeof (devices[0]); loop++)
    {
      for (int id = 0; id < sizeof (macids) / sizeof (macids[0]); id++)
      {
        if (memcmp (macids[id], devices[loop].macid, sizeof (devices[loop].macid)) == 0)
        {
          devices[loop].ttl_ms = devices[loop].max_ttl_ms;
        }
      }
    }
  }
}

/**
   Function to initialize sniffing/promiscuous mode.
*/
void init_sniffing (void)
{
  //
  // totally de-initialize the Arduino WIFI stack first
  //
  WiFi.disconnect (true);
  WiFi.mode (WIFI_OFF);
  esp_wifi_deinit ();

  //
  // use the low-level ESP32 code to do the sniffing
  //
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init (&cfg);
  esp_wifi_set_storage (WIFI_STORAGE_RAM);
  esp_wifi_set_mode (WIFI_MODE_NULL);
  esp_wifi_start ();
  esp_wifi_set_promiscuous_rx_cb (&sniffer);
  esp_wifi_set_promiscuous (true);
  esp_wifi_set_channel (channel, WIFI_SECOND_CHAN_NONE);

  Serial.print ("scanning channel ");
  Serial.print (channel);
  Serial.print (" on ");
  Serial.print (networks[network].ssid);
  Serial.println ();
}

/**
   Function to initialize normal station access mode.
*/
void init_station (void)
{
  //
  // disable sniffing before using the Arduino WIFI stack
  //
  esp_wifi_set_promiscuous (false);

  //
  // set the network hostname
  //
  WiFi.config (INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname ("proximity");

  //
  // typical Arduino WIFI station initialization
  //
  WiFi.mode (WIFI_STA);
  WiFi.begin (networks[network].ssid.c_str (), networks[network].password.c_str ());
  while (WiFi.status () != WL_CONNECTED)
  {
    delay (500);
    Serial.print (".");
  }
  Serial.println ();

  Serial.print ("connected to ");
  Serial.print (networks[network].ssid);
  Serial.print (" as ");
  Serial.print (WiFi.localIP ());
  Serial.println ();
}

/**
   Function to send IOT control update requests.
*/
void toggle (bool state)
{
  //
  // get into station mode first
  //
  init_station ();

  //
  // form the IOT request message
  //
  String trigger = "proximity_false";
  if (state)
  {
    Serial.println ("TURN ON");
    trigger = webhook_trigger_on;
  }
  else
  {
    Serial.println ("TURN OFF");
    trigger = webhook_trigger_off;
  }

  String host = "maker.ifttt.com";
  const int port = 80;
  const String url = "/trigger/" + trigger + "/with/key/" + webhook_key;
  
  //
  // send the IFTTT Webhook request message and process the response
  //
  Serial.println ("----------------------------------------");
  Serial.println ("http://" + host + ":" + String (port) + url);
  Serial.println ("----------------------------------------");
  WiFiClient client;
  if (client.connect (host.c_str (), port))
  {
    try
    {
      client.print ("GET " + url + " HTTP/1.1\r\n");
      client.print ("Host: " + host + "\r\n");
      client.print ("Connection: close\r\n");
      client.print ("\r\n");

      unsigned long timeout = millis ();
      while (client.available () == 0)
      {
        if (millis () - timeout > 5000)
        {
          Serial.println ("** client timeout **");
          throw true;
        }
      }

      while (client.available ())
      {
        String line = client.readStringUntil ('\r');
        Serial.print (line);
      }
    }
    catch (...)
    {
    }

    client.stop ();
    Serial.println ();
  }
  Serial.println ("----------------------------------------");

  //
  // start examining packets again
  //
  init_sniffing ();
}

/**
   Arduino sketch setup function to identify the local network and start packet sniffing.
*/
void setup ()
{
  //
  // initalize the serial/USB output for debugging/etc.
  //
  Serial.begin (115200);
  delay (250);

  //
  // set the network hostname
  //
  WiFi.config (INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
  WiFi.setHostname ("proximity");

  //
  // find the first available/configured access point to use
  //
  WiFi.mode (WIFI_STA);
  while (true)
  {
    int count = WiFi.scanNetworks ();

    for (int j = 0; j < sizeof (networks) / sizeof (networks[0]); j++)
    {
      for (int i = 0; i < count; i++)
      {
        if (WiFi.SSID (i) == networks[j].ssid)
        {
          network = j;
          channel = WiFi.channel (i);
        }
      }
    }

    if (network < 0)
    {
      Serial.println ("error - failed to locate access point");
      delay (30 * 1000);
    }
    else
    {
      break;
    }
  }

  //
  // get into station mode first
  //
  init_station ();

  //
  // start examining packets
  //
  init_sniffing ();
}

/**
   Arduino sketch loop function to determine of IOT updates are required.
*/
void loop ()
{
  //
  // hold the latest IOT control state
  //
  static bool state = false;

  //
  // update the device status and determine if any devices are in range
  //
  int ttl_ms = 0;
  for (int loop = 0; loop < sizeof (devices) / sizeof (devices[0]); loop++)
  {
    devices[loop].ttl_ms = max (devices[loop].ttl_ms - INTERVAL_MS, 0);
    ttl_ms = ttl_ms + devices[loop].ttl_ms;
  }

#if 1
  Serial.print (ttl_ms);
  Serial.print (" ");
  Serial.print (state);
  Serial.println ();
#endif

  //
  // turn on the IOT device is any devices are in range
  //
  if ((state == false) && (ttl_ms > 0))
  {
    state = true;
    toggle (state);
  }

  //
  // turn off the IOT device is no devices are in range
  //
  if ((state == true) && (ttl_ms == 0))
  {
    state = false;
    toggle (state);
  }

  //
  // wait for more time/samples
  //
  delay (INTERVAL_MS);
}
