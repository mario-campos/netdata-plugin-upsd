#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <bsd/string.h>
#include <upsclient.h>

// https://learn.netdata.cloud/docs/developer-and-contributor-corner/external-plugins#disable
#define NETDATA_PLUGIN_EXIT_DISABLE 1

int main(int argc, char *argv[])
{
        int status;
        size_t numq, numa;
        const char *query[2];
        char **answers[1], ups_name[64];
        UPSCONN_t ups;

        // If we fail to initialize libupsclient or connect to a local
        // UPS, then there's nothing more to be done; Netdata should disable
        // this plugin, since it cannot offer any metrics.
        if (-1 == upscli_init(0, NULL, NULL, NULL)) {
                fputs("failed to initialize libupsclient", stderr);
                puts("DISABLE");
                exit(NETDATA_PLUGIN_EXIT_DISABLE);
        }

        // TODO: get address/port from configuration file
        if (-1 == upscli_connect(&ups, "127.0.0.1", 3493, 0)) {
                upscli_cleanup();
                fputs("failed to connect to upsd at 127.0.0.1:3493", stderr);
                puts("DISABLE");
                exit(NETDATA_PLUGIN_EXIT_DISABLE);
        }
        
        numq = 1;
        query[0] = "UPS";
        assert(0 == upscli_list_start(&ups, numq, query));

        assert(1 == upscli_list_next(&ups, numq, query, &numa, (char***)&answers));
        printf("LIST %s %s %s\n", answers[0][0], answers[0][1], answers[0][2]);
        strlcpy(ups_name, answers[0][1], sizeof(ups_name));

        status = upscli_list_next(&ups, numq, query, &numa, (char***)&answers);
        if (-1 == status) {
                fprintf(stderr, "error: failed to retrieve data from upsd: libupsclient: %s",
                    upscli_strerror(&ups)
                );
        }

        numq = 2;
        query[0] = "VAR";
        query[1] = ups_name;
        assert(0 == upscli_list_start(&ups, numq, query));

        do {
                status = upscli_list_next(&ups, numq, query, &numa, (char***)&answers);
                for (size_t i = 0; i < numa; i++) {
                        printf("%s ", answers[0][i]);
                }
                printf("\n");
        } while (1 == status);

        if (-1 == status) {
                fprintf(stderr, "error: failed to retrieve data from upsd: libupsclient: %s",
                    upscli_strerror(&ups)
                );
        }

        upscli_disconnect(&ups);
        upscli_cleanup();
}
