/*****************************************************************************
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*
*****************************************************************************/

#define TEST_CHECKS_C
#define NSCORE 1
#include "config.h"
#include "comments.h"
#include "common.h"
#include "statusdata.h"
#include "downtime.h"
#include "macros.h"
#include "nagios.h"
#include "broker.h"
#include "perfdata.h"
#include "../lib/lnag-utils.h"
#include "tap.h"
#include "stub_sehandlers.c"
#include "stub_comments.c"
#include "stub_perfdata.c"
#include "stub_downtime.c"
#include "stub_notifications.c"
#include "stub_logging.c"
#include "stub_broker.c"
#include "stub_macros.c"
#include "stub_workers.c"
#include "stub_events.c"
#include "stub_statusdata.c"
#include "stub_flapping.c"
#include "stub_nebmods.c"
#include "stub_netutils.c"
#include "stub_commands.c"
#include "stub_xodtemplate.c"

int date_format;

service         * svc1          = NULL;
host            * hst1          = NULL;
check_result    * chk_result    = NULL;

int found_log_rechecking_host_when_service_wobbles = 0;
int found_log_run_async_host_check                 = 0;
int hst1_notifications  = 0;
int svc1_notifications  = 0;
int hst1_event_handlers = 0;
int svc1_event_handlers = 0;
int hst1_logs           = 0;
int svc1_logs           = 0;
int c                   = 0;

void free_hst1()
{
    if (hst1 != NULL) {
        my_free(hst1->name);
        my_free(hst1->address);
        my_free(hst1->plugin_output);
        my_free(hst1->long_plugin_output);
        my_free(hst1->perf_data);
        my_free(hst1->check_command);
        my_free(hst1);
    }
}

void free_svc1()
{
    if (svc1 != NULL) {
        my_free(svc1->host_name);
        my_free(svc1->description);
        my_free(svc1->plugin_output);
        my_free(svc1->long_plugin_output);
        my_free(svc1->perf_data);
        my_free(svc1->check_command);
        my_free(svc1);
    }
}

void free_chk_result()
{
    if (chk_result != NULL) {
        my_free(chk_result->host_name);
        my_free(chk_result->service_description);
        my_free(chk_result->output);
        my_free(chk_result);
    }
}

void free_all()
{
    free_hst1();
    free_svc1();
    free_chk_result();
}

int service_notification(service *svc, int type, char *not_author, char *not_data, int options)
{
    svc1_notifications++;
    return OK;
}

int host_notification(host *hst, int type, char *not_author, char *not_data, int options)
{
    hst1_notifications++;
    return OK;
}

int handle_host_event(host *hst) 
{
    hst1_event_handlers++;
    return OK;
}

int handle_service_event(service *svc) 
{
    svc1_event_handlers++;
    return OK;
}

int log_host_event(host *hst)
{
    hst1_logs++;
    return OK;
}

int log_service_event(service *svc)
{
    svc1_logs++;
    return OK;
}

void adjust_check_result_output(char * output)
{
    my_free(chk_result->output);
    chk_result->output = strdup(output);
}

void setup_check_result(int check_type)
{
    struct timeval start_time, finish_time;

    start_time.tv_sec = 1234567890L;
    start_time.tv_usec = 0L;

    finish_time.tv_sec = 1234567891L;
    finish_time.tv_usec = 0L;

    free_chk_result();

    chk_result = (check_result *) calloc(1, sizeof(check_result));

    chk_result->host_name           = strdup("hst1");
    chk_result->service_description = strdup("Normal service");
    chk_result->check_type          = check_type;
    chk_result->check_options       = CHECK_OPTION_NONE;
    chk_result->scheduled_check     = TRUE;
    chk_result->reschedule_check    = TRUE;
    chk_result->exited_ok           = TRUE;
    chk_result->return_code         = STATE_OK | HOST_UP;
    chk_result->early_timeout       = FALSE;
    chk_result->latency             = 0.5;
    chk_result->start_time          = start_time;
    chk_result->finish_time         = finish_time;

    adjust_check_result_output("Fake result");
}

void adjust_check_result(int check_type, int return_code, char * output)
{
    setup_check_result(check_type);
    adjust_check_result_output(output);

    chk_result->return_code = return_code;
}

int update_program_status(int aggregated_dump)
{
    c++;
    /* printf ("# In the update_program_status hook: %d\n", c); */

    /* Set this to break out of event_execution_loop */
    if (c > 10) {
        sigshutdown = TRUE;
        c = 0;
    }

    return OK;
}

