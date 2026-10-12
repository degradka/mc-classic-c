// main.c: MinecraftServer.main

#include "server.h"
#include "jfile.h"
#include "log.h"
#include "level/level_io.h"
#include "level/levelgen/level_gen.h"
#include "level/tile/tile.h"
#include <locale.h>
#include <stdlib.h>

int main(void) {
    setlocale(LC_CTYPE, "");
    Log_openFile();
    Tile_registerAll();
    static MinecraftServer server;
    char exception[512];
    if (!Server_init(&server, exception, sizeof exception)) {
        Log_severe("Failed to start the server!");
        Log_stackTrace(exception);
        return 0;
    }
    Log_info("Setting up");
    if (jfile_exists("server_level.dat")) {
        int len = 0;
        uint8_t* data = jfile_read("server_level.dat", &len, exception, sizeof exception);
        if (data) {
            server.level = LevelIO_load(data, len);
            free(data);
        } else {
            Log_warning("Failed to load level. Generating a new level");
            Log_stackTrace(exception);
        }
    } else {
        Log_warning("No level file found. Generating a new level");
    }
    if (!server.level) server.level = LevelGen_generateLevel("--");
    Server_save(&server, true);
    server.level->listener = Server_tileChanged;
    server.level->listenerCtx = &server;
    Server_run(&server);
    return 0;
}
