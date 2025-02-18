#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <genht/htss.h>
#include <genht/hash.h>

#include <upsclient.h>

// https://learn.netdata.cloud/docs/developer-and-contributor-corner/external-plugins#operation
#define NETDATA_UPSD_EXIT_AND_RESTART 0
// https://learn.netdata.cloud/docs/developer-and-contributor-corner/external-plugins#disable
#define NETDATA_UPSD_EXIT_AND_DISABLE 1

// This arbitrary base priority was copied from Netdata's go.d orchestrator plugin.
#define NETDATA_UPSD_PRIO                     70000
#define NETDATA_UPSD_PRIO_LOAD                ( 0 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_LOAD_USAGE          ( 1 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_STATUS              ( 2 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_TEMP                ( 3 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_BATT_CHARGE         ( 4 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_BATT_ESTRUN         ( 5 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_BATT_VOLT           ( 6 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_BATT_VOLT_NOMINAL   ( 7 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_INPUT_VOLT          ( 8 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_INPUT_VOLT_NOMINAL  ( 9 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_INPUT_CURR          (10 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_INPUT_CURR_NOMINAL  (11 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_INPUT_FREQ          (12 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_INPUT_FREQ_NOMINAL  (13 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_OUTPUT_VOLT         (14 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_OUTPUT_VOLT_NOMINAL (15 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_OUTPUT_CURR         (16 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_OUTPUT_CURR_NOMINAL (17 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_OUTPUT_FREQ         (18 + NETDATA_UPSD_PRIO)
#define NETDATA_UPSD_PRIO_OUTPUT_FREQ_NOMINAL (19 + NETDATA_UPSD_PRIO)

// This macro defines the number of arguments in the "LIST UPS" query.
// Since "LIST" is implied, there is only one argument: "UPS".
#define LISTUPS_NUMQ 1
// This macro defines the number of arguments in the "LIST VAR <UPS>" query.
// Since "LIST" is implied, there are two arguments: "VAR" and the UPS identifier.
#define LISTVAR_NUMQ 2

// These macros contain the indices of the "LIST VAR <UPS>" query's answer.
#define LISTVAR_ANS_VAR       0 // "VAR"
#define LISTVAR_ANS_UPS_NAME  1 // <UPS>
#define LISTVAR_ANS_VAR_NAME  2 // <variable>
#define LISTVAR_ANS_VAR_VALUE 3 // <value>

#define UPS_STATUS_BIT_OL      0x0001
#define UPS_STATUS_BIT_OB      0x0002
#define UPS_STATUS_BIT_LB      0x0004
#define UPS_STATUS_BIT_HB      0x0008
#define UPS_STATUS_BIT_RB      0x0010
#define UPS_STATUS_BIT_CHRG    0x0020
#define UPS_STATUS_BIT_DISCHRG 0x0040
#define UPS_STATUS_BIT_BYPASS  0x0080
#define UPS_STATUS_BIT_CAL     0x0100
#define UPS_STATUS_BIT_OFF     0x0200
#define UPS_STATUS_BIT_OVER    0x0400
#define UPS_STATUS_BIT_TRIM    0x0800
#define UPS_STATUS_BIT_BOOST   0x1000
#define UPS_STATUS_BIT_FSD     0x2000
#define UPS_STATUS_BIT_OTHER   0x4000

