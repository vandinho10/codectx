#include "codectx.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace codectx
{

namespace
{

const char *REPO_DEFAULT = "vandinho10/codectx";
const char *API_HOST = "https://api.github.com";

std::string repo_slug()
{
    if (const char *e = std::getenv("CODECTX_UPDATE_REPO"))
        if (*e)
            return e;
    return REPO_DEFAULT;
}

// ---------- util ----------

std::string aparar(const std::string &s)
{
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a])))
        ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1])))
        --b;
    return s.substr(a, b - a);
}

bool tem_comando(const char *nome)
{
#ifdef _WIN32
    std::string cmd = std::string("where ") + nome + " >nul 2>nul";
#else
    std::string cmd = std::string("command -v ") + nome + " >/dev/null 2>&1";
#endif
    return std::system(cmd.c_str()) == 0;
}

// Executa comando e captura stdout. Retorna false se o processo falhar.
bool capturar_cmd(const std::string &cmd, std::string &out)
{
    out.clear();
#ifdef _WIN32
    FILE *pipe = _popen(cmd.c_str(), "r");
#else
    FILE *pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe)
        return false;
    char buf[4096];
    while (std::fgets(buf, sizeof buf, pipe))
        out += buf;
#ifdef _WIN32
    const int rc = _pclose(pipe);
#else
    const int rc = pclose(pipe);
#endif
    return rc == 0;
}

// ---------- SemVer ----------

struct SemVer
{
    int major = 0;
    int minor = 0;
    int patch = 0;
    bool ok = false;
};

SemVer parse_semver(std::string s)
{
    SemVer v;
    s = aparar(s);
    if (!s.empty() && (s[0] == 'v' || s[0] == 'V'))
        s = s.substr(1);
    // ignora sufixo pré-release (-rc.1) e build (+git)
    const size_t corte = s.find_first_of("-+");
    if (corte != std::string::npos)
        s = s.substr(0, corte);

    int parts[3] = {0, 0, 0};
    int idx = 0;
    std::string cur;
    for (char c : s)
    {
        if (c == '.')
        {
            if (idx >= 3)
                return v;
            try
            {
                parts[idx++] = std::stoi(cur.empty() ? "0" : cur);
            }
            catch (...)
            {
                return v;
            }
            cur.clear();
        }
        else if (std::isdigit(static_cast<unsigned char>(c)))
        {
            cur += c;
        }
        else
        {
            return v;
        }
    }
    if (idx >= 3)
        return v;
    try
    {
        parts[idx] = std::stoi(cur.empty() ? "0" : cur);
    }
    catch (...)
    {
        return v;
    }
    v.major = parts[0];
    v.minor = parts[1];
    v.patch = parts[2];
    v.ok = true;
    return v;
}

int cmp_semver(const SemVer &a, const SemVer &b)
{
    if (a.major != b.major)
        return a.major < b.major ? -1 : 1;
    if (a.minor != b.minor)
        return a.minor < b.minor ? -1 : 1;
    if (a.patch != b.patch)
        return a.patch < b.patch ? -1 : 1;
    return 0;
}

std::string fmt_semver(const SemVer &v)
{
    return std::to_string(v.major) + "." + std::to_string(v.minor) + "." +
           std::to_string(v.patch);
}

// ---------- plataforma / artefato ----------

std::string nome_artefato_esperado()
{
#ifdef _WIN32
#if defined(_M_X64) || defined(__x86_64__) || defined(__amd64__)
    return "codectx-windows-amd64.exe";
#else
    return "codectx-windows-x86.exe";
#endif
#else
    std::string arch;
#if defined(__x86_64__) || defined(__amd64__)
    arch = "x86_64";
#elif defined(__aarch64__) || defined(__arm64__)
    arch = "aarch64";
#elif defined(__arm__) || defined(__ARM_ARCH)
    arch = "arm32";
#else
    // fallback via uname
    std::string u;
    if (capturar_cmd("uname -m", u))
    {
        u = aparar(u);
        if (u == "x86_64" || u == "amd64")
            arch = "x86_64";
        else if (u == "aarch64" || u == "arm64")
            arch = "aarch64";
        else if (u.rfind("arm", 0) == 0)
            arch = "arm32";
    }
#endif
    if (arch.empty())
        return {};
    return "codectx-linux-" + arch;
#endif
}

