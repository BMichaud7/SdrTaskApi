/*
========================================================================
Project: OpenRFStack
Author:  Brendan Michaud
Year:    2026
Part of OpenRFStack (https://github.com/OpenRFStack)

Licensed under the Personal Use License.
Do not use for commercial, organizational, or military purposes.
Contact author for permission: https://github.com/OpenRFStack
========================================================================
*/
#include <gtest/gtest.h>
#include "sdr/MessageCodec.hpp"
#include "sdr/Types.hpp"
#include <nlohmann/json.hpp>
#include <chrono>

using namespace sdr;
using json = nlohmann::json;

static int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

// ── rank field decode ─────────────────────────────────────────────────────────

TEST(MessageCodec, DecodeRankPresent) {
    auto body = json{
        {"msg_type",       "TASK_REQUEST_IMMEDIATE"},
        {"schema_version", "2.0"},
        {"timestamp_ms",   nowMs()},
        {"request_id",     "req-rank-001"},
        {"task_type",      "NARROWBAND"},
        {"priority",       5},
        {"rank",           3},
        {"schedule",       {{"mode","IMMEDIATE"}}},
        {"rf", {{"center_freq_hz",162.4e6},{"bandwidth_hz",200e3},
                {"sample_rate_sps",250e3},{"rx_count",1},{"tx_count",0}}},
        {"streaming", {{"dest_ip","127.0.0.1"},{"dest_ports",{5000}}}}
    }.dump();

    auto req = MessageCodec::decode(body);
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->priority, 5);
    EXPECT_EQ(req->rank,     3);
}

TEST(MessageCodec, DecodeRankAbsentDefaultsToZero) {
    auto body = json{
        {"msg_type",       "TASK_REQUEST_IMMEDIATE"},
        {"schema_version", "2.0"},
        {"timestamp_ms",   nowMs()},
        {"request_id",     "req-rank-002"},
        {"task_type",      "NARROWBAND"},
        {"schedule",       {{"mode","IMMEDIATE"}}},
        {"rf", {{"center_freq_hz",162.4e6},{"bandwidth_hz",200e3},
                {"sample_rate_sps",250e3},{"rx_count",1},{"tx_count",0}}},
        {"streaming", {{"dest_ip","127.0.0.1"},{"dest_ports",{5000}}}}
    }.dump();

    auto req = MessageCodec::decode(body);
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->rank, 0);
}

// ── scan_params key (canonical) ───────────────────────────────────────────────

TEST(MessageCodec, DecodeScanWithScanParamsKey) {
    auto body = json{
        {"msg_type",       "TASK_REQUEST_SCAN"},
        {"schema_version", "2.0"},
        {"timestamp_ms",   nowMs()},
        {"request_id",     "req-scan-canonical"},
        {"task_type",      "SCAN"},
        {"schedule",       {{"mode","CONTINUOUS"}}},
        {"rf", {{"center_freq_hz",433.92e6},{"bandwidth_hz",2e6},
                {"sample_rate_sps",2e6},{"rx_count",1},{"tx_count",0},
                {"rx_gain_db",{30.0}}}},
        {"streaming", {{"dest_ip","127.0.0.1"},{"dest_ports",{5100}}}},
        {"scan_params", {
            {"repeat", true},
            {"entries", {
                {{"step",0},{"center_freq_hz",433.92e6},{"bandwidth_hz",2e6},
                 {"sample_rate_sps",2e6},{"dwell_ms",500}},
                {{"step",1},{"center_freq_hz",915e6},{"bandwidth_hz",5e6},
                 {"sample_rate_sps",5e6},{"dwell_ms",1000}}
            }}
        }}
    }.dump();

    auto req = MessageCodec::decode(body);
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->task_type, TaskType::SCAN);
    ASSERT_TRUE(req->scan_params.has_value());
    EXPECT_TRUE(req->scan_params->repeat);
    ASSERT_EQ(req->scan_params->entries.size(), 2u);
    EXPECT_DOUBLE_EQ(req->scan_params->entries[0].center_freq_hz, 433.92e6);
    EXPECT_EQ(req->scan_params->entries[0].dwell_ms, 500);
    EXPECT_DOUBLE_EQ(req->scan_params->entries[1].center_freq_hz, 915e6);
    EXPECT_EQ(req->scan_params->entries[1].dwell_ms, 1000);
}

