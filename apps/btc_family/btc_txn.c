/**
 * @file    btc_txn.c
 * @author  Cypherock X1 Team
 * @brief   Bitcoin family transaction flow.
 * @copyright Copyright (c) 2023 HODL TECH PTE LTD
 * <br/> You may obtain a copy of license at <a href="https://mitcc.org/"
 *target=_blank>https://mitcc.org/</a>
 *
 ******************************************************************************
 * @attention
 *
 * (c) Copyright 2023 by HODL TECH PTE LTD
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 *
 * "Commons Clause" License Condition v1.0
 *
 * The Software is provided to you by the Licensor under the License,
 * as defined below, subject to the following condition.
 *
 * Without limiting other conditions in the License, the grant of
 * rights under the License will not include, and the License does not
 * grant to you, the right to Sell the Software.
 *
 * For purposes of the foregoing, "Sell" means practicing any or all
 * of the rights granted to you under the License to provide to third
 * parties, for a fee or other consideration (including without
 * limitation fees for hosting or consulting/ support services related
 * to the Software), a product or service whose value derives, entirely
 * or substantially, from the functionality of the Software. Any license
 * notice or attribution required by the License must also include
 * this Commons Clause License Condition notice.
 *
 * Software: All X1Wallet associated files.
 * License: MIT
 * Licensor: HODL TECH PTE LTD
 *
 ******************************************************************************
 */

/*****************************************************************************
 * INCLUDES
 *****************************************************************************/

#include "btc_api.h"
#include "btc_helpers.h"
#include "btc_priv.h"
#include "btc_txn_helpers.h"
#include "constant_texts.h"
#include "reconstruct_seed_flow.h"
#include "script.h"
#include "status_api.h"
#include "ui_core_confirm.h"
#include "ui_screens.h"
#include "wallet_list.h"

/*****************************************************************************
 * EXTERN VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * PRIVATE MACROS AND DEFINES
 *****************************************************************************/

/*****************************************************************************
 * PRIVATE TYPEDEFS
 *****************************************************************************/

/*****************************************************************************
 * STATIC FUNCTION PROTOTYPES
 *****************************************************************************/

/**
 * @brief Checks if the provided query contains expected request.
 * @details The function performs the check on the request type and if the check
 * fails, then it will send an error to the host bitcoin app and return false.
 *
 * @param query Reference to an instance of btc_query_t containing query
 * received from host app
 * @param which_request The expected request type enum
 *
 * @return bool Indicating if the check succeeded or failed
 * @retval true If the query contains the expected request
 * @retval false If the query does not contain the expected request
 */
static bool check_which_request(const btc_query_t *query,
                                pb_size_t which_request);

/**
 * @brief Validates the derivation path received in the request from host
 * @details The function validates the provided account derivation path in the
 * request. If invalid path is detected, the function will send an error to the
 * host and return false.
 *
 * @param request Reference to an instance of btc_sign_txn_request_t
 * @return bool Indicating if the verification passed or failed
 * @retval true If all the derivation path entries are valid
 * @retval false If any of the derivation path entries are invalid
 */
static bool validate_request_data(const btc_sign_txn_request_t *request);

/**
 * @brief The function prepares and sends empty responses
 *
 * @param which_response Constant value for the response type to be sent
 */
static void send_response(pb_size_t which_response);

/**
 * @brief Takes already received and decoded query for the user confirmation.
 * @details The function will verify if the query contains the BTC_SIGN_TXN type
 * of request. Additionally, the wallet-id is validated for sanity and the
 * derivation path for the account is also validated. After the validations,
 * user is prompted about the action for confirmation. The function returns true
 * indicating all the validation and user confirmation was a success. The
 * function also duplicates the data from query into the btc_txn_context  for
 * further processing.
 *
 * @param query Constant reference to the decoded query received from the host
 *
 * @return bool Indicating if the function actions succeeded or failed
 * @retval true If all the validation and user confirmation was positive
 * @retval false If any of the validation or user confirmation was negative
 */
static bool handle_initiate_query(const btc_query_t *query);

/**
 * @brief Handles fetching of the metadata/top-level transaction elements
 * @details The function waits on USB event then decoding and validation of the
 * received query. Post validation, based on the values in the query, the
 * function allocates memory for storing inputs & outputs in btc_txn_context.
 * Also, the data received in the query is duplicated into btc_txn_context .
 *
 * @param query Reference to storage for decoding query from host
 *
 * @return bool Indicating if the function actions succeeded or failed
 */
