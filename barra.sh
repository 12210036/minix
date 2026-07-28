#!/bin/bash

# Bucle infinito
while true; do
    # 1. Obtener la batería (si no existe, muestra N/A)
    BAT=$(cat /sys/class/power_supply/BAT0/capacity 2>/dev/null || echo "N/A")
    
    # 2. Obtener la hora actual
    HORA=$(TZ="America/Monterrey" date "+%H:%M:%S")

    BOTON_ARCHIVOS="%{A:thunar:} Archivos %{A}"
        
        # Botón para abrir Firefox
    BOTON_WEB="%{A:firefox:} Navegador %{A}"

    # Botón para abrir Firefox
    BOTON_TERMINAL="%{A:alacritty:} Terminal %{A}"

    # Botón para abrir Firefox
    BOTON_NOTAS="%{A:obsidian:} Notas %{A}"
    
    # 3. Formatear la salida para lemonbar
    # %{l} alinea a la izquierda, %{r} alinea a la derecha
    echo "%{L}${BOTON_ARCHIVOS} | ${BOTON_WEB} | ${BOTON_TERMINAL} | ${BOTON_NOTAS}  %{r}  Batería: $BAT% | $HORA "
    
    # Esperar 1 segundo antes de la siguiente actualización
    sleep 1
done
