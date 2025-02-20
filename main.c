#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <genht/htss.h>
#include <genht/hash.h>

#include <upsclient.h>

// https://learn.netdata.cloud/docs/developer-and-contributor-corner/external-plugins#operation
#define NETDATA_PLUGIN_EXIT_AND_RESTART 0
// https://learn.netdata.cloud/docs/developer-and-contributor-corner/external-plugins#disable
#define NETDATA_PLUGIN_EXIT_AND_DISABLE 1

// https://learn.netdata.cloud/docs/developer-and-contributor-corner/external-plugins/#clabel
#define NETDATA_PLUGIN_CLABEL_SOURCE_AUTO   1
#define NETDATA_PLUGIN_CLABEL_SOURCE_MANUAL 2
#define NETDATA_PLUGIN_CLABEL_SOURCE_K8     4
#define NETDATA_PLUGIN_CLABEL_SOURCE_AGENT  8

// This macro defines the number of arguments in the "LIST UPS" query.
// Since "LIST" is implied, there is only one argument: "UPS".
#define LISTUPS_NUMQ 1
// This macro defines the number of arguments in the "LIST VAR <UPS>" query.
// Since "LIST" is implied, there are two arguments: "VAR" and the UPS identifier.
#define LISTVAR_NUMQ 2

// https://networkupstools.org/docs/developer-guide.chunked/new-drivers.html#_status_data
struct nut_ups_status {
    unsigned int OL      : 1; // On line
    unsigned int OB      : 1; // On battery
    unsigned int LB      : 1; // Low battery
    unsigned int HB      : 1; // High battery
    unsigned int RB      : 1; // The battery needs to be replaced
    unsigned int CHRG    : 1; // The battery is charging
    unsigned int DISCHRG : 1; // The battery is discharging (inverter is providing load power)
    unsigned int BYPASS  : 1; // UPS bypass circuit is active -- no battery protection is available
    unsigned int CAL     : 1; // UPS is currently performing runtime calibration (on battery)
    unsigned int OFF     : 1; // UPS is offline and is not supplying power to the load
    unsigned int OVER    : 1; // UPS is overloaded
    unsigned int TRIM    : 1; // UPS is trimming incoming voltage (called "buck" in some hardware)
    unsigned int BOOST   : 1; // UPS is boosting incoming voltage
    unsigned int FSD     : 1; // Forced Shutdown
    unsigned int OTHER   : 1;
};

// https://learn.netdata.cloud/docs/developer-and-contributor-corner/external-plugins/#chart
struct ups_var_chart {
    const char *name;
    const char *chart_id;
    const char *chart_name;
    const char *chart_title;
    const char *chart_units;
    const char *chart_family;
    const char *chart_context;
    const char *chart_type;
    unsigned int chart_priority;
    const size_t chart_dimlength;
    const char *chart_dimension[15];
};