static bool fetch_transaction_meta(btc_query_t *query);

/**
 * @brief Fetches each input and along with its corresponding raw transaction
 * for verification
 * @details The function will try to fetch and consequently verify each input
 * by referring to the declared input count in btc_txn_context . The function
 * will duplicate each input transaction information into btc_txn_context.
 *
 * @param query Reference to an instance of btc_query_t for storing the
 * transient inputs.
 *
 * @return bool Indicating if all the inputs are received and verified
 * @retval true If all the inputs are fetched and verified
 * @retval flase If any of the inputs failed verification or weren't fetched
 */
static bool fetch_valid_input(btc_query_t *query);

/**
 * @brief Fetches the outputs list for the transaction
 * @details The function refers to the number of outputs declared in the
 * btc_txn_context . It will also duplicate the received output.
 *
 * @param query Reference to an instance of btc_query_t for storing the
 * transient outputs.
 *
 * @return bool Indicating if all the outputs were fetched
 * @retval true If all the outputs were fetched
 */
static bool fetch_valid_output(btc_query_t *query);

/**
 * @brief Aggregates user consent for all outputs and the transaction fee
 * @details The function encodes all the receiver addresses along with their
 * corresponding transfer value in BTC. It also calculates the transaction fee
 * and checks for exaggerated fees. The user is assisted with additional
 * prompt/warning if the function detects the high fee (for calculation of the
 * upper limit of fee see get_transaction_fee_threshold(). The exact fee amount
 * is also confirmed with the user.
 *
 * @return bool Indicating if the user confirmed the transaction
 * @retval true If user confirmed the fee (along with high fee prompt if
 * applicable) and all the receiver addresses along with the corresponding
 * value.
 * @retval false Immediate return if any of the confirmation is disapproved
 */
static bool get_user_verification();

/**
 *
 * @param query Reference to an instance of btc_query_t for storing the
 * transient outputs.
 *
 * @return
 */
static bool sign_input_utxo(btc_query_t *query);

/*****************************************************************************
 * STATIC VARIABLES
 *****************************************************************************/

static btc_txn_context_t *btc_txn_context = NULL;

/*****************************************************************************
 * GLOBAL VARIABLES
 *****************************************************************************/

/*****************************************************************************
 * STATIC FUNCTIONS
 *****************************************************************************/

static bool check_which_request(const btc_query_t *query,
                                pb_size_t which_request) {
  if (which_request != query->sign_txn.which_request) {
    btc_send_error(ERROR_COMMON_ERROR_CORRUPT_DATA_TAG,
                   ERROR_DATA_FLOW_INVALID_REQUEST);
    return false;
  }

  return true;
}

static bool validate_request_data(const btc_sign_txn_request_t *request) {
  bool status = true;

  if (!btc_derivation_path_guard(request->initiate.derivation_path,
                                 request->initiate.derivation_path_count)) {
    btc_send_error(ERROR_COMMON_ERROR_CORRUPT_DATA_TAG,
                   ERROR_DATA_FLOW_INVALID_DATA);
    status = false;
  }
  return status;
}

static void send_response(const pb_size_t which_response) {
  btc_result_t result = init_btc_result(BTC_RESULT_SIGN_TXN_TAG);
  result.sign_txn.which_response = which_response;
  btc_send_result(&result);
}

static bool handle_initiate_query(const btc_query_t *query) {
  char wallet_name[NAME_SIZE] = "";
  char msg[100] = "";

  // TODO: Handle wallet search failures - eg: Wallet ID not found, Wallet
  // ID found but is invalid/locked wallet
  if (!check_which_request(query, BTC_SIGN_TXN_REQUEST_INITIATE_TAG) ||
      !validate_request_data(&query->sign_txn) ||
      !get_wallet_name_by_id(query->sign_txn.initiate.wallet_id,
                             (uint8_t *)wallet_name)) {
    return false;
  }

  snprintf(msg, sizeof(msg), UI_TEXT_BTC_SEND_PROMPT, g_app->name, wallet_name);
  // Take user consent to sign transaction for the wallet
  if (!core_confirmation(msg, btc_send_error)) {
    return false;
  }

  core_status_set_flow_status(BTC_SIGN_TXN_STATUS_CONFIRM);
  memcpy(&btc_txn_context->init_info,
         &query->sign_txn.initiate,
         sizeof(btc_sign_txn_initiate_request_t));
  send_response(BTC_SIGN_TXN_RESPONSE_CONFIRMATION_TAG);
  return true;
}

