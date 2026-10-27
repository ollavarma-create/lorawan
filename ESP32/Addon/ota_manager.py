#!/usr/bin/env python3
"""
ota_manager.py – LoRaWAN-laitteiden OTA-hallinta

Toiminta:
  1. Kuuntelee Chirpstack MQTT -integraatiota (laitelista, akkutiedot)
  2. Näyttää laiteluettelon MAC/devEUI -avaimella
  3. Lähettää OTA-komennon valitulle laitteelle Chirpstack REST API:n kautta

Vaatimukset:
  pip install paho-mqtt requests

Käyttö:
  python3 ota_manager.py                     # Interaktiivinen tila
  python3 ota_manager.py --list              # Listaa havaitut laitteet
  python3 ota_manager.py --ota <devEUI>      # Lähetä OTA-komento
  python3 ota_manager.py --ota <devEUI> --timeout 10  # OTA, 10 min timeout

Asetukset:
  Muokkaa CONFIG-lohkoa alla tai aseta ympäristömuuttujat.
"""

import argparse
import base64
import json
import os
import sys
import threading
import time
from datetime import datetime

try:
    import paho.mqtt.client as mqtt
    import requests
except ImportError:
    print("Asenna riippuvuudet: pip install paho-mqtt requests")
    sys.exit(1)

# ── Asetukset ────────────────────────────────────────────────────────────────
CONFIG = {
    # Chirpstack-palvelin
    "chirpstack_host":  os.getenv("CHIRPSTACK_HOST",  "localhost"),
    "chirpstack_port":  int(os.getenv("CHIRPSTACK_PORT", "8080")),
    "chirpstack_token": os.getenv("CHIRPSTACK_TOKEN", ""),  # API-token Chirpstackista

    # MQTT-brokeri (sama kuin Chirpstackin integraatiossa)
    "mqtt_host":        os.getenv("MQTT_HOST",    "localhost"),
    "mqtt_port":        int(os.getenv("MQTT_PORT", "1883")),
    "mqtt_user":        os.getenv("MQTT_USER",    ""),
    "mqtt_pass":        os.getenv("MQTT_PASS",    ""),

    # Chirpstack application ID (löytyy web-käyttöliittymästä)
    "application_id":   os.getenv("APPLICATION_ID", ""),

    # MQTT topic (Chirpstack v4 muoto)
    "uplink_topic":     "application/+/device/+/event/up",

    # Downlink fPort
    "fport":            1,
}
# ─────────────────────────────────────────────────────────────────────────────

# Havaitut laitteet: {devEUI: {"name": ..., "last_seen": ..., "batt_pct": ..., "rssi": ...}}
devices: dict = {}
devices_lock = threading.Lock()


def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"[mqtt] Yhdistetty brokeriin {CONFIG['mqtt_host']}:{CONFIG['mqtt_port']}")
        client.subscribe(CONFIG["uplink_topic"])
        print(f"[mqtt] Kuunnellaan: {CONFIG['uplink_topic']}")
    else:
        print(f"[mqtt] Yhteys epäonnistui, koodi: {rc}")


def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode())
    except Exception:
        return

    dev_eui  = payload.get("deviceInfo", {}).get("devEui", "")
    dev_name = payload.get("deviceInfo", {}).get("deviceName", "")
    obj      = payload.get("object", {})
    rx_info  = payload.get("rxInfo", [{}])

    if not dev_eui:
        return

    batt_pct = obj.get("batt_pct")
    batt_v   = obj.get("batt_v")
    rssi     = rx_info[0].get("rssi") if rx_info else None
    snr      = rx_info[0].get("snr")  if rx_info else None

    with devices_lock:
        if dev_eui not in devices:
            print(f"\n[mqtt] Uusi laite havaittu: {dev_name} ({dev_eui})")

        devices[dev_eui] = {
            "name":      dev_name,
            "last_seen": datetime.now().strftime("%H:%M:%S"),
            "batt_pct":  batt_pct,
            "batt_v":    batt_v,
            "rssi":      rssi,
            "snr":       snr,
            "temp":      obj.get("temp"),
            "hum":       obj.get("hum"),
            "pres":      obj.get("pres"),
        }


def list_devices():
    with devices_lock:
        if not devices:
            print("Ei havaittuja laitteita. Odota uplink-viestejä...")
            return

        print(f"\n{'DevEUI':<20} {'Nimi':<25} {'Viimeksi':<10} {'Akku':>8} {'RSSI':>7}")
        print("─" * 75)
        for eui, d in devices.items():
            batt  = f"{d['batt_pct']}% ({d['batt_v']}V)" if d['batt_pct'] is not None else "–"
            rssi  = f"{d['rssi']} dBm" if d['rssi'] is not None else "–"
            print(f"{eui:<20} {d['name']:<25} {d['last_seen']:<10} {batt:>8} {rssi:>7}")
        print()


