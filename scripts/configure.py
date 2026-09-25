"""Read-only MESHBBS USB diagnostics. Administrative writes require the owner app."""
import argparse
import serial
import time
from serial.tools import list_ports

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port")
    parser.add_argument("--status",action="store_true")
    parser.add_argument("--activity",action="store_true")
    parser.add_argument("--monitor",action="store_true")
    args=parser.parse_args()
    if not args.port:
        for p in list_ports.comports():print(f"{p.device}: {p.description}")
        parser.error("Select a USB port with --port")
    p=serial.Serial();p.port=args.port;p.baudrate=115200;p.timeout=.2;p.dtr=False;p.rts=False
    with p:
        if args.activity:p.write(b"\nACTIVITY\n")
        elif not args.monitor:p.write(b"\nSTATUS\n")
        end=time.monotonic()+4
        try:
            while args.monitor or time.monotonic()<end:
                data=p.read(p.in_waiting or 1)
                if data:print(data.decode("utf-8",errors="replace"),end="",flush=True)
        except KeyboardInterrupt:pass
if __name__=="__main__":main()