const struct ups_var_chart ups_var_charts[] = {
    {
        .name = "ups.load",
        .chart_id = "load_percentage",
        .chart_title = "UPS load",
        .chart_units = "percentage",
        .chart_family = "ups",
        .chart_context = "upsd.ups_load",
        .chart_type = "area",
        .chart_priority = 70000,
        .chart_dimlength = 1,
        .chart_dimension = { "load" },
    },
    {
        // TODO: this is not a real variable from NUT
        .name = "ups.load_usage",
        .chart_id = "load_usage",
        .chart_title = "UPS load usage (power output)",
        .chart_units = "Watts",
        .chart_family = "ups",
        .chart_context = "upsd.ups_load_usage",
        .chart_type = "line",
        .chart_priority = 70001,
        .chart_dimlength = 1,
        .chart_dimension = { "load_usage" },
    },
    {
        .name = "ups.status",
        .chart_id = "status",
        .chart_title = "UPS status",
        .chart_units = "status",
        .chart_family = "ups",
        .chart_context = "upsd.ups_status",
        .chart_type = "line",
        .chart_priority = 70002,
        .chart_dimlength = 15,
        .chart_dimension = {
            "on_line",
            "on_battery",
            "low_battery",
            "high_battery",
            "replace_battery",
            "charging",
            "discharging",
            "bypass",
            "calibration",
            "offline",
            "overloaded",
            "trim_input_voltage",
            "boost_input_voltage",
            "forced_shutdown",
            "other",
        },
    },
    {
        .name = "ups.temperature",
        .chart_id = "temperature",
        .chart_title = "UPS temperature",
        .chart_units = "Celsius",
        .chart_family = "ups",
        .chart_context = "upsd.ups_temperature",
        .chart_type = "line",
        .chart_priority = 70003,
        .chart_dimlength = 1,
        .chart_dimension = { "temperature" },
    },
    {
        .name = "battery.charge",
        .chart_id = "battery_charge_percentage",
        .chart_title = "UPS Battery charge",
        .chart_units = "percentage",
        .chart_family = "battery",
        .chart_context = "upsd.ups_battery_charge",
        .chart_type = "area",
        .chart_priority = 70004,
        .chart_dimlength = 1,
        .chart_dimension = { "charge" },
    },
    {
        .name = "battery.runtime",
        .chart_id = "battery_estimated_runtime",
        .chart_title = "UPS Battery estimated runtime",
        .chart_units = "seconds",
        .chart_family = "battery",
        .chart_context = "upsd.ups_battery_estimated_runtime",
        .chart_type = "line",
        .chart_priority = 70005,
        .chart_dimlength = 1,
        .chart_dimension = { "runtime" },
    },
    {
        .name = "battery.voltage",
        .chart_id = "battery_voltage",
        .chart_title = "UPS Battery voltage",
        .chart_units = "Volts",
        .chart_family = "battery",
        .chart_context = "upsd.ups_battery_voltage",
        .chart_type = "line",
        .chart_priority = 70006,
        .chart_dimlength = 1,
        .chart_dimension = { "voltage" },
    },
    {
        .name = "battery.voltage.nominal",
        .chart_id = "battery_voltage_nominal",
        .chart_title = "UPS Battery voltage nominal",
        .chart_units = "Volts",
        .chart_family = "battery",
        .chart_context = "upsd.ups_battery_voltage_nominal",
        .chart_type = "line",
        .chart_priority = 70007,
        .chart_dimlength = 1,
        .chart_dimension = { "nominal_voltage" },
    },
    {
        .name = "input.voltage",
        .chart_id = "input_voltage",
        .chart_title = "UPS Input voltage",
        .chart_units = "Volts",
        .chart_family = "input",
        .chart_context = "upsd.ups_input_voltage",
        .chart_type = "line",
        .chart_priority = 70008,
        .chart_dimlength = 1,
        .chart_dimension = { "voltage" },
    },
    {
        .name = "input.voltage.nominal",
        .chart_id = "input_voltage_nominal",
        .chart_title = "UPS Input voltage nominal",
        .chart_units = "Volts",
        .chart_family = "input",
        .chart_context = "upsd.ups_input_voltage_nominal",
        .chart_type = "line",
        .chart_priority = 70009,
        .chart_dimlength = 1,
        .chart_dimension = { "nominal_voltage" },
    },
    {
        .name = "input.current",
        .chart_id = "input_current",
        .chart_title = "UPS Input current",
        .chart_units = "Ampere",
        .chart_family = "input",
        .chart_context = "upsd.ups_input_current",
        .chart_type = "line",
        .chart_priority = 70010,
        .chart_dimlength = 1,
        .chart_dimension = { "current" },
    },
    {
        .name = "input.current.nominal",
        .chart_id = "input_current_nominal",
        .chart_title = "UPS Input current nominal",
        .chart_units = "Ampere",
        .chart_family = "input",
        .chart_context = "upsd.ups_input_current_nominal",
        .chart_type = "line",
        .chart_priority = 70011,
        .chart_dimlength = 1,
        .chart_dimension = { "nominal_current" },
    },
    {
        .name = "input.frequency",
        .chart_id = "input_frequency",
        .chart_title = "UPS Input frequency",
        .chart_units = "Hz",
        .chart_family = "input",
        .chart_context = "upsd.ups_input_frequency",
        .chart_type = "line",
        .chart_priority = 70012,
        .chart_dimlength = 1,
        .chart_dimension = { "frequency" },
    },
    {
        .name = "input.frequency.nominal",
        .chart_id = "input_frequency_nominal",
        .chart_title = "UPS Input frequency nominal",
        .chart_units = "Hz",
        .chart_family = "input",
        .chart_context = "upsd.ups_input_frequency_nominal",
        .chart_type = "line",
        .chart_priority = 70013,
        .chart_dimlength = 1,
        .chart_dimension = { "nominal_frequency" },
    },
    {
        .name = "output.voltage",
        .chart_id = "output_voltage",
        .chart_title = "UPS Output voltage",
        .chart_units = "Volts",
        .chart_family = "output",
        .chart_context = "upsd.ups_output_voltage",
        .chart_type = "line",
        .chart_priority = 70014,
        .chart_dimlength = 1,
        .chart_dimension = { "voltage" },
    },
    {
        .name = "output.voltage.nominal",
        .chart_id = "output_voltage_nominal",
        .chart_title = "UPS Output voltage nominal",
        .chart_units = "Volts",
        .chart_family = "output",
        .chart_context = "upsd.ups_output_voltage_nominal",
        .chart_type = "line",
        .chart_priority = 70015,
        .chart_dimlength = 1,
        .chart_dimension = { "nominal_voltage" },
    },
    {
        .name = "output.current",
        .chart_id = "output_current",
        .chart_title = "UPS Output current",
        .chart_units = "Ampere",
        .chart_family = "output",
        .chart_context = "upsd.ups_output_current",
        .chart_type = "line",
        .chart_priority = 70016,
        .chart_dimlength = 1,
        .chart_dimension = { "current" },
    },
    {
        .name = "output.current.nominal",
        .chart_id = "output_current_nominal",
        .chart_title = "UPS Output current nominal",
        .chart_units = "Ampere",
        .chart_family = "output",
        .chart_context = "upsd.ups_output_current_nominal",
        .chart_type = "line",
        .chart_priority = 70017,
        .chart_dimlength = 1,
        .chart_dimension = { "nominal_current" },
    },
    {
        .name = "output.frequency",
        .chart_id = "output_frequency",
        .chart_title = "UPS Output frequency",
        .chart_units = "Hz",
        .chart_family = "output",
        .chart_context = "upsd.ups_output_frequency",
        .chart_type = "line",
        .chart_priority = 70018,
        .chart_dimlength = 1,
        .chart_dimension = { "frequency" },
    },
    {
        .name = "output.frequency.nominal",
        .chart_id = "output_frequency_nominal",
        .chart_title = "UPS Output frequency nominal",
        .chart_units = "Hz",
        .chart_family = "output",
        .chart_context = "upsd.ups_output_frequency_nominal",
        .chart_type = "line",
        .chart_priority = 70019,
        .chart_dimlength = 1,
        .chart_dimension = { "nominal_frequency" },
    },
    { 0 },
};

