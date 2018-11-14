/**
 * Copyright Soramitsu Co., Ltd. All Rights Reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ordering/impl/on_demand_ordering_service_impl.hpp"

#include <memory>
#include <thread>

#include <gtest/gtest.h>
#include "backend/protobuf/proto_proposal_factory.hpp"
#include "builders/protobuf/transaction.hpp"
#include "datetime/time.hpp"
#include "interfaces/iroha_internal/transaction_batch_impl.hpp"
#include "module/irohad/ametsuchi/ametsuchi_mocks.hpp"
#include "module/shared_model/interface_mocks.hpp"
#include "module/shared_model/validators/validators.hpp"

using namespace iroha;
using namespace iroha::ordering;
using namespace iroha::ordering::transport;

using testing::_;
using testing::ByMove;
using testing::Matcher;
using testing::NiceMock;
using testing::Ref;
using testing::Return;

using shared_model::interface::Proposal;
using shared_model::validation::MockValidator;
using MockProposalValidator = MockValidator<Proposal>;

class OnDemandOsTest : public ::testing::Test {
 public:
  std::shared_ptr<OnDemandOrderingService> os;
  const uint64_t transaction_limit = 20;
  const uint32_t proposal_limit = 5;
  const consensus::Round initial_round = {2, 1}, target_round = {4, 1},
                         commit_round = {3, 1}, reject_round = {2, 2};
  NiceMock<iroha::ametsuchi::MockTxPresenceCache> *mock_cache;

  void SetUp() override {
    // TODO: nickaleks IR-1811 use mock factory
    auto factory = std::make_unique<
        shared_model::proto::ProtoProposalFactory<MockProposalValidator>>();
    auto tx_cache =
        std::make_unique<NiceMock<iroha::ametsuchi::MockTxPresenceCache>>();
    mock_cache = tx_cache.get();
    // every batch is new by default
    ON_CALL(
        *mock_cache,
        check(
            testing::Matcher<const shared_model::interface::TransactionBatch &>(
                _)))
        .WillByDefault(Return(std::vector<iroha::ametsuchi::TxCacheStatusType>{
            iroha::ametsuchi::tx_cache_status_responses::Missing()}));
    os = std::make_shared<OnDemandOrderingServiceImpl>(transaction_limit,
                                                       std::move(factory),
                                                       std::move(tx_cache),
                                                       proposal_limit,
                                                       initial_round);
  }

  /**
   * Generate transactions with provided range
   * @param os - ordering service for insertion
   * @param range - pair of [from, to)
   */
  void generateTransactionsAndInsert(consensus::Round round,
                                     std::pair<uint64_t, uint64_t> range) {
    os->onBatches(round, generateTransactions(range));
  }

  OnDemandOrderingService::CollectionType generateTransactions(
      std::pair<uint64_t, uint64_t> range) {
    auto now = iroha::time::now();
    OnDemandOrderingService::CollectionType collection;

    for (auto i = range.first; i < range.second; ++i) {
      collection.push_back(
          std::make_unique<shared_model::interface::TransactionBatchImpl>(
              shared_model::interface::types::SharedTxsCollectionType{
                  std::make_unique<shared_model::proto::Transaction>(
                      shared_model::proto::TransactionBuilder()
                          .createdTime(now + i)
                          .creatorAccountId("foo@bar")
                          .createAsset("asset", "domain", 1)
                          .quorum(1)
                          .build()
                          .signAndAddSignature(
                              shared_model::crypto::DefaultCryptoAlgorithmType::
                                  generateKeypair())
                          .finish())}));
    }
    return collection;
  }

  std::unique_ptr<Proposal> makeMockProposal() {
    auto proposal = std::make_unique<NiceMock<MockProposal>>();
    // TODO: nickaleks IR-1811 clone should return initialized mock
    ON_CALL(*proposal, clone()).WillByDefault(Return(new MockProposal()));

    return proposal;
  }
};

/**
 * @given initialized on-demand OS
 * @when  don't send transactions
 * AND initiate next round
 * @then  check that previous round doesn't have proposal
 */
TEST_F(OnDemandOsTest, EmptyRound) {
  ASSERT_FALSE(os->onRequestProposal(initial_round));

  os->onCollaborationOutcome(commit_round);

  ASSERT_FALSE(os->onRequestProposal(initial_round));
}

/**
 * @given initialized on-demand OS
 * @when  send number of transactions less that limit
 * AND initiate next round
 * @then  check that previous round has all transaction
 */
TEST_F(OnDemandOsTest, NormalRound) {
  generateTransactionsAndInsert(target_round, {1, 2});

  os->onCollaborationOutcome(commit_round);

  ASSERT_TRUE(os->onRequestProposal(target_round));
}

/**
 * @given initialized on-demand OS
 * @when  send number of transactions greater that limit
 * AND initiate next round
 * @then  check that previous round has only limit of transactions
 * AND the rest of transactions isn't appeared in next after next round
 */
TEST_F(OnDemandOsTest, OverflowRound) {
  generateTransactionsAndInsert(target_round, {1, transaction_limit * 2});

  os->onCollaborationOutcome(commit_round);

  ASSERT_TRUE(os->onRequestProposal(target_round));
  ASSERT_EQ(transaction_limit,
            (*os->onRequestProposal(target_round))->transactions().size());
}

/**
 * @given initialized on-demand OS
 * @when  send transactions from different threads
 * AND initiate next round
 * @then  check that all transactions appear in proposal
 */
