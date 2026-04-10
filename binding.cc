/**
 * @utexo/rgb-lib-bare - Bare native addon wrapping rgb-lib C FFI
 *
 * Wraps all 50 C FFI functions from rgb-lib v0.3.0-beta.15 rgblib.h.
 * Uses <bare.h> + <js.h> API (same pattern as sodium-native).
 */

#include <bare.h>
#include <js.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include "rgblib.h"
}

// ============================================================================
// Helpers
// ============================================================================

/**
 * Extract a UTF-8 C string from a JS value.
 * Returns NULL for null/undefined (used for optional parameters).
 * Caller must free() the returned pointer.
 */
static char *js_to_cstring(js_env_t *env, js_value_t *val) {
  js_value_type_t type;
  js_typeof(env, val, &type);
  if (type == js_null || type == js_undefined) return NULL;

  size_t len;
  js_get_value_string_utf8(env, val, NULL, 0, &len);
  utf8_t *buf = (utf8_t *)malloc(len + 1);
  js_get_value_string_utf8(env, val, buf, len + 1, NULL);
  return (char *)buf;
}

/**
 * Create a JS string from a C char pointer.
 * Returns JS null for NULL input.
 */
static js_value_t *cstring_to_js(js_env_t *env, const char *str) {
  if (!str) {
    js_value_t *null_val;
    js_get_null(env, &null_val);
    return null_val;
  }
  js_value_t *result;
  js_create_string_utf8(env, (const utf8_t *)str, strlen(str), &result);
  return result;
}

/**
 * Get a bool from JS value at given index in callback args.
 */
static bool js_to_bool(js_env_t *env, js_value_t *val) {
  bool result;
  js_get_value_bool(env, val, &result);
  return result;
}

/**
 * Handle CResultString: return JS string on Ok, throw on Err.
 */
static js_value_t *handle_result_string(js_env_t *env, struct CResultString res) {
  if (res.result == Ok) {
    js_value_t *val = cstring_to_js(env, res.inner);
    if (res.inner) free(res.inner);
    return val;
  } else {
    const char *msg = res.inner ? res.inner : "Unknown rgb-lib error";
    js_throw_error(env, NULL, msg);
    if (res.inner) free(res.inner);
    js_value_t *undef;
    js_get_undefined(env, &undef);
    return undef;
  }
}

// ============================================================================
// Opaque handle management (Wallet, Online, Invoice)
// ============================================================================

typedef void (*opaque_free_fn)(struct COpaqueStruct);

struct OpaqueRef {
  struct COpaqueStruct opaque;
  opaque_free_fn free_fn;
  bool freed;
};

static void opaque_ref_destructor(js_env_t *env, void *data, void *hint) {
  OpaqueRef *ref = (OpaqueRef *)data;
  if (!ref->freed && ref->free_fn) {
    ref->free_fn(ref->opaque);
  }
  free(ref);
}

/**
 * Wrap a COpaqueStruct as a JS external with GC destructor.
 */
static js_value_t *wrap_opaque(js_env_t *env, struct COpaqueStruct opaque,
                               opaque_free_fn free_fn) {
  OpaqueRef *ref = (OpaqueRef *)malloc(sizeof(OpaqueRef));
  ref->opaque = opaque;
  ref->free_fn = free_fn;
  ref->freed = false;

  js_value_t *external;
  js_create_external(env, ref, opaque_ref_destructor, NULL, &external);
  return external;
}

/**
 * Extract COpaqueStruct pointer from a JS external.
 */
static const struct COpaqueStruct *unwrap_opaque(js_env_t *env, js_value_t *val) {
  void *data;
  js_get_value_external(env, val, &data);
  OpaqueRef *ref = (OpaqueRef *)data;
  return &ref->opaque;
}

/**
 * Handle CResult: return wrapped external on Ok, throw on Err.
 */
static js_value_t *handle_result_opaque(js_env_t *env, struct CResult res,
                                        opaque_free_fn free_fn) {
  if (res.result == Ok) {
    return wrap_opaque(env, res.inner, free_fn);
  } else {
    // On error, inner.ptr contains error message string
    const char *msg = res.inner.ptr ? (const char *)res.inner.ptr : "Unknown rgb-lib error";
    js_throw_error(env, NULL, msg);
    js_value_t *undef;
    js_get_undefined(env, &undef);
    return undef;
  }
}