// This function parses the 'ups.status' variable and emits the Netdata metrics
// for each status, printing 1 for each set status and 0 otherwise.
static inline void print_ups_status_metrics(const char *ups_name, const char *value)
{
        uint16_t status = 0;

        for (const char *c = value; *c; c++) {
                switch (*c) {
                case ' ':
                        continue;
                case 'L':
                        c++;
                        status |= UPS_STATUS_BIT_LB;
                        break;
                case 'H':
                        c++;
                        status |= UPS_STATUS_BIT_HB;
                        break;
                case 'R':
                        c++;
                        status |= UPS_STATUS_BIT_RB;
                        break;
                case 'D':
                        c += 6;
                        status |= UPS_STATUS_BIT_DISCHRG;
                        break;
                case 'T':
                        c += 3;
                        status |= UPS_STATUS_BIT_TRIM;
                        break;
                case 'F':
                        c += 2;
                        status |= UPS_STATUS_BIT_FSD;
                        break;
                case 'B':
                        switch (*++c) {
                        case 'O':
                                c += 3;
                                status |= UPS_STATUS_BIT_BOOST;
                                break;
                        case 'Y':
                                c += 4;
                                status |= UPS_STATUS_BIT_BYPASS;
                                break;
                        default:
                                status |= UPS_STATUS_BIT_OTHER;
                                break;
                        }
                case 'C':
                        switch (*++c) {
                        case 'H':
                                c += 2;
                                status |= UPS_STATUS_BIT_CHRG;
                                break;
                        case 'A':
                                c++;
                                status |= UPS_STATUS_BIT_CAL;
                                break;
                        default:
                                status |= UPS_STATUS_BIT_OTHER;
                                break;
                        }
                case 'O':
                        switch (*++c) {
                        case 'B':
                                status |= UPS_STATUS_BIT_OB;
                                break;
                        case 'F':
                                status |= UPS_STATUS_BIT_OFF;
                                break;
                        case 'L':
                                status |= UPS_STATUS_BIT_OL;
                                break;
                        case 'V':
                                c += 2;
                                status |= UPS_STATUS_BIT_OVER;
                                break;
                        default:
                                /* fallthrough */
                        }
                default:
                        status |= UPS_STATUS_BIT_OTHER;
                        break;
                }
        }

        printf("BEGIN %s.status\n"
               "SET ups_%s_ups.status.OL = %u\n"
               "SET ups_%s_ups.status.OB = %u\n"
               "SET ups_%s_ups.status.LB = %u\n"
               "SET ups_%s_ups.status.HB = %u\n"
               "SET ups_%s_ups.status.RB = %u\n"
               "SET ups_%s_ups.status.CHRG = %u\n"
               "SET ups_%s_ups.status.DISCHRG = %u\n"
               "SET ups_%s_ups.status.BYPASS = %u\n"
               "SET ups_%s_ups.status.CAL = %u\n"
               "SET ups_%s_ups.status.OFF = %u\n"
               "SET ups_%s_ups.status.OVER = %u\n"
               "SET ups_%s_ups.status.TRIM = %u\n"
               "SET ups_%s_ups.status.BOOST = %u\n"
               "SET ups_%s_ups.status.FSD = %u\n"
               "SET ups_%s_ups.status.other = %u\n"
               "END\n",
               ups_name,
               ups_name, !!(status & UPS_STATUS_BIT_OL),
               ups_name, !!(status & UPS_STATUS_BIT_OB),
               ups_name, !!(status & UPS_STATUS_BIT_LB),
               ups_name, !!(status & UPS_STATUS_BIT_HB),
               ups_name, !!(status & UPS_STATUS_BIT_RB),
               ups_name, !!(status & UPS_STATUS_BIT_CHRG),
               ups_name, !!(status & UPS_STATUS_BIT_DISCHRG),
               ups_name, !!(status & UPS_STATUS_BIT_BYPASS),
               ups_name, !!(status & UPS_STATUS_BIT_CAL),
               ups_name, !!(status & UPS_STATUS_BIT_OFF),
               ups_name, !!(status & UPS_STATUS_BIT_OVER),
               ups_name, !!(status & UPS_STATUS_BIT_TRIM),
               ups_name, !!(status & UPS_STATUS_BIT_BOOST),
               ups_name, !!(status & UPS_STATUS_BIT_FSD),
               ups_name, !!(status & UPS_STATUS_BIT_OTHER));
}

