#pragma once

#include <filesystem>
#include <ostream>
#include <regex>
#include <string>
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

} // namespace codectx
