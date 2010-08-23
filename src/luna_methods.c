#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <syslog.h>
#include "luna_service.h"
#include "keyboss.h"

struct action_map {
  ACTIONS action;
  char name[16];
};

static char message_buf[1024];
static char tmp[256];

static struct action_map available_actions[MAX_ACTIONS] = {
  {ACTION_DEFAULT, "Default"},
  {ACTION_FUNCTION, "Function"},
  {ACTION_CAPITALIZE, "Capitalize"},
  {ACTION_NONE, ""},
  {ACTION_NONE, ""},
  {ACTION_NONE, ""},
  {ACTION_NONE, ""},
  {ACTION_NONE, ""},
};

static get_action_code(char *name) {
  int i;

  for (i=0; i<MAX_ACTIONS; i++) {
    if (available_actions[i].name[0] && !strcmp(available_actions[i].name, name)) {
      return available_actions[i].action;
    }
  }

  return ACTION_NONE;
}

bool emulate_key(LSHandle* lshandle, LSMessage *message, void *ctx) {
  LSError lserror;
  LSErrorInit(&lserror);
  int code = -1;
  bool keydown = false;
  json_t *object;
 
  object = LSMessageGetPayloadJSON(message);
  json_get_int(object, "code", &code);
  json_get_bool(object, "keydown", &keydown);
  if (!is_valid_code(code)) {
    LSMessageRespond(message, 
        "{\"returnValue\": false, \"errorCode\": -1, \"errorText\": \"Invalid key code\"}", &lserror);
    return true;
  }

  send_key(code, keydown);
  LSMessageRespond(message, "{\"returnValue\": true}", &lserror);

  return true;
}

bool get_repeat_rate(LSHandle* lshandle, LSMessage *message, void *ctx) {
  LSError lserror;
  LSErrorInit(&lserror);

  memset(message_buf, 0, sizeof message_buf);
  sprintf(message_buf, "{\"returnValue\": true, \"delay\": %d, \"period\": %d}",
      current_delay, current_period);

  LSMessageRespond(message, message_buf, &lserror);

  return true;
}

bool set_repeat_rate(LSHandle* lshandle, LSMessage *message, void *ctx) {
  LSError lserror;
  LSErrorInit(&lserror);
  json_t *object;
  int delay = 0;
  int period = 0;
  bool use_default = false;

  object = LSMessageGetPayloadJSON(message);
  json_get_bool(object, "useDefault", &use_default);
  json_get_int(object, "delay", &delay);
  json_get_int(object, "period", &period);

  syslog(LOG_DEBUG, "useDefault %d, delay %d, period %d\n", use_default, delay, period);
  if (!use_default && (delay < 0 || delay > 3000) && (period < 0 || period > 3000)) {
    LSMessageRespond(message, 
        "{\"returnValue\": false, \"errorCode\": -1, \"errorText\": \"Bad paramater\"}", &lserror);
  }

  if (use_default)
    set_repeat(DEFAULT_DELAY, DEFAULT_PERIOD);
  else 
    set_repeat(delay, period);

  LSMessageRespond(message, "{\"returnValue\": true}", &lserror);

  return true;
}

