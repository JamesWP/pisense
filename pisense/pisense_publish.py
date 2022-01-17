import paho.mqtt.client as mqtt
import argparse
import os
import logging
from . import bme280

DEFAULT_HOST="localhost"
DEFAULT_PORT=1883
DEFAULT_CLIENT_ID="pisense-probe"
DEFAULT_TOPIC_TEMPLATE="raspberrypi/%s"

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default=os.environ.get("MQTT_BROKER_HOST", DEFAULT_HOST))
    parser.add_argument("--port", type=int, default=os.environ.get("MQTT_BROKER_PORT", DEFAULT_PORT))
    parser.add_argument("--client_id", default=os.environ.get("MQTT_CLIENT_ID", DEFAULT_CLIENT_ID))
    parser.add_argument("--topic_template", default=os.environ.get("MQTT_TOPIC_TEMPLATE", DEFAULT_TOPIC_TEMPLATE))

    return parser.parse_args()

def on_connect(client, userdata, flags, rc):
    logger = logging.getLogger(__name__)
    logger.info("Connection started %s", rc)

def on_publish(client, userdata, mid):
    logger = logging.getLogger(__name__)
    logger.info("Published %s", mid)

def main():
    logger = logging.getLogger(__name__)
    logger.info("STARTING")

    args = parse_args()

    client = mqtt.Client(client_id=args.client_id)
    client.on_connect = on_connect
    client.on_publish = on_publish
    client.enable_logger(logger=logging.getLogger('paho_mqtt'))

    client.connect(args.host, port=args.port)
    client.loop()

    sensors = bme280.get_sensor_data_for_publish()   
 
    for sensor, value in sensors:
        message_info = client.publish(args.topic_template % sensor, payload=value, qos=0, retain=True)
        message_info.wait_for_publish()

if __name__ == '__main__':
    logging.basicConfig(level=logging.DEBUG, handlers=[logging.StreamHandler()])
    main()
