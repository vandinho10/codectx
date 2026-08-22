#include "../codectx.hpp"

#include "test_framework.hpp"

#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace codectx;

// ============ helpers de teste ============

static Config parse(std::vector<std::string> args)
{
    std::vector<char *> ptrs;
    for (auto &s : args)
        ptrs.push_back(const_cast<char *>(s.c_str()));

    std::string prog = "codectx";
    std::vector<char *> argv{const_cast<char *>(prog.c_str())};
    for (auto &p : ptrs)
        argv.push_back(p);

    Config cfg;
    bool ok = parse_args(static_cast<int>(argv.size()), argv.data(), cfg);
    CHECK(ok);
    return cfg;
}

struct TempDir
{
    fs::path dir;

    TempDir()
    {
        static unsigned long seq = 0;
        auto id = std::chrono::steady_clock::now().time_since_epoch().count();
        dir = fs::current_path() / ("codectx_test_tmp_" + std::to_string(id) + "_" + std::to_string(seq++));
        fs::remove_all(dir);
        fs::create_directories(dir);
    }

    ~TempDir()
    {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
};

static void escrever(const fs::path &p, const std::string &conteudo)
{
    fs::create_directories(p.parent_path());
    std::ofstream f(p);
    f << conteudo;
}

static int contar_marcas(const std::string &saida)
{
    int n = 0;
    size_t pos = 0;
    const std::string marca = "INICIO ARQUIVO";
    while ((pos = saida.find(marca, pos)) != std::string::npos)
    {
        ++n;
        pos += marca.size();
    }
    return n;
}

// ============ parse_args: tabela de estados ============

TEST("parse_args: sem argumentos -> alvo padrao '.' e recursivo")
{
    Config cfg = parse({});
    CHECK(cfg.alvos.size() == 1 && cfg.alvos[0] == ".");
    CHECK(cfg.recursivo == true);
    CHECK(cfg.extensoes_permitidas.empty());
    CHECK(cfg.tem_extensoes == false);
}

TEST("parse_args: -e cpp hpp -> extensoes em sequencia")
{
    Config cfg = parse({"-e", "cpp", "hpp"});
    CHECK(cfg.extensoes_permitidas.size() == 2);
    CHECK(cfg.extensoes_permitidas[0] == "cpp");
    CHECK(cfg.extensoes_permitidas[1] == "hpp");
    CHECK(cfg.tem_extensoes == true);
    CHECK(cfg.alvos.size() == 1 && cfg.alvos[0] == ".");
}

TEST("parse_args: -e cpp -t src -> extensao + alvo")
{
    Config cfg = parse({"-e", "cpp", "-t", "src"});
    CHECK(cfg.extensoes_permitidas.size() == 1 && cfg.extensoes_permitidas[0] == "cpp");
    CHECK(cfg.alvos.size() == 1 && cfg.alvos[0] == "src");
}

TEST("parse_args: -e '.cpp' remove o ponto inicial")
{
    Config cfg = parse({"-e", ".cpp", "-t", "src"});
    CHECK(cfg.extensoes_permitidas.size() == 1 && cfg.extensoes_permitidas[0] == "cpp");
}

TEST("parse_args: -e com diretorio existente vira alvo (nao extensao)")
{
    TempDir td;
    Config cfg = parse({"-e", "cpp", td.dir.string()});
    CHECK(cfg.extensoes_permitidas.size() == 1 && cfg.extensoes_permitidas[0] == "cpp");
    CHECK(cfg.alvos.size() == 1 && cfg.alvos[0] == td.dir.string());
}

TEST("parse_args: -e com caminho (parent path) vira alvo")
{
    Config cfg = parse({"-e", "cpp", "sub/file.cpp"});
    CHECK(cfg.extensoes_permitidas.size() == 1 && cfg.extensoes_permitidas[0] == "cpp");
    CHECK(cfg.alvos.size() == 1 && cfg.alvos[0] == "sub/file.cpp");
}

TEST("parse_args: -f com nomes exatos e caminho")
{
    Config cfg = parse({"-f", "Makefile", "sub/x.sh"});
    CHECK(cfg.nomes_permitidos.size() == 2);
    CHECK(cfg.nomes_permitidos[0] == "Makefile");
    CHECK(cfg.nomes_permitidos[1] == "x.sh");
    CHECK(cfg.alvos.size() == 1 && cfg.alvos[0] == "sub/x.sh");
}

TEST("parse_args: -a adiciona alvo preservando a base '.'")
{
    Config cfg = parse({"-a", "extra"});
    CHECK(cfg.alvos.size() == 2);
    CHECK(cfg.alvos[0] == ".");
    CHECK(cfg.alvos[1] == "extra");
}

TEST("parse_args: -n registra exclusao manual")
{
    Config cfg = parse({"-n", "segredo.txt"});
    CHECK(cfg.lista_ignorar_manual.size() == 1 && cfg.lista_ignorar_manual[0] == "segredo.txt");
}

TEST("parse_args: -o define saida e auto-ignora o arquivo de saida")
{
    Config cfg = parse({"-o", "out.txt", "-t", "src"});
    CHECK(cfg.arquivo_saida == "out.txt");
    CHECK(cfg.lista_ignorar_manual.size() == 1 && cfg.lista_ignorar_manual[0] == "out.txt");
}

TEST("parse_args: --no-rec desativa recursao")
{
    Config cfg = parse({"--no-rec"});
    CHECK(cfg.recursivo == false);
}

TEST("parse_args: -h marca ajuda")
{
    Config cfg = parse({"-h"});
    CHECK(cfg.ajuda == true);
}

TEST("parse_args: --version marca versao")
{
    Config cfg = parse({"--version"});
    CHECK(cfg.versao == true);
}

TEST("parse_args: flag desconhecida e ignorada")
{
    Config cfg = parse({"--flag-invalida", "src"});
    CHECK(cfg.alvos.size() == 1 && cfg.alvos[0] == "src");
}

TEST("parse_args: extensao com metacaracteres nao lanca excecao")
{
    Config cfg = parse({"-e", "(", "-t", "src"});
    CHECK(cfg.extensoes_permitidas.size() == 1 && cfg.extensoes_permitidas[0] == "(");
}

// ============ deve_ignorar: tabela ============

TEST("deve_ignorar: diretorios e arquivos do blacklist")
{
    ConcatenadorDeArquivos app(parse({}));
    CHECK(app.deve_ignorar(fs::path(".git")) == true);
    CHECK(app.deve_ignorar(fs::path("proj/node_modules")) == true);
    CHECK(app.deve_ignorar(fs::path("proj/.venv")) == true);
    CHECK(app.deve_ignorar(fs::path("proj/.env")) == true);
    CHECK(app.deve_ignorar(fs::path("proj/main.cpp")) == false);
    CHECK(app.deve_ignorar(fs::path("proj/src/foo.h")) == false);
}

TEST("deve_ignorar: '.' e '..' nunca sao ignorados")
{
    ConcatenadorDeArquivos app(parse({}));
    CHECK(app.deve_ignorar(fs::path(".")) == false);
    CHECK(app.deve_ignorar(fs::path("proj/..")) == false);
}

TEST("deve_ignorar: exclusao manual via -n")
{
    ConcatenadorDeArquivos app(parse({"-n", "segredo.txt"}));
    CHECK(app.deve_ignorar(fs::path("proj/segredo.txt")) == true);
    CHECK(app.deve_ignorar(fs::path("proj/outro.txt")) == false);
}

// ============ processar_arquivo: filtros e formato ============

TEST("processar_arquivo: default exclui binarios (.bin) e inclui texto")
{
    TempDir td;
    escrever(td.dir / "a.txt", "ola");
    escrever(td.dir / "c.bin", "\x00\x01\x02");

    ConcatenadorDeArquivos app(parse({"-t", td.dir.string()}));
    std::ostringstream out;
    CHECK(app.processar_arquivo(td.dir / "a.txt", out) == true);
    CHECK(out.str().find("ola") != std::string::npos);

    std::ostringstream out2;
    CHECK(app.processar_arquivo(td.dir / "c.bin", out2) == false);
}

TEST("processar_arquivo: filtro -e seleciona apenas a extensao (case-insensitive)")
{
    TempDir td;
    escrever(td.dir / "a.cpp", "int x;");
    escrever(td.dir / "A.H", "int h;");
    escrever(td.dir / "b.txt", "texto");

    ConcatenadorDeArquivos app(parse({"-e", "cpp", "-t", td.dir.string()}));
    std::ostringstream out;
    CHECK(app.processar_arquivo(td.dir / "a.cpp", out) == true);
    CHECK(app.processar_arquivo(td.dir / "A.H", out) == false);
    CHECK(app.processar_arquivo(td.dir / "b.txt", out) == false);
}

TEST("processar_arquivo: filtro -f seleciona nome exato")
{
    TempDir td;
    escrever(td.dir / "Makefile", "all:");
    escrever(td.dir / "out.txt", "x");

    ConcatenadorDeArquivos app(parse({"-f", "Makefile", "-t", td.dir.string()}));
    std::ostringstream out;
    CHECK(app.processar_arquivo(td.dir / "Makefile", out) == true);
    CHECK(app.processar_arquivo(td.dir / "out.txt", out) == false);
}

TEST("processar_arquivo: formato de saida com delimitadores")
{
    TempDir td;
    escrever(td.dir / "a.txt", "conteudo-x");

    ConcatenadorDeArquivos app(parse({"-t", td.dir.string()}));
    std::ostringstream out;
    CHECK(app.processar_arquivo(td.dir / "a.txt", out) == true);

    const std::string saida = out.str();
    CHECK(saida.find("INICIO ARQUIVO:") != std::string::npos);
    CHECK(saida.find("FIM ARQUIVO:") != std::string::npos);
    CHECK(saida.find("conteudo-x") != std::string::npos);
    CHECK(saida.find("<><>========================================<><>") != std::string::npos);
}

TEST("processar_arquivo: arquivo vazio nao injeta conteudo")
{
    TempDir td;
    escrever(td.dir / "vazio.txt", "");

    ConcatenadorDeArquivos app(parse({"-t", td.dir.string()}));
    std::ostringstream out;
    CHECK(app.processar_arquivo(td.dir / "vazio.txt", out) == true);
    CHECK(out.str().find("INICIO ARQUIVO:") != std::string::npos);
    CHECK(out.str().find("FIM ARQUIVO:") != std::string::npos);
}

// ============ percorrer_alvos: varredura ============

TEST("percorrer_alvos: recursivo encontra arquivos aninhados")
{
    TempDir td;
    escrever(td.dir / "a.txt", "1");
    escrever(td.dir / "sub/b.cpp", "2");

    ConcatenadorDeArquivos app(parse({"-t", td.dir.string()}));
    std::ostringstream out;
    app.percorrer_alvos(out);
    CHECK(contar_marcas(out.str()) == 2);
}

TEST("percorrer_alvos: --no-rec processa apenas o nivel superior")
{
    TempDir td;
    escrever(td.dir / "a.txt", "1");
    escrever(td.dir / "sub/b.cpp", "2");

    ConcatenadorDeArquivos app(parse({"--no-rec", "-t", td.dir.string()}));
    std::ostringstream out;
    app.percorrer_alvos(out);
    CHECK(contar_marcas(out.str()) == 1);
}

TEST("percorrer_alvos: ignora node_modules recursivamente")
{
    TempDir td;
    escrever(td.dir / "a.txt", "1");
    escrever(td.dir / "node_modules/lib/x.js", "2");

    ConcatenadorDeArquivos app(parse({"-t", td.dir.string()}));
    std::ostringstream out;
    app.percorrer_alvos(out);
    CHECK(contar_marcas(out.str()) == 1);
    CHECK(out.str().find("x.js") == std::string::npos);
}

TEST("percorrer_alvos: alvo inexistente gera aviso e nao derruba")
{
    ConcatenadorDeArquivos app(parse({"-t", "nao_existe_dir_xyz"}));
    std::ostringstream out;
    app.percorrer_alvos(out);
    CHECK(out.str().empty());
}

TEST("percorrer_alvos: alvo arquivo unico")
{
    TempDir td;
    escrever(td.dir / "unico.cpp", "int main() {}");

    ConcatenadorDeArquivos app(parse({"-t", (td.dir / "unico.cpp").string()}));
    std::ostringstream out;
    app.percorrer_alvos(out);
    CHECK(contar_marcas(out.str()) == 1);
}

TEST("percorrer_alvos: varredura default nao despeja .bin")
{
    TempDir td;
    escrever(td.dir / "a.txt", "1");
    escrever(td.dir / "b.bin", "2");
    escrever(td.dir / "c.o", "3");

    ConcatenadorDeArquivos app(parse({"-t", td.dir.string()}));
    std::ostringstream out;
    app.percorrer_alvos(out);
    CHECK(contar_marcas(out.str()) == 1);
    CHECK(out.str().find("b.bin") == std::string::npos);
    CHECK(out.str().find("c.o") == std::string::npos);
}

// ============ executar: saida em arquivo e codigos de retorno ============

TEST("executar: -o grava arquivo de saida e retorna sucesso")
{
    TempDir td;
    escrever(td.dir / "a.txt", "ola mundo");
    Config cfg = parse({"-o", (td.dir / "out.txt").string(), "-t", td.dir.string()});

    ConcatenadorDeArquivos app(cfg);
    CHECK(app.executar() == EXIT_SUCCESS);
    CHECK(fs::exists(td.dir / "out.txt") == true);
}

TEST("executar: saida apontando para diretorio retorna falha")
{
    TempDir td;
    Config cfg = parse({"-o", td.dir.string(), "-t", td.dir.string()});

    ConcatenadorDeArquivos app(cfg);
    CHECK(app.executar() == EXIT_FAILURE);
}

// ============ regressoes dos fixes criticos ============

TEST("regressao: -e '(' nao derruba durante varredura real")
{
    TempDir td;
    escrever(td.dir / "a.txt", "1");
    escrever(td.dir / "b.cpp", "2");

    Config cfg = parse({"-e", "(", "-t", td.dir.string()});
    ConcatenadorDeArquivos app(cfg);
    std::ostringstream out;
    app.percorrer_alvos(out);
    CHECK(contar_marcas(out.str()) == 0);
}

TEST("regressao: -e cpp + diretorio existente nao engole o alvo")
{
    TempDir td;
    escrever(td.dir / "b.cpp", "int x;");
    escrever(td.dir / "a.txt", "texto");

    Config cfg = parse({"-e", "cpp", td.dir.string()});
    ConcatenadorDeArquivos app(cfg);
    std::ostringstream out;
    app.percorrer_alvos(out);
    CHECK(contar_marcas(out.str()) == 1);
    CHECK(out.str().find("b.cpp") != std::string::npos);
    CHECK(out.str().find("a.txt") == std::string::npos);
}

// ============ configuração persistente ============

#include <cstdio>

// Helper: executa comando_config com argv sintético.
static int rodar_config(std::vector<std::string> args)
{
    std::vector<char *> ptrs;
    for (auto &s : args)
        ptrs.push_back(const_cast<char *>(s.c_str()));
    std::string prog = "codectx";
    std::vector<char *> argv{const_cast<char *>(prog.c_str())};
    for (auto &p : ptrs)
        argv.push_back(p);
    return comando_config(static_cast<int>(argv.size()), argv.data());
}

static std::string ler_arquivo(const fs::path &p)
{
    std::ifstream f(p);
    return std::string((std::istreambuf_iterator<char>(f)),
                       std::istreambuf_iterator<char>());
}

TEST("NivelConfig: define/busca/acrescenta/remove")
{
    NivelConfig n;
    CHECK(n.vazia() == true);
    CHECK(n.busca("ext") == nullptr);

    n.define("ext", {"cpp", "hpp"});
    const auto *ext = n.busca("ext");
    CHECK(n.vazia() == false);
    CHECK(ext != nullptr);
    if (ext == nullptr)
        return;
    CHECK(ext->size() == 2);

    n.acrescenta("ext", {"cpp", "js"}); // cpp duplicado não entra
    ext = n.busca("ext");
    if (ext != nullptr)
        CHECK(ext->size() == 3);

    n.define("ext", {"rs"}); // define substitui
    ext = n.busca("ext");
    if (ext != nullptr)
        CHECK(ext->size() == 1 && (*ext)[0] == "rs");

    n.remove("ext");
    CHECK(n.busca("ext") == nullptr && n.vazia() == true);
}

TEST("chave_valida aceita exatamente as 7 chaves")
{
    CHECK(chave_valida("alvos") && chave_valida("ext") && chave_valida("file"));
    CHECK(chave_valida("add") && chave_valida("not") && chave_valida("output"));
    CHECK(chave_valida("no-rec"));
    CHECK(chave_valida("outra") == false);
    CHECK(chave_valida("") == false);
}

TEST("valor_bool interpreta variantes e cai no padrao")
{
    CHECK(valor_bool("true", false) == true);
    CHECK(valor_bool("TRUE", false) == true);
    CHECK(valor_bool(" 1 ", true) == true);
    CHECK(valor_bool("sim", false) == true);
    CHECK(valor_bool("false", true) == false);
    CHECK(valor_bool("off", true) == false);
    CHECK(valor_bool("nao", true) == false);
    CHECK(valor_bool("lixo", true) == true);   // padrao
    CHECK(valor_bool("lixo", false) == false); // padrao
}

TEST("caminho_config_global respeita CODECTX_CONFIG_HOME")
{
#ifdef _WIN32
    _putenv_s("CODECTX_CONFIG_HOME", R"(C:\sandbox)");
#else
    setenv("CODECTX_CONFIG_HOME", "/sandbox", 1);
#endif
    const fs::path p = caminho_config_global();
#ifdef _WIN32
    CHECK(p == fs::path(R"(C:\sandbox)") / "codectx" / "config.txt");
#else
    CHECK(p == fs::path("/sandbox") / "codectx" / "config");
#endif
#ifdef _WIN32
    _putenv_s("CODECTX_CONFIG_HOME", "");
#else
    unsetenv("CODECTX_CONFIG_HOME");
#endif
}

TEST("descobrir_config_local sobe a hierarquia ate encontrar .codectx")
{
    TempDir raiz;
    fs::create_directories(raiz.dir / "a" / "b");
    escrever(raiz.dir / ".codectx", "ext=php\n");

    CHECK(descobrir_config_local(raiz.dir / "a" / "b").empty() == false);
    CHECK(descobrir_config_local(raiz.dir / "a" / "b").filename() == ".codectx");
    CHECK(descobrir_config_local(raiz.dir).filename() == ".codectx");

    TempDir sem_config;
    CHECK(descobrir_config_local(sem_config.dir).empty());
}

TEST("carregar_nivel: formato chave=valor, comentarios e lixo ignorado")
{
    TempDir td;
    escrever(td.dir / "cfg",
             "# comentario\n"
             "\n"
             "ext=php\n"
             "ext = js \n"
             "no-rec=true\n"
             "sem-igual\n"
             "desconhecida=x\n"
             "output=out.md\n");

    // stderr recebe avisos; silencia para nao poluir o teste
    std::ostringstream vazio;
    auto *antigo = std::cerr.rdbuf(vazio.rdbuf());
    const NivelConfig n = carregar_nivel(td.dir / "cfg");
    std::cerr.rdbuf(antigo);

    const auto *v_ext = n.busca("ext");
    CHECK(v_ext != nullptr);
    if (v_ext == nullptr)
        return;
    CHECK(v_ext->size() == 2);
    CHECK((*v_ext)[0] == "php");
    CHECK((*v_ext)[1] == "js");
    CHECK(n.busca("desconhecida") == nullptr);

    const auto *v_rec = n.busca("no-rec");
    CHECK(v_rec != nullptr);
    if (v_rec != nullptr)
        CHECK(valor_bool((*v_rec)[0], true) == true);

    const auto *v_out = n.busca("output");
    CHECK(v_out != nullptr);
    if (v_out != nullptr)
        CHECK((*v_out)[0] == "out.md");
}

TEST("salvar/carregar roundtrip deterministico com valor contendo '='")
{
    TempDir td;
    NivelConfig n;
    n.define("not", {"*.min.js", "segredo=senha.txt"});
    n.define("no-rec", {"false"});
    n.define("alvos", {"src"});
    CHECK(salvar_nivel(td.dir / "cfg", n) == true);

    const std::string bruto = ler_arquivo(td.dir / "cfg");
    CHECK(bruto.find("not=segredo=senha.txt") != std::string::npos);
    CHECK(bruto.find("# codectx") != std::string::npos);

    const NivelConfig lido = carregar_nivel(td.dir / "cfg");
    const auto *v_not = lido.busca("not");
    CHECK(v_not != nullptr);
    if (v_not == nullptr)
        return;
    CHECK(v_not->size() == 2);
    const auto *v_alvos = lido.busca("alvos");
    CHECK(v_alvos != nullptr);
    if (v_alvos != nullptr)
        CHECK((*v_alvos)[0] == "src");
    // chaves gravadas em ordem alfabetica
    CHECK(bruto.find("alvos=") < bruto.find("no-rec="));
    CHECK(bruto.find("no-rec=") < bruto.find("not="));
}

TEST("mesclar_niveis: local sobrepoe global por chave inteira")
{
    NivelConfig g, l;
    g.define("ext", {"cpp", "hpp"});
    g.define("output", {"global.md"});
    l.define("ext", {"php"});

    const NivelConfig m = mesclar_niveis(g, l);
    const auto *v_ext = m.busca("ext");
    CHECK(v_ext != nullptr);
    if (v_ext == nullptr)
        return;
    CHECK(v_ext->size() == 1 && (*v_ext)[0] == "php");

    const auto *v_out = m.busca("output");
    CHECK(v_out != nullptr);
    if (v_out == nullptr)
        return;
    CHECK((*v_out)[0] == "global.md");
}

TEST("aplicar_config: preenche categorias ausentes na CLI")
{
    Config cli = parse({"-e", "cpp"}); // CLI definiu ext; resto vazio
    NivelConfig cfg;
    cfg.define("ext", {"php"});       // deve ser IGNORADO (CLI vence)
    cfg.define("file", {"Makefile"}); // deve ser aplicado
    cfg.define("not", {"rascunho"});
    cfg.define("alvos", {"src"});
    aplicar_config(cli, cfg);

    CHECK(cli.extensoes_permitidas.size() == 1 && cli.extensoes_permitidas[0] == "cpp");
    CHECK(cli.cli_ext == true);
    CHECK(cli.nomes_permitidos.size() == 1);
    if (!cli.nomes_permitidos.empty())
        CHECK(cli.nomes_permitidos[0] == "Makefile");
    // alvos da CLI ('.' default) sao preservados quando config tem alvos? NAO:
    // CLI nao definiu alvo explicitamente -> config substitui
    CHECK(cli.alvos.size() == 1 && cli.alvos[0] == "src");
    CHECK(cli.tem_extensoes == true);
}

TEST("aplicar_config: not e add sempre somam com a CLI")
{
    Config cli = parse({"-n", "cli.txt", "-a", "extra_cli"});
    NivelConfig cfg;
    cfg.define("not", {"cli.txt", "cfg.txt"});
    cfg.define("add", {"extra_cfg"});
    aplicar_config(cli, cfg);

    CHECK(cli.lista_ignorar_manual.size() == 2); // união sem duplicatas
    CHECK(cli.arquivos_adicionais.size() == 2);
    bool tem_extra_cfg = false;
    for (const auto &a : cli.alvos)
        if (a == "extra_cfg")
            tem_extra_cfg = true;
    CHECK(tem_extra_cfg == true);
}

TEST("aplicar_config: output da config replica auto-ignore")
{
    Config cli = parse({});
    NivelConfig cfg;
    cfg.define("output", {"relatorio.md"});
    aplicar_config(cli, cfg);

    CHECK(cli.arquivo_saida == "relatorio.md");
    CHECK(cli.lista_ignorar_manual.size() == 1);
    CHECK(cli.lista_ignorar_manual[0] == "relatorio.md");
}

TEST("aplicar_config: no-rec da config aplica somente se CLI nao usou --no-rec")
{
    Config cli_sem_flag = parse({});
    NivelConfig cfg;
    cfg.define("no-rec", {"true"});
    aplicar_config(cli_sem_flag, cfg);
    CHECK(cli_sem_flag.recursivo == false);

    Config cli_com_flag = parse({"--no-rec"});
    cfg.define("no-rec", {"false"});
    aplicar_config(cli_com_flag, cfg);
    CHECK(cli_com_flag.recursivo == false); // CLI vence
}

TEST("comando_config: ciclo set -> get -> list -> unset em escopo global sandbox")
{
#ifdef _WIN32
    TempDir td;
    std::string home = td.dir.string();
    _putenv_s("CODECTX_CONFIG_HOME", home.c_str());
#else
    TempDir td;
    setenv("CODECTX_CONFIG_HOME", td.dir.string().c_str(), 1);
#endif

    CHECK(rodar_config({"config", "set", "ext", "php", "js"}) == EXIT_SUCCESS);
    CHECK(fs::exists(caminho_config_global()));

    CHECK(rodar_config({"config", "get", "ext"}) == EXIT_SUCCESS);
    CHECK(rodar_config({"config", "list"}) == EXIT_SUCCESS);
    CHECK(rodar_config({"config", "get", "inexistente"}) == EXIT_FAILURE);
    CHECK(rodar_config({"config", "set", "chave_mala", "x"}) == EXIT_FAILURE);
    CHECK(rodar_config({"config", "verbo_falso"}) == EXIT_FAILURE);

    CHECK(rodar_config({"config", "unset", "ext"}) == EXIT_SUCCESS);
    CHECK(rodar_config({"config", "get", "ext"}) == EXIT_FAILURE);

#ifdef _WIN32
    _putenv_s("CODECTX_CONFIG_HOME", "");
#else
    unsetenv("CODECTX_CONFIG_HOME");
#endif
}

TEST("comando_config: --local grava ./.codectx no diretorio atual")
{
#ifdef _WIN32
    TempDir td;
    std::string home = td.dir.string();
    _putenv_s("CODECTX_CONFIG_HOME", home.c_str());
#else
    TempDir td;
    setenv("CODECTX_CONFIG_HOME", td.dir.string().c_str(), 1);
#endif

    const fs::path original = fs::current_path();
    fs::current_path(td.dir); // cwd do teste
    CHECK(rodar_config({"config", "--local", "set", "output", "ctx.md"}) == EXIT_SUCCESS);
    CHECK(fs::exists(td.dir / ".codectx"));

    const NivelConfig lido = carregar_nivel(td.dir / ".codectx");
    const auto *v_out = lido.busca("output");
    CHECK(v_out != nullptr);
    if (v_out == nullptr)
        return;
    CHECK((*v_out)[0] == "ctx.md");

    CHECK(descobrir_config_local(td.dir) == td.dir / ".codectx");
    fs::current_path(original);

#ifdef _WIN32
    _putenv_s("CODECTX_CONFIG_HOME", "");
#else
    unsetenv("CODECTX_CONFIG_HOME");
#endif
}

int main()
{
    return tests::run_all();
}
