#----------librerias estandar de python
import random 
import time
import threading

#----------librerias externas (REQUIEREN INSTALAR DESDE SÍMBOLO DE SISTEMA o cd)
#---------------
#1.tener Python 3.8 o superior
#2. escribir en la cd la siguiente linea
# pip install pygame
#extra: para verificar que se instalo correctamente, escribir la siguiente linea en cd
# python -m pygame.examples.aliens      
import pygame #solo para probar que funcione la conexión con resolume
#-----------------
#1.escribir en la cd la siguiente linea
# pip install pyserial  
import serial #comunicacion con Arduino por el puerto serie
#----------------
#1.escribir en la cd la siguiente linea
# pip install python-osc   
from pythonosc import  udp_client
from pythonosc import dispatcher
from pythonosc import osc_server

#---CONFIGURACION OSC para enviar datos a resolume---
IP = "127.0.0.1" #localhost
PORT = 7000 #resolume port
client = udp_client.SimpleUDPClient(IP, PORT)

#---CONFIGURACION OSC para recibir msj de resolume
OSC_RECEIVE_IP = "127.0.0.1"
OSC_RECEIVE_PORT = 8000
POSICION_CLIP_ADDRESS = (
    "/composition/layers/1/clips/2/transport/position" #verifica el clip que debe estar en la columna 2, seleccionando aura
)
# Se considera que el clip terminó cuando llega a este valor.
END_THRESHOLD = 0.99

#----CONFIGURACION SERIAL (ARDUINO)----
PORT_ARDUINO = "COM4" #Reemplazar por tu puerto real (ver en Arduino)
BAUD_RATE = 9600 #velocidad de transmision, debe ser identica a la declarada en Serial.begin(9600) del sketch de Arduino
serial_conn = None
#graceful degradation: el programa intenta conectarse al arduino en cierto tiempo (timeout=0.01). Si falla, se captura con except y el programa continua funcionando con el mouse
try:
    serial_conn = serial.Serial(PORT_ARDUINO, BAUD_RATE, timeout=0.01)
    print(f">>Conectado existosamente al Arduino en el puerto {PORT_ARDUINO}")
except Exception as e: #guarda el error en la variable e
    print(f">>ADVERTENCIA: No se pudo conectar al Arduino ({e}) El sistema funcionará conectando con mouse")
    
# --- CONFIGURACIÓN PYGAME ---
pygame.init()
screen = pygame.display.set_mode((900, 640))
pygame.display.set_caption("Control Resolume OSC - Pygame")

#--------Variables para dibujar un cuadrado en la interfaz de Pygame
posX = 900/2
posY = 640/2
rectW = 150
rectH = 150
# Crear el cuadrado
cuadrado = pygame.Rect(
    int(posX - rectW / 2),
    int(posY - rectH / 2),
    rectW,
    rectH
)

#------------VARIABLES DE ESTADO
run = True

# True mientras la columna 2 está reproduciendo el clip
seleccionando = False

# True cuando Resolume informa que el clip llegó al final
clip_fin_pendiente = False

# Evita procesar varias veces el mismo final
fin_ya_procesado = False

# Momento en que el mouse entró a la columna 2
inicio_columna_2 = None

# Evita seleccionar varias columnas después de los 30 segundos
columna_2_ya_cambio = False

# Guarda la última columna disparada por la lógica del mouse
ultima_columna_mouse = None

# Guarda si el mouse estaba dentro del cuadrado en el ciclo anterior
mouse_estaba_dentro = False

# Columna seleccionada aleatoriamente: 3, 4, 5 o 6
columna_azar_actual = None

# Momento en que el mouse salió del cuadrado
inicio_espera_retorno = None

# Columna que está activa actualmente
columna_actual = None

# Protege las variables compartidas entre el hilo OSC
# y el hilo principal de Pygame
estado_lock = threading.Lock()

#--------------------------------------
#----Funcion para activar una columna
def disparar_columna(col_num, origen=""): # "" se fija si la señal proviene del mouse o de arduino
    global columna_actual
    osc_address = f"/composition/columns/{col_num}/connect"
    client.send_message(osc_address, 1)
    if col_num == 1:
        client.send_message("/composition/layers/1/clips/1/transport/position/behaviour/playdirection",2)
    columna_actual = col_num
    print(f">> [{origen}] Disparando Columna {col_num} a Resolume")