fs::path caminho_executavel_atual(const char *argv0)
{
#ifdef _WIN32
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH)
        return fs::path(buf);
#else
    std::error_code ec;
    fs::path self = fs::read_symlink("/proc/self/exe", ec);
    if (!ec && !self.empty())
        return self;
#endif
    if (argv0 && *argv0)
    {
        std::error_code ec;
        fs::path p(argv0);
        if (p.is_absolute())
            return p;
        return fs::absolute(p, ec);
    }
    return {};
}

// ---------- HTTP (via curl) ----------

bool garantir_curl()
{
    if (tem_comando("curl"))
        return true;
    std::cerr << "codectx: 'curl' nao encontrado no PATH.\n"
              << "  Instale curl ou baixe manualmente em:\n"
              << "  https://github.com/" << repo_slug() << "/releases/latest\n";
    return false;
}

std::string curl_user_agent()
{
    return std::string("codectx/") + CODECTX_VERSION + " (+https://github.com/" +
           repo_slug() + ")";
}

// GET texto; falha → string vazia e mensagem em stderr.
std::string http_get(const std::string &url)
{
    // -f falha em HTTP>=400; -s silencioso; -S mostra erro; -L segue redirect
    const std::string cmd =
        "curl -fsSL -A '" + curl_user_agent() + "' "
        "-H 'Accept: application/vnd.github+json' "
        "-H 'X-GitHub-Api-Version: 2022-11-28' "
        "'" +
        url + "'";
    std::string body;
    if (!capturar_cmd(cmd, body))
    {
        std::cerr << "codectx: falha ao consultar " << url << "\n";
        return {};
    }
    return body;
}

bool http_download(const std::string &url, const fs::path &dest)
{
    const std::string cmd =
        "curl -fsSL -A '" + curl_user_agent() + "' -o '" + dest.string() + "' '" +
        url + "'";
    const int rc = std::system(cmd.c_str());
    return rc == 0 && fs::exists(dest);
}

// ---------- parse mínimo de JSON da release ----------

// Extrai o valor de string de uma chave de topo simples: "tag_name": "v1.2.3"
std::string json_string_field(const std::string &json, const std::string &key)
{
    const std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos)
        return {};
    pos = json.find(':', pos + needle.size());
    if (pos == std::string::npos)
        return {};
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos)
        return {};
    size_t end = pos + 1;
    while (end < json.size())
    {
        if (json[end] == '\\' && end + 1 < json.size())
        {
            end += 2;
            continue;
        }
        if (json[end] == '"')
            break;
        ++end;
    }
    if (end >= json.size())
        return {};
    return json.substr(pos + 1, end - pos - 1);
}

struct AssetInfo
{
    std::string name;
    std::string url;
};

// Percorre blocos "name":"...","...","browser_download_url":"..."
std::vector<AssetInfo> json_assets(const std::string &json)
{
    std::vector<AssetInfo> out;
    size_t pos = 0;
    while (true)
    {
        const size_t npos = json.find("\"name\"", pos);
        if (npos == std::string::npos)
            break;
        const size_t colon = json.find(':', npos + 6);
        if (colon == std::string::npos)
            break;
        const size_t q1 = json.find('"', colon + 1);
        if (q1 == std::string::npos)
            break;
        const size_t q2 = json.find('"', q1 + 1);
        if (q2 == std::string::npos)
            break;
        const std::string name = json.substr(q1 + 1, q2 - q1 - 1);

        // browser_download_url próximo (dentro de alguns KB)
        const size_t url_key = json.find("\"browser_download_url\"", q2);
        if (url_key == std::string::npos || url_key > q2 + 800)
        {
            pos = q2 + 1;
            continue;
        }
        const size_t uc = json.find(':', url_key + 22);
        const size_t u1 = json.find('"', uc + 1);
        const size_t u2 = json.find('"', u1 + 1);
        if (u1 == std::string::npos || u2 == std::string::npos)
        {
            pos = q2 + 1;
            continue;
        }
        AssetInfo a;
        a.name = name;
        a.url = json.substr(u1 + 1, u2 - u1 - 1);
        out.push_back(std::move(a));
        pos = u2 + 1;
    }
    return out;
}

