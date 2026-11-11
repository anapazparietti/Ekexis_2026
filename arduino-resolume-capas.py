#IMPORTANTE
#El código se debe ejecutar por la terminal de sistema
#1) Abrir simbolo de sistema
#2) Escribir:
#  cd ruta del archivo
#3) ENTER y escribir:
#python arduino-resolume.py

#----------librerias estandar de python
import random 
import time

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

# True únicamente si NO hay Arduino: Pygame asume el control de los estados
modo_mouse = serial_conn is None

#---------CONSTANTES DE TIEMPO Y CAPAS (modificar aquí para ajustar la experiencia)
CAPA_INICIAL = 5                     # capa de la columna 2 que se activa al entrar al cuadrado
CAPAS_AL_AZAR = [4, 3, 2, 1]         # capas candidatas al cumplirse el temporizador
SEGUNDOS_EN_CAPA_INICIAL = 25        # tiempo antes de elegir una capa al azar
SEGUNDOS_RETORNO = 10                # tiempo de espera antes de volver a la columna 1
    
# --- CONFIGURACIÓN PYGAME ---
pygame.init()
screen = pygame.display.set_mode((900, 640))
pygame.display.set_caption("Control Resolume OSC - Pygame")

#--------cuadrado_blanco interfaz de Pygame
posX = 900/2
posY = 640/2
rectW = 150
rectH = 150
cuadrado_blanco = pygame.Rect(int(posX - rectW / 2),int(posY - rectH / 2),rectW,rectH)
#-------------cuadrado_rojo

#------------VARIABLES DE ESTADO
run = True

# Momento en que se activó la capa 5 (inicio del temporizador de 25 s)
inicio_capa_inicial = None

# Evita seleccionar varias capas al azar después de los 25 segundos
capa_azar_ya_elegida = False

# Guarda si el mouse estaba dentro del cuadrado_blanco en el ciclo anterior
mouse_estaba_dentro = False

# Capa seleccionada aleatoriamente: 4, 3, 2 o 1
capa_azar_actual = None

# Momento en que el mouse salió del cuadrado_blanco
inicio_espera_retorno = None

# Columna que está activa actualmente
columna_actual = None

# Capa que está activa actualmente (None cuando se está en la columna 1)
capa_actual = None

#--------------------------------------
#----Funcion para reiniciar un clip y reproducirlo desde 0
def reiniciar_clip(capa_num, clip_num):
    osc_address = f"/composition/layers/{capa_num}/clips/{clip_num}/transport/position/behaviour/playdirection"
    client.send_message(osc_address, 2) #2 = reproducción hacia adelante, desde el inicio del clip

#----Funcion para activar una columna
def disparar_columna(col_num, origen=""): # "" se fija si la señal proviene del mouse o de arduino
    global columna_actual, capa_actual
    osc_address = f"/composition/columns/{col_num}/connect"
    client.send_message(osc_address, 1)
    reiniciar_clip(1, col_num) #el clip de la columna se reproduce desde 0
    if col_num == 1:
        capa_actual = None #al volver a la columna 1 ya no hay ninguna capa activa
    columna_actual = col_num
    print(f">> [{origen}] Disparando Columna {col_num} a Resolume")

#----Funcion para activar una capa
def disparar_capa(capa_num, origen=""):
    global capa_actual
    osc_address = f"/composition/layers/{capa_num}/clips/2/connect"
    client.send_message(osc_address,1)
    reiniciar_clip(capa_num, 2) #el clip de la capa se reproduce desde 0
    capa_actual = capa_num
    print(f">>[{origen}] Disparando Capa {capa_num} a Resolume")

# ---------- REPRODUCIR COLUMNA 1 AL INICIAR ----------
disparar_columna(1,origen="Inicio del programa")

