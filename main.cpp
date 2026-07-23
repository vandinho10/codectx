#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <filesystem>
#include <unordered_set>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

// ================= CONFIGURAÇÃO DE EXCLUSÃO =================

const std::unordered_set<std::string> DIRS_IGNORADOS = {
    ".git", ".gitignore", ".env", "node_modules", "vendor", "__pycache__", "dist", "build",
    "venv", ".venv", "env",
    ".next", ".nuxt", "out", "target",
    "bin", "obj", "coverage",
    ".idea", ".vscode",
    "logs", "log", "tmp", "temp"};

const std::string PADROES_IGNORAR_REGEX_STR = R"((\.css$|\.jpg$|\.db$|\.db-shm$|\.db-wal$|\.sqlite$|\.jpeg$|\.png$|\.gif$|\.svg$|\.webp$|\.ico$|\.zip$|\.rar$|\.tar\.gz$|\.7z$|\.exe$|\.dll$|\.so$|\.pdf$|\.docx?$|\.xlsx?$|\.csv$|\.woff2?$|\.ttf$|\.eot$|\.mp[34]$|\.pyc$|\.class$|\.jar$|\.map$|\.min\.(js|css)$|package-lock\.json$|yarn\.lock$|pnpm-lock\.yaml$|poetry\.lock$|\.DS_Store$))";

// ============================================================

class ConcatenadorDeArquivos
{
private:
    std::vector<std::string> lista_ignorar_manual;
    std::vector<std::string> extensoes_permitidas;
    std::vector<std::string> nomes_permitidos;
    std::vector<std::string> arquivos_adicionais;
    std::vector<std::string> alvos;
    bool recursivo = true;
    std::string arquivo_saida;

    std::regex regex_extensoes;
    bool tem_extensoes = false;
    std::regex padroes_ignorar;

    // NOVO: Controle de Estados de Leitura
    enum class ParseState {
        ALVOS,         // Lê como alvo principal (comportamento padrão)
        EXT,           // Lê como extensão permitida (-e)
        FILE_EXACT,    // Lê como arquivo exato permitido (-f)
        ADD,           // Lê como arquivo avulso (-a)
        NOT            // Lê como arquivo ignorado (-n)
    };

    void exibir_ajuda(const char *prog_name)
    {
        std::cout << "Uso: " << prog_name << " [OPCOES] [ALVOS]\n\n"
                  << "Opcoes que aceitam multiplos argumentos em sequencia:\n"
                  << "  -e, --ext [EXTS...]   Filtra por extensao (ex: -e php js)\n"
                  << "  -f, --file [NOMES...] Filtra por arquivo exato (ex: -f Makefile Dockerfile)\n"
                  << "  -a, --add [ALVOS...]  Adiciona arquivos/pastas preservando a base (.)\n"
                  << "  -n, --not [NOMES...]  Ignora arquivos ou pastas especificas\n"
                  << "  -t, --target [ALVOS]  Volta a ler os argumentos como ALVOS principais\n\n"
                  << "Opcoes simples:\n"
                  << "  -h, --help            Exibe esta ajuda\n"
                  << "  --no-rec              Nao busca recursivamente (apenas pasta atual)\n"
                  << "  -o, --output [ARQ]    Salva a saida em um arquivo em vez de terminal\n";
        exit(0);
    }

    bool deve_ignorar(const fs::path &path, bool is_dir)
    {
        std::string nome = path.filename().string();

        if (nome == "." || nome == "..")
            return false;

        if (DIRS_IGNORADOS.count(nome))
            return true;

        for (const auto &ign : lista_ignorar_manual)
        {
            if (nome == ign)
                return true;
        }

        return false;
    }

    void processar_arquivo(const fs::path &arquivo, std::ostream &out)
    {
        std::string filepath = arquivo.string();
        std::string filename = arquivo.filename().string();
        bool processar = false;

        if (!nomes_permitidos.empty() || tem_extensoes)
        {
            if (!nomes_permitidos.empty())
            {
                for (const auto &nome : nomes_permitidos)
                {
                    if (filename == nome)
                    {
                        processar = true;
                        break;
                    }
                }
            }

            if (!processar && tem_extensoes)
            {
                if (std::regex_search(filepath, regex_extensoes))
                {
                    processar = true;
                }
            }
        }
        else
        {
            if (!std::regex_search(filepath, padroes_ignorar))
            {
                processar = true;
            }
        }

        if (!processar) return;

        std::ifstream in(arquivo);
        if (!in.is_open())
            return;

        out << "<><>========================================<><>\n";
        out << "INICIO ARQUIVO: " << fs::absolute(arquivo).lexically_normal().string() << "\n";
        out << "========================================\n";

        if (fs::file_size(arquivo) > 0)
        {
            out << in.rdbuf();
        }
        else
        {
            out << "";
        }

        if (out.tellp() > 0)
            out << "\n";

        out << "========================================\n";
        out << "    FIM ARQUIVO: " << fs::absolute(arquivo).lexically_normal().string() << "\n";
        out << "<><>========================================<><>\n\n";
    }