char *clean_name(char *buf, size_t bufsize, const char *name)
{
    assert(buf);
    assert(name);

    for (size_t i = 0; i < bufsize; i++) {
        buf[i] = (name[i] == ' ' || name[i] == '.') ? '_': name[i];
        if (name[i] == '\0')
            break;
        if (i+1 == bufsize)
            buf[i] = '\0';
    }
    return buf;
}

void print_netdata_charts(const char *ups_name, htss_t *variables_ht)
{
    assert(ups_name);
    assert(variables_ht);

    char buffer[64];

    for (const struct ups_var_chart *chart = ups_var_charts; chart->name; chart++) {
        // Skip metrics that are not available from the UPS.
        if (!htss_has(variables_ht, chart->name))
            continue;

        // TODO: do not hardcode update_every and plugin name
        // CHART type.id name title units [family [context [charttype [priority [update_every [options [plugin [module]]]]]]]]
        printf("CHART '%s.%s' '' '%s' '%s' '%s' '%s' '%s' '%u' '%u' '' '%s'\n",
               clean_name(buffer, sizeof(buffer), ups_name), // type
               chart->chart_id,       // id
               chart->chart_title,    // title
               chart->chart_units,    // units
               chart->chart_family,   // family
               chart->chart_context,  // context
               chart->chart_type,     // charttype
               chart->chart_priority, // priority
               1,                     // update_every
               "upsd");               // plugin

        if (htss_has(variables_ht, "battery.type"))
            printf("CLABEL 'battery_type' '%s' '%u'\n", htss_get(variables_ht, "battery.type"), NETDATA_PLUGIN_CLABEL_SOURCE_AUTO);
        if (htss_has(variables_ht, "device.model"))
            printf("CLABEL 'device_model' '%s' '%u'\n", htss_get(variables_ht, "device.model"), NETDATA_PLUGIN_CLABEL_SOURCE_AUTO);
        if (htss_has(variables_ht, "device.serial"))
            printf("CLABEL 'device_serial' '%s' '%u'\n", htss_get(variables_ht, "device.serial"), NETDATA_PLUGIN_CLABEL_SOURCE_AUTO);
        if (htss_has(variables_ht, "device.mfr"))
            printf("CLABEL 'device_manufacturer' '%s' '%u'\n", htss_get(variables_ht, "device.mfr"), NETDATA_PLUGIN_CLABEL_SOURCE_AUTO);
        if (htss_has(variables_ht, "device.type"))
            printf("CLABEL 'device_type' '%s' '%u'\n", htss_get(variables_ht, "device.type"), NETDATA_PLUGIN_CLABEL_SOURCE_AUTO);
        printf("CLABEL '%s' '%s' '%u'\n"
               "CLABEL '%s' '%s' '%u'\n"
               "CLABEL_COMMIT\n",
               "ups_name", ups_name, NETDATA_PLUGIN_CLABEL_SOURCE_AUTO,
               "_collect_job", "local", NETDATA_PLUGIN_CLABEL_SOURCE_AUTO);

        for (size_t i = 0; i < chart->chart_dimlength; i++)
            printf("DIMENSION '%s'\n", chart->chart_dimension[i]);
    }
}