#----Funcion para activar una capa
def disparar_capa(capa_num, origen=""):
    global capa_actual
    osc_address = f"/composition/layer/{capa_num}/clips/2/connect"
    client.send_message(osc_address,1)
    capa_actual = capa_num
    print(f">>[{origen}] Disparando Capa {capa_num} a Resolume")

# ---------- REPRODUCIR COLUMNA 1 AL INICIAR ----------
disparar_columna(
    1,
    origen="Inicio del programa"
)

while run:
    #1. Dibujar interfaz en pygame
    screen.fill((0))
    pygame.draw.rect(screen, (255,255,255), cuadrado)
    
    # 2. Leer datos del Arduino en tiempo real (si está conectado)
    if serial_conn and serial_conn.in_waiting > 0:
        try:
            linea = serial_conn.readline().decode('utf-8').strip() #Lectura en una línea, conversión de bytes a texto (.decode) eliminar (.strip)
            if linea == "inicio":
                disparar_columna(1, origen="Arduino: inicio")
            elif linea == "seleccionando":
                disparar_columna(2, origen="Arduino: seleccionando")
            elif linea == "nivis":
                disparar_columna(3, origen="Arduino: nivis")
            elif linea == "sahra":
                disparar_columna(4, origen="Arduino: sahra")
            elif linea == "jangal":
                disparar_columna(5, origen="Arduino: jangal")
            elif linea == "skog":
                disparar_columna(6, origen="Arduino: skog")
        except Exception as e:
            print(f">>Error leyendo Arduino: {e}")
        
    # 3. Procesamiento de eventos de Pygame (Mouse y Cierre)
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            run = False

        elif event.type == pygame.MOUSEMOTION:
            mouse_pos = event.pos
            mouse_dentro = cuadrado.collidepoint(mouse_pos)

            # El mouse entró al cuadrado
            if mouse_dentro and not mouse_estaba_dentro:
                seleccionando = True
                inicio_columna_2 = time.monotonic()
                columna_2_ya_cambio = False

                # Cancelar espera anterior
                inicio_espera_retorno = None
                columna_azar_actual = None

                disparar_columna(2,origen="Posición del Mouse")

    # El mouse salió del cuadrado
            elif not mouse_dentro and mouse_estaba_dentro:
                seleccionando = False
                inicio_columna_2 = None
                columna_2_ya_cambio = False

        # Si está en columna 3, 4, 5 o 6,
        # esperar 10 segundos antes de ir a la columna 1
                if columna_actual in (3, 4, 5, 6):
                    inicio_espera_retorno = time.monotonic()

                    print(
                        f">> Mouse fuera del cuadrado. "
                        f"Columna {columna_actual} permanecerá "
                        "10 segundos."
                    )

                else:
            # Si estaba en columna 2, volver inmediatamente
                    disparar_columna(1,origen="Salida del cuadrado")

            mouse_estaba_dentro = mouse_dentro

    # 4. Verificar si pasaron x segundos en la columna 2
    if (
        columna_actual ==2
        and inicio_columna_2 is not None
        and not columna_2_ya_cambio
    ):

        tiempo_en_columna_2 = (
            time.monotonic() - inicio_columna_2
        )
        print(">>Inicio de temporizador en columna 2")
        
        if tiempo_en_columna_2 >= 30:

            columna_azar_actual = random.choice([3, 4, 5, 6])

            disparar_columna(
                columna_azar_actual,origen="Temporizador de 30 segundos-python")

            columna_2_ya_cambio = True

    # 5. Si el mouse salió después de seleccionar
    # una columna aleatoria, esperar 10 segundos
    if inicio_espera_retorno is not None:

        tiempo_fuera = (
            time.monotonic() - inicio_espera_retorno
        )

        if tiempo_fuera >= 10:

            disparar_columna(1,origen="10 segundos fuera del cuadrado-python")

            inicio_espera_retorno = None
            columna_azar_actual = None

    # 6. Actualizar pantalla
    pygame.display.flip()


# ---------- CIERRE LIMPIO ----------
if serial_conn:
    serial_conn.close()

pygame.quit()

print(">> Programa finalizado.")            