int log_debug_info(int level, int verbosity, const char *fmt, ...)
{
    va_list ap;
    char *buffer = NULL;

    va_start(ap, fmt);
    /* vprintf( fmt, ap ); */
    vasprintf(&buffer, fmt, ap);

    if (strcmp(buffer, "Service wobbled between non-OK states, so we'll recheck the host state...\n") == 0) {
        found_log_rechecking_host_when_service_wobbles++;
    }
    else if (strcmp(buffer, "run_async_host_check()\n") == 0) {
        found_log_run_async_host_check++;
    }

    /*
    printf("DEBUG: %s", buffer);
    */

    free(buffer);
    va_end(ap);

    return OK;
}

void setup_objects(time_t time)
{
    enable_predictive_service_dependency_checks = FALSE;

    free_hst1();
    free_svc1();

    hst1 = (host *) calloc(1, sizeof(host));
    svc1 = (service *) calloc(1, sizeof(service));
    
    hst1->name                         = strdup("hst1");
    hst1->address                      = strdup("127.0.0.1");
    hst1->retry_interval               = 1;
    hst1->check_interval               = 5;
    hst1->current_attempt              = 1;
    hst1->max_attempts                 = 4;
    hst1->check_options                = 0;
    hst1->state_type                   = SOFT_STATE;
    hst1->current_state                = HOST_DOWN;
    hst1->has_been_checked             = TRUE;
    hst1->last_check                   = time;
    hst1->next_check                   = time;

    svc1->host_name                     = strdup("hst1");
    svc1->host_ptr                      = hst1;
    svc1->description                   = strdup("Normal service");
    svc1->check_options                 = 0;
    svc1->next_check                    = time;
    svc1->state_type                    = SOFT_STATE;
    svc1->current_state                 = STATE_CRITICAL;
    svc1->retry_interval                = 1;
    svc1->check_interval                = 5;
    svc1->current_attempt               = 1;
    svc1->max_attempts                  = 4;
    svc1->last_state_change             = 0;
    svc1->last_state_change             = 0;
    svc1->last_check                    = (time_t)1234560000;
    svc1->host_problem_at_last_check    = FALSE;
    svc1->plugin_output                 = strdup("Initial state");
    svc1->last_hard_state_change        = (time_t)1111111111;
    svc1->accept_passive_checks         = 1;
}