// ============================================================================
// Helper: extract args from callback info
// ============================================================================

#define MAX_ARGS 10

static int get_args(js_env_t *env, js_callback_info_t *info,
                    js_value_t **args, size_t expected) {
  size_t argc = expected;
  js_get_callback_info(env, info, &argc, args, NULL, NULL);
  return (int)argc;
}

// ============================================================================
// Group 1: Stateless string functions
// ============================================================================

static js_value_t *fn_generate_keys(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[1];
  get_args(env, info, args, 1);
  char *network = js_to_cstring(env, args[0]);
  struct CResultString res = rgblib_generate_keys(network);
  free(network);
  return handle_result_string(env, res);
}

static js_value_t *fn_restore_keys(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[2];
  get_args(env, info, args, 2);
  char *network = js_to_cstring(env, args[0]);
  char *mnemonic = js_to_cstring(env, args[1]);
  struct CResultString res = rgblib_restore_keys(network, mnemonic);
  free(network);
  free(mnemonic);
  return handle_result_string(env, res);
}

// ============================================================================
// Group 2: Constructors returning opaque handles
// ============================================================================

static js_value_t *fn_new_wallet(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[1];
  get_args(env, info, args, 1);
  char *wallet_data = js_to_cstring(env, args[0]);
  struct CResult res = rgblib_new_wallet(wallet_data);
  free(wallet_data);
  return handle_result_opaque(env, res, free_wallet);
}

static js_value_t *fn_go_online(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[3];
  get_args(env, info, args, 3);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  bool skip_check = js_to_bool(env, args[1]);
  char *url = js_to_cstring(env, args[2]);
  struct CResult res = rgblib_go_online(wallet, skip_check, url);
  free(url);
  return handle_result_opaque(env, res, free_online);
}

static js_value_t *fn_invoice_new(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[1];
  get_args(env, info, args, 1);
  char *invoice_str = js_to_cstring(env, args[0]);
  struct CResult res = rgblib_invoice_new(invoice_str);
  free(invoice_str);
  return handle_result_opaque(env, res, free_invoice);
}

static js_value_t *fn_restore_backup(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[3];
  get_args(env, info, args, 3);
  char *path = js_to_cstring(env, args[0]);
  char *password = js_to_cstring(env, args[1]);
  char *target_dir = js_to_cstring(env, args[2]);
  struct CResult res = rgblib_restore_backup(path, password, target_dir);
  free(path);
  free(password);
  free(target_dir);
  // restore_backup returns CResult but no opaque handle on success
  if (res.result == Ok) {
    js_value_t *undef;
    js_get_undefined(env, &undef);
    return undef;
  } else {
    const char *msg = res.inner.ptr ? (const char *)res.inner.ptr : "Restore backup failed";
    js_throw_error(env, NULL, msg);
    js_value_t *undef;
    js_get_undefined(env, &undef);
    return undef;
  }
}

// ============================================================================
// Group 3: Wallet operations -> CResultString
// ============================================================================

static js_value_t *fn_get_address(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[1];
  get_args(env, info, args, 1);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  return handle_result_string(env, rgblib_get_address(wallet));
}

static js_value_t *fn_get_asset_balance(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[2];
  get_args(env, info, args, 2);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  char *asset_id = js_to_cstring(env, args[1]);
  struct CResultString res = rgblib_get_asset_balance(wallet, asset_id);
  free(asset_id);
  return handle_result_string(env, res);
}

static js_value_t *fn_get_btc_balance(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[3];
  get_args(env, info, args, 3);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  // online can be null
  js_value_type_t type;
  js_typeof(env, args[1], &type);
  const struct COpaqueStruct *online = (type == js_null || type == js_undefined)
    ? NULL : unwrap_opaque(env, args[1]);
  bool skip_sync = js_to_bool(env, args[2]);
  return handle_result_string(env, rgblib_get_btc_balance(wallet, online, skip_sync));
}