// ---------- SHA256 ----------

std::string sha256_arquivo(const fs::path &p)
{
    std::string out;
#if defined(_WIN32)
    // certutil -hashfile file SHA256
    if (capturar_cmd("certutil -hashfile \"" + p.string() + "\" SHA256", out))
    {
        // segunda linha costuma ser o hash
        std::istringstream iss(out);
        std::string line;
        std::getline(iss, line); // header
        std::getline(iss, line);
        line = aparar(line);
        // remove espaços que o certutil intercala
        std::string hex;
        for (char c : line)
            if (std::isxdigit(static_cast<unsigned char>(c)))
                hex += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (hex.size() == 64)
            return hex;
    }
#else
    if (tem_comando("sha256sum") && capturar_cmd("sha256sum '" + p.string() + "'", out))
    {
        auto sp = out.find(' ');
        if (sp != std::string::npos)
        {
            std::string hex = aparar(out.substr(0, sp));
            if (hex.size() == 64)
                return hex;
        }
    }
    if (tem_comando("shasum") && capturar_cmd("shasum -a 256 '" + p.string() + "'", out))
    {
        auto sp = out.find(' ');
        if (sp != std::string::npos)
        {
            std::string hex = aparar(out.substr(0, sp));
            if (hex.size() == 64)
                return hex;
        }
    }
#endif
    return {};
}

// SHA256SUMS: linhas "hash  nome" ou "hash *nome"
std::string hash_em_sumas(const std::string &sumas, const std::string &nome)
{
    std::istringstream iss(sumas);
    std::string line;
    while (std::getline(iss, line))
    {
        line = aparar(line);
        if (line.empty() || line[0] == '#')
            continue;
        auto sp = line.find_first_of(" \t");
        if (sp == std::string::npos)
            continue;
        std::string hash = line.substr(0, sp);
        std::string rest = aparar(line.substr(sp));
        if (!rest.empty() && rest[0] == '*')
            rest = rest.substr(1);
        // basename
        auto slash = rest.find_last_of("/\\");
        if (slash != std::string::npos)
            rest = rest.substr(slash + 1);
        if (rest == nome)
        {
            std::string hex;
            for (char c : hash)
                if (std::isxdigit(static_cast<unsigned char>(c)))
                    hex += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return hex;
        }
    }
    return {};
}

// ---------- instalação ----------

bool tornar_executavel(const fs::path &p)
{
#ifndef _WIN32
    std::error_code ec;
    fs::permissions(p,
                    fs::perms::owner_all | fs::perms::group_read | fs::perms::group_exec |
                        fs::perms::others_read | fs::perms::others_exec,
                    fs::perm_options::replace, ec);
    return !ec;
#else
    (void)p;
    return true;
#endif
}

bool substituir_binario(const fs::path &destino, const fs::path &baixado)
{
    std::error_code ec;
    const fs::path backup = destino.string() + ".old";
    const fs::path staged = destino.string() + ".new";

    // Copia para .new ao lado do destino (mesma partição → rename atômico).
    fs::remove(staged, ec);
    ec.clear();
    fs::copy_file(baixado, staged, fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        std::cerr << "codectx: falha ao preparar binario em " << staged.string()
                  << ": " << ec.message() << "\n";
        return false;
    }
    tornar_executavel(staged);

#ifdef _WIN32
    // No Windows o .exe em uso não pode ser sobrescrito de forma confiável.
    // Renomeia o atual para .old e o novo para o nome final.
    fs::remove(backup, ec);
    ec.clear();
    if (fs::exists(destino))
    {
        fs::rename(destino, backup, ec);
        if (ec)
        {
            std::cerr << "codectx: nao foi possivel mover o binario atual "
                      << "(em uso?). Arquivo novo em: " << staged.string() << "\n"
                      << "  Feche processos codectx e renomeie manualmente.\n";
            return false;
        }
    }
    ec.clear();
    fs::rename(staged, destino, ec);
    if (ec)
    {
        std::cerr << "codectx: falha ao ativar novo binario: " << ec.message() << "\n";
        // tenta restaurar
        std::error_code ec2;
        if (fs::exists(backup))
            fs::rename(backup, destino, ec2);
        return false;
    }
    fs::remove(backup, ec);
#else
    // Unix: rename atômico sobre o destino.
    fs::rename(staged, destino, ec);
    if (ec)
    {
        // fallback: tenta via backup
        std::error_code ec2;
        fs::remove(backup, ec2);
        if (fs::exists(destino))
            fs::rename(destino, backup, ec2);
        ec2.clear();
        fs::rename(staged, destino, ec2);
        if (ec2)
        {
            std::cerr << "codectx: falha ao substituir " << destino.string()
                      << ": " << ec2.message() << "\n"
                      << "  Sem permissao? Tente: sudo mv " << baixado.string()
                      << " " << destino.string() << "\n";
            if (fs::exists(backup))
                fs::rename(backup, destino, ec2);
            return false;
        }
        fs::remove(backup, ec2);
    }
#endif
    return true;
}