int main(int argc, char *argv[])
{
        size_t numa;
        char **listups_answer[1], **listvar_answer[1];
        int listups_status, listvar_status;
        const char *listups_query[LISTUPS_NUMQ] = { "UPS" };
        const char *listvar_query[LISTVAR_NUMQ] = { "VAR" };
        UPSCONN_t listups_conn, listvar_conn;
        htss_t var_chart_ht;
        htss_t clean_ups_names_ht;

        // If we fail to initialize libupsclient or connect to a local
        // UPS, then there's nothing more to be done; Netdata should disable
        // this plugin, since it cannot offer any metrics.
        if (-1 == upscli_init(0, NULL, NULL, NULL)) {
                fputs("error: failed to initialize libupsclient", stderr);
                puts("DISABLE");
                exit(NETDATA_UPSD_EXIT_AND_DISABLE);
        }

        // TODO: get address/port from configuration file
        if ((-1 == upscli_connect(&listups_conn, "127.0.0.1", 3493, 0)) ||
            (-1 == upscli_connect(&listvar_conn, "127.0.0.1", 3493, 0))) {
                upscli_cleanup();
                fputs("error: failed to connect to upsd at 127.0.0.1:3493", stderr);
                puts("DISABLE");
                exit(NETDATA_UPSD_EXIT_AND_DISABLE);
        }

        htss_init(&var_chart_ht, strhash, strkeyeq);
        htss_init(&clean_ups_names_ht, strhash, strkeyeq);

        // Populate a hash table with the NUT variable names and their corresponding chart IDs.
        htss_set(&var_chart_ht, "battery.charge", "battery_charge_percentage");
        htss_set(&var_chart_ht, "battery.runtime", "battery_estimated_runtime");
        htss_set(&var_chart_ht, "battery.voltage", "battery_voltage");
        htss_set(&var_chart_ht, "battery.voltage.nominal", "battery_voltage_nominal");
        htss_set(&var_chart_ht, "input.voltage", "input_voltage");
        htss_set(&var_chart_ht, "input.voltage.nominal", "input_voltage_nominal");
        htss_set(&var_chart_ht, "input.current", "input_current");
        htss_set(&var_chart_ht, "input.current.nominal", "input_current_nominal");
        htss_set(&var_chart_ht, "input.frequency", "input_frequency");
        htss_set(&var_chart_ht, "input.frequency.nominal", "input_frequency_nominal");
        htss_set(&var_chart_ht, "output.voltage", "output_voltage");
        htss_set(&var_chart_ht, "output.voltage.nominal", "output_voltage_nominal");
        htss_set(&var_chart_ht, "output.current", "output_current");
        htss_set(&var_chart_ht, "output.current.nominal", "output_current_nominal");
        htss_set(&var_chart_ht, "output.frequency", "output_frequency");
        htss_set(&var_chart_ht, "output.frequency.nominal", "output_frequency_nominal");
        htss_set(&var_chart_ht, "ups.load", "ups_load_percentage");
        htss_set(&var_chart_ht, "ups.temperature", "temperature");

        // Query upsd for UPSes with the 'LIST UPS' command.
        if (-1 == upscli_list_start(&listups_conn, LISTUPS_NUMQ, listups_query)) {
                fprintf(stderr, "error: failed to 'LIST UPS': libupsclient: %s\n",
                    upscli_strerror(&listups_conn)
                );
        }

        do {
                listups_status = upscli_list_next(&listups_conn, LISTUPS_NUMQ, listups_query, &numa, (char***)&listups_answer);

                // Unfortunately, upscli_list_next() will emit the list delimiter
                // "END LIST UPS" as its last iteration before returning 0. We don't
                // need it, so let's skip processing on that item.
                 if (!strcmp("END", listups_answer[0][0]))
                         continue;

                // TODO: clean up the UPS name by replacing spaces and periods with underscores.
                //const char *ups_name = htss_get(&clean_ups_names_ht, listups_answer[0][LISTUPS_NUMQ]);
                const char *ups_name = listups_answer[0][LISTUPS_NUMQ];


                // Query upsd for UPS properties with the 'LIST VAR <ups>' command.
                listvar_query[1] = ups_name;
                if (-1 == upscli_list_start(&listvar_conn, 2, listvar_query)) {
                        fprintf(stderr, "error: failed to 'LIST VAR %s': libupsclient: %s\n",
                            ups_name, upscli_strerror(&listvar_conn)
                        );
                }

                do {
                        listvar_status = upscli_list_next(&listvar_conn, LISTVAR_NUMQ, listvar_query, &numa, (char***)&listvar_answer);

                        // Unfortunately, upscli_list_next() will emit the list delimiter
                        // "END LIST VAR" as its last iteration before returning 0. We don't
                        // need it, so let's skip processing on that item.
                        if (numa < 4) continue;

                        const char *var_name = listvar_answer[0][LISTVAR_ANS_VAR_NAME];
                        const char *var_value = listvar_answer[0][LISTVAR_ANS_VAR_VALUE];

                        // The 'ups.status' variable is a special case, because its chart has more
                        // than one dimension. So, we can't simply print one data point.
                        if (!strcmp(var_name, "ups.status")) {
                                print_ups_status_metrics(ups_name, var_value);
                                continue;
                        }

                        if (htss_has(&var_chart_ht, var_name)) {
                                printf("BEGIN %s.%s\n"
                                       "SET ups_%s_%s = %s\n"
                                       "END\n",
                                       ups_name, htss_get(&var_chart_ht, var_name),
                                       ups_name, var_name, var_value);
                        }
                } while (1 == listvar_status);

                if (-1 == listvar_status) {
                        fprintf(stderr, "error: failed to finish 'LIST VAR %s': libupsclient: %s\n",
                            ups_name, upscli_strerror(&listvar_conn)
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

        htss_uninit(&var_chart_ht);
        htss_uninit(&clean_ups_names_ht);

        upscli_disconnect(&listups_conn);
        upscli_disconnect(&listvar_conn);
        upscli_cleanup();
}