bool change_action(LSHandle* lshandle, LSMessage *message, void *ctx) {
  LSError lserror;
  LSErrorInit(&lserror);
  json_t *object;
  char *param_trigger = NULL;
  char *param_action = NULL;
  int param_index = -1;
  ACTIONS action;
  int ret = 0;

  object = LSMessageGetPayloadJSON(message);
  json_get_string(object, "trigger", &param_trigger);
  json_get_int(object, "index", &param_index);
  json_get_string(object, "action", &param_action);

  if (!param_trigger || !param_action || param_index < 0)
    goto err;

  action = get_action_code(param_action);
  if (action == ACTION_NONE)
    goto err;

  if (!strcmp(param_trigger, "tap"))
    ret = change_tap_action(param_index, action);
  else if (!strcmp(param_trigger, "hold"))
    ret = change_hold_action(param_index, action);

  LSMessageRespond(message, "{\"returnValue\": true}", &lserror);

  return true;

err:
  LSMessageRespond(message, "{\"returnValue\": false, \"errorCode\": -1, \"errorText\": \"Invalid or missing trigger/index/action\"}", &lserror);

  return true;
}
bool remove_action(LSHandle* lshandle, LSMessage *message, void *ctx) {
  LSError lserror;
  LSErrorInit(&lserror);
  json_t *object;
  char *param_trigger = NULL;
  char *param_action = NULL;
  int param_index = -1;
  ACTIONS action;
  int ret = 0;

  object = LSMessageGetPayloadJSON(message);
  json_get_string(object, "trigger", &param_trigger);
  json_get_int(object, "index", &param_index);
  json_get_string(object, "action", &param_action);

  if (!param_trigger || (!param_action && param_index < 0))
    goto err;

  if (param_action) {
    syslog(LOG_DEBUG, "param action %s\n", param_action);
    action = get_action_code(param_action);
    if (action == ACTION_NONE)
      goto err;
  }

  if (!strcmp(param_trigger, "tap"))
    ret = remove_tap_action(action, param_index);
  else if (!strcmp(param_trigger, "hold"))
    ret = remove_hold_action(action, param_index);

  LSMessageRespond(message, "{\"returnValue\": true}", &lserror);

  return true;

err:
  LSMessageRespond(message, "{\"returnValue\": false, \"errorCode\": -1, \"errorText\": \"Invalid or missing trigger/index/action\"}", &lserror);

  return true;
}
bool install_action(LSHandle* lshandle, LSMessage *message, void *ctx) {
  LSError lserror;
  LSErrorInit(&lserror);
  json_t *object;
  char *param_trigger = NULL;
  char *param_action = NULL;
  ACTIONS action;
  int ret = 0;

  object = LSMessageGetPayloadJSON(message);
  json_get_string(object, "trigger", &param_trigger);
  json_get_string(object, "action", &param_action);

  if (!param_trigger || !param_action)
    goto err;

  action = get_action_code(param_action);
  if (action == ACTION_NONE)
    goto err;

  if (!strcmp(param_trigger, "tap"))
    ret = install_tap_action(action);
  else if (!strcmp(param_trigger, "hold"))
    ret = install_hold_action(action);

  LSMessageRespond(message, "{\"returnValue\": true}", &lserror);

  return true;

err:
  LSMessageRespond(message, "{\"returnValue\": false, \"errorCode\": -1, \"errorText\": \"Invalid or missing trigger/action\"}", &lserror);

  return true;
}

bool set_tap_timeout(LSHandle* lshandle, LSMessage *message, void *ctx) {
  LSError lserror;
  LSErrorInit(&lserror);
  json_t *object;
  int timeout = 0;

  object = LSMessageGetPayloadJSON(message);
  json_get_int(object, "timeout", &timeout);

  syslog(LOG_DEBUG, "set tap timeout: %dms", timeout);
  if (timeout < 0 || timeout > 2000) {
    LSMessageRespond(message, 
        "{\"returnValue\": false, \"errorCode\": -1, \"errorText\": \"Bad paramater\"}", &lserror);
  }

  set_tap_timeout_ms(timeout);

  LSMessageRespond(message, "{\"returnValue\": true}", &lserror);

  return true;
}

bool set_modifiers(LSHandle* lshandle, LSMessage *message, void *ctx) {
  LSError lserror;
  LSErrorInit(&lserror);
  json_t *object;
  char *hold = NULL;
  char *doubletap = NULL;

  object = LSMessageGetPayloadJSON(message);
  json_get_string(object, "hold", &hold);
  json_get_string(object, "doubletap", &doubletap);

  if (hold) {
    if (!strcmp(hold, "on"))
      hold_enabled = 1;
    else if (!strcmp(hold, "off"))
      hold_enabled = 0;
  }

  if (doubletap) {
    if (!strcmp(doubletap, "on"))
      double_enabled = 1;
    else if (!strcmp(doubletap, "off"))
      double_enabled = 0;
  }

  LSMessageRespond(message, "{\"returnValue\": true}", &lserror);
  
  return true;
}

