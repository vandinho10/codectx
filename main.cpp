#include "codectx.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    // Subcomando de configuração persistente (estilo git config).
    if (argc > 1 && std::string(argv[1]) == "config")
        return codectx::comando_config(argc, argv);

    codectx::Config cfg;
    if (!codectx::parse_args(argc, argv, cfg))
        return EXIT_FAILURE;

    if (cfg.ajuda)
    {
        codectx::exibir_ajuda(argv[0]);
        return EXIT_SUCCESS;
    }

    if (cfg.versao)
    {
        std::cout << "codectx v" << CODECTX_VERSION << "\n";
        return EXIT_SUCCESS;
    }

    // Precedência: CLI > .codectx.conf local > config global > defaults.
    const codectx::NivelConfig global = codectx::carregar_nivel(codectx::caminho_config_global());
    const std::filesystem::path local_path =
        codectx::descobrir_config_local(std::filesystem::current_path());
    const codectx::NivelConfig local = codectx::carregar_nivel(local_path);
    codectx::aplicar_config(cfg, codectx::mesclar_niveis(global, local));

    codectx::ConcatenadorDeArquivos app(cfg);
    return app.executar();
}