static bool fetch_transaction_meta(btc_query_t *query) {
  if (!btc_get_query(query, BTC_QUERY_SIGN_TXN_TAG) ||
      !check_which_request(query, BTC_SIGN_TXN_REQUEST_META_TAG)) {
    return false;
  }
  // check important information for supported/compatibility
  if (0x00000001 != query->sign_txn.meta.sighash ||
      0 == query->sign_txn.meta.input_count ||
      0 == query->sign_txn.meta.output_count) {
    // TODO: Add upper limit on number input+output count
    /** Do not accept transaction with empty input/output.
     * Only accept SIGHASH_ALL for sighash type More info:
     * https://wiki.bitcoinsv.io/index.php/SIGHASH_flags
     */
    btc_send_error(ERROR_COMMON_ERROR_CORRUPT_DATA_TAG,
                   ERROR_DATA_FLOW_INVALID_DATA);
    return false;
  }

  // we now know the number of input and outputs
  // allocate memory for input and outputs in btc_txn_context
  memcpy(&btc_txn_context->metadata,
         &query->sign_txn.meta,
         sizeof(btc_sign_txn_metadata_t));
  btc_txn_context->inputs = (btc_txn_input_t *)malloc(
      sizeof(btc_txn_input_t) * btc_txn_context->metadata.input_count);
  btc_txn_context->outputs = (btc_sign_txn_output_t *)malloc(
      sizeof(btc_sign_txn_output_t) * btc_txn_context->metadata.output_count);
  // TODO: check if malloc failed; report to host and exit
  send_response(BTC_SIGN_TXN_RESPONSE_META_ACCEPTED_TAG);
  return true;
}

static bool fetch_valid_input(btc_query_t *query) {
  // Validate inputs for safety from attack. Ref:
  // https://blog.trezor.io/details-of-firmware-updates-for-trezor-one-version-1-9-1-and-trezor-model-t-version-2-3-1-1eba8f60f2dd
  for (int idx = 0; idx < btc_txn_context->metadata.input_count; idx++) {
    if (!btc_get_query(query, BTC_QUERY_SIGN_TXN_TAG) &&
        !check_which_request(query, BTC_SIGN_TXN_REQUEST_INPUT_TAG)) {
      return false;
    }
    // TODO: check input type; exit if unsupported
    // P2PK 68, P2PKH 25 (21 excluding OP_CODES), P2WPKH 22, P2MS ~, P2SH 23 (21
    // excluding OP_CODES) refer https://learnmeabitcoin.com/technical/script
    // for explaination Currently the device can spend P2PKH or P2WPKH inputs
    // only for (int i = 0; i < in_count; i++) {
    //   if (txn_ctx->inputs[i].script_pub_key.size != 22 &&
    //       txn_ctx->inputs[i].script_pub_key.size != 25) {
    //     return false;
    //   }
    // }

    // verify transaction details and discard the raw-transaction (prev_txn)
    const btc_sign_txn_input_prev_txn_t *txn = &query->sign_txn.input.prev_txn;
    if (!btc_verify_input(
            txn->bytes, txn->size, &btc_txn_context->inputs[idx])) {
      // input validation failed, terminate immediately
      btc_send_error(ERROR_COMMON_ERROR_CORRUPT_DATA_TAG,
                     ERROR_DATA_FLOW_INVALID_DATA);
      return false;
    }

    // clone the input details into btc_txn_context
    btc_txn_input_t *input = &btc_txn_context->inputs[idx];
    input->prev_output_index = query->sign_txn.input.prev_output_index;
    input->address_index = query->sign_txn.input.address_index;
    input->change_index = query->sign_txn.input.change_index;
    input->value = query->sign_txn.input.value;
    input->sequence = query->sign_txn.input.sequence;
    input->script_pub_key.size = query->sign_txn.input.script_pub_key.size;
    memcpy(input->prev_txn_hash, query->sign_txn.input.prev_txn_hash, 32);
    memcpy(input->script_pub_key.bytes,
           query->sign_txn.input.script_pub_key.bytes,
           input->script_pub_key.size);

    // send accepted response to indicate validation of input passed
    send_response(BTC_SIGN_TXN_RESPONSE_INPUT_ACCEPTED_TAG);
  }
  return true;
}

