#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <bsd/string.h>
#include <upsclient.h>

// https://learn.netdata.cloud/docs/developer-and-contributor-corner/external-plugins#disable
#define NETDATA_PLUGIN_EXIT_DISABLE 1

#define LISTUPS_NUMQ 1
#define LISTVAR_NUMQ 2

int main(int argc, char *argv[])
{
        size_t numa;
        char **answers[1];
        int listups_status, listvar_status;
        const char *listups_query[LISTUPS_NUMQ] = { "UPS" };
        const char *listvar_query[LISTVAR_NUMQ] = { "VAR" };
        char ups_id[64];
        UPSCONN_t listups_conn, listvar_conn;

        // If we fail to initialize libupsclient or connect to a local
        // UPS, then there's nothing more to be done; Netdata should disable
        // this plugin, since it cannot offer any metrics.
        if (-1 == upscli_init(0, NULL, NULL, NULL)) {
                fputs("error: failed to initialize libupsclient", stderr);
                puts("DISABLE");
                exit(NETDATA_PLUGIN_EXIT_DISABLE);
        }

        // TODO: get address/port from configuration file
        if ((-1 == upscli_connect(&listups_conn, "127.0.0.1", 3493, 0)) ||
            (-1 == upscli_connect(&listvar_conn, "127.0.0.1", 3493, 0))) {
                upscli_cleanup();
                fputs("error: failed to connect to upsd at 127.0.0.1:3493", stderr);
                puts("DISABLE");
                exit(NETDATA_PLUGIN_EXIT_DISABLE);
        }

        // Query upsd for UPSes with the 'LIST UPS' command.
        if (-1 == upscli_list_start(&listups_conn, LISTUPS_NUMQ, listups_query)) {
                fprintf(stderr, "error: failed to 'LIST UPS': libupsclient: %s\n",
                    upscli_strerror(&listups_conn)
                );
        }

        do {
                listups_status = upscli_list_next(&listups_conn, LISTUPS_NUMQ, listups_query, &numa, (char***)&answers);

                // Unfortunately, upscli_list_next() will emit the list delimiter
                // "END LIST UPS" as its last iteration before returning 0. We don't
                // need it, so let's skip processing on that item.
                 if (!strcmp("END", answers[0][0]))
                         continue;

                // Since we don't know how libupsclient allocates or deallocates
                // memory, it would be unwise to feed it a query that it would
                // then deallocate at some point. So, copy it to be safe.
                strlcpy(ups_id, answers[0][LISTUPS_NUMQ], sizeof(ups_id));

                // Query upsd for UPS properties with the 'LIST VAR <ups>' command.
                listvar_query[1] = ups_id;
                if (-1 == upscli_list_start(&listvar_conn, 2, listvar_query)) {
                        fprintf(stderr, "error: failed to 'LIST VAR %s': libupsclient: %s\n",
                            ups_id, upscli_strerror(&listvar_conn)
                        );
                }

                do {
                        listvar_status = upscli_list_next(&listvar_conn, LISTVAR_NUMQ, listvar_query, &numa, (char***)&answers);

                        // Unfortunately, upscli_list_next() will emit the list delimiter
                        // "END LIST VAR" as its last iteration before returning 0. We don't
                        // need it, so let's skip processing on that item.
                        if (!strcmp("END", answers[0][0]))
                                continue;

                        for (size_t i = 0; i < numa; i++) {
                                // TODO: print the metrics to stdout for Netdata.
                                printf("%s ", answers[0][i]);
                        }
                        printf("\n");
                } while (1 == listvar_status);

                if (-1 == listvar_status) {
                        fprintf(stderr, "error: failed to finish 'LIST VAR %s': libupsclient: %s\n",
                            ups_id, upscli_strerror(&listvar_conn)
                        );
                }

        } while (1 == listups_status);

        if (-1 == listups_status) {
                fprintf(stderr, "error: failed to finish 'LIST UPS': libupsclient: %s\n",
                    upscli_strerror(&listups_conn)
                );
        }

        // Flush the data out of the stream buffer to ensure netdata gets it immediately.
        // https://learn.netdata.cloud/docs/developer-and-contributor-corner/external-plugins#the-output-of-the-plugin
        fflush(stdout);

        upscli_disconnect(&listups_conn);
        upscli_disconnect(&listvar_conn);
        upscli_cleanup();
}