static js_value_t *fn_get_fee_estimation(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[3];
  get_args(env, info, args, 3);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *online = unwrap_opaque(env, args[1]);
  char *blocks = js_to_cstring(env, args[2]);
  struct CResultString res = rgblib_get_fee_estimation(wallet, online, blocks);
  free(blocks);
  return handle_result_string(env, res);
}

static js_value_t *fn_list_assets(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[2];
  get_args(env, info, args, 2);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  char *filter = js_to_cstring(env, args[1]);
  struct CResultString res = rgblib_list_assets(wallet, filter);
  free(filter);
  return handle_result_string(env, res);
}

static js_value_t *fn_list_transfers(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[2];
  get_args(env, info, args, 2);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  char *asset_id = js_to_cstring(env, args[1]);
  struct CResultString res = rgblib_list_transfers(wallet, asset_id);
  free(asset_id);
  return handle_result_string(env, res);
}

static js_value_t *fn_list_transactions(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[3];
  get_args(env, info, args, 3);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *online = unwrap_opaque(env, args[1]);
  bool skip_sync = js_to_bool(env, args[2]);
  return handle_result_string(env, rgblib_list_transactions(wallet, online, skip_sync));
}

static js_value_t *fn_list_unspents(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[4];
  get_args(env, info, args, 4);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *online = unwrap_opaque(env, args[1]);
  bool settled_only = js_to_bool(env, args[2]);
  bool skip_sync = js_to_bool(env, args[3]);
  return handle_result_string(env, rgblib_list_unspents(wallet, online, settled_only, skip_sync));
}

static js_value_t *fn_sync(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[2];
  get_args(env, info, args, 2);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *online = unwrap_opaque(env, args[1]);
  return handle_result_string(env, rgblib_sync(wallet, online));
}

static js_value_t *fn_refresh(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[5];
  get_args(env, info, args, 5);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *online = unwrap_opaque(env, args[1]);
  char *asset_id = js_to_cstring(env, args[2]);
  char *filter = js_to_cstring(env, args[3]);
  bool skip_sync = js_to_bool(env, args[4]);
  struct CResultString res = rgblib_refresh(wallet, online, asset_id, filter, skip_sync);
  free(asset_id);
  free(filter);
  return handle_result_string(env, res);
}

// ============================================================================
// Group 4: UTXO management
// ============================================================================

static js_value_t *fn_create_utxos(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[7];
  get_args(env, info, args, 7);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *online = unwrap_opaque(env, args[1]);
  bool up_to = js_to_bool(env, args[2]);
  char *num = js_to_cstring(env, args[3]);
  char *size = js_to_cstring(env, args[4]);
  char *fee_rate = js_to_cstring(env, args[5]);
  bool skip_sync = js_to_bool(env, args[6]);
  struct CResultString res = rgblib_create_utxos(wallet, online, up_to, num, size, fee_rate, skip_sync);
  free(num); free(size); free(fee_rate);
  return handle_result_string(env, res);
}

static js_value_t *fn_create_utxos_begin(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[7];
  get_args(env, info, args, 7);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *online = unwrap_opaque(env, args[1]);
  bool up_to = js_to_bool(env, args[2]);
  char *num = js_to_cstring(env, args[3]);
  char *size = js_to_cstring(env, args[4]);
  char *fee_rate = js_to_cstring(env, args[5]);
  bool skip_sync = js_to_bool(env, args[6]);
  struct CResultString res = rgblib_create_utxos_begin(wallet, online, up_to, num, size, fee_rate, skip_sync);
  free(num); free(size); free(fee_rate);
  return handle_result_string(env, res);
}

static js_value_t *fn_create_utxos_end(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[4];
  get_args(env, info, args, 4);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *online = unwrap_opaque(env, args[1]);
  char *signed_psbt = js_to_cstring(env, args[2]);
  bool skip_sync = js_to_bool(env, args[3]);
  struct CResultString res = rgblib_create_utxos_end(wallet, online, signed_psbt, skip_sync);
  free(signed_psbt);
  return handle_result_string(env, res);
}

// ============================================================================
// Group 5: Send / Transfer
// ============================================================================

