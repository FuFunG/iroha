/**
 * Copyright Soramitsu Co., Ltd. 2018 All Rights Reserved.
 * http://soramitsu.co.jp
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef IROHA_SHA3_HASH_HPP
#define IROHA_SHA3_HASH_HPP

#include "crypto/hash_types.hpp"

namespace iroha {
  namespace model {
    struct Transaction;
    struct Block;
    struct Query;
  }  // namespace model

  hash256_t hash(const model::Transaction &tx);
  hash256_t hash(const model::Block &block);
  hash256_t hash(const model::Query &query);
}  // namespace iroha
#endif  // IROHA_SHA3_HASH_HPP
