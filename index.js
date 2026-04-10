/**
 * @utexo/rgb-lib-bare - JavaScript wrapper providing the same API as @utexo/rgb-lib (SWIG)
 *
 * This module wraps the bare native addon binding to present the same
 * class-based interface that rgb-lib-client.ts expects.
 */

const binding = require('./binding')

/**
 * Parse JSON result from C FFI. Returns raw value if not valid JSON.
 */
function parseResult (raw) {
  if (typeof raw !== 'string') return raw
  try { return JSON.parse(raw) } catch { return raw }
}

/**
 * Ensure a value is a string for C FFI. Arrays/objects → JSON.stringify,
 * numbers → String(), null/undefined → null (C FFI handles NULL).
 */
function toFFIString (val) {
  if (val === null || val === undefined) return null
  if (typeof val === 'string') return val
  if (typeof val === 'number' || typeof val === 'bigint') return String(val)
  // Arrays, objects → JSON string
  return JSON.stringify(val)
}

// ============================================================================
// Enums (matching SWIG exports)
// ============================================================================

const DatabaseType = Object.freeze({
  Sqlite: 'Sqlite'
})

const AssetSchema = Object.freeze({
  Nia: 'Nia',
  Cfa: 'Cfa',
  Uda: 'Uda'
})

const BitcoinNetwork = Object.freeze({
  Mainnet: 'Mainnet',
  Testnet: 'Testnet',
  Testnet4: 'Testnet4',
  Signet: 'Signet',
  Regtest: 'Regtest'
})

// ============================================================================
// WalletData
// ============================================================================

class WalletData {
  constructor (data) {
    if (typeof data === 'string') {
      this._json = data
    } else {
      this._json = JSON.stringify(data)
    }
  }

  toString () {
    return this._json
  }
}

// ============================================================================
// Online handle wrapper
// ============================================================================

class Online {
  constructor (handle) {
    this._handle = handle
  }

  drop () {
    if (this._handle) {
      binding.freeOnline(this._handle)
      this._handle = null
    }
  }
}

// ============================================================================
// Invoice
// ============================================================================

class Invoice {
  constructor (invoiceString) {
    this._handle = binding.invoiceNew(invoiceString)
  }

  invoiceData () {
    return binding.invoiceData(this._handle)
  }

  invoiceString () {
    return binding.invoiceString(this._handle)
  }

  toString () {
    return this.invoiceString()
  }

  drop () {
    if (this._handle) {
      binding.freeInvoice(this._handle)
      this._handle = null
    }
  }
}

// ============================================================================
// Wallet
// ============================================================================

class Wallet {
  constructor (walletData, keys) {
    const json = walletData instanceof WalletData
      ? walletData.toString()
      : (typeof walletData === 'string' ? walletData : JSON.stringify(walletData))
    const keysJson = keys == null
      ? null
      : (typeof keys === 'string' ? keys : JSON.stringify(keys))
    this._handle = binding.newWallet(json, keysJson)
  }

  goOnline (skipConsistencyCheck, electrumUrl) {
    const handle = binding.goOnline(this._handle, !!skipConsistencyCheck, electrumUrl)
    return new Online(handle)
  }

  getAddress () {
    return binding.getAddress(this._handle)
  }

  getAssetBalance (assetId) {
    return parseResult(binding.getAssetBalance(this._handle, assetId))
  }

  getBtcBalance (online, skipSync) {
    const onlineHandle = online instanceof Online ? online._handle : online
    return parseResult(binding.getBtcBalance(this._handle, onlineHandle || null, !!skipSync))
  }

  getFeeEstimation (online, blocks) {
    const onlineHandle = online instanceof Online ? online._handle : online
    return parseResult(binding.getFeeEstimation(this._handle, onlineHandle, toFFIString(blocks)))
  }

  listAssets (filterAssetSchemas) {
    return parseResult(binding.listAssets(this._handle, toFFIString(filterAssetSchemas)))
  }

  listTransfers (assetIdOpt) {
    return parseResult(binding.listTransfers(this._handle, assetIdOpt || null))
  }

  listTransactions (online, skipSync) {
    const onlineHandle = online instanceof Online ? online._handle : online
    return parseResult(binding.listTransactions(this._handle, onlineHandle, !!skipSync))
  }

  listUnspents (online, settledOnly, skipSync) {
    const onlineHandle = online instanceof Online ? online._handle : online
    return parseResult(binding.listUnspents(this._handle, onlineHandle, !!settledOnly, !!skipSync))
  }

  sync (online) {
    const onlineHandle = online instanceof Online ? online._handle : online
    return parseResult(binding.sync(this._handle, onlineHandle))
  }

  refresh (online, assetIdOpt, filter, skipSync) {
    const onlineHandle = online instanceof Online ? online._handle : online
    return parseResult(binding.refresh(this._handle, onlineHandle, assetIdOpt || null, toFFIString(filter), !!skipSync))
  }