//TODO: Pixi/Pre
bool set_prox_timeout(LSHandle* lshandle, LSMessage *message, void *ctx) {
  LSError lserror;
  LSErrorInit (&lserror);
  char *sysfs_path = "/sys/class/i2c-adapter/i2c-3/3-0038/prox_timeout";
  FILE *fd;
  json_t *object;
  int prox_timeout = -1;
 
  object = LSMessageGetPayloadJSON(message);
  json_get_int(object, "value", &prox_timeout);

  if (prox_timeout < 0) {
    LSMessageReply(lshandle, message, "{\"returnValue\": false, \"errorCode\": -1, \"errorText\": \"Must supply value\"}", &lserror);
    return true;
  }

  fd = fopen(sysfs_path, "w");
  if (!fd || fprintf(fd, "%d", prox_timeout) < 0) {
    LSMessageReply(lshandle, message, "{\"returnValue\": false, \"errorCode\": -1, \"errorText\": \"Error writing to prox_timeout\"}", &lserror);
    return true;
  }

  LSMessageReply(lshandle, message, "{\"returnValue\": true}", &lserror);
  return true;
}

bool get_status(LSHandle* lshandle, LSMessage *message, void *ctx) {
  LSError lserror;
  LSErrorInit(&lserror);
  int i;
  int count = 0;
  char *sysfs_path = "/sys/class/i2c-adapter/i2c-3/3-0038/prox_timeout";
  FILE *fd;
  int prox_timeout = -1;
  char buffer[4];
  struct key_modifier *hold = &keystate.hold;
  struct key_modifier *tap = &keystate.tap;
  char *actions = NULL;
  char *installed_hold = NULL;
  char *installed_tap = NULL;

  fd = fopen(sysfs_path, "w");
  if (fd)
    fclose(fd);

  count = 0;
  asprintf(&actions, "[");
  for (i=0; i<MAX_ACTIONS; i++) {
    if (available_actions[i].name[0]) {
      if (count++) 
        asprintf(&actions, "%s,", actions);
      asprintf(&actions, "%s\"%s\"", actions, available_actions[i].name);
    }
  }
  asprintf(&actions, "%s]", actions);

  count = 0;
  asprintf(&installed_hold, "[");
  for (i=0; i<hold->num_active; i++) {
    if (count++) 
      asprintf(&installed_hold, "%s,", installed_hold);
    asprintf(&installed_hold, "%s%d", hold->actions[i], installed_hold);
  }
  asprintf(&installed_hold, "%s]", installed_hold);

  count = 0;
  asprintf(&installed_tap, "[");
  for (i=0; i<tap->num_active; i++) {
    if (count++) 
      asprintf(&installed_tap, "%s,", installed_tap);
    asprintf(&installed_tap, "%s%d", tap->actions[i], installed_tap);
  }
  asprintf(&installed_tap, "%s]", installed_tap);

  memset(message_buf, 0, sizeof message_buf);
  sprintf(message_buf, "{\"returnValue\": true, \"u_fd\": %d, \"k_fd\": %d, max_actions: %d, actions: %s, installed_hold: %s, installed_tap: %s, \"prox_timeout\": %d}", u_fd, k_fd, MAX_ACTIONS, actions, installed_hold, installed_tap, prox_timeout);

  LSMessageRespond(message, message_buf, &lserror);

  return true;
}

LSMethod luna_methods[] = { 
  {"getStatus", get_status},
  {"emulateKey", emulate_key},
  {"getRepeatRate", get_repeat_rate},
  {"setRepeatRate", set_repeat_rate},
  {"setModifiers", set_modifiers},
  {"installAction", install_action},
  {"removeAction", remove_action},
  {"changeAction", change_action},
  {"setTapTimeout", set_tap_timeout},
  {"setProxTimeout", set_prox_timeout},
  {0,0}
};

bool register_methods(LSPalmService *serviceHandle, LSError lserror) {
	return LSPalmServiceRegisterCategory(serviceHandle, "/", luna_methods,
			NULL, NULL, NULL, &lserror);
}
