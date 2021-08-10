#ifndef __CONFIGURATION_H__
#define __CONFIGURATION_H__

/**
   Array of access point configurations.
*/
NETWORK networks[] =
{
  {"ssid1", "password1"},
  {"ssid2", "password2"}
};

/**
   Array of device configurations.
*/
DEVICE devices[] =
{
  {"device name 1", {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}, 10 * 60 * 1000, 0},
  {"device name 2", {0xff, 0xee, 0xdd, 0xcc, 0xbb, 0xaa}, 10 * 60 * 1000, 0}
};

/**
   IFTTT Webhook configuration data to form a URL like the follwing example.
   http://maker.ifttt.com:80//trigger/{webhook_trigger}/with/key/{webhook_key}
   See https://ifttt.com/maker_webhooks --> (Documentation) for more information.
*/
String webhook_key = "not_a_real_ifttt_webhook_key";
String webhook_trigger_on = "webhook_on_trigger";
String webhook_trigger_off = "webhook_off_trigger";

#endif