static js_value_t *fn_send(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[7];
  get_args(env, info, args, 7);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *online = unwrap_opaque(env, args[1]);
  char *recipient_map = js_to_cstring(env, args[2]);
  bool donation = js_to_bool(env, args[3]);
  char *fee_rate = js_to_cstring(env, args[4]);
  char *min_conf = js_to_cstring(env, args[5]);
  bool skip_sync = js_to_bool(env, args[6]);
  struct CResultString res = rgblib_send(wallet, online, recipient_map, donation, fee_rate, min_conf, skip_sync);
  free(recipient_map); free(fee_rate); free(min_conf);
  return handle_result_string(env, res);
}

static js_value_t *fn_send_begin(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[6];
  get_args(env, info, args, 6);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *online = unwrap_opaque(env, args[1]);
  char *recipient_map = js_to_cstring(env, args[2]);
  bool donation = js_to_bool(env, args[3]);
  char *fee_rate = js_to_cstring(env, args[4]);
  char *min_conf = js_to_cstring(env, args[5]);
  struct CResultString res = rgblib_send_begin(wallet, online, recipient_map, donation, fee_rate, min_conf);
  free(recipient_map); free(fee_rate); free(min_conf);
  return handle_result_string(env, res);
}

static js_value_t *fn_send_end(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[4];
  get_args(env, info, args, 4);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *online = unwrap_opaque(env, args[1]);
  char *signed_psbt = js_to_cstring(env, args[2]);
  bool skip_sync = js_to_bool(env, args[3]);
  struct CResultString res = rgblib_send_end(wallet, online, signed_psbt, skip_sync);
  free(signed_psbt);
  return handle_result_string(env, res);
}

static js_value_t *fn_send_btc(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[6];
  get_args(env, info, args, 6);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *online = unwrap_opaque(env, args[1]);
  char *address = js_to_cstring(env, args[2]);
  char *amount = js_to_cstring(env, args[3]);
  char *fee_rate = js_to_cstring(env, args[4]);
  bool skip_sync = js_to_bool(env, args[5]);
  struct CResultString res = rgblib_send_btc(wallet, online, address, amount, fee_rate, skip_sync);
  free(address); free(amount); free(fee_rate);
  return handle_result_string(env, res);
}

static js_value_t *fn_blind_receive(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[6];
  get_args(env, info, args, 6);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  char *asset_id = js_to_cstring(env, args[1]);
  char *assignment = js_to_cstring(env, args[2]);
  char *duration = js_to_cstring(env, args[3]);
  char *endpoints = js_to_cstring(env, args[4]);
  char *min_conf = js_to_cstring(env, args[5]);
  struct CResultString res = rgblib_blind_receive(wallet, asset_id, assignment, duration, endpoints, min_conf);
  free(asset_id); free(assignment); free(duration); free(endpoints); free(min_conf);
  return handle_result_string(env, res);
}

static js_value_t *fn_witness_receive(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[6];
  get_args(env, info, args, 6);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  char *asset_id = js_to_cstring(env, args[1]);
  char *assignment = js_to_cstring(env, args[2]);
  char *duration = js_to_cstring(env, args[3]);
  char *endpoints = js_to_cstring(env, args[4]);
  char *min_conf = js_to_cstring(env, args[5]);
  struct CResultString res = rgblib_witness_receive(wallet, asset_id, assignment, duration, endpoints, min_conf);
  free(asset_id); free(assignment); free(duration); free(endpoints); free(min_conf);
  return handle_result_string(env, res);
}

// ============================================================================
// Group 6: Asset issuance
// ============================================================================

static js_value_t *fn_issue_asset_nia(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[5];
  get_args(env, info, args, 5);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  char *ticker = js_to_cstring(env, args[1]);
  char *name = js_to_cstring(env, args[2]);
  char *precision = js_to_cstring(env, args[3]);
  char *amounts = js_to_cstring(env, args[4]);
  struct CResultString res = rgblib_issue_asset_nia(wallet, ticker, name, precision, amounts);
  free(ticker); free(name); free(precision); free(amounts);
  return handle_result_string(env, res);
}

