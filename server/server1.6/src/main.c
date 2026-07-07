// main.c: entry point, matching MinecraftServer.main()

#include "server.h"
#include "log.h"

int main(void) {
    // static, not stack local: MinecraftServer embeds the full Level plus
    // three PlayerLists worth of fixed entry arrays, tens of KB total
    static MinecraftServer srv;

    if (!Server_init(&srv)) {
        Log_severe("Failed to start server");
        return 1;
    }

    Server_run(&srv); // never returns
    return 0;
}