bool confirmar(const std::string &pergunta)
{
    std::cout << pergunta << " [s/N] " << std::flush;
    std::string resp;
    if (!std::getline(std::cin, resp))
        return false;
    resp = aparar(resp);
    if (resp.empty())
        return false;
    const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(resp[0])));
    return c == 's' || c == 'y';
}

void usage_update()
{
    std::cerr
        << "Uso: codectx update [check] [--yes]\n"
        << "  check   Apenas consulta a versao remota (nao instala)\n"
        << "  --yes   Instala sem perguntar (scripts/CI)\n"
        << "\n"
        << "Fonte: https://github.com/" << repo_slug() << "/releases/latest\n"
        << "Override: CODECTX_UPDATE_REPO=owner/repo\n";
}

struct ReleaseInfo
{
    std::string tag;
    SemVer ver;
    std::vector<AssetInfo> assets;
};

bool buscar_latest(ReleaseInfo &rel)
{
    const std::string url =
        std::string(API_HOST) + "/repos/" + repo_slug() + "/releases/latest";
    const std::string body = http_get(url);
    if (body.empty())
        return false;

    rel.tag = json_string_field(body, "tag_name");
    if (rel.tag.empty())
    {
        std::cerr << "codectx: resposta da API sem tag_name\n";
        return false;
    }
    rel.ver = parse_semver(rel.tag);
    if (!rel.ver.ok)
    {
        std::cerr << "codectx: tag invalida -> " << rel.tag << "\n";
        return false;
    }
    rel.assets = json_assets(body);
    return true;
}

const AssetInfo *achar_asset(const std::vector<AssetInfo> &assets, const std::string &nome)
{
    for (const auto &a : assets)
        if (a.name == nome)
            return &a;
    return nullptr;
}

} // namespace

// Exposto para testes unitários (mesmo TU do parse).
bool update_parse_semver_publico(const std::string &s, int &maj, int &min, int &pat)
{
    const SemVer v = parse_semver(s);
    if (!v.ok)
        return false;
    maj = v.major;
    min = v.minor;
    pat = v.patch;
    return true;
}

int update_cmp_semver_publico(int a0, int a1, int a2, int b0, int b1, int b2)
{
    SemVer a{a0, a1, a2, true};
    SemVer b{b0, b1, b2, true};
    return cmp_semver(a, b);
}

std::string update_nome_artefato_publico() { return nome_artefato_esperado(); }

