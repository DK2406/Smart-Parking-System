"""
Smart Parking - Serial to Excel Logger
----------------------------------------
Reads structured "LOG,..." lines from the STM32 over UART and appends
each event as a row in an Excel sheet, with an accurate PC-side
timestamp plus the board's own millisecond tick.

Install dependencies first:
    pip install pyserial openpyxl

Run:
    python excel_logger.py
"""

import os
import serial
from datetime import datetime
from openpyxl import Workbook, load_workbook

# ---------------- CONFIG -----------------------------------------
SERIAL_PORT = "COM4"        # Windows: "COM4"  |  Linux: "/dev/ttyUSB0"
BAUD_RATE = 115200             # must match MX_USART1_UART_Init() baud
EXCEL_FILE = "parking_log.xlsx"
# -------------------------------------------------------------------

HEADERS = ["PC_Timestamp", "Tick_ms", "Event", "User/UID", "Slot", "Status"]


def init_excel():
    """Open the log workbook, creating it with headers if it doesn't exist."""
    if os.path.exists(EXCEL_FILE):
        wb = load_workbook(EXCEL_FILE)
        ws = wb.active
    else:
        wb = Workbook()
        ws = wb.active
        ws.title = "ParkingLog"
        ws.append(HEADERS)
        wb.save(EXCEL_FILE)

    return wb, ws


def parse_line(raw_line):
    """
    Parse one 'LOG,<tick>,<event>,<user>,<slot>,<status>' line.
    Returns a tuple of fields, or None if the line isn't a valid log line.
    """
    if not raw_line.startswith("LOG,"):
        return None  # boot banner / decorative text -> ignore

    fields = raw_line.split(",")

    if len(fields) < 6:
        return None  # malformed / truncated line -> ignore

    _, tick_ms, event, user, slot, status = fields[:6]

    try:
        tick_ms = int(tick_ms)
    except ValueError:
        return None

    return tick_ms, event, user, slot, status


def main():
    wb, ws = init_excel()

    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    print(f"Listening on {SERIAL_PORT} @ {BAUD_RATE} baud... (Ctrl+C to stop)")

    try:
        while True:
            raw = ser.readline().decode(errors="ignore").strip()

            if not raw:
                continue

            parsed = parse_line(raw)

            if parsed is None:
                continue

            tick_ms, event, user, slot, status = parsed

            # PC timestamp assigned the instant the line arrives.
            # UART transit is on the order of microseconds-to-milliseconds,
            # so this is effectively the true wall-clock time of the event.
            pc_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]

            ws.append([pc_time, tick_ms, event, user, slot, status])

            # Save after every single row so a crash/power loss never
            # costs more than the one row currently being written.
            wb.save(EXCEL_FILE)

            print(f"[{pc_time}] {event:<8} {user:<6} {slot:<5} {status}")

    except KeyboardInterrupt:
        print("\nStopping logger, final save...")
        wb.save(EXCEL_FILE)

    finally:
        ser.close()
        print(f"Log saved to {EXCEL_FILE}")


if __name__ == "__main__":
    main()