TEST(MessageCodec, DecodeScanWithLegacyScanKey) {
    auto body = json{
        {"msg_type",       "TASK_REQUEST_SCAN"},
        {"schema_version", "2.0"},
        {"timestamp_ms",   nowMs()},
        {"request_id",     "req-scan-legacy"},
        {"task_type",      "SCAN"},
        {"schedule",       {{"mode","CONTINUOUS"}}},
        {"rf", {{"center_freq_hz",915e6},{"bandwidth_hz",5e6},
                {"sample_rate_sps",5e6},{"rx_count",1},{"tx_count",0}}},
        {"streaming", {{"dest_ip","127.0.0.1"},{"dest_ports",{5200}}}},
        {"scan", {
            {"repeat", false},
            {"entries", {
                {{"step",0},{"center_freq_hz",915e6},{"bandwidth_hz",5e6},
                 {"sample_rate_sps",5e6},{"dwell_ms",750}}
            }}
        }}
    }.dump();

    auto req = MessageCodec::decode(body);
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->task_type, TaskType::SCAN);
    ASSERT_TRUE(req->scan_params.has_value());
    EXPECT_FALSE(req->scan_params->repeat);
    ASSERT_EQ(req->scan_params->entries.size(), 1u);
    EXPECT_EQ(req->scan_params->entries[0].dwell_ms, 750);
}

TEST(MessageCodec, DecodeScanMissingEntriesKeyReturnsNullopt) {
    auto body = json{
        {"msg_type",   "TASK_REQUEST_SCAN"},
        {"request_id", "req-scan-bad"},
        {"task_type",  "SCAN"},
        {"rf",         {{"center_freq_hz",915e6},{"bandwidth_hz",5e6},
                        {"sample_rate_sps",5e6},{"rx_count",1},{"tx_count",0}}}
        // no scan_params or scan key
    }.dump();

    auto req = MessageCodec::decode(body);
    EXPECT_FALSE(req.has_value());
}

// ── encode rank in task status ────────────────────────────────────────────────

TEST(MessageCodec, EncodeTaskStatusIncludesRank) {
    TaskRecord rec;
    rec.task_id      = "abcdef12";
    rec.task_type    = TaskType::NARROWBAND;
    rec.schedule_mode= ScheduleMode::CONTINUOUS;
    rec.state        = TaskState::RUNNING;
    rec.priority     = 5;
    rec.rank         = 3;
    rec.start_time_ms= 1000;
    rec.stop_time_ms = TIME_INFINITE;

    auto j = json::parse(MessageCodec::encodeTaskStatus(rec, {}));
    EXPECT_EQ(j["priority"], 5);
    EXPECT_EQ(j["rank"],     3);
}

TEST(MessageCodec, EncodeTaskStatusRankZeroWhenDefault) {
    TaskRecord rec;
    rec.task_id      = "zzzzzzzz";
    rec.task_type    = TaskType::SCAN;
    rec.schedule_mode= ScheduleMode::SCHEDULED;
    rec.state        = TaskState::SCHEDULED;
    rec.start_time_ms= 0;
    rec.stop_time_ms = TIME_INFINITE;

    auto j = json::parse(MessageCodec::encodeTaskStatus(rec, {}));
    EXPECT_EQ(j["rank"], 0);
}

// ── basic decode correctness ──────────────────────────────────────────────────

TEST(MessageCodec, DecodeHealthQuery) {
    auto body = json{
        {"msg_type",       "HEALTH_QUERY"},
        {"schema_version", "2.0"},
        {"timestamp_ms",   nowMs()},
        {"request_id",     "req-hq-001"}
    }.dump();

    auto req = MessageCodec::decode(body);
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->msg_type,   "HEALTH_QUERY");
    EXPECT_EQ(req->request_id, "req-hq-001");
}

TEST(MessageCodec, DecodeTaskStop) {
    auto body = json{
        {"msg_type",     "TASK_STOP"},
        {"schema_version","2.0"},
        {"timestamp_ms", nowMs()},
        {"request_id",   "req-stop-001"},
        {"task_id",      "task-abc"},
        {"reason",       "unit test"}
    }.dump();

    auto req = MessageCodec::decode(body);
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->task_id, "task-abc");
    EXPECT_EQ(req->reason,  "unit test");
}

TEST(MessageCodec, MissingRequestIdReturnsNullopt) {
    auto body = json{
        {"msg_type",       "HEALTH_QUERY"},
        {"schema_version", "2.0"},
        {"timestamp_ms",   nowMs()}
    }.dump();
    EXPECT_FALSE(MessageCodec::decode(body).has_value());
}

TEST(MessageCodec, MalformedJsonReturnsNullopt) {
    EXPECT_FALSE(MessageCodec::decode("{not valid json").has_value());
}

