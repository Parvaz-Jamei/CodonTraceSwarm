"""Loopback-only real aMQTT process for the test laboratory."""
import asyncio,json,logging,sys
from amqtt.broker import Broker
logging.basicConfig(level=logging.WARNING)
async def main():
    config=json.load(open(sys.argv[1],encoding='utf-8'))
    if config['listeners']['default']['bind'].split(':')[0]!='127.0.0.1':
        raise ValueError('lab broker must be loopback only')
    broker=Broker(config)
    await broker.start()
    print('READY',flush=True)
    try: await asyncio.Event().wait()
    finally: await broker.shutdown()
asyncio.run(main())
