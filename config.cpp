#include "codectx.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <set>

namespace codectx
{

namespace
{

const std::set<std::string> CHAVES_VALIDAS = {
    "alvos", "ext", "file", "add", "not", "output", "no-rec"};

const char *NOME_ARQUIVO_LOCAL = ".codectx";

std::string aparar(const std::string &s)
{
    size_t ini = 0, fim = s.size();
    while (ini < fim && (s[ini] == ' ' || s[ini] == '\t' || s[ini] == '\r'))
        ++ini;
    while (fim > ini && (s[fim - 1] == ' ' || s[fim - 1] == '\t' || s[fim - 1] == '\r'))
        --fim;
    return s.substr(ini, fim - ini);
}

std::string minusculas(std::string s)
{
    for (char &c : s)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return s;
}

std::vector<std::string> *busca_linear(
    std::vector<std::pair<std::string, std::vector<std::string>>> &entradas,
    const std::string &chave)
{
    for (auto &par : entradas)
        if (par.first == chave)
            return &par.second;
    return nullptr;
}

} // namespace

// ------------------------------- NivelConfig -------------------------------

bool NivelConfig::vazia() const { return entradas.empty(); }

const std::vector<std::string> *NivelConfig::busca(const std::string &chave) const
{
    for (const auto &par : entradas)
        if (par.first == chave)
            return &par.second;
    return nullptr;
}

void NivelConfig::define(const std::string &chave, std::vector<std::string> valores)
{
    if (auto *existente = busca_linear(entradas, chave))
    {
        *existente = std::move(valores);
        return;
    }
    entradas.emplace_back(chave, std::move(valores));
}

void NivelConfig::acrescenta(const std::string &chave,
                             const std::vector<std::string> &valores)
{
    if (auto *existente = busca_linear(entradas, chave))
    {
        for (const auto &v : valores)
            if (std::find(existente->begin(), existente->end(), v) == existente->end())
                existente->push_back(v);
        return;
    }
    entradas.emplace_back(chave, valores);
}

void NivelConfig::remove(const std::string &chave)
{
    entradas.erase(
        std::remove_if(entradas.begin(), entradas.end(),
                       [&chave](const auto &par) { return par.first == chave; }),
        entradas.end());
}

// ------------------------------ chaves / bool ------------------------------

bool chave_valida(const std::string &chave) { return CHAVES_VALIDAS.count(chave) > 0; }

bool valor_bool(const std::string &valor, bool padrao)
{
    const std::string v = minusculas(aparar(valor));
    if (v == "true" || v == "1" || v == "sim" || v == "yes" || v == "on")
        return true;
    if (v == "false" || v == "0" || v == "nao" || v == "não" || v == "no" || v == "off")
        return false;
    return padrao;
}

// --------------------------------- caminhos ---------------------------------

fs::path caminho_config_global()
{
#ifdef _WIN32
    const char *base = std::getenv("CODECTX_CONFIG_HOME");
    if (base != nullptr && *base != '\0')
        return fs::path(base) / "codectx" / "config.txt";
    base = std::getenv("APPDATA");
    if (base != nullptr && *base != '\0')
        return fs::path(base) / "codectx" / "config.txt";
    base = std::getenv("USERPROFILE");
    if (base != nullptr && *base != '\0')
        return fs::path(base) / "codectx" / "config.txt";
    return {};
#else
    const char *base = std::getenv("CODECTX_CONFIG_HOME");
    if (base != nullptr && *base != '\0')
        return fs::path(base) / "codectx" / "config";
    base = std::getenv("XDG_CONFIG_HOME");
    if (base == nullptr || *base == '\0')
    {
        base = std::getenv("HOME");
        if (base == nullptr || *base == '\0')
            return {};
        return fs::path(base) / ".config" / "codectx" / "config";
    }
    return fs::path(base) / "codectx" / "config";
#endif
}

fs::path descobrir_config_local(const fs::path &inicio)
{
    std::error_code ec;
    fs::path dir = fs::absolute(inicio, ec);
    if (ec)
        return {};

    while (true)
    {
        const fs::path candidato = dir / NOME_ARQUIVO_LOCAL;
        if (fs::exists(candidato, ec) && !ec)
            return candidato;
        const fs::path pai = dir.parent_path();
        if (pai == dir || pai.empty())
            break;
        dir = pai;
    }
    return {};
}

// ------------------------------ carregar/salvar ------------------------------

NivelConfig carregar_nivel(const fs::path &arquivo)
{
    NivelConfig nivel;
    if (arquivo.empty())
        return nivel;

    std::ifstream in(arquivo);
    if (!in.is_open())
        return nivel; // ausente ou ilegível: nível vazio, sem erro fatal

    std::string linha;
    while (std::getline(in, linha))
    {
        const std::string conteudo = aparar(linha);
        if (conteudo.empty() || conteudo[0] == '#')
            continue;

        const size_t pos = conteudo.find('=');
        if (pos == std::string::npos)
        {
            std::cerr << "codectx: aviso: linha invalida em " << arquivo.string()
                      << " (esperado chave=valor): " << conteudo << "\n";
            continue;
        }

        const std::string chave = aparar(conteudo.substr(0, pos));
        const std::string valor = aparar(conteudo.substr(pos + 1));
        if (!chave_valida(chave))
        {
            std::cerr << "codectx: aviso: chave desconhecida ignorada -> " << chave << "\n";
            continue;
        }
        nivel.acrescenta(chave, {valor});
    }
    return nivel;
}

bool salvar_nivel(const fs::path &arquivo, const NivelConfig &nivel)
{
    if (arquivo.empty())
        return false;

    std::error_code ec;
    fs::create_directories(arquivo.parent_path(), ec); // erro tolerável se já existe

    // Cópia ordenada por chave para saída determinística.
    NivelConfig ordenado = nivel;
    std::sort(ordenado.entradas.begin(), ordenado.entradas.end(),
              [](const auto &a, const auto &b) { return a.first < b.first; });

    const fs::path temporario = arquivo.string() + ".tmp";
    {
        std::ofstream out(temporario, std::ios::trunc);
        if (!out.is_open())
            return false;
        out << "# codectx — configuracao persistente\n";
        out << "# edite a mao ou use: codectx config set|add|unset [--global|--local]\n";
        for (const auto &[chave, valores] : ordenado.entradas)
            for (const auto &valor : valores)
                out << chave << "=" << valor << "\n";
    }

    fs::rename(temporario, arquivo, ec);
    if (ec)
    {
        std::error_code ec_rm;
        fs::remove(temporario, ec_rm);
        return false;
    }
    return true;
}

// --------------------------------- mesclagem ---------------------------------

NivelConfig mesclar_niveis(const NivelConfig &global, const NivelConfig &local)
{
    NivelConfig efetivo = global;
    for (const auto &[chave, valores] : local.entradas)
        efetivo.define(chave, valores);
    return efetivo;
}

// ------------------------------- aplicar config ------------------------------

static void somar_exclusivos(std::vector<std::string> &destino,
                             const std::vector<std::string> &origem)
{
    for (const auto &v : origem)
        if (std::find(destino.begin(), destino.end(), v) == destino.end())
            destino.push_back(v);
}

void aplicar_config(Config &cfg, const NivelConfig &efetivo)
{
    if (efetivo.vazia())
        return;

    // Alvos: só substituem quando a CLI não forneceu nenhum.
    if (!cfg.cli_alvos)
    {
        if (const auto *v = efetivo.busca("alvos"); v != nullptr && !v->empty())
            cfg.alvos = *v;
    }

    // Filtros de inclusão: substituem a categoria inteira quando ausentes na CLI.
    if (!cfg.cli_ext)
    {
        if (const auto *v = efetivo.busca("ext"); v != nullptr && !v->empty())
        {
            cfg.extensoes_permitidas = *v;
            cfg.tem_extensoes = true;
        }
    }

    if (!cfg.cli_file)
    {
        if (const auto *v = efetivo.busca("file"); v != nullptr && !v->empty())
            cfg.nomes_permitidos = *v;
    }

    // Exclusões e adições sempre somam (união com a CLI).
    if (const auto *v = efetivo.busca("not"); v != nullptr)
        somar_exclusivos(cfg.lista_ignorar_manual, *v);

    if (const auto *v = efetivo.busca("add"); v != nullptr && !v->empty())
    {
        somar_exclusivos(cfg.arquivos_adicionais, *v);
        somar_exclusivos(cfg.alvos, *v);
    }

    // Saída: só quando a CLI não definiu; replica o auto-ignore do parse_args.
    if (!cfg.cli_output)
    {
        if (const auto *v = efetivo.busca("output");
            v != nullptr && !v->empty() && cfg.arquivo_saida.empty())
        {
            cfg.arquivo_saida = v->front();
            cfg.lista_ignorar_manual.push_back(fs::path(cfg.arquivo_saida).filename().string());
        }
    }

    // Recursão: flag CLI vence; default da config aplicado apenas na ausência.
    // A chave "no-rec" é invertida: valor verdadeiro DESATIVA a recursão.
    if (!cfg.cli_no_rec)
    {
        if (const auto *v = efetivo.busca("no-rec"); v != nullptr && !v->empty())
        {
            const bool eh_verdadeiro = valor_bool(v->front(), false);
            const bool eh_falso = !valor_bool(v->front(), true);
            if (eh_verdadeiro)
                cfg.recursivo = false;
            else if (eh_falso)
                cfg.recursivo = true;
            // valor irreconhecível: mantém o estado atual
        }
    }
}

// ------------------------------ subcomando config ------------------------------

namespace
{

int cmd_lista()
{
    const NivelConfig g = carregar_nivel(caminho_config_global());
    const fs::path local_path = descobrir_config_local(fs::current_path());
    const NivelConfig l = carregar_nivel(local_path);

    if (g.vazia() && l.vazia())
    {
        std::cout << "(vazio)\n";
        return EXIT_SUCCESS;
    }

    const NivelConfig efetivo = mesclar_niveis(g, l);
    for (const auto &[chave, valores] : efetivo.entradas)
    {
        const char *origem =
            (l.busca(chave) != nullptr) ? "[local]" : "[global]";
        for (const auto &valor : valores)
            std::cout << origem << " " << chave << "=" << valor << "\n";
    }
    return EXIT_SUCCESS;
}

int usage_erro(const std::string &msg)
{
    std::cerr << msg << "\n\n"
              << "Uso: codectx config [get <chave> | set <chave> <valores...> |\n"
              << "                     add <chave> <valores...> | unset <chave> |\n"
              << "                     list | path]\n"
              << "Escopo de escrita: --global (padrao) | --local (grava ./.codectx)\n"
              << "Chaves: alvos ext file add not output no-rec\n";
    return EXIT_FAILURE;
}

} // namespace

int comando_config(int argc, char *argv[])
{
    bool escopo_global = true;
    std::vector<std::string> tokens;
    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--global")
            escopo_global = true;
        else if (arg == "--local")
            escopo_global = false;
        else
            tokens.push_back(arg);
    }

