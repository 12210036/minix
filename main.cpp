#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

// -----------------------mantente enfocado-------------

int manejador_errores_x11(Display* dpy, XErrorEvent* ev) {
    return 0;
}


enum Workspaces {
    WS_1 = 0,
    WS_2,
    WS_3,
    WS_4,
    CANTIDAD_WS
};

struct VentanaWM {
    Window id;
    bool es_flotante;
    int fx, fy;
    unsigned int fancho, falto;
};

std::vector<VentanaWM> escritorios[CANTIDAD_WS];
int ws_actual = WS_1;

// Variables globales
Window win_moviendose = None;
int start_x = 0, start_y = 0, win_x = 0, win_y = 0;
unsigned int win_w = 0, win_h = 0;
int boton_presionado = 0;

void ejecutar_comando(const char* cmd, const char* arg1 = nullptr, const char* arg2 = nullptr, const char* arg3 = nullptr) {
    if (fork() == 0) {
        if (fork() == 0) {
            setsid();
            if (arg3) {
                execlp(cmd, cmd, arg1, arg2, arg3, nullptr);
            } else if (arg2) {
                execlp(cmd, cmd, arg1, arg2, nullptr);
            } else if (arg1) {
                execlp(cmd, cmd, arg1, nullptr);
            } else {
                execlp(cmd, cmd, nullptr);
            }
            _exit(0);
        }
        _exit(0);
    }
}

// Helper para ignorar estados de NumLock / CapsLock
void registrar_tecla(Display* dpy, KeyCode code, unsigned int mod, Window root) {
    if (code == 0) return;
    unsigned int modifiers[] = {
        mod,
        mod | Mod2Mask,             // Con NumLock
        mod | LockMask,             // Con CapsLock
        mod | Mod2Mask | LockMask   // Con ambos
    };
    for (unsigned int m : modifiers) {
        XGrabKey(dpy, code, m, root, False, GrabModeAsync, GrabModeAsync);
    }
}

unsigned long obtener_color_hex(Display* dpy, const std::string& hex_str) {
    XColor color;
    Colormap cmap = DefaultColormap(dpy, DefaultScreen(dpy));
    XParseColor(dpy, cmap, hex_str.c_str(), &color);
    XAllocColor(dpy, cmap, &color);
    return color.pixel;
}


void organizar_mosaico(Display* dpy) {
    auto& todas = escritorios[ws_actual];
    if (todas.empty()) return;

    int monitor_ancho = DisplayWidth(dpy, DefaultScreen(dpy));
    int monitor_alto = DisplayHeight(dpy, DefaultScreen(dpy));

    int alto_barra = 17;
    int gap = 5;

    int area_x = gap;
    int area_y = alto_barra + gap;
    int area_ancho = monitor_ancho - (gap * 2);
    int area_alto = monitor_alto - alto_barra - (gap * 2);

    std::vector<Window> tiling_wins;
    for (const auto& v : todas) {
        if (v.es_flotante) {
            XMoveResizeWindow(dpy, v.id, v.fx, v.fy, v.fancho, v.falto);
            XRaiseWindow(dpy, v.id);
        } else {
            tiling_wins.push_back(v.id);
        }
    }

    int n = tiling_wins.size();
    if (n == 0) {
        XFlush(dpy);
        return;
    }

    if (n == 1) {
        XMoveResizeWindow(dpy, tiling_wins[0], area_x, area_y, (unsigned int)area_ancho, (unsigned int)area_alto);
    }
    else {
        int master_ancho = (area_ancho * 55) / 100;
        int stack_ancho = area_ancho - master_ancho - gap;

        XMoveResizeWindow(dpy, tiling_wins[0], area_x, area_y, (unsigned int)master_ancho, (unsigned int)area_alto);

        int num_stack = n - 1;
        int stack_x = area_x + master_ancho + gap;
        int stack_ventana_alto = (area_alto - (gap * (num_stack - 1))) / num_stack;

        for (int i = 1; i < n; ++i) {
            int stack_y = area_y + ((i - 1) * (stack_ventana_alto + gap));
            XMoveResizeWindow(dpy, tiling_wins[i], stack_x, stack_y, (unsigned int)stack_ancho, (unsigned int)stack_ventana_alto);
        }
    }
    XFlush(dpy);
}