TEST(MessageCodec, PeekMsgType) {
    auto body = json{{"msg_type","TASK_STOP"},{"request_id","x"}}.dump();
    EXPECT_EQ(MessageCodec::peekMsgType(body), "TASK_STOP");
    EXPECT_EQ(MessageCodec::peekMsgType("{bad"), "");
}

// ── encode response ───────────────────────────────────────────────────────────

TEST(MessageCodec, EncodeTaskResponseAccepted) {
    TaskResponse resp;
    resp.request_id    = "req-001";
    resp.accepted      = true;
    resp.task_id       = "task-uuid";
    resp.schedule_mode = "CONTINUOUS";

    AssignedStream s;
    s.stream_id    = "s1";
    s.device_id    = "fake-0";
    s.channel_type = "RX";
    s.udp_port     = 30000;
    s.format       = "CF32";
    resp.streams.push_back(s);

    auto j = json::parse(MessageCodec::encodeTaskResponse(resp));
    EXPECT_EQ(j["status"],  "ACCEPTED");
    EXPECT_EQ(j["task_id"], "task-uuid");
    ASSERT_EQ(j["streams"].size(), 1u);
    EXPECT_EQ(j["streams"][0]["udp_port"], 30000);
}

TEST(MessageCodec, EncodeTaskResponseRejected) {
    TaskResponse resp;
    resp.request_id    = "req-002";
    resp.accepted      = false;
    resp.reject_code   = RejectCode::NO_DEVICE_AVAILABLE;
    resp.reject_reason = "no devices online";

    auto j = json::parse(MessageCodec::encodeTaskResponse(resp));
    EXPECT_EQ(j["status"],        "REJECTED");
    EXPECT_EQ(j["reject_code"],   "NO_DEVICE_AVAILABLE");
    EXPECT_EQ(j["reject_reason"], "no devices online");
    EXPECT_FALSE(j.contains("streams"));
}

// ── TASK_CANCEL ───────────────────────────────────────────────────────────────

TEST(MessageCodec, DecodeTaskCancel) {
    auto body = json{
        {"msg_type",     "TASK_CANCEL"},
        {"schema_version","2.0"},
        {"timestamp_ms", nowMs()},
        {"request_id",   "req-cancel-001"},
        {"task_id",      "task-to-cancel"},
        {"reason",       "operator abort"}
    }.dump();

    auto req = MessageCodec::decode(body);
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->msg_type, "TASK_CANCEL");
    EXPECT_EQ(req->task_id,  "task-to-cancel");
    EXPECT_EQ(req->reason,   "operator abort");
    EXPECT_EQ(req->rank,     0);
}

// ── TASK_REQUEST_TRIGGERED ────────────────────────────────────────────────────

TEST(MessageCodec, DecodeTriggeredTask) {
    auto body = json{
        {"msg_type",       "TASK_REQUEST_TRIGGERED"},
        {"schema_version", "2.0"},
        {"timestamp_ms",   nowMs()},
        {"request_id",     "req-trig-001"},
        {"task_type",      "TRIGGERED"},
        {"priority",       3},
        {"rank",           1},
        {"schedule",       {{"mode","CONTINUOUS"}}},
        {"rf", {{"center_freq_hz",433.92e6},{"bandwidth_hz",2e6},
                {"sample_rate_sps",2e6},{"rx_count",1},{"tx_count",0}}},
        {"streaming", {{"dest_ip","127.0.0.1"},{"dest_ports",{5000}}}},
        {"trigger", {
            {"type",            "POWER_THRESHOLD"},
            {"threshold_dbfs",  -55.0},
            {"pre_trigger_ms",  100},
            {"post_trigger_ms", 500},
            {"max_captures",    3}
        }}
    }.dump();

    auto req = MessageCodec::decode(body);
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->task_type, TaskType::TRIGGERED);
    EXPECT_EQ(req->priority,  3);
    EXPECT_EQ(req->rank,      1);
    ASSERT_TRUE(req->trigger_params.has_value());
    EXPECT_EQ(req->trigger_params->trigger_type,  "POWER_THRESHOLD");
    EXPECT_DOUBLE_EQ(req->trigger_params->threshold_dbfs, -55.0);
    EXPECT_EQ(req->trigger_params->pre_trigger_ms,  100);
    EXPECT_EQ(req->trigger_params->post_trigger_ms, 500);
    EXPECT_EQ(req->trigger_params->max_captures,    3);
}

