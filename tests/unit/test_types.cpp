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
#include "sdr/Types.hpp"

using namespace sdr;

// ── IqPacketHeader layout ─────────────────────────────────────────────────────

TEST(Types, IqPacketHeaderIs32Bytes) {
    static_assert(sizeof(IqPacketHeader) == 32, "header must be 32 bytes");
    EXPECT_EQ(sizeof(IqPacketHeader), 32u);
}

TEST(Types, IqPacketMagic) {
    EXPECT_EQ(IQ_PACKET_MAGIC, 0x49515030u);
}

// ── TaskType conversions ──────────────────────────────────────────────────────

TEST(Types, TaskTypeRoundTrip) {
    for (auto t : {TaskType::DF, TaskType::NARROWBAND, TaskType::WIDEBAND,
                   TaskType::SCAN, TaskType::SNAPSHOT, TaskType::TRIGGERED,
                   TaskType::CALIBRATION}) {
        EXPECT_EQ(taskTypeFromString(taskTypeToString(t)), t);
    }
}

TEST(Types, TaskTypeFromStringUnknown) {
    EXPECT_EQ(taskTypeFromString("BOGUS"),  TaskType::UNKNOWN);
    EXPECT_EQ(taskTypeFromString(""),       TaskType::UNKNOWN);
}

TEST(Types, TaskTypeToStringUnknown) {
    EXPECT_EQ(taskTypeToString(TaskType::UNKNOWN), "UNKNOWN");
}

// ── ScheduleMode conversions ──────────────────────────────────────────────────

TEST(Types, ScheduleModeRoundTrip) {
    for (auto m : {ScheduleMode::SCHEDULED, ScheduleMode::IMMEDIATE,
                   ScheduleMode::CONTINUOUS}) {
        EXPECT_EQ(scheduleModeFromString(scheduleModeToString(m)), m);
    }
}

TEST(Types, ScheduleModeDefaultIsScheduled) {
    EXPECT_EQ(scheduleModeFromString("anything_else"), ScheduleMode::SCHEDULED);
}

// ── TaskState conversions ─────────────────────────────────────────────────────

TEST(Types, TaskStateRoundTrip) {
    for (auto s : {TaskState::EVALUATING, TaskState::SCHEDULED, TaskState::PENDING,
                   TaskState::RUNNING,    TaskState::COMPLETING, TaskState::COMPLETED,
                   TaskState::FAILED,     TaskState::CANCELLED}) {
        EXPECT_NE(taskStateToString(s), "UNKNOWN");
    }
}

TEST(Types, IsTerminalState) {
    EXPECT_TRUE(isTerminalState(TaskState::COMPLETED));
    EXPECT_TRUE(isTerminalState(TaskState::FAILED));
    EXPECT_TRUE(isTerminalState(TaskState::CANCELLED));

    EXPECT_FALSE(isTerminalState(TaskState::EVALUATING));
    EXPECT_FALSE(isTerminalState(TaskState::SCHEDULED));
    EXPECT_FALSE(isTerminalState(TaskState::PENDING));
    EXPECT_FALSE(isTerminalState(TaskState::RUNNING));
    EXPECT_FALSE(isTerminalState(TaskState::COMPLETING));
}

// ── RejectCode conversions ────────────────────────────────────────────────────

TEST(Types, RejectCodeNone) {
    EXPECT_EQ(rejectCodeToString(RejectCode::NONE), "NONE");
}

TEST(Types, RejectCodeAllHaveStrings) {
    for (auto c : {RejectCode::INVALID_REQUEST, RejectCode::SPECTRUM_CONFLICT,
                   RejectCode::NO_DEVICE_AVAILABLE, RejectCode::INTERNAL_ERROR,
                   RejectCode::TASK_NOT_FOUND, RejectCode::PORT_POOL_EXHAUSTED}) {
        EXPECT_NE(rejectCodeToString(c), "UNKNOWN");
    }
}

// ── rank field defaults ───────────────────────────────────────────────────────

TEST(Types, TaskRequestRankDefaultsToZero) {
    TaskRequest req;
    EXPECT_EQ(req.rank, 0);
}

TEST(Types, TaskRecordRankDefaultsToZero) {
    TaskRecord rec;
    EXPECT_EQ(rec.rank, 0);
}

TEST(Types, TaskRequestRankCanBeSet) {
    TaskRequest req;
    req.rank = 42;
    EXPECT_EQ(req.rank, 42);
}