def send_ota_downlink(dev_eui: str, timeout_min: int = 5) -> bool:
    """
    Lähettää OTA-komennon laitteelle Chirpstack REST API:n kautta.
    Komento: [0x02, timeout_min]
    """
    host  = CONFIG["chirpstack_host"]
    port  = CONFIG["chirpstack_port"]
    token = CONFIG["chirpstack_token"]

    if not token:
        print("[ota] VIRHE: CHIRPSTACK_TOKEN ei ole asetettu")
        print("       Luo API-token: Chirpstack UI → API Keys → Lisää")
        return False

    # Payload: [0x02, timeout_min]
    raw = bytes([0x02, max(1, min(255, timeout_min))])
    b64 = base64.b64encode(raw).decode()

    url = f"http://{host}:{port}/api/devices/{dev_eui}/queue"
    headers = {
        "Content-Type":  "application/json",
        "Authorization": f"Bearer {token}",
        "Grpc-Metadata-Authorization": f"Bearer {token}",
    }
    body = {
        "queueItem": {
            "fPort":     CONFIG["fport"],
            "data":      b64,
            "confirmed": False,
        }
    }

    print(f"\n[ota] Lähetetään OTA-komento → {dev_eui}")
    print(f"[ota] Timeout: {timeout_min} min | Payload: {raw.hex().upper()}")

    try:
        r = requests.post(url, headers=headers, json=body, timeout=10)
        if r.status_code in (200, 201):
            data = r.json()
            print(f"[ota] Onnistui! downlink ID: {data.get('id', '–')}")
            print(f"[ota] Laite vastaanottaa komennon seuraavassa uplinkissa.")
            print(f"      Avaa Arduino IDE → Portit kun laite yhdistää WiFiin.")
            return True
        else:
            print(f"[ota] HTTP virhe {r.status_code}: {r.text}")
            return False
    except requests.RequestException as e:
        print(f"[ota] Verkkovirhe: {e}")
        return False


def send_command_downlink(dev_eui: str, command_bytes: bytes) -> bool:
    """Lähetä mielivaltainen downlink-komento."""
    host  = CONFIG["chirpstack_host"]
    port  = CONFIG["chirpstack_port"]
    token = CONFIG["chirpstack_token"]

    if not token:
        print("[dl] VIRHE: CHIRPSTACK_TOKEN ei ole asetettu")
        return False

    url = f"http://{host}:{port}/api/devices/{dev_eui}/queue"
    headers = {
        "Content-Type":  "application/json",
        "Authorization": f"Bearer {token}",
        "Grpc-Metadata-Authorization": f"Bearer {token}",
    }
    body = {
        "queueItem": {
            "fPort":     CONFIG["fport"],
            "data":      base64.b64encode(command_bytes).decode(),
            "confirmed": False,
        }
    }
    try:
        r = requests.post(url, headers=headers, json=body, timeout=10)
        if r.status_code in (200, 201):
            print(f"[dl] Komento lähetetty: {command_bytes.hex().upper()}")
            return True
        else:
            print(f"[dl] HTTP virhe {r.status_code}: {r.text}")
            return False
    except requests.RequestException as e:
        print(f"[dl] Verkkovirhe: {e}")
        return False


def start_mqtt_listener() -> mqtt.Client:
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    if CONFIG["mqtt_user"]:
        client.username_pw_set(CONFIG["mqtt_user"], CONFIG["mqtt_pass"])

    try:
        client.connect(CONFIG["mqtt_host"], CONFIG["mqtt_port"], keepalive=60)
        client.loop_start()
    except Exception as e:
        print(f"[mqtt] Yhteys epäonnistui: {e}")

    return client


def interactive_mode(client):
    """Interaktiivinen komentorivi-käyttöliittymä."""
    print("\nLoRaWAN OTA Manager – interaktiivinen tila")
    print("Komennot: list | ota <devEUI> [timeout_min] | sleep <devEUI> <sec> | quit\n")

    while True:
        try:
            line = input(">>> ").strip()
        except (EOFError, KeyboardInterrupt):
            break

        parts = line.split()
        if not parts:
            continue

        cmd = parts[0].lower()

        if cmd in ("list", "l"):
            list_devices()

        elif cmd in ("ota", "o") and len(parts) >= 2:
            eui = parts[1]
            timeout = int(parts[2]) if len(parts) >= 3 else 5
            send_ota_downlink(eui, timeout)

        elif cmd == "sleep" and len(parts) >= 3:
            eui = parts[1]
            sec = int(parts[2])
            high = (sec >> 8) & 0xFF
            low  = sec & 0xFF
            send_command_downlink(eui, bytes([0x01, high, low]))

        elif cmd == "probe" and len(parts) >= 2:
            # Tyhjennä pin-välimuisti → pakottaa uudelleentunnistuksen
            send_command_downlink(parts[1], bytes([0x03]))

        elif cmd in ("quit", "q", "exit"):
            break

        else:
            print("Tuntematon komento. Käytä: list | ota <devEUI> [min] | sleep <devEUI> <sec> | quit")

    client.loop_stop()
    client.disconnect()


def main():
    parser = argparse.ArgumentParser(description="LoRaWAN OTA Manager")
    parser.add_argument("--list",    action="store_true",     help="Listaa havaitut laitteet (5 s kuuntelu)")
    parser.add_argument("--ota",     metavar="DEVEUI",        help="Lähetä OTA-komento laitteelle")
    parser.add_argument("--timeout", metavar="MIN", type=int, help="OTA-timeout minuutteina (oletus: 5)")
    args = parser.parse_args()

    client = start_mqtt_listener()

    if args.list:
        print("Kuunnellaan 5 sekuntia...")
        time.sleep(5)
        list_devices()
        client.loop_stop()
        client.disconnect()

    elif args.ota:
        timeout = args.timeout or 5
        success = send_ota_downlink(args.ota, timeout)
        client.loop_stop()
        client.disconnect()
        sys.exit(0 if success else 1)

    else:
        interactive_mode(client)


if __name__ == "__main__":
    main()