void cambiar_workspace(Display* dpy, int nuevo_ws) {
    if (nuevo_ws == ws_actual) return;

    for (const auto& v : escritorios[ws_actual]) {
        XUnmapWindow(dpy, v.id);
    }

    for (const auto& v : escritorios[nuevo_ws]) {
        XMapWindow(dpy, v.id);
    }

    ws_actual = nuevo_ws;
    organizar_mosaico(dpy);

    if (!escritorios[ws_actual].empty()) {
        Window ultima_win = escritorios[ws_actual].back().id;
        XRaiseWindow(dpy, ultima_win);
        XSetInputFocus(dpy, ultima_win, RevertToPointerRoot, CurrentTime);
    } else {
        XSetInputFocus(dpy, DefaultRootWindow(dpy), RevertToPointerRoot, CurrentTime);
    }
    XFlush(dpy);
}

void enviar_a_workspace(Display* dpy, Window w, int destino_ws) {
    if (destino_ws == ws_actual) return;

    auto& actual_list = escritorios[ws_actual];
    auto it = std::find_if(actual_list.begin(), actual_list.end(), [w](const VentanaWM& v) { return v.id == w; });

    if (it != actual_list.end()) {
        VentanaWM copia = *it;
        actual_list.erase(it);
        escritorios[destino_ws].push_back(copia);
        XUnmapWindow(dpy, w);
        organizar_mosaico(dpy);
        XFlush(dpy);
    }
}

void alternar_flotante(Display* dpy, Window w) {
    auto& lista = escritorios[ws_actual];
    for (auto& v : lista) {
        if (v.id == w) {
            v.es_flotante = !v.es_flotante;
            if (v.es_flotante) {
                int monitor_ancho = DisplayWidth(dpy, DefaultScreen(dpy));
                int monitor_alto = DisplayHeight(dpy, DefaultScreen(dpy));
                v.fancho = 800;
                v.falto = 500;
                v.fx = (monitor_ancho - v.fancho) / 2;
                v.fy = (monitor_alto - v.falto) / 2;
            }
            organizar_mosaico(dpy);
            break;
        }
    }
}