while run:
    #1. Dibujar interfaz en pygame
    screen.fill((0))
    pygame.draw.rect(screen, (255,255,255), cuadrado_blanco)
    
    # 2. Leer datos del Arduino en tiempo real (si está conectado)
    if serial_conn and serial_conn.in_waiting > 0:
        try:
            linea = serial_conn.readline().decode('utf-8').strip() #Lectura en una línea, conversión de bytes a texto (.decode) eliminar (.strip)
            if linea == "inicio":
                disparar_columna(1, origen="Arduino: inicio")
            elif linea == "seleccionando":
                disparar_capa(5, origen="Arduino: seleccionando")
            elif linea == "nivis":
                disparar_capa(4, origen="Arduino: nivis")
            elif linea == "sahra":
                disparar_capa(3, origen="Arduino: sahra")
            elif linea == "jangal":
                disparar_capa(2, origen="Arduino: jangal")
            elif linea == "skog":
                disparar_capa(1, origen="Arduino: skog")
        except Exception as e:
            print(f">>Error leyendo Arduino: {e}")

    # 3. Procesamiento de eventos de Pygame (Mouse y Cierre)
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            run = False

        elif event.type == pygame.MOUSEMOTION and modo_mouse:
            mouse_pos = event.pos
            mouse_dentro = cuadrado_blanco.collidepoint(mouse_pos)

            # El mouse entró al cuadrado_blanco
            if mouse_dentro and not mouse_estaba_dentro:
                print(f">>Inicio de temporizador de {SEGUNDOS_EN_CAPA_INICIAL} s en capa {CAPA_INICIAL}")
                capa_azar_ya_elegida = False

                # Cancelar espera anterior
                inicio_espera_retorno = None
                capa_azar_actual = None

                disparar_capa(CAPA_INICIAL, origen="Posición del Mouse")
                # El temporizador comienza en el instante exacto de la activación de la capa 5
                inicio_capa_inicial = time.monotonic()

            # El mouse salió del cuadrado_blanco
            elif not mouse_dentro and mouse_estaba_dentro:
                inicio_capa_inicial = None
                capa_azar_ya_elegida = False

                # Si ya se activó una capa al azar (4, 3, 2 o 1),
                # esperar 10 segundos antes de ir a la columna 1
                if capa_actual in CAPAS_AL_AZAR:
                    inicio_espera_retorno = time.monotonic()

                    print(
                        f">> Mouse fuera del cuadrado_blanco. "
                        f"Capa {capa_actual} permanecerá "
                        f"{SEGUNDOS_RETORNO} segundos."
                    )

                else:
                    # Si todavía estaba en la capa 5, volver inmediatamente
                    disparar_columna(1,origen="Salida del cuadrado_blanco")

            mouse_estaba_dentro = mouse_dentro

    # 4. Verificar si pasaron 25 segundos en la capa 5
    if (modo_mouse 
        and capa_actual == CAPA_INICIAL 
        and inicio_capa_inicial is not None 
        and not capa_azar_ya_elegida):
        
        tiempo_en_capa_inicial = (time.monotonic() - inicio_capa_inicial)
        
        if tiempo_en_capa_inicial >= SEGUNDOS_EN_CAPA_INICIAL:
            client.send_message("/composition/layers/5/clear", 1)
            capa_azar_actual = random.choice(CAPAS_AL_AZAR)
            disparar_capa(capa_azar_actual, origen=f"Temporizador de {SEGUNDOS_EN_CAPA_INICIAL} segundos-python")
            capa_azar_ya_elegida = True

    # 5. Si el mouse salió después de seleccionar
    # una capa aleatoria, esperar 10 segundos
    if (modo_mouse and inicio_espera_retorno is not None):

        tiempo_fuera = (time.monotonic() - inicio_espera_retorno)

        if tiempo_fuera >= SEGUNDOS_RETORNO:
            disparar_columna(1,origen=f"{SEGUNDOS_RETORNO} segundos fuera del cuadrado_blanco-python")
            inicio_espera_retorno = None
            capa_azar_actual = None

    # 6. Actualizar pantalla
    pygame.display.flip()


# ---------- CIERRE LIMPIO ----------
if serial_conn:
    serial_conn.close()

pygame.quit()

print(">> Programa finalizado.")