void run_check_tests(int check_type, time_t when)
{
    hst1_notifications  = 0;
    svc1_notifications  = 0;
    hst1_event_handlers = 0;
    svc1_event_handlers = 0;
    hst1_logs           = 0;
    svc1_logs           = 0;

    /* Test:
        to confirm that if a service is warning, the notified_on_critical is reset 
    */
    setup_objects(when);
    adjust_check_result(check_type, STATE_WARNING, "Warning - check notified_on_critical reset");

    chk_result->object_check_type       = SERVICE_CHECK;

    svc1->last_state                    = STATE_CRITICAL;
    svc1->notification_options          = OPT_CRITICAL;
    svc1->current_notification_number   = 999;
    svc1->last_notification             = (time_t)11111;
    svc1->next_notification             = (time_t)22222;
    svc1->no_more_notifications         = TRUE;

    handle_async_service_check_result(svc1, chk_result);

    ok(svc1->last_notification == (time_t)0, 
        "last notification reset due to state change");
    ok(svc1->next_notification == (time_t)0, 
        "next notification reset due to state change");
    ok(svc1->no_more_notifications == FALSE, 
        "no_more_notifications reset due to state change");
    ok(svc1->current_notification_number == 999, 
        "notification number NOT reset");
    ok(svc1_notifications == 1,
        "contacts were notified");
    ok(svc1_logs == 0,
        "state change didnt show up in log");



    /* Test:
        service that transitions from OK to CRITICAL (where its host is set to DOWN) will get set to a hard state
        even though check attempts = 1 of 4
    */
    setup_objects((time_t) 1234567800L);
    adjust_check_result(check_type, STATE_CRITICAL, "CRITICAL failure");

    svc1->current_state = STATE_OK;
    svc1->state_type    = HARD_STATE;

    handle_async_service_check_result(svc1, chk_result);

    ok(svc1->last_hard_state_change == (time_t)1234567890, 
        "Got last_hard_state_change time=%lu", svc1->last_hard_state_change);
    ok(svc1->last_state_change == svc1->last_hard_state_change, 
        "Got same last_state_change");
    ok(svc1->last_hard_state == 2, 
        "Should save the last hard state as critical for next time");
    ok(svc1->host_problem_at_last_check == TRUE, 
        "Got host_problem_at_last_check set to TRUE due to host failure - this needs to be saved otherwise extra alerts raised in subsequent runs");
    ok(svc1->state_type == HARD_STATE, 
        "This should be a HARD state since the host is in a failure state");
    ok(svc1->current_attempt == 1, 
        "Previous status was OK, so this failure should show current_attempt=1") 
        || diag("Current attempt=%d", svc1->current_attempt);



    /* Test:
        OK -> WARNING 1/4 -> ack -> WARNING 2/4 -> OK transition
        Tests that the ack is left for 2/4
    */
    setup_objects(when);
    adjust_check_result(check_type, STATE_WARNING, "WARNING failure");

    hst1->current_state     = HOST_UP;

    svc1->last_state        = STATE_OK;
    svc1->last_hard_state   = STATE_OK;
    svc1->current_state     = STATE_OK;
    svc1->state_type        = SOFT_STATE;

    handle_async_service_check_result(svc1, chk_result);

    ok(svc1->last_notification == (time_t)0, 
        "last notification reset due to state change");
    ok(svc1->next_notification == (time_t)0, 
        "next notification reset due to state change");
    ok(svc1->no_more_notifications == FALSE, 
        "no_more_notifications reset due to state change");
    ok(svc1->current_notification_number == 0, 
        "notification number reset");
    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
        "No acks");

    svc1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;

    adjust_check_result(check_type, STATE_WARNING, "WARNING failure");
    handle_async_service_check_result(svc1, chk_result);

    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
        "Ack left");

    adjust_check_result(check_type, STATE_OK, "Back to OK");
    handle_async_service_check_result(svc1, chk_result);

    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
        "Ack reset to none");



    /* Test:
        OK -> WARNING 1/4 -> ack -> WARNING 2/4 -> WARNING 3/4 -> WARNING 4/4 -> WARNING 4/4 -> OK transition
        Tests that the ack is not removed on hard state change
    */
    setup_objects(when);
    adjust_check_result(check_type, STATE_OK, "Reset to OK");

    hst1->current_state    = HOST_UP;

    svc1->last_state        = STATE_OK;
    svc1->last_hard_state   = STATE_OK;
    svc1->current_state     = STATE_OK;
    svc1->state_type        = SOFT_STATE;
    svc1->current_attempt   = 1;

    handle_async_service_check_result(svc1, chk_result);

    ok(svc1->current_attempt == 1, 
        "Current attempt is 1")
        || diag("Current attempt now: %d", svc1->current_attempt);

    adjust_check_result(check_type, STATE_WARNING, "WARNING failure");
    handle_async_service_check_result(svc1, chk_result);

    ok(svc1->state_type == SOFT_STATE, 
        "Soft state");
    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
        "No acks - testing transition to hard warning state");

    svc1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;

    ok(svc1->current_attempt == 1, 
        "Current attempt is 1") 
        || diag("Current attempt now: %d", svc1->current_attempt);

    adjust_check_result(check_type, STATE_WARNING, "WARNING failure 2");
    handle_async_service_check_result(svc1, chk_result);

    ok(svc1->state_type == SOFT_STATE, 
        "Soft state");
    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
        "Ack left");
    ok(svc1->current_attempt == 2, 
        "Current attempt is 2") 
        || diag("Current attempt now: %d", svc1->current_attempt);

    adjust_check_result(check_type, STATE_WARNING, "WARNING failure 3");
    handle_async_service_check_result(svc1, chk_result);

    ok(svc1->state_type == SOFT_STATE, 
        "Soft state");
    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
        "Ack left");
    ok(svc1->current_attempt == 3, 
        "Current attempt is 3") 
        || diag("Current attempt now: %d", svc1->current_attempt);

    adjust_check_result(check_type, STATE_WARNING, "WARNING failure 4");
    handle_async_service_check_result(svc1, chk_result);

    ok(svc1->state_type == HARD_STATE, 
        "Hard state");
    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
        "Ack left on hard failure");
    ok(svc1->current_attempt == 4, 
        "Current attempt is 4") 
        || diag("Current attempt now: %d", svc1->current_attempt);

    adjust_check_result(check_type, STATE_OK, "Back to OK");
    handle_async_service_check_result(svc1, chk_result);

    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
        "Ack removed");



    /* Test:
        OK -> WARNING 1/1 -> ack -> WARNING -> OK transition
        Tests that the ack is not removed on 2nd warning, but is on OK
    */
    setup_objects(when);
    adjust_check_result(check_type, STATE_WARNING, "WARNING failure 1");

    hst1->current_state     = HOST_UP;

    svc1->last_state        = STATE_OK;
    svc1->last_hard_state   = STATE_OK;
    svc1->current_state     = STATE_OK;
    svc1->state_type        = SOFT_STATE;
    svc1->max_attempts      = 2;

    handle_async_service_check_result(svc1, chk_result);

    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
        "No acks - testing transition to immediate hard then OK");

    svc1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;

    adjust_check_result(check_type, STATE_WARNING, "WARNING failure 2");
    handle_async_service_check_result(svc1, chk_result);

    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
        "Ack left");

    adjust_check_result(check_type, STATE_OK, "Back to OK");
    handle_async_service_check_result(svc1, chk_result);

    ok(svc1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
        "Ack removed");



    /* Test:
        UP -> DOWN 1/4 -> ack -> DOWN 2/4 -> DOWN 3/4 -> DOWN 4/4 -> UP transition
        Tests that the ack is not removed on 2nd DOWN, but is on UP
    */
    setup_objects(when);
    adjust_check_result(HOST_CHECK_PASSIVE, HOST_DOWN, "DOWN failure 2");

    hst1->current_state             = HOST_UP;
    hst1->last_state                = HOST_UP;
    hst1->last_hard_state           = HOST_UP;
    hst1->state_type                = SOFT_STATE;
    hst1->acknowledgement_type      = ACKNOWLEDGEMENT_NONE;
    hst1->plugin_output             = strdup("");
    hst1->long_plugin_output        = strdup("");
    hst1->perf_data                 = strdup("");
    hst1->check_command             = strdup("Dummy command required");
    hst1->accept_passive_checks     = TRUE;

    passive_host_checks_are_soft    = TRUE;

    handle_async_host_check_result(hst1, chk_result);

    ok(hst1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
        "No ack set");
    ok(hst1->current_attempt == 2, 
        "Attempts right (not sure why this goes into 2 and not 1)") 
        || diag("current_attempt=%d", hst1->current_attempt);
    ok(strcmp(hst1->plugin_output, "DOWN failure 2") == 0, 
        "output set") 
        || diag("plugin_output=%s", hst1->plugin_output);

    hst1->acknowledgement_type = ACKNOWLEDGEMENT_NORMAL;

    adjust_check_result(HOST_CHECK_PASSIVE, HOST_DOWN, "DOWN failure 3");
    handle_async_host_check_result(hst1, chk_result);

    ok(hst1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
        "Ack should be retained as in soft state");
    ok(hst1->current_attempt == 3, 
        "Attempts incremented") 
        || diag("current_attempt=%d", hst1->current_attempt);
    ok(strcmp(hst1->plugin_output, "DOWN failure 3") == 0, 
        "output set") 
        || diag("plugin_output=%s", hst1->plugin_output);

    adjust_check_result(HOST_CHECK_PASSIVE, HOST_DOWN, "DOWN failure 4");
    handle_async_host_check_result(hst1, chk_result);

    ok(hst1->acknowledgement_type == ACKNOWLEDGEMENT_NORMAL, 
        "Ack should be retained as in soft state");
    ok(hst1->current_attempt == 4, 
        "Attempts incremented") 
        || diag("current_attempt=%d", hst1->current_attempt);
    ok(strcmp(hst1->plugin_output, "DOWN failure 4") == 0, 
        "output set") 
        || diag("plugin_output=%s", hst1->plugin_output);

    adjust_check_result(HOST_CHECK_PASSIVE, HOST_UP, "UP again");
    handle_async_host_check_result(hst1, chk_result);

    ok(hst1->acknowledgement_type == ACKNOWLEDGEMENT_NONE, 
        "Ack reset due to state change");
    ok(hst1->current_attempt == 1, 
        "Attempts reset") 
        || diag("current_attempt=%d", hst1->current_attempt);
    ok(strcmp(hst1->plugin_output, "UP again") == 0, 
        "output set") 
        || diag("plugin_output=%s", hst1->plugin_output);

    free_all();
}

int main(int argc, char **argv)
{
    time_t now = 0L;

    execute_host_checks             = TRUE;
    execute_service_checks          = TRUE;
    accept_passive_host_checks      = TRUE;
    accept_passive_service_checks   = TRUE;

    plan_tests(96);

    time(&now);

    run_check_tests(CHECK_TYPE_ACTIVE, now);
    run_check_tests(CHECK_TYPE_PASSIVE, now);


    return exit_status();
}
