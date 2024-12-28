#! /usr/bin/env python3
import serial
import binascii
import logging
import json
import time

logging.basicConfig(
    level=logging.WARNING,
    format="%(asctime)s - %(levelname)s - %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
# Configure file handler to save logs to log.txt
file_handler = logging.FileHandler("log.txt")
file_handler.setLevel(logging.INFO)
# Create formatter and add it to the handler
formatter = logging.Formatter(
    "%(asctime)s - %(levelname)s - %(message)s", datefmt="%Y-%m-%d %H:%M:%S"
)
file_handler.setFormatter(formatter)
# Add the file handler to the logger
logging.getLogger().addHandler(file_handler)

LSM = json.load(open("code.json"))


def byte2hex(payload):
    msg_hex = binascii.hexlify(payload).decode("utf-8")
    msg_hex = " ".join([msg_hex[i : i + 4] for i in range(0, len(msg_hex), 4)])
    return msg_hex.upper()


def hex2byte(message_hex):
    return bytes.fromhex(message_hex.replace(" ", ""))


def publish_cmd(Fd: serial.Serial, cmd):
    time.sleep(5 / 1000.0)  # sleep 5ms
    Fd.write(hex2byte(cmd))
    logging.info(f"Publish cmd: {cmd}")


if __name__ == "__main__":
    serialFd = serial.Serial(
        "COM4", baudrate=9600, bytesize=8, parity=serial.PARITY_EVEN, stopbits=1
    )
    STATE = "IDLE"
    UNLOCK_CNT = 0
    if serialFd.is_open:
        logging.info("Serial is open")
    while True:
        try:
            msg = serialFd.read(size=4)
            message_hex = byte2hex(msg)

            for k, v in LSM.items():
                message_hex = message_hex.replace(v, k)
            logging.info(f"message_hex: {message_hex}")
            if "H_Discover" in message_hex:
                publish_cmd(serialFd, LSM["C_ImHere"])
            elif "H_Ring" in message_hex:
                publish_cmd(serialFd, LSM["C_RingACK"])
                STATE = "RING"
            elif "H_PkupAck" in message_hex:
                STATE = "SPEAKING"
                publish_cmd(serialFd, LSM["C_Spking"])
            elif "H_UnlockAck" in message_hex:
                publish_cmd(serialFd, LSM["C_Spking"])
            elif "H_CallHangupACK" in message_hex:
                STATE = "IDLE"
                publish_cmd(serialFd, LSM["CHB"])
            elif "HHB" in message_hex:
                if STATE == "IDLE":
                    publish_cmd(serialFd, LSM["CHB"])
                elif STATE == "RING":
                    publish_cmd(serialFd, LSM["C_Pkup"])
                elif STATE == "SPEAKING":
                    if UNLOCK_CNT < 5:
                        publish_cmd(serialFd, LSM["C_Unlock"])
                        UNLOCK_CNT += 1
                    else:
                        publish_cmd(serialFd, LSM["C_CallHangup"])
                        UNLOCK_CNT = 0
            elif "H_ACK" in message_hex:
                pass
            else:
                logging.error(f"Unknown message: {message_hex}")
                STATE = "IDLE"

            if STATE != "IDLE":
                logging.warning(f"STATE: {STATE}")
        except KeyboardInterrupt:
            serialFd.close()
            logging.error("KeyboardInterrrupt, closing")
            exit(0)
