#include <raylib.h>
#include <vector>
#include <string>
#include <cstdlib>
#include <filesystem>
#include <algorithm>
#include <fstream>

namespace fs = std::filesystem;

struct AppEntry {
    std::string nombre;
    std::string ejecutable;
};

// Limpia el comando 'Exec' de los archivos .desktop (quita %u, %f, etc.)
std::string limpiarComandoExec(const std::string& execRaw) {
    std::string cmd = execRaw;
    size_t pos = cmd.find('%');
    if (pos != std::string::npos) {
        cmd = cmd.substr(0, pos);
    }
    // Eliminar espacios al final
    while (!cmd.empty() && (cmd.back() == ' ' || cmd.back() == '\t')) {
        cmd.pop_back();
    }
    return cmd;
}

// Escanea un directorio en busca de archivos .desktop
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

            // Guardar solo si es una aplicación válida y visible en menús
            if (esApp && !noDisplay && !nombre.empty() && !exec.empty()) {
                lista.push_back({nombre, exec});
            }
        }
    }
}

// Carga las apps de los directorios estándar de Linux
std::vector<AppEntry> cargarAplicacionesDesktop() {
    std::vector<AppEntry> lista;

    escanearDirectorioDesktop("/usr/share/applications", lista);

    // También buscar en el home del usuario (~/.local/share/applications)
    const char* home = std::getenv("HOME");
    if (home) {
        std::string userPath = std::string(home) + "/.local/share/applications";
        escanearDirectorioDesktop(userPath, lista);
    }

    // Ordenar alfabéticamente por nombre
    std::sort(lista.begin(), lista.end(), [](const AppEntry& a, const AppEntry& b) {
        return a.nombre < b.nombre;
    });

    return lista;
}

int main() {
    // 1. Escanear aplicaciones .desktop al iniciar
    std::vector<AppEntry> apps = cargarAplicacionesDesktop();

    // 2. Configurar ventana de Raylib (Flotante)
    SetConfigFlags(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TOPMOST);
    
    int anchoVentana = 500;
    int altoVentana = 300;
    
    InitWindow(anchoVentana, altoVentana, "Launcher");
    SetTargetFPS(60);

    // Centrar en pantalla
    int monitor = GetCurrentMonitor();
    int posX = (GetMonitorWidth(monitor) - anchoVentana) / 2;
    int posY = (GetMonitorHeight(monitor) - altoVentana) / 2;
    SetWindowPosition(posX, posY);

    std::string input = "";
    int sel = 0;
    int scrollOffset = 0;
    const int maxVisibles = 9;

    // Convertir string a minúsculas para búsqueda insensible a mayúsculas
    auto toLower = [](std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    };

    // 3. Loop principal
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE)) break;

        // Capturar texto
        int k = GetCharPressed();
        while (k > 0) {
            if (k >= 32 && k <= 125) { 
                input += (char)k; 
                sel = 0;
                scrollOffset = 0;
            }
            k = GetCharPressed();
        }

        // Borrar texto
        if (IsKeyPressed(KEY_BACKSPACE) && !input.empty()) { 
            input.pop_back(); 
            sel = 0;
            scrollOffset = 0;
        }

        // Filtrar apps por Nombre o Comando (Insensible a mayúsculas)
        std::vector<AppEntry> match;
        std::string query = toLower(input);
        for (const auto& a : apps) {
            if (query.empty() || toLower(a.nombre).find(query) != std::string::npos) {
                match.push_back(a);
            }
        }

        // Navegación con teclado
        if (!match.empty()) {
            if (IsKeyPressed(KEY_DOWN)) {
                sel = (sel + 1) % match.size();
            }
            if (IsKeyPressed(KEY_UP)) {
                sel = (sel - 1 + match.size()) % match.size();
            }

            // Ajuste de Scroll automático
            if (sel < scrollOffset) {
                scrollOffset = sel;
            } else if (sel >= scrollOffset + maxVisibles) {
                scrollOffset = sel - maxVisibles + 1;
            }

            // Ejecutar con ENTER
            if (IsKeyPressed(KEY_ENTER)) {
                std::string cmd = match[sel].ejecutable + " &";
                std::system(cmd.c_str());
                break;
            }
        }

        // 4. Renderizado
        BeginDrawing();
            ClearBackground(GetColor(0x1e1e2eff)); // Catppuccin Base

            // Caja de búsqueda
            DrawRectangle(10, 10, 480, 35, GetColor(0x313244ff));
            DrawText(TextFormat("> %s", input.c_str()), 20, 18, 20, RAYWHITE);

            // Lista de resultados
            if (match.empty()) {
                DrawText("Sin coincidencias", 20, 60, 16, RED);
            } else {
                for (int i = 0; i < maxVisibles && (i + scrollOffset) < (int)match.size(); i++) {
                    int indexReal = i + scrollOffset;
                    Color c = (indexReal == sel) ? GetColor(0xcba6f7ff) : RAYWHITE;
                    
                    // Fondo resaltado si está seleccionado
                    if (indexReal == sel) {
                        DrawRectangle(10, 55 + (i * 35), 480, 30, GetColor(0x45475a80));
                    }

                    // Dibuja el Nombre de la Aplicación
                    DrawText(match[indexReal].nombre.c_str(), 20, 60 + (i * 35), 18, c);
                }

                // Contador de elementos
                std::string contador = std::to_string(sel + 1) + "/" + std::to_string(match.size());
                DrawText(contador.c_str(), 420, 18, 16, GRAY);
            }
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
