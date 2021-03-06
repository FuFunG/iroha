/**
 * Copyright Soramitsu Co., Ltd. 2017 All Rights Reserved.
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

#ifndef IROHA_PEER_BUILDER_HPP
#define IROHA_PEER_BUILDER_HPP

#include "interfaces/common_objects/peer.hpp"
#include "module/shared_model/builders/common_objects/common.hpp"

// TODO: 14.02.2018 nickaleks Add check for uninitialized fields IR-972

namespace shared_model {
  namespace builder {

    /**
     * PeerBuilder is a class, used for construction of Peer objects
     * @tparam BuilderImpl is a type, which defines builder for implementation
     * of shared_model. Since we return abstract classes, it is necessary for
     * them to be instantiated with some concrete implementation
     * @tparam Validator is a type, whose responsibility is
     * to perform stateless validation on model fields
     */
    template <typename BuilderImpl, typename Validator>
    class DEPRECATED PeerBuilder
        : public CommonObjectBuilder<interface::Peer, BuilderImpl, Validator> {
     public:
      PeerBuilder address(const interface::types::AddressType &address) {
        PeerBuilder copy(*this);
        copy.builder_ = this->builder_.address(address);
        return copy;
      }

      PeerBuilder pubkey(const interface::types::PubkeyType &key) {
        PeerBuilder copy(*this);
        copy.builder_ = this->builder_.pubkey(key);
        return copy;
      }

     protected:
      virtual std::string builderName() const override {
        return "Peer Builder";
      }

      virtual validation::ReasonsGroupType validate(
          const interface::Peer &object) override {
        validation::ReasonsGroupType reasons;
        this->validator_.validatePeer(reasons, object);

        return reasons;
      }
    };
  }  // namespace builder
}  // namespace shared_model

#endif  // IROHA_PEER_BUILDER_HPP