TEST(MessageCodec, DecodeTriggeredTaskDefaultParams) {
    auto body = json{
        {"msg_type",       "TASK_REQUEST_TRIGGERED"},
        {"schema_version", "2.0"},
        {"timestamp_ms",   nowMs()},
        {"request_id",     "req-trig-default"},
        {"task_type",      "TRIGGERED"},
        {"schedule",       {{"mode","CONTINUOUS"}}},
        {"rf", {{"center_freq_hz",915e6},{"bandwidth_hz",5e6},
                {"sample_rate_sps",5e6},{"rx_count",1},{"tx_count",0}}},
        {"streaming", {{"dest_ip","127.0.0.1"},{"dest_ports",{5001}}}}
        // no "trigger" block — should use defaults
    }.dump();

    auto req = MessageCodec::decode(body);
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->task_type, TaskType::TRIGGERED);
    ASSERT_TRUE(req->trigger_params.has_value());
    EXPECT_EQ(req->trigger_params->trigger_type,  "POWER_THRESHOLD");
    EXPECT_DOUBLE_EQ(req->trigger_params->threshold_dbfs, -60.0);
    EXPECT_EQ(req->trigger_params->pre_trigger_ms,  50);
    EXPECT_EQ(req->trigger_params->post_trigger_ms, 200);
    EXPECT_EQ(req->trigger_params->max_captures,    0);
}

// ── TASK_REQUEST_CALIBRATION ──────────────────────────────────────────────────

TEST(MessageCodec, DecodeCalibrationTask) {
    auto body = json{
        {"msg_type",       "TASK_REQUEST_CALIBRATION"},
        {"schema_version", "2.0"},
        {"timestamp_ms",   nowMs()},
        {"request_id",     "req-cal-001"},
        {"calibration", {
            {"center_freq_hz",   2400e6},
            {"bandwidth_hz",     20e6},
            {"sample_rate_sps",  20e6},
            {"duration_ms",      8000},
            {"rx_count_per_device", 2},
            {"coherency_group",  "group-a"},
            {"devices",          {"dev-0","dev-1"}}
        }},
        {"streaming", {{"dest_ip","10.0.0.5"},{"dest_ports",{6000,6001}}}}
    }.dump();

    auto req = MessageCodec::decode(body);
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->task_type, TaskType::CALIBRATION);
    ASSERT_TRUE(req->cal_params.has_value());
    EXPECT_DOUBLE_EQ(req->cal_params->center_freq_hz, 2400e6);
    EXPECT_EQ(req->cal_params->duration_ms,           8000);
    EXPECT_EQ(req->cal_params->rx_count_per_device,   2);
    EXPECT_EQ(req->cal_params->coherency_group,       "group-a");
    ASSERT_EQ(req->cal_params->devices.size(), 2u);
    EXPECT_EQ(req->cal_params->devices[0], "dev-0");
    EXPECT_EQ(req->cal_params->devices[1], "dev-1");
    EXPECT_EQ(req->streaming.dest_ip, "10.0.0.5");
    ASSERT_EQ(req->streaming.dest_ports.size(), 2u);
}

TEST(MessageCodec, DecodeCalibrationMissingCalibrationBlockReturnsNullopt) {
    auto body = json{
        {"msg_type",   "TASK_REQUEST_CALIBRATION"},
        {"request_id", "req-cal-bad"}
        // no "calibration" block
    }.dump();
    EXPECT_FALSE(MessageCodec::decode(body).has_value());
}

// ── task_params blocks ────────────────────────────────────────────────────────

TEST(MessageCodec, DecodeWidebandTaskParams) {
    auto body = json{
        {"msg_type",       "TASK_REQUEST_CONTINUOUS"},
        {"schema_version", "2.0"},
        {"timestamp_ms",   nowMs()},
        {"request_id",     "req-wb-001"},
        {"task_type",      "WIDEBAND"},
        {"schedule",       {{"mode","CONTINUOUS"}}},
        {"rf", {{"center_freq_hz",2400e6},{"bandwidth_hz",40e6},
                {"sample_rate_sps",40e6},{"rx_count",1},{"tx_count",0}}},
        {"streaming", {{"dest_ip","127.0.0.1"},{"dest_ports",{5200}}}},
        {"task_params", {
            {"wideband", {{"record_raw_iq",false},
                          {"detect_threshold_dbfs",-55.0},
                          {"fft_size",4096}}}
        }}
    }.dump();

    auto req = MessageCodec::decode(body);
    ASSERT_TRUE(req.has_value());
    EXPECT_EQ(req->task_type, TaskType::WIDEBAND);
    ASSERT_TRUE(req->wb_params.has_value());
    EXPECT_FALSE(req->wb_params->record_raw_iq);
    EXPECT_DOUBLE_EQ(req->wb_params->detect_threshold_dbfs, -55.0);
    EXPECT_EQ(req->wb_params->fft_size, 4096);
}

