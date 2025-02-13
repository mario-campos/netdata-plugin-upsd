#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <upsclient.h>

int main(int argc, char *argv[])
{
        int ret = upscli_init(0, NULL, NULL, NULL);
        printf("upscli_init(0, NULL, NULL, NULL) = %d\n", ret);

        puts("connecting to upsd");
        UPSCONN_t ups;
        assert(0 == upscli_connect(&ups, "127.0.0.1", 3493, 0));
        puts("connected to upsd");

        puts("listing UPSes");
        size_t numq = 1;
        const char *query[2];
        query[0] = "UPS";
        assert(0 == upscli_list_start(&ups, numq, query));

        size_t numa;
        char **answers[1];
        assert(1 == upscli_list_next(&ups, numq, query, &numa, (char***)&answers));
        printf("LIST %s %s %s\n", answers[0][0], answers[0][1], answers[0][2]);
        const char *ups_name = strdup(answers[0][1]);

        ret = upscli_list_next(&ups, numq, query, &numa, (char***)&answers);
        if (ret == 0) puts("done listing UPSes");

        puts("listing variables");
        numq = 2;
        query[0] = "VAR";
        query[1] = ups_name;
        assert(0 == upscli_list_start(&ups, numq, query));

        do {
                ret = upscli_list_next(&ups, numq, query, &numa, (char***)&answers);
                for (size_t i = 0; i < numa; i++) {
                        printf("%s ", answers[0][i]);
                }
                printf("\n");
        } while (1 == ret);

        if (ret == 0) puts("done listing UPSes");

        puts("disconnecting from upsd");
        assert(0 == upscli_disconnect(&ups));
        puts("disconnected from upsd");

        upscli_cleanup();
}