    const std::string verbo = tokens.empty() ? "list" : tokens[0];

    if (verbo == "list")
    {
        if (tokens.size() > 1)
            return usage_erro("Erro: 'list' nao aceita argumentos.");
        return cmd_lista();
    }

    if (verbo == "path")
    {
        if (tokens.size() > 1)
            return usage_erro("Erro: 'path' nao aceita argumentos.");
        std::cout << "global: " << caminho_config_global().string() << "\n";
        const fs::path loc = descobrir_config_local(fs::current_path());
        std::cout << "local:  "
                  << (loc.empty() ? std::string("(nenhum)") : loc.string()) << "\n";
        return EXIT_SUCCESS;
    }

    if (verbo == "get")
    {
        if (tokens.size() != 2)
            return usage_erro("Erro: uso correto -> codectx config get <chave>");
        const std::string &chave = tokens[1];
        if (!chave_valida(chave))
            return usage_erro("Erro: chave invalida -> " + chave);

        const NivelConfig g = carregar_nivel(caminho_config_global());
        const NivelConfig l = carregar_nivel(descobrir_config_local(fs::current_path()));
        const NivelConfig efetivo = mesclar_niveis(g, l);

        const auto *valores = efetivo.busca(chave);
        if (valores == nullptr || valores->empty())
        {
            std::cerr << "codectx: chave sem valor -> " << chave << "\n";
            return EXIT_FAILURE;
        }
        for (const auto &valor : *valores)
            std::cout << valor << "\n";
        return EXIT_SUCCESS;
    }