static bool fetch_valid_output(btc_query_t *query) {
  // track if it is a zero valued transaction; all input is going into fee
  bool zero_value_transaction = true;

  for (int idx = 0; idx < btc_txn_context->metadata.output_count; idx++) {
    if (!btc_get_query(query, BTC_QUERY_SIGN_TXN_TAG) &&
        !check_which_request(query, BTC_SIGN_TXN_REQUEST_OUTPUT_TAG)) {
      return false;
    }

    btc_sign_txn_output_t *output = &query->sign_txn.output;
    memcpy(
        &btc_txn_context->outputs[idx], output, sizeof(btc_sign_txn_output_t));
    if (0 != output->value) {
      zero_value_transaction = false;
    }
    if (OP_RETURN == output->script_pub_key.bytes[0] && 0 != output->value) {
      // ensure any funds are not being locked (not spendable)
      btc_send_error(ERROR_COMMON_ERROR_CORRUPT_DATA_TAG,
                     ERROR_DATA_FLOW_INVALID_DATA);
      return false;
    }
    // send accepted response to indicate validation of output passed
    send_response(BTC_SIGN_TXN_RESPONSE_INPUT_ACCEPTED_TAG);
  }
  if (true == zero_value_transaction) {
    // do not allow zero valued transaction; all input is going into fee
    btc_send_error(ERROR_COMMON_ERROR_CORRUPT_DATA_TAG,
                   ERROR_DATA_FLOW_INVALID_DATA);
    return false;
  }
  return true;
}

static bool get_user_verification() {
  char title[20] = "";
  char value[100] = "";
  char address[100] = "";

  for (int idx = 0; idx < btc_txn_context->metadata.output_count; idx++) {
    btc_sign_txn_output_t *output = &btc_txn_context->outputs[idx];
    btc_sign_txn_output_script_pub_key_t *script = &output->script_pub_key;
    snprintf(title, sizeof(title), UI_TEXT_BTC_RECEIVER, (idx + 1));

    if (true == output->is_change) {
      // do not show the change outputs to user
      continue;
    }
    format_value(output->value, value, sizeof(value));
    script_output_to_address(
        script->bytes, script->size, address, sizeof(address));
    if (!core_scroll_page(title, address, btc_send_error) ||
        !core_scroll_page(title, value, btc_send_error)) {
      return false;
    }
  }

  // calculate fee in various forms
  uint64_t max_fee = get_transaction_fee_threshold(btc_txn_context);
  uint64_t fee_in_satoshi = 0;

  if (!btc_get_txn_fee(btc_txn_context, &fee_in_satoshi)) {
    // The transaction is overspending
    btc_send_error(ERROR_COMMON_ERROR_CORRUPT_DATA_TAG,
                   ERROR_DATA_FLOW_INVALID_DATA);
    return false;
  }

  // all the receivers are verified, check fee limit & show the fee
  // validate fee limit is not too high and acceptable to user
  if (fee_in_satoshi > max_fee &&
      !core_confirmation(ui_text_warning_txn_fee_too_high, btc_send_error)) {
    return false;
  }

  format_value(fee_in_satoshi, value, sizeof(value));
  if (!core_scroll_page(UI_TEXT_BTC_FEE, value, btc_send_error)) {
    return false;
  }
  return true;
}

static bool sign_input_utxo(btc_query_t *query) {
  if (!btc_get_query(query, BTC_QUERY_SIGN_TXN_TAG) ||
      !check_which_request(query, BTC_SIGN_TXN_REQUEST_SIGNATURE_TAG)) {
    return false;
  }

  // TODO: Sign each input and send the signature
  return false;
}

/*****************************************************************************
 * GLOBAL FUNCTIONS
 *****************************************************************************/

void btc_sign_transaction(btc_query_t *query) {
  btc_txn_context = (btc_txn_context_t *)malloc(sizeof(btc_txn_context_t));
  memzero(btc_txn_context, sizeof(btc_txn_context_t));

  if (!handle_initiate_query(query) && !fetch_transaction_meta(query) &&
      !fetch_valid_input(query) && !fetch_valid_output(query) &&
      !get_user_verification() && !sign_input_utxo(query)) {
    delay_scr_init(ui_text_check_cysync, DELAY_TIME);
  }

  if (NULL != btc_txn_context && NULL != btc_txn_context->inputs) {
    free(btc_txn_context->inputs);
  }
  if (NULL != btc_txn_context && NULL != btc_txn_context->outputs) {
    free(btc_txn_context->outputs);
  }
  if (NULL != btc_txn_context) {
    free(btc_txn_context);
    btc_txn_context = NULL;
  }
}