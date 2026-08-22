#pragma once

#include <filesystem>
#include <ostream>
#include <regex>
#include <string>
#include <utility>
#include <vector>

namespace codectx
{

namespace fs = std::filesystem;

#ifndef CODECTX_VERSION
#define CODECTX_VERSION "1.0.0"
#endif

struct Config
{
    std::vector<std::string> alvos;
    std::vector<std::string> extensoes_permitidas;
    std::vector<std::string> nomes_permitidos;
    std::vector<std::string> arquivos_adicionais;
    std::vector<std::string> lista_ignorar_manual;
    std::string arquivo_saida;
    bool recursivo = true;
    bool tem_extensoes = false;
    bool ajuda = false;
    bool versao = false;

    // Categorias efetivamente fornecidas na linha de comando (para a
    // precedência CLI > config local > config global > defaults).
    bool cli_alvos = false;
    bool cli_ext = false;
    bool cli_file = false;
    bool cli_add = false;
    bool cli_not = false;
    bool cli_output = false;
    bool cli_no_rec = false;
};

bool parse_args(int argc, char *argv[], Config &cfg);

void exibir_ajuda(const char *prog_name);

class ConcatenadorDeArquivos
{
public:
    explicit ConcatenadorDeArquivos(const Config &cfg);

    bool deve_ignorar(const fs::path &path) const;

    bool processar_arquivo(const fs::path &arquivo, std::ostream &out);

    void percorrer_alvos(std::ostream &out);

    int executar();

private:
    Config cfg_;
    std::regex regex_extensoes_;
    std::regex padroes_ignorar_;
    bool tem_extensoes_ = false;
};

// ------------------------- configuração persistente ------------------------

// Nível de configuração: conjunto ordenado de chaves com valores múltiplos.
struct NivelConfig
{
    std::vector<std::pair<std::string, std::vector<std::string>>> entradas;

    bool vazia() const;
    const std::vector<std::string> *busca(const std::string &chave) const;
    void define(const std::string &chave, std::vector<std::string> valores);
    void acrescenta(const std::string &chave, const std::vector<std::string> &valores);
    void remove(const std::string &chave);
};

// Chaves aceitas no arquivo de configuração.
bool chave_valida(const std::string &chave);

// Converte "true/1/sim/yes/on" (e negações) para bool; fora disso usa o padrão.
bool valor_bool(const std::string &valor, bool padrao);

// Global: $CODECTX_CONFIG_HOME/codectx/config (teste/sandbox) ou o diretório
// de config do usuário ($XDG_CONFIG_HOME|~/.config; %APPDATA% no Windows).
fs::path caminho_config_global();

// Local: primeiro arquivo `.codectx` a partir de `inicio` subindo até a raiz.
// Retorna caminho vazio se não houver.
fs::path descobrir_config_local(const fs::path &inicio);

NivelConfig carregar_nivel(const fs::path &arquivo);
bool salvar_nivel(const fs::path &arquivo, const NivelConfig &nivel);

// Local sobrepõe global por chave inteira (estilo git).
NivelConfig mesclar_niveis(const NivelConfig &global, const NivelConfig &local);

// Preenche em `cfg` as categorias NÃO fornecidas na CLI (`not` e `add` sempre
// somam com a config). Replica os efeitos derivados do parse_args
// (auto-ignorar saída, adicionar `add` aos alvos, tem_extensoes).
void aplicar_config(Config &cfg, const NivelConfig &efetivo);

// Subcomando `codectx config [get|set|add|unset|list|path]` — retorna código
// de saída. Escrita via --global (padrão) ou --local.
int comando_config(int argc, char *argv[]);

} // namespace codectx
