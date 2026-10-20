#include "codectx.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <regex>
#include <system_error>
#include <unordered_set>
#include <vector>

namespace codectx
{

namespace
{

const std::unordered_set<std::string> DIRS_IGNORADOS = {
    ".git", ".env", "node_modules", "vendor", "__pycache__", "dist", "build",
    "venv", ".venv", "env",
    ".next", ".nuxt", "out", "target",
    "bin", "obj", "coverage",
    ".idea", ".vscode",
    "logs", "log", "tmp", "temp",
    ".cache", ".pytest_cache", ".mypy_cache", ".tox", ".gradle",
    ".terraform", ".svn", ".hg", ".svelte-kit"};

// Extensões e nomes de artefatos não-texto / lockfiles / minificados.
const std::string PADROES_IGNORAR_REGEX_STR =
    R"((\.css$|\.jpg$|\.db$|\.db-shm$|\.db-wal$|\.sqlite3?$|\.jpeg$|\.png$|\.gif$|\.svg$|\.webp$|\.ico$|\.zip$|\.rar$|\.tar\.gz$|\.7z$|\.exe$|\.dll$|\.so$|\.bin$|\.o$|\.obj$|\.a$|\.lib$|\.wasm$|\.iso$|\.img$|\.dat$|\.pkl$|\.npy$|\.h5$|\.pb$|\.parquet$|\.pdf$|\.docx?$|\.xlsx?$|\.csv$|\.woff2?$|\.ttf$|\.eot$|\.mp[34]$|\.pyc$|\.class$|\.jar$|\.map$|\.min\.(js|css)$|package-lock\.json$|yarn\.lock$|pnpm-lock\.yaml$|poetry\.lock$|Cargo\.lock$|composer\.lock$|Gemfile\.lock$|go\.sum$|uv\.lock$|\.DS_Store$|\.pem$|\.key$|\.p12$|\.pfx$|\.jks$))";

// Nomes de arquivo sensíveis (basename, case-insensitive via comparação minúscula).
const std::unordered_set<std::string> NOMES_SEGREDO = {
    ".env", ".env.local", ".env.production", ".env.development", ".env.test",
    ".npmrc", ".pypirc", "credentials.json", "service-account.json",
    "id_rsa", "id_dsa", "id_ecdsa", "id_ed25519",
    "secrets.yaml", "secrets.yml", "secrets.json"};

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

std::string para_minusculas(std::string s)
{
    for (char &c : s)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return s;
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
            else
            {
                std::cerr << "Aviso: -o/--output requer um argumento (arquivo de saida)\n";
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
        else if (arg == "--")
        {
            // Fim das opções: restantes são alvos literais.
            estado_atual = ParseState::ALVOS;
            for (++i; i < argc; ++i)
            {
                cfg.alvos.push_back(argv[i]);
                cfg.cli_alvos = true;
            }
            break;
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
              << "  -o, --output [ARQ]    Salva a saida em um arquivo em vez de terminal\n"
              << "  --                    Fim das opcoes; restantes sao alvos literais\n\n"
              << "Configuracao persistente (estilo git config):\n"
              << "  " << prog_name << " config [list|get|set|add|unset|path] [--global|--local]\n"
              << "  Precedencia: CLI > .codectx.conf local > config global > defaults\n\n"
              << "Atualizacao (GitHub Releases, requer curl):\n"
              << "  " << prog_name << " update check   # consulta versao remota\n"
              << "  " << prog_name << " update         # baixa e instala (confirma)\n"
              << "  " << prog_name << " update --yes   # instala sem prompt\n";
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

    if (!cfg_.arquivo_saida.empty())
    {
        std::error_code ec;
        saida_abs_ = fs::absolute(cfg_.arquivo_saida, ec);
    }
}

bool ConcatenadorDeArquivos::parece_binario(const fs::path &p)
{
    std::ifstream f(p, std::ios::binary);
    if (!f.is_open())
        return true;
    char buf[8192];
    f.read(buf, static_cast<std::streamsize>(sizeof buf));
    const auto n = static_cast<std::size_t>(f.gcount());
    return std::memchr(buf, '\0', n) != nullptr;
}

bool ConcatenadorDeArquivos::parece_segredo(const fs::path &p)
{
    const std::string nome = para_minusculas(p.filename().string());
    if (NOMES_SEGREDO.count(nome))
        return true;
    // .env.* (qualquer variante)
    if (nome.size() > 4 && nome.compare(0, 4, ".env") == 0)
        return true;
    // chaves privadas comuns
    if (nome.find("id_rsa") != std::string::npos ||
        nome.find("id_ed25519") != std::string::npos ||
        nome.find("id_ecdsa") != std::string::npos ||
        nome.find("id_dsa") != std::string::npos)
        return true;
    return false;
}

std::string ConcatenadorDeArquivos::caminho_exibicao(const fs::path &arquivo) const
{
    std::error_code ec;
    const fs::path abs = fs::absolute(arquivo, ec);
    if (ec)
        return arquivo.string();
    const fs::path rel = fs::relative(abs, fs::current_path(), ec);
    // generic_string() é portátil (char em todos os alvos); native() no
    // Windows é wstring e quebra .find("..") no cross-compile mingw.
    if (!ec && !rel.empty())
    {
        const std::string rel_s = rel.generic_string();
        if (rel_s.find("..") == std::string::npos)
            return rel.lexically_normal().string();
    }
    return abs.lexically_normal().string();
}

bool ConcatenadorDeArquivos::ja_processado_ou_saida(const fs::path &arquivo)
{
    std::error_code ec;
    if (!saida_abs_.empty())
    {
        if (fs::equivalent(arquivo, saida_abs_, ec) && !ec)
            return true;
        // fallback por caminho canônico se equivalent falhar (arquivo ainda não existe)
        const fs::path cand = fs::absolute(arquivo, ec);
        if (!ec && cand.lexically_normal() == saida_abs_.lexically_normal())
            return true;
    }

    fs::path canon = fs::weakly_canonical(arquivo, ec);
    if (ec)
        canon = fs::absolute(arquivo, ec);
    return processados_.count(canon.lexically_normal().string()) > 0;
}

void ConcatenadorDeArquivos::marcar_processado(const fs::path &arquivo)
{
    std::error_code ec;
    fs::path canon = fs::weakly_canonical(arquivo, ec);
    if (ec)
        canon = fs::absolute(arquivo, ec);
    processados_.insert(canon.lexically_normal().string());
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

bool ConcatenadorDeArquivos::processar_arquivo(const fs::path &arquivo, std::ostream &out, bool forcar)
{
    if (ja_processado_ou_saida(arquivo))
        return false;

    std::string filepath = arquivo.string();
    std::string filename = arquivo.filename().string();
    bool processar = false;

    if (forcar)
    {
        // Alvo explícito: só bloqueia binário e segredos.
        processar = true;
    }
    else if (!cfg_.nomes_permitidos.empty() || tem_extensoes_)
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
                processar = true;
        }
    }
    else
    {
        if (!std::regex_search(filepath, padroes_ignorar_))
            processar = true;
    }

    if (!processar)
        return false;

    if (parece_segredo(arquivo))
        return false;

    if (parece_binario(arquivo))
        return false;

    std::ifstream in(arquivo, std::ios::binary);
    if (!in.is_open())
        return false;

    std::string conteudo((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    marcar_processado(arquivo);

    const std::string caminho = caminho_exibicao(arquivo);

    out << "<><>========================================<><>\n";
    out << "INICIO ARQUIVO: " << caminho << "\n";
    out << "========================================\n";
    out << conteudo;
    if (!conteudo.empty() && conteudo.back() != '\n')
        out << '\n';
    out << "========================================\n";
    out << "    FIM ARQUIVO: " << caminho << "\n";
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
            std::error_code ec_ex;

            if (!fs::exists(alvo_path, ec_ex))
            {
                std::cerr << "Aviso: alvo nao encontrado -> " << alvo_str << "\n";
                continue;
            }

            // Alvo explícito (arquivo ou diretório): NÃO aplicar deve_ignorar
            // na raiz — o usuário pediu aquele caminho de propósito.
            if (fs::is_directory(alvo_path, ec_ex))
            {
                std::vector<fs::path> encontrados;

                if (cfg_.recursivo)
                {
                    std::error_code ec;
                    auto it = fs::recursive_directory_iterator(
                        alvo_path,
                        fs::directory_options::skip_permission_denied,
                        ec);
                    auto end = fs::recursive_directory_iterator();

                    while (it != end)
                    {
                        if (ec)
                        {
                            std::cerr << "Aviso: erro ao iterar -> " << alvo_str
                                      << ": " << ec.message() << "\n";
                            ec.clear();
                            it.increment(ec);
                            continue;
                        }

                        const auto &entrada = *it;
                        std::error_code ec_st;
                        const bool is_dir = entrada.is_directory(ec_st);

                        if (deve_ignorar(entrada.path()))
                        {
                            if (is_dir)
                                it.disable_recursion_pending();
                        }
                        else if (!is_dir && entrada.is_regular_file(ec_st))
                        {
                            encontrados.push_back(entrada.path());
                        }

                        it.increment(ec);
                    }
                }
                else
                {
                    std::error_code ec;
                    auto it = fs::directory_iterator(
                        alvo_path,
                        fs::directory_options::skip_permission_denied,
                        ec);
                    auto end = fs::directory_iterator();

                    while (it != end)
                    {
                        if (ec)
                        {
                            std::cerr << "Aviso: erro ao iterar -> " << alvo_str
                                      << ": " << ec.message() << "\n";
                            ec.clear();
                            it.increment(ec);
                            continue;
                        }

                        const auto &entrada = *it;
                        std::error_code ec_st;
                        if (entrada.is_regular_file(ec_st) && !deve_ignorar(entrada.path()))
                            encontrados.push_back(entrada.path());

                        it.increment(ec);
                    }
                }

                std::sort(encontrados.begin(), encontrados.end());
                for (const auto &p : encontrados)
                    processar_arquivo(p, out, /*forcar=*/false);
            }
            else if (fs::is_regular_file(alvo_path, ec_ex))
            {
                // Arquivo citado explicitamente: força inclusão (bypass -e/-f).
                processar_arquivo(alvo_path, out, /*forcar=*/true);
            }
        }
        catch (const fs::filesystem_error &e)
        {
            std::cerr << "Aviso: erro de sistema de arquivos no alvo -> " << alvo_str
                      << ": " << e.what() << "\n";
        }
        catch (const std::exception &e)
        {
            std::cerr << "Aviso: erro inesperado no alvo -> " << alvo_str
                      << ": " << e.what() << "\n";
        }
    }
}

int ConcatenadorDeArquivos::executar()
{
    if (!cfg_.arquivo_saida.empty())
    {
        const fs::path destino(cfg_.arquivo_saida);
        const fs::path temporario = destino.string() + ".tmp";

        {
            std::ofstream out(temporario, std::ios::binary | std::ios::trunc);
            if (!out.is_open())
            {
                std::cerr << "Erro ao abrir arquivo de saida: " << cfg_.arquivo_saida << "\n";
                return EXIT_FAILURE;
            }
            percorrer_alvos(out);
            if (!out.good())
            {
                std::cerr << "Erro ao gravar arquivo de saida: " << cfg_.arquivo_saida << "\n";
                std::error_code ec_rm;
                fs::remove(temporario, ec_rm);
                return EXIT_FAILURE;
            }
        }

        std::error_code ec;
        fs::rename(temporario, destino, ec);
        if (ec)
        {
            std::cerr << "Erro ao finalizar arquivo de saida: " << ec.message() << "\n";
            std::error_code ec_rm;
            fs::remove(temporario, ec_rm);
            return EXIT_FAILURE;
        }
        std::cout << "Saida salva em: " << cfg_.arquivo_saida << "\n";
    }
    else
    {
        percorrer_alvos(std::cout);
    }

    return EXIT_SUCCESS;
}

} // namespace codectx
