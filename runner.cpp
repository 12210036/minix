#include <raylib.h>
#include <vector>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <fstream>

namespace fs = std::filesystem;

// --- PALETA EN ESCALA DE GRISES ---
Color FondoOscuro    = { 18,  18,  18, 255 }; // Gris casi negro (#121212)
Color GrisCaja       = { 38,  38,  38, 255 }; // Gris oscuro para la barra (#262626)
Color ResaltadoGris  = { 220, 220, 220, 255 }; // Gris claro/blanco para elemento activo
Color TextoBlanco    = { 240, 240, 240, 255 }; // Texto general
Color TextoGrisMedio = { 140, 140, 140, 255 }; // Texto secundario / contador
Color TextoGrisAlerta= { 180, 180, 180, 255 }; // Mensajes de alerta

struct AppEntry {
    std::string nombre;
    std::string ejecutable;
};

std::string limpiarComandoExec(const std::string& execRaw) {
    std::string cmd = execRaw;
    size_t pos = cmd.find('%');
    if (pos != std::string::npos) {
        cmd = cmd.substr(0, pos);
    }
    while (!cmd.empty() && (cmd.back() == ' ' || cmd.back() == '\t')) {
        cmd.pop_back();
    }
    return cmd;
}

void escanearDirectorioDesktop(const std::string& ruta, std::vector<AppEntry>& lista) {
    if (!fs::exists(ruta) || !fs::is_directory(ruta)) return;

    for (const auto& entry : fs::directory_iterator(ruta)) {
        if (entry.is_regular_file() && entry.path().extension() == ".desktop") {
            std::ifstream file(entry.path());
            if (!file.is_open()) continue;

            std::string line;
            std::string nombre = "";
            std::string exec = "";
            bool noDisplay = false;
            bool esApp = false;

            while (std::getline(file, line)) {
                if (line.rfind("Type=Application", 0) == 0) esApp = true;
                if (line.rfind("NoDisplay=true", 0) == 0) noDisplay = true;

                if (nombre.empty() && line.rfind("Name=", 0) == 0) {
                    nombre = line.substr(5);
                }
                if (exec.empty() && line.rfind("Exec=", 0) == 0) {
                    exec = limpiarComandoExec(line.substr(5));
                }
            }

            if (esApp && !noDisplay && !nombre.empty() && !exec.empty()) {
                lista.push_back({nombre, exec});
            }
        }
    }
}

std::vector<AppEntry> cargarAplicacionesDesktop() {
    std::vector<AppEntry> lista;

    escanearDirectorioDesktop("/usr/share/applications", lista);

    const char* home = std::getenv("HOME");
    if (home) {
        std::string userPath = std::string(home) + "/.local/share/applications";
        escanearDirectorioDesktop(userPath, lista);
    }

    std::sort(lista.begin(), lista.end(), [](const AppEntry& a, const AppEntry& b) {
        return a.nombre < b.nombre;
    });

    return lista;
}

int main() {
    std::vector<AppEntry> apps = cargarAplicacionesDesktop();

    SetConfigFlags(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TOPMOST);

    int anchoVentana = 800;
    int altoVentana = 600;

    InitWindow(anchoVentana, altoVentana, "Launcher");
    SetTargetFPS(60);

    int monitor = GetCurrentMonitor();
    int posX = (GetMonitorWidth(monitor) - anchoVentana) / 2;
    int posY = (GetMonitorHeight(monitor) - altoVentana) / 2;
    SetWindowPosition(posX, posY);

    std::string input = "";
    int sel = 0;
    int scrollOffset = 0;
    const int maxVisibles = 13;

    auto toLower = [](std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    };

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE)) break;

        // Entrada de teclado
        int k = GetCharPressed();
        while (k > 0) {
            if (k >= 32 && k <= 125) {
                input += (char)k;
                sel = 0;
                scrollOffset = 0;
            }
            k = GetCharPressed();
        }

        if (IsKeyPressed(KEY_BACKSPACE) && !input.empty()) {
            input.pop_back();
            sel = 0;
            scrollOffset = 0;
        }

        // Búsqueda
        std::vector<AppEntry> match;
        std::string query = toLower(input);
        for (const auto& a : apps) {
            if (query.empty() || toLower(a.nombre).find(query) != std::string::npos) {
                match.push_back(a);
            }
        }

        // Navegación
        if (!match.empty()) {
            if (IsKeyPressed(KEY_DOWN)) {
                sel = (sel + 1) % match.size();
            }
            if (IsKeyPressed(KEY_UP)) {
                sel = (sel - 1 + match.size()) % match.size();
            }

            if (sel < scrollOffset) {
                scrollOffset = sel;
            } else if (sel >= scrollOffset + maxVisibles) {
                scrollOffset = sel - maxVisibles + 1;
            }

            if (IsKeyPressed(KEY_ENTER)) {
                std::string cmd = match[sel].ejecutable + " &";
                std::system(cmd.c_str());
                break;
            }
        }

        int margen = 15;
        int anchoCaja = anchoVentana - (margen * 2);

        BeginDrawing();
            ClearBackground(FondoOscuro);

            // 1. Buscador
            DrawRectangle(margen, 15, anchoCaja, 40, GrisCaja);
            DrawText(TextFormat("> %s", input.c_str()), margen + 15, 24, 20, TextoBlanco);

            // 2. Resultados
            if (match.empty()) {
                DrawText("Sin coincidencias", margen + 10, 75, 18, TextoGrisAlerta);
            } else {
                for (int i = 0; i < maxVisibles && (i + scrollOffset) < (int)match.size(); i++) {
                    int indexReal = i + scrollOffset;
                    bool estaSeleccionado = (indexReal == sel);
                    int posY_Fila = 68 + (i * 38);

                    if (estaSeleccionado) {
                        DrawRectangle(margen, posY_Fila, anchoCaja, 34, ResaltadoGris);
                    }

                    // Si la fila está seleccionada, el texto pasa a gris oscuro/negro para contraste
                    Color colorTexto = estaSeleccionado ? FondoOscuro : TextoBlanco;
                    DrawText(match[indexReal].nombre.c_str(), margen + 15, posY_Fila + 7, 18, colorTexto);
                }

                // 3. Contador X/Y
                std::string contador = std::to_string(sel + 1) + "/" + std::to_string(match.size());
                int anchoTextoContador = MeasureText(contador.c_str(), 16);
                int posXContador = (anchoVentana - margen) - anchoTextoContador - 15;
                
                DrawText(contador.c_str(), posXContador, 26, 16, TextoGrisMedio);
            }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
