# minix

Un Window Manager minimalista mixto (tiling y floating) para X11 escrito en C++.

## Requisitos

- compilador de C++ (`g++`)
- X11 librerias de desarrollo (`libx11` en Arch, `libX11-devel` en Void)
- barra lemonbar

## Compilación

```bash
g++ main.cpp -o minix -lX11
```
## tutorial

cuando tengas el archivo binario (minix o el nombre que sea) tu lo tienes que poner en el archivo `~/.xinitrc` donde se tiene que ver algo haci

```
#!/bin/sh
export GTK_USE_PORTAL=0
export QT_NO_PORTAL=1
export MOZ_X11_EGL=1
export XDG_DEACTIVE_PORTALS=1


(sleep 1 && ~/carpeta-repo/barra.sh | lemonbar -p -n lemonbar -g 1366x15+0+0 -f "Monospace:size=9" | xargs -I {} sh -c "{} &") &
# Arrancar el WM

//si lo usas xd
pulseaudio -D &

exec dbus-run-session /home/tu_usuario/carpeta_repo/el_binario
```
## instalacion de dependencia
en este caso fue desarrollado en void, entonces usaremos `xbps-install` para las instalaciones, cualquier otra cosa deben usar el administrador de paquetes de tu respectiva distribucion

### (obligatorio)instalar lemonbar-xft: 
```
git clone https://github.com/KryptosLogic/lemonbar-xft.git
cd lemonbar-xft
make
sudo make install
```
### (obligatorio)instalar devel-X11:
```
sudo xbps-install -S libX11-devel
```

### (opcional)instalar firefox:
```
sudo xbps-install -S firefox
```