int comando_update(int argc, char *argv[])
{
    bool so_check = false;
    bool yes = false;

    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "check")
            so_check = true;
        else if (arg == "--yes" || arg == "-y")
            yes = true;
        else if (arg == "-h" || arg == "--help")
        {
            usage_update();
            return EXIT_SUCCESS;
        }
        else
        {
            std::cerr << "codectx: argumento desconhecido em update -> " << arg << "\n";
            usage_update();
            return EXIT_FAILURE;
        }
    }

    const SemVer local = parse_semver(CODECTX_VERSION);
    if (!local.ok)
    {
        std::cerr << "codectx: CODECTX_VERSION invalida -> " << CODECTX_VERSION << "\n";
        return EXIT_FAILURE;
    }

    if (!garantir_curl())
        return EXIT_FAILURE;

    std::cout << "Local:  v" << fmt_semver(local) << "\n";
    std::cout << "Repo:   " << repo_slug() << "\n";
    std::cout << "Consultando releases/latest...\n";

    ReleaseInfo rel;
    if (!buscar_latest(rel))
        return EXIT_FAILURE;

    std::cout << "Remoto: " << rel.tag << "\n";

    const int cmp = cmp_semver(local, rel.ver);
    if (cmp > 0)
    {
        std::cout << "Versao local e mais nova que a release publica.\n";
        return EXIT_SUCCESS;
    }
    if (cmp == 0)
    {
        std::cout << "Ja esta na versao mais recente.\n";
        return EXIT_SUCCESS;
    }

    std::cout << "Atualizacao disponivel: v" << fmt_semver(local) << " -> " << rel.tag
              << "\n";

    if (so_check)
        return EXIT_SUCCESS; // exit 0 = check ok; há update (scripts podem comparar)

    const std::string artefato = nome_artefato_esperado();
    if (artefato.empty())
    {
        std::cerr << "codectx: arquitetura nao suportada para update automatico.\n"
                  << "  Baixe manualmente: https://github.com/" << repo_slug()
                  << "/releases/tag/" << rel.tag << "\n";
        return EXIT_FAILURE;
    }

    const AssetInfo *asset = achar_asset(rel.assets, artefato);
    if (asset == nullptr)
    {
        std::cerr << "codectx: artefato '" << artefato << "' ausente na release "
                  << rel.tag << "\n"
                  << "  Disponiveis:\n";
        for (const auto &a : rel.assets)
            std::cerr << "    - " << a.name << "\n";
        return EXIT_FAILURE;
    }

    std::cout << "Artefato: " << asset->name << "\n";

    if (!yes && !confirmar("Baixar e instalar agora?"))
    {
        std::cout << "Cancelado.\n";
        return EXIT_SUCCESS;
    }

    const fs::path self = caminho_executavel_atual(argv[0]);
    if (self.empty())
    {
        std::cerr << "codectx: nao foi possivel determinar o caminho do executavel.\n";
        return EXIT_FAILURE;
    }
    std::cout << "Destino:  " << self.string() << "\n";

    const fs::path tmp_dir = fs::temp_directory_path();
    const fs::path tmp_bin = tmp_dir / (asset->name + ".download");
    std::error_code ec;
    fs::remove(tmp_bin, ec);

    std::cout << "Baixando...\n";
    if (!http_download(asset->url, tmp_bin))
    {
        std::cerr << "codectx: falha no download de " << asset->url << "\n";
        return EXIT_FAILURE;
    }

    // SHA256SUMS opcional
    if (const AssetInfo *sums = achar_asset(rel.assets, "SHA256SUMS"))
    {
        std::cout << "Verificando SHA256...\n";
        const std::string sumas = http_get(sums->url);
        const std::string esperado = hash_em_sumas(sumas, asset->name);
        if (esperado.empty())
        {
            std::cerr << "codectx: aviso: hash de '" << asset->name
                      << "' nao encontrado em SHA256SUMS (seguindo sem verificar)\n";
        }
        else
        {
            const std::string obtido = sha256_arquivo(tmp_bin);
            if (obtido.empty())
            {
                std::cerr << "codectx: aviso: nao foi possivel calcular SHA256 local "
                          << "(seguindo sem verificar)\n";
            }
            else if (obtido != esperado)
            {
                std::cerr << "codectx: SHA256 nao confere!\n"
                          << "  esperado: " << esperado << "\n"
                          << "  obtido:   " << obtido << "\n";
                fs::remove(tmp_bin, ec);
                return EXIT_FAILURE;
            }
            else
            {
                std::cout << "SHA256 OK.\n";
            }
        }
    }
    else
    {
        std::cout << "SHA256SUMS ausente na release — pulando verificacao.\n";
    }

    tornar_executavel(tmp_bin);

    if (!substituir_binario(self, tmp_bin))
    {
        std::cerr << "Binario baixado em: " << tmp_bin.string() << "\n";
        return EXIT_FAILURE;
    }

    fs::remove(tmp_bin, ec);
    std::cout << "Atualizado para " << rel.tag << ".\n";
    std::cout << "Execute 'codectx --version' para confirmar.\n";
    return EXIT_SUCCESS;
}

} // namespace codectx
