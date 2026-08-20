/*
 * Copyright (c) 2026 Tiresias Firmware Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include "tiresias_service.h"

#include "dsp_parameter_catalog.h"
#include "dsp_parameter_controller.h"

#include <errno.h>
#include <string.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>
#include <zephyr/sys/util.h>

#define TIRESIAS_CAPABILITIES                                                                                          \
  (TIRESIAS_CAPABILITY_GET_PARAMETER | TIRESIAS_CAPABILITY_SET_PARAMETER | TIRESIAS_CAPABILITY_PERSISTENCE             \
      | TIRESIAS_CAPABILITY_DSP_APPLY_DEFERRED)
#define TIRESIAS_CATALOG_SIZE (TIRESIAS_CATALOG_HEADER_SIZE + DSP_PARAMETER_CATALOG_COUNT * TIRESIAS_CATALOG_ENTRY_SIZE)
#define TIRESIAS_SERVICE_THREAD_STACK_SIZE 2048
#define TIRESIAS_SERVICE_THREAD_PRIORITY 4

LOG_MODULE_REGISTER(tiresias_service, CONFIG_LOG_DEFAULT_LEVEL);

BUILD_ASSERT(TIRESIAS_CATALOG_SIZE == 144U, "MVP catalog size changed unexpectedly");

struct tiresias_request {
  uint8_t opcode;
  uint32_t transaction_id;
  uint16_t parameter_id;
  int32_t value;
  uint32_t session_id;
};

static struct bt_uuid_128 service_uuid = BT_UUID_INIT_128(TIRESIAS_SERVICE_UUID);
static struct bt_uuid_128 protocol_info_uuid = BT_UUID_INIT_128(TIRESIAS_PROTOCOL_INFO_UUID);
static struct bt_uuid_128 catalog_uuid = BT_UUID_INIT_128(TIRESIAS_PARAMETER_CATALOG_UUID);
static struct bt_uuid_128 status_uuid = BT_UUID_INIT_128(TIRESIAS_STATUS_UUID);
static struct bt_uuid_128 request_uuid = BT_UUID_INIT_128(TIRESIAS_REQUEST_UUID);
static struct bt_uuid_128 response_uuid = BT_UUID_INIT_128(TIRESIAS_RESPONSE_UUID);

static K_MUTEX_DEFINE(service_mutex);
K_MSGQ_DEFINE(request_queue, sizeof(struct tiresias_request), 1, 4);

static uint8_t catalog_wire[TIRESIAS_CATALOG_SIZE];
static uint8_t response_wire[TIRESIAS_RESPONSE_SIZE];
static struct bt_gatt_indicate_params response_indication;
static struct bt_conn* control_conn;
enum request_phase {
  REQUEST_PHASE_IDLE,
  REQUEST_PHASE_QUEUED,
  REQUEST_PHASE_PROCESSING,
  REQUEST_PHASE_INDICATING,
};
static atomic_t request_phase;
static uint32_t catalog_crc;
static uint32_t boot_id;
static uint32_t active_session_id;
static atomic_t control_state;
static atomic_t last_transaction_id;
static atomic_t last_parameter_id;
static atomic_t last_result;
static atomic_t last_set_persisted;
static uint32_t status_revision;
static bool service_initialized;

extern const struct bt_gatt_attr attr_tiresias_gatt[];

static void encode_catalog(void)
{
  const struct dsp_parameter_descriptor* catalog = dsp_parameter_catalog();
  uint8_t* entry = &catalog_wire[TIRESIAS_CATALOG_HEADER_SIZE];

  for (size_t index = 0; index < dsp_parameter_catalog_count(); index++, entry += TIRESIAS_CATALOG_ENTRY_SIZE) {
    sys_put_le16(catalog[index].id, &entry[0]);
    entry[2] = catalog[index].flags;
    entry[3] = catalog[index].encoding;
    sys_put_le16(catalog[index].dsp_address, &entry[4]);
    entry[6] = catalog[index].word_count;
    entry[7] = catalog[index].unit;
    sys_put_le32((uint32_t)catalog[index].minimum, &entry[8]);
    sys_put_le32((uint32_t)catalog[index].maximum, &entry[12]);
    sys_put_le32((uint32_t)catalog[index].default_value, &entry[16]);
    sys_put_le32((uint32_t)catalog[index].step, &entry[20]);
    memcpy(&entry[24], catalog[index].name, sizeof(catalog[index].name));
  }

  catalog_crc
      = crc32_ieee(&catalog_wire[TIRESIAS_CATALOG_HEADER_SIZE], TIRESIAS_CATALOG_SIZE - TIRESIAS_CATALOG_HEADER_SIZE);
  catalog_wire[0] = 1U;
  catalog_wire[1] = TIRESIAS_CATALOG_ENTRY_SIZE;
  sys_put_le16(DSP_PARAMETER_CATALOG_COUNT, &catalog_wire[2]);
  sys_put_le16(TIRESIAS_CATALOG_SIZE, &catalog_wire[4]);
  sys_put_le16(0U, &catalog_wire[6]);
  sys_put_le32(DSP_PARAMETER_LAYOUT_ID, &catalog_wire[8]);
  sys_put_le32(catalog_crc, &catalog_wire[12]);
}

static void encode_protocol_info(uint8_t data[TIRESIAS_PROTOCOL_INFO_SIZE])
{
  data[0] = TIRESIAS_PROTOCOL_MAJOR;
  data[1] = TIRESIAS_PROTOCOL_MINOR;
  sys_put_le16(TIRESIAS_PROTOCOL_INFO_SIZE, &data[2]);
  sys_put_le32(TIRESIAS_CAPABILITIES, &data[4]);
  sys_put_le16(TIRESIAS_REQUEST_SIZE, &data[8]);
  sys_put_le16(TIRESIAS_RESPONSE_SIZE, &data[10]);
  sys_put_le16(TIRESIAS_CATALOG_ENTRY_SIZE, &data[12]);
  sys_put_le16(DSP_PARAMETER_CATALOG_COUNT, &data[14]);
  sys_put_le32(DSP_PARAMETER_LAYOUT_ID, &data[16]);
  sys_put_le32(catalog_crc, &data[20]);
  sys_put_le32(boot_id, &data[24]);
  sys_put_le32(dsp_parameter_controller_revision(), &data[28]);
}

static int encode_status(uint8_t data[TIRESIAS_STATUS_SIZE])
{
  uint8_t flags = TIRESIAS_STATUS_DSP_APPLY_DEFERRED;

  if (dsp_parameter_controller_loaded()) {
    flags |= TIRESIAS_STATUS_PARAMETERS_LOADED;
  }

  if (k_mutex_lock(&service_mutex, K_NO_WAIT) != 0) {
    return -EBUSY;
  }

  if (atomic_get(&last_set_persisted) != 0) {
    flags |= TIRESIAS_STATUS_LAST_SET_PERSISTED;
  }
  data[0] = (uint8_t)atomic_get(&control_state);
  data[1] = flags;
  data[2] = (uint8_t)atomic_get(&last_result);
  data[3] = 0U;
  sys_put_le32(status_revision, &data[4]);
  sys_put_le32((uint32_t)atomic_get(&last_transaction_id), &data[8]);
  sys_put_le16((uint16_t)atomic_get(&last_parameter_id), &data[12]);
  sys_put_le16(0U, &data[14]);
  k_mutex_unlock(&service_mutex);

  return 0;
}

static ssize_t read_protocol_info(
    struct bt_conn* conn, const struct bt_gatt_attr* attr, void* buf, uint16_t len, uint16_t offset)
{
  uint8_t data[TIRESIAS_PROTOCOL_INFO_SIZE];

  encode_protocol_info(data);
  return bt_gatt_attr_read(conn, attr, buf, len, offset, data, sizeof(data));
}

static ssize_t read_catalog(
    struct bt_conn* conn, const struct bt_gatt_attr* attr, void* buf, uint16_t len, uint16_t offset)
{
  return bt_gatt_attr_read(conn, attr, buf, len, offset, catalog_wire, sizeof(catalog_wire));
}

static ssize_t read_status(
    struct bt_conn* conn, const struct bt_gatt_attr* attr, void* buf, uint16_t len, uint16_t offset)
{
  uint8_t data[TIRESIAS_STATUS_SIZE];

  if (encode_status(data) != 0) {
    return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
  }
  return bt_gatt_attr_read(conn, attr, buf, len, offset, data, sizeof(data));
}

static ssize_t write_request(struct bt_conn* conn, const struct bt_gatt_attr* attr, const void* buf, uint16_t len,
    uint16_t offset, uint8_t flags)
{
  const uint8_t* data = buf;
  struct tiresias_request request;
  bool ready;

  ARG_UNUSED(attr);
  ARG_UNUSED(flags);

  if (offset != 0U || len != TIRESIAS_REQUEST_SIZE) {
    return BT_GATT_ERR(BT_ATT_ERR_INVALID_ATTRIBUTE_LEN);
  }

  if (data[1] != 0U || (data[0] != TIRESIAS_OPCODE_GET_PARAMETER && data[0] != TIRESIAS_OPCODE_SET_PARAMETER)) {
    return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
  }

  if (sys_get_le32(&data[2]) == 0U || (data[0] == TIRESIAS_OPCODE_GET_PARAMETER && sys_get_le32(&data[8]) != 0U)) {
    return BT_GATT_ERR(BT_ATT_ERR_VALUE_NOT_ALLOWED);
  }

  if (!bt_gatt_is_subscribed(conn, &attr_tiresias_gatt[11], BT_GATT_CCC_INDICATE)) {
    return BT_GATT_ERR(BT_ATT_ERR_CCC_IMPROPER_CONF);
  }

  if (k_mutex_lock(&service_mutex, K_NO_WAIT) != 0) {
    return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
  }
  ready = control_conn == conn && atomic_get(&control_state) == CONTROL_LINK_STATE_READY;
  request.session_id = active_session_id;
  k_mutex_unlock(&service_mutex);
  if (!ready) {
    return BT_GATT_ERR(BT_ATT_ERR_WRITE_NOT_PERMITTED);
  }

  if (!atomic_cas(&request_phase, REQUEST_PHASE_IDLE, REQUEST_PHASE_QUEUED)) {
    return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
  }

  request.opcode = data[0];
  request.transaction_id = sys_get_le32(&data[2]);
  request.parameter_id = sys_get_le16(&data[6]);
  request.value = (int32_t)sys_get_le32(&data[8]);

  if (k_msgq_put(&request_queue, &request, K_NO_WAIT) != 0) {
    atomic_set(&request_phase, REQUEST_PHASE_IDLE);
    return BT_GATT_ERR(BT_ATT_ERR_INSUFFICIENT_RESOURCES);
  }

  return len;
}

BT_GATT_SERVICE_DEFINE(tiresias_gatt, BT_GATT_PRIMARY_SERVICE(&service_uuid),
    BT_GATT_CHARACTERISTIC(
        &protocol_info_uuid.uuid, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, read_protocol_info, NULL, NULL),
    BT_GATT_CHARACTERISTIC(&catalog_uuid.uuid, BT_GATT_CHRC_READ, BT_GATT_PERM_READ, read_catalog, NULL, NULL),
    BT_GATT_CHARACTERISTIC(
        &status_uuid.uuid, BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY, BT_GATT_PERM_READ, read_status, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
    BT_GATT_CHARACTERISTIC(&request_uuid.uuid, BT_GATT_CHRC_WRITE, BT_GATT_PERM_WRITE, NULL, write_request, NULL),
    BT_GATT_CHARACTERISTIC(&response_uuid.uuid, BT_GATT_CHRC_INDICATE, BT_GATT_PERM_NONE, NULL, NULL, NULL),
    BT_GATT_CCC(NULL, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE));

static enum tiresias_result map_result(int error, bool setting)
{
  switch (error) {
  case 0:
    return TIRESIAS_RESULT_OK;
  case -ENOENT:
    return TIRESIAS_RESULT_NOT_FOUND;
  case -EACCES:
    return TIRESIAS_RESULT_READ_ONLY;
  case -ERANGE:
    return TIRESIAS_RESULT_OUT_OF_RANGE;
  case -EINVAL:
    return TIRESIAS_RESULT_BAD_REQUEST;
  default:
    return setting ? TIRESIAS_RESULT_PERSIST_FAILED : TIRESIAS_RESULT_INTERNAL;
  }
}

static struct bt_conn* get_control_connection(void)
{
  struct bt_conn* conn = NULL;

  k_mutex_lock(&service_mutex, K_FOREVER);
  if (control_conn != NULL) {
    conn = bt_conn_ref(control_conn);
  }
  k_mutex_unlock(&service_mutex);

  return conn;
}

static bool session_is_active(uint32_t session_id)
{
  bool active;

  k_mutex_lock(&service_mutex, K_FOREVER);
  active = control_conn != NULL && active_session_id == session_id;
  k_mutex_unlock(&service_mutex);

  return active;
}

static void indication_complete(struct bt_conn* conn, struct bt_gatt_indicate_params* params, uint8_t att_error)
{
  ARG_UNUSED(conn);
  ARG_UNUSED(params);

  if (att_error != 0U) {
    LOG_WRN("Response indication failed: 0x%02x", att_error);
  }
}

static void indication_destroy(struct bt_gatt_indicate_params* params)
{
  ARG_UNUSED(params);

  atomic_set(&request_phase, REQUEST_PHASE_IDLE);
}

static void send_status_notification(struct bt_conn* conn)
{
  uint8_t status[TIRESIAS_STATUS_SIZE];
  int ret;

  if (!bt_gatt_is_subscribed(conn, &attr_tiresias_gatt[6], BT_GATT_CCC_NOTIFY)) {
    return;
  }

  if (encode_status(status) != 0) {
    return;
  }
  ret = bt_gatt_notify(conn, &attr_tiresias_gatt[6], status, sizeof(status));
  if (ret != 0) {
    LOG_DBG("Status notification was not delivered: %d", ret);
  }
}

static void process_request(const struct tiresias_request* request)
{
  struct bt_conn* conn;
  int32_t value = request->value;
  uint32_t revision = dsp_parameter_controller_revision();
  bool setting = request->opcode == TIRESIAS_OPCODE_SET_PARAMETER;
  enum tiresias_result result;
  int ret;

  if (!session_is_active(request->session_id)) {
    atomic_set(&request_phase, REQUEST_PHASE_IDLE);
    return;
  }

  if (setting) {
    ret = dsp_parameter_controller_set(request->parameter_id, value, &revision);
  } else {
    ret = dsp_parameter_controller_get(request->parameter_id, &value, &revision);
  }
  result = map_result(ret, setting);

  k_mutex_lock(&service_mutex, K_FOREVER);
  atomic_set(&last_transaction_id, (atomic_val_t)request->transaction_id);
  atomic_set(&last_parameter_id, request->parameter_id);
  atomic_set(&last_result, result);
  if (setting) {
    atomic_set(&last_set_persisted, result == TIRESIAS_RESULT_OK);
  }
  status_revision = revision;
  k_mutex_unlock(&service_mutex);

  response_wire[0] = request->opcode;
  response_wire[1] = result;
  sys_put_le32(request->transaction_id, &response_wire[2]);
  sys_put_le16(request->parameter_id, &response_wire[6]);
  sys_put_le32((uint32_t)value, &response_wire[8]);
  sys_put_le32(revision, &response_wire[12]);

  conn = get_control_connection();
  if (conn == NULL || !session_is_active(request->session_id)) {
    if (conn != NULL) {
      bt_conn_unref(conn);
    }
    atomic_set(&request_phase, REQUEST_PHASE_IDLE);
    return;
  }

  response_indication.attr = &attr_tiresias_gatt[11];
  response_indication.func = indication_complete;
  response_indication.destroy = indication_destroy;
  response_indication.data = response_wire;
  response_indication.len = sizeof(response_wire);
  atomic_set(&request_phase, REQUEST_PHASE_INDICATING);
  ret = bt_gatt_indicate(conn, &response_indication);
  if (ret != 0) {
    LOG_WRN("Failed to queue response indication: %d", ret);
    atomic_set(&request_phase, REQUEST_PHASE_IDLE);
  }

  send_status_notification(conn);
  bt_conn_unref(conn);
}

static void tiresias_service_thread(void* arg1, void* arg2, void* arg3)
{
  struct tiresias_request request;

  ARG_UNUSED(arg1);
  ARG_UNUSED(arg2);
  ARG_UNUSED(arg3);

  while (1) {
    if (k_msgq_get(&request_queue, &request, K_FOREVER) == 0) {
      atomic_set(&request_phase, REQUEST_PHASE_PROCESSING);
      process_request(&request);
    }
  }
}

K_THREAD_DEFINE(tiresias_service_thread_id, TIRESIAS_SERVICE_THREAD_STACK_SIZE, tiresias_service_thread, NULL, NULL,
    NULL, TIRESIAS_SERVICE_THREAD_PRIORITY, 0, 0);

int tiresias_service_init(void)
{
  int ret;

  if (service_initialized) {
    return 0;
  }

  ret = dsp_parameter_controller_init();
  if (ret != 0) {
    return ret;
  }

  encode_catalog();
  if (catalog_crc != TIRESIAS_CATALOG_CRC32) {
    LOG_ERR("Catalog golden CRC mismatch: 0x%08x", catalog_crc);
    return -EINVAL;
  }
  boot_id = k_cycle_get_32();
  service_initialized = true;
  return 0;
}

void tiresias_service_on_connected(struct bt_conn* conn)
{
  k_mutex_lock(&service_mutex, K_FOREVER);
  if (control_conn != NULL) {
    bt_conn_unref(control_conn);
  }
  control_conn = bt_conn_ref(conn);
  active_session_id++;
  k_mutex_unlock(&service_mutex);
}

void tiresias_service_on_disconnected(struct bt_conn* conn)
{
  struct tiresias_request discarded_request;

  k_mutex_lock(&service_mutex, K_FOREVER);
  if (control_conn == conn) {
    bt_conn_unref(control_conn);
    control_conn = NULL;
    active_session_id++;
  }
  k_mutex_unlock(&service_mutex);

  if (k_msgq_get(&request_queue, &discarded_request, K_NO_WAIT) == 0) {
    atomic_cas(&request_phase, REQUEST_PHASE_QUEUED, REQUEST_PHASE_IDLE);
  }
}

void tiresias_service_set_control_state(control_link_state state)
{
  struct bt_conn* conn;

  k_mutex_lock(&service_mutex, K_FOREVER);
  atomic_set(&control_state, state);
  status_revision = dsp_parameter_controller_revision();
  k_mutex_unlock(&service_mutex);

  conn = get_control_connection();
  if (conn != NULL) {
    send_status_notification(conn);
    bt_conn_unref(conn);
  }
}
