// Copyright 2019 DeepMind Technologies Limited.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "reverb/cc/client.h"

#include <memory>

#include "grpcpp/impl/codegen/client_context.h"
#include "grpcpp/impl/codegen/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "reverb/cc/reverb_service.pb.h"
#include "reverb/cc/reverb_service_mock.grpc.pb.h"
#include "reverb/cc/support/uint128.h"
#include "reverb/cc/testing/proto_test_util.h"
#include "tensorflow/core/lib/core/status_test_util.h"

namespace deepmind {
namespace reverb {
namespace {

constexpr char kCheckpointPath[] = "/path/to/checkpoint";

class FakeStub : public /* grpc_gen:: */MockReverbServiceStub {
 public:
  grpc::Status MutatePriorities(grpc::ClientContext* context,
                                const MutatePrioritiesRequest& request,
                                MutatePrioritiesResponse* response) override {
    mutate_priorities_request_ = request;
    return grpc::Status::OK;
  }

  grpc::Status Reset(grpc::ClientContext* context, const ResetRequest& request,
                     ResetResponse* response) override {
    reset_request_ = request;
    return grpc::Status::OK;
  }

  grpc::Status Checkpoint(grpc::ClientContext* context,
                          const CheckpointRequest& request,
                          CheckpointResponse* response) override {
    response->set_checkpoint_path(kCheckpointPath);
    return grpc::Status::OK;
  }

  grpc::Status ServerInfo(grpc::ClientContext* context,
                          const ServerInfoRequest& request,
                          ServerInfoResponse* response) override {
    *response->mutable_tables_state_id() =
        Uint128ToMessage(absl::MakeUint128(1, 2));
    response->add_table_info()->set_max_size(2);
    return grpc::Status::OK;
  }

  const MutatePrioritiesRequest& mutate_priorities_request() {
    return mutate_priorities_request_;
  }

  const ResetRequest& reset_request() { return reset_request_; }

 private:
  MutatePrioritiesRequest mutate_priorities_request_;
  ResetRequest reset_request_;
};

TEST(ClientTest, MutatePrioritiesDefaultValues) {
  auto stub = std::make_shared<FakeStub>();
  Client client(stub);
  TF_EXPECT_OK(client.MutatePriorities("", {}, {}));
  EXPECT_THAT(stub->mutate_priorities_request(),
              testing::EqualsProto(MutatePrioritiesRequest()));
}

TEST(ClientTest, MutatePrioritiesFilled) {
  auto stub = std::make_shared<FakeStub>();
  Client client(stub);
  auto pair = testing::MakeKeyWithPriority(123, 456);
  TF_EXPECT_OK(client.MutatePriorities("table", {pair}, {4}));

  MutatePrioritiesRequest expected;
  expected.set_table("table");
  *expected.add_updates() = pair;
  expected.add_delete_keys(4);
  EXPECT_THAT(stub->mutate_priorities_request(),
              testing::EqualsProto(expected));
}

TEST(ClientTest, ResetRequestFilled) {
  auto stub = std::make_shared<FakeStub>();
  Client client(stub);
  TF_EXPECT_OK(client.Reset("table"));

  ResetRequest expected;
  expected.set_table("table");
  EXPECT_THAT(stub->reset_request(), testing::EqualsProto(expected));
}

TEST(ClientTest, Checkpoint) {
  auto stub = std::make_shared<FakeStub>();
  Client client(stub);
  std::string path;
  TF_EXPECT_OK(client.Checkpoint(&path));
  EXPECT_EQ(path, kCheckpointPath);
}

TEST(ClientTest, ServerInfoRequestFilled) {
  auto stub = std::make_shared<FakeStub>();
  Client client(stub);
  struct Client::ServerInfo info;
  TF_EXPECT_OK(client.ServerInfo(&info));

  TableInfo expected_info;
  expected_info.set_max_size(2);
  EXPECT_EQ(info.tables_state_id, absl::MakeUint128(1, 2));
  EXPECT_EQ(info.table_info.size(), 1);
  EXPECT_THAT(info.table_info[0], testing::EqualsProto(expected_info));
}

}  // namespace
}  // namespace reverb
}  // namespace deepmind
