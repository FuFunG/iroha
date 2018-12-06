/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "framework/integration_framework/fake_peer/network/mst_network_notifier.hpp"

namespace integration_framework {

  void MstNetworkNotifier::onNewState(
      const shared_model::crypto::PublicKey &from,
      const iroha::MstState &new_state) {
    mst_subject_.get_subscriber().on_next(
        std::make_shared<MstMessage>(from, new_state));
  }

  rxcpp::observable<MstNetworkNotifier::MstMessagePtr>
  MstNetworkNotifier::get_observable() {
    return mst_subject_.get_observable();
  }

}  // namespace integration_framework