TEST(MessageCodec, DecodeDfTaskParams) {
    auto body = json{
        {"msg_type",       "TASK_REQUEST_SCHEDULED"},
        {"schema_version", "2.0"},
        {"timestamp_ms",   nowMs()},
        {"request_id",     "req-df-001"},
        {"task_type",      "DF"},
        {"schedule",       {{"mode","SCHEDULED"},
                            {"start_time_epoch_ms", nowMs()+5000},
                            {"end_time_epoch_ms",   nowMs()+65000}}},
        {"rf", {{"center_freq_hz",915e6},{"bandwidth_hz",10e6},
                {"sample_rate_sps",10e6},{"rx_count",4},{"tx_count",0}}},
        {"streaming", {{"dest_ip","10.0.0.1"},{"dest_ports",{5300,5301,5302,5303}}}},
        {"task_params", {
            {"df", {{"algorithm","MUSIC"},{"num_sources",2},
                    {"snapshot_count",512},{"angular_res_deg",0.5}}}
        }}
    }.dump();

    auto req = MessageCodec::decode(body);
    ASSERT_TRUE(req.has_value());
    ASSERT_TRUE(req->df_params.has_value());
    EXPECT_EQ(req->df_params->algorithm,      "MUSIC");
    EXPECT_EQ(req->df_params->num_sources,    2);
    EXPECT_EQ(req->df_params->snapshot_count, 512);
    EXPECT_DOUBLE_EQ(req->df_params->angular_res_deg, 0.5);
}

// ── encode health ─────────────────────────────────────────────────────────────

TEST(MessageCodec, EncodeDeviceHealth) {
    MessageCodec::DevHealthEntry e;
    e.device_id       = "dev-0";
    e.driver          = "uhd";
    e.uri             = "type=b200";
    e.coherency_group = "group-a";
    e.online          = true;
    e.active_tasks    = 2;
    e.cf_hz           = 915e6;
    e.rate_sps        = 10e6;
    e.alloc_bw_hz     = 8e6;
    e.free_bw_hz      = 2e6;
    e.temp_c          = 42.5;

    auto j = json::parse(MessageCodec::encodeDeviceHealth("req-dh", {e}));
    EXPECT_EQ(j["msg_type"], "DEVICE_HEALTH");
    ASSERT_EQ(j["devices"].size(), 1u);
    auto& d = j["devices"][0];
    EXPECT_EQ(d["device_id"],       "dev-0");
    EXPECT_EQ(d["online"],          true);
    EXPECT_EQ(d["active_tasks"],    2);
    EXPECT_DOUBLE_EQ(d["temperature_c"].get<double>(), 42.5);
    EXPECT_EQ(d["coherency_group"], "group-a");
}

TEST(MessageCodec, EncodeDeviceHealthEmptyList) {
    auto j = json::parse(MessageCodec::encodeDeviceHealth("req-dh-empty", {}));
    EXPECT_EQ(j["msg_type"],    "DEVICE_HEALTH");
    EXPECT_TRUE(j["devices"].empty());
}

TEST(MessageCodec, EncodeControllerHealth) {
    auto j = json::parse(MessageCodec::encodeControllerHealth(
        "req-ch", 4, 3, 2, 1, 5, 10, 8, 92, 3600LL));
    EXPECT_EQ(j["msg_type"],        "CONTROLLER_HEALTH");
    EXPECT_EQ(j["total_devices"],   4);
    EXPECT_EQ(j["online_devices"],  3);
    EXPECT_EQ(j["scheduled_tasks"], 2);
    EXPECT_EQ(j["pending_tasks"],   1);
    EXPECT_EQ(j["running_tasks"],   5);
    EXPECT_EQ(j["total_tasks"],     10);
    EXPECT_EQ(j["udp_ports_in_use"],8);
    EXPECT_EQ(j["udp_ports_free"],  92);
    EXPECT_EQ(j["uptime_sec"],      3600);
}

// ── encode snapshot failure ───────────────────────────────────────────────────

TEST(MessageCodec, EncodeSnapshotResultFailure) {
    SnapshotResult r;
    r.device_id = "dev-1";
    r.success   = false;
    r.error_msg = "device offline";

    auto j = json::parse(MessageCodec::encodeSnapshotResult("req-snap-fail", r));
    EXPECT_EQ(j["msg_type"],  "SNAPSHOT_RESULT");
    EXPECT_EQ(j["status"],    "FAILED");
    EXPECT_EQ(j["error_msg"], "device offline");
    EXPECT_TRUE(j["power_bins"].empty());
}
