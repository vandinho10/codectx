#include "codectx.hpp"

#include <cstdlib>
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

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

    codectx::ConcatenadorDeArquivos app(cfg);
    return app.executar();
}
