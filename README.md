# proximity
A proximity monitor for WiFi devices.

## Configuration

The proximity monitor requires several items of configuration.
- A list of access points and passwords to try to use (makes it more portable).
- A list of devices (arbitrary name, WiFi MAC ID, and idle time) to monitor.
- An IFTTT Webhook key.
- An IFTTT Webhook 'on' trigger.
- An IFTTT Webhook 'off' trigger.

Make sure to use the correct MAC IDs in the configuration, otherwise you will
be waiting for packets that will never arrive. You will need to check your
device(s) to find their MAC IDs.

- iOS: Settings --> Wi-Fi --> (AP) --> Wi-Fi Access.
- Windows: cmd --> ipconfig /all --> Wireless LAN adapter Wi-Fi --> Physical Access.

## Operation

The proximity monitor determines a local WiFi access point and channel to use,
then configures to sniff packets (promiscuous mode) until it identifies a packet
related to any of the configured devices, or until it has not seen one of those
packets for more than the configured idle time.

If the in-range or out-of-range state changes, the proximity monitor exits
promiscuous mode, enters normal station mode, and sends the appropriate on or
off trigger to IFTTT.

And then it loops.

## Interactions

The basic interactions are as shown in the following sequence diagram.
```mermaid
sequenceDiagram
  participant D as device(s)
  participant P as proximity
  participant A as access point
  participant I as maker.ifttt.com

  activate P
  P->>A: scan access point
  A->>P: reponse
  deactivate P

  loop forever
    note over P: configure promiscuous mode

    activate P
    loop until state changes
    alt device in range
      D->>P: device packet
      P->>P: reset timer
    else device not in range
      P->>P: timeout
    end
    end
    deactivate P

    note over P: configure station mode
    activate P
    alt turn IOT device on
      P->>+A: GET http://maker.ifttt.com:80//trigger/{trigger_on}/with/key/{key}
      A->>+I: GET http://maker.ifttt.com:80//trigger/{trigger_on}/with/key/{key}
      I->>-A: response
      A->>-P: response
    else turn IOT device off
      P->>+A: GET http://maker.ifttt.com:80//trigger/{trigger_off}/with/key/{key}
      A->>+I: GET http://maker.ifttt.com:80//trigger/{trigger_off}/with/key/{key}
      I->>-A: response
      A->>-P: response
    end
    deactivate P
end
```
