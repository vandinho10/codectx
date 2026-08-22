#include "codectx.hpp"

#include <cstdint>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <regex>
#include <unordered_set>

namespace codectx
{

namespace
{

const std::unordered_set<std::string> DIRS_IGNORADOS = {
    ".git", ".gitignore", ".env", "node_modules", "vendor", "__pycache__", "dist", "build",
    "venv", ".venv", "env",
    ".next", ".nuxt", "out", "target",
    "bin", "obj", "coverage",
    ".idea", ".vscode",
    "logs", "log", "tmp", "temp"};

const std::string PADROES_IGNORAR_REGEX_STR = R"((\.css$|\.jpg$|\.db$|\.db-shm$|\.db-wal$|\.sqlite$|\.jpeg$|\.png$|\.gif$|\.svg$|\.webp$|\.ico$|\.zip$|\.rar$|\.tar\.gz$|\.7z$|\.exe$|\.dll$|\.so$|\.bin$|\.o$|\.obj$|\.a$|\.lib$|\.wasm$|\.iso$|\.img$|\.dat$|\.pkl$|\.npy$|\.h5$|\.pb$|\.parquet$|\.pdf$|\.docx?$|\.xlsx?$|\.csv$|\.woff2?$|\.ttf$|\.eot$|\.mp[34]$|\.pyc$|\.class$|\.jar$|\.map$|\.min\.(js|css)$|package-lock\.json$|yarn\.lock$|pnpm-lock\.yaml$|poetry\.lock$|\.DS_Store$))";

std::string regex_escape(const std::string &s)
{
    static const std::string especiais = R"(.^$|()[]{}\*+?)";
    std::string out;
    out.reserve(s.size() * 2);
    for (char c : s)
    {
        if (especiais.find(c) != std::string::npos)
            out += '\\';
        out += c;
    }
    return out;
}

enum class ParseState
{
    ALVOS,
    EXT,
    FILE_EXACT,
    ADD,
    NOT
};

} // namespace

bool parse_args(int argc, char *argv[], Config &cfg)
{
    ParseState estado_atual = ParseState::ALVOS;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help")
        {
            cfg.ajuda = true;
            return true;
        }
        else if (arg == "-v" || arg == "--version")
        {
            cfg.versao = true;
            return true;
        }
        else if (arg == "--no-rec")
        {
            cfg.recursivo = false;
            cfg.cli_no_rec = true;
        }
        else if (arg == "-o" || arg == "--output")
        {
            if (i + 1 < argc)
            {
                cfg.arquivo_saida = argv[++i];
                cfg.cli_output = true;
            }
            estado_atual = ParseState::ALVOS;
        }
        else if (arg == "-e" || arg == "--ext")
        {
            estado_atual = ParseState::EXT;
        }
        else if (arg == "-f" || arg == "--file")
        {
            estado_atual = ParseState::FILE_EXACT;
        }
        else if (arg == "-a" || arg == "--add")
        {
            estado_atual = ParseState::ADD;
        }
        else if (arg == "-n" || arg == "--not")
        {
            estado_atual = ParseState::NOT;
        }
        else if (arg == "-t" || arg == "--target")
        {
            estado_atual = ParseState::ALVOS;
        }
        else if (!arg.empty() && arg[0] == '-' && arg.length() > 1)
        {
            std::cerr << "Aviso: flag desconhecida ignorada -> " << arg << "\n";
        }
        else
        {
            switch (estado_atual)
            {
                case ParseState::EXT:
                {
                    std::error_code ec;
                    fs::path possivel_alvo(arg);
                    if (possivel_alvo.has_parent_path() || fs::exists(possivel_alvo, ec))
                    {
                        cfg.alvos.push_back(arg);
                        cfg.cli_alvos = true;
                    }
                    else
                    {
                        if (!arg.empty() && arg[0] == '.')
                            arg = arg.substr(1);
                        cfg.extensoes_permitidas.push_back(arg);
                        cfg.cli_ext = true;
                    }
                    break;
                }
                case ParseState::FILE_EXACT:
                {
                    fs::path filepath(arg);
                    cfg.nomes_permitidos.push_back(filepath.filename().string());
                    cfg.cli_file = true;
                    if (filepath.has_parent_path())
                    {
                        cfg.alvos.push_back(arg);
                        cfg.cli_alvos = true;
                    }
                    break;
                }
                case ParseState::ADD:
                    cfg.arquivos_adicionais.push_back(arg);
                    cfg.cli_add = true;
                    break;
                case ParseState::NOT:
                    cfg.lista_ignorar_manual.push_back(arg);
                    cfg.cli_not = true;
                    break;
                case ParseState::ALVOS:
                default:
                    cfg.alvos.push_back(arg);
                    cfg.cli_alvos = true;
                    break;
            }
        }
    }

    if (cfg.alvos.empty())
    {
        cfg.alvos.push_back(".");
    }

    for (const auto &add_arq : cfg.arquivos_adicionais)
    {
        cfg.alvos.push_back(add_arq);
    }

    if (!cfg.arquivo_saida.empty())
    {
        fs::path caminho_saida(cfg.arquivo_saida);
        cfg.lista_ignorar_manual.push_back(caminho_saida.filename().string());
    }

    if (!cfg.extensoes_permitidas.empty())
    {
        cfg.tem_extensoes = true;
    }