static js_value_t *fn_issue_asset_cfa(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[6];
  get_args(env, info, args, 6);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  char *name = js_to_cstring(env, args[1]);
  char *details = js_to_cstring(env, args[2]);
  char *precision = js_to_cstring(env, args[3]);
  char *amounts = js_to_cstring(env, args[4]);
  char *file_path = js_to_cstring(env, args[5]);
  struct CResultString res = rgblib_issue_asset_cfa(wallet, name, details, precision, amounts, file_path);
  free(name); free(details); free(precision); free(amounts); free(file_path);
  return handle_result_string(env, res);
}

static js_value_t *fn_issue_asset_ifa(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[8];
  get_args(env, info, args, 8);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  char *ticker = js_to_cstring(env, args[1]);
  char *name = js_to_cstring(env, args[2]);
  char *precision = js_to_cstring(env, args[3]);
  char *amounts = js_to_cstring(env, args[4]);
  char *inflation = js_to_cstring(env, args[5]);
  char *replace_rights = js_to_cstring(env, args[6]);
  char *reject_url = js_to_cstring(env, args[7]);
  struct CResultString res = rgblib_issue_asset_ifa(wallet, ticker, name, precision, amounts,
                                                    inflation, replace_rights, reject_url);
  free(ticker); free(name); free(precision); free(amounts);
  free(inflation); free(replace_rights); free(reject_url);
  return handle_result_string(env, res);
}

static js_value_t *fn_issue_asset_uda(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[7];
  get_args(env, info, args, 7);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  char *ticker = js_to_cstring(env, args[1]);
  char *name = js_to_cstring(env, args[2]);
  char *details = js_to_cstring(env, args[3]);
  char *precision = js_to_cstring(env, args[4]);
  char *media_path = js_to_cstring(env, args[5]);
  char *attachments = js_to_cstring(env, args[6]);
  struct CResultString res = rgblib_issue_asset_uda(wallet, ticker, name, details, precision,
                                                    media_path, attachments);
  free(ticker); free(name); free(details); free(precision);
  free(media_path); free(attachments);
  return handle_result_string(env, res);
}

static js_value_t *fn_inflate(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[6];
  get_args(env, info, args, 6);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *online = unwrap_opaque(env, args[1]);
  char *asset_id = js_to_cstring(env, args[2]);
  char *amounts = js_to_cstring(env, args[3]);
  char *fee_rate = js_to_cstring(env, args[4]);
  char *min_conf = js_to_cstring(env, args[5]);
  struct CResultString res = rgblib_inflate(wallet, online, asset_id, amounts, fee_rate, min_conf);
  free(asset_id); free(amounts); free(fee_rate); free(min_conf);
  return handle_result_string(env, res);
}

// ============================================================================
// Group 7: PSBT operations
// ============================================================================

static js_value_t *fn_sign_psbt(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[2];
  get_args(env, info, args, 2);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  char *psbt = js_to_cstring(env, args[1]);
  struct CResultString res = rgblib_sign_psbt(wallet, psbt);
  free(psbt);
  return handle_result_string(env, res);
}

static js_value_t *fn_finalize_psbt(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[2];
  get_args(env, info, args, 2);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  char *psbt = js_to_cstring(env, args[1]);
  struct CResultString res = rgblib_finalize_psbt(wallet, psbt);
  free(psbt);
  return handle_result_string(env, res);
}

// ============================================================================
// Group 8: Backup
// ============================================================================

static js_value_t *fn_backup(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[3];
  get_args(env, info, args, 3);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  char *path = js_to_cstring(env, args[1]);
  char *password = js_to_cstring(env, args[2]);
  struct CResult res = rgblib_backup(wallet, path, password);
  free(path); free(password);
  if (res.result == Ok) {
    js_value_t *undef;
    js_get_undefined(env, &undef);
    return undef;
  } else {
    const char *msg = res.inner.ptr ? (const char *)res.inner.ptr : "Backup failed";
    js_throw_error(env, NULL, msg);
    js_value_t *undef;
    js_get_undefined(env, &undef);
    return undef;
  }
}

static js_value_t *fn_backup_info(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[1];
  get_args(env, info, args, 1);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  return handle_result_string(env, rgblib_backup_info(wallet));
}

// ============================================================================
// Group 9: Invoice operations
// ============================================================================