TEST(Types, TaskRecordRankCanBeSet) {
    TaskRecord rec;
    rec.rank = 7;
    EXPECT_EQ(rec.rank, 7);
}

// ── priority is independent of rank ──────────────────────────────────────────

TEST(Types, PriorityAndRankAreIndependent) {
    TaskRequest req;
    req.priority = 10;
    req.rank     = 3;
    EXPECT_EQ(req.priority, 10);
    EXPECT_EQ(req.rank,      3);
}

// ── TIME_INFINITE ─────────────────────────────────────────────────────────────

TEST(Types, TimeInfiniteIsInt64Max) {
    EXPECT_EQ(TIME_INFINITE, std::numeric_limits<int64_t>::max());
}

TEST(Types, TaskRequestEndTimeDefaultsToInfinite) {
    TaskRequest req;
    EXPECT_EQ(req.end_time_ms, TIME_INFINITE);
}

TEST(Types, TaskRecordStopTimeDefaultsToInfinite) {
    TaskRecord rec;
    EXPECT_EQ(rec.stop_time_ms, TIME_INFINITE);
}

// ── Struct field defaults ─────────────────────────────────────────────────────

TEST(Types, RfRequestDefaultsAllZero) {
    RfRequest rf;
    EXPECT_EQ(rf.center_freq_hz,  0.0);
    EXPECT_EQ(rf.bandwidth_hz,    0.0);
    EXPECT_EQ(rf.sample_rate_sps, 0.0);
    EXPECT_EQ(rf.rx_count, 0);
    EXPECT_EQ(rf.tx_count, 0);
    EXPECT_TRUE(rf.rx_gain_db.empty());
    EXPECT_TRUE(rf.rx_agc.empty());
    EXPECT_TRUE(rf.tx_atten_db.empty());
    EXPECT_TRUE(rf.preferred_device.empty());
    EXPECT_TRUE(rf.coherency_group.empty());
}

TEST(Types, StreamingDestDefaultsEmpty) {
    StreamingDest sd;
    EXPECT_TRUE(sd.dest_ip.empty());
    EXPECT_TRUE(sd.dest_ports.empty());
}

TEST(Types, TaskRequestDefaultState) {
    TaskRequest req;
    EXPECT_TRUE(req.msg_type.empty());
    EXPECT_EQ(req.task_type,    TaskType::UNKNOWN);
    EXPECT_EQ(req.schedule_mode, ScheduleMode::SCHEDULED);
    EXPECT_EQ(req.priority,  0);
    EXPECT_EQ(req.rank,      0);
    EXPECT_EQ(req.timestamp_ms,  0);
    EXPECT_EQ(req.start_time_ms, 0);
    EXPECT_EQ(req.duration_ms,   0);
    EXPECT_FALSE(req.df_params.has_value());
    EXPECT_FALSE(req.nb_params.has_value());
    EXPECT_FALSE(req.wb_params.has_value());
    EXPECT_FALSE(req.scan_params.has_value());
    EXPECT_FALSE(req.snapshot_params.has_value());
    EXPECT_FALSE(req.trigger_params.has_value());
    EXPECT_FALSE(req.cal_params.has_value());
}

TEST(Types, TaskRecordDefaultState) {
    TaskRecord rec;
    EXPECT_TRUE(rec.task_id.empty());
    EXPECT_EQ(rec.task_type,    TaskType::UNKNOWN);
    EXPECT_EQ(rec.schedule_mode, ScheduleMode::SCHEDULED);
    EXPECT_EQ(rec.state,    TaskState::EVALUATING);
    EXPECT_EQ(rec.priority, 0);
    EXPECT_EQ(rec.rank,     0);
    EXPECT_EQ(rec.start_time_ms, 0);
    EXPECT_TRUE(rec.allocations.empty());
    EXPECT_TRUE(rec.stream_metrics.empty());
    EXPECT_TRUE(rec.terminal_reason.empty());
}

TEST(Types, DfParamsDefaults) {
    DfParams p;
    EXPECT_EQ(p.algorithm,       "MUSIC");
    EXPECT_EQ(p.num_sources,     1);
    EXPECT_EQ(p.snapshot_count,  1024);
    EXPECT_DOUBLE_EQ(p.angular_res_deg, 1.0);
}