    if (verbo == "set" || verbo == "add" || verbo == "unset")
    {
        if (tokens.size() < 2)
            return usage_erro("Erro: uso correto -> codectx config " + verbo +
                              " <chave> [valores...]");
        const std::string &chave = tokens[1];
        if (!chave_valida(chave))
            return usage_erro("Erro: chave invalida -> " + chave);

        const std::vector<std::string> valores(tokens.begin() + 2, tokens.end());
        if (verbo != "unset" && valores.empty())
            return usage_erro("Erro: informe ao menos um valor para '" + chave + "'.");

        const fs::path arquivo =
            escopo_global ? caminho_config_global() : fs::current_path() / NOME_ARQUIVO_LOCAL;
        if (arquivo.empty())
            return usage_erro("Erro: diretorio de configuracao indisponivel.");

        NivelConfig nivel = carregar_nivel(arquivo);
        if (verbo == "set")
            nivel.define(chave, valores);
        else if (verbo == "add")
            nivel.acrescenta(chave, valores);
        else
            nivel.remove(chave);

        if (!salvar_nivel(arquivo, nivel))
        {
            std::cerr << "Erro: falha ao gravar " << arquivo.string() << "\n";
            return EXIT_FAILURE;
        }
        std::cout << (escopo_global ? "[global]" : "[local]") << " " << chave
                  << " gravado em " << arquivo.string() << "\n";
        return EXIT_SUCCESS;
    }

    return usage_erro("Erro: verbo desconhecido -> " + verbo);
}

} // namespace codectx