static js_value_t *fn_invoice_data(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[1];
  get_args(env, info, args, 1);
  const struct COpaqueStruct *invoice = unwrap_opaque(env, args[0]);
  return handle_result_string(env, rgblib_invoice_data(invoice));
}

static js_value_t *fn_invoice_string(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[1];
  get_args(env, info, args, 1);
  const struct COpaqueStruct *invoice = unwrap_opaque(env, args[0]);
  return handle_result_string(env, rgblib_invoice_string(invoice));
}

// ============================================================================
// Group 9b: Additional wallet operations (v0.3.0-beta.15)
// ============================================================================

static js_value_t *fn_get_asset_metadata(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[2];
  get_args(env, info, args, 2);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  char *asset_id = js_to_cstring(env, args[1]);
  struct CResultString res = rgblib_get_asset_metadata(wallet, asset_id);
  free(asset_id);
  return handle_result_string(env, res);
}

static js_value_t *fn_delete_transfers(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[3];
  get_args(env, info, args, 3);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  char *batch_idx_opt = js_to_cstring(env, args[1]);
  bool no_asset_only = js_to_bool(env, args[2]);
  struct CResultString res = rgblib_delete_transfers(wallet, batch_idx_opt, no_asset_only);
  free(batch_idx_opt);
  return handle_result_string(env, res);
}

static js_value_t *fn_fail_transfers(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[5];
  get_args(env, info, args, 5);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *online = unwrap_opaque(env, args[1]);
  char *batch_idx_opt = js_to_cstring(env, args[2]);
  bool no_asset_only = js_to_bool(env, args[3]);
  bool skip_sync = js_to_bool(env, args[4]);
  struct CResultString res = rgblib_fail_transfers(wallet, online, batch_idx_opt, no_asset_only, skip_sync);
  free(batch_idx_opt);
  return handle_result_string(env, res);
}

static js_value_t *fn_send_btc_begin(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[6];
  get_args(env, info, args, 6);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *online = unwrap_opaque(env, args[1]);
  char *address = js_to_cstring(env, args[2]);
  char *amount = js_to_cstring(env, args[3]);
  char *fee_rate = js_to_cstring(env, args[4]);
  bool skip_sync = js_to_bool(env, args[5]);
  struct CResultString res = rgblib_send_btc_begin(wallet, online, address, amount, fee_rate, skip_sync);
  free(address); free(amount); free(fee_rate);
  return handle_result_string(env, res);
}

static js_value_t *fn_send_btc_end(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[4];
  get_args(env, info, args, 4);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *online = unwrap_opaque(env, args[1]);
  char *signed_psbt = js_to_cstring(env, args[2]);
  bool skip_sync = js_to_bool(env, args[3]);
  struct CResultString res = rgblib_send_btc_end(wallet, online, signed_psbt, skip_sync);
  free(signed_psbt);
  return handle_result_string(env, res);
}

static js_value_t *fn_validate_consignment(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[3];
  get_args(env, info, args, 3);
  char *file_path = js_to_cstring(env, args[0]);
  char *indexer_url = js_to_cstring(env, args[1]);
  char *bitcoin_network = js_to_cstring(env, args[2]);
  struct CResultString res = rgblib_validate_consignment(file_path, indexer_url, bitcoin_network);
  free(file_path); free(indexer_url); free(bitcoin_network);
  return handle_result_string(env, res);
}

// ============================================================================
// Group 9c: VSS backup operations (v0.3.0-beta.15)
// ============================================================================

static js_value_t *fn_new_vss_backup_client(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[1];
  get_args(env, info, args, 1);
  char *config_json = js_to_cstring(env, args[0]);
  struct CResult res = rgblib_new_vss_backup_client(config_json);
  free(config_json);
  return handle_result_opaque(env, res, free_vss_backup_client);
}

static js_value_t *fn_free_vss_backup_client(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[1];
  get_args(env, info, args, 1);
  void *data;
  js_get_value_external(env, args[0], &data);
  OpaqueRef *ref = (OpaqueRef *)data;
  if (!ref->freed && ref->free_fn) {
    ref->free_fn(ref->opaque);
    ref->freed = true;
  }
  js_value_t *undef;
  js_get_undefined(env, &undef);
  return undef;
}