  createUtxos (online, upTo, numOpt, sizeOpt, feeRate, skipSync) {
    const onlineHandle = online instanceof Online ? online._handle : online
    return parseResult(binding.createUtxos(this._handle, onlineHandle, !!upTo,
      toFFIString(numOpt), toFFIString(sizeOpt), toFFIString(feeRate), !!skipSync))
  }

  createUtxosBegin (online, upTo, numOpt, sizeOpt, feeRate, skipSync) {
    const onlineHandle = online instanceof Online ? online._handle : online
    return binding.createUtxosBegin(this._handle, onlineHandle, !!upTo,
      toFFIString(numOpt), toFFIString(sizeOpt), toFFIString(feeRate), !!skipSync)  // returns raw PSBT string
  }

  createUtxosEnd (online, signedPsbt, skipSync) {
    const onlineHandle = online instanceof Online ? online._handle : online
    return parseResult(binding.createUtxosEnd(this._handle, onlineHandle, signedPsbt, !!skipSync))
  }

  send (online, recipientMap, donation, feeRate, minConfirmations, expirationTimestampOpt, skipSync) {
    const onlineHandle = online instanceof Online ? online._handle : online
    return parseResult(binding.send(this._handle, onlineHandle, toFFIString(recipientMap), !!donation,
      toFFIString(feeRate), toFFIString(minConfirmations), toFFIString(expirationTimestampOpt), !!skipSync))
  }

  sendBegin (online, recipientMap, donation, feeRate, minConfirmations, expirationTimestampOpt, dryRun) {
    const onlineHandle = online instanceof Online ? online._handle : online
    return binding.sendBegin(this._handle, onlineHandle, toFFIString(recipientMap), !!donation,
      toFFIString(feeRate), toFFIString(minConfirmations), toFFIString(expirationTimestampOpt), !!dryRun)  // returns raw PSBT string
  }

  sendEnd (online, signedPsbt, skipSync) {
    const onlineHandle = online instanceof Online ? online._handle : online
    return parseResult(binding.sendEnd(this._handle, onlineHandle, signedPsbt, !!skipSync))
  }

  sendBtc (online, address, amount, feeRate, skipSync) {
    const onlineHandle = online instanceof Online ? online._handle : online
    return parseResult(binding.sendBtc(this._handle, onlineHandle, address, toFFIString(amount),
      toFFIString(feeRate), !!skipSync))
  }

  blindReceive (assetIdOpt, assignment, durationSecondsOpt, transportEndpoints, minConfirmations) {
    return parseResult(binding.blindReceive(this._handle, assetIdOpt || null, toFFIString(assignment),
      toFFIString(durationSecondsOpt), toFFIString(transportEndpoints), toFFIString(minConfirmations)))
  }

  witnessReceive (assetIdOpt, assignment, durationSecondsOpt, transportEndpoints, minConfirmations) {
    return parseResult(binding.witnessReceive(this._handle, assetIdOpt || null, toFFIString(assignment),
      toFFIString(durationSecondsOpt), toFFIString(transportEndpoints), toFFIString(minConfirmations)))
  }

  issueAssetNia (ticker, name, precision, amounts) {
    return parseResult(binding.issueAssetNia(this._handle, ticker, name, toFFIString(precision), toFFIString(amounts)))
  }

  // SWIG-compatible uppercase aliases (rgb-lib-client.ts uses these)
  issueAssetNIA (ticker, name, precision, amounts) {
    return this.issueAssetNia(ticker, name, precision, amounts)
  }

  issueAssetCfa (name, detailsOpt, precision, amounts, filePathOpt) {
    return parseResult(binding.issueAssetCfa(this._handle, name, detailsOpt || null,
      toFFIString(precision), toFFIString(amounts), filePathOpt || null))
  }

  issueAssetCFA (name, detailsOpt, precision, amounts, filePathOpt) {
    return this.issueAssetCfa(name, detailsOpt, precision, amounts, filePathOpt)
  }

  issueAssetIfa (ticker, name, precision, amounts, inflationAmounts, rejectListUrlOpt) {
    return parseResult(binding.issueAssetIfa(this._handle, ticker, name, toFFIString(precision), toFFIString(amounts),
      toFFIString(inflationAmounts), rejectListUrlOpt || null))
  }

  issueAssetUda (ticker, name, detailsOpt, precision, mediaFilePathOpt, attachmentsFilePaths) {
    return parseResult(binding.issueAssetUda(this._handle, ticker, name, detailsOpt || null,
      toFFIString(precision), mediaFilePathOpt || null, toFFIString(attachmentsFilePaths)))
  }

