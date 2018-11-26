/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef IROHA_ON_DEMAND_CONNECTION_MANAGER_HPP
#define IROHA_ON_DEMAND_CONNECTION_MANAGER_HPP

#include "ordering/on_demand_os_transport.hpp"

#include <condition_variable>
#include <shared_mutex>

#include <rxcpp/rx.hpp>
#include "logger/logger.hpp"

namespace iroha {
  namespace ordering {

    /**
     * Proxy class which redirects requests to appropriate peers
     */
    class OnDemandConnectionManager : public transport::OdOsNotification {
     public:
      /**
       * Responsibilities of individual peers from the peers array
       * Transactions are sent to three ordering services:
       * reject round for current block, reject round for next block, and
       * commit for subsequent next round
       * Proposal is requested from the current ordering service: issuer
       */
      enum PeerType {
        kCurrentRoundRejectConsumer = 0,
        kNextRoundRejectConsumer,
        kNextRoundCommitConsumer,
        kIssuer,
        kCount
      };

      /// Collection with value types which represent peers
      template <typename T>
      using PeerCollectionType = std::array<T, kCount>;

      /**
       * Current peers to send transactions and request proposals
       * @see PeerType for individual descriptions
       */
      struct PropagationParams {
        PeerCollectionType<std::shared_ptr<shared_model::interface::Peer>>
            peers;
        consensus::Round current_round;
      };

      OnDemandConnectionManager(
          std::shared_ptr<transport::OdOsNotificationFactory> factory,
          rxcpp::observable<PropagationParams> propagation_params);

      OnDemandConnectionManager(
          std::shared_ptr<transport::OdOsNotificationFactory> factory,
          rxcpp::observable<PropagationParams> propagation_params,
          PropagationParams initial_peers);

      void onBatches(consensus::Round round, CollectionType batches) override;

      boost::optional<ProposalType> onRequestProposal(
          consensus::Round round) override;

     private:
      /**
       * Corresponding connections created by OdOsNotificationFactory
       * @see PeerType for individual descriptions
       */
      struct CurrentConnections {
        PeerCollectionType<std::unique_ptr<transport::OdOsNotification>> peers;
      };

      /**
       * Initialize corresponding peers in connections_ using factory_
       * @param peers to initialize connections with
       */
      void initializeConnections(const PropagationParams &peers);

      /**
       * Match the requested round and the current propagation data round.
       * Will warn if the newer round is actually used and wait for the newer
       * propagation data if the requested round is newer than current.
       *
       * @param request_round - the round an operation was requested for.
       * @param lock - the lock used for this operation. Must be locked.
       * @return true if we have the appropriate propagation data to proceed
       *    with the operation, false otherwise.
       */
      template <typename Lock>
      bool verifyPropagtionData(consensus::Round request_round, Lock &lock);

      logger::Logger log_;
      std::shared_ptr<transport::OdOsNotificationFactory> factory_;
      rxcpp::composite_subscription subscription_;

      CurrentConnections connections_;
      consensus::Round current_round_;

      std::shared_timed_mutex mutex_;
      std::condition_variable_any propagation_data_update_cv_;
    };

  }  // namespace ordering
}  // namespace iroha

#endif  // IROHA_ON_DEMAND_CONNECTION_MANAGER_HPP