static js_value_t *fn_configure_vss_backup(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[2];
  get_args(env, info, args, 2);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  char *config_json = js_to_cstring(env, args[1]);
  struct CResultString res = rgblib_configure_vss_backup(wallet, config_json);
  free(config_json);
  return handle_result_string(env, res);
}

static js_value_t *fn_disable_vss_auto_backup(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[1];
  get_args(env, info, args, 1);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  return handle_result_string(env, rgblib_disable_vss_auto_backup(wallet));
}

static js_value_t *fn_vss_backup(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[2];
  get_args(env, info, args, 2);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *client = unwrap_opaque(env, args[1]);
  return handle_result_string(env, rgblib_vss_backup(wallet, client));
}

static js_value_t *fn_vss_backup_info(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[2];
  get_args(env, info, args, 2);
  const struct COpaqueStruct *wallet = unwrap_opaque(env, args[0]);
  const struct COpaqueStruct *client = unwrap_opaque(env, args[1]);
  return handle_result_string(env, rgblib_vss_backup_info(wallet, client));
}

static js_value_t *fn_vss_backup_client_encryption_enabled(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[1];
  get_args(env, info, args, 1);
  const struct COpaqueStruct *client = unwrap_opaque(env, args[0]);
  return handle_result_string(env, rgblib_vss_backup_client_encryption_enabled(client));
}

static js_value_t *fn_vss_delete_backup(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[1];
  get_args(env, info, args, 1);
  const struct COpaqueStruct *client = unwrap_opaque(env, args[0]);
  return handle_result_string(env, rgblib_vss_delete_backup(client));
}

static js_value_t *fn_restore_from_vss(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[2];
  get_args(env, info, args, 2);
  char *config_json = js_to_cstring(env, args[0]);
  char *target_dir = js_to_cstring(env, args[1]);
  struct CResultString res = rgblib_restore_from_vss(config_json, target_dir);
  free(config_json); free(target_dir);
  return handle_result_string(env, res);
}

// ============================================================================
// Group 10: Free functions (explicit drop)
// ============================================================================

static js_value_t *fn_free_wallet(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[1];
  get_args(env, info, args, 1);
  void *data;
  js_get_value_external(env, args[0], &data);
  OpaqueRef *ref = (OpaqueRef *)data;
  if (!ref->freed && ref->free_fn) {
    ref->free_fn(ref->opaque);
    ref->freed = true;
  }
  js_value_t *undef;
  js_get_undefined(env, &undef);
  return undef;
}

static js_value_t *fn_free_online(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[1];
  get_args(env, info, args, 1);
  void *data;
  js_get_value_external(env, args[0], &data);
  OpaqueRef *ref = (OpaqueRef *)data;
  if (!ref->freed && ref->free_fn) {
    ref->free_fn(ref->opaque);
    ref->freed = true;
  }
  js_value_t *undef;
  js_get_undefined(env, &undef);
  return undef;
}

static js_value_t *fn_free_invoice(js_env_t *env, js_callback_info_t *info) {
  js_value_t *args[1];
  get_args(env, info, args, 1);
  void *data;
  js_get_value_external(env, args[0], &data);
  OpaqueRef *ref = (OpaqueRef *)data;
  if (!ref->freed && ref->free_fn) {
    ref->free_fn(ref->opaque);
    ref->freed = true;
  }
  js_value_t *undef;
  js_get_undefined(env, &undef);
  return undef;
}

// ============================================================================
// Module exports
// ============================================================================

static void set_fn(js_env_t *env, js_value_t *exports, const char *name,
                   js_value_t *(*fn)(js_env_t *, js_callback_info_t *)) {
  js_value_t *js_fn;
  js_create_function(env, name, strlen(name), fn, NULL, &js_fn);
  js_set_named_property(env, exports, name, js_fn);
}

