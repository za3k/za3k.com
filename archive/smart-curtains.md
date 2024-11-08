How to set up IKEA Fyrtur curtains (with or without zigbee).

First, plug in the repeater, and put a battery in the remote.

To have your remote control Fyrtur curtains:

1. Pair repeater to remote. Hold it next to the repeater, and hold down the button for 10 seconds. It should glow red. This indicates the remote is (IKEA) paired to the repeater. **Continue holding the button!**
2. On the blind, tap both buttons. A white light should come on steady. This indicates the curtain is in (IKEA) pairing mode.
3. Hold the remote the the blinds. The white light should dim and fade. The blinds are now (IKEA) paired to the remote and repeater.

Note that the "pairing" above is not the same as zigbee pairing.

Also, you can hook together 1 repeater, 1 remote, and 1-4 sets of blinds. The remote will control them together.

---

To connect all the above devices to a third-party zigbee hub:

1. Open zigbee2mqtt and enable pairing (or similar for your setup)
2. Insert a paperclip into the hole in the repeater, until the white light dims. This performs a factory reset and allows zigbee pairing. The repeater should zigbee-pair and appear as E1746. Note, this didn't really do anything. Removing the repeater works fine with zigbee control for me.
3. Press both curtain buttons. Hold for 5 seconds. This performs a factory reset and allows zigbee pairing. The curtain should zigbee-pair and appear as E1757.
4. Press the remote button 4 times in a row. This performs a factory reset and allows zigbee pairing. The remote should zigbee-pair and appear as E1766.
5. AFTER the preceeding, you might be able to get devices to pair. I didn't get it to work and also support zigbee.

With readable name "Blinds/1/Blind", "Blinds/1/Repeater", and "Blinds/1/Remote":

    mosquitto_pub -h $ZIGBEE_BROKER -t "zigbee2mqtt/Blinds/1/Blind/set" -m '{"position": 28}'

Sets the curtains to 28% of the way from the bottom (almost fully extended). 

---

These blinds came with 6 manuals. That's too many. Most of it is warnings and install instructions. Here's the stuff you'll need later:

TRÅDFRI (remote)
    - The remote takes a CR2032, which may not be included.
    - IKEA's operation mode for the remote requires the repeater to be present and plugged in.
    - Tap up/down to raise/lower the curtains all the way.
    - Hold up/down to raise/lower curtains until you release.
    - Opening the back, there is a pairing button.
        - Tap 4 times to perform a factory reset and enter zigbee pairing mode.
        - Hold for 10 seconds to pair with the repeater. Then repeat, holding for 10 seconds to pair with blinds.
TRÅDFRI (repeater)
    - Insert a paperclip and hold 5 seconds to do a factory reset and enter zigbee pairing mode.
FYRTUR (blinds)
    - Tap up/down to raise/lower the curtains all the way.
    - Tap up and down at the same time to enter IKEA pairing mode
    - Double-tap up to set the highest the curtain will go (minimum extension). Double-tap down to set the lowest the curtain will go (maximum extension).
        - While the curtains at at the top, double-tap either up or down to reset limits.
        - In Zigbee, position 0 is maximum extension, and 100 is minimum extension.
    - Hold up and down for 5 seconds to do a factory reset and enter zigbee pairing mode.