TEST_F(OnDemandOsTest, DISABLED_ConcurrentInsert) {
  auto large_tx_limit = 10000u;
  auto factory = std::make_unique<
      shared_model::proto::ProtoProposalFactory<MockProposalValidator>>();
  auto tx_cache = std::make_unique<StubTxCache>();
  os = std::make_shared<OnDemandOrderingServiceImpl>(large_tx_limit,
                                                     std::move(factory),
                                                     std::move(tx_cache),
                                                     proposal_limit,
                                                     initial_round);

  auto call = [this](auto bounds) {
    for (auto i = bounds.first; i < bounds.second; ++i) {
      this->generateTransactionsAndInsert(target_round, {i, i + 1});
    }
  };

  std::thread one(call, std::make_pair(0u, large_tx_limit / 2));
  std::thread two(call, std::make_pair(large_tx_limit / 2, large_tx_limit));
  one.join();
  two.join();
  os->onCollaborationOutcome(commit_round);
  ASSERT_EQ(large_tx_limit,
            os->onRequestProposal(target_round).get()->transactions().size());
}

/**
 * @given initialized on-demand OS
 * @when  insert proposal_limit rounds twice
 * @then  on second rounds check that old proposals are expired
 */
TEST_F(OnDemandOsTest, Erase) {
  for (auto i = commit_round.block_round;
       i < commit_round.block_round + proposal_limit;
       ++i) {
    generateTransactionsAndInsert({i + 1, commit_round.reject_round}, {1, 2});
    os->onCollaborationOutcome({i, commit_round.reject_round});
    ASSERT_TRUE(os->onRequestProposal({i + 1, commit_round.reject_round}));
  }

  for (consensus::BlockRoundType i = commit_round.block_round + proposal_limit;
       i < commit_round.block_round + 2 * proposal_limit;
       ++i) {
    generateTransactionsAndInsert({i + 1, commit_round.reject_round}, {1, 2});
    os->onCollaborationOutcome({i, commit_round.reject_round});
    ASSERT_FALSE(os->onRequestProposal(
        {i + 1 - proposal_limit, commit_round.reject_round}));
  }
}

/**
 * @given initialized on-demand OS
 * @when  insert proposal_limit rounds twice
 * AND outcome is reject
 * @then  on second rounds check that old proposals are expired
 */
TEST_F(OnDemandOsTest, EraseReject) {
  for (auto i = reject_round.reject_round;
       i < reject_round.reject_round + proposal_limit;
       ++i) {
    generateTransactionsAndInsert({reject_round.block_round, i + 1}, {1, 2});
    os->onCollaborationOutcome({reject_round.block_round, i});
    ASSERT_TRUE(os->onRequestProposal({reject_round.block_round, i + 1}));
  }

  for (consensus::RejectRoundType i =
           reject_round.reject_round + proposal_limit;
       i < reject_round.reject_round + 2 * proposal_limit;
       ++i) {
    generateTransactionsAndInsert({reject_round.block_round, i + 1}, {1, 2});
    os->onCollaborationOutcome({reject_round.block_round, i});
    ASSERT_FALSE(os->onRequestProposal(
        {reject_round.block_round, i + 1 - proposal_limit}));
  }
}

/**
 * @given initialized on-demand OS @and some transactions are sent to it
 * @when proposal is requested after calling onCollaborationOutcome
 * @then check that proposal factory is called and returns a proposal
 */
TEST_F(OnDemandOsTest, UseFactoryForProposal) {
  auto factory = std::make_unique<MockUnsafeProposalFactory>();
  auto mock_factory = factory.get();
  auto tx_cache = std::make_unique<StubTxCache>();
  os = std::make_shared<OnDemandOrderingServiceImpl>(transaction_limit,
                                                     std::move(factory),
                                                     std::move(tx_cache),
                                                     proposal_limit,
                                                     initial_round);

  EXPECT_CALL(*mock_factory, unsafeCreateProposal(_, _, _))
      .WillOnce(Return(ByMove(makeMockProposal())));

  generateTransactionsAndInsert(target_round, {1, 2});

  os->onCollaborationOutcome(commit_round);

  ASSERT_TRUE(os->onRequestProposal(target_round));
}

// Return matcher for batch, which passes it by const &
// used when passing batch as an argument to check() in transaction cache
auto batchRef(const shared_model::interface::TransactionBatch &batch) {
  return Matcher<const shared_model::interface::TransactionBatch &>(Ref(batch));
}

/**
 * @given initialized on-demand OS
 * @when add a batch which was already commited
 * @then the batch is not present in a proposal
 */
TEST_F(OnDemandOsTest, AlreadyProcessedProposalDiscarded) {
  auto batches = generateTransactions({1, 2});
  auto &batch = *batches.at(0);

  EXPECT_CALL(*mock_cache, check(batchRef(batch)))
      .WillOnce(Return(std::vector<iroha::ametsuchi::TxCacheStatusType>{
          iroha::ametsuchi::tx_cache_status_responses::Committed()}));

  os->onBatches(initial_round, batches);

  os->onCollaborationOutcome(commit_round);

  auto proposal = os->onRequestProposal(initial_round);

  EXPECT_FALSE(proposal);
}

/**
 * @given initialized on-demand OS
 * @when add a batch with new transaction
 * @then batch is present in a proposal
 */
TEST_F(OnDemandOsTest, PassMissingTransaction) {
  auto batches = generateTransactions({1, 2});
  auto &batch = *batches.at(0);

  EXPECT_CALL(*mock_cache, check(batchRef(batch)))
      .WillOnce(Return(std::vector<iroha::ametsuchi::TxCacheStatusType>{
          iroha::ametsuchi::tx_cache_status_responses::Missing()}));

  os->onBatches(target_round, batches);

  os->onCollaborationOutcome(commit_round);

  auto proposal = os->onRequestProposal(target_round);

  // since we only sent one transaction,
  // if the proposal is present, there is no need to check for that specific tx
  EXPECT_TRUE(proposal);
}