int main() {
    signal(SIGCHLD, SIG_IGN);

    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return 1;

    XSetErrorHandler(manejador_errores_x11);

    Window root = DefaultRootWindow(dpy);

    unsigned long color_escritorio = obtener_color_hex(dpy, "#04b864");
    XSetWindowBackground(dpy, root, color_escritorio);
    XClearWindow(dpy, root);
    XFlush(dpy);

    Cursor cursor = XCreateFontCursor(dpy, XC_left_ptr);
    XDefineCursor(dpy, root, cursor);

    XSelectInput(dpy, root, SubstructureRedirectMask | SubstructureNotifyMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask);

    // --- Atajos estándar con Alt (Mod1Mask) ---
    KeyCode t_code = XKeysymToKeycode(dpy, XK_t);
    registrar_tecla(dpy, t_code, Mod1Mask, root);

    KeyCode q_code = XKeysymToKeycode(dpy, XK_q);
    registrar_tecla(dpy, q_code, Mod1Mask, root);

    KeyCode c_code = XKeysymToKeycode(dpy, XK_c);
    registrar_tecla(dpy, c_code, Mod1Mask, root);

    KeyCode n_code = XKeysymToKeycode(dpy, XK_n);
    registrar_tecla(dpy, n_code, Mod1Mask, root);

    KeyCode f_code = XKeysymToKeycode(dpy, XK_f);
    registrar_tecla(dpy, f_code, Mod1Mask, root);

    KeySym teclas_ws[] = {XK_1, XK_2, XK_3, XK_4};
    for (int i = 0; i < 4; ++i) {
        KeyCode code = XKeysymToKeycode(dpy, teclas_ws[i]);
        registrar_tecla(dpy, code, Mod1Mask, root);
        registrar_tecla(dpy, code, Mod1Mask | ShiftMask, root);
    }

    KeyCode up_code   = XKeysymToKeycode(dpy, XK_Up);
    KeyCode down_code = XKeysymToKeycode(dpy, XK_Down);
    KeyCode m_code    = XKeysymToKeycode(dpy, XK_m);

    registrar_tecla(dpy, up_code, Mod1Mask, root);
    registrar_tecla(dpy, down_code, Mod1Mask, root);
    registrar_tecla(dpy, m_code, Mod1Mask, root);

    XGrabButton(dpy, Button1, Mod1Mask, root, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);
    XGrabButton(dpy, Button3, Mod1Mask, root, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);

    XEvent ev;

    while (true) {
        XNextEvent(dpy, &ev);

        switch (ev.type) {
            case MapRequest: {
                Window w = ev.xmaprequest.window;

                XClassHint ch;
                if (XGetClassHint(dpy, w, &ch)) {
                    std::string class_name = ch.res_name ? ch.res_name : "";
                    if (ch.res_name) XFree(ch.res_name);
                    if (ch.res_class) XFree(ch.res_class);

                    if (class_name == "lemonbar") {
                        unsigned long fondo_color = obtener_color_hex(dpy, "#20222c");
                        XSetWindowBackground(dpy, w, fondo_color);
                        XClearWindow(dpy, w);
                        XMapWindow(dpy, w);
                        XFlush(dpy);
                        break;
                    }
                }

                XWindowAttributes wa;
                XGetWindowAttributes(dpy, w, &wa);
                if (wa.override_redirect) {
                    XMapWindow(dpy, w);
                    XFlush(dpy);
                    break;
                }

                auto& ws_lista = escritorios[ws_actual];
                auto it = std::find_if(ws_lista.begin(), ws_lista.end(), [w](const VentanaWM& v) { return v.id == w; });
                if (it == ws_lista.end()) {
                    ws_lista.push_back({w, false, 100, 100, 800, 500});
                }

                XMapWindow(dpy, w);
                XSelectInput(dpy, w, EnterWindowMask);

                organizar_mosaico(dpy);
                XSetInputFocus(dpy, w, RevertToPointerRoot, CurrentTime);
                XFlush(dpy);
                break;
            }
            case DestroyNotify: {
                Window w = ev.xdestroywindow.window;
                bool cambio = false;

                for (int i = 0; i < CANTIDAD_WS; ++i) {
                    auto& lista = escritorios[i];
                    auto it = std::remove_if(lista.begin(), lista.end(), [w](const VentanaWM& v) { return v.id == w; });
                    if (it != lista.end()) {
                        lista.erase(it, lista.end());
                        if (i == ws_actual) cambio = true;
                    }
                }

                if (cambio) organizar_mosaico(dpy);
                break;
            }
            case EnterNotify: {
                XEnterWindowEvent *ee = &ev.xcrossing;
                if (ee->window != None && ee->window != root && ee->mode == NotifyNormal) {
                    XSetInputFocus(dpy, ee->window, RevertToPointerRoot, CurrentTime);
                }
                break;
            }

            case ButtonPress: {
                if (ev.xbutton.subwindow != None && ev.xbutton.subwindow != root) {
                    auto& lista = escritorios[ws_actual];
                    for (auto& v : lista) {
                        if (v.id == ev.xbutton.subwindow && v.es_flotante) {
                            win_moviendose = v.id;
                            boton_presionado = ev.xbutton.button;
                            start_x = ev.xbutton.x_root;
                            start_y = ev.xbutton.y_root;
                            win_x = v.fx;
                            win_y = v.fy;
                            win_w = v.fancho;
                            win_h = v.falto;
                            XRaiseWindow(dpy, win_moviendose);
                            break;
                        }
                    }
                }
                break;
            }
            case MotionNotify: {
                if (win_moviendose != None) {
                    int diff_x = ev.xmotion.x_root - start_x;
                    int diff_y = ev.xmotion.y_root - start_y;

                    auto& lista = escritorios[ws_actual];
                    for (auto& v : lista) {
                        if (v.id == win_moviendose) {
                            if (boton_presionado == Button1) {
                                v.fx = win_x + diff_x;
                                v.fy = win_y + diff_y;
                                XMoveWindow(dpy, v.id, v.fx, v.fy);
                            }
                            else if (boton_presionado == Button3) {
                                v.fancho = std::max(100u, (unsigned int)(win_w + diff_x));
                                v.falto = std::max(100u, (unsigned int)(win_h + diff_y));
                                XResizeWindow(dpy, v.id, v.fancho, v.falto);
                            }
                            break;
                        }
                    }
                    XFlush(dpy);
                }
                break;
            }
            case ButtonRelease: {
                win_moviendose = None;
                boton_presionado = 0;
                break;
            }
            case KeyPress: {
                KeySym keysym = XLookupKeysym(&ev.xkey, 0);

                if (keysym == XK_Up && (ev.xkey.state & Mod1Mask)) {
                    ejecutar_comando("pactl", "set-sink-volume", "@DEFAULT_SINK@", "+5%");
                }
                else if (keysym == XK_Down && (ev.xkey.state & Mod1Mask)) {
                    ejecutar_comando("pactl", "set-sink-volume", "@DEFAULT_SINK@", "-5%");
                }
                else if (keysym == XK_m && (ev.xkey.state & Mod1Mask)) {
                    ejecutar_comando("pactl", "set-sink-mute", "@DEFAULT_SINK@", "toggle");
                }

                else if (keysym >= XK_1 && keysym <= XK_4) {
                    int target_ws = keysym - XK_1;

                    if (ev.xkey.state & ShiftMask) {
                        Window focused_win;
                        int revert_to;
                        XGetInputFocus(dpy, &focused_win, &revert_to);

                        if (focused_win != None && focused_win != root && focused_win != PointerRoot) {
                            enviar_a_workspace(dpy, focused_win, target_ws);
                        }
                    }
                    else {
                        cambiar_workspace(dpy, target_ws);
                    }
                }
                else if (ev.xkey.keycode == f_code && (ev.xkey.state & Mod1Mask)) {
                    Window focused_win;
                    int revert_to;
                    XGetInputFocus(dpy, &focused_win, &revert_to);

                    if (focused_win != None && focused_win != root && focused_win != PointerRoot) {
                        alternar_flotante(dpy, focused_win);
                    }
                }
                else if (ev.xkey.keycode == t_code && (ev.xkey.state & Mod1Mask)) {
                    ejecutar_comando("alacritty");
                }
                else if (ev.xkey.keycode == n_code && (ev.xkey.state & Mod1Mask)) {
                    ejecutar_comando("firefox");
                }
                else if (ev.xkey.keycode == q_code && (ev.xkey.state & Mod1Mask)) {
                    XCloseDisplay(dpy);
                    return 0;
                }
                else if (ev.xkey.keycode == c_code && (ev.xkey.state & Mod1Mask)) {
                    Window focused_win;
                    int revert_to;
                    XGetInputFocus(dpy, &focused_win, &revert_to);

                    if (focused_win != None && focused_win != root && focused_win != PointerRoot) {
                        XKillClient(dpy, focused_win);
                        XFlush(dpy);
                    }
                }
                break;
            }
        }
    }
}