  issueAssetUDA (ticker, name, detailsOpt, precision, mediaFilePathOpt, attachmentsFilePaths) {
    return this.issueAssetUda(ticker, name, detailsOpt, precision, mediaFilePathOpt, attachmentsFilePaths)
  }

  inflate (online, assetId, inflationAmounts, feeRate, minConfirmations) {
    const onlineHandle = online instanceof Online ? online._handle : online
    return parseResult(binding.inflate(this._handle, onlineHandle, assetId, toFFIString(inflationAmounts),
      toFFIString(feeRate), toFFIString(minConfirmations)))
  }

  signPsbt (unsignedPsbt) {
    return binding.signPsbt(this._handle, unsignedPsbt)  // returns raw PSBT string, no parse
  }

  finalizePsbt (signedPsbt) {
    return binding.finalizePsbt(this._handle, signedPsbt)  // returns raw PSBT string, no parse
  }

  backup (backupPath, password) {
    return binding.backup(this._handle, backupPath, password)
  }

  backupInfo () {
    return parseResult(binding.backupInfo(this._handle))
  }

  // ───── v0.3.0-beta.15 additions ─────

  getAssetMetadata (assetId) {
    return parseResult(binding.getAssetMetadata(this._handle, assetId))
  }

  deleteTransfers (batchTransferIdxOpt, noAssetOnly) {
    return parseResult(binding.deleteTransfers(this._handle, toFFIString(batchTransferIdxOpt), !!noAssetOnly))
  }

  failTransfers (online, batchTransferIdxOpt, noAssetOnly, skipSync) {
    const onlineHandle = online instanceof Online ? online._handle : online
    return parseResult(binding.failTransfers(this._handle, onlineHandle, toFFIString(batchTransferIdxOpt), !!noAssetOnly, !!skipSync))
  }

  rotateColoredAddress () {
    return parseResult(binding.rotateColoredAddress(this._handle))
  }

  rotateVanillaAddress () {
    return parseResult(binding.rotateVanillaAddress(this._handle))
  }

  // VSS backup methods
  configureVssBackup (configJson) {
    return parseResult(binding.configureVssBackup(this._handle, toFFIString(configJson)))
  }

  disableVssAutoBackup () {
    return parseResult(binding.disableVssAutoBackup(this._handle))
  }

  vssBackup (vssClient) {
    const clientHandle = vssClient instanceof VssBackupClient ? vssClient._handle : vssClient
    return parseResult(binding.vssBackup(this._handle, clientHandle))
  }

  vssBackupInfo (vssClient) {
    const clientHandle = vssClient instanceof VssBackupClient ? vssClient._handle : vssClient
    return parseResult(binding.vssBackupInfo(this._handle, clientHandle))
  }

  drop () {
    if (this._handle) {
      binding.freeWallet(this._handle)
      this._handle = null
    }
  }
}

// ============================================================================
// VssBackupClient (v0.3.0-beta.15)
// ============================================================================

class VssBackupClient {
  constructor (handle) {
    this._handle = handle
  }

  static create (configJson) {
    const handle = binding.newVssBackupClient(toFFIString(configJson))
    return new VssBackupClient(handle)
  }

  encryptionEnabled () {
    return parseResult(binding.vssBackupClientEncryptionEnabled(this._handle))
  }

  deleteBackup () {
    return parseResult(binding.vssDeleteBackup(this._handle))
  }

  drop () {
    if (this._handle) {
      binding.freeVssBackupClient(this._handle)
      this._handle = null
    }
  }
}

// ============================================================================
// Module exports (matching @utexo/rgb-lib SWIG surface)
// ============================================================================

module.exports = {
  // Classes
  Wallet,
  WalletData,
  Online,
  Invoice,
  VssBackupClient,

  // Enums
  DatabaseType,
  AssetSchema,
  BitcoinNetwork,

  // Global functions
  generateKeys (network) {
    return parseResult(binding.generateKeys(network))
  },
  restoreKeys (network, mnemonic) {
    return parseResult(binding.restoreKeys(network, mnemonic))
  },
  restoreBackup (backupPath, password, targetDir) {
    return binding.restoreBackup(backupPath, password, targetDir)
  },
  dropOnline (online) {
    if (online instanceof Online) {
      online.drop()
    } else if (online) {
      binding.freeOnline(online)
    }
  },

  // v0.3.0-beta.15+ additions — global functions
  validateConsignment (filePath, indexerUrl, bitcoinNetwork) {
    return parseResult(binding.validateConsignment(filePath, indexerUrl, bitcoinNetwork))
  },
  validateConsignmentOffchain (filePath, txid, indexerUrl, bitcoinNetwork) {
    return parseResult(binding.validateConsignmentOffchain(filePath, txid, indexerUrl, bitcoinNetwork))
  },
  restoreFromVss (configJson, targetDir) {
    return parseResult(binding.restoreFromVss(toFFIString(configJson), targetDir))
  }
}
