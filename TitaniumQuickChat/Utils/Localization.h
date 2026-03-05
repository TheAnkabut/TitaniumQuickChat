#pragma once
#include <string>
#include <unordered_map>

namespace TitaniumLocalization
{
    inline bool isSpanish = false;

    inline void Initialize(std::shared_ptr<GameWrapper> wrapper)
    {
        isSpanish = (wrapper->GetUILanguage().ToString() == "ESN");
    }

    inline const char* Get(const char* key)
    {
        // Keys that are the same in both languages
        static const std::unordered_map<std::string, const char*> SHARED = {
            {"normal", "Normal"},
            {"simple", "Simple"},
        };

        // Spanish translations
        static const std::unordered_map<std::string, const char*> ES = {
            // Header
            {"join_discord", "¡Haz clic para unirte al Discord!"},
            {"combine", "Combinar (+)"},
            {"progressive", "Progresivo (>)"},
            {"sequence", "Secuencia (-)"},
            {"mode", "Modo:"},
            {"custom_channel_per_qc", "Canal personalizado por QC"},

            // Welcome
            {"welcome_prefix", "Bienvenido a"},
            {"profile_name", "Nombre del perfil:"},
            {"create", "Crear"},

            // Normal Mode
            {"open_playground", "Abrir Playground"},
            {"open_custom_ui", "Abrir Custom UI"},
            {"available", "Disponibles"},
            {"tab_quickchats", "Quick Chats"},
            {"tab_profiles", "Perfiles"},
            {"title_label", "Título:"},
            {"title_tooltip", "Puede que no funcione siempre"},
            {"category_n", "Categoría %d"},
            {"channel_cat_tooltip", "Canal para los 4 QCs de esta categoría"},
            {"channel_slot_tooltip", "Canal: Match/Team/Party"},

            // Simple Mode
            {"select", "Seleccionar..."},
            {"search", "Buscar..."},

            // Playground
            {"playground", "Playground"},
            {"user_keys", "User Keys"},
            {"match_data_tokens", "Match Data Tokens"},
            {"copy", "Copiar"},
            {"delete", "Eliminar"},
            {"shuffle", "Aleatorio"},
            {"amount", "Cantidad:"},
            {"seconds", "Segundos:"},

            // Playground: Token descriptions
            {"desc_mynickname", "Tu nombre en el juego"},
            {"desc_myscore", "Tu puntuación actual"},
            {"desc_mate_name", "Nombre del compañero #%d"},
            {"desc_mate_score", "Puntuación del compañero #%d"},
            {"desc_opp_name", "Nombre del oponente #%d"},
            {"desc_opp_score", "Puntuación del oponente #%d"},
            {"desc_lastgoalby", "Nombre del último goleador"},
            {"desc_lastgoalspeed", "Velocidad del último gol"},
            {"desc_lastsaveby", "Nombre de la última parada"},
            {"desc_goaldiff", "Diferencia de goles (+X, -X, 0)"},
            {"desc_lastmessage", "Último mensaje del chat"},
            {"desc_lastplayername", "Nombre del último remitente"},
            {"desc_remaining", "Tiempo restante (m:ss)"},
            {"desc_overtime", "Tiempo extra (+m:ss)"},
            {"desc_wait", "Esperar X segundos antes del siguiente fragmento"},
            {"desc_upper", "Convertir texto a MAYÚSCULAS"},
            {"desc_lower", "Convertir texto a minúsculas"},
            {"desc_randomize", "Mezcla aleatoria de mayúsculas/minúsculas"},

            // Profile
            {"add", "Añadir"},
            {"rename", "Renombrar"},
            {"copy_share_tooltip", "Copiar código compartido al portapapeles"},
            {"load_share_tooltip", "Cargar un código compartido en el perfil actual"},
            {"profile_created", "Perfil Creado"},
            {"profile_switch", "Cambio de perfil"},
            {"profile_active", "Perfil activo"},
            {"plugin_loaded", "Cargado!"},
            {"copied", "Copiado!"},
            {"profile_prefix", "Perfil: "},

            // Loadout
            {"insert_share_code", "Insertar código compartido"},
            {"code", "Código"},
            {"load", "Cargar"},
            {"paste_first", "¡Pega un código primero!"},
            {"invalid_code", "¡Código inválido!"},
            {"loadout_loaded", "Loadout cargado"},
        };

        // English translations
        static const std::unordered_map<std::string, const char*> EN = {
            // Header
            {"join_discord", "Click to join the Discord!"},
            {"combine", "Combine (+)"},
            {"progressive", "Progressive (>)"},
            {"sequence", "Sequence (-)"},
            {"mode", "Mode:"},
            {"custom_channel_per_qc", "Custom channel per QC"},

            // Welcome
            {"welcome_prefix", "Welcome to"},
            {"profile_name", "Profile name:"},
            {"create", "Create"},

            // Normal Mode
            {"open_playground", "Open Playground"},
            {"open_custom_ui", "Open Custom UI"},
            {"available", "Available"},
            {"tab_quickchats", "Quick Chats"},
            {"tab_profiles", "Profiles"},
            {"title_label", "Title:"},
            {"title_tooltip", "Might not always work"},
            {"category_n", "Category %d"},
            {"channel_cat_tooltip", "Channel for all 4 QCs in this category"},
            {"channel_slot_tooltip", "Channel: Match/Team/Party"},

            // Simple Mode
            {"select", "Select..."},
            {"search", "Search..."},

            // Playground
            {"playground", "Playground"},
            {"user_keys", "User Keys"},
            {"match_data_tokens", "Match Data Tokens"},
            {"copy", "Copy"},
            {"delete", "Delete"},
            {"shuffle", "Shuffle"},
            {"amount", "Amount:"},
            {"seconds", "Seconds:"},

            // Playground: Token descriptions
            {"desc_mynickname", "Your in-game nickname"},
            {"desc_myscore", "Your current match score"},
            {"desc_mate_name", "Teammate #%d name"},
            {"desc_mate_score", "Teammate #%d score"},
            {"desc_opp_name", "Opponent #%d name"},
            {"desc_opp_score", "Opponent #%d score"},
            {"desc_lastgoalby", "Name of last goal scorer"},
            {"desc_lastgoalspeed", "Speed of last goal"},
            {"desc_lastsaveby", "Name of last save maker"},
            {"desc_goaldiff", "Goal difference (+X, -X, 0)"},
            {"desc_lastmessage", "Last chat message received"},
            {"desc_lastplayername", "Name of last message sender"},
            {"desc_remaining", "Time remaining (m:ss)"},
            {"desc_overtime", "Overtime time (+m:ss)"},
            {"desc_wait", "Wait X seconds before next chunk"},
            {"desc_upper", "Convert text to UPPERCASE"},
            {"desc_lower", "Convert text to lowercase"},
            {"desc_randomize", "Random upper/lowercase mix"},

            // Profile
            {"add", "Add"},
            {"rename", "Rename"},
            {"copy_share_tooltip", "Copy share code to clipboard"},
            {"load_share_tooltip", "Load a share code into the current profile"},
            {"profile_created", "Profile Created"},
            {"profile_switch", "Profile switch"},
            {"profile_active", "Active profile"},
            {"plugin_loaded", "Loaded!"},
            {"copied", "Copied!"},
            {"profile_prefix", "Profile: "},

            // Loadout
            {"insert_share_code", "Insert share code"},
            {"code", "Code"},
            {"load", "Load"},
            {"paste_first", "Paste a code first!"},
            {"invalid_code", "Invalid code!"},
            {"loadout_loaded", "Loadout loaded"},
        };

        // Check shared first (same in both languages)
        auto shared = SHARED.find(key);
        if (shared != SHARED.end())
        {
            return shared->second;
        }

        const auto& dict = isSpanish ? ES : EN;
        auto it = dict.find(key);
        return (it != dict.end()) ? it->second : key;
    }
}

#define TL(key) TitaniumLocalization::Get(key)
