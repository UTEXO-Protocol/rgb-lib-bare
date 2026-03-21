#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>


typedef enum CResultValue {
  Ok,
  Err,
} CResultValue;

typedef struct COpaqueStruct {
  const void *ptr;
  uint64_t ty;
} COpaqueStruct;

typedef struct CResult {
  enum CResultValue result;
  struct COpaqueStruct inner;
} CResult;

typedef struct CResultString {
  enum CResultValue result;
  char *inner;
} CResultString;

void free_invoice(struct COpaqueStruct obj);

void free_online(struct COpaqueStruct obj);

void free_wallet(struct COpaqueStruct obj);

struct CResult rgblib_backup(const struct COpaqueStruct *wallet,
                             const char *backup_path,
                             const char *password);

struct CResultString rgblib_backup_info(const struct COpaqueStruct *wallet);

struct CResultString rgblib_blind_receive(const struct COpaqueStruct *wallet,
                                          const char *asset_id_opt,
                                          const char *assignment,
                                          const char *duration_seconds_opt,
                                          const char *transport_endpoints,
                                          const char *min_confirmations);

struct CResultString rgblib_create_utxos(const struct COpaqueStruct *wallet,
                                         const struct COpaqueStruct *online,
                                         bool up_to,
                                         const char *num_opt,
                                         const char *size_opt,
                                         const char *fee_rate,
                                         bool skip_sync);

struct CResultString rgblib_create_utxos_begin(const struct COpaqueStruct *wallet,
                                               const struct COpaqueStruct *online,
                                               bool up_to,
                                               const char *num_opt,
                                               const char *size_opt,
                                               const char *fee_rate,
                                               bool skip_sync);

struct CResultString rgblib_create_utxos_end(const struct COpaqueStruct *wallet,
                                             const struct COpaqueStruct *online,
                                             const char *signed_psbt,
                                             bool skip_sync);

struct CResultString rgblib_finalize_psbt(const struct COpaqueStruct *wallet,
                                          const char *signed_psbt);

struct CResultString rgblib_generate_keys(const char *bitcoin_network);

struct CResultString rgblib_get_address(const struct COpaqueStruct *wallet);

struct CResultString rgblib_get_asset_balance(const struct COpaqueStruct *wallet,
                                              const char *asset_id);

struct CResultString rgblib_get_btc_balance(const struct COpaqueStruct *wallet,
                                            const struct COpaqueStruct *online,
                                            bool skip_sync);

struct CResultString rgblib_get_fee_estimation(const struct COpaqueStruct *wallet,
                                               const struct COpaqueStruct *online,
                                               const char *blocks);

struct CResult rgblib_go_online(const struct COpaqueStruct *wallet,
                                bool skip_consistency_check,
                                const char *electrum_url);

struct CResultString rgblib_inflate(const struct COpaqueStruct *wallet,
                                    const struct COpaqueStruct *online,
                                    const char *asset_id,
                                    const char *inflation_amounts,
                                    const char *fee_rate,
                                    const char *min_confirmations);

struct CResultString rgblib_invoice_data(const struct COpaqueStruct *invoice);

struct CResult rgblib_invoice_new(const char *invoice_string);

struct CResultString rgblib_invoice_string(const struct COpaqueStruct *invoice);

struct CResultString rgblib_issue_asset_cfa(const struct COpaqueStruct *wallet,
                                            const char *name,
                                            const char *details_opt,
                                            const char *precision,
                                            const char *amounts,
                                            const char *file_path_opt);

struct CResultString rgblib_issue_asset_ifa(const struct COpaqueStruct *wallet,
                                            const char *ticker,
                                            const char *name,
                                            const char *precision,
                                            const char *amounts,
                                            const char *inflation_amounts,
                                            const char *replace_rights_num,
                                            const char *reject_list_url_opt);

struct CResultString rgblib_issue_asset_nia(const struct COpaqueStruct *wallet,
                                            const char *ticker,
                                            const char *name,
                                            const char *precision,
                                            const char *amounts);

struct CResultString rgblib_issue_asset_uda(const struct COpaqueStruct *wallet,
                                            const char *ticker,
                                            const char *name,
                                            const char *details_opt,
                                            const char *precision,
                                            const char *media_file_path_opt,
                                            const char *attachments_file_paths);

struct CResultString rgblib_list_assets(const struct COpaqueStruct *wallet,
                                        const char *filter_asset_schemas);

struct CResultString rgblib_list_transactions(const struct COpaqueStruct *wallet,
                                              const struct COpaqueStruct *online,
                                              bool skip_sync);

struct CResultString rgblib_list_transfers(const struct COpaqueStruct *wallet,
                                           const char *asset_id_opt);

struct CResultString rgblib_list_unspents(const struct COpaqueStruct *wallet,
                                          const struct COpaqueStruct *online,
                                          bool settled_only,
                                          bool skip_sync);

struct CResult rgblib_new_wallet(const char *wallet_data);

struct CResultString rgblib_refresh(const struct COpaqueStruct *wallet,
                                    const struct COpaqueStruct *online,
                                    const char *asset_id_opt,
                                    const char *filter,
                                    bool skip_sync);

struct CResult rgblib_restore_backup(const char *backup_path,
                                     const char *password,
                                     const char *target_dir);

struct CResultString rgblib_restore_keys(const char *bitcoin_network, const char *mnemonic);

struct CResultString rgblib_send(const struct COpaqueStruct *wallet,
                                 const struct COpaqueStruct *online,
                                 const char *recipient_map,
                                 bool donation,
                                 const char *fee_rate,
                                 const char *min_confirmations,
                                 bool skip_sync);

struct CResultString rgblib_send_begin(const struct COpaqueStruct *wallet,
                                       const struct COpaqueStruct *online,
                                       const char *recipient_map,
                                       bool donation,
                                       const char *fee_rate,
                                       const char *min_confirmations);

struct CResultString rgblib_send_btc(const struct COpaqueStruct *wallet,
                                     const struct COpaqueStruct *online,
                                     const char *address,
                                     const char *amount,
                                     const char *fee_rate,
                                     bool skip_sync);

struct CResultString rgblib_send_end(const struct COpaqueStruct *wallet,
                                     const struct COpaqueStruct *online,
                                     const char *signed_psbt,
                                     bool skip_sync);

struct CResultString rgblib_sign_psbt(const struct COpaqueStruct *wallet,
                                      const char *unsigned_psbt);

struct CResultString rgblib_sync(const struct COpaqueStruct *wallet,
                                 const struct COpaqueStruct *online);

struct CResultString rgblib_witness_receive(const struct COpaqueStruct *wallet,
                                            const char *asset_id_opt,
                                            const char *assignment,
                                            const char *duration_seconds_opt,
                                            const char *transport_endpoints,
                                            const char *min_confirmations);
