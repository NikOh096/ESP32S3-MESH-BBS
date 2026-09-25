"""BLE owner protocol regression. Requires bleak; physical enrollment is mandatory.

Use --enroll only after the owner opens enrollment using BOOT. The temporary PC
owner key stays in .local, never in a release. Re-enroll the Android phone later.
Existing bulletins, bans and radio settings are left unchanged.
"""
import argparse
import asyncio
import hashlib
import hmac
import json
import secrets
import time
from pathlib import Path
from bleak import BleakClient, BleakScanner

INFO = "8f3a0002-6ca2-4d15-9d49-75b974ad2716"
RPC = "8f3a0006-6ca2-4d15-9d49-75b974ad2716"
PRIVATE = Path(__file__).resolve().parents[1] / ".local" / "pc-owner.json"

class Session:
    def __init__(self, client): self.client, self.id = client, 0
    async def info(self): return json.loads(await self.client.read_gatt_char(INFO))
    async def call(self, op, **args):
        self.id += 1
        payload = json.dumps(dict(id=self.id, op=op, **args), ensure_ascii=False, separators=(",", ":")).encode()
        assert len(payload) <= 512
        await self.client.write_gatt_char(RPC, payload, response=True)
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            await asyncio.sleep(.07)
            reply = json.loads(await self.client.read_gatt_char(RPC))
            if reply.get("id") == self.id and not reply.get("busy"): return reply
        raise TimeoutError("RPC response timeout")
    async def auth(self, key):
        info = await self.info()
        proof = hmac.new(key, bytes.fromhex(info["nonce"]) + info["id"].encode("ascii"), hashlib.sha256).hexdigest()
        assert (await self.call("auth", proof=proof))["ok"]
        return proof

async def run(args):
    device = await BleakScanner.find_device_by_address(args.address, timeout=20)
    if not device: raise RuntimeError("Board not advertising to this PC. Open physical enrollment first.")
    client = BleakClient(device, pair=True, timeout=20, winrt={"use_cached_services": False})
    key = None
    try:
        await asyncio.wait_for(client.connect(), 40)
        s = Session(client); info = await s.info()
        print(f"Encrypted owner service v{info['v']}; board={info['id']}; enrollment={info['enroll']}", flush=True)
        assert not (await s.call("status"))["ok"]
        print("PASS: unauthenticated administrative request denied", flush=True)
        if args.enroll:
            if not info["enroll"]: raise RuntimeError("Physical enrollment window is closed")
            key = secrets.token_bytes(32)
            PRIVATE.parent.mkdir(exist_ok=True)
            PRIVATE.write_text(json.dumps(dict(board=info["id"], key=key.hex())), encoding="utf-8")
            assert (await s.call("enroll", proof=key.hex()))["ok"]
            print("PASS: physical enrollment; test owner stored privately", flush=True)
        else:
            saved = json.loads(PRIVATE.read_text(encoding="utf-8"))
            assert saved["board"] == info["id"]
            key = bytes.fromhex(saved["key"])
        proof = await s.auth(key)
        assert not (await s.call("auth", proof=proof))["ok"]
        assert not (await s.call("status"))["ok"]
        await s.auth(key)
        assert not (await s.call("auth", proof="00"*32))["ok"]
        assert not (await s.call("status"))["ok"]
        await s.auth(key)
        print("PASS: HMAC authentication; replay and wrong-key rejection", flush=True)
        assert (await s.call("clock", time=int(time.time())))["ok"]
        status = await s.call("status"); assert status["ok"]
        print(f"Node state: pairing={status['pair']}; BLE={status['ble']}; API={status['api']}", flush=True)
        slots = [await s.call("list", slot=i) for i in range(1,10)]
        assert all(x["ok"] for x in slots)
        # Test only a previously empty slot, and remove the test record in finally.
        empty = next((x["slot"] for x in slots if not x["active"]), None)
        if empty:
            generation = None
            try:
                assert (await s.call("create", slot=empty, title="BLE test", text="Temporary owner protocol test", expires=0))["ok"]
                meta = await s.call("list", slot=empty); generation=meta["generation"]
                root=await s.call("read", slot=empty, generation=generation, index=-1)
                assert root["ok"] and root["text"]=="Temporary owner protocol test"
                assert not (await s.call("delete", slot=empty, generation=generation+1))["ok"]
                assert (await s.call("expiry", slot=empty, generation=generation, expires=int(time.time())+86400))["ok"]
                print("PASS: create/read/expiry and stale-generation guard", flush=True)
            finally:
                if generation is not None: assert (await s.call("delete", slot=empty, generation=generation))["ok"]
        bans=await s.call("ban_list", index=0);assert bans["ok"]
        banned=set()
        for i in range(bans["count"]): banned.add((await s.call("ban_list",index=i))["node"])
        test_node="!feda0001"
        if test_node not in banned and bans["count"]<64 and status["node"]!=test_node:
            try:
                assert (await s.call("ban_add",node=test_node))["ok"]
                assert (await s.call("ban_list",index=bans["count"]))["node"]==test_node
            finally: assert (await s.call("ban_remove",node=test_node))["ok"]
            print("PASS: ban add/list/remove, previous list retained",flush=True)
        await client.disconnect();await asyncio.sleep(2)
        client=BleakClient(device,pair=True,timeout=20,winrt={"use_cached_services":False})
        await asyncio.wait_for(client.connect(),40)
        s=Session(client);await s.auth(key);assert (await s.call("status"))["ok"]
        print("PASS: bonded owner reconnect outside enrollment; authentication required again",flush=True)
    finally:
        if client.is_connected: await client.disconnect()

if __name__=="__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--address",required=True)
    parser.add_argument("--enroll",action="store_true")
    asyncio.run(run(parser.parse_args()))