// This function parses the 'ups.status' variable and emits the Netdata metrics
// for each status, printing 1 for each set status and 0 otherwise.
static inline void print_ups_status_metrics(const char *ups_name, const char *value)
{
    assert(ups_name);
    assert(value);

    struct nut_ups_status status = { 0 };

    for (const char *c = value; *c; c++) {
        switch (*c) {
        case ' ':
            continue;
        case 'L':
            c++;
            status.LB = 1;
            break;
        case 'H':
            c++;
            status.HB = 1;
            break;
        case 'R':
            c++;
            status.RB = 1;
            break;
        case 'D':
            c += 6;
            status.DISCHRG = 1;
            break;
        case 'T':
            c += 3;
            status.TRIM = 1;
            break;
        case 'F':
            c += 2;
            status.FSD = 1;
            break;
        case 'B':
            switch (*++c) {
            case 'O':
                c += 3;
                status.BOOST = 1;
                break;
            case 'Y':
                c += 4;
                status.BYPASS = 1;
                break;
            default:
                status.OTHER = 1;
                break;
            }
        case 'C':
            switch (*++c) {
            case 'H':
                c += 2;
                status.CHRG = 1;
                break;
            case 'A':
                c++;
                status.CAL = 1;
                break;
            default:
                status.OTHER = 1;
                break;
            }
        case 'O':
            switch (*++c) {
            case 'B':
                status.OB = 1;
                break;
            case 'F':
                status.OFF = 1;
                break;
            case 'L':
                status.OL = 1;
                break;
            case 'V':
                c += 2;
                status.OVER = 1;
                break;
            default:
                /* fallthrough */
            }
        default:
            status.OTHER = 1;
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
           ups_name, status.OL,
           ups_name, status.OB,
           ups_name, status.LB,
           ups_name, status.HB,
           ups_name, status.RB,
           ups_name, status.CHRG,
           ups_name, status.DISCHRG,
           ups_name, status.BYPASS,
           ups_name, status.CAL,
           ups_name, status.OFF,
           ups_name, status.OVER,
           ups_name, status.TRIM,
           ups_name, status.BOOST,
           ups_name, status.FSD,
           ups_name, status.OTHER);
}

int main(int argc, char *argv[])
{
    size_t numa;
    char **listups_answer[1], **listvar_answer[1];
    int rc;
    const char *listups_query[LISTUPS_NUMQ] = { "UPS" };
    const char *listvar_query[LISTVAR_NUMQ] = { "VAR" };
    UPSCONN_t listups_conn, listvar_conn;
    htss_t variables_ht;
    htss_t var_chart_ht;
    char buffer[64];

    // If we fail to initialize libupsclient or connect to a local
    // UPS, then there's nothing more to be done; Netdata should disable
    // this plugin, since it cannot offer any metrics.
    if (-1 == upscli_init(0, NULL, NULL, NULL)) {
        fputs("error: failed to initialize libupsclient", stderr);
        puts("DISABLE");
        exit(NETDATA_PLUGIN_EXIT_AND_DISABLE);
    }

    // TODO: get address/port from configuration file
    if ((-1 == upscli_connect(&listups_conn, "127.0.0.1", 3493, 0)) ||
        (-1 == upscli_connect(&listvar_conn, "127.0.0.1", 3493, 0))) {
        upscli_cleanup();
        fputs("error: failed to connect to upsd at 127.0.0.1:3493", stderr);
        puts("DISABLE");
        exit(NETDATA_PLUGIN_EXIT_AND_DISABLE);
    }

    htss_init(&variables_ht, strhash, strkeyeq);
    htss_init(&var_chart_ht, strhash, strkeyeq);

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

    rc = upscli_list_start(&listups_conn, LISTUPS_NUMQ, listups_query);
    assert(-1 != rc);

    // Query upsd for UPSes with the 'LIST UPS' command.
    while ((rc = upscli_list_next(&listups_conn, LISTUPS_NUMQ, listups_query, &numa, (char***)&listups_answer))) {
        assert(-1 != rc);

        // Unfortunately, upscli_list_next() will emit the list delimiter
        // "END LIST UPS" as its last iteration before returning 0. We don't
        // need it, so let's skip processing on that item.
         if (!strcmp("END", listups_answer[0][0]))
             continue;

        char *ups_name = listups_answer[0][LISTUPS_NUMQ];

        // Query upsd for UPS properties with the 'LIST VAR <ups>' command.
        listvar_query[1] = ups_name;
        rc = upscli_list_start(&listvar_conn, 2, listvar_query);
        assert(-1 != rc);

        while ((rc = upscli_list_next(&listvar_conn, LISTVAR_NUMQ, listvar_query, &numa, (char***)&listvar_answer))) {
            assert(-1 != rc);

            // Unfortunately, upscli_list_next() will emit the list delimiter
            // "END LIST VAR" as its last iteration before returning 0. We don't
            // need it, so let's skip processing on that item.
            if (numa < 4) continue;

            // TODO: don't forget to free these strings
            // The output of upscli_list_next() will be something like:
            //   { { [0] = "VAR", [1] = <UPS name>, [2] = <variable name>, [3] = <variable value> } }
            char *variable_name = strdup(listvar_answer[0][2]);
            char *variable_value = strdup(listvar_answer[0][3]);
            htss_set(&variables_ht, variable_name, variable_value);
        }

        print_netdata_charts(ups_name, &variables_ht);
    }

    for (int i = 0; i < 1; i++) {
        rc = upscli_list_start(&listups_conn, LISTUPS_NUMQ, listups_query);
        assert(-1 != rc);

        while ((rc = upscli_list_next(&listups_conn, LISTUPS_NUMQ, listups_query, &numa, (char***)&listups_answer))) {
            assert(-1 != rc);

            if (!strcmp("END", listups_answer[0][0])) continue;

            const char *ups_name = listups_answer[0][LISTUPS_NUMQ];
            const char *clean_ups_name = clean_name(buffer, sizeof(buffer), ups_name);

            // Query upsd for UPS properties with the 'LIST VAR <ups>' command.
            listvar_query[1] = ups_name;
            rc = upscli_list_start(&listvar_conn, 2, listvar_query);
            assert(-1 != rc);

            while ((rc = upscli_list_next(&listvar_conn, LISTVAR_NUMQ, listvar_query, &numa, (char***)&listvar_answer))) {
                assert(-1 != rc);

                if (numa < 4) continue;

                // The output of upscli_list_next() will be something like:
                //   { { [0] = "VAR", [1] = <UPS name>, [2] = <variable name>, [3] = <variable value> } }
                const char *var_name = listvar_answer[0][2];
                const char *var_value = listvar_answer[0][3];

                // The 'ups.status' variable is a special case, because its chart has more
                // than one dimension. So, we can't simply print one data point.
                if (!strcmp(var_name, "ups.status")) {
                    print_ups_status_metrics(clean_ups_name, var_value);
                    continue;
                }

                if (htss_has(&var_chart_ht, var_name)) {
                    printf("BEGIN %s.%s\n"
                           "SET ups_%s_%s = %s\n"
                           "END\n",
                           clean_ups_name, htss_get(&var_chart_ht, var_name),
                           clean_ups_name, var_name, var_value);
                }
            }

        }

        // Flush the data out of the stream buffer to ensure netdata gets it immediately.
        // https://learn.netdata.cloud/docs/developer-and-contributor-corner/external-plugins#the-output-of-the-plugin
        fflush(stdout);

        // TODO: get the sleep duration from argv[1]
        sleep(1);
    }

    htss_uninit(&var_chart_ht);

    upscli_disconnect(&listups_conn);
    upscli_disconnect(&listvar_conn);
    upscli_cleanup();
}