static js_value_t *
rgb_lib_bare_exports(js_env_t *env, js_value_t *exports) {
  // Stateless
  set_fn(env, exports, "generateKeys", fn_generate_keys);
  set_fn(env, exports, "restoreKeys", fn_restore_keys);

  // Constructors
  set_fn(env, exports, "newWallet", fn_new_wallet);
  set_fn(env, exports, "goOnline", fn_go_online);
  set_fn(env, exports, "invoiceNew", fn_invoice_new);
  set_fn(env, exports, "restoreBackup", fn_restore_backup);

  // Wallet queries
  set_fn(env, exports, "getAddress", fn_get_address);
  set_fn(env, exports, "getAssetBalance", fn_get_asset_balance);
  set_fn(env, exports, "getBtcBalance", fn_get_btc_balance);
  set_fn(env, exports, "getFeeEstimation", fn_get_fee_estimation);
  set_fn(env, exports, "listAssets", fn_list_assets);
  set_fn(env, exports, "listTransfers", fn_list_transfers);
  set_fn(env, exports, "listTransactions", fn_list_transactions);
  set_fn(env, exports, "listUnspents", fn_list_unspents);
  set_fn(env, exports, "sync", fn_sync);
  set_fn(env, exports, "refresh", fn_refresh);

  // UTXO management
  set_fn(env, exports, "createUtxos", fn_create_utxos);
  set_fn(env, exports, "createUtxosBegin", fn_create_utxos_begin);
  set_fn(env, exports, "createUtxosEnd", fn_create_utxos_end);

  // Send / Transfer
  set_fn(env, exports, "send", fn_send);
  set_fn(env, exports, "sendBegin", fn_send_begin);
  set_fn(env, exports, "sendEnd", fn_send_end);
  set_fn(env, exports, "sendBtc", fn_send_btc);
  set_fn(env, exports, "blindReceive", fn_blind_receive);
  set_fn(env, exports, "witnessReceive", fn_witness_receive);

  // Asset issuance
  set_fn(env, exports, "issueAssetNia", fn_issue_asset_nia);
  set_fn(env, exports, "issueAssetCfa", fn_issue_asset_cfa);
  set_fn(env, exports, "issueAssetIfa", fn_issue_asset_ifa);
  set_fn(env, exports, "issueAssetUda", fn_issue_asset_uda);
  set_fn(env, exports, "inflate", fn_inflate);

  // PSBT
  set_fn(env, exports, "signPsbt", fn_sign_psbt);
  set_fn(env, exports, "finalizePsbt", fn_finalize_psbt);

  // Backup
  set_fn(env, exports, "backup", fn_backup);
  set_fn(env, exports, "backupInfo", fn_backup_info);

  // Invoice
  set_fn(env, exports, "invoiceData", fn_invoice_data);
  set_fn(env, exports, "invoiceString", fn_invoice_string);

  // Additional wallet ops (v0.3.0-beta.15)
  set_fn(env, exports, "getAssetMetadata", fn_get_asset_metadata);
  set_fn(env, exports, "deleteTransfers", fn_delete_transfers);
  set_fn(env, exports, "failTransfers", fn_fail_transfers);
  set_fn(env, exports, "sendBtcBegin", fn_send_btc_begin);
  set_fn(env, exports, "sendBtcEnd", fn_send_btc_end);
  set_fn(env, exports, "validateConsignment", fn_validate_consignment);

  // VSS backup (v0.3.0-beta.15)
  set_fn(env, exports, "newVssBackupClient", fn_new_vss_backup_client);
  set_fn(env, exports, "configureVssBackup", fn_configure_vss_backup);
  set_fn(env, exports, "disableVssAutoBackup", fn_disable_vss_auto_backup);
  set_fn(env, exports, "vssBackup", fn_vss_backup);
  set_fn(env, exports, "vssBackupInfo", fn_vss_backup_info);
  set_fn(env, exports, "vssBackupClientEncryptionEnabled", fn_vss_backup_client_encryption_enabled);
  set_fn(env, exports, "vssDeleteBackup", fn_vss_delete_backup);
  set_fn(env, exports, "restoreFromVss", fn_restore_from_vss);

  // Free (explicit drop)
  set_fn(env, exports, "freeWallet", fn_free_wallet);
  set_fn(env, exports, "freeOnline", fn_free_online);
  set_fn(env, exports, "freeInvoice", fn_free_invoice);
  set_fn(env, exports, "freeVssBackupClient", fn_free_vss_backup_client);

  return exports;
}

BARE_MODULE(rgb_lib_bare, rgb_lib_bare_exports)