    void percorrer_alvos(std::ostream &out)
    {
        for (const auto &alvo_str : alvos)
        {
            fs::path alvo_path(alvo_str);

            if (!fs::exists(alvo_path))
            {
                std::cerr << "Aviso: alvo nao encontrado -> " << alvo_str << "\n";
                continue;
            }

            if (deve_ignorar(alvo_path, fs::is_directory(alvo_path)))
            {
                continue;
            }

            if (fs::is_directory(alvo_path))
            {
                if (recursivo)
                {
                    auto it = fs::recursive_directory_iterator(alvo_path, fs::directory_options::skip_permission_denied);
                    auto end = fs::recursive_directory_iterator();

                    for (; it != end; ++it)
                    {
                        const auto &entrada = *it;
                        bool is_dir = fs::is_directory(entrada);

                        if (deve_ignorar(entrada.path(), is_dir))
                        {
                            if (is_dir)
                            {
                                it.disable_recursion_pending();
                            }
                        }
                        else if (!is_dir && fs::is_regular_file(entrada))
                        {
                            processar_arquivo(entrada.path(), out);
                        }
                    }
                }
                else
                {
                    auto it = fs::directory_iterator(alvo_path, fs::directory_options::skip_permission_denied);
                    auto end = fs::directory_iterator();
                    for (; it != end; ++it)
                    {
                        const auto &entrada = *it;
                        bool is_dir = fs::is_directory(entrada);

                        if (!is_dir && fs::is_regular_file(entrada))
                        {
                            if (!deve_ignorar(entrada.path(), false))
                            {
                                processar_arquivo(entrada.path(), out);
                            }
                        }
                    }
                }
            }
            else if (fs::is_regular_file(alvo_path))
            {
                processar_arquivo(alvo_path, out);
            }
        }
    }

public:
    void parse_args(int argc, char *argv[])
    {
        ParseState estado_atual = ParseState::ALVOS;

        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                exibir_ajuda(argv[0]);
            }
            else if (arg == "--no-rec") {
                recursivo = false;
            }
            else if (arg == "-o" || arg == "--output") {
                if (i + 1 < argc) {
                    arquivo_saida = argv[++i];
                }
                estado_atual = ParseState::ALVOS;
            }
            else if (arg == "-e" || arg == "--ext") { estado_atual = ParseState::EXT; }
            else if (arg == "-f" || arg == "--file") { estado_atual = ParseState::FILE_EXACT; }
            else if (arg == "-a" || arg == "--add") { estado_atual = ParseState::ADD; }
            else if (arg == "-n" || arg == "--not") { estado_atual = ParseState::NOT; }
            else if (arg == "-t" || arg == "--target") { estado_atual = ParseState::ALVOS; }
            else if (!arg.empty() && arg[0] == '-' && arg.length() > 1) {
                std::cerr << "Aviso: flag desconhecida ignorada -> " << arg << "\n";
            }
            else
            {
                switch (estado_atual)
                {
                    case ParseState::EXT:
                        if (!arg.empty() && arg[0] == '.') arg = arg.substr(1);
                        extensoes_permitidas.push_back(arg);
                        break;
                    case ParseState::FILE_EXACT: {
                        // FIX: Extrai apenas o nome do arquivo (ex: "processa.plugin.zsh")
                        fs::path filepath(arg);
                        nomes_permitidos.push_back(filepath.filename().string());

                        // FIX UX: Se o usuário passou um caminho (contém diretórios),
                        // adiciona o caminho aos alvos para garantir que ele seja encontrado e lido.
                        if (filepath.has_parent_path()) {
                            alvos.push_back(arg);
                        }
                        break;
                    }
                    case ParseState::ADD:
                        arquivos_adicionais.push_back(arg);
                        break;
                    case ParseState::NOT:
                        lista_ignorar_manual.push_back(arg);
                        break;
                    case ParseState::ALVOS:
                    default:
                        alvos.push_back(arg);
                        break;
                }
            }
        }

        // Se nenhum alvo foi setado (e nenhum alvo indireto veio do -f com caminhos absolutos)
        if (alvos.empty()) {
            alvos.push_back(".");
        }

        for (const auto& add_arq : arquivos_adicionais) {
            alvos.push_back(add_arq);
        }

        if (!arquivo_saida.empty()) {
            fs::path caminho_saida(arquivo_saida);
            lista_ignorar_manual.push_back(caminho_saida.filename().string());
        }

        if (!extensoes_permitidas.empty()) {
            tem_extensoes = true;
            std::string pattern = "\\.(";
            for (size_t i = 0; i < extensoes_permitidas.size(); ++i) {
                pattern += extensoes_permitidas[i];
                if (i + 1 < extensoes_permitidas.size()) pattern += "|";
            }
            pattern += ")$";
            regex_extensoes = std::regex(pattern, std::regex_constants::icase);
        }

        padroes_ignorar = std::regex(PADROES_IGNORAR_REGEX_STR, std::regex_constants::icase);
    }

    void executar()
    {
        if (!arquivo_saida.empty())
        {
            std::ofstream out(arquivo_saida);
            if (!out.is_open())
            {
                std::cerr << "Erro ao abrir arquivo de saida: " << arquivo_saida << "\n";
                return;
            }
            percorrer_alvos(out);
            std::cout << "Saida salva em: " << arquivo_saida << "\n";
        }
        else
        {
            percorrer_alvos(std::cout);
        }
    }
};

int main(int argc, char *argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    ConcatenadorDeArquivos app;
    app.parse_args(argc, argv);
    app.executar();
    return 0;
}