TEST(Types, NarrowbandParamsDefaults) {
    NarrowbandParams p;
    EXPECT_EQ(p.demod,            "FM");
    EXPECT_DOUBLE_EQ(p.squelch_dbfs, -80.0);
    EXPECT_EQ(p.output_rate_sps,  48000);
}

TEST(Types, WidebandParamsDefaults) {
    WidebandParams p;
    EXPECT_TRUE(p.record_raw_iq);
    EXPECT_DOUBLE_EQ(p.detect_threshold_dbfs, -60.0);
    EXPECT_EQ(p.fft_size, 2048);
}

TEST(Types, ScanEntryDefaults) {
    ScanEntry e;
    EXPECT_EQ(e.step,            0);
    EXPECT_DOUBLE_EQ(e.center_freq_hz,  0.0);
    EXPECT_DOUBLE_EQ(e.bandwidth_hz,    0.0);
    EXPECT_DOUBLE_EQ(e.sample_rate_sps, 0.0);
    EXPECT_EQ(e.dwell_ms,        1000);
}

TEST(Types, TriggerParamsDefaults) {
    TriggerParams p;
    EXPECT_EQ(p.trigger_type,     "POWER_THRESHOLD");
    EXPECT_DOUBLE_EQ(p.threshold_dbfs, -60.0);
    EXPECT_EQ(p.pre_trigger_ms,   50);
    EXPECT_EQ(p.post_trigger_ms,  200);
    EXPECT_EQ(p.max_captures,     0);
}

TEST(Types, AssignedStreamDefaults) {
    AssignedStream s;
    EXPECT_TRUE(s.stream_id.empty());
    EXPECT_EQ(s.channel_index,   0);
    EXPECT_EQ(s.udp_port,        0);
    EXPECT_DOUBLE_EQ(s.center_freq_hz,  0.0);
    EXPECT_EQ(s.format,          "CF32");
}

TEST(Types, TaskResponseDefaultNotAccepted) {
    TaskResponse resp;
    EXPECT_FALSE(resp.accepted);
    EXPECT_EQ(resp.reject_code, RejectCode::NONE);
    EXPECT_TRUE(resp.streams.empty());
}

// ── IQ flag constants ─────────────────────────────────────────────────────────

TEST(Types, IqFlagsAreSingleBitsAndMutuallyDistinct) {
    EXPECT_EQ(IQ_FLAG_OVERFLOW    & IQ_FLAG_FIRST_PACKET,  0u);
    EXPECT_EQ(IQ_FLAG_OVERFLOW    & IQ_FLAG_DWELL_CHANGE,  0u);
    EXPECT_EQ(IQ_FLAG_FIRST_PACKET & IQ_FLAG_DWELL_CHANGE, 0u);
}

TEST(Types, SchemaVersionString) {
    EXPECT_STREQ(SCHEMA_VERSION, "2.0");
}

// ── PREEMPT_TERMINAL_REASON and isPreempted ───────────────────────────────────

TEST(Types, PreemptTerminalReasonIsNotEmpty) {
    EXPECT_FALSE(std::string(PREEMPT_TERMINAL_REASON).empty());
}

TEST(Types, PreemptTerminalReasonContainsExpectedText) {
    EXPECT_NE(std::string(PREEMPT_TERMINAL_REASON).find("PREEMPTED"), std::string::npos);
}

TEST(Types, IsPreemptedReturnsTrueForCancelledWithPreemptReason) {
    TaskRecord rec;
    rec.state = TaskState::CANCELLED;
    rec.terminal_reason = std::string(PREEMPT_TERMINAL_REASON) + " rank=5 request=req-1";
    EXPECT_TRUE(isPreempted(rec));
}

TEST(Types, IsPreemptedReturnsFalseForOtherCancelledReason) {
    TaskRecord rec;
    rec.state = TaskState::CANCELLED;
    rec.terminal_reason = "user requested stop";
    EXPECT_FALSE(isPreempted(rec));
}

TEST(Types, IsPreemptedReturnsFalseForNonCancelledState) {
    TaskRecord rec;
    rec.state = TaskState::RUNNING;
    rec.terminal_reason = PREEMPT_TERMINAL_REASON;
    EXPECT_FALSE(isPreempted(rec));
}

TEST(Types, IsPreemptedReturnsFalseForEmptyTerminalReason) {
    TaskRecord rec;
    rec.state = TaskState::CANCELLED;
    rec.terminal_reason = "";
    EXPECT_FALSE(isPreempted(rec));
}
