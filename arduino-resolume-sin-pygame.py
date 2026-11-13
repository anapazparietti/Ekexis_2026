import serial
import time
from pythonosc import udp_client

client = udp_client.SimpleUDPClient("127.0.0.1", 7000)  # Puerto de entrada OSC de Resolume

COLUMNAS = {
    "inicio": 1,
    "seleccionando": 2,
    "nivis": 3,
    "sahra": 4,
    "jangal": 5,
    "skog": 6,
}

arduino = serial.Serial('COM3', 9600, timeout=1)
time.sleep(2)  # Tiempo de reinicio del Arduino al abrir el puerto

try:
    while True:
        data = arduino.readline().decode('utf-8').strip()
        if data in COLUMNAS:
            client.send_message(f"/composition/columns/{COLUMNAS[data]}/connect", 1)
            print(f"Recibido: {data} -> Columna {COLUMNAS[data]}")
except KeyboardInterrupt:
    print("Conexión cerrada.")
finally:
    arduino.close()