    return true;
}

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
              << "  -v, --version         Exibe a versao\n"
              << "  --no-rec              Nao busca recursivamente (apenas pasta atual)\n"
              << "  -o, --output [ARQ]    Salva a saida em um arquivo em vez de terminal\n";
}

ConcatenadorDeArquivos::ConcatenadorDeArquivos(const Config &cfg)
    : cfg_(cfg)
{
    if (!cfg_.extensoes_permitidas.empty())
    {
        tem_extensoes_ = true;
        std::string pattern = "\\.(";
        for (size_t i = 0; i < cfg_.extensoes_permitidas.size(); ++i)
        {
            pattern += regex_escape(cfg_.extensoes_permitidas[i]);
            if (i + 1 < cfg_.extensoes_permitidas.size())
                pattern += "|";
        }
        pattern += ")$";
        try
        {
            regex_extensoes_ = std::regex(pattern, std::regex_constants::icase);
        }
        catch (const std::regex_error &e)
        {
            std::cerr << "Aviso: filtro de extensoes invalido, ignorado -> " << e.what() << "\n";
            tem_extensoes_ = false;
        }
    }

    try
    {
        padroes_ignorar_ = std::regex(PADROES_IGNORAR_REGEX_STR, std::regex_constants::icase);
    }
    catch (const std::regex_error &e)
    {
        std::cerr << "Aviso: padrao de exclusao invalido -> " << e.what() << "\n";
        padroes_ignorar_ = std::regex("a^");
    }
}

bool ConcatenadorDeArquivos::deve_ignorar(const fs::path &path) const
{
    std::string nome = path.filename().string();

    if (nome == "." || nome == "..")
        return false;

    if (DIRS_IGNORADOS.count(nome))
        return true;

    for (const auto &ign : cfg_.lista_ignorar_manual)
    {
        if (nome == ign)
            return true;
    }

    return false;
}

bool ConcatenadorDeArquivos::processar_arquivo(const fs::path &arquivo, std::ostream &out)
{
    std::string filepath = arquivo.string();
    std::string filename = arquivo.filename().string();
    bool processar = false;

    if (!cfg_.nomes_permitidos.empty() || tem_extensoes_)
    {
        if (!cfg_.nomes_permitidos.empty())
        {
            for (const auto &nome : cfg_.nomes_permitidos)
            {
                if (filename == nome)
                {
                    processar = true;
                    break;
                }
            }
        }

        if (!processar && tem_extensoes_)
        {
            if (std::regex_search(filepath, regex_extensoes_))
            {
                processar = true;
            }
        }
    }
    else
    {
        if (!std::regex_search(filepath, padroes_ignorar_))
        {
            processar = true;
        }
    }

    if (!processar)
        return false;

    std::ifstream in(arquivo);
    if (!in.is_open())
        return false;

    out << "<><>========================================<><>\n";
    out << "INICIO ARQUIVO: " << fs::absolute(arquivo).lexically_normal().string() << "\n";
    out << "========================================\n";

    std::error_code ec;
    uintmax_t tamanho = fs::file_size(arquivo, ec);
    if (!ec && tamanho > 0)
    {
        out << in.rdbuf();
    }

    if (out.tellp() > 0)
        out << "\n";

    out << "========================================\n";
    out << "    FIM ARQUIVO: " << fs::absolute(arquivo).lexically_normal().string() << "\n";
    out << "<><>========================================<><>\n\n";

    return true;
}

void ConcatenadorDeArquivos::percorrer_alvos(std::ostream &out)
{
    for (const auto &alvo_str : cfg_.alvos)
    {
        try
        {
            fs::path alvo_path(alvo_str);

            if (!fs::exists(alvo_path))
            {
                std::cerr << "Aviso: alvo nao encontrado -> " << alvo_str << "\n";
                continue;
            }

            if (deve_ignorar(alvo_path))
            {
                continue;
            }

            if (fs::is_directory(alvo_path))
            {
                if (cfg_.recursivo)
                {
                    auto it = fs::recursive_directory_iterator(alvo_path, fs::directory_options::skip_permission_denied);
                    auto end = fs::recursive_directory_iterator();

                    for (; it != end; ++it)
                    {
                        const auto &entrada = *it;
                        bool is_dir = fs::is_directory(entrada);

                        if (deve_ignorar(entrada.path()))
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
                            if (!deve_ignorar(entrada.path()))
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
        catch (const fs::filesystem_error &e)
        {
            std::cerr << "Aviso: erro de sistema de arquivos no alvo -> " << alvo_str << ": " << e.what() << "\n";
        }
        catch (const std::exception &e)
        {
            std::cerr << "Aviso: erro inesperado no alvo -> " << alvo_str << ": " << e.what() << "\n";
        }
    }
}

int ConcatenadorDeArquivos::executar()
{
    if (!cfg_.arquivo_saida.empty())
    {
        std::ofstream out(cfg_.arquivo_saida);
        if (!out.is_open())
        {
            std::cerr << "Erro ao abrir arquivo de saida: " << cfg_.arquivo_saida << "\n";
            return EXIT_FAILURE;
        }
        percorrer_alvos(out);
        std::cout << "Saida salva em: " << cfg_.arquivo_saida << "\n";
    }
    else
    {
        percorrer_alvos(std::cout);
    }

    return EXIT_SUCCESS;
}

} // namespace